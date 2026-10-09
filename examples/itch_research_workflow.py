
from __future__ import annotations

import argparse
import json
import math
import os
import sys
from pathlib import Path

import matplotlib.pyplot as plt
import numpy as np
import pytest


ROOT = Path(__file__).resolve().parents[1]
PYTHON_DIR = ROOT / "python"

if str(PYTHON_DIR) not in sys.path:
    sys.path.insert(0, str(PYTHON_DIR))

from backtests.historical_replay import HistoricalReplay
from itch_directory import print_stock_directory


DEFAULT_ITCH_CANDIDATES = (
    ROOT / "data" / "itch" / "2019-10-18" / "raw" / "decompressed" / "S101819-v50.txt",
    ROOT / "data" / "itch" / "2019-10-18" / "decompressed" / "S101819-v50.txt",
    ROOT / "data" / "itch" / "2019-10-18" / "raw" / "S101819-v50.txt.gz",
)

DEFAULT_OUTPUT_DIR = ROOT / "results" / "itch_research_demo"


def resolve_itch_file(value: str | Path | None) -> Path:
    if value is not None:
        supplied = Path(value).expanduser()

        candidates = (
            supplied,
            ROOT / supplied,
        )

        for candidate in candidates:
            if candidate.is_file():
                return candidate.resolve()

        raise FileNotFoundError(
            f"ITCH file not found: {value}\n"
            f"Checked: {supplied}\n"
            f"Checked: {ROOT / supplied}"
        )

    for candidate in DEFAULT_ITCH_CANDIDATES:
        if candidate.is_file():
            return candidate.resolve()

    expected = "\n".join(str(path) for path in DEFAULT_ITCH_CANDIDATES)

    raise FileNotFoundError(
        "Could not locate the default ITCH dataset. "
        "Pass --file PATH to specify it.\n"
        f"Default locations checked:\n{expected}"
    )


def get_snapshots(dataset) -> tuple:
    for name in ("snapshots", "observations", "records"):
        value = getattr(dataset, name, None)

        if value is not None:
            if callable(value):
                value = value()

            return tuple(value)

    if isinstance(dataset, (tuple, list)):
        return tuple(dataset)

    raise TypeError(
        "Could not extract snapshots from the replay result. "
        "Expected a dataset exposing snapshots, observations, or records."
    )


def format_timestamp(timestamp_ms: int) -> str:
    timestamp_ms = int(timestamp_ms) % 86_400_000

    hours, remainder = divmod(timestamp_ms, 3_600_000)
    minutes, remainder = divmod(remainder, 60_000)
    seconds, milliseconds = divmod(remainder, 1_000)

    return (
        f"{hours:02d}:{minutes:02d}:"
        f"{seconds:02d}.{milliseconds:03d}"
    )


def snapshot_bid_depth(snapshot) -> int:
    return sum(int(quantity) for _, quantity in snapshot.bid_levels)


def snapshot_ask_depth(snapshot) -> int:
    return sum(int(quantity) for _, quantity in snapshot.ask_levels)


def snapshot_imbalance(snapshot) -> float:
    bid_depth = snapshot_bid_depth(snapshot)
    ask_depth = snapshot_ask_depth(snapshot)
    total_depth = bid_depth + ask_depth

    if total_depth == 0:
        return math.nan

    return (bid_depth - ask_depth) / total_depth


def valid_quote(snapshot) -> bool:
    return (
        snapshot.best_bid > 0
        and snapshot.best_ask > snapshot.best_bid
    )


def quote_derived_spread(snapshot) -> float:
    if not valid_quote(snapshot):
        return math.nan

    return float(snapshot.best_ask - snapshot.best_bid)


