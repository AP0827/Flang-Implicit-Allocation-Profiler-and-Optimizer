#!/usr/bin/env python3
from __future__ import annotations

import argparse
import csv
import json
import re
from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]

REQUIRED_SOURCES = {
    "vector_add.f90": "provably-eliminable",
    "matrix_stencil.f90": "provably-eliminable",
    "function_result.f90": "possibly-unnecessary",
    "allocatable_update.f90": "possibly-unnecessary",
    "escaping_temp.f90": "necessary",
    "saxpy_real_kernel.f90": "provably-eliminable",
    "laplace2d_real_kernel.f90": "provably-eliminable",
    "option_pricing_real_kernel.f90": "provably-eliminable",
    "rank3_tensor_update.f90": "provably-eliminable",
    "pointer_alias.f90": "necessary",
    "assumed_shape_kernel.f90": "necessary",
    "strided_section_update.f90": "necessary",
    "polybench_jacobi1d.f90": "provably-eliminable",
}

MUST_ELIMINATE = {
    "vector_add",
    "matrix_stencil",
    "saxpy_real_kernel",
    "laplace2d_real_kernel",
    "option_pricing_real_kernel",
    "rank3_tensor_update",
    "polybench_jacobi1d",
}


def read_csv(path: Path) -> list[dict[str, str]]:
    if not path.exists():
        raise SystemExit(f"missing required file: {path}")
    with path.open(newline="", encoding="utf-8") as handle:
        return list(csv.DictReader(handle))


def require(condition: bool, message: str) -> None:
    if not condition:
        raise SystemExit(message)


def source_name(row: dict[str, str]) -> str:
    return Path(row.get("source", "")).name


def resolve_generated_path(reports_dir: Path, value: str) -> Path:
    raw = Path(value)
    if raw.is_absolute():
        return raw

    candidates = [
        ROOT / raw,
        reports_dir / raw,
        reports_dir.parent / raw,
    ]
    for candidate in candidates:
        if candidate.exists():
            return candidate
    return candidates[0]


def check_summary(reports_dir: Path) -> None:
    rows = read_csv(reports_dir / "hlfir" / "summary.csv")
    by_source = {source_name(row): row for row in rows}
    missing = sorted(set(REQUIRED_SOURCES) - set(by_source))
    require(not missing, f"summary is missing sources: {', '.join(missing)}")

    for name, expected in REQUIRED_SOURCES.items():
        row = by_source[name]
        require(row.get("expected_primary_classification") == expected,
                f"{name}: expected classification field is wrong")
        require(row.get("expected_met") == "true",
                f"{name}: expected classification was not met")
        require(int(row.get("sites", "0") or 0) > 0, f"{name}: no sites found")
        sarif = row.get("sarif", "")
        require(sarif, f"{name}: SARIF path missing from summary")
        sarif_path = resolve_generated_path(reports_dir, sarif)
        require(sarif_path.exists(), f"{name}: SARIF report does not exist: {sarif}")
        sarif_json = json.loads(sarif_path.read_text(encoding="utf-8-sig"))
        require(sarif_json.get("version") == "2.1.0", f"{name}: invalid SARIF version")

        report_path = resolve_generated_path(reports_dir, row.get("report", ""))
        report = json.loads(report_path.read_text(encoding="utf-8-sig"))
        for entry in report.get("entries", []):
            require("legality" in entry, f"{name}: report entry missing legality")
            require("aliasRisk" in entry, f"{name}: report entry missing aliasRisk")
            require("rank" in entry, f"{name}: report entry missing rank")
            require("estimatedElements" in entry, f"{name}: report entry missing estimatedElements")
            require("sourceLine" in entry, f"{name}: report entry missing sourceLine")
            require("sourceExpression" in entry, f"{name}: report entry missing sourceExpression")
            require("shapeExtents" in entry, f"{name}: report entry missing shapeExtents")


