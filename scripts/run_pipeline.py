#!/usr/bin/env python3
from __future__ import annotations

import argparse
import csv
import os
import re
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


def clean_generated_reports(reports_dir: Path) -> None:
    root = ROOT.resolve()
    target = reports_dir.resolve()
    try:
        target.relative_to(root)
    except ValueError:
        return

    for child_name in ("hlfir", "source", "profile", "refinement", "benchmark"):
        child = target / child_name
        if child.exists():
            shutil.rmtree(child)


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


def run_fortran_hlfir(tool: Path, reports_dir: Path, flang: Path) -> Path:
    print("\n[1/7] Flang HLFIR + FIAP analysis")
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


def generated_program_stems(reports_dir: Path) -> list[str]:
    return sorted(
        path.stem
        for path in (reports_dir / "hlfir").glob("*.mlir")
        if not path.stem.endswith(".transformed")
    )


def extract_mlir_module(output: str) -> str:
    lines = output.splitlines()
    for index, line in enumerate(lines):
        if line.startswith("module "):
            return "\n".join(lines[index:]) + "\n"
    raise SystemExit("fiap-opt did not print a transformed MLIR module")


def run_hlfir_transforms(tool: Path, reports_dir: Path) -> Path:
    print("\n[2/7] HLFIR/FIR transform rewrite")
    out_dir = reports_dir / "hlfir"
    summary = out_dir / "transforms.csv"
    rows = [
        "program,transformed_ir,applied_rewrites,original_hlfir_elementals,residual_hlfir_elementals,eliminated_hlfir_elementals"
    ]

    for stem in generated_program_stems(reports_dir):
        original_text = (out_dir / f"{stem}.mlir").read_text(encoding="utf-8")
        original = len(re.findall(r"^\s*%[\w\d_]+ = hlfir\.elemental\b", original_text, re.MULTILINE))
        completed = run(
            [
                str(tool),
                str(out_dir / f"{stem}.mlir"),
                "--apply-transforms",
                "--print-annotated-ir",
            ],
            print_output=False,
        )
        module_text = extract_mlir_module(completed.stdout)
        applied = module_text.count('fiap.rewrite_status = "applied-scalarization"')
        applied += module_text.count('fiap.rewrite_status = "applied-stack-promotion"')
        residual = len(re.findall(r"^\s*%[\w\d_]+ = hlfir\.elemental\b", module_text, re.MULTILINE))
        eliminated = max(original - residual, 0)
        transformed_ir = ""
        if applied > 0 or eliminated > 0:
            transformed_path = out_dir / f"{stem}.transformed.mlir"
            transformed_path.write_text(module_text, encoding="utf-8")
            transformed_ir = rel(transformed_path)
        rows.append(f"{stem},{transformed_ir},{applied},{original},{residual},{eliminated}")

    summary.write_text("\n".join(rows) + "\n", encoding="utf-8")
    return summary


def run_source_transform(reports_dir: Path) -> list[Path]:
    print("\n[3/7] Source transformation")
    outputs: list[Path] = []
    for stem in generated_program_stems(reports_dir):
        output = reports_dir / "source" / f"{stem}.transformed.f90"
        run(
            [
                resolve_python(),
                str(ROOT / "src" / "fiap_source_transformer.py"),
                "--report",
                str(reports_dir / "hlfir" / f"{stem}.json"),
                "--source",
                str(ROOT / "testcases" / "fortran" / f"{stem}.f90"),
                "--output",
                str(output),
            ]
        )
        outputs.append(output)
    return outputs


def find_fir_opt(explicit: str | None) -> Path | None:
    candidates: list[str | Path] = []
    if explicit:
        candidates.append(explicit)
    candidates.extend(os.environ.get(name, "") for name in ("FIAP_FIR_OPT", "FIR_OPT"))
    candidates.extend(found for name in ("fir-opt", "fir-opt.exe") if (found := shutil.which(name)))
    candidates.extend(
        [
            Path("D:/llvm-project/build/bin/fir-opt.exe"),
            Path("/mnt/d/llvm-project/build/bin/fir-opt"),
        ]
    )
    for candidate_value in candidates:
        if not str(candidate_value).strip():
            continue
        candidate = Path(candidate_value)
        if candidate.exists():
            return candidate.resolve()
    return None


