from __future__ import annotations

from collections.abc import Sequence

Observation = tuple[float, float, float, float]
FeatureOutput = dict[str, list[float]]

FEATURE_NAMES = (
    "mid_price",
    "spread",
    "relative_spread",
    "bid_depth",
    "ask_depth",
    "total_depth",
    "depth_imbalance",
    "mid_price_return",
)


def calculate_features_naive(observations: Sequence[Observation]) -> FeatureOutput:
    output: FeatureOutput = {name: [] for name in FEATURE_NAMES}
    previous_mid: float | None = None

    for observation in observations:
        if len(observation) != 4:
            raise ValueError(
                "Each observation must contain bid, ask, bid_depth, ask_depth."
            )

        bid, ask, bid_depth, ask_depth = map(float, observation)
        mid = (bid + ask) / 2.0
        spread = ask - bid
        total_depth = bid_depth + ask_depth

        relative_spread = spread / mid if mid != 0.0 else 0.0
        imbalance = (
            (bid_depth - ask_depth) / total_depth
            if total_depth != 0.0
            else 0.0
        )
        mid_return = (
            (mid - previous_mid) / previous_mid
            if previous_mid is not None and previous_mid != 0.0
            else 0.0
        )

        output["mid_price"].append(mid)
        output["spread"].append(spread)
        output["relative_spread"].append(relative_spread)
        output["bid_depth"].append(bid_depth)
        output["ask_depth"].append(ask_depth)
        output["total_depth"].append(total_depth)
        output["depth_imbalance"].append(imbalance)
        output["mid_price_return"].append(mid_return)

        previous_mid = mid

    return output