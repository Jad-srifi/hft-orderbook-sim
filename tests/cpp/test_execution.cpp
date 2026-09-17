#include <execution.hpp>
#include <iostream>
#include <cassert>
#include <cmath>

void test_empty_trades() {
    std::vector<Trade> trades;

    assert(calculate_executed_quantity(trades) == 0);
    assert(calculate_executed_value(trades) == 0);
    assert(calculate_VWAP(trades) == 0.0);
    assert(calculate_slippage(trades, Side::BUY, 10000.0) == 0.0);
    assert(calculate_execution_cost(trades, Side::BUY, 10000.0) == 0);
    assert(calculate_liquidity_consumed(trades) == 0);
}

void test_single_trade() {
    std::vector<Trade> trades = {
        {1, 2, 10500, 50}
    };

    assert(calculate_executed_quantity(trades) == 50);
    assert(calculate_executed_value(trades) == 525000);
    assert(calculate_VWAP(trades) == 10500.0);
    assert(calculate_liquidity_consumed(trades) == 50);
}

void test_multiple_trades() {
    std::vector<Trade> trades = {
        {1, 2, 10500, 50},
        {1, 3, 10600, 70}
    };

    assert(calculate_executed_quantity(trades) == 120);
    assert(calculate_executed_value(trades) == 1267000);

    double expected_vwap = 1267000.0 / 120.0;

    assert(std::abs(calculate_VWAP(trades) - expected_vwap) < 1e-9);
    assert(calculate_liquidity_consumed(trades) == 120);
}

void test_buy_slippage() {
    std::vector<Trade> trades = {
        {1, 2, 10500, 50},
        {1, 3, 10600, 70}
    };

    double reference_price = 10250.0;

    double expected_vwap = 1267000.0 / 120.0;
    double expected_slippage = expected_vwap - reference_price;

    assert(
        std::abs(
            calculate_slippage(
                trades,
                Side::BUY,
                reference_price
            ) - expected_slippage
        ) < 1e-9
    );
}

void test_sell_slippage() {
    std::vector<Trade> trades = {
        {2, 4, 10000, 80}
    };

    double reference_price = 10300.0;

    double expected_slippage = reference_price - 10000.0;

    assert(
        std::abs(
            calculate_slippage(
                trades,
                Side::SELL,
                reference_price
            ) - expected_slippage
        ) < 1e-9
    );
}

void test_execution_cost() {
    std::vector<Trade> buy_trades = {
        {1, 2, 10500, 50},
        {1, 3, 10600, 70}
    };

    double buy_reference = 10250.0;

    assert(
        calculate_execution_cost(
            buy_trades,
            Side::BUY,
            buy_reference
        ) == 37000
    );

    std::vector<Trade> sell_trades = {
        {2, 4, 10000, 80}
    };

    double sell_reference = 10300.0;

    assert(
        calculate_execution_cost(
            sell_trades,
            Side::SELL,
            sell_reference
        ) == 24000
    );
}

void test_execution_result() {
    std::vector<Trade> trades = {
        {1, 2, 10500, 50},
        {1, 3, 10600, 70}
    };

    ExecutionResult result =
        calculate_execution_result(
            trades,
            Side::BUY,
            10250.0
        );

    assert(result.executed_quantity == 120);
    assert(result.execution_value == 1267000);
    assert(
        std::abs(result.execution_vwap - (1267000.0 / 120.0)) < 1e-9
    );
    assert(result.reference_price == 10250.0);
    assert(
        std::abs(result.slippage - (1267000.0 / 120.0 - 10250.0)) < 1e-9
    );
    assert(result.execution_cost == 37000);
    assert(result.liquidity_consumed == 120);
}

void test_grouping() {
    std::vector<Trade> trades = {
        {1, 2, 10500, 50},
        {1, 3, 10600, 70},
        {2, 4, 10000, 80}
    };

    TradesByIncomingOrder grouped =
        group_trades_by_incoming_order(trades);

    assert(grouped.size() == 2);

    assert(grouped.at(1).size() == 2);
    assert(grouped.at(2).size() == 1);

    assert(grouped.at(1)[0].resting_order == 2);
    assert(grouped.at(1)[1].resting_order == 3);
    assert(grouped.at(2)[0].resting_order == 4);
}

void test_multiple_order_results() {
    std::vector<Trade> trades = {
        {105, 104, 10500, 50},
        {105, 103, 10600, 70},
        {106, 101, 10000, 80}
    };

    SidesByOrder sides = {
        {105, Side::BUY},
        {106, Side::SELL}
    };

    ReferencePricesByOrder references = {
        {105, 10250.0},
        {106, 10300.0}
    };

    ExecutionResultsByOrder results =
        calculate_execution_results_by_order(
            trades,
            sides,
            references
        );

    assert(results.size() == 2);

    const ExecutionResult& buy_result = results.at(105);

    assert(buy_result.executed_quantity == 120);
    assert(buy_result.execution_value == 1267000);
    assert(buy_result.execution_cost == 37000);

    const ExecutionResult& sell_result = results.at(106);

    assert(sell_result.executed_quantity == 80);
    assert(sell_result.execution_value == 800000);
    assert(sell_result.execution_vwap == 10000.0);
    assert(sell_result.slippage == 300.0);
    assert(sell_result.execution_cost == 24000);
}

void test_orders_with_no_trades_are_excluded() {
    std::vector<Trade> trades = {
        {105, 104, 10500, 50}
    };

    SidesByOrder sides = {
        {105, Side::BUY},
        {106, Side::SELL}
    };

    ReferencePricesByOrder references = {
        {105, 10250.0},
        {106, 10300.0}
    };

    ExecutionResultsByOrder results =
        calculate_execution_results_by_order(
            trades,
            sides,
            references
        );

    assert(results.size() == 1);
    assert(results.find(105) != results.end());
    assert(results.find(106) == results.end());
}

int main() {

    test_empty_trades();
    test_single_trade();
    test_multiple_trades();
    test_buy_slippage();
    test_sell_slippage();
    test_execution_cost();
    test_execution_result();
    test_grouping();
    test_multiple_order_results();
    test_orders_with_no_trades_are_excluded();

    std::cout << "All Chapter 7 execution tests passed.\n";

    return 0;
}