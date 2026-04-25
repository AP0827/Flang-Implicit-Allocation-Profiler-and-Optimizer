from __future__ import annotations

from dataclasses import dataclass, field
from enum import Enum
from typing import Any


class AllocationKind(str, Enum):
    TEMPORARY_ARRAY = "temporary_array"
    ARRAY_FUNCTION_RESULT = "array_function_result"
    REALLOC_ON_ASSIGNMENT = "realloc_on_assignment"
    UNKNOWN = "unknown"


class AllocationClassification(str, Enum):
    PROVABLY_UNNECESSARY = "provably_unnecessary"
    POSSIBLY_UNNECESSARY = "possibly_unnecessary"
    NECESSARY = "necessary"


@dataclass(slots=True)
class SourceLocation:
    file: str
    line: int
    column: int | None = None


@dataclass(slots=True)
class AnalysisRecord:
    kind: AllocationKind
    location: SourceLocation
    expression: str
    source_construct: str
    bytes_allocated: int | None = None
    shape_match: bool | None = None
    provable_stack_safety: bool = False
    user_hint_needed: bool = False
    details: dict[str, Any] = field(default_factory=dict)


@dataclass(slots=True)
class AllocationSite:
    record: AnalysisRecord
    classification: AllocationClassification
    reason: str


@dataclass(slots=True)
class TransformationSuggestion:
    location: SourceLocation
    original: str
    rewritten: str
    rationale: str
