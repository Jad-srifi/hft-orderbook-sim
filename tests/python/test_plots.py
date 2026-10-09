import sys
from pathlib import Path

import matplotlib
import matplotlib.pyplot as plt
import pytest

matplotlib.use("Agg")

PROJECT_ROOT = Path(__file__).resolve().parents[2]
PYTHON_DIR = PROJECT_ROOT / "python"
sys.path.insert(0, str(PYTHON_DIR))

from observations import BookSnapshot
from plots.book_depth import plot_bid_ask_depth, plot_bid_ask_depth_change, plot_depth_by_level, plot_total_depth
from plots.distributions import plot_box_distribution, plot_comparison_distribution, plot_cumulative_distribution, plot_distribution
from plots.execution import plot_execution_cost, plot_executed_quantity, plot_executed_value, plot_liquidity_consumed, plot_vwap, plot_vwap_vs_reference
from plots.imbalance import plot_bid_ask_depth as plot_imbalance_bid_ask_depth
from plots.imbalance import plot_imbalance, plot_imbalance_distribution, plot_imbalance_vs_return
from plots.inventory import plot_inventory_exposure, plot_inventory_summary, plot_portfolio_value, plot_position, plot_realized_pnl, plot_unrealized_pnl
from plots.midpoint import plot_midpoint, plot_midpoint_distribution, plot_midpoint_returns, plot_return_distribution
from plots.replay import ReplayPlayer, create_replay, replay_frame, replay_state
from plots.slippage import plot_slippage, plot_slippage_by_side, plot_slippage_distribution, plot_slippage_vs_quantity
from plots.spread import plot_relative_spread, plot_spread, plot_spread_distribution


@pytest.fixture
def snapshot():
    return BookSnapshot(
        best_bid=10000,
        best_ask=10010,
        spread=10,
        midpoint=10005.0,
        bid_levels=((10000, 500), (9990, 300), (9980, 200)),
        ask_levels=((10010, 400), (10020, 250), (10030, 150)),
        timestamp_ms=34245000,
        message_index=1000,
    )


@pytest.fixture
def snapshots(snapshot):
    return [
        snapshot,
        BookSnapshot(
            best_bid=10000,
            best_ask=10010,
            spread=10,
            midpoint=10005.0,
            bid_levels=((10000, 600), (9990, 250), (9980, 150)),
            ask_levels=((10010, 350), (10020, 300), (10030, 200)),
            timestamp_ms=34245010,
            message_index=1010,
        ),
    ]


def test_plot_bid_ask_depth(snapshot):
    fig, ax = plot_bid_ask_depth(snapshot)

    assert fig is not None
    assert ax is not None

    plt.close(fig)


def test_plot_depth_by_level(snapshot):
    fig, ax = plot_depth_by_level(snapshot)

    assert fig is not None
    assert ax is not None

    plt.close(fig)


def test_plot_total_depth(snapshots):
    fig, ax = plot_total_depth(snapshots)

    assert fig is not None
    assert ax is not None

    plt.close(fig)


def test_plot_bid_ask_depth_change(snapshots):
    fig, ax = plot_bid_ask_depth_change(snapshots)

    assert fig is not None
    assert ax is not None

    plt.close(fig)


def test_plot_imbalance():
    fig, ax = plot_imbalance([0.1, 0.2, -0.1, 0.3])

    assert fig is not None
    assert ax is not None

    plt.close(fig)


def test_plot_imbalance_bid_ask_depth():
    fig, ax = plot_imbalance_bid_ask_depth([100, 200, 150], [80, 250, 120])

    assert fig is not None
    assert ax is not None

    plt.close(fig)


def test_plot_imbalance_distribution():
    fig, ax = plot_imbalance_distribution([0.1, 0.2, -0.1, 0.3, 0.0])

    assert fig is not None
    assert ax is not None

    plt.close(fig)