def numeric_stats(values) -> dict:
    array = np.asarray(values, dtype=float)
    array = array[np.isfinite(array)]

    if array.size == 0:
        return {
            "count": 0,
            "mean": None,
            "median": None,
            "variance": None,
            "std": None,
            "quantile_50": None,
            "autocorrelation_lag_1": None,
        }

    autocorrelation = None

    if array.size >= 2:
        left = array[:-1]
        right = array[1:]

        if np.std(left) > 0 and np.std(right) > 0:
            autocorrelation = float(np.corrcoef(left, right)[0, 1])

            if not math.isfinite(autocorrelation):
                autocorrelation = None

    return {
        "count": int(array.size),
        "mean": float(np.mean(array)),
        "median": float(np.median(array)),
        "variance": float(np.var(array)),
        "std": float(np.std(array)),
        "quantile_50": float(np.quantile(array, 0.5)),
        "autocorrelation_lag_1": autocorrelation,
    }


def diagnose_snapshots(snapshots) -> dict:
    print("\n" + "=" * 100)
    print("SNAPSHOT DATA-INTEGRITY DIAGNOSTICS")
    print("=" * 100)

    zero_bid = sum(snapshot.best_bid == 0 for snapshot in snapshots)
    zero_ask = sum(snapshot.best_ask == 0 for snapshot in snapshots)
    zero_midpoint = sum(snapshot.midpoint == 0 for snapshot in snapshots)
    zero_spread = sum(snapshot.spread == 0 for snapshot in snapshots)
    empty_bids = sum(not snapshot.bid_levels for snapshot in snapshots)
    empty_asks = sum(not snapshot.ask_levels for snapshot in snapshots)

    print(f"Total snapshots:       {len(snapshots)}")
    print(f"Empty bid sides:       {empty_bids}")
    print(f"Empty ask sides:       {empty_asks}")
    print(f"Zero best bids:        {zero_bid}")
    print(f"Zero best asks:        {zero_ask}")
    print(f"Zero midpoints:        {zero_midpoint}")
    print(f"Zero reported spreads: {zero_spread}")

    largest = sorted(
        enumerate(snapshots),
        key=lambda item: int(item[1].spread),
        reverse=True,
    )[:10]

    print("\nSnapshots with the largest reported spreads:")

    for index, snapshot in largest:
        print(
            f"index={index:4d} "
            f"message={snapshot.message_index:>10} "
            f"time={format_timestamp(snapshot.timestamp_ms)} "
            f"bid={snapshot.best_bid:>14,} "
            f"ask={snapshot.best_ask:>14,} "
            f"reported_spread={snapshot.spread:>14,} "
            f"midpoint={snapshot.midpoint:>16,.2f} "
            f"levels={len(snapshot.bid_levels)}/{len(snapshot.ask_levels)}"
        )

    issues = []

    for index, snapshot in enumerate(snapshots):
        snapshot_issues = []

        bid = int(snapshot.best_bid)
        ask = int(snapshot.best_ask)
        spread = int(snapshot.spread)
        midpoint = float(snapshot.midpoint)

        if bid > 0 and ask > 0:
            if spread != ask - bid:
                snapshot_issues.append(
                    f"reported spread {spread} != ask - bid ({ask - bid})"
                )

            expected_midpoint = (bid + ask) / 2.0

            if (
                not math.isfinite(midpoint)
                or abs(midpoint - expected_midpoint) > 1e-9
            ):
                snapshot_issues.append(
                    f"reported midpoint {midpoint} != expected "
                    f"midpoint {expected_midpoint}"
                )

            if bid >= ask:
                snapshot_issues.append("locked or crossed best quotes")

        elif spread != 0 or midpoint != 0:
            snapshot_issues.append(
                "missing best quote but reported spread or midpoint is nonzero"
            )

        if snapshot.bid_levels:
            if int(snapshot.bid_levels[0][0]) != bid:
                snapshot_issues.append(
                    "best bid disagrees with first bid depth level"
                )
        elif bid != 0:
            snapshot_issues.append(
                "best bid is nonzero but bid depth levels are empty"
            )

        if snapshot.ask_levels:
            if int(snapshot.ask_levels[0][0]) != ask:
                snapshot_issues.append(
                    "best ask disagrees with first ask depth level"
                )
        elif ask != 0:
            snapshot_issues.append(
                "best ask is nonzero but ask depth levels are empty"
            )

        if snapshot_issues:
            issues.append(
                {
                    "snapshot_index": index,
                    "message_index": int(snapshot.message_index),
                    "timestamp_ms": int(snapshot.timestamp_ms),
                    "best_bid": bid,
                    "best_ask": ask,
                    "reported_spread": spread,
                    "reported_midpoint": midpoint,
                    "issues": snapshot_issues,
                }
            )

    print(f"\nSnapshots with invariant violations: {len(issues)}")

    for issue in issues[:30]:
        print(
            f"index={issue['snapshot_index']} "
            f"message={issue['message_index']}: "
            f"{'; '.join(issue['issues'])}"
        )

    if len(issues) > 30:
        print(f"... and {len(issues) - 30} additional violations")

    if not issues:
        print("No quote/level consistency violations detected.")

    print("=" * 100)

    return {
        "total_snapshots": len(snapshots),
        "empty_bid_sides": empty_bids,
        "empty_ask_sides": empty_asks,
        "zero_best_bids": zero_bid,
        "zero_best_asks": zero_ask,
        "zero_midpoints": zero_midpoint,
        "zero_reported_spreads": zero_spread,
        "invariant_violation_count": len(issues),
        "invariant_violations": issues,
        "largest_reported_spreads": [
            {
                "snapshot_index": index,
                "message_index": int(snapshot.message_index),
                "timestamp_ms": int(snapshot.timestamp_ms),
                "best_bid": int(snapshot.best_bid),
                "best_ask": int(snapshot.best_ask),
                "reported_spread": int(snapshot.spread),
                "midpoint": float(snapshot.midpoint),
                "bid_levels": len(snapshot.bid_levels),
                "ask_levels": len(snapshot.ask_levels),
            }
            for index, snapshot in largest
        ],
    }


