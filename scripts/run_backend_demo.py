#!/usr/bin/env python3
from __future__ import annotations

import argparse
import csv
import json
import os
import shutil
import subprocess
import sys
from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]


def is_windows() -> bool:
    return os.name == "nt"


def exe_name(name: str) -> str:
    return f"{name}.exe" if is_windows() else name


def rel(path: Path) -> str:
    try:
        return str(path.resolve().relative_to(ROOT))
    except ValueError:
        return str(path)


def run(
    command: list[str],
    *,
    cwd: Path = ROOT,
    allow_failure: bool = False,
    print_output: bool = True,
) -> subprocess.CompletedProcess[str]:
    completed = subprocess.run(
        command,
        cwd=str(cwd),
        text=True,
        stdout=subprocess.PIPE,
        stderr=subprocess.PIPE,
    )
    if print_output and completed.stdout.strip():
        print(completed.stdout.rstrip())
    if print_output and completed.stderr.strip():
        print(completed.stderr.rstrip(), file=sys.stderr)
    if completed.returncode != 0 and not allow_failure:
        print(f"failed command: {' '.join(command)}", file=sys.stderr)
        raise SystemExit(completed.returncode)
    return completed


def resolve_python() -> str:
    return sys.executable or "python"


def resolve_fiap_tool(explicit: str | None, build_dir: Path) -> Path:
    candidates: list[Path] = []
    if explicit:
        candidates.append(Path(explicit))
    candidates.extend(
        [
            build_dir / exe_name("fiap-opt"),
            build_dir / "Release" / exe_name("fiap-opt"),
            ROOT / "build" / exe_name("fiap-opt"),
            ROOT / "build" / "Release" / exe_name("fiap-opt"),
        ]
    )
    for candidate in candidates:
        if candidate.exists():
            return candidate.resolve()
    searched = "\n  ".join(rel(candidate) for candidate in candidates)
    raise SystemExit(f"fiap-opt was not found. Run scripts/build.sh first. Searched:\n  {searched}")


def resolve_flang(explicit: str | None) -> Path:
    candidates: list[str | Path] = []
    if explicit:
        candidates.append(explicit)
    candidates.extend(os.environ.get(name, "") for name in ("FIAP_FLANG", "FLANG"))
    candidates.extend(found for name in ("flang-new", "flang", "flang-new.exe", "flang.exe") if (found := shutil.which(name)))
    candidates.extend(
        [
            Path("D:/llvm-project/build/bin/flang.exe"),
            Path("D:/llvm-project/build/bin/flang-new.exe"),
            Path("/mnt/d/llvm-project/build/bin/flang"),
            Path("/mnt/d/llvm-project/build/bin/flang-new"),
        ]
    )
    for candidate_value in candidates:
        if not str(candidate_value).strip():
            continue
        candidate = Path(candidate_value)
        if candidate.exists():
            return candidate.resolve()
    raise SystemExit(
        "Flang was not found. Full end-to-end mode requires Flang. "
        "Set FIAP_FLANG or pass --flang D:\\llvm-project\\build\\bin\\flang.exe."
    )


def run_ctest(build_dir: Path) -> str:
    ctest_candidates = [
        shutil.which("ctest"),
        r"C:\Program Files\Microsoft Visual Studio\18\Community\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\ctest.exe",
        r"C:\Program Files\Microsoft Visual Studio\17\Community\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\ctest.exe",
    ]
    ctest = next((str(Path(candidate)) for candidate in ctest_candidates if candidate and Path(candidate).exists()), None)
    if ctest is None:
        return "skipped: ctest not found"
    completed = run([ctest, "--test-dir", str(build_dir), "--output-on-failure"], allow_failure=True)
    return "ok" if completed.returncode == 0 else f"failed: ctest exit {completed.returncode}"


def write_json_report(tool: Path, testcase: Path, output: Path) -> dict:
    completed = run([str(tool), str(testcase), "--format=json"], print_output=False)
    output.parent.mkdir(parents=True, exist_ok=True)
    output.write_text(completed.stdout, encoding="utf-8")
    return json.loads(completed.stdout)


def summarize_report(report: dict) -> tuple[int, int, int, int, int]:
    entries = report.get("entries", [])
    provably = 0
    possible = 0
    necessary = 0
    estimated = 0
    for entry in entries:
        classification = entry.get("classification")
        estimated += int(entry.get("estimatedBytes", 0) or 0)
        if classification == "provably-eliminable":
            provably += 1
        elif classification == "possibly-unnecessary":
            possible += 1
        elif classification == "necessary":
            necessary += 1
    return len(entries), provably, possible, necessary, estimated


def run_mlir_regression(tool: Path, reports_dir: Path) -> Path:
    rows: list[dict[str, object]] = []
    collected_dir = reports_dir / "mlir-regression"
    collected_dir.mkdir(parents=True, exist_ok=True)
    for testcase in sorted((ROOT / "test" / "mlir_regression").glob("*.mlir")):
        report_path = collected_dir / f"{testcase.stem}.json"
        report = write_json_report(tool, testcase, report_path)
        sites, provably, possible, necessary, estimated = summarize_report(report)
        rows.append(
            {
                "testcase": testcase.name,
                "sites": sites,
                "provably_eliminable": provably,
                "possibly_unnecessary": possible,
                "necessary": necessary,
                "estimated_bytes": estimated,
                "report": rel(report_path),
            }
        )
        print(
            f"REGRESSION {testcase.name}: sites={sites}, "
            f"provably={provably}, possible={possible}, necessary={necessary}, bytes={estimated}"
        )

    out = collected_dir / "mlir-regression-summary.csv"
    with out.open("w", newline="", encoding="utf-8") as handle:
        writer = csv.DictWriter(handle, fieldnames=list(rows[0].keys()))
        writer.writeheader()
        writer.writerows(rows)
    return out


