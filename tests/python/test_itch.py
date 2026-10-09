import sys
from pathlib import Path

import pytest

PROJECT_ROOT = Path(__file__).resolve().parents[2]
PYTHON_DIR = PROJECT_ROOT / "python"
sys.path.insert(0, str(PYTHON_DIR))

import lob_cpp
from itch import HistoricalReplay


def test_historical_replay_empty_file(tmp_path):
    file_path = tmp_path / "empty.txt"
    file_path.write_bytes(b"")

    book = lob_cpp.OrderBook()

    replay = HistoricalReplay(
        str(file_path),
        123,
        book,
    )

    result = replay.run()

    assert result is book
    assert book.best_bid() == 0
    assert book.best_ask() == 0


@pytest.mark.skipif(
    not (
        PROJECT_ROOT
        / "data"
        / "itch"
        / "2019-10-18"
        / "decompressed"
        / "S101819-v50.txt"
    ).exists(),
    reason="Full ITCH dataset not available",
)
def test_historical_replay_real_itch_file():
    file_path = (
        PROJECT_ROOT
        / "data"
        / "itch"
        / "2019-10-18"
        / "decompressed"
        / "S101819-v50.txt"
    )

    book = lob_cpp.OrderBook()

    replay = HistoricalReplay(
        str(file_path),
        123,
        book,
    )

    result = replay.run()

    assert result is book