def compute_returns(snapshots) -> np.ndarray:
    if len(snapshots) < 2:
        return np.asarray([], dtype=float)

    returns = np.full(len(snapshots) - 1, np.nan, dtype=float)

    for index in range(len(snapshots) - 1):
        previous = float(snapshots[index].midpoint)
        current = float(snapshots[index + 1].midpoint)

        if (
            previous > 0
            and current > 0
            and math.isfinite(previous)
            and math.isfinite(current)
        ):
            returns[index] = (current - previous) / previous

    return returns


def save_plot(output_dir, filename, title, xlabel, ylabel, draw):
    fig, ax = plt.subplots(figsize=(11, 6))
    draw(ax)
    ax.set_title(title)
    ax.set_xlabel(xlabel)
    ax.set_ylabel(ylabel)
    ax.grid(True, alpha=0.3)
    fig.tight_layout()

    path = output_dir / filename
    fig.savefig(path, dpi=150, bbox_inches="tight")

    return fig


def plot_research_graphs(snapshots, output_dir) -> list:
    indices = np.arange(len(snapshots))

    bids = np.asarray(
        [float(snapshot.best_bid) if snapshot.best_bid > 0 else np.nan for snapshot in snapshots]
    )
    asks = np.asarray(
        [float(snapshot.best_ask) if snapshot.best_ask > 0 else np.nan for snapshot in snapshots]
    )
    midpoints = np.asarray(
        [float(snapshot.midpoint) if snapshot.midpoint > 0 else np.nan for snapshot in snapshots]
    )
    reported_spreads = np.asarray(
        [float(snapshot.spread) for snapshot in snapshots],
        dtype=float,
    )
    derived_spreads = np.asarray(
        [quote_derived_spread(snapshot) for snapshot in snapshots],
        dtype=float,
    )
    bid_depths = np.asarray(
        [snapshot_bid_depth(snapshot) for snapshot in snapshots],
        dtype=float,
    )
    ask_depths = np.asarray(
        [snapshot_ask_depth(snapshot) for snapshot in snapshots],
        dtype=float,
    )
    total_depths = bid_depths + ask_depths
    imbalances = np.asarray(
        [snapshot_imbalance(snapshot) for snapshot in snapshots],
        dtype=float,
    )

    relative_spreads = np.divide(
        derived_spreads,
        midpoints,
        out=np.full(len(snapshots), np.nan),
        where=np.isfinite(derived_spreads)
        & np.isfinite(midpoints)
        & (midpoints > 0),
    )

    returns = compute_returns(snapshots)
    figures = []

    def draw_book_depth(ax):
        chosen_index, chosen = max(
            enumerate(snapshots),
            key=lambda pair: (
                len(pair[1].bid_levels) + len(pair[1].ask_levels)
            ),
        )

        bid_levels = sorted(chosen.bid_levels, key=lambda level: level[0])
        ask_levels = sorted(chosen.ask_levels, key=lambda level: level[0])

        if bid_levels:
            bid_prices = [int(price) for price, _ in bid_levels]
            bid_quantities = [int(quantity) for _, quantity in bid_levels]
            bid_cumulative = np.cumsum(bid_quantities)
            ax.plot(
                bid_prices,
                bid_cumulative,
                marker="o",
                label="Bid cumulative depth",
            )

        if ask_levels:
            ask_prices = [int(price) for price, _ in ask_levels]
            ask_quantities = [int(quantity) for _, quantity in ask_levels]
            ask_cumulative = np.cumsum(ask_quantities)
            ax.plot(
                ask_prices,
                ask_cumulative,
                marker="o",
                label="Ask cumulative depth",
            )

        if not bid_levels and not ask_levels:
            ax.text(
                0.5,
                0.5,
                "No depth levels exist in any selected snapshot",
                ha="center",
                va="center",
                transform=ax.transAxes,
            )
        else:
            ax.legend()

        ax.set_title(
            "Order-book depth "
            f"(snapshot {chosen_index}, message {chosen.message_index})"
        )
        ax.set_xlabel("Price (internal units)")
        ax.set_ylabel("Cumulative quantity")

    fig, ax = plt.subplots(figsize=(11, 6))
    draw_book_depth(ax)
    ax.grid(True, alpha=0.3)
    fig.tight_layout()
    fig.savefig(output_dir / "book_depth.png", dpi=150, bbox_inches="tight")
    figures.append(fig)

    figures.append(
        save_plot(
            output_dir,
            "best_quotes.png",
            "Best bid and ask over snapshots",
            "Snapshot index",
            "Price (internal units)",
            lambda ax: (
                ax.plot(indices, bids, label="Best bid"),
                ax.plot(indices, asks, label="Best ask"),
                ax.legend(),
            ),
        )
    )

    figures.append(
        save_plot(
            output_dir,
            "midpoint.png",
            "Midpoint over snapshots",
            "Snapshot index",
            "Midpoint (internal units)",
            lambda ax: ax.plot(indices, midpoints),
        )
    )

    def draw_reported_spread(ax):
        ax.plot(indices, reported_spreads, label="Reported spread")
        ax.set_yscale("symlog", linthresh=1)
        ax.legend()

    figures.append(
        save_plot(
            output_dir,
            "reported_spread.png",
            "Spread reported by the OrderBook",
            "Snapshot index",
            "Reported spread (symlog scale)",
            draw_reported_spread,
        )
    )

    figures.append(
        save_plot(
            output_dir,
            "quote_derived_spread.png",
            "Spread calculated from best ask minus best bid",
            "Snapshot index",
            "Ask - bid (internal units)",
            lambda ax: ax.plot(indices, derived_spreads),
        )
    )

    figures.append(
        save_plot(
            output_dir,
            "relative_spread.png",
            "Relative spread from valid quotes",
            "Snapshot index",
            "Spread / midpoint",
            lambda ax: ax.plot(indices, relative_spreads),
        )
    )

    figures.append(
        save_plot(
            output_dir,
            "bid_depth.png",
            "Total bid-side depth over snapshots",
            "Snapshot index",
            "Bid quantity",
            lambda ax: ax.plot(indices, bid_depths),
        )
    )

    figures.append(
        save_plot(
            output_dir,
            "ask_depth.png",
            "Total ask-side depth over snapshots",
            "Snapshot index",
            "Ask quantity",
            lambda ax: ax.plot(indices, ask_depths),
        )
    )

    figures.append(
        save_plot(
            output_dir,
            "imbalance.png",
            "Order-book imbalance over snapshots",
            "Snapshot index",
            "Imbalance",
            lambda ax: (
                ax.plot(indices, imbalances),
                ax.axhline(0, linestyle="--", linewidth=1),
            ),
        )
    )

    figures.append(
        save_plot(
            output_dir,
            "midpoint_returns.png",
            "Midpoint returns between adjacent snapshots",
            "Return interval index",
            "Simple return",
            lambda ax: ax.plot(np.arange(len(returns)), returns),
        )
    )

    def draw_spread_distribution(ax):
        values = derived_spreads[np.isfinite(derived_spreads)]

        if values.size:
            ax.hist(values, bins=min(40, max(5, int(np.sqrt(values.size))))
                    )
        else:
            ax.text(
                0.5,
                0.5,
                "No valid two-sided quotes",
                ha="center",
                va="center",
                transform=ax.transAxes,
            )

    figures.append(
        save_plot(
            output_dir,
            "spread_distribution.png",
            "Distribution of quote-derived spreads",
            "Ask - bid (internal units)",
            "Count",
            draw_spread_distribution,
        )
    )

    def draw_imbalance_distribution(ax):
        values = imbalances[np.isfinite(imbalances)]

        if values.size:
            ax.hist(values, bins=30)
        else:
            ax.text(
                0.5,
                0.5,
                "No snapshots with available depth",
                ha="center",
                va="center",
                transform=ax.transAxes,
            )

    figures.append(
        save_plot(
            output_dir,
            "imbalance_distribution.png",
            "Distribution of order-book imbalance",
            "Imbalance",
            "Count",
            draw_imbalance_distribution,
        )
    )

    def draw_depth_distribution(ax):
        values = total_depths[np.isfinite(total_depths)]

        if values.size:
            ax.hist(values, bins=min(40, max(5, int(np.sqrt(values.size)))))
        else:
            ax.text(
                0.5,
                0.5,
                "No depth observations",
                ha="center",
                va="center",
                transform=ax.transAxes,
            )

    figures.append(
        save_plot(
            output_dir,
            "total_depth_distribution.png",
            "Distribution of total displayed depth",
            "Bid depth + ask depth",
            "Count",
            draw_depth_distribution,
        )
    )

    def draw_depth_vs_imbalance(ax):
        mask = np.isfinite(total_depths) & np.isfinite(imbalances)

        if np.any(mask):
            ax.scatter(total_depths[mask], imbalances[mask], alpha=0.7)
        else:
            ax.text(
                0.5,
                0.5,
                "No valid depth and imbalance observations",
                ha="center",
                va="center",
                transform=ax.transAxes,
            )

    figures.append(
        save_plot(
            output_dir,
            "depth_vs_imbalance.png",
            "Total depth versus order-book imbalance",
            "Total depth",
            "Imbalance",
            draw_depth_vs_imbalance,
        )
    )

    def draw_midpoint_vs_spread(ax):
        mask = (
            np.isfinite(midpoints)
            & np.isfinite(derived_spreads)
            & (midpoints > 0)
        )

        if np.any(mask):
            ax.scatter(
                midpoints[mask],
                derived_spreads[mask],
                alpha=0.7,
            )
        else:
            ax.text(
                0.5,
                0.5,
                "No valid quote observations",
                ha="center",
                va="center",
                transform=ax.transAxes,
            )

    figures.append(
        save_plot(
            output_dir,
            "midpoint_vs_spread.png",
            "Midpoint versus quote-derived spread",
            "Midpoint (internal units)",
            "Ask - bid (internal units)",
            draw_midpoint_vs_spread,
        )
    )

    return figures


