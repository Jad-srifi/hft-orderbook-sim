from __future__ import annotations

import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
PYTHON_DIR = ROOT / "python"
if str(PYTHON_DIR) not in sys.path:
    sys.path.insert(0, str(PYTHON_DIR))

import lob_cpp


def print_side(name: str, levels: object) -> None:
    print(name)
    for index, level in enumerate(levels, start=1):
        price = getattr(level, "price", None)
        quantity = getattr(level, "total_quantity", None)
        if quantity is None:
            quantity = sum(float(getattr(order, "quantity", 0)) for order in getattr(level, "orders", []))
        print(f"  level={index} price={price} quantity={quantity}")


def main() -> None:
    book = lob_cpp.OrderBook()
    book.add(lob_cpp.Order(1, lob_cpp.Side.BUY, 10000, 8))
    book.add(lob_cpp.Order(2, lob_cpp.Side.BUY, 9990, 12))
    book.add(lob_cpp.Order(3, lob_cpp.Side.BUY, 9980, 5))
    book.add(lob_cpp.Order(4, lob_cpp.Side.SELL, 10020, 7))
    book.add(lob_cpp.Order(5, lob_cpp.Side.SELL, 10030, 9))
    book.add(lob_cpp.Order(6, lob_cpp.Side.SELL, 10040, 4))

    print(f"Best bid: {book.best_bid()}")
    print(f"Best ask: {book.best_ask()}")
    print(f"Spread: {book.spread()}")
    print(f"Midpoint: {(book.best_bid() + book.best_ask()) / 2 if book.best_bid() and book.best_ask() else 0}")
    print_side("Bids:", book.bid_levels())
    print_side("Asks:", book.ask_levels())


if __name__ == "__main__":
    main()
