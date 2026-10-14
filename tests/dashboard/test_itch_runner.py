from __future__ import annotations

import enum
import sys
import types
from pathlib import Path
from types import SimpleNamespace

import pytest

from webapp.services.itch_runner import run_bounded_itch_replay


class ReplayError(enum.Enum):
    IgnoredMessage = 1
    InvalidOperation = 2


class ParseError(enum.Enum):
    BadMessage = 1


class EndOfFile:
    pass


class FileReaderError:
    pass


class FakeLevel:
    def __init__(self, price: int, quantity: int) -> None:
        self.price = price
        self.total_quantity = quantity


class FakeBook:
    def __init__(self) -> None:
        self.bids = []
        self.asks = []

    def bid_levels(self):
        return self.bids

    def ask_levels(self):
        return self.asks

    def best_bid(self):
        return self.bids[0].price if self.bids else 0

    def best_ask(self):
        return self.asks[0].price if self.asks else 0

    def spread(self):
        return (
            self.best_ask() - self.best_bid()
            if self.bids and self.asks
            else 0
        )


class FakeReader:
    total = 999_999

    def __init__(self, _path: str) -> None:
        self.index = 0

    def next_message(self):
        if self.index >= self.total:
            return EndOfFile()

        self.index += 1
        return [self.index]


class FakeMapper:
    def __init__(self, stock_locate: int) -> None:
        self.stock_locate = stock_locate

    def map(self, message):
        if message.stock_locate != self.stock_locate:
            return ReplayError.IgnoredMessage

        return message


class FakeReplay:
    def __init__(self, book: FakeBook) -> None:
        self.book = book

    def apply(self, message):
        self.book.bids = [
            FakeLevel(10000 + message.index, message.index)
        ]
        self.book.asks = [
            FakeLevel(10100 + message.index, message.index + 1)
        ]

        # Successful std::monostate conversion from pybind11 is None.
        return None


def install_fake_lob_cpp(monkeypatch):
    api = types.ModuleType("lob_cpp")
    api.ReplayError = ReplayError
    api.ParseError = ParseError
    api.EndOfFile = EndOfFile
    api.FileReaderError = FileReaderError
    api.ItchFileReader = FakeReader
    api.OrderBook = FakeBook
    api.ItchMapper = FakeMapper
    api.ItchReplay = FakeReplay

    def parse(payload):
        return SimpleNamespace(
            index=payload[0],
            timestamp=payload[0] * 100,
            stock_locate=14,
        )

    api.get_type_parser = parse
    monkeypatch.setitem(sys.modules, "lob_cpp", api)


def test_honors_message_limit_and_samples_interval(
    tmp_path,
    monkeypatch,
):
    install_fake_lob_cpp(monkeypatch)

    source = tmp_path / "sample.txt"
    source.write_bytes(b"fixture")

    progress = []

    snapshots, raw_count, processed = run_bounded_itch_replay(
        source,
        stock_locate=14,
        max_messages=5,
        snapshot_interval=2,
        levels_per_side=2,
        max_snapshots=10,
        progress_callback=lambda *args: progress.append(args),
    )

    assert processed == 5
    assert raw_count == 3
    assert [item["message_index"] for item in snapshots] == [2, 4, 5]
    assert snapshots[-1]["timestamp"] == 500
    assert snapshots[-1]["best_bid"] == 10005
    assert len(progress) >= 1

    # The callback reports:
    # (messages_read, total, snapshots_captured, percent, counters).
    final_progress = progress[-1]

    assert len(final_progress) == 5
    assert final_progress[:4] == (5, 5, 3, 88)

    counters = final_progress[4]

    assert isinstance(counters, dict)

    # Raw messages read and snapshots captured are separate callback arguments.
    assert final_progress[0] == 5
    assert final_progress[2] == 3

    # All five raw messages were successfully applied.
    assert counters["applied_messages"] == 5
    assert counters["ignored_different_stock_locate"] == 0
    assert counters["non_applied_messages"] == 0
    assert counters["failed_applications"] == 0
    assert counters["parse_errors"] == 0
    assert counters["mapping_errors"] == 0

    # Every consumed raw message must have exactly one outcome.
    assert counters["messages_accounted"] == 5
    assert counters["messages_accounted"] == final_progress[0]

    outcome_total = sum(
        counters[name]
        for name in (
            "applied_messages",
            "ignored_different_stock_locate",
            "non_applied_messages",
            "failed_applications",
            "parse_errors",
            "mapping_errors",
        )
    )
    assert outcome_total == final_progress[0]


def test_bounds_snapshot_memory_and_keeps_endpoints(
    tmp_path,
    monkeypatch,
):
    install_fake_lob_cpp(monkeypatch)

    source = tmp_path / "sample.txt"
    source.write_bytes(b"fixture")

    FakeReader.total = 999_999

    try:
        snapshots, raw_count, processed = run_bounded_itch_replay(
            source,
            stock_locate=14,
            max_messages=100,
            snapshot_interval=1,
            levels_per_side=2,
            max_snapshots=5,
        )
    finally:
        FakeReader.total = 999_999

    assert processed == 100
    assert raw_count == 100
    assert len(snapshots) == 5
    assert snapshots[0]["message_index"] == 1
    assert snapshots[-1]["message_index"] == 100


def test_rejects_invalid_limits(tmp_path, monkeypatch):
    install_fake_lob_cpp(monkeypatch)

    source = tmp_path / "sample.txt"
    source.write_bytes(b"fixture")

    with pytest.raises(ValueError, match="max_messages"):
        run_bounded_itch_replay(
            source,
            stock_locate=14,
            max_messages=0,
            snapshot_interval=1,
            levels_per_side=2,
            max_snapshots=10,
        )


def test_rejects_gzip_until_reader_supports_it(
    tmp_path,
    monkeypatch,
):
    install_fake_lob_cpp(monkeypatch)

    source = tmp_path / "sample.gz"
    source.write_bytes(b"fixture")

    with pytest.raises(ValueError, match="decompressed .txt"):
        run_bounded_itch_replay(
            source,
            stock_locate=14,
            max_messages=10,
            snapshot_interval=1,
            levels_per_side=2,
            max_snapshots=10,
        )