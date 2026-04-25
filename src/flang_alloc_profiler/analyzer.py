from __future__ import annotations

from dataclasses import dataclass
from pathlib import Path
from typing import Iterable

import json

from .models import (
    AllocationClassification,
    AllocationKind,
    AllocationSite,
    AnalysisRecord,
    SourceLocation,
)


@dataclass(slots=True)
class AnalysisSummary:
    sites: list[AllocationSite]

    @property
    def total_bytes(self) -> int:
        return sum(site.record.bytes_allocated or 0 for site in self.sites)


class AllocationAnalyzer:
    """Classifies implicit allocation records produced from Flang HLFIR/FIR dumps."""

    def analyze(self, records: Iterable[AnalysisRecord]) -> AnalysisSummary:
        return AnalysisSummary([self._classify(record) for record in records])

    def load_records(self, path: str | Path) -> list[AnalysisRecord]:
        payload = json.loads(Path(path).read_text(encoding="utf-8"))
        entries = payload if isinstance(payload, list) else payload.get("allocations", [])
        return [self._record_from_json(entry) for entry in entries]

    def _record_from_json(self, entry: dict) -> AnalysisRecord:
        location = SourceLocation(
            file=entry["file"],
            line=int(entry["line"]),
            column=int(entry["column"]) if entry.get("column") is not None else None,
        )
        return AnalysisRecord(
            kind=AllocationKind(entry.get("kind", AllocationKind.UNKNOWN.value)),
            location=location,
            expression=entry.get("expression", ""),
            source_construct=entry.get("source_construct", ""),
            bytes_allocated=entry.get("bytes_allocated"),
            shape_match=entry.get("shape_match"),
            provable_stack_safety=bool(entry.get("provable_stack_safety", False)),
            user_hint_needed=bool(entry.get("user_hint_needed", False)),
            details=entry.get("details", {}),
        )

    def _classify(self, record: AnalysisRecord) -> AllocationSite:
        if record.kind is AllocationKind.REALLOC_ON_ASSIGNMENT:
            if record.shape_match is False:
                return AllocationSite(record, AllocationClassification.NECESSARY, "shape mismatch requires reallocation")
            if record.shape_match is True:
                return AllocationSite(record, AllocationClassification.PROVABLY_UNNECESSARY, "reallocation can be removed when shapes already match")
            return AllocationSite(record, AllocationClassification.POSSIBLY_UNNECESSARY, "shape information is incomplete")

        if record.kind is AllocationKind.ARRAY_FUNCTION_RESULT:
            if record.provable_stack_safety:
                return AllocationSite(record, AllocationClassification.PROVABLY_UNNECESSARY, "function result can be stack-allocated or inlined")
            if record.user_hint_needed:
                return AllocationSite(record, AllocationClassification.POSSIBLY_UNNECESSARY, "needs user intent to remove result allocation")
            return AllocationSite(record, AllocationClassification.NECESSARY, "array-valued result is required by the source semantics")

        if record.kind is AllocationKind.TEMPORARY_ARRAY:
            if record.provable_stack_safety:
                return AllocationSite(record, AllocationClassification.PROVABLY_UNNECESSARY, "temporary is not semantically required")
            if record.user_hint_needed:
                return AllocationSite(record, AllocationClassification.POSSIBLY_UNNECESSARY, "may disappear after refactoring or alias analysis")
            return AllocationSite(record, AllocationClassification.NECESSARY, "temporary is required by the current lowering")

        return AllocationSite(record, AllocationClassification.POSSIBLY_UNNECESSARY, "unrecognized allocation kind")
