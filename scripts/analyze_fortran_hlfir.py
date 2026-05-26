#!/usr/bin/env python3
from __future__ import annotations

import argparse
import csv
import json
import os
import shutil
import subprocess
from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]

EXPECTED_PRIMARY_CLASSIFICATION = {
    "vector_add.f90": "provably-eliminable",
    "matrix_stencil.f90": "provably-eliminable",
    "function_result.f90": "possibly-unnecessary",
    "allocatable_update.f90": "possibly-unnecessary",
    "escaping_temp.f90": "necessary",
}


def resolve_tool(
    path: str | Path | None,
    fallback_exe: str | None = None,
    env_names: tuple[str, ...] = (),
    command_names: tuple[str, ...] = (),
    fallback_paths: tuple[Path, ...] = (),
) -> Path:
    candidates: list[str | Path] = []
    if path:
        candidates.append(path)
    candidates.extend(os.environ.get(name, "") for name in env_names)
    candidates.extend(found for name in command_names if (found := shutil.which(name)))
    candidates.extend(fallback_paths)

    for candidate_value in candidates:
        if not str(candidate_value).strip():
            continue
        candidate = Path(candidate_value)
        if candidate.exists():
            return candidate
        if fallback_exe:
            executable_candidate = candidate.with_suffix(fallback_exe)
            if executable_candidate.exists():
                return executable_candidate

    searched = ", ".join([str(path or ""), *env_names, *command_names]).strip(", ")
    raise SystemExit(f"required tool not found: {searched}")


def run(command: list[str]) -> subprocess.CompletedProcess[str]:
    return subprocess.run(
        command,
        check=True,
        text=True,
        stdout=subprocess.PIPE,
        stderr=subprocess.PIPE,
    )


def display_path(path: Path) -> str:
    try:
        return str(path.resolve().relative_to(ROOT))
    except ValueError:
        return str(path)


def summarize_report(source: Path, hlfir: Path, report_path: Path, report: dict) -> dict[str, object]:
    entries = report.get("entries", [])
    counts = {
        "provably-eliminable": 0,
        "possibly-unnecessary": 0,
        "necessary": 0,
    }
    total_bytes = 0
    removable_bytes = 0
    constructs: set[str] = set()

    for entry in entries:
        classification = entry.get("classification", "necessary")
        estimated = int(entry.get("estimatedBytes", 0) or 0)
        counts[classification] = counts.get(classification, 0) + 1
        total_bytes += estimated
        constructs.add(str(entry.get("construct", "unknown")))
        if classification == "provably-eliminable":
            removable_bytes += estimated

    reduction = 0.0 if total_bytes == 0 else (removable_bytes / total_bytes) * 100.0
    expected = EXPECTED_PRIMARY_CLASSIFICATION.get(source.name, "")
    expected_met = "" if not expected else str(counts.get(expected, 0) > 0).lower()
    return {
        "source": display_path(source),
        "generated_hlfir": display_path(hlfir),
        "report": display_path(report_path),
        "sites": len(entries),
        "provably_eliminable": counts.get("provably-eliminable", 0),
        "possibly_unnecessary": counts.get("possibly-unnecessary", 0),
        "necessary": counts.get("necessary", 0),
        "expected_primary_classification": expected,
        "expected_met": expected_met,
        "estimated_bytes": total_bytes,
        "estimated_removable_bytes": removable_bytes,
        "estimated_reduction_percent": round(reduction, 2),
        "constructs": ";".join(sorted(constructs)),
    }


def main() -> int:
    parser = argparse.ArgumentParser(
        description=(
            "Analyze Fortran sources by first asking Flang to emit HLFIR, "
            "then running fiap-opt on the generated HLFIR."
        )
    )
    parser.add_argument("--flang", default="", help="Path to flang/flang-new. Falls back to FIAP_FLANG, FLANG, then PATH.")
    parser.add_argument("--tool", default="build/fiap-opt.exe", type=Path)
    parser.add_argument("--source-dir", default="testcases/fortran", type=Path)
    parser.add_argument("--out-dir", default="reports/hlfir", type=Path)
    parser.add_argument("--summary", default="reports/hlfir/summary.csv", type=Path)
    parser.add_argument(
        "--strict",
        action="store_true",
        help="fail if known Fortran cases miss their expected primary classification",
    )
    args = parser.parse_args()

    flang = resolve_tool(
        args.flang,
        env_names=("FIAP_FLANG", "FLANG"),
        command_names=("flang-new", "flang", "flang-new.exe", "flang.exe"),
        fallback_paths=(
            Path(Path.cwd().anchor) / "llvm-project" / "build" / "bin" / "flang.exe",
            Path(Path.cwd().anchor) / "llvm-project" / "build" / "bin" / "flang-new.exe",
        ),
    )
    tool = resolve_tool(args.tool, ".exe")
    sources = sorted(path for path in args.source_dir.glob("*.f90"))
    if not sources:
        raise SystemExit(f"no .f90 files found in {args.source_dir}")

    args.out_dir.mkdir(parents=True, exist_ok=True)
    args.summary.parent.mkdir(parents=True, exist_ok=True)

    rows = []
    for source in sources:
        hlfir_path = args.out_dir / f"{source.stem}.mlir"
        report_path = args.out_dir / f"{source.stem}.json"

        run(
            [
                str(flang),
                "-fc1",
                "-emit-hlfir",
                "-mmlir",
                "--mlir-print-debuginfo",
                "-o",
                str(hlfir_path),
                str(source),
            ]
        )
        completed = run([str(tool), str(hlfir_path), "--format=json"])
        report_path.write_text(completed.stdout, encoding="utf-8")
        report = json.loads(completed.stdout)
        rows.append(summarize_report(source, hlfir_path, report_path, report))
        expected_text = ""
        if rows[-1]["expected_primary_classification"]:
            expected_text = (
                f", expected={rows[-1]['expected_primary_classification']}, "
                f"met={rows[-1]['expected_met']}"
            )
        print(
            f"{source.name}: HLFIR generated, sites={rows[-1]['sites']}, "
            f"reduction={rows[-1]['estimated_reduction_percent']}%"
            f"{expected_text}"
        )

    with args.summary.open("w", newline="", encoding="utf-8") as handle:
        writer = csv.DictWriter(handle, fieldnames=list(rows[0].keys()))
        writer.writeheader()
        writer.writerows(rows)

    print(f"wrote {display_path(args.summary)}")
    if args.strict:
        failed = [
            row
            for row in rows
            if row["expected_primary_classification"] and row["expected_met"] != "true"
        ]
        if failed:
            names = ", ".join(Path(str(row["source"])).name for row in failed)
            raise SystemExit(f"strict Fortran-HLFIR evaluation failed for: {names}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
