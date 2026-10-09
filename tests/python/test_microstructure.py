import sys
from pathlib import Path

PROJECT_ROOT = Path(__file__).resolve().parents[2]
PYTHON_DIR = PROJECT_ROOT / "python"
RESEARCH_DIR = PYTHON_DIR / "research"

sys.path.insert(0, str(PYTHON_DIR))
sys.path.insert(0, str(RESEARCH_DIR))

from observations import BookSnapshot
from research.microstructure import (
    get_ask_depth,
    get_bid_depth,
    get_imbalance,
    get_midpoint,
    get_relative_spread,
    get_spread,
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


def test_empty_book():
    snapshot = make_snapshot(
        0,
        0,
        0,
        0.0,
        (),
        (),
    )

    assert get_midpoint(snapshot) == 0.0
    assert get_spread(snapshot) == 0
    assert get_relative_spread(snapshot) == 0.0
    assert get_bid_depth(snapshot) == 0
    assert get_ask_depth(snapshot) == 0
    assert get_imbalance(snapshot) == 0.0


def test_two_sided_book():
    snapshot = make_snapshot(
        10000,
        10100,
        100,
        10050.0,
        ((10000, 10),),
        ((10100, 20),),
    )

    assert get_midpoint(snapshot) == 10050.0
    assert get_spread(snapshot) == 100
    assert get_relative_spread(snapshot) == (100 / 10050.0) * 100.0
    assert get_bid_depth(snapshot) == 10
    assert get_ask_depth(snapshot) == 20


def test_multiple_levels():
    snapshot = make_snapshot(
        10000,
        10100,
        100,
        10050.0,
        (
            (10000, 10),
            (9900, 20),
            (9800, 30),
        ),
        (
            (10100, 15),
            (10200, 25),
        ),
    )

    assert get_bid_depth(snapshot) == 60
    assert get_ask_depth(snapshot) == 40


def test_positive_imbalance():
    snapshot = make_snapshot(
        10000,
        10100,
        100,
        10050.0,
        ((10000, 60),),
        ((10100, 40),),
    )

    assert get_imbalance(snapshot) == 0.2


def test_negative_imbalance():
    snapshot = make_snapshot(
        10000,
        10100,
        100,
        10050.0,
        ((10000, 40),),
        ((10100, 60),),
    )

    assert get_imbalance(snapshot) == -0.2


def test_zero_depth_imbalance():
    snapshot = make_snapshot(
        10000,
        10100,
        100,
        10050.0,
        (),
        (),
    )

    assert get_imbalance(snapshot) == 0.0


def test_one_sided_book():
    snapshot = make_snapshot(
        10000,
        0,
        0,
        0.0,
        ((10000, 50),),
        (),
    )

    assert get_midpoint(snapshot) == 0.0
    assert get_relative_spread(snapshot) == 0.0
    assert get_bid_depth(snapshot) == 50
    assert get_ask_depth(snapshot) == 0
    assert get_imbalance(snapshot) == 1.0


def test_fractional_midpoint():
    snapshot = make_snapshot(
        10000,
        10101,
        101,
        10050.5,
        ((10000, 10),),
        ((10101, 10),),
    )

    assert get_midpoint(snapshot) == 10050.5