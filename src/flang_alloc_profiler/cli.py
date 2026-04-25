from __future__ import annotations

import argparse
import sys
from pathlib import Path

from .analyzer import AllocationAnalyzer
from .report import render_report
from .transform import load_source, rewrite_simple_temporary


def build_parser() -> argparse.ArgumentParser:
    parser = argparse.ArgumentParser(prog="flang-alloc-profiler")
    subparsers = parser.add_subparsers(dest="command", required=True)

    analyze_parser = subparsers.add_parser("analyze", help="analyze allocation records")
    analyze_parser.add_argument("--input", required=True, help="JSON file containing allocation records")

    transform_parser = subparsers.add_parser("transform", help="rewrite a simple temporary-producing assignment")
    transform_parser.add_argument("--input", required=True, help="JSON file containing allocation records")
    transform_parser.add_argument("--source", required=True, help="Fortran source file to rewrite")
    transform_parser.add_argument("--output", help="Optional path for the rewritten source")

    return parser


def main(argv: list[str] | None = None) -> int:
    parser = build_parser()
    args = parser.parse_args(argv)
    analyzer = AllocationAnalyzer()

    if args.command == "analyze":
        records = analyzer.load_records(args.input)
        summary = analyzer.analyze(records)
        sys.stdout.write(render_report(summary))
        return 0

    if args.command == "transform":
        records = analyzer.load_records(args.input)
        summary = analyzer.analyze(records)
        source_text = load_source(args.source)

        transformed = source_text
        suggestions: list[str] = []
        for site in summary.sites:
            result = rewrite_simple_temporary(transformed, site)
            if result.suggestions:
                transformed = result.source
                suggestions.extend(
                    f"{suggestion.location.file}:{suggestion.location.line}: {suggestion.rationale}"
                    for suggestion in result.suggestions
                )

        if args.output:
            Path(args.output).write_text(transformed, encoding="utf-8")
        else:
            sys.stdout.write(transformed)

        if suggestions:
            sys.stderr.write("\n".join(suggestions) + "\n")
        return 0

    return 1


if __name__ == "__main__":
    raise SystemExit(main())