def test_plot_imbalance_vs_return():
    fig, ax = plot_imbalance_vs_return([0.1, 0.2, -0.1], [0.001, 0.002, -0.001])

    assert fig is not None
    assert ax is not None

    plt.close(fig)


def test_plot_spread():
    fig, ax = plot_spread([10, 12, 8, 11])

    assert fig is not None
    assert ax is not None

    plt.close(fig)


def test_plot_relative_spread():
    fig, ax = plot_relative_spread([0.1, 0.12, 0.08, 0.11])

    assert fig is not None
    assert ax is not None

    plt.close(fig)


def test_plot_spread_distribution():
    fig, ax = plot_spread_distribution([10, 12, 8, 11, 9])

    assert fig is not None
    assert ax is not None

    plt.close(fig)


def test_plot_midpoint():
    fig, ax = plot_midpoint([10000, 10005, 10002, 10010])

    assert fig is not None
    assert ax is not None

    plt.close(fig)


def test_plot_midpoint_returns():
    fig, ax = plot_midpoint_returns([0.001, -0.002, 0.003, 0.001])

    assert fig is not None
    assert ax is not None

    plt.close(fig)


def test_plot_midpoint_distribution():
    fig, ax = plot_midpoint_distribution([10000, 10005, 10002, 10010])

    assert fig is not None
    assert ax is not None

    plt.close(fig)


def test_plot_return_distribution():
    fig, ax = plot_return_distribution([0.001, -0.002, 0.003, 0.001])

    assert fig is not None
    assert ax is not None

    plt.close(fig)


def test_plot_position():
    fig, ax = plot_position([0, 10, 5, -3])

    assert fig is not None
    assert ax is not None

    plt.close(fig)


def test_plot_inventory_exposure():
    fig, ax = plot_inventory_exposure([0, 1000, 500, -300])

    assert fig is not None
    assert ax is not None

    plt.close(fig)


def test_plot_realized_pnl():
    fig, ax = plot_realized_pnl([0, 10, 15, 20])

    assert fig is not None
    assert ax is not None

    plt.close(fig)


def test_plot_unrealized_pnl():
    fig, ax = plot_unrealized_pnl([0, 5, 12, 8])

    assert fig is not None
    assert ax is not None

    plt.close(fig)


def test_plot_portfolio_value():
    fig, ax = plot_portfolio_value([10000, 10010, 10020, 10015])

    assert fig is not None
    assert ax is not None

    plt.close(fig)


def test_plot_inventory_summary():
    fig, axes = plot_inventory_summary(
        [0, 10, 5, -3],
        [0, 10, 15, 20],
        [0, 5, 12, 8],
        [10000, 10010, 10020, 10015],
    )

    assert fig is not None
    assert axes is not None
    assert len(axes) == 4

    plt.close(fig)


def test_plot_slippage():
    fig, ax = plot_slippage([1.0, 1.5, 0.8, 1.2])

    assert fig is not None
    assert ax is not None

    plt.close(fig)


def test_plot_slippage_vs_quantity():
    fig, ax = plot_slippage_vs_quantity([10, 20, 30, 40], [1.0, 1.5, 2.0, 2.5])

    assert fig is not None
    assert ax is not None

    plt.close(fig)


def test_plot_slippage_by_side():
    fig, ax = plot_slippage_by_side([1.0, 1.5, 1.2], [0.8, 1.1, 1.0])

    assert fig is not None
    assert ax is not None

    plt.close(fig)


def test_plot_slippage_distribution():
    fig, ax = plot_slippage_distribution([1.0, 1.5, 0.8, 1.2, 1.1])

    assert fig is not None
    assert ax is not None

    plt.close(fig)


def test_plot_vwap():
    fig, ax = plot_vwap([10000, 10005, 10003, 10008])

    assert fig is not None
    assert ax is not None

    plt.close(fig)


