from __future__ import annotations

import argparse
from pathlib import Path

from _common import run_historical_replay


def build_parser() -> argparse.ArgumentParser:
    parser = argparse.ArgumentParser(description="Replay historical ITCH messages through the project replay API.")
    parser.add_argument("--file", type=Path, default=None, help="ITCH file path; uses the project default if omitted.")
    parser.add_argument("--stock-locate", type=int, default=123)
    parser.add_argument("--max-messages", type=int, default=20_000_000, help="Use 0 to request the full file.")
    parser.add_argument("--snapshot-interval", type=int, default=10)
    return parser


def main() -> None:
    args = build_parser().parse_args()
    max_messages = None if args.max_messages == 0 else args.max_messages
    _, result, snapshots, file_path = run_historical_replay(
        args.file,
        stock_locate=args.stock_locate,
        max_messages=max_messages,
        snapshot_interval=args.snapshot_interval,
    )
    print(f"ITCH file: {file_path}")
    print(f"Stock Locate: {args.stock_locate}")
    print(f"Message limit: {'full file' if max_messages is None else max_messages:,}")
    print(f"Snapshots returned: {len(snapshots):,}")
    print(f"Replay result: {result!r}")


if __name__ == "__main__":
    main()
