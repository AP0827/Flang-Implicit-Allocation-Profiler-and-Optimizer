#!/usr/bin/env python3
from __future__ import annotations

import argparse
import csv
import json
import re
from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]


def resolve_path(path_text: str) -> Path:
    path = Path(path_text)
    return path if path.is_absolute() else ROOT / path


def load_report(reports_dir: Path, stem: str) -> dict:
    path = reports_dir / "hlfir" / f"{stem}.json"
    if not path.exists():
        raise SystemExit(f"missing report: {path}")
    return json.loads(path.read_text(encoding="utf-8-sig"))


def entries(report: dict) -> list[dict]:
    return list(report.get("entries", []))


def require(condition: bool, message: str) -> None:
    if not condition:
        raise SystemExit(f"FAIL: {message}")


def require_entry(report: dict, predicate, message: str) -> dict:
    for entry in entries(report):
        if predicate(entry):
            return entry
    raise SystemExit(f"FAIL: {message}")


def read_transform_rows(reports_dir: Path) -> dict[str, dict[str, str]]:
    path = reports_dir / "hlfir" / "transforms.csv"
    if not path.exists():
        raise SystemExit(f"missing transform summary: {path}")
    with path.open(newline="", encoding="utf-8") as handle:
        return {row["program"]: row for row in csv.DictReader(handle)}


def check_vector_expression(reports_dir: Path) -> None:
    report = load_report(reports_dir, "vector_add")
    entry = require_entry(
        report,
        lambda item: item.get("classification") == "provably-eliminable"
        and item.get("sourceExpression") == "b + c"
        and item.get("legality") == "legal-for-rewrite",
        "vector_add should expose exact RHS expression and legal rewrite state",
    )
    require(entry.get("rank") == 1, "vector_add rank should be 1")


def check_alias_negative(reports_dir: Path) -> None:
    report = load_report(reports_dir, "pointer_alias")
    require_entry(
        report,
        lambda item: item.get("classification") == "necessary"
        and item.get("aliasRisk") is True
        and item.get("legality") == "illegal-for-local-rewrite",
        "pointer_alias should be blocked by alias evidence",
    )


def check_descriptor_negative(reports_dir: Path) -> None:
    report = load_report(reports_dir, "assumed_shape_kernel")
    require_entry(
        report,
        lambda item: item.get("classification") == "necessary"
        and item.get("shapeExtents") == "?"
        and item.get("aliasRisk") is True,
        "assumed_shape_kernel should be dynamic descriptor/alias-sensitive",
    )


def check_strided_section_negative(reports_dir: Path) -> None:
    report = load_report(reports_dir, "strided_section_update")
    require_entry(
        report,
        lambda item: item.get("classification") == "necessary"
        and item.get("sourceExpression") == "b(1:n:2) + c(1:n:2)"
        and item.get("aliasRisk") is True,
        "strided_section_update should preserve strided expression and block rewrite",
    )


def check_backend_rewrites(reports_dir: Path) -> None:
    rows = read_transform_rows(reports_dir)
    for program, minimum in {
        "rank3_tensor_update": 2,
        "polybench_jacobi1d": 4,
        "laplace2d_real_kernel": 5,
    }.items():
        row = rows.get(program)
        require(row is not None, f"{program} missing from transform summary")
        eliminated = int(row.get("eliminated_hlfir_elementals", "0") or 0)
        require(eliminated >= minimum, f"{program} eliminated {eliminated}, expected >= {minimum}")
        transformed = resolve_path(row.get("transformed_ir", ""))
        require(transformed.exists(), f"{program} transformed IR missing")
        text = transformed.read_text(encoding="utf-8")
        require('fiap.rewrite_status = "applied-scalarization"' in text,
                f"{program} lacks scalarization marker")
        require(re.search(r"\bfir\.do_loop\b", text) is not None,
                f"{program} transformed IR lacks fir.do_loop")


def main() -> int:
    parser = argparse.ArgumentParser(description="Run FIAP lit-style generated-evidence checks.")
    parser.add_argument("--reports-dir", default="reports", type=Path)
    args = parser.parse_args()

    reports_dir = args.reports_dir.resolve()
    checks = [
        check_vector_expression,
        check_alias_negative,
        check_descriptor_negative,
        check_strided_section_negative,
        check_backend_rewrites,
    ]
    for check in checks:
        check(reports_dir)
        print(f"PASS {check.__name__}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
