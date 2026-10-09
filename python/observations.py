from dataclasses import dataclass

import lob_cpp


@dataclass(frozen=True)
class BookSnapshot:
    best_bid: int
    best_ask: int
    spread: int
    midpoint: float
    bid_levels: tuple[tuple[int, int], ...]
    ask_levels: tuple[tuple[int, int], ...]
    timestamp_ms: int = 0
    message_index: int = 0

    @classmethod
    def from_book(cls, book: lob_cpp.OrderBook, timestamp_ms: int = 0, message_index: int = 0) -> "BookSnapshot":
        bid_levels = tuple(sorted(((level.price, level.total_quantity) for level in book.bid_levels()), key=lambda level: level[0], reverse=True))
        ask_levels = tuple(sorted(((level.price, level.total_quantity) for level in book.ask_levels()), key=lambda level: level[0]))

        return cls(
            best_bid=book.best_bid(),
            best_ask=book.best_ask(),
            spread=book.spread(),
            midpoint=lob_cpp.calculate_mid_price(book),
            bid_levels=bid_levels,
            ask_levels=ask_levels,
            timestamp_ms=timestamp_ms,
            message_index=message_index,
        )