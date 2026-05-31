#!/usr/bin/env python3
from __future__ import annotations

import argparse
import csv
import os
import shutil
import subprocess
import time
import re
from statistics import median
from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]


def display_path(path: Path) -> str:
    try:
        return str(path.resolve().relative_to(ROOT))
    except ValueError:
        return str(path)


def discover_compiler(explicit: str | None) -> str | None:
    if explicit:
        return explicit
    for env_name in ("FIAP_FLANG", "FLANG", "FC"):
        value = os.environ.get(env_name)
        if value and Path(value).exists():
            return value
    for name in ("flang-new", "flang", "gfortran", "ifx"):
        found = shutil.which(name)
        if found:
            return found
    for fallback in (
        Path("/usr/lib/llvm-20/bin/flang-new"),
        Path("/usr/lib/llvm-19/bin/flang-new"),
        Path("/usr/lib/llvm-18/bin/flang-new"),
        Path("/usr/lib/llvm-20/bin/flang"),
        Path("/usr/lib/llvm-19/bin/flang"),
        Path("/usr/lib/llvm-18/bin/flang"),
    ):
        if fallback.exists():
            return str(fallback)
    return None


def compile_program(compiler: str, source: Path, exe: Path) -> None:
    exe = exe.resolve()
    exe.parent.mkdir(parents=True, exist_ok=True)
    command = [compiler, "-O3", str(source.resolve()), "-o", str(exe)]
    subprocess.run(
        command,
        check=True,
        text=True,
        stdout=subprocess.PIPE,
        stderr=subprocess.PIPE,
    )


NUMBER = re.compile(r"[-+]?(?:\d+(?:\.\d*)?|\.\d+)(?:[Ee][-+]?\d+)?")


def numeric_output(stdout: str) -> float | None:
    match = NUMBER.search(stdout)
    return float(match.group(0)) if match else None


def time_program(exe: Path, runs: int) -> tuple[float, float, str]:
    exe = exe.resolve()
    timings: list[float] = []
    last_stdout = ""
    for _ in range(runs):
        start = time.perf_counter()
        completed = subprocess.run(
            [str(exe)],
            check=True,
            text=True,
            stdout=subprocess.PIPE,
            stderr=subprocess.PIPE,
            cwd=str(exe.parent),
        )
        timings.append(time.perf_counter() - start)
        last_stdout = completed.stdout.strip()
    return median(timings), min(timings), last_stdout


