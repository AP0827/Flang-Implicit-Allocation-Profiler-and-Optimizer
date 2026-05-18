#!/usr/bin/env python3
from __future__ import annotations

import argparse
import csv
import json
from pathlib import Path


def key_for(entry: dict) -> tuple[str, int, int]:
    return (
        Path(entry.get("file", "")).name,
        int(entry.get("line", 0) or 0),
        int(entry.get("column", 0) or 0),
    )


def load_profile(path: Path) -> dict[tuple[str, int, int], dict[str, str]]:
    rows: dict[tuple[str, int, int], dict[str, str]] = {}
    with path.open(newline="", encoding="utf-8-sig") as handle:
        for row in csv.DictReader(handle):
            key = (
                Path(row["file"]).name,
                int(row.get("line", 0) or 0),
                int(row.get("column", 0) or 0),
            )
            rows[key] = row
    return rows


def is_truthy(value: str | None) -> bool:
    return str(value or "").strip().lower() in {"1", "true", "yes", "y"}


def refine_entry(entry: dict, profile_row: dict[str, str]) -> dict:
    refined = dict(entry)
    stable_shape = is_truthy(profile_row.get("stable_shape"))
    observations = int(profile_row.get("observations", 0) or 0)

    refined["profile"] = {
        "observations": observations,
        "stableShape": stable_shape,
        "observedBytes": int(profile_row.get("observed_bytes", 0) or 0),
        "allocationCount": int(profile_row.get("allocation_count", 0) or 0),
    }

    if (
        stable_shape
        and observations > 0
        and refined.get("classification") == "possibly-unnecessary"
    ):
        refined["classification"] = "provably-eliminable"
        refined["profileRefined"] = True
        refined["reason"] = (
            "profile-validated shape invariant: "
            + str(refined.get("reason", "runtime shape was ambiguous"))
        )
        if refined.get("transform") == "none":
            refined["transform"] = "add-shape-guard"
        refined["advice"] = (
            "guard the optimized path with the observed shape invariant; "
            + str(refined.get("advice", ""))
        ).strip()
    else:
        refined["profileRefined"] = False

    return refined


def recompute_summary(entries: list[dict]) -> dict:
    summary = {
        "totalSites": len(entries),
        "provablyEliminable": 0,
        "possiblyUnnecessary": 0,
        "necessary": 0,
        "totalEstimatedBytes": 0,
    }
    for entry in entries:
        classification = entry.get("classification", "necessary")
        if classification == "provably-eliminable":
            summary["provablyEliminable"] += 1
        elif classification == "possibly-unnecessary":
            summary["possiblyUnnecessary"] += 1
        else:
            summary["necessary"] += 1
        summary["totalEstimatedBytes"] += int(entry.get("estimatedBytes", 0) or 0)
    return summary


def main() -> int:
    parser = argparse.ArgumentParser(
        description="Refine FIAP JSON classifications using profile-validated shape data."
    )
    parser.add_argument("--report", required=True, type=Path)
    parser.add_argument("--profile", required=True, type=Path)
    parser.add_argument("--out", required=True, type=Path)
    args = parser.parse_args()

    report = json.loads(args.report.read_text(encoding="utf-8-sig"))
    profile = load_profile(args.profile)

    refined_entries = []
    for entry in report.get("entries", []):
        profile_row = profile.get(key_for(entry))
        refined_entries.append(
            refine_entry(entry, profile_row) if profile_row is not None else entry
        )

    refined_report = dict(report)
    refined_report["entries"] = refined_entries
    refined_report["summary"] = recompute_summary(refined_entries)
    refined_report["profileGuidedRefinement"] = {
        "profile": str(args.profile),
        "matchedSites": sum(1 for entry in report.get("entries", []) if key_for(entry) in profile),
    }

    args.out.parent.mkdir(parents=True, exist_ok=True)
    args.out.write_text(json.dumps(refined_report, indent=2), encoding="utf-8")
    print(f"wrote {args.out}")
    print(
        "profile-refined sites="
        f"{sum(1 for entry in refined_entries if entry.get('profileRefined'))}"
    )
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
