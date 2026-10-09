from observations import BookSnapshot

from research.microstructure import (
    get_ask_depth,
    get_bid_depth,
    get_imbalance,
    get_midpoint,
    get_relative_spread,
    get_spread,
)


def midpoint_change(
    current: BookSnapshot,
    previous: BookSnapshot,
) -> float:
    return get_midpoint(current) - get_midpoint(previous)


def midpoint_return(
    current: BookSnapshot,
    previous: BookSnapshot,
) -> float:
    previous_midpoint = get_midpoint(previous)

    if previous_midpoint == 0.0:
        return 0.0

    current_midpoint = get_midpoint(current)

    return (current_midpoint / previous_midpoint) - 1.0


def spread_change(
    current: BookSnapshot,
    previous: BookSnapshot,
) -> int:
    return get_spread(current) - get_spread(previous)


def imbalance_change(
    current: BookSnapshot,
    previous: BookSnapshot,
) -> float:
    return get_imbalance(current) - get_imbalance(previous)


def extract_features(
    snapshot: BookSnapshot,
) -> dict[str, float | int]:
    return {
        "midpoint": get_midpoint(snapshot),
        "spread": get_spread(snapshot),
        "relative_spread": get_relative_spread(snapshot),
        "bid_depth": get_bid_depth(snapshot),
        "ask_depth": get_ask_depth(snapshot),
        "imbalance": get_imbalance(snapshot),
    }