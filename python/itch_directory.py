
from __future__ import annotations

import gzip
from dataclasses import dataclass
from pathlib import Path
from typing import BinaryIO


@dataclass(frozen=True, slots=True)
class StockDirectoryEntry:
    stock_locate: int
    symbol: str
    market_category: str


def _open_binary(path: str | Path) -> BinaryIO:
    path = Path(path)
    if path.suffix.lower() == ".gz":
        return gzip.open(path, "rb")
    return path.open("rb")


def read_stock_directory(
    path: str | Path,
    *,
    non_directory_limit: int = 256,
) -> tuple[StockDirectoryEntry, ...]:
    """Read Stock Directory ('R') messages from a length-prefixed ITCH 5.0 file."""
    if non_directory_limit < 1:
        raise ValueError("non_directory_limit must be at least 1")

    entries: dict[int, StockDirectoryEntry] = {}
    directory_started = False
    consecutive_non_directory = 0
    message_index = 0

    with _open_binary(path) as stream:
        while True:
            length_prefix = stream.read(2)

            if not length_prefix:
                break

            if len(length_prefix) != 2:
                raise ValueError(
                    f"Truncated ITCH message length prefix after message {message_index}"
                )

            message_length = int.from_bytes(
                length_prefix,
                byteorder="big",
                signed=False,
            )

            if message_length < 1:
                raise ValueError(
                    f"Invalid ITCH message length {message_length} "
                    f"after message {message_index}"
                )

            message = stream.read(message_length)

            if len(message) != message_length:
                raise ValueError(
                    f"Truncated ITCH message {message_index + 1}: expected "
                    f"{message_length} bytes, received {len(message)}"
                )

            message_index += 1

            if message[:1] == b"R":
                if len(message) < 20:
                    raise ValueError(
                        f"Malformed Stock Directory message {message_index}: "
                        f"expected at least 20 bytes, received {len(message)}"
                    )

                stock_locate = int.from_bytes(
                    message[1:3],
                    byteorder="big",
                    signed=False,
                )

                try:
                    symbol = message[11:19].decode("ascii").strip()
                    market_category = message[19:20].decode("ascii").strip()
                except UnicodeDecodeError as exc:
                    raise ValueError(
                        f"Non-ASCII Stock Directory field in message {message_index}"
                    ) from exc

                if not symbol:
                    raise ValueError(
                        f"Empty stock symbol in Stock Directory message {message_index}"
                    )

                entry = StockDirectoryEntry(
                    stock_locate=stock_locate,
                    symbol=symbol,
                    market_category=market_category,
                )

                previous = entries.get(stock_locate)

                if previous is not None and previous.symbol != symbol:
                    raise ValueError(
                        f"Conflicting symbols for Stock Locate {stock_locate}: "
                        f"{previous.symbol!r} and {symbol!r}"
                    )

                entries[stock_locate] = entry
                directory_started = True
                consecutive_non_directory = 0

            elif directory_started:
                consecutive_non_directory += 1

                if consecutive_non_directory >= non_directory_limit:
                    break

    return tuple(
        sorted(
            entries.values(),
            key=lambda entry: (entry.symbol, entry.stock_locate),
        )
    )


def print_stock_directory(path: str | Path) -> int:
    entries = read_stock_directory(path)

    if not entries:
        print(f"No Stock Directory ('R') messages found in: {path}")
        return 1

    print(f"Stock Directory: {path}")
    print(f"{'STOCK LOCATE':>12}  {'SYMBOL':<10}  {'MARKET CATEGORY':<16}")
    print(f"{'-' * 12}  {'-' * 10}  {'-' * 16}")

    for entry in entries:
        print(
            f"{entry.stock_locate:>12}  "
            f"{entry.symbol:<10}  "
            f"{entry.market_category or '-':<16}"
        )

    print(f"\nTotal symbols: {len(entries)}")
    return 0