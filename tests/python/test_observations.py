import sys
from pathlib import Path
from dataclasses import FrozenInstanceError

import pytest

PROJECT_ROOT = Path(__file__).resolve().parents[2]
PYTHON_DIR = PROJECT_ROOT / "python"
sys.path.insert(0, str(PYTHON_DIR))

import lob_cpp
from observations import BookSnapshot


def test_empty_book():
    book = lob_cpp.OrderBook()

    snapshot = BookSnapshot.from_book(book)

    assert snapshot.best_bid == 0
    assert snapshot.best_ask == 0
    assert snapshot.spread == 0
    assert snapshot.midpoint == 0.0
    assert snapshot.bid_levels == ()
    assert snapshot.ask_levels == ()


def test_single_bid():
    book = lob_cpp.OrderBook()
    book.add(lob_cpp.Order(1, lob_cpp.Side.BUY, 10000, 10))

    snapshot = BookSnapshot.from_book(book)

    assert snapshot.best_bid == 10000
    assert snapshot.best_ask == 0
    assert snapshot.spread == 0
    assert snapshot.midpoint == 0.0
    assert snapshot.bid_levels == ((10000, 10),)
    assert snapshot.ask_levels == ()


def test_single_ask():
    book = lob_cpp.OrderBook()
    book.add(lob_cpp.Order(1, lob_cpp.Side.SELL, 10100, 10))

    snapshot = BookSnapshot.from_book(book)

    assert snapshot.best_bid == 0
    assert snapshot.best_ask == 10100
    assert snapshot.spread == 0
    assert snapshot.midpoint == 0.0
    assert snapshot.bid_levels == ()
    assert snapshot.ask_levels == ((10100, 10),)


def test_bid_and_ask():
    book = lob_cpp.OrderBook()

    book.add(lob_cpp.Order(1, lob_cpp.Side.BUY, 10000, 10))
    book.add(lob_cpp.Order(2, lob_cpp.Side.SELL, 10100, 20))

    snapshot = BookSnapshot.from_book(book)

    assert snapshot.best_bid == 10000
    assert snapshot.best_ask == 10100
    assert snapshot.spread == 100
    assert snapshot.midpoint == 10050.0
    assert snapshot.bid_levels == ((10000, 10),)
    assert snapshot.ask_levels == ((10100, 20),)


def test_multiple_price_levels():
    book = lob_cpp.OrderBook()

    book.add(lob_cpp.Order(1, lob_cpp.Side.BUY, 9900, 20))
    book.add(lob_cpp.Order(2, lob_cpp.Side.BUY, 10000, 10))
    book.add(lob_cpp.Order(3, lob_cpp.Side.BUY, 9800, 30))

    book.add(lob_cpp.Order(4, lob_cpp.Side.SELL, 10200, 40))
    book.add(lob_cpp.Order(5, lob_cpp.Side.SELL, 10100, 15))

    snapshot = BookSnapshot.from_book(book)

    assert snapshot.best_bid == 10000
    assert snapshot.best_ask == 10100
    assert snapshot.spread == 100
    assert snapshot.midpoint == 10050.0

    assert snapshot.bid_levels == (
        (10000, 10),
        (9900, 20),
        (9800, 30),
    )

    assert snapshot.ask_levels == (
        (10100, 15),
        (10200, 40),
    )


def test_level_quantity_aggregation():
    book = lob_cpp.OrderBook()

    book.add(lob_cpp.Order(1, lob_cpp.Side.BUY, 10000, 10))
    book.add(lob_cpp.Order(2, lob_cpp.Side.BUY, 10000, 25))

    snapshot = BookSnapshot.from_book(book)

    assert snapshot.bid_levels == ((10000, 35),)


def test_snapshot_is_independent_from_book():
    book = lob_cpp.OrderBook()
    book.add(lob_cpp.Order(1, lob_cpp.Side.BUY, 10000, 10))
    book.add(lob_cpp.Order(2, lob_cpp.Side.SELL, 10100, 10))

    snapshot = BookSnapshot.from_book(book)

    book.add(lob_cpp.Order(3, lob_cpp.Side.BUY, 9900, 50))
    book.modify(1, 10000, 5)

    assert snapshot.best_bid == 10000
    assert snapshot.best_ask == 10100
    assert snapshot.spread == 100
    assert snapshot.midpoint == 10050.0
    assert snapshot.bid_levels == ((10000, 10),)


def test_snapshot_is_immutable():
    book = lob_cpp.OrderBook()
    book.add(lob_cpp.Order(1, lob_cpp.Side.BUY, 10000, 10))

    snapshot = BookSnapshot.from_book(book)

    with pytest.raises(FrozenInstanceError):
        snapshot.best_bid = 11000