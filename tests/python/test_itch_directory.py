
import gzip
import sys
from pathlib import Path

import pytest


ROOT = Path(__file__).resolve().parents[2]
PYTHON_DIR = ROOT / "python"

if str(PYTHON_DIR) not in sys.path:
    sys.path.insert(0, str(PYTHON_DIR))

from itch_directory import read_stock_directory


def _itch_record(message: bytes) -> bytes:
    return len(message).to_bytes(2, "big") + message


def _stock_directory(
    stock_locate: int,
    symbol: str,
    market_category: str = "Q",
) -> bytes:
    message = bytearray(39)
    message[0] = ord("R")
    message[1:3] = stock_locate.to_bytes(2, "big")
    message[11:19] = symbol.encode("ascii").ljust(8, b" ")[:8]
    message[19] = ord(market_category)
    return bytes(message)


def test_reads_and_sorts_stock_directory_entries(tmp_path):
    path = tmp_path / "sample.itch"
    path.write_bytes(
        _itch_record(b"S" + b"\x00" * 11)
        + _itch_record(_stock_directory(123, "MSFT", "Q"))
        + _itch_record(_stock_directory(7, "AAPL", "Q"))
        + _itch_record(b"H" + b"\x00" * 10)
        + _itch_record(b"H" + b"\x00" * 10)
    )

    entries = read_stock_directory(path, non_directory_limit=2)

    assert [
        (entry.stock_locate, entry.symbol, entry.market_category)
        for entry in entries
    ] == [
        (7, "AAPL", "Q"),
        (123, "MSFT", "Q"),
    ]


def test_reads_gzip_input(tmp_path):
    path = tmp_path / "sample.itch.gz"

    with gzip.open(path, "wb") as stream:
        stream.write(_itch_record(_stock_directory(123, "MSFT")))
        stream.write(_itch_record(b"H" + b"\x00" * 10))

    entries = read_stock_directory(path, non_directory_limit=1)

    assert len(entries) == 1
    assert entries[0].stock_locate == 123
    assert entries[0].symbol == "MSFT"


def test_rejects_truncated_length_prefix(tmp_path):
    path = tmp_path / "truncated.itch"
    path.write_bytes(b"\x00")

    with pytest.raises(
        ValueError,
        match="Truncated ITCH message length prefix",
    ):
        read_stock_directory(path)


def test_rejects_truncated_message_body(tmp_path):
    path = tmp_path / "truncated.itch"
    path.write_bytes((12).to_bytes(2, "big") + b"R\x00")

    with pytest.raises(ValueError, match="Truncated ITCH message"):
        read_stock_directory(path)


def test_rejects_conflicting_symbols_for_same_locate(tmp_path):
    path = tmp_path / "conflict.itch"
    path.write_bytes(
        _itch_record(_stock_directory(123, "MSFT"))
        + _itch_record(_stock_directory(123, "AAPL"))
    )

    with pytest.raises(
        ValueError,
        match="Conflicting symbols for Stock Locate 123",
    ):
        read_stock_directory(path, non_directory_limit=1)