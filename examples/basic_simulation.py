from __future__ import annotations

import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
PYTHON_DIR = ROOT / "python"
if str(PYTHON_DIR) not in sys.path:
    sys.path.insert(0, str(PYTHON_DIR))

import lob_cpp


def main() -> None:
    book = lob_cpp.OrderBook()
    book.add(lob_cpp.Order(1, lob_cpp.Side.BUY, 10000, 10))
    book.add(lob_cpp.Order(2, lob_cpp.Side.SELL, 10010, 6))

    print(f"Initial best bid: {book.best_bid()}")
    print(f"Initial best ask: {book.best_ask()}")
    print(f"Initial spread: {book.spread()}")

    incoming = lob_cpp.Order(3, lob_cpp.Side.BUY, 10010, 4)
    trades = book.process_order(incoming)

    print("Trades:")
    for trade in trades or []:
        print(
            f"  incoming={trade.incoming_order}, "
            f"resting={trade.resting_order}, "
            f"price={trade.price}, quantity={trade.quantity}"
        )

    print(f"Final best bid: {book.best_bid()}")
    print(f"Final best ask: {book.best_ask()}")
    print(f"Final spread: {book.spread()}")


if __name__ == "__main__":
    main()
