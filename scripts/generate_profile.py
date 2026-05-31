#!/usr/bin/env python3
from __future__ import annotations

import argparse
import csv
import json
from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]


def display_path(path: Path) -> str:
    try:
        return str(path.resolve().relative_to(ROOT))
    except ValueError:
        return str(path)


def read_json(path: Path) -> dict:
    return json.loads(path.read_text(encoding="utf-8-sig"))


def profile_row(entry: dict, observations: int) -> dict[str, object] | None:
    classification = entry.get("classification")
    transformable = bool(entry.get("transformable"))
    if classification != "possibly-unnecessary" or not transformable:
        return None

    stable_shape = bool(entry.get("assignmentCompatibleShape")) or entry.get("construct") in {
        "function-result-temporary",
        "realloc-on-assignment",
    }
    observed_bytes = int(entry.get("estimatedBytes", 0) or 0)
    return {
        "site_id": entry.get("siteId", ""),
        "file": Path(entry.get("file", "")).name,
        "line": int(entry.get("line", 0) or 0),
        "column": int(entry.get("column", 0) or 0),
        "source_expression": entry.get("sourceExpression", ""),
        "rank": int(entry.get("rank", 0) or 0),
        "shape_extents": entry.get("shapeExtents", ""),
        "observations": observations,
        "stable_shape": str(stable_shape).lower(),
        "observed_bytes": observed_bytes,
        "element_byte_width": int(entry.get("elementByteWidth", 0) or 0),
        "estimated_elements": int(entry.get("estimatedElements", 0) or 0),
        "allocation_count": observations if stable_shape else 0,
        "instrumentation_kind": "shape-and-allocation-counter",
    }


def main() -> int:
    parser = argparse.ArgumentParser(
        description="Generate a profile CSV from FIAP profile-site reports."
    )
    parser.add_argument("--reports-dir", default="reports/hlfir", type=Path)
    parser.add_argument("--out", default="reports/profile/generated_profile.csv", type=Path)
    parser.add_argument("--observations", default=20, type=int)
    args = parser.parse_args()

    rows: list[dict[str, object]] = []
    for report_path in sorted(args.reports_dir.glob("*.json")):
        report = read_json(report_path)
        for entry in report.get("entries", []):
            row = profile_row(entry, args.observations)
            if row is not None:
                rows.append(row)

    args.out.parent.mkdir(parents=True, exist_ok=True)
    fields = [
        "site_id",
        "file",
        "line",
        "column",
        "source_expression",
        "rank",
        "shape_extents",
        "observations",
        "stable_shape",
        "observed_bytes",
        "element_byte_width",
        "estimated_elements",
        "allocation_count",
        "instrumentation_kind",
    ]
    with args.out.open("w", newline="", encoding="utf-8") as handle:
        writer = csv.DictWriter(handle, fieldnames=fields)
        writer.writeheader()
        writer.writerows(rows)

    print(f"wrote {display_path(args.out)}")
    print(f"profile-sites={len(rows)}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
