from __future__ import annotations

import argparse
import csv
import math
from pathlib import Path

from _common import ROOT, as_float, read_csv_rows


def main() -> None:
    parser = argparse.ArgumentParser(description="Calculate descriptive order-book features from an observations dataset.")
    parser.add_argument("--input", type=Path, required=True, help="CSV created by extract_observations.py or generate_dataset.py.")
    parser.add_argument("--output", type=Path, default=None)
    args = parser.parse_args()

    fields, rows = read_csv_rows(args.input)
    required = {"midpoint", "spread", "bid_depth", "ask_depth"}
    missing = required.difference(fields)
    if missing:
        raise ValueError(f"Input CSV is missing required columns: {', '.join(sorted(missing))}")

    feature_fields = [
        "mid_return_1",
        "relative_spread",
        "total_depth",
        "depth_imbalance",
    ]
    output_fields = fields + [name for name in feature_fields if name not in fields]
    previous_midpoint: float | None = None
    for row in rows:
        midpoint = as_float(row, "midpoint")
        spread = as_float(row, "spread")
        bid_depth = as_float(row, "bid_depth")
        ask_depth = as_float(row, "ask_depth")

        mid_return = None
        if midpoint is not None and previous_midpoint not in (None, 0.0):
            mid_return = midpoint / previous_midpoint - 1.0
        if midpoint is not None and midpoint > 0:
            row["relative_spread"] = "" if spread is None else str(spread / midpoint)
        else:
            row["relative_spread"] = ""
        if bid_depth is not None and ask_depth is not None:
            row["total_depth"] = str(bid_depth + ask_depth)
            denominator = bid_depth + ask_depth
            row["depth_imbalance"] = str((bid_depth - ask_depth) / denominator) if denominator > 0 else ""
        else:
            row["total_depth"] = ""
            row["depth_imbalance"] = ""
        row["mid_return_1"] = "" if mid_return is None or not math.isfinite(mid_return) else str(mid_return)
        if midpoint is not None:
            previous_midpoint = midpoint

    output = args.output or (ROOT / "results" / f"{args.input.stem}_features.csv")
    output.parent.mkdir(parents=True, exist_ok=True)
    with output.open("w", newline="", encoding="utf-8") as file:
        writer = csv.DictWriter(file, fieldnames=output_fields, extrasaction="ignore")
        writer.writeheader()
        writer.writerows(rows)
    print(f"Rows processed: {len(rows):,}")
    print(f"Features: {', '.join(feature_fields)}")
    print(f"Output: {output}")


if __name__ == "__main__":
    main()