def print_stats(title: str, values) -> None:
    stats = numeric_stats(values)

    print(f"\n{title}")

    if stats["count"] == 0:
        print("No valid observations.")
        return

    print(f"Count: {stats['count']}")
    print(f"Mean: {stats['mean']}")
    print(f"Median: {stats['median']}")
    print(f"Variance: {stats['variance']}")
    print(f"STD: {stats['std']}")
    print(f"Quantile(50.0%): {stats['quantile_50']}")
    print(f"Autocorrelation(Lag=1): {stats['autocorrelation_lag_1']}")


def run_workflow(
    file_path: str | Path,
    stock_locate: int = 123,
    max_messages: int | None = 20_000_000,
    snapshot_interval: int = 10,
    output_dir: str | Path | None = None,
    show: bool = True,
) -> dict:
    file_path = resolve_itch_file(file_path)
    
    if output_dir is None:
        message_limit_name = (
            "full" if max_messages is None else str(max_messages)
        )
        output_dir = (
            ROOT
            / "results"
            / f"itch_results_{stock_locate}_{message_limit_name}"
        )
    else:
        output_dir = Path(output_dir)

    output_dir.mkdir(parents=True, exist_ok=True)

    if snapshot_interval < 1:
        raise ValueError("snapshot_interval must be at least 1")

    if max_messages is not None and max_messages < 1:
        raise ValueError("max_messages must be at least 1 or None")

    print(f"ITCH file: {file_path}")
    print(f"Stock Locate: {stock_locate}")
    print(
        "Message limit: "
        f"{max_messages if max_messages is not None else 'full file'}"
    )
    print(f"Snapshot interval: every {snapshot_interval} replay operations")
    print("\nStarting historical replay...")

    replay = HistoricalReplay(file_path, stock_locate)
    dataset = replay.run_and_collect(
        snapshot_interval,
        max_messages=max_messages,
    )

    snapshots = get_snapshots(dataset)

    print("\nReplay summary")
    print(f"Messages processed: {getattr(replay, 'messages_seen', 'unknown')}")
    print(f"Replay operations applied: {getattr(replay, 'replay_operations', 'unknown')}")
    print(f"Messages ignored by mapper: {getattr(replay, 'ignored_messages', 'unknown')}")
    print(f"Snapshots captured: {len(snapshots)}")

    if snapshots:
        first = snapshots[0]
        last = snapshots[-1]

        print(
            f"First snapshot: {format_timestamp(first.timestamp_ms)} "
            f"| message {first.message_index}"
        )
        print(
            f"Last snapshot:  {format_timestamp(last.timestamp_ms)} "
            f"| message {last.message_index}"
        )

    if len(snapshots) < 3:
        raise RuntimeError(
            f"Only {len(snapshots)} snapshots were captured. "
            "Increase --max-messages or reduce --snapshot-interval."
        )

    diagnostics = diagnose_snapshots(snapshots)

    with (output_dir / "snapshot_diagnostics.json").open(
        "w",
        encoding="utf-8",
    ) as stream:
        json.dump(diagnostics, stream, indent=2, allow_nan=False)

    midpoint_values = np.asarray(
        [float(snapshot.midpoint) for snapshot in snapshots],
        dtype=float,
    )
    reported_spreads = np.asarray(
        [float(snapshot.spread) for snapshot in snapshots],
        dtype=float,
    )
    quote_spreads = np.asarray(
        [quote_derived_spread(snapshot) for snapshot in snapshots],
        dtype=float,
    )
    bid_depths = np.asarray(
        [snapshot_bid_depth(snapshot) for snapshot in snapshots],
        dtype=float,
    )
    ask_depths = np.asarray(
        [snapshot_ask_depth(snapshot) for snapshot in snapshots],
        dtype=float,
    )
    imbalances = np.asarray(
        [snapshot_imbalance(snapshot) for snapshot in snapshots],
        dtype=float,
    )
    returns = compute_returns(snapshots)

    relative_spreads = np.divide(
        quote_spreads,
        midpoint_values,
        out=np.full(len(snapshots), np.nan),
        where=np.isfinite(quote_spreads)
        & np.isfinite(midpoint_values)
        & (midpoint_values > 0),
    )

    valid_midpoints = midpoint_values[
        np.isfinite(midpoint_values) & (midpoint_values > 0)
    ]
    valid_reported_spreads = reported_spreads[np.isfinite(reported_spreads)]
    valid_quote_spreads = quote_spreads[np.isfinite(quote_spreads)]
    valid_returns = returns[np.isfinite(returns)]

    print("\nFeature example")

    example = next(
        (snapshot for snapshot in snapshots if valid_quote(snapshot)),
        snapshots[0],
    )

    print(f"midpoint: {example.midpoint}")
    print(f"reported spread: {example.spread}")
    print(f"ask minus bid: {quote_derived_spread(example)}")
    print(f"bid_depth: {snapshot_bid_depth(example)}")
    print(f"ask_depth: {snapshot_ask_depth(example)}")
    print(f"imbalance: {snapshot_imbalance(example)}")

    print_stats("Reported spread statistics", valid_reported_spreads)
    print_stats("Quote-derived spread statistics", valid_quote_spreads)
    print_stats("Valid midpoint statistics", valid_midpoints)
    print_stats("Midpoint-return statistics", valid_returns)
    print_stats("Order-book imbalance statistics", imbalances)
    print_stats("Bid depth statistics", bid_depths)
    print_stats("Ask depth statistics", ask_depths)

    correlation = None

    if len(returns) > 0 and len(imbalances) > 1:
        x = imbalances[:-1]
        y = returns
        mask = np.isfinite(x) & np.isfinite(y)

        if (
            np.count_nonzero(mask) >= 2
            and np.std(x[mask]) > 0
            and np.std(y[mask]) > 0
        ):
            correlation = float(np.corrcoef(x[mask], y[mask])[0, 1])

            if not math.isfinite(correlation):
                correlation = None

    print(
        "\nCorrelation between sampled imbalance and "
        f"next-snapshot midpoint return: {correlation}"
    )

    if len(snapshots) >= 2:
        previous = snapshots[-2]
        current = snapshots[-1]

        midpoint_change = float(current.midpoint - previous.midpoint)
        midpoint_return = (
            midpoint_change / previous.midpoint
            if previous.midpoint > 0
            else None
        )

        print("Last two snapshot comparison:")
        print(f"midpoint_change: {midpoint_change}")
        print(f"midpoint_return: {midpoint_return}")
        print(f"spread_change: {current.spread - previous.spread}")
        print(
            "bid_depth_change: "
            f"{snapshot_bid_depth(current) - snapshot_bid_depth(previous)}"
        )
        print(
            "ask_depth_change: "
            f"{snapshot_ask_depth(current) - snapshot_ask_depth(previous)}"
        )
        print(
            "imbalance_change: "
            f"{snapshot_imbalance(current) - snapshot_imbalance(previous)}"
        )

    figures = plot_research_graphs(snapshots, output_dir)

    stats = {
        "reported_spread": numeric_stats(valid_reported_spreads),
        "quote_derived_spread": numeric_stats(valid_quote_spreads),
        "valid_midpoint": numeric_stats(valid_midpoints),
        "midpoint_returns": numeric_stats(valid_returns),
        "imbalance": numeric_stats(imbalances),
        "bid_depth": numeric_stats(bid_depths),
        "ask_depth": numeric_stats(ask_depths),
        "relative_spread": numeric_stats(relative_spreads),
        "imbalance_next_midpoint_return_correlation": correlation,
    }

    experiment_record = {
        "name": "itch_research_demo",
        "input_file": str(file_path),
        "stock_locate": stock_locate,
        "parameters": {
            "max_messages": max_messages,
            "snapshot_interval": snapshot_interval,
        },
        "replay": {
            "messages_seen": getattr(replay, "messages_seen", None),
            "replay_operations": getattr(replay, "replay_operations", None),
            "ignored_messages": getattr(replay, "ignored_messages", None),
            "snapshot_count": len(snapshots),
        },
        "feature_definitions": {
            "quote_derived_spread": "best_ask - best_bid, only when best_ask > best_bid > 0",
            "relative_spread": "quote_derived_spread / midpoint",
            "bid_depth": "sum of displayed quantities across bid levels",
            "ask_depth": "sum of displayed quantities across ask levels",
            "imbalance": "(bid_depth - ask_depth) / (bid_depth + ask_depth)",
            "midpoint_return": "(next_midpoint - midpoint) / midpoint",
        },
        "results": stats,
        "snapshot_diagnostics": diagnostics,
        "plots": [
            "book_depth.png",
            "best_quotes.png",
            "midpoint.png",
            "reported_spread.png",
            "quote_derived_spread.png",
            "relative_spread.png",
            "bid_depth.png",
            "ask_depth.png",
            "imbalance.png",
            "midpoint_returns.png",
            "spread_distribution.png",
            "imbalance_distribution.png",
            "total_depth_distribution.png",
            "depth_vs_imbalance.png",
            "midpoint_vs_spread.png",
        ],
    }

    experiment_path = output_dir / "itch_research_experiment.json"

    with experiment_path.open("w", encoding="utf-8") as stream:
        json.dump(experiment_record, stream, indent=2, allow_nan=False)

    print(f"\nExperiment record saved to: {experiment_path}")
    print(f"Snapshot diagnostics saved to: {output_dir / 'snapshot_diagnostics.json'}")
    print(f"Saved {len(figures)} graphs to: {output_dir}")

    if show:
        plt.show()

    for figure in figures:
        plt.close(figure)

    return {
        "snapshots": snapshots,
        "statistics": stats,
        "diagnostics": diagnostics,
        "experiment_path": experiment_path,
        "output_dir": output_dir,
    }