def run_fortran_hlfir(tool: Path, reports_dir: Path, flang: Path) -> Path:
    print("\n[1/4] Flang HLFIR + FIAP analysis")
    out_dir = reports_dir / "hlfir"
    summary = out_dir / "summary.csv"
    run(
        [
            resolve_python(),
            str(ROOT / "scripts" / "analyze_fortran_hlfir.py"),
            "--flang",
            str(flang),
            "--tool",
            str(tool),
            "--source-dir",
            str(ROOT / "testcases" / "fortran"),
            "--out-dir",
            str(out_dir),
            "--summary",
            str(summary),
            "--strict",
        ]
    )
    return summary


def run_source_transform(reports_dir: Path) -> Path:
    print("\n[2/4] Source transformation")
    output = reports_dir / "source" / "vector_add.transformed.f90"
    run(
        [
            resolve_python(),
            str(ROOT / "src" / "fiap_source_transformer.py"),
            "--report",
            str(reports_dir / "hlfir" / "vector_add.json"),
            "--source",
            str(ROOT / "testcases" / "fortran" / "vector_add.f90"),
            "--output",
            str(output),
        ]
    )
    return output


def run_profile_refinement(reports_dir: Path) -> Path:
    print("\n[3/4] Profile-guided refinement")
    output = reports_dir / "refinement" / "function_result.refined.json"
    run(
        [
            resolve_python(),
            str(ROOT / "scripts" / "refine_profile.py"),
            "--report",
            str(reports_dir / "hlfir" / "function_result.json"),
            "--profile",
            str(ROOT / "profiles" / "sample_profile.csv"),
            "--out",
            str(output),
        ]
    )
    return output


def run_runtime_benchmark(reports_dir: Path, compiler: str | None, flang: Path, runs_count: int) -> Path:
    print("\n[4/4] Runtime baseline comparison")
    out = reports_dir / "benchmark" / "runtime.csv"
    chosen_compiler = compiler or str(flang)
    run(
        [
            resolve_python(),
            str(ROOT / "scripts" / "benchmark_fortran.py"),
            "--out",
            str(out),
            "--runs",
            str(runs_count),
            "--compiler",
            chosen_compiler,
        ]
    )
    return out


def main() -> int:
    parser = argparse.ArgumentParser(
        description="Run the real Fortran -> Flang HLFIR -> FIAP backend workflow."
    )
    parser.add_argument("--tool", default="", help="Path to fiap-opt. Defaults to build/fiap-opt(.exe).")
    parser.add_argument("--build-dir", default="build", type=Path)
    parser.add_argument("--reports-dir", default="reports", type=Path)
    parser.add_argument("--flang", default="", help="Path to flang/flang-new. Required for full end-to-end mode.")
    parser.add_argument("--compiler", default="", help="Optional Fortran compiler path for runtime benchmark.")
    parser.add_argument("--benchmark-runs", default=5, type=int)
    parser.add_argument("--skip-benchmark", action="store_true")
    parser.add_argument("--include-mlir-regression", action="store_true")
    parser.add_argument("--include-ctest", action="store_true")
    args = parser.parse_args()

    build_dir = (ROOT / args.build_dir).resolve() if not args.build_dir.is_absolute() else args.build_dir.resolve()
    reports_dir = (ROOT / args.reports_dir).resolve() if not args.reports_dir.is_absolute() else args.reports_dir.resolve()
    reports_dir.mkdir(parents=True, exist_ok=True)

    tool = resolve_fiap_tool(args.tool or None, build_dir)
    flang = resolve_flang(args.flang or None)

    print("")
    print("FIAP real end-to-end pipeline")
    print(f"source inputs: {rel(ROOT / 'testcases' / 'fortran')}")
    print(f"reports: {rel(reports_dir)}")

    ctest_status = run_ctest(build_dir) if args.include_ctest else "not run: optional MLIR/CMake regression"
    hlfir_summary = run_fortran_hlfir(tool, reports_dir, flang)
    transformed = run_source_transform(reports_dir)
    refined = run_profile_refinement(reports_dir)
    benchmark = None if args.skip_benchmark else run_runtime_benchmark(
        reports_dir,
        args.compiler or None,
        flang,
        args.benchmark_runs,
    )
    mlir_summary = run_mlir_regression(tool, reports_dir) if args.include_mlir_regression else None

    print("\nDone")
    print(f"Fortran HLFIR summary: {rel(hlfir_summary)}")
    print(f"source transform: {rel(transformed)}")
    print(f"profile refinement: {rel(refined)}")
    if benchmark:
        print(f"runtime benchmark: {rel(benchmark)}")
    if args.include_ctest:
        print(f"ctest: {ctest_status}")
    if mlir_summary:
        print(f"optional MLIR regression: {rel(mlir_summary)}")
    print("failure case: testcases/fortran/escaping_temp.f90 -> necessary")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
