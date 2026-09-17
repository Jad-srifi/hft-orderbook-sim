#pragma once

#include <trade.hpp>
#include <types.hpp>
#include <vector>
#include <unordered_map>

struct ExecutionResult {
    Quantity executed_quantity;

    ExecutionValue execution_value;

    Vwap execution_vwap;

    MidPrice reference_price;

    Slippage slippage;

    ExecutionCost execution_cost;

    Quantity liquidity_consumed;
};

using TradesByIncomingOrder = std::unordered_map<OrderId, std::vector<Trade>>;

using SidesByOrder = std::unordered_map<OrderId, Side>;

using ReferencePricesByOrder = std::unordered_map<OrderId, MidPrice>;

using ExecutionResultsByOrder = std::unordered_map<OrderId, ExecutionResult>;


TradesByIncomingOrder group_trades_by_incoming_order(const std::vector<Trade>& trades);

Quantity calculate_executed_quantity(std::vector<Trade> trades);

ExecutionValue calculate_executed_value(std::vector<Trade> trades);

Vwap calculate_VWAP(std::vector<Trade> trades);

Slippage calculate_slippage(std::vector<Trade> trades, Side side, MidPrice ref_price);

ExecutionCost calculate_execution_cost(std::vector<Trade> trades, Side side, MidPrice ref_price);

Quantity calculate_liquidity_consumed(std::vector<Trade> trades);

ExecutionResult calculate_execution_result(std::vector<Trade> trades, Side side, MidPrice ref_price);

ExecutionResultsByOrder calculate_execution_results_by_order(std::vector<Trade> trades, SidesByOrder sides, ReferencePricesByOrder ref_prices);