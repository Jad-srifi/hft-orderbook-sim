from observations import BookSnapshot

from research.features import (
    imbalance_change,
    midpoint_change,
    midpoint_return,
    spread_change,
)
from research.microstructure import (
    get_ask_depth,
    get_bid_depth,
)


def bid_depth_change(current, previous):
    return get_bid_depth(current) - get_bid_depth(previous)


def ask_depth_change(current, previous):
    return get_ask_depth(current) - get_ask_depth(previous)


def summarize_market_impact(current, previous):
    return {
        "midpoint_change": midpoint_change(current, previous),
        "midpoint_return": midpoint_return(current, previous),
        "spread_change": spread_change(current, previous),
        "bid_depth_change": bid_depth_change(current, previous),
        "ask_depth_change": ask_depth_change(current, previous),
        "imbalance_change": imbalance_change(current, previous),
    }