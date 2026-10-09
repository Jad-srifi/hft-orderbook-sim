import sys
from pathlib import Path
import pytest

PROJECT_ROOT = Path(__file__).resolve().parents[2]
PYTHON_DIR = PROJECT_ROOT / "python"
RESEARCH_DIR = PYTHON_DIR / "research"

sys.path.insert(0, str(PYTHON_DIR))
sys.path.insert(0, str(RESEARCH_DIR))

from observations import BookSnapshot
from research.features import (
    extract_features,
    imbalance_change,
    midpoint_change,
    midpoint_return,
    spread_change,
)


def make_snapshot(
    best_bid: int,
    best_ask: int,
    spread: int,
    midpoint: float,
    bid_levels: tuple[tuple[int, int], ...],
    ask_levels: tuple[tuple[int, int], ...],
) -> BookSnapshot:
    return BookSnapshot(
        best_bid=best_bid,
        best_ask=best_ask,
        spread=spread,
        midpoint=midpoint,
        bid_levels=bid_levels,
        ask_levels=ask_levels,
    )


def test_midpoint_change():
    previous = make_snapshot(
        10000,
        10100,
        100,
        10050.0,
        ((10000, 10),),
        ((10100, 10),),
    )

    current = make_snapshot(
        10050,
        10150,
        100,
        10100.0,
        ((10050, 10),),
        ((10150, 10),),
    )

    assert midpoint_change(current, previous) == 50.0


def test_midpoint_return():
    previous = make_snapshot(
        10000,
        10100,
        100,
        10050.0,
        ((10000, 10),),
        ((10100, 10),),
    )

    current = make_snapshot(
        10050,
        10150,
        100,
        10100.0,
        ((10050, 10),),
        ((10150, 10),),
    )

    assert midpoint_return(current, previous) == (
        10100.0 / 10050.0
    ) - 1.0


def test_midpoint_return_with_missing_previous_midpoint():
    previous = make_snapshot(
        10000,
        0,
        0,
        0.0,
        ((10000, 10),),
        (),
    )

    current = make_snapshot(
        10000,
        10100,
        100,
        10050.0,
        ((10000, 10),),
        ((10100, 10),),
    )

    assert midpoint_return(current, previous) == 0.0


def test_spread_change():
    previous = make_snapshot(
        10000,
        10100,
        100,
        10050.0,
        ((10000, 10),),
        ((10100, 10),),
    )

    current = make_snapshot(
        10000,
        10200,
        200,
        10100.0,
        ((10000, 10),),
        ((10200, 10),),
    )

    assert spread_change(current, previous) == 100


def test_imbalance_change():
    previous = make_snapshot(
        10000,
        10100,
        100,
        10050.0,
        ((10000, 40),),
        ((10100, 60),),
    )

    current = make_snapshot(
        10000,
        10100,
        100,
        10050.0,
        ((10000, 70),),
        ((10100, 30),),
    )

    assert imbalance_change(current, previous) == pytest.approx(0.6)


def test_extract_features():
    snapshot = make_snapshot(
        10000,
        10100,
        100,
        10050.0,
        (
            (10000, 10),
            (9900, 20),
        ),
        (
            (10100, 15),
            (10200, 25),
        ),
    )

    features = extract_features(snapshot)

    assert features["midpoint"] == 10050.0
    assert features["spread"] == 100
    assert features["relative_spread"] == (100 / 10050.0) * 100.0
    assert features["bid_depth"] == 30
    assert features["ask_depth"] == 40
    assert features["imbalance"] == -0.14285714285714285