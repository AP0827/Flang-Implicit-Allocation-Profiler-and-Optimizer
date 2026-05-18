#!/usr/bin/env python3
from __future__ import annotations

import argparse
import json
import re
from pathlib import Path


ASSIGNMENT = re.compile(
    r"^(?P<indent>\s*)(?P<lhs>[A-Za-z_][A-Za-z0-9_]*)\s*=\s*(?P<rhs>.+?)\s*$"
)
IDENTIFIER = re.compile(r"\b[A-Za-z_][A-Za-z0-9_]*\b")
KEYWORDS = {
    "abs",
    "cos",
    "do",
    "else",
    "end",
    "exp",
    "if",
    "lbound",
    "max",
    "min",
    "sin",
    "sqrt",
    "then",
    "ubound",
    "where",
}


def should_rewrite(entry: dict, source_path: Path) -> bool:
    if entry.get("classification") != "provably-eliminable":
        return False
    if entry.get("transform") != "scalarize-to-loop-nest":
        return False
    reported = Path(entry.get("file", ""))
    return reported.name == source_path.name


def index_expression(expression: str, lhs: str, index: str = "i") -> str:
    def replace(match: re.Match[str]) -> str:
        token = match.group(0)
        lower = token.lower()
        if token == lhs or lower in KEYWORDS:
            return token
        before = expression[match.start() - 1] if match.start() > 0 else ""
        after = expression[match.end()] if match.end() < len(expression) else ""
        if before == "%" or after == "(":
            return token
        return f"{token}({index})"

    return IDENTIFIER.sub(replace, expression)


def rewrite_line(line: str) -> str | None:
    match = ASSIGNMENT.match(line)
    if not match:
        return None
    rhs = match.group("rhs")
    if not any(op in rhs for op in ["+", "-", "*", "/"]):
        return None
    indent = match.group("indent")
    lhs = match.group("lhs")
    indexed_rhs = index_expression(rhs, lhs)
    return "\n".join(
        [
            f"{indent}do concurrent (i = lbound({lhs}, 1):ubound({lhs}, 1))",
            f"{indent}  {lhs}(i) = {indexed_rhs}",
            f"{indent}end do",
        ]
    )


def transform(report: dict, source_path: Path) -> tuple[str, list[str]]:
    lines = source_path.read_text(encoding="utf-8").splitlines()
    notes: list[str] = []
    for entry in report.get("entries", []):
        if not should_rewrite(entry, source_path):
            continue
        line_no = int(entry.get("line", 0))
        if line_no < 1 or line_no > len(lines):
            notes.append(f"skip line {line_no}: source location is outside the file")
            continue
        replacement = rewrite_line(lines[line_no - 1])
        if replacement is None:
            notes.append(f"skip line {line_no}: not a simple rank-1 assignment")
            continue
        original = lines[line_no - 1].strip()
        lines[line_no - 1] = replacement
        notes.append(f"rewrote line {line_no}: {original}")
    return "\n".join(lines) + "\n", notes


def main() -> int:
    parser = argparse.ArgumentParser(
        description="Apply simple source-level FIAP rewrites from a JSON report."
    )
    parser.add_argument("--report", required=True, type=Path)
    parser.add_argument("--source", required=True, type=Path)
    parser.add_argument("--output", required=True, type=Path)
    args = parser.parse_args()

    report = json.loads(args.report.read_text(encoding="utf-8-sig"))
    rewritten, notes = transform(report, args.source)
    args.output.parent.mkdir(parents=True, exist_ok=True)
    args.output.write_text(rewritten, encoding="utf-8")
    for note in notes:
        print(note)
    if not notes:
        print("no safe source rewrite was applied")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
