from __future__ import annotations

import argparse
import json
import math
import statistics
from pathlib import Path

from _common import ROOT, as_float, read_csv_rows


def summarize(values: list[float]) -> dict[str, float | int | None]:
    if not values:
        return {"count": 0, "mean": None, "stdev": None, "min": None, "median": None, "max": None}
    return {
        "count": len(values),
        "mean": statistics.fmean(values),
        "stdev": statistics.stdev(values) if len(values) > 1 else 0.0,
        "min": min(values),
        "median": statistics.median(values),
        "max": max(values),
    }


def main() -> None:
    parser = argparse.ArgumentParser(description="Summarize a dataset and generate basic market-data diagnostic plots.")
    parser.add_argument("--input", type=Path, required=True, help="CSV created by calculate_features.py.")
    parser.add_argument("--output-dir", type=Path, default=None)
    args = parser.parse_args()

    _, rows = read_csv_rows(args.input)
    columns = ("midpoint", "spread", "relative_spread", "bid_depth", "ask_depth", "depth_imbalance", "mid_return_1")
    summary = {
        column: summarize([value for row in rows if (value := as_float(row, column)) is not None])
        for column in columns
    }
    output_dir = args.output_dir or (ROOT / "results" / f"{args.input.stem}_analysis")
    output_dir.mkdir(parents=True, exist_ok=True)
    summary_path = output_dir / "summary.json"
    summary_path.write_text(json.dumps(summary, indent=2, allow_nan=False) + "\n", encoding="utf-8")

    try:
        import matplotlib.pyplot as plt
    except ImportError:
        print("matplotlib is not installed; summary JSON was created, plots were skipped.")
        print(f"Summary: {summary_path}")
        return

    plot_specs = [
        ("midpoint", "Midpoint (internal price ticks)", "midpoint.png"),
        ("spread", "Spread (internal price ticks)", "spread.png"),
        ("depth_imbalance", "Order-book depth imbalance", "depth_imbalance.png"),
    ]
    x_values = list(range(len(rows)))
    for column, ylabel, filename in plot_specs:
        selected = [(x, as_float(row, column)) for x, row in zip(x_values, rows)]
        selected = [(x, value) for x, value in selected if value is not None and math.isfinite(value)]
        if not selected:
            continue
        fig, ax = plt.subplots()
        ax.plot([item[0] for item in selected], [item[1] for item in selected])
        ax.set_xlabel("Observation index")
        ax.set_ylabel(ylabel)
        ax.set_title(ylabel)
        fig.tight_layout()
        fig.savefig(output_dir / filename, dpi=150)
        plt.close(fig)

    print(f"Rows analyzed: {len(rows):,}")
    print(f"Summary: {summary_path}")
    print(f"Plots and outputs: {output_dir}")


if __name__ == "__main__":
    main()
