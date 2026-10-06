import sys
from pathlib import Path

PYTHON_DIR = Path(__file__).resolve().parents[2] / "python"
sys.path.insert(0, str(PYTHON_DIR))

import lob_cpp


def test_module_import():
    assert lob_cpp is not None


def test_core_types():
    assert lob_cpp.Side.BUY is not None
    assert lob_cpp.Side.SELL is not None
    assert lob_cpp.Side.NONE is not None

    order = lob_cpp.Order(1, lob_cpp.Side.BUY, 10000, 10)

    assert order.id == 1
    assert order.side == lob_cpp.Side.BUY
    assert order.price == 10000
    assert order.quantity == 10


def test_trade():
    trade = lob_cpp.Trade(1, 2, 10000, 5)

    assert trade.incoming_order == 1
    assert trade.resting_order == 2
    assert trade.price == 10000
    assert trade.quantity == 5


def test_order_book():
    book = lob_cpp.OrderBook()

    order = lob_cpp.Order(1, lob_cpp.Side.BUY, 10000, 10)
    book.add(order)

    assert book.best_bid() == 10000


def test_price_levels():
    book = lob_cpp.OrderBook()

    order = lob_cpp.Order(1, lob_cpp.Side.BUY, 10000, 10)
    book.add(order)

    levels = book.bid_levels()

    assert len(levels) == 1
    assert levels[0].price == 10000
    assert levels[0].total_quantity == 10
    assert len(levels[0].orders) == 1
    assert levels[0].orders[0].id == 1


def test_metrics():
    book = lob_cpp.OrderBook()

    bid = lob_cpp.Order(1, lob_cpp.Side.BUY, 10000, 10)
    ask = lob_cpp.Order(2, lob_cpp.Side.SELL, 10100, 10)

    book.add(bid)
    book.add(ask)

    assert lob_cpp.calculate_best_bid(book) == 10000
    assert lob_cpp.calculate_best_ask(book) == 10100
    assert lob_cpp.calculate_spread(book) == 100


def test_inventory_model():
    inventory = lob_cpp.InventoryModel(100000)

    assert inventory.get_position() == 0
    assert inventory.get_cash() == 100000


def test_simulator():
    simulator = lob_cpp.Simulator(100000)

    inventory = simulator.get_inventory_model()

    assert inventory.get_position() == 0
    assert inventory.get_cash() == 100000


if __name__ == "__main__":
    test_module_import()
    test_core_types()
    test_trade()
    test_order_book()
    test_price_levels()
    test_metrics()
    test_inventory_model()
    test_simulator()
    print("All binding smoke tests passed.")
