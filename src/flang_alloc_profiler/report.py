from __future__ import annotations

from collections import defaultdict

from .analyzer import AnalysisSummary


def render_report(summary: AnalysisSummary) -> str:
    lines: list[str] = []
    lines.append("Implicit allocation report")
    lines.append(f"Sites: {len(summary.sites)}")
    lines.append(f"Total bytes: {summary.total_bytes}")
    lines.append("")

    grouped = defaultdict(list)
    for site in summary.sites:
        grouped[site.classification.value].append(site)

    for category in ("provably_unnecessary", "possibly_unnecessary", "necessary"):
        sites = grouped.get(category, [])
        lines.append(f"{category.replace('_', ' ').title()} ({len(sites)})")
        for site in sites:
            location = site.record.location
            size_text = f"{site.record.bytes_allocated} bytes" if site.record.bytes_allocated is not None else "unknown size"
            lines.append(
                f"  {location.file}:{location.line}: {site.record.source_construct} -> {site.record.expression} [{size_text}]"
            )
            lines.append(f"    {site.reason}")
        lines.append("")

    return "\n".join(lines).rstrip() + "\n"
