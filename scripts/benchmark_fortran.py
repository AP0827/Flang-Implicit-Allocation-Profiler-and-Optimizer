#!/usr/bin/env python3
from __future__ import annotations

import argparse
import csv
import shutil
import subprocess
import time
from pathlib import Path


def discover_compiler(explicit: str | None) -> str | None:
    if explicit:
        return explicit
    for name in ("flang-new", "flang", "gfortran", "ifx"):
        found = shutil.which(name)
        if found:
            return found
    return None


def compile_program(compiler: str, source: Path, exe: Path) -> None:
    exe = exe.resolve()
    exe.parent.mkdir(parents=True, exist_ok=True)
    subprocess.run(
        [compiler, "-O3", str(source.resolve()), "-o", str(exe)],
        check=True,
        text=True,
        stdout=subprocess.PIPE,
        stderr=subprocess.PIPE,
    )


def time_program(exe: Path, runs: int) -> float:
    exe = exe.resolve()
    timings: list[float] = []
    for _ in range(runs):
        start = time.perf_counter()
        subprocess.run(
            [str(exe)],
            check=True,
            text=True,
            stdout=subprocess.PIPE,
            stderr=subprocess.PIPE,
            cwd=str(exe.parent),
        )
        timings.append(time.perf_counter() - start)
    return min(timings)


def main() -> int:
    parser = argparse.ArgumentParser(
        description="Compile and time baseline/optimized Fortran programs when a compiler is available."
    )
    parser.add_argument("--compiler")
    parser.add_argument("--baseline-dir", default="testcases/fortran", type=Path)
    parser.add_argument("--optimized-dir", default="testcases/fortran_optimized", type=Path)
    parser.add_argument("--out", default="reports/runtime-benchmark.csv", type=Path)
    parser.add_argument("--runs", default=5, type=int)
    parser.add_argument("--build-dir", default="reports/fortran-bench-build", type=Path)
    args = parser.parse_args()

    compiler = discover_compiler(args.compiler)
    args.out.parent.mkdir(parents=True, exist_ok=True)

    fields = [
        "program",
        "compiler",
        "baseline_seconds",
        "optimized_seconds",
        "speedup_percent",
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
                    "speedup_percent": "",
                    "status": "skipped: install flang-new, flang, gfortran, or ifx",
                }
            )
        print(f"wrote {args.out}")
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
                    "speedup_percent": "",
                    "status": "skipped: optimized source missing",
                }
            )
            continue

        baseline_exe = args.build_dir / f"case_{case_index:02d}_baseline.exe"
        optimized_exe = args.build_dir / f"case_{case_index:02d}_optimized.exe"
        try:
            compile_program(compiler, baseline, baseline_exe)
            compile_program(compiler, optimized, optimized_exe)
            baseline_time = time_program(baseline_exe, args.runs)
            optimized_time = time_program(optimized_exe, args.runs)
        except subprocess.CalledProcessError as error:
            rows.append(
                {
                    "program": baseline.name,
                    "compiler": compiler,
                    "baseline_seconds": "",
                    "optimized_seconds": "",
                    "speedup_percent": "",
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
                    "speedup_percent": "",
                    "status": f"failed: {error}",
                }
            )
            continue

        speedup = 0.0 if baseline_time == 0 else (baseline_time - optimized_time) / baseline_time
        rows.append(
            {
                "program": baseline.name,
                "compiler": compiler,
                "baseline_seconds": f"{baseline_time:.6f}",
                "optimized_seconds": f"{optimized_time:.6f}",
                "speedup_percent": f"{speedup * 100.0:.2f}",
                "status": "ok",
            }
        )

    with args.out.open("w", newline="", encoding="utf-8") as handle:
        writer = csv.DictWriter(handle, fieldnames=fields)
        writer.writeheader()
        writer.writerows(rows)

    print(f"wrote {args.out}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