def test_plot_vwap_vs_reference():
    fig, ax = plot_vwap_vs_reference([10000, 10005, 10003], [9998, 10002, 10001])

    assert fig is not None
    assert ax is not None

    plt.close(fig)


def test_plot_execution_cost():
    fig, ax = plot_execution_cost([100, 150, 120, 180])

    assert fig is not None
    assert ax is not None

    plt.close(fig)


def test_plot_executed_quantity():
    fig, ax = plot_executed_quantity([10, 20, 15, 25])

    assert fig is not None
    assert ax is not None

    plt.close(fig)


def test_plot_executed_value():
    fig, ax = plot_executed_value([100000, 200000, 150000, 250000])

    assert fig is not None
    assert ax is not None

    plt.close(fig)


def test_plot_liquidity_consumed():
    fig, ax = plot_liquidity_consumed([10, 30, 20, 40])

    assert fig is not None
    assert ax is not None

    plt.close(fig)


def test_plot_distribution():
    fig, ax = plot_distribution([1, 2, 3, 4, 5])

    assert fig is not None
    assert ax is not None

    plt.close(fig)


def test_plot_comparison_distribution():
    fig, ax = plot_comparison_distribution([1, 2, 3, 4], [2, 3, 4, 5])

    assert fig is not None
    assert ax is not None

    plt.close(fig)


def test_plot_box_distribution():
    fig, ax = plot_box_distribution([[1, 2, 3], [2, 3, 4]], ["First", "Second"])

    assert fig is not None
    assert ax is not None

    plt.close(fig)


def test_plot_cumulative_distribution():
    fig, ax = plot_cumulative_distribution([1, 2, 3, 4, 5])

    assert fig is not None
    assert ax is not None

    plt.close(fig)


def test_replay_player_creation(snapshots):
    player = ReplayPlayer(tuple(snapshots), 100)

    assert player.total == 2
    assert player.index == 0
    assert player.current == snapshots[0]
    assert player.finished is False


def test_replay_player_step_forward(snapshots):
    player = create_replay(snapshots, 100)

    current = player.step_forward()

    assert current == snapshots[1]
    assert player.index == 1
    assert player.finished is True


def test_replay_player_step_backward(snapshots):
    player = create_replay(snapshots, 100)

    player.step_forward()
    current = player.step_backward()

    assert current == snapshots[0]
    assert player.index == 0


def test_replay_player_reset(snapshots):
    player = create_replay(snapshots, 100)

    player.step_forward()
    current = player.reset()

    assert current == snapshots[0]
    assert player.index == 0


def test_replay_player_seek(snapshots):
    player = create_replay(snapshots, 100)

    current = player.seek(1)

    assert current == snapshots[1]
    assert player.index == 1


def test_replay_player_set_interval(snapshots):
    player = create_replay(snapshots, 100)

    player.set_interval(250)

    assert player.interval_ms == 250


def test_replay_frame(snapshot):
    frame = replay_frame(snapshot)

    assert frame["timestamp_ms"] == 34245000
    assert frame["message_index"] == 1000
    assert frame["best_bid"] == 10000
    assert frame["best_ask"] == 10010
    assert frame["spread"] == 10
    assert frame["midpoint"] == 10005.0
    assert frame["bid_levels"] == [(10000, 500), (9990, 300), (9980, 200)]
    assert frame["ask_levels"] == [(10010, 400), (10020, 250), (10030, 150)]


def test_replay_state(snapshots):
    player = create_replay(snapshots, 100)

    state = replay_state(player)

    assert state["index"] == 0
    assert state["total"] == 2
    assert state["finished"] is False
    assert state["interval_ms"] == 100
    assert state["snapshot"]["timestamp_ms"] == 34245000
    assert state["snapshot"]["message_index"] == 1000


def test_empty_replay():
    player = create_replay([], 100)

    assert player.current is None
    assert player.total == 0
    assert player.finished is True
    assert replay_frame(None)["timestamp_ms"] == 0