def main() -> int:
    parser = argparse.ArgumentParser(
        description="Compile and time baseline/optimized Fortran programs when a compiler is available."
    )
    parser.add_argument("--compiler")
    parser.add_argument("--baseline-dir", default="testcases/fortran", type=Path)
    parser.add_argument("--optimized-dir", default="testcases/fortran_optimized", type=Path)
    parser.add_argument("--out", default="reports/benchmark/runtime.csv", type=Path)
    parser.add_argument("--runs", default=5, type=int)
    parser.add_argument("--build-dir", default="reports/benchmark/build", type=Path)
    args = parser.parse_args()

    compiler = discover_compiler(args.compiler)
    args.out.parent.mkdir(parents=True, exist_ok=True)

    fields = [
        "program",
        "compiler",
        "baseline_seconds",
        "optimized_seconds",
        "baseline_best_seconds",
        "optimized_best_seconds",
        "speedup_percent",
        "outputs_match",
        "output_delta",
        "runs",
        "status",
    ]

    if compiler is None:
        with args.out.open("w", newline="", encoding="utf-8") as handle:
            writer = csv.DictWriter(handle, fieldnames=fields)
            writer.writeheader()
            writer.writerow(
                {
                    "program": "*",
                    "compiler": "",
                    "baseline_seconds": "",
                    "optimized_seconds": "",
                    "baseline_best_seconds": "",
                    "optimized_best_seconds": "",
                    "speedup_percent": "",
                    "outputs_match": "",
                    "output_delta": "",
                    "runs": args.runs,
                    "status": "skipped: install flang-new, flang, gfortran, or ifx",
                }
            )
        print(f"wrote {display_path(args.out)}")
        print("no Fortran compiler found; install flang-new/flang/gfortran/ifx and rerun")
        return 0

    rows = []
    for case_index, baseline in enumerate(sorted(args.baseline_dir.glob("*.f90")), start=1):
        optimized = args.optimized_dir / baseline.name
        if not optimized.exists():
            rows.append(
                {
                    "program": baseline.name,
                    "compiler": compiler,
                    "baseline_seconds": "",
                    "optimized_seconds": "",
                    "baseline_best_seconds": "",
                    "optimized_best_seconds": "",
                    "speedup_percent": "",
                    "outputs_match": "",
                    "output_delta": "",
                    "runs": args.runs,
                    "status": "skipped: optimized source missing",
                }
            )
            continue

        baseline_exe = args.build_dir / f"case_{case_index:02d}_baseline"
        optimized_exe = args.build_dir / f"case_{case_index:02d}_optimized"
        try:
            compile_program(compiler, baseline, baseline_exe)
            compile_program(compiler, optimized, optimized_exe)
            baseline_time, baseline_best, baseline_stdout = time_program(baseline_exe, args.runs)
            optimized_time, optimized_best, optimized_stdout = time_program(optimized_exe, args.runs)
        except subprocess.CalledProcessError as error:
            rows.append(
                {
                    "program": baseline.name,
                    "compiler": compiler,
                    "baseline_seconds": "",
                    "optimized_seconds": "",
                    "baseline_best_seconds": "",
                    "optimized_best_seconds": "",
                    "speedup_percent": "",
                    "outputs_match": "",
                    "output_delta": "",
                    "runs": args.runs,
                    "status": f"failed: {error.stderr.strip() or error}",
                }
            )
            continue
        except OSError as error:
            rows.append(
                {
                    "program": baseline.name,
                    "compiler": compiler,
                    "baseline_seconds": "",
                    "optimized_seconds": "",
                    "baseline_best_seconds": "",
                    "optimized_best_seconds": "",
                    "speedup_percent": "",
                    "outputs_match": "",
                    "output_delta": "",
                    "runs": args.runs,
                    "status": f"failed: {error}",
                }
            )
            continue

        speedup = 0.0 if baseline_time == 0 else (baseline_time - optimized_time) / baseline_time
        baseline_value = numeric_output(baseline_stdout)
        optimized_value = numeric_output(optimized_stdout)
        if baseline_value is None or optimized_value is None:
            outputs_match = baseline_stdout == optimized_stdout
            output_delta = ""
        else:
            output_delta_value = abs(baseline_value - optimized_value)
            tolerance = max(1.0e-3, abs(baseline_value) * 1.0e-4)
            outputs_match = output_delta_value <= tolerance
            output_delta = f"{output_delta_value:.6g}"
        status = "ok"
        if abs(speedup) < 0.05:
            status = "ok: small/noisy runtime delta"
        if not outputs_match:
            status = "failed: optimized output differs"
        rows.append(
            {
                "program": baseline.name,
                "compiler": compiler,
                "baseline_seconds": f"{baseline_time:.6f}",
                "optimized_seconds": f"{optimized_time:.6f}",
                "baseline_best_seconds": f"{baseline_best:.6f}",
                "optimized_best_seconds": f"{optimized_best:.6f}",
                "speedup_percent": f"{speedup * 100.0:.2f}",
                "outputs_match": str(outputs_match).lower(),
                "output_delta": output_delta,
                "runs": args.runs,
                "status": status,
            }
        )

    with args.out.open("w", newline="", encoding="utf-8") as handle:
        writer = csv.DictWriter(handle, fieldnames=fields)
        writer.writeheader()
        writer.writerows(rows)

    print(f"wrote {display_path(args.out)}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
