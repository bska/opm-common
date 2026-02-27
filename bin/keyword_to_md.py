#!/usr/bin/env python3
"""
Generate a Markdown documentation skeleton from an OPM keyword JSON description.

Usage:
    keyword_to_md.py <keyword.json> [output.md]

If no output file is specified, the Markdown is written to stdout.
The keyword JSON files are found in opm/input/eclipse/share/keywords/.
"""

import argparse
import json
import sys
from pathlib import Path


def _fmt_default(value) -> str:
    """Return a human-readable string for a default value."""
    if value is None:
        return ""
    return str(value)


def _fmt_dimension(dimension) -> str:
    """Return a human-readable string for a dimension entry."""
    if dimension is None:
        return ""
    if isinstance(dimension, list):
        return ", ".join(str(d) for d in dimension)
    return str(dimension)


def _items_table(items: list) -> list[str]:
    """Render a list of item dicts as a Markdown table, returning lines."""
    if not items:
        return []

    lines = [
        "| # | Name | Type | Default | Dimension | Description |",
        "|---|------|------|---------|-----------|-------------|",
    ]
    for idx, item in enumerate(items, start=1):
        num = item.get("item", idx)
        name = item.get("name", "")
        vtype = item.get("value_type", "")
        default = _fmt_default(item.get("default", item.get("default_value")))
        dimension = _fmt_dimension(item.get("dimension", item.get("dimensions")))
        comment = item.get("comment", item.get("description", ""))
        lines.append(
            f"| {num} | `{name}` | {vtype} | {default} | {dimension} | {comment} |"
        )
    return lines


def _records_section(records: list, heading_prefix: str = "##") -> list[str]:
    """Render a list of records (each record is a list of items) as Markdown."""
    lines = []
    for rec_idx, record in enumerate(records, start=1):
        lines.append(f"{heading_prefix} Record {rec_idx}")
        lines.append("")
        lines.extend(_items_table(record))
        lines.append("")
    return lines


