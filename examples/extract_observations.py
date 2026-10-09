from __future__ import annotations

import argparse
from pathlib import Path

from _common import ROOT, run_historical_replay, write_observations_csv


def main() -> None:
    parser = argparse.ArgumentParser(description="Extract periodic order-book observations from historical replay.")
    parser.add_argument("--file", type=Path, default=None)
    parser.add_argument("--stock-locate", type=int, default=123)
    parser.add_argument("--max-messages", type=int, default=20_000_000, help="Use 0 to request the full file.")
    parser.add_argument("--snapshot-interval", type=int, default=10)
    parser.add_argument("--output", type=Path, default=None)
    args = parser.parse_args()

    max_messages = None if args.max_messages == 0 else args.max_messages
    _, result, snapshots, source = run_historical_replay(
        args.file,
        stock_locate=args.stock_locate,
        max_messages=max_messages,
        snapshot_interval=args.snapshot_interval,
    )
    output = args.output or (
        ROOT / "results" / f"observations_{args.stock_locate}_{'full' if max_messages is None else max_messages}.csv"
    )
    if not snapshots:
        raise RuntimeError(
            "The replay API returned no snapshots. Check the HistoricalReplay return value and its snapshot API. "
            f"Replay result: {result!r}"
        )
    written = write_observations_csv(snapshots, output)
    print(f"Source: {source}")
    print(f"Snapshots extracted: {len(snapshots):,}")
    print(f"Output: {written}")


if __name__ == "__main__":
    main()
