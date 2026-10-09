import lob_cpp


class OrderBook:
    def __init__(self) -> None:
        self._book = lob_cpp.OrderBook()

    def add(self, order: lob_cpp.Order) -> None:
        self._book.add(order)

    def cancel(self, order_id: int) -> None:
        self._book.cancel(order_id)

    def modify(
        self,
        order_id: int,
        new_price: int,
        new_quantity: int,
    ) -> None:
        self._book.modify(
            order_id,
            new_price,
            new_quantity,
        )

    def process_order(self, order: lob_cpp.Order):
        return self._book.process_order(order)

    def best_bid(self) -> int:
        return self._book.best_bid()

    def best_ask(self) -> int:
        return self._book.best_ask()

    def spread(self) -> int:
        return self._book.spread()

    def bid_levels(self):
        return self._book.bid_levels()

    def ask_levels(self):
        return self._book.ask_levels()