def keyword_to_markdown(data: dict) -> str:
    """Convert a parsed keyword JSON dict to a Markdown documentation skeleton."""
    lines: list[str] = []

    name = data.get("name", "UNKNOWN")

    # ------------------------------------------------------------------ Title
    lines.append(f"# {name}")
    lines.append("")

    # ------------------------------------------------------- Short description
    description = data.get("description", "")
    comment = data.get("comment", "")
    if description:
        lines.append(description)
        lines.append("")
    elif comment:
        lines.append(f"*{comment}*")
        lines.append("")
    else:
        lines.append("*TODO: Add a short description of this keyword.*")
        lines.append("")

    # ------------------------------------------------------- Alternative names
    deck_names: list[str] = []
    if "deck_name" in data:
        deck_names.append(data["deck_name"])
    if "deck_names" in data:
        deck_names.extend(data["deck_names"])
    if "deck_name_regex" in data:
        deck_names.append(f"regex: `{data['deck_name_regex']}`")
    if deck_names:
        lines.append("## Alternative Names")
        lines.append("")
        for dn in deck_names:
            lines.append(f"- `{dn}`")
        lines.append("")

    # ---------------------------------------------------------------- Sections
    sections = data.get("sections", [])
    if sections:
        lines.append("## Sections")
        lines.append("")
        for sec in sections:
            lines.append(f"- {sec}")
        lines.append("")

    # -------------------------------------------------- Dependencies / Conflicts
    requires = data.get("requires", [])
    prohibits = data.get("prohibits", [])
    if requires or prohibits:
        lines.append("## Dependencies")
        lines.append("")
        if requires:
            lines.append(
                "**Requires:** "
                + ", ".join(f"`{r}`" for r in requires)
            )
            lines.append("")
        if prohibits:
            lines.append(
                "**Prohibits:** "
                + ", ".join(f"`{p}`" for p in prohibits)
            )
            lines.append("")

    # ------------------------------------------------------ Size / multiplicity
    size = data.get("size")
    min_size = data.get("min_size")
    num_tables = data.get("num_tables")

    if size is not None or min_size is not None or num_tables is not None:
        lines.append("## Sizing")
        lines.append("")
        if size is not None:
            if isinstance(size, dict):
                kw = size.get("keyword", "")
                item = size.get("item", "")
                lines.append(
                    f"Number of records is determined by item `{item}` of keyword `{kw}`."
                )
            else:
                lines.append(f"Fixed number of records: {size}.")
            lines.append("")
        if min_size is not None:
            lines.append(f"Minimum number of records: {min_size}.")
            lines.append("")
        if num_tables is not None:
            if isinstance(num_tables, dict):
                kw = num_tables.get("keyword", "")
                item = num_tables.get("item", "")
                lines.append(
                    f"Number of tables is determined by item `{item}` of keyword `{kw}`."
                )
            else:
                lines.append(f"Number of tables: {num_tables}.")
            lines.append("")

    # -------------------------------------------------------------- Code block
    code = data.get("code")
    if code is not None:
        end_kw = code.get("end", "") if isinstance(code, dict) else ""
        lines.append("## Syntax")
        lines.append("")
        lines.append(
            "This keyword introduces a free-form code block"
            + (f" terminated by `{end_kw}`." if end_kw else ".")
        )
        lines.append("")

    # ------------------------------------------------------------------- Items
    if "items" in data:
        lines.append("## Items")
        lines.append("")
        lines.extend(_items_table(data["items"]))
        lines.append("")

    # ----------------------------------------------------------------- Data kw
    if "data" in data:
        data_spec = data["data"]
        lines.append("## Data")
        lines.append("")
        vtype = data_spec.get("value_type", "")
        dimension = _fmt_dimension(data_spec.get("dimension", data_spec.get("dimensions")))
        lines.append(f"This is a *data* keyword containing a flat array of `{vtype}` values.")
        if dimension:
            lines.append(f"Dimension: {dimension}.")
        lines.append("")

    # --------------------------------------------------------------- Records
    if "records" in data:
        lines.append("## Records")
        lines.append("")
        lines.extend(_records_section(data["records"]))

    # ---------------------------------------------------------- Records set
    if "records_set" in data:
        lines.append("## Records Set")
        lines.append("")
        lines.append(
            "This keyword uses a *records set* format with the following record types:"
        )
        lines.append("")
        lines.extend(_records_section(data["records_set"]))

    # -------------------------------------------------- Alternating records
    if "alternating_records" in data:
        lines.append("## Alternating Records")
        lines.append("")
        lines.append(
            "This keyword uses alternating records with the following layouts:"
        )
        lines.append("")
        lines.extend(_records_section(data["alternating_records"]))

    # ------------------------------------------------------------------ Notes
    lines.append("## Notes")
    lines.append("")
    lines.append("*TODO: Add any additional notes or examples here.*")
    lines.append("")

    return "\n".join(lines)


def main(argv: list[str] | None = None) -> int:
    parser = argparse.ArgumentParser(
        description="Generate a Markdown documentation skeleton from an OPM keyword JSON file."
    )
    parser.add_argument(
        "input",
        metavar="keyword.json",
        help="Path to the OPM keyword JSON description file.",
    )
    parser.add_argument(
        "output",
        metavar="output.md",
        nargs="?",
        default=None,
        help="Output Markdown file (default: stdout).",
    )
    args = parser.parse_args(argv)

    input_path = Path(args.input)
    if not input_path.is_file():
        print(f"Error: '{input_path}' is not a file.", file=sys.stderr)
        return 1

    try:
        with input_path.open(encoding="utf-8") as fh:
            data = json.load(fh)
    except json.JSONDecodeError as exc:
        print(f"Error: Failed to parse JSON: {exc}", file=sys.stderr)
        return 1

    markdown = keyword_to_markdown(data)

    if args.output is None:
        print(markdown)
    else:
        output_path = Path(args.output)
        output_path.write_text(markdown, encoding="utf-8")
        print(f"Written to '{output_path}'.", file=sys.stderr)

    return 0


if __name__ == "__main__":
    sys.exit(main())
