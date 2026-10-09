from __future__ import annotations

import argparse
import csv
import json
from pathlib import Path

from _common import ROOT, read_csv_rows


def main() -> None:
    parser = argparse.ArgumentParser(description="Validate and package order-book observations as a reproducible dataset.")
    parser.add_argument("--input", type=Path, required=True, help="CSV created by extract_observations.py.")
    parser.add_argument("--output", type=Path, default=None)
    args = parser.parse_args()

    fields, rows = read_csv_rows(args.input)
    required = {"message_index", "best_bid", "best_ask", "midpoint", "spread"}
    missing = required.difference(fields)
    if missing:
        raise ValueError(f"Input CSV is missing required columns: {', '.join(sorted(missing))}")

    valid_rows = []
    for row in rows:
        try:
            bid = float(row["best_bid"])
            ask = float(row["best_ask"])
            midpoint = float(row["midpoint"])
            spread = float(row["spread"])
        except (TypeError, ValueError, KeyError):
            continue
        if bid <= 0 or ask <= 0 or ask < bid or midpoint <= 0 or spread < 0:
            continue
        valid_rows.append(row)

    try:
        valid_rows.sort(key=lambda row: int(row["message_index"]))
    except (TypeError, ValueError):
        pass

    output = args.output or (ROOT / "results" / f"{args.input.stem}_dataset.csv")
    output.parent.mkdir(parents=True, exist_ok=True)
    with output.open("w", newline="", encoding="utf-8") as file:
        writer = csv.DictWriter(file, fieldnames=fields, extrasaction="ignore")
        writer.writeheader()
        writer.writerows(valid_rows)

    manifest = {
        "source_csv": str(args.input.resolve()),
        "dataset_csv": str(output.resolve()),
        "input_rows": len(rows),
        "output_rows": len(valid_rows),
        "dropped_rows": len(rows) - len(valid_rows),
        "columns": fields,
        "ordering": "ascending message_index where numeric",
        "validation": "positive bid/ask/midpoint; ask >= bid; nonnegative spread",
    }
    manifest_path = output.with_suffix(".metadata.json")
    manifest_path.write_text(json.dumps(manifest, indent=2, allow_nan=False) + "\n", encoding="utf-8")
    print(f"Input rows: {len(rows):,}")
    print(f"Valid dataset rows: {len(valid_rows):,}")
    print(f"Dataset: {output}")
    print(f"Metadata: {manifest_path}")


if __name__ == "__main__":
    main()