def test_real_itch_research_workflow(tmp_path):
    if os.environ.get("RUN_ITCH_RESEARCH_INTEGRATION") != "1":
        pytest.skip("Set RUN_ITCH_RESEARCH_INTEGRATION=1 to run the real ITCH test")

    try:
        file_path = resolve_itch_file(None)
    except FileNotFoundError:
        pytest.skip("The real ITCH dataset is not available")

    result = run_workflow(
        file_path=file_path,
        stock_locate=123,
        max_messages=20_000_000,
        snapshot_interval=10,
        output_dir=tmp_path,
        show=False,
    )

    assert len(result["snapshots"]) >= 3
    assert result["experiment_path"].is_file()
    assert len(list(tmp_path.glob("*.png"))) >= 10


def main(argv=None) -> int:
    parser = argparse.ArgumentParser(
        description="Replay historical ITCH data and run market microstructure diagnostics."
    )
    parser.add_argument(
        "--file",
        type=Path,
        default=None,
        help="ITCH input file. Defaults to the project's October 18, 2019 dataset.",
    )
    parser.add_argument(
        "--stock-locate",
        type=int,
        default=123,
        help="ITCH Stock Locate ID to reconstruct (default: 123).",
    )
    parser.add_argument(
        "--list-stocks",
        action="store_true",
        help="List stock symbols and Stock Locate IDs, then exit.",
    )
    parser.add_argument(
        "--max-messages",
        type=int,
        default=20_000_000,
        help="Raw ITCH messages to process; use 0 for the entire file.",
    )
    parser.add_argument(
        "--snapshot-interval",
        type=int,
        default=10,
        help="Capture a snapshot every N successfully applied replay operations.",
    )
    parser.add_argument(
        "--output-dir",
        type=Path,
        default=None,
        help="Directory for plots, diagnostics, and experiment output.",
    )
    parser.add_argument(
        "--no-show",
        action="store_true",
        help="Save plots without opening the interactive plot window.",
    )

    args = parser.parse_args(argv)

    try:
        file_path = resolve_itch_file(args.file)
    except FileNotFoundError as exc:
        parser.error(str(exc))

    if args.list_stocks:
        return print_stock_directory(file_path)

    if args.max_messages < 0:
        parser.error("--max-messages must be 0 or greater")

    if args.snapshot_interval < 1:
        parser.error("--snapshot-interval must be at least 1")

    run_workflow(
        file_path=file_path,
        stock_locate=args.stock_locate,
        max_messages=None if args.max_messages == 0 else args.max_messages,
        snapshot_interval=args.snapshot_interval,
        output_dir=args.output_dir,
        show=not args.no_show,
    )

    return 0


if __name__ == "__main__":
    raise SystemExit(main())