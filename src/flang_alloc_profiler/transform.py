from __future__ import annotations

from dataclasses import dataclass
from pathlib import Path
import re

from .models import AllocationSite, AllocationClassification, TransformationSuggestion


_SIMPLE_ASSIGNMENT = re.compile(r"^(?P<indent>\s*)(?P<lhs>[A-Za-z_][A-Za-z0-9_]*)\s*=\s*(?P<rhs>.+?)\s*$")
_IDENTIFIER = re.compile(r"\b[A-Za-z_][A-Za-z0-9_]*\b")
_FORTRAN_KEYWORDS = {
    "and",
    "do",
    "else",
    "end",
    "if",
    "or",
    "then",
    "where",
    "while",
}


@dataclass(slots=True)
class TransformationResult:
    source: str
    suggestions: list[TransformationSuggestion]


def rewrite_simple_temporary(source_text: str, site: AllocationSite) -> TransformationResult:
    lines = source_text.splitlines()
    if site.classification is not AllocationClassification.PROVABLY_UNNECESSARY:
        return TransformationResult(source_text, [])

    target_line = site.record.location.line - 1
    if target_line < 0 or target_line >= len(lines):
        return TransformationResult(source_text, [])

    match = _SIMPLE_ASSIGNMENT.match(lines[target_line])
    if not match:
        return TransformationResult(source_text, [])

    indent = match.group("indent")
    lhs = match.group("lhs")
    rhs = match.group("rhs")
    if "+" not in rhs and "*" not in rhs and "-" not in rhs and "/" not in rhs:
        return TransformationResult(source_text, [])

    indexed_rhs = _index_rhs_expression(rhs, "i", lhs)

    rewritten = [
        f"{indent}do concurrent (i = lbound({lhs}, 1):ubound({lhs}, 1))",
        f"{indent}  {lhs}(i) = {indexed_rhs}",
        f"{indent}end do",
    ]
    new_lines = list(lines)
    new_lines[target_line] = "\n".join(rewritten)

    suggestion = TransformationSuggestion(
        location=site.record.location,
        original=lines[target_line],
        rewritten="\n".join(rewritten),
        rationale="replace a temporary-producing array expression with an explicit elemental loop",
    )
    return TransformationResult("\n".join(new_lines), [suggestion])


def _index_rhs_expression(expression: str, index_variable: str, lhs: str) -> str:
    def replace_identifier(match: re.Match[str]) -> str:
        identifier = match.group(0)
        if identifier.lower() in _FORTRAN_KEYWORDS:
            return identifier
        if identifier == lhs:
            return identifier

        start = match.start()
        end = match.end()
        before = expression[start - 1] if start > 0 else ""
        after = expression[end] if end < len(expression) else ""
        if before == "%" or after == "(":
            return identifier
        return f"{identifier}({index_variable})"

    return _IDENTIFIER.sub(replace_identifier, expression)


def load_source(path: str | Path) -> str:
    return Path(path).read_text(encoding="utf-8")
