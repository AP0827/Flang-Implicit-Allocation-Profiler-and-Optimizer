#!/usr/bin/env python3
from __future__ import annotations

import argparse
import csv
import json
import subprocess
from pathlib import Path


def run_report(tool: Path, testcase: Path) -> dict:
    completed = subprocess.run(
        [str(tool), str(testcase), "--format=json"],
        check=True,
        text=True,
        stdout=subprocess.PIPE,
        stderr=subprocess.PIPE,
    )
    return json.loads(completed.stdout)


def summarize(testcase: Path, report: dict) -> dict[str, object]:
    entries = report.get("entries", [])
    total_bytes = 0
    removable_bytes = 0
    counts = {
        "provably-eliminable": 0,
        "possibly-unnecessary": 0,
        "necessary": 0,
    }

    for entry in entries:
        classification = entry.get("classification", "necessary")
        estimated = int(entry.get("estimatedBytes", 0) or 0)
        total_bytes += estimated
        counts[classification] = counts.get(classification, 0) + 1
        if classification == "provably-eliminable":
            removable_bytes += estimated

    optimized_bytes = total_bytes - removable_bytes
    reduction = 0.0 if total_bytes == 0 else removable_bytes / total_bytes
    return {
        "testcase": testcase.name,
        "sites": len(entries),
        "provably_eliminable": counts.get("provably-eliminable", 0),
        "possibly_unnecessary": counts.get("possibly-unnecessary", 0),
        "necessary": counts.get("necessary", 0),
        "baseline_estimated_bytes": total_bytes,
        "optimized_estimated_bytes": optimized_bytes,
        "estimated_byte_reduction": removable_bytes,
        "estimated_reduction_percent": round(reduction * 100.0, 2),
    }


def main() -> int:
    parser = argparse.ArgumentParser(
        description="Run fiap-opt over all MLIR testcases and write a CSV evaluation summary."
    )
    parser.add_argument("--tool", default="build/fiap-opt", type=Path)
    parser.add_argument("--testcases", default="testcases", type=Path)
    parser.add_argument("--out", default="reports/evaluation-summary.csv", type=Path)
    args = parser.parse_args()

    tool = args.tool
    if not tool.exists():
      windows_release = tool.parent / "Release" / "fiap-opt.exe"
      windows_plain = tool.with_suffix(".exe")
      if windows_release.exists():
          tool = windows_release
      elif windows_plain.exists():
          tool = windows_plain
      else:
          raise SystemExit(f"fiap-opt not found: {args.tool}")

    testcases = sorted(args.testcases.glob("*.mlir"))
    if not testcases:
        raise SystemExit(f"no .mlir testcases found in {args.testcases}")

    rows = [summarize(path, run_report(tool, path)) for path in testcases]
    args.out.parent.mkdir(parents=True, exist_ok=True)
    with args.out.open("w", newline="", encoding="utf-8") as handle:
        writer = csv.DictWriter(handle, fieldnames=list(rows[0].keys()))
        writer.writeheader()
        writer.writerows(rows)

    print(f"wrote {args.out}")
    for row in rows:
        print(
            f"{row['testcase']}: sites={row['sites']} "
            f"reduction={row['estimated_reduction_percent']}%"
        )
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