def validate_generated_outputs(reports_dir: Path, flang: Path, fir_opt: Path | None) -> Path:
    print("\n[4/7] Generated artifact validation")
    out = reports_dir / "validation.csv"
    rows: list[dict[str, str]] = []

    for mlir_file in sorted((reports_dir / "hlfir").glob("*.transformed.mlir")):
        if fir_opt is None:
            rows.append({"artifact": rel(mlir_file), "validator": "fir-opt", "status": "skipped: fir-opt not found"})
            continue
        completed = run([str(fir_opt), str(mlir_file), "-o", os.devnull], print_output=False, allow_failure=True)
        rows.append(
            {
                "artifact": rel(mlir_file),
                "validator": "fir-opt",
                "status": "ok" if completed.returncode == 0 else "failed",
            }
        )

    for source_file in sorted((reports_dir / "source").glob("*.transformed.f90")):
        completed = run([str(flang), "-fsyntax-only", str(source_file)], print_output=False, allow_failure=True)
        rows.append(
            {
                "artifact": rel(source_file),
                "validator": "flang -fsyntax-only",
                "status": "ok" if completed.returncode == 0 else "failed",
            }
        )

    out.parent.mkdir(parents=True, exist_ok=True)
    with out.open("w", newline="", encoding="utf-8") as handle:
        writer = csv.DictWriter(handle, fieldnames=["artifact", "validator", "status"])
        writer.writeheader()
        writer.writerows(rows)

    failed = [row for row in rows if row["status"] == "failed"]
    if failed:
        names = ", ".join(row["artifact"] for row in failed)
        raise SystemExit(f"generated artifact validation failed for: {names}")
    return out


def run_profile_generation(reports_dir: Path) -> Path:
    print("\n[5/7] Profile-site generation")
    output = reports_dir / "profile" / "generated_profile.csv"
    run(
        [
            resolve_python(),
            str(ROOT / "scripts" / "generate_profile.py"),
            "--reports-dir",
            str(reports_dir / "hlfir"),
            "--out",
            str(output),
            "--observations",
            "20",
        ]
    )
    return output


def run_profile_refinement(reports_dir: Path, profile: Path) -> Path:
    print("\n[6/7] Profile-guided refinement")
    output = reports_dir / "refinement" / "function_result.refined.json"
    run(
        [
            resolve_python(),
            str(ROOT / "scripts" / "refine_profile.py"),
            "--report",
            str(reports_dir / "hlfir" / "function_result.json"),
            "--profile",
            str(profile),
            "--out",
            str(output),
        ]
    )
    return output


def run_runtime_benchmark(reports_dir: Path, compiler: str | None, flang: Path, runs_count: int) -> Path:
    print("\n[7/7] Runtime baseline comparison")
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
    parser.add_argument("--fir-opt", default="", help="Optional fir-opt path for transformed HLFIR validation.")
    parser.add_argument("--benchmark-runs", default=5, type=int)
    parser.add_argument("--skip-benchmark", action="store_true")
    args = parser.parse_args()

    build_dir = (ROOT / args.build_dir).resolve() if not args.build_dir.is_absolute() else args.build_dir.resolve()
    reports_dir = (ROOT / args.reports_dir).resolve() if not args.reports_dir.is_absolute() else args.reports_dir.resolve()
    clean_generated_reports(reports_dir)
    reports_dir.mkdir(parents=True, exist_ok=True)

    tool = resolve_fiap_tool(args.tool or None, build_dir)
    flang = resolve_flang(args.flang or None)
    fir_opt = find_fir_opt(args.fir_opt or None)

    print("")
    print("FIAP real end-to-end pipeline")
    print(f"source inputs: {rel(ROOT / 'testcases' / 'fortran')}")
    print(f"reports: {rel(reports_dir)}")

    hlfir_summary = run_fortran_hlfir(tool, reports_dir, flang)
    ir_transform_summary = run_hlfir_transforms(tool, reports_dir)
    transformed = run_source_transform(reports_dir)
    validation = validate_generated_outputs(reports_dir, flang, fir_opt)
    generated_profile = run_profile_generation(reports_dir)
    refined = run_profile_refinement(reports_dir, generated_profile)
    benchmark = None if args.skip_benchmark else run_runtime_benchmark(
        reports_dir,
        args.compiler or None,
        flang,
        args.benchmark_runs,
    )

    print("\nDone")
    print(f"Fortran HLFIR summary: {rel(hlfir_summary)}")
    print(f"HLFIR/FIR transform summary: {rel(ir_transform_summary)}")
    print(f"validation summary: {rel(validation)}")
    print("source transforms:")
    for transformed_path in transformed:
        print(f"  {rel(transformed_path)}")
    print(f"profile data: {rel(generated_profile)}")
    print(f"profile refinement: {rel(refined)}")
    if benchmark:
        print(f"runtime benchmark: {rel(benchmark)}")
    print("failure case: testcases/fortran/escaping_temp.f90 -> necessary")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
