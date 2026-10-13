from __future__ import annotations

import random
from pathlib import Path
from typing import Any, Callable

ProgressCallback = Callable[[int, int | None, int, int], None]


def _is_alternative(value: Any, name: str, api: Any) -> bool:
    candidate = getattr(api, name, None)
    if isinstance(candidate, type):
        try:
            if isinstance(value, candidate):
                return True
        except TypeError:
            pass
    return type(value).__name__ == name


def _read_member(obj: Any, name: str) -> Any:
    value = getattr(obj, name, None)
    return value() if callable(value) else value


def _level_rows(
    book: Any,
    method_name: str,
    descending: bool,
    keep: int,
) -> tuple[list[dict[str, int]], int]:
    raw_levels = _read_member(book, method_name)
    rows: list[dict[str, int]] = []
    total_quantity = 0

    for level in raw_levels or []:
        price = _read_member(level, "price")
        quantity = _read_member(level, "total_quantity")

        if price is None or quantity is None:
            continue

        price = int(price)
        quantity = int(quantity)

        if quantity < 0:
            continue

        rows.append({"price": price, "quantity": quantity})
        total_quantity += quantity

    rows.sort(key=lambda item: item["price"], reverse=descending)

    # Keep one extra level so the replay serializer can truthfully flag truncation.
    return rows[: keep + 1], total_quantity


def _capture_snapshot(
    book: Any,
    timestamp: Any,
    message_index: int,
    levels_per_side: int,
) -> dict[str, Any]:
    bids, bid_depth = _level_rows(
        book, "bid_levels", True, levels_per_side
    )
    asks, ask_depth = _level_rows(
        book, "ask_levels", False, levels_per_side
    )

    best_bid = _read_member(book, "best_bid")
    best_ask = _read_member(book, "best_ask")
    spread = _read_member(book, "spread")

    return {
        "timestamp": timestamp,
        "message_index": message_index,
        "best_bid": int(best_bid or 0),
        "best_ask": int(best_ask or 0),
        "spread": int(spread or 0),
        "bid_depth": bid_depth,
        "ask_depth": ask_depth,
        "bid_levels": bids,
        "ask_levels": asks,
    }


class _SnapshotReservoir:
    """Keep bounded, deterministic coverage across a replay of unknown length."""

    def __init__(self, capacity: int) -> None:
        if capacity < 1:
            raise ValueError("max_snapshots must be at least 1")

        self.capacity = capacity
        self.first: dict[str, Any] | None = None
        self.reservoir: list[dict[str, Any]] = []
        self.seen = 0
        self.rng = random.Random(20261013)

    def add(self, snapshot: dict[str, Any]) -> None:
        self.seen += 1

        if self.first is None:
            self.first = snapshot
            return

        if self.capacity == 1:
            return

        interior_capacity = self.capacity - 1

        if len(self.reservoir) < interior_capacity:
            self.reservoir.append(snapshot)
            return

        slot = self.rng.randrange(self.seen - 1)
        if slot < interior_capacity:
            self.reservoir[slot] = snapshot

    def finish(self, last: dict[str, Any] | None) -> list[dict[str, Any]]:
        if self.first is None:
            return []

        if (
            self.capacity == 1
            or last is None
            or last["message_index"] == self.first["message_index"]
        ):
            return [self.first]

        target_interior = self.capacity - 2

        interior = [
            item
            for item in self.reservoir
            if item["message_index"] != last["message_index"]
        ]

        if target_interior <= 0:
            interior = []
        elif len(interior) > target_interior:
            # Evenly thin the bounded reservoir while retaining temporal coverage.
            n = len(interior)

            indices = (
                sorted(
                    {
                        round(i * (n - 1) / (target_interior - 1))
                        for i in range(target_interior)
                    }
                )
                if target_interior > 1
                else [n - 1]
            )

            interior = [interior[i] for i in indices]

        combined = [self.first, *interior, last]
        combined.sort(key=lambda item: item["message_index"])

        return combined[: self.capacity]


def run_bounded_itch_replay(
    file_path: str | Path,
    *,
    stock_locate: int,
    max_messages: int | None,
    snapshot_interval: int,
    levels_per_side: int,
    max_snapshots: int,
    progress_callback: ProgressCallback | None = None,
) -> tuple[list[dict[str, Any]], int, int]:
    """Replay raw ITCH messages, sampling after every N successful book operations.

    max_messages counts raw feed messages, including messages for other securities.
    snapshot_interval counts successful ItchReplay.apply() calls for the selected security.
    Snapshot message_index remains the position in the raw feed.
    Snapshot retention is bounded by max_snapshots.
    """
    if max_messages is not None and max_messages < 1:
        raise ValueError("max_messages must be at least 1 or None for the full file")

    if snapshot_interval < 1:
        raise ValueError("snapshot_interval must be at least 1")

    if levels_per_side < 1:
        raise ValueError("levels_per_side must be at least 1")

    if max_snapshots < 1:
        raise ValueError("max_snapshots must be at least 1")

    if Path(file_path).suffix.lower() == ".gz":
        raise ValueError(
            "The current C++ ItchFileReader reads binary ITCH .txt files, "
            "not gzip streams. Use the decompressed .txt file."
        )

    try:
        import lob_cpp as api
    except ImportError as exc:
        raise RuntimeError(
            "Could not import lob_cpp. Run the dashboard with the MSYS2 "
            "UCRT64 Python used to build the extension."
        ) from exc

    reader = api.ItchFileReader(str(Path(file_path)))
    order_book = api.OrderBook()
    mapper = api.ItchMapper(int(stock_locate))
    replay = api.ItchReplay(order_book)
    retained = _SnapshotReservoir(max_snapshots)

    latest_snapshot: dict[str, Any] | None = None
    messages_read = 0
    applied_operations = 0
    last_snapshot_operation = 0
    last_applied_timestamp: Any = None
    last_applied_message = 0

    progress_every = max(
        1_000,
        min(100_000, (max_messages or 10_000_000) // 100),
    )

    while max_messages is None or messages_read < max_messages:
        read_result = reader.next_message()

        if _is_alternative(read_result, "EndOfFile", api):
            break

        if _is_alternative(read_result, "FileReaderError", api):
            raise RuntimeError(
                f"ITCH file-reader error after {messages_read:,} messages: "
                f"{read_result!r}"
            )

        messages_read += 1

        parse_result = api.get_type_parser(read_result)

        if _is_alternative(parse_result, "ParseError", api):
            raise RuntimeError(
                f"ITCH parse error at feed message {messages_read:,}: "
                f"{parse_result!r}"
            )

        message = parse_result
        timestamp = getattr(message, "timestamp", None)
        mapped = mapper.map(message)

        if _is_alternative(mapped, "ReplayError", api):
            ignored = getattr(api.ReplayError, "IgnoredMessage", object())

            if mapped != ignored:
                raise RuntimeError(
                    f"ITCH mapping error at feed message {messages_read:,}: "
                    f"{mapped!r}"
                )
        else:
            apply_result = replay.apply(mapped)

            if apply_result is not None:
                raise RuntimeError(
                    f"ITCH replay error at feed message {messages_read:,}: "
                    f"{apply_result!r}"
                )

            # Count only successfully applied operations for the selected stock.
            applied_operations += 1
            last_applied_timestamp = timestamp
            last_applied_message = messages_read

            if applied_operations % snapshot_interval == 0:
                latest_snapshot = _capture_snapshot(
                    order_book,
                    timestamp,
                    messages_read,
                    levels_per_side,
                )
                retained.add(latest_snapshot)
                last_snapshot_operation = applied_operations

        if (
            progress_callback is not None
            and messages_read % progress_every == 0
        ):
            percent = (
                68
                if max_messages is None
                else min(
                    87,
                    68 + int(19 * messages_read / max_messages),
                )
            )

            progress_callback(
                messages_read,
                max_messages,
                retained.seen,
                percent,
            )

    if messages_read == 0:
        raise RuntimeError(
            "The ITCH reader returned no messages; verify the source file and format."
        )

    # Preserve the final book state if operations remain after the last interval.
    if applied_operations > last_snapshot_operation:
        latest_snapshot = _capture_snapshot(
            order_book,
            last_applied_timestamp,
            last_applied_message,
            levels_per_side,
        )
        retained.add(latest_snapshot)

    result = retained.finish(latest_snapshot)

    if progress_callback is not None:
        progress_callback(
            messages_read,
            max_messages,
            retained.seen,
            88,
        )

    return result, retained.seen, messages_read