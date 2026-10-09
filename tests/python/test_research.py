import sys
from pathlib import Path

import pytest

PROJECT_ROOT = Path(__file__).resolve().parents[2]
PYTHON_DIR = PROJECT_ROOT / "python"
sys.path.insert(0, str(PYTHON_DIR))

import lob_cpp

from observations import BookSnapshot
from research.microstructure import get_midpoint, get_spread, get_relative_spread, get_bid_depth, get_ask_depth, get_imbalance
from research.features import midpoint_change, midpoint_return, spread_change, imbalance_change, extract_features
from research.execution_analysis import group_trades_by_incoming_order, calculate_executed_quantity, calculate_executed_value, calculate_VWAP, calculate_slippage, calculate_execution_cost, calculate_liquidity_consumed, calculate_execution_result, calculate_execution_results_by_order
from research.statistics import mean, median, variance, standard_deviation, quantile, correlation, autocorrelation, describe
from research.slippage_analysis import extract_slippages, mean_slippage, median_slippage, slippage_by_side, mean_slippage_by_side, slippage_by_quantity, slippage_by_order
from research.inventory_analysis import get_position, get_cash, get_average_cost, get_realized_pnl, get_unrealized_pnl, get_portfolio_value, get_inventory_exposure, summarize_inventory
from research.market_impact import bid_depth_change, ask_depth_change, summarize_market_impact


@pytest.fixture
def snapshot():
    return BookSnapshot(
        best_bid=10000,
        best_ask=10010,
        spread=10,
        midpoint=10005.0,
        bid_levels=((10000, 100), (9990, 50)),
        ask_levels=((10010, 80), (10020, 70)),
        timestamp_ms=34245000,
        message_index=1000,
    )


@pytest.fixture
def previous_snapshot():
    return BookSnapshot(
        best_bid=9990,
        best_ask=10010,
        spread=20,
        midpoint=10000.0,
        bid_levels=((9990, 100),),
        ask_levels=((10010, 100),),
        timestamp_ms=34245000,
        message_index=1000,
    )


@pytest.fixture
def current_snapshot():
    return BookSnapshot(
        best_bid=10010,
        best_ask=10020,
        spread=10,
        midpoint=10015.0,
        bid_levels=((10010, 100), (10000, 60)),
        ask_levels=((10020, 25), (10030, 15)),
        timestamp_ms=34245010,
        message_index=1010,
    )


def make_trades():
    return [
        lob_cpp.Trade(1, 2, 10000, 10),
        lob_cpp.Trade(1, 3, 10100, 20),
    ]


def make_execution_results():
    trades = [
        lob_cpp.Trade(1, 101, 10000, 10),
        lob_cpp.Trade(1, 102, 10100, 20),
        lob_cpp.Trade(2, 103, 9800, 10),
    ]
    sides = {1: lob_cpp.Side.BUY, 2: lob_cpp.Side.SELL}
    reference_prices = {1: 9900, 2: 10000}
    return calculate_execution_results_by_order(trades, sides, reference_prices)


def make_inventory():
    inventory = lob_cpp.InventoryModel(1_000_000.0)
    inventory.process_trade(lob_cpp.Trade(1, 2, 10000, 10), lob_cpp.Side.BUY)
    return inventory


# ============================================================
# Microstructure
# ============================================================

def test_get_midpoint(snapshot):
    assert get_midpoint(snapshot) == 10005.0


def test_get_midpoint_when_quote_missing():
    snapshot = BookSnapshot(0, 10010, 0, 0.0, (), ((10010, 20),))
    assert get_midpoint(snapshot) == 0.0


def test_get_spread(snapshot):
    assert get_spread(snapshot) == 10


def test_get_relative_spread(snapshot):
    assert get_relative_spread(snapshot) == pytest.approx(10 / 10005.0 * 100)


def test_get_relative_spread_when_midpoint_zero():
    snapshot = BookSnapshot(0, 10010, 0, 0.0, (), ((10010, 20),))
    assert get_relative_spread(snapshot) == 0.0


def test_get_bid_depth(snapshot):
    assert get_bid_depth(snapshot) == 150


def test_get_ask_depth(snapshot):
    assert get_ask_depth(snapshot) == 150


def test_get_imbalance(snapshot):
    assert get_imbalance(snapshot) == pytest.approx(0.0)


def test_get_imbalance_with_empty_book():
    snapshot = BookSnapshot(0, 0, 0, 0.0, (), ())
    assert get_imbalance(snapshot) == 0.0


# ============================================================
# Features
# ============================================================

def test_midpoint_change(previous_snapshot, current_snapshot):
    assert midpoint_change(current_snapshot, previous_snapshot) == 15.0


def test_midpoint_return(previous_snapshot, current_snapshot):
    assert midpoint_return(current_snapshot, previous_snapshot) == pytest.approx(0.0015)


def test_midpoint_return_when_previous_midpoint_zero(current_snapshot):
    previous = BookSnapshot(0, 0, 0, 0.0, (), ())
    assert midpoint_return(current_snapshot, previous) == 0.0


def test_spread_change(previous_snapshot, current_snapshot):
    assert spread_change(current_snapshot, previous_snapshot) == -10


def test_imbalance_change(previous_snapshot, current_snapshot):
    assert imbalance_change(current_snapshot, previous_snapshot) == pytest.approx(0.6)


def test_extract_features(snapshot):
    features = extract_features(snapshot)

    assert features["midpoint"] == 10005.0
    assert features["spread"] == 10
    assert features["relative_spread"] == pytest.approx(10 / 10005.0 * 100)
    assert features["bid_depth"] == 150
    assert features["ask_depth"] == 150
    assert features["imbalance"] == pytest.approx(0.0)


def test_research_features_independent_of_timestamp_metadata(snapshot):
    later_snapshot = BookSnapshot(
        best_bid=snapshot.best_bid,
        best_ask=snapshot.best_ask,
        spread=snapshot.spread,
        midpoint=snapshot.midpoint,
        bid_levels=snapshot.bid_levels,
        ask_levels=snapshot.ask_levels,
        timestamp_ms=34246000,
        message_index=2000,
    )

    assert extract_features(snapshot) == extract_features(later_snapshot)
    assert get_imbalance(snapshot) == get_imbalance(later_snapshot)
    assert get_bid_depth(snapshot) == get_bid_depth(later_snapshot)
    assert get_ask_depth(snapshot) == get_ask_depth(later_snapshot)


# ============================================================
# Execution Analysis
# ============================================================

def test_group_trades_by_incoming_order():
    grouped = group_trades_by_incoming_order(make_trades())

    assert 1 in grouped
    assert len(grouped[1]) == 2


def test_calculate_executed_quantity():
    assert calculate_executed_quantity(make_trades()) == 30


def test_calculate_executed_value():
    assert calculate_executed_value(make_trades()) == 302000


def test_calculate_vwap():
    assert calculate_VWAP(make_trades()) == pytest.approx(10066.6666666667)


def test_calculate_buy_slippage():
    assert calculate_slippage(make_trades(), lob_cpp.Side.BUY, 9900) == pytest.approx(166.6666666667)


def test_calculate_sell_slippage():
    trades = [
        lob_cpp.Trade(1, 2, 9800, 10),
        lob_cpp.Trade(1, 3, 9900, 20),
    ]

    assert calculate_slippage(trades, lob_cpp.Side.SELL, 10000) == pytest.approx(133.3333333333)


def test_calculate_execution_cost():
    assert calculate_execution_cost(make_trades(), lob_cpp.Side.BUY, 9900) == 4999


def test_calculate_liquidity_consumed():
    assert calculate_liquidity_consumed(make_trades()) == 30


def test_calculate_execution_result():
    result = calculate_execution_result(make_trades(), lob_cpp.Side.BUY, 9900)

    assert result.executed_quantity == 30
    assert result.execution_value == 302000
    assert result.execution_vwap == pytest.approx(10066.6666666667)
    assert result.slippage == pytest.approx(166.6666666667)
    assert result.execution_cost == 4999


def test_calculate_execution_results_by_order():
    results = make_execution_results()

    assert 1 in results
    assert 2 in results
    assert results[1].executed_quantity == 30
    assert results[2].executed_quantity == 10


# ============================================================
# Statistics
# ============================================================

def test_statistics_mean():
    assert mean([1, 2, 3, 4, 5]) == pytest.approx(3.0)


def test_statistics_mean_empty():
    assert mean([]) == 0.0


def test_statistics_median():
    assert median([1, 2, 3, 4, 5]) == pytest.approx(3.0)


def test_statistics_median_even_length():
    assert median([1, 2, 3, 4]) == pytest.approx(2.5)


def test_statistics_median_empty():
    assert median([]) == 0.0


def test_statistics_variance():
    assert variance([1, 2, 3, 4, 5]) == pytest.approx(2.5)


def test_statistics_variance_single_value():
    assert variance([5]) == 0.0


def test_statistics_standard_deviation():
    assert standard_deviation([1, 2, 3, 4, 5]) == pytest.approx(2.5 ** 0.5)


def test_statistics_quantile():
    assert quantile([1, 2, 3, 4, 5], 0.5) == pytest.approx(3.0)


def test_statistics_quantile_invalid():
    with pytest.raises(ValueError):
        quantile([1, 2, 3], 1.5)


def test_statistics_correlation():
    assert correlation([1, 2, 3, 4, 5], [2, 4, 6, 8, 10]) == pytest.approx(1.0)


def test_statistics_correlation_length_mismatch():
    with pytest.raises(ValueError):
        correlation([1, 2], [1, 2, 3])


def test_statistics_correlation_constant_input():
    assert correlation([1, 1, 1], [2, 3, 4]) == 0.0


def test_statistics_autocorrelation():
    assert autocorrelation([1, 2, 3, 4, 5], 1) == pytest.approx(1.0)


def test_statistics_autocorrelation_invalid_lag():
    with pytest.raises(ValueError):
        autocorrelation([1, 2, 3], 0)


def test_statistics_autocorrelation_lag_too_large():
    with pytest.raises(ValueError):
        autocorrelation([1, 2, 3], 3)


def test_statistics_describe():
    result = describe([1, 2, 3, 4, 5], [2, 4, 6, 8, 10], 0.5, 1)

    assert "Mean:" in result
    assert "Median:" in result
    assert "Variance:" in result
    assert "STD:" in result
    assert "Quantile(50.0%):" in result
    assert "Autocorrelation(Lag=1):" in result
    assert "Correlation:" in result


# ============================================================
# Slippage Analysis
# ============================================================

def test_extract_slippages():
    results = make_execution_results()
    slippages = extract_slippages(list(results.values()))

    assert len(slippages) == 2
    assert slippages[0] == pytest.approx(166.6666666667)
    assert slippages[1] == pytest.approx(200.0)


def test_mean_slippage():
    results = make_execution_results()
    values = list(results.values())

    assert mean_slippage(values) == pytest.approx((166.6666666667 + 200.0) / 2)


def test_median_slippage():
    results = make_execution_results()
    values = list(results.values())

    assert median_slippage(values) == pytest.approx((166.6666666667 + 200.0) / 2)


def test_slippage_by_side():
    results = make_execution_results()
    sides = {1: lob_cpp.Side.BUY, 2: lob_cpp.Side.SELL}

    grouped = slippage_by_side(results, sides)

    assert lob_cpp.Side.BUY in grouped
    assert lob_cpp.Side.SELL in grouped
    assert grouped[lob_cpp.Side.BUY] == pytest.approx([166.6666666667])
    assert grouped[lob_cpp.Side.SELL] == pytest.approx([200.0])


def test_mean_slippage_by_side():
    results = make_execution_results()
    sides = {1: lob_cpp.Side.BUY, 2: lob_cpp.Side.SELL}

    grouped = mean_slippage_by_side(results, sides)

    assert grouped[lob_cpp.Side.BUY] == pytest.approx(166.6666666667)
    assert grouped[lob_cpp.Side.SELL] == pytest.approx(200.0)


def test_slippage_by_quantity():
    results = make_execution_results()
    values = slippage_by_quantity(list(results.values()))

    assert [quantity for quantity, _ in values] == [30, 10]


def test_slippage_by_order():
    results = make_execution_results()
    values = slippage_by_order(results)

    assert set(values) == {1, 2}
    assert values[1] == pytest.approx(166.6666666667)
    assert values[2] == pytest.approx(200.0)


# ============================================================
# Inventory Analysis
# ============================================================

def test_inventory_get_position():
    assert get_position(make_inventory()) == 10


def test_inventory_get_cash():
    assert get_cash(make_inventory()) == pytest.approx(900000.0)


def test_inventory_get_average_cost():
    assert get_average_cost(make_inventory()) == pytest.approx(10000.0)


def test_inventory_get_realized_pnl():
    assert get_realized_pnl(make_inventory()) == pytest.approx(0.0)


def test_inventory_get_unrealized_pnl():
    assert get_unrealized_pnl(make_inventory(), 10100) == pytest.approx(1000.0)


def test_inventory_get_portfolio_value():
    assert get_portfolio_value(make_inventory(), 10100) == pytest.approx(1001000.0)


def test_inventory_get_exposure():
    assert get_inventory_exposure(make_inventory(), 10100) == pytest.approx(101000.0)


def test_inventory_summary():
    summary = summarize_inventory(make_inventory(), 10100)

    assert summary["position"] == 10
    assert summary["cash"] == pytest.approx(900000.0)
    assert summary["average_cost"] == pytest.approx(10000.0)
    assert summary["realized_pnl"] == pytest.approx(0.0)
    assert summary["unrealized_pnl"] == pytest.approx(1000.0)
    assert summary["portfolio_value"] == pytest.approx(1001000.0)
    assert summary["inventory_exposure"] == pytest.approx(101000.0)


# ============================================================
# Market-State Comparison
# ============================================================

def test_bid_depth_change(previous_snapshot, current_snapshot):
    assert bid_depth_change(current_snapshot, previous_snapshot) == 60


def test_ask_depth_change(previous_snapshot, current_snapshot):
    assert ask_depth_change(current_snapshot, previous_snapshot) == -60


def test_summarize_market_impact(previous_snapshot, current_snapshot):
    summary = summarize_market_impact(current_snapshot, previous_snapshot)

    assert summary["midpoint_change"] == 15.0
    assert summary["midpoint_return"] == pytest.approx(0.0015)
    assert summary["spread_change"] == -10
    assert summary["bid_depth_change"] == 60
    assert summary["ask_depth_change"] == -60
    assert summary["imbalance_change"] == pytest.approx(0.6)