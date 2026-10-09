from __future__ import annotations

from collections.abc import Sequence

from benchmark_naive import FEATURE_NAMES, FeatureOutput, Observation

try:
    import numpy as np
except ImportError as exc:
    raise ImportError(
        "benchmark_optimized.py requires NumPy. "
        "Install it with: python -m pip install numpy"
    ) from exc


def calculate_features_optimized(
    observations: Sequence[Observation],
) -> FeatureOutput:
    if not observations:
        return {name: [] for name in FEATURE_NAMES}

    values = np.asarray(observations, dtype=np.float64)

    if values.ndim != 2 or values.shape[1] != 4:
        raise ValueError(
            "Observations must have shape (n, 4): "
            "bid, ask, bid_depth, ask_depth."
        )

    bid = values[:, 0]
    ask = values[:, 1]
    bid_depth = values[:, 2]
    ask_depth = values[:, 3]

    mid_price = (bid + ask) / 2.0
    spread = ask - bid

    relative_spread = np.divide(
        spread,
        mid_price,
        out=np.zeros_like(spread),
        where=mid_price != 0.0,
    )

    total_depth = bid_depth + ask_depth

    depth_imbalance = np.divide(
        bid_depth - ask_depth,
        total_depth,
        out=np.zeros_like(total_depth),
        where=total_depth != 0.0,
    )

    mid_price_return = np.zeros_like(mid_price)

    if mid_price.size > 1:
        previous_mid = mid_price[:-1]

        mid_price_return[1:] = np.divide(
            mid_price[1:] - previous_mid,
            previous_mid,
            out=np.zeros_like(previous_mid),
            where=previous_mid != 0.0,
        )

    columns = {
        "mid_price": mid_price,
        "spread": spread,
        "relative_spread": relative_spread,
        "bid_depth": bid_depth,
        "ask_depth": ask_depth,
        "total_depth": total_depth,
        "depth_imbalance": depth_imbalance,
        "mid_price_return": mid_price_return,
    }

    return {
        name: columns[name].tolist()
        for name in FEATURE_NAMES
    }