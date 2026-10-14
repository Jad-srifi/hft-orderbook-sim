
from __future__ import annotations

import random
from pathlib import Path
from typing import Any, Callable

ProgressCallback = Callable[
    [int, int | None, int, int, dict[str, int]],
    None,
]

_COUNTER_NAMES = (
    "applied_messages",
    "ignored_different_stock_locate",
    "non_applied_messages",
    "failed_applications",
    "parse_errors",
    "mapping_errors",
)


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

    # Keep one extra level so the serializer can detect truncation.
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
            count = len(interior)
            indices = (
                sorted(
                    {
                        round(i * (count - 1) / (target_interior - 1))
                        for i in range(target_interior)
                    }
                )
                if target_interior > 1
                else [count - 1]
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
    """Replay ITCH, sampling after N successful selected-stock book operations.

    max_messages counts raw messages consumed, including messages for other stocks.
    snapshot_interval counts successful ItchReplay.apply() calls.
    message_index is the raw-feed position.
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

    path = Path(file_path)

    if path.suffix.lower() == ".gz":
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

    reader = api.ItchFileReader(str(path))
    order_book = api.OrderBook()
    mapper = api.ItchMapper(int(stock_locate))
    replay = api.ItchReplay(order_book)
    retained = _SnapshotReservoir(max_snapshots)

    counters = {name: 0 for name in _COUNTER_NAMES}
    latest_snapshot: dict[str, Any] | None = None

    messages_read = 0
    applied_operations = 0
    last_snapshot_operation = 0
    last_applied_timestamp: Any = None
    last_applied_message = 0

    # Frequent enough to keep the UI alive, without writing metadata per message.
    progress_every = 1_000

    def report_progress(percent: int) -> None:
        if progress_callback is None:
            return

        progress_callback(
            messages_read,
            max_messages,
            retained.seen,
            percent,
            {
                **counters,
                "messages_accounted": sum(counters.values()),
            },
        )

    report_progress(8)

    while max_messages is None or messages_read < max_messages:
        read_result = reader.next_message()

        if _is_alternative(read_result, "EndOfFile", api):
            break

        # Reader errors are not counted as consumed messages.
        if _is_alternative(read_result, "FileReaderError", api):
            report_progress(68)
            raise RuntimeError(
                f"ITCH file-reader error after {messages_read:,} messages: "
                f"{read_result!r}"
            )

        messages_read += 1

        # Each consumed raw message receives exactly one outcome.
        try:
            message = api.get_type_parser(read_result)
        except Exception:
            counters["parse_errors"] += 1
            message = None

        if message is None:
            pass
        elif _is_alternative(message, "ParseError", api):
            counters["parse_errors"] += 1
        else:
            timestamp = getattr(message, "timestamp", None)
            message_locate = getattr(message, "stock_locate", None)

            try:
                mapped = mapper.map(message)
            except Exception:
                counters["mapping_errors"] += 1
                mapped = None

            if mapped is None:
                # Mapping exceptions were already counted above.
                pass
            elif _is_alternative(mapped, "ReplayError", api):
                ignored = getattr(api.ReplayError, "IgnoredMessage", None)

                if ignored is not None and mapped == ignored:
                    if (
                        message_locate is not None
                        and int(message_locate) != int(stock_locate)
                    ):
                        counters["ignored_different_stock_locate"] += 1
                    else:
                        # Selected-stock metadata, unsupported messages, and
                        # messages without a stock locate do not change the book.
                        counters["non_applied_messages"] += 1
                else:
                    counters["mapping_errors"] += 1
            else:
                try:
                    apply_result = replay.apply(mapped)
                except Exception:
                    counters["failed_applications"] += 1
                else:
                    # In this binding, successful std::monostate becomes None.
                    if apply_result is not None:
                        counters["failed_applications"] += 1
                    else:
                        counters["applied_messages"] += 1
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

        if messages_read % progress_every == 0:
            if max_messages is None:
                percent = 68
            else:
                percent = min(
                    87,
                    8 + int(79 * messages_read / max_messages),
                )
            report_progress(percent)

    if messages_read == 0:
        raise RuntimeError(
            "The ITCH reader returned no messages; verify the source file and format."
        )

    # Preserve the final book state if successful operations remain after
    # the last scheduled interval.
    if applied_operations > last_snapshot_operation:
        latest_snapshot = _capture_snapshot(
            order_book,
            last_applied_timestamp,
            last_applied_message,
            levels_per_side,
        )
        retained.add(latest_snapshot)

    result = retained.finish(latest_snapshot)
    report_progress(88)

    if sum(counters.values()) != messages_read:
        raise RuntimeError(
            "Internal accounting error: consumed ITCH messages do not match "
            "the sum of their outcome counters."
        )

    return result, retained.seen, messages_read
