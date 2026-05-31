#!/usr/bin/env python3
from __future__ import annotations

import argparse
import json
import re
from pathlib import Path


ASSIGNMENT = re.compile(
    r"^(?P<indent>\s*)(?P<lhs>[A-Za-z_][A-Za-z0-9_]*)\s*=\s*(?P<rhs>.+?)\s*$"
)
FUNCTION_CALL_ASSIGNMENT = re.compile(
    r"^(?P<indent>\s*)(?P<lhs>[A-Za-z_][A-Za-z0-9_]*)\s*=\s*"
    r"(?P<func>[A-Za-z_][A-Za-z0-9_]*)\((?P<args>.*)\)\s*$"
)
FUNCTION_DEF = re.compile(
    r"^(?P<indent>\s*)function\s+(?P<name>[A-Za-z_][A-Za-z0-9_]*)"
    r"\((?P<args>[^)]*)\)\s+result\((?P<result>[A-Za-z_][A-Za-z0-9_]*)\)\s*$",
    re.IGNORECASE,
)
END_FUNCTION = re.compile(
    r"^(?P<indent>\s*)end\s+function(?:\s+(?P<name>[A-Za-z_][A-Za-z0-9_]*))?\s*$",
    re.IGNORECASE,
)
DECLARATION = re.compile(
    r"^\s*(real|integer|logical|complex|character)\b(?P<attrs>[^:]*)::(?P<vars>.+)$",
    re.IGNORECASE,
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
MAX_FORTRAN_RANK = 15
INDEX_NAMES = ["i", "j", "k", "l", "m", "p"] + [
    f"idx{rank}" for rank in range(7, MAX_FORTRAN_RANK + 1)
]


def split_top_level_commas(text: str) -> list[str]:
    parts: list[str] = []
    current: list[str] = []
    depth = 0
    for char in text:
        if char == "(":
            depth += 1
        elif char == ")":
            depth = max(0, depth - 1)
        if char == "," and depth == 0:
            parts.append("".join(current).strip())
            current = []
            continue
        current.append(char)
    if current:
        parts.append("".join(current).strip())
    return parts


def parse_declarations(lines: list[str]) -> dict[str, dict[str, object]]:
    declarations: dict[str, dict[str, object]] = {}
    for line in lines:
        stripped = line.split("!", 1)[0]
        match = DECLARATION.match(stripped)
        if not match:
            continue
        attrs = match.group("attrs").lower()
        allocatable = "allocatable" in attrs
        for item in split_top_level_commas(match.group("vars")):
            item = item.split("=", 1)[0].strip()
            var_match = re.match(r"(?P<name>[A-Za-z_][A-Za-z0-9_]*)(?:\((?P<dims>.*)\))?$", item)
            if not var_match:
                continue
            name = var_match.group("name")
            dims = var_match.group("dims")
            rank = 0 if dims is None else len(split_top_level_commas(dims))
            declarations[name.lower()] = {
                "rank": rank,
                "allocatable": allocatable,
            }
    return declarations


def read_json_file(path: Path) -> dict:
    for encoding in ("utf-8-sig", "utf-16"):
        try:
            return json.loads(path.read_text(encoding=encoding))
        except UnicodeDecodeError:
            continue
    return json.loads(path.read_text(encoding="utf-8", errors="replace"))


def should_rewrite(entry: dict, source_path: Path) -> bool:
    if entry.get("classification") != "provably-eliminable":
        return False
    if entry.get("transform") != "scalarize-to-loop-nest":
        return False
    reported = Path(entry.get("file", ""))
    return reported.name == source_path.name


def index_expression(
    expression: str,
    lhs: str,
    declarations: dict[str, dict[str, object]],
    indices: list[str],
) -> str:
    def replace(match: re.Match[str]) -> str:
        token = match.group(0)
        lower = token.lower()
        if lower in KEYWORDS:
            return token
        before = expression[match.start() - 1] if match.start() > 0 else ""
        after = expression[match.end()] if match.end() < len(expression) else ""
        if before == "%" or after == "(":
            return token
        declaration = declarations.get(lower)
        if not declaration or int(declaration.get("rank", 0) or 0) != len(indices):
            return token
        return f"{token}({', '.join(indices)})"

    return IDENTIFIER.sub(replace, expression)


def rewrite_array_expression(
    line: str,
    declarations: dict[str, dict[str, object]],
) -> str | None:
    match = ASSIGNMENT.match(line)
    if not match:
        return None
    rhs = match.group("rhs")
    if not any(op in rhs for op in ["+", "-", "*", "/"]):
        return None
    indent = match.group("indent")
    lhs = match.group("lhs")
    rank = int(declarations.get(lhs.lower(), {}).get("rank", 0) or 0)
    if rank < 1 or rank > MAX_FORTRAN_RANK:
        return None

    indices = INDEX_NAMES[:rank]
    indexed_rhs = index_expression(rhs, lhs, declarations, indices)
    lhs_indexed = f"{lhs}({', '.join(indices)})"
    loop_specs = [
        f"{index} = lbound({lhs}, {dim}):ubound({lhs}, {dim})"
        for dim, index in reversed(list(enumerate(indices, start=1)))
    ]
    loop_header = f"do concurrent ({', '.join(loop_specs)})"
    return "\n".join(
        [
            f"{indent}{loop_header}",
            f"{indent}  {lhs_indexed} = {indexed_rhs}",
            f"{indent}end do",
        ]
    )


def rewrite_allocatable_guard(
    line: str,
    declarations: dict[str, dict[str, object]],
) -> str | None:
    match = ASSIGNMENT.match(line)
    if not match:
        return None
    lhs = match.group("lhs")
    rhs = match.group("rhs").strip()
    if not re.fullmatch(r"[A-Za-z_][A-Za-z0-9_]*", rhs):
        return None

    lhs_decl = declarations.get(lhs.lower(), {})
    rhs_decl = declarations.get(rhs.lower(), {})
    if not lhs_decl.get("allocatable") or not rhs_decl.get("allocatable"):
        return None
    rank = int(lhs_decl.get("rank", 0) or 0)
    if rank < 1 or rank != int(rhs_decl.get("rank", 0) or 0):
        return None

    indent = match.group("indent")
    allocation_shape = ", ".join(f"size({rhs}, {dim})" for dim in range(1, rank + 1))
    return "\n".join(
        [
            f"{indent}if (.not. allocated({lhs})) then",
            f"{indent}  allocate({lhs}({allocation_shape}))",
            f"{indent}else if (any(shape({lhs}) /= shape({rhs}))) then",
            f"{indent}  deallocate({lhs})",
            f"{indent}  allocate({lhs}({allocation_shape}))",
            f"{indent}end if",
            f"{indent}{lhs} = {rhs}",
        ]
    )


def rewrite_function_result_call(line: str) -> tuple[str, str] | None:
    match = FUNCTION_CALL_ASSIGNMENT.match(line)
    if not match:
        return None
    args = match.group("args").strip()
    lhs = match.group("lhs")
    func = match.group("func")
    call_args = f"{args}, {lhs}" if args else lhs
    return f"{match.group('indent')}call {func}({call_args})", func.lower()


def convert_function_results_to_subroutines(lines: list[str], function_names: set[str]) -> list[str]:
    if not function_names:
        return lines

    converted = list(lines)
    active_function: str | None = None
    active_result: str | None = None

    for index, line in enumerate(converted):
        def_match = FUNCTION_DEF.match(line)
        if def_match and def_match.group("name").lower() in function_names:
            active_function = def_match.group("name")
            active_result = def_match.group("result")
            args = def_match.group("args").strip()
            subroutine_args = f"{args}, {active_result}" if args else active_result
            converted[index] = (
                f"{def_match.group('indent')}subroutine {active_function}"
                f"({subroutine_args})"
            )
            continue

        if active_function and active_result:
            stripped = line.split("!", 1)[0]
            declaration = DECLARATION.match(stripped)
            if declaration:
                items = split_top_level_commas(declaration.group("vars"))
                if len(items) == 1:
                    item = items[0].strip()
                    result_decl = re.match(
                        rf"(?P<name>{re.escape(active_result)})(?P<dims>\(.*\))?\s*$",
                        item,
                        re.IGNORECASE,
                    )
                    if result_decl:
                        type_part = line.split("::", 1)[0].strip()
                        if "intent(" not in type_part.lower():
                            type_part = f"{type_part}, intent(out)"
                        converted[index] = (
                            f"{line[: len(line) - len(line.lstrip())]}"
                            f"{type_part} :: {active_result}{result_decl.group('dims') or ''}"
                        )
                        continue

            end_match = END_FUNCTION.match(line)
            if end_match:
                converted[index] = (
                    f"{end_match.group('indent')}end subroutine {active_function}"
                )
                active_function = None
                active_result = None

    return converted


def ensure_indices_declared(lines: list[str], indices: set[str]) -> None:
    missing = []
    for index in sorted(indices):
        declaration = re.compile(rf"^\s*integer\b.*\b{re.escape(index)}\b", re.IGNORECASE)
        if not any(declaration.search(line) for line in lines):
            missing.append(index)
    if not missing:
        return

    for offset, line in enumerate(lines):
        if line.strip().lower() == "implicit none":
            indent = line[: len(line) - len(line.lstrip())]
            lines.insert(offset + 1, f"{indent}integer :: {', '.join(missing)}")
            return


def transform(report: dict, source_path: Path) -> tuple[str, list[str]]:
    lines = source_path.read_text(encoding="utf-8").splitlines()
    declarations = parse_declarations(lines)
    notes: list[str] = []
    required_indices: set[str] = set()
    functions_to_convert: set[str] = set()
    rewritten_lines: set[int] = set()
    for entry in report.get("entries", []):
        reported = Path(entry.get("file", ""))
        if reported.name != source_path.name:
            continue
        line_no = int(entry.get("line", 0))
        if line_no < 1 or line_no > len(lines):
            notes.append(f"skip line {line_no}: source location is outside the file")
            continue

        if line_no in rewritten_lines:
            continue

        replacement = None
        if should_rewrite(entry, source_path):
            replacement = rewrite_array_expression(lines[line_no - 1], declarations)
            lhs_match = ASSIGNMENT.match(lines[line_no - 1])
            if replacement is not None and lhs_match is not None:
                rank = int(declarations.get(lhs_match.group("lhs").lower(), {}).get("rank", 0) or 0)
                required_indices.update(INDEX_NAMES[:rank])
        elif (
            entry.get("classification") in {"possibly-unnecessary", "provably-eliminable"}
            and entry.get("transform") == "add-shape-guard"
        ):
            replacement = rewrite_allocatable_guard(lines[line_no - 1], declarations)
        elif (
            entry.get("classification") in {"possibly-unnecessary", "provably-eliminable"}
            and entry.get("transform") == "preallocate-lhs"
        ):
            function_rewrite = rewrite_function_result_call(lines[line_no - 1])
            if function_rewrite is not None:
                replacement, function_name = function_rewrite
                functions_to_convert.add(function_name)

        if replacement is None:
            continue
        original = lines[line_no - 1].strip()
        lines[line_no - 1] = replacement
        notes.append(f"rewrote line {line_no}: {original}")
        rewritten_lines.add(line_no)
    if required_indices:
        ensure_indices_declared(lines, required_indices)
    if functions_to_convert:
        lines = convert_function_results_to_subroutines(lines, functions_to_convert)
        notes.append(
            "converted function result to explicit result-buffer subroutine: "
            + ", ".join(sorted(functions_to_convert))
        )
    return "\n".join(lines) + "\n", notes


def main() -> int:
    parser = argparse.ArgumentParser(
        description="Apply simple source-level FIAP rewrites from a JSON report."
    )
    parser.add_argument("--report", required=True, type=Path)
    parser.add_argument("--source", required=True, type=Path)
    parser.add_argument("--output", required=True, type=Path)
    args = parser.parse_args()

    report = read_json_file(args.report)
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