def check_transforms(reports_dir: Path) -> None:
    rows = read_csv(reports_dir / "hlfir" / "transforms.csv")
    by_program = {row["program"]: row for row in rows}
    missing = sorted(MUST_ELIMINATE - set(by_program))
    require(not missing, f"transform summary is missing programs: {', '.join(missing)}")

    for program in MUST_ELIMINATE:
        row = by_program[program]
        eliminated = int(row.get("eliminated_hlfir_elementals", "0") or 0)
        residual = int(row.get("residual_hlfir_elementals", "0") or 0)
        transformed = row.get("transformed_ir", "")
        require(eliminated > 0, f"{program}: no HLFIR elementals were eliminated")
        require(residual == 0, f"{program}: residual HLFIR elementals remain")
        require(transformed, f"{program}: transformed IR path is empty")
        transformed_path = resolve_generated_path(reports_dir, transformed)
        require(transformed_path.exists(), f"{program}: transformed IR file does not exist: {transformed}")
        text = transformed_path.read_text(encoding="utf-8")
        require('fiap.rewrite_status = "applied-scalarization"' in text,
                f"{program}: transformed IR lacks applied-scalarization marker")
        require(not re.search(r"^\s*%[\w\d_]+ = hlfir\.elemental\b", text, re.MULTILINE),
                f"{program}: transformed IR still contains hlfir.elemental ops")

    escaping = by_program.get("escaping_temp")
    require(escaping is not None, "escaping_temp is missing from transform summary")
    require(int(escaping.get("eliminated_hlfir_elementals", "0") or 0) == 0,
            "escaping_temp should not be transformed")

    pointer_alias = by_program.get("pointer_alias")
    require(pointer_alias is not None, "pointer_alias is missing from transform summary")
    require(int(pointer_alias.get("eliminated_hlfir_elementals", "0") or 0) == 0,
            "pointer_alias should not be transformed without a stronger alias proof")

    assumed_shape = by_program.get("assumed_shape_kernel")
    require(assumed_shape is not None, "assumed_shape_kernel is missing from transform summary")
    require(int(assumed_shape.get("eliminated_hlfir_elementals", "0") or 0) == 0,
            "assumed_shape_kernel should not be transformed without descriptor alias proof")

    strided_section = by_program.get("strided_section_update")
    require(strided_section is not None, "strided_section_update is missing from transform summary")
    require(int(strided_section.get("eliminated_hlfir_elementals", "0") or 0) == 0,
            "strided_section_update should not be transformed without section-contiguity proof")


def check_validation(reports_dir: Path) -> None:
    rows = read_csv(reports_dir / "validation.csv")
    require(rows, "validation.csv is empty")
    failed = [row for row in rows if row.get("status") != "ok"]
    require(not failed, "validation failures: " + ", ".join(row["artifact"] for row in failed))


def check_benchmark(reports_dir: Path, require_benchmark: bool) -> None:
    benchmark = reports_dir / "benchmark" / "runtime.csv"
    if not benchmark.exists():
        if require_benchmark:
            raise SystemExit("benchmark/runtime.csv is missing")
        return
    rows = read_csv(benchmark)
    failed = [
        row
        for row in rows
        if row.get("status", "").startswith("failed")
        or row.get("outputs_match", "").lower() == "false"
    ]
    require(not failed, "benchmark failures: " + ", ".join(row["program"] for row in failed))


def check_profile_refinement(reports_dir: Path) -> None:
    profile = reports_dir / "profile" / "generated_profile.csv"
    profile_rows = read_csv(profile)
    require(profile_rows, f"profile output is empty: {profile}")
    for row in profile_rows:
        require(row.get("source_expression", "") != "",
                "profile row is missing source_expression")
        require(row.get("instrumentation_kind") == "shape-and-allocation-counter",
                "profile row is missing instrumentation kind")

    path = reports_dir / "refinement" / "function_result.refined.json"
    require(path.exists(), f"profile refinement output missing: {path}")
    report = json.loads(path.read_text(encoding="utf-8-sig"))
    entries = report.get("entries", [])
    require(any(entry.get("profileRefined") for entry in entries),
            "function_result refinement did not mark any profile-refined site")


def main() -> int:
    parser = argparse.ArgumentParser(description="Validate generated FIAP pipeline evidence.")
    parser.add_argument("--reports-dir", default="reports", type=Path)
    parser.add_argument("--require-benchmark", action="store_true")
    args = parser.parse_args()

    reports_dir = args.reports_dir.resolve()
    check_summary(reports_dir)
    check_transforms(reports_dir)
    check_validation(reports_dir)
    check_profile_refinement(reports_dir)
    check_benchmark(reports_dir, args.require_benchmark)
    print("pipeline evidence checks passed")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
