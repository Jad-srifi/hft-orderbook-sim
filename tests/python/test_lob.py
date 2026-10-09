import sys
from pathlib import Path

PROJECT_ROOT = Path(__file__).resolve().parents[2]
PYTHON_DIR = PROJECT_ROOT / "python"

sys.path.insert(0, str(PYTHON_DIR))

import lob_cpp
from lob import OrderBook


def test_order_book_construction():
    book = OrderBook()

    assert book.best_bid() == 0
    assert book.best_ask() == 0
    assert book.spread() == 0


def test_add_and_quotes():
    book = OrderBook()

    bid = lob_cpp.Order(
        1,
        lob_cpp.Side.BUY,
        10000,
        100,
    )

    ask = lob_cpp.Order(
        2,
        lob_cpp.Side.SELL,
        10100,
        50,
    )

    book.add(bid)
    book.add(ask)

    assert book.best_bid() == 10000
    assert book.best_ask() == 10100
    assert book.spread() == 100


def test_cancel():
    book = OrderBook()

    bid = lob_cpp.Order(
        1,
        lob_cpp.Side.BUY,
        10000,
        100,
    )

    book.add(bid)

    assert book.best_bid() == 10000

    book.cancel(1)

    assert book.best_bid() == 0
    assert book.best_ask() == 0
    assert book.spread() == 0


def test_modify_quantity_decrease():
    book = OrderBook()

    bid = lob_cpp.Order(
        1,
        lob_cpp.Side.BUY,
        10000,
        100,
    )

    book.add(bid)
    book.modify(1, 10000, 50)

    levels = book.bid_levels()

    assert len(levels) == 1
    assert levels[0].price == 10000
    assert levels[0].total_quantity == 50
    assert len(levels[0].orders) == 1
    assert levels[0].orders[0].id == 1
    assert levels[0].orders[0].quantity == 50


def test_modify_price_change():
    book = OrderBook()

    bid = lob_cpp.Order(
        1,
        lob_cpp.Side.BUY,
        10000,
        100,
    )

    book.add(bid)
    book.modify(1, 9900, 100)

    assert book.best_bid() == 9900

    levels = book.bid_levels()

    assert len(levels) == 1
    assert levels[0].price == 9900
    assert levels[0].total_quantity == 100


def test_modify_quantity_increase_reinserts():
    book = OrderBook()

    first = lob_cpp.Order(
        1,
        lob_cpp.Side.BUY,
        10000,
        100,
    )

    second = lob_cpp.Order(
        2,
        lob_cpp.Side.BUY,
        10000,
        100,
    )

    book.add(first)
    book.add(second)

    book.modify(1, 10000, 150)

    levels = book.bid_levels()

    assert len(levels) == 1
    assert levels[0].price == 10000
    assert levels[0].total_quantity == 250
    assert len(levels[0].orders) == 2

    assert levels[0].orders[0].id == 2
    assert levels[0].orders[1].id == 1


def test_process_order_partial_fill():
    book = OrderBook()

    resting = lob_cpp.Order(
        1,
        lob_cpp.Side.SELL,
        10000,
        100,
    )

    incoming = lob_cpp.Order(
        2,
        lob_cpp.Side.BUY,
        10000,
        40,
    )

    book.add(resting)

    trades = book.process_order(incoming)

    assert len(trades) == 1
    assert trades[0].incoming_order == 2
    assert trades[0].resting_order == 1
    assert trades[0].price == 10000
    assert trades[0].quantity == 40

    levels = book.ask_levels()

    assert len(levels) == 1
    assert levels[0].price == 10000
    assert levels[0].total_quantity == 60
    assert len(levels[0].orders) == 1
    assert levels[0].orders[0].id == 1
    assert levels[0].orders[0].quantity == 60


def test_process_order_full_fill():
    book = OrderBook()

    resting = lob_cpp.Order(
        1,
        lob_cpp.Side.SELL,
        10000,
        100,
    )

    incoming = lob_cpp.Order(
        2,
        lob_cpp.Side.BUY,
        10000,
        100,
    )

    book.add(resting)

    trades = book.process_order(incoming)

    assert len(trades) == 1
    assert trades[0].incoming_order == 2
    assert trades[0].resting_order == 1
    assert trades[0].price == 10000
    assert trades[0].quantity == 100

    assert len(book.ask_levels()) == 0


def test_bid_and_ask_levels():
    book = OrderBook()

    bid = lob_cpp.Order(
        1,
        lob_cpp.Side.BUY,
        10000,
        100,
    )

    ask = lob_cpp.Order(
        2,
        lob_cpp.Side.SELL,
        10100,
        50,
    )

    book.add(bid)
    book.add(ask)

    bid_levels = book.bid_levels()
    ask_levels = book.ask_levels()

    assert len(bid_levels) == 1
    assert bid_levels[0].price == 10000
    assert bid_levels[0].total_quantity == 100

    assert len(ask_levels) == 1
    assert ask_levels[0].price == 10100
    assert ask_levels[0].total_quantity == 50