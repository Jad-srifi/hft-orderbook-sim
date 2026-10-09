from observations import BookSnapshot


def get_midpoint(snapshot: BookSnapshot) -> float:
    if snapshot.best_bid == 0 or snapshot.best_ask == 0:
        return 0.0

    return (snapshot.best_bid + snapshot.best_ask) / 2.0


def get_spread(snapshot: BookSnapshot) -> int:
    return snapshot.spread


def get_relative_spread(snapshot: BookSnapshot) -> float:
    if snapshot.midpoint == 0.0:
        return 0.0

    return (snapshot.spread / snapshot.midpoint) * 100.0


def get_bid_depth(snapshot: BookSnapshot) -> int:
    return sum(quantity for _, quantity in snapshot.bid_levels)


def get_ask_depth(snapshot: BookSnapshot) -> int:
    return sum(quantity for _, quantity in snapshot.ask_levels)


def get_imbalance(snapshot: BookSnapshot) -> float:
    bid_depth = get_bid_depth(snapshot)
    ask_depth = get_ask_depth(snapshot)

    total_depth = bid_depth + ask_depth

    if total_depth == 0:
        return 0.0

    return (bid_depth - ask_depth) / total_depth