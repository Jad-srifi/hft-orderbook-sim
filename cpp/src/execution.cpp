#include <execution.hpp>
#include <order_book.hpp>
#include <metrics.hpp>

TradesByIncomingOrder group_trades_by_incoming_order(const std::vector<Trade>& trades) {
    TradesByIncomingOrder grouped;

    for (const Trade& trade: trades) {
        grouped[trade.incoming_order].push_back(trade);
    }

    return grouped;
}

ExecutionValue calculate_executed_value(std::vector<Trade> trades) {
    ExecutionValue total_value = 0;

    if (trades.empty()) {
        return total_value;
    }

    for (const Trade& trade: trades) {
        total_value += (trade.price * trade.quantity);
    }

    return total_value;
}

Quantity calculate_executed_quantity(std::vector<Trade> trades) {
    Quantity total_executed_quantity = 0;

    if (trades.empty()) {
        return total_executed_quantity;
    }

    for (const Trade& trade: trades) {
        total_executed_quantity += trade.quantity;
    } 

    return total_executed_quantity;
}

Vwap calculate_VWAP(std::vector<Trade> trades) {
    Quantity total_executed_quantity = calculate_executed_quantity(trades);
    ExecutionValue total_value = calculate_executed_value(trades);

    if (total_executed_quantity == 0 || total_value == 0) {
        return 0;
    }

    return static_cast<Vwap>(static_cast<double>(total_value) / static_cast<double>(total_executed_quantity));
}

Slippage calculate_slippage(std::vector<Trade> trades, Side side, MidPrice ref_price) {
    Vwap vwap = calculate_VWAP(trades);
    Slippage Slippage = 0;

    if (trades.empty()) {
        return 0.0;
    }

    if (side == Side::BUY) {
        Slippage = vwap - ref_price;
    }

    else if (side == Side::SELL) {
        Slippage = ref_price - vwap;
    }

    return Slippage;
}

ExecutionCost calculate_execution_cost(std::vector<Trade> trades, Side side, MidPrice ref_price) {
    Slippage slippage = calculate_slippage(trades, side, ref_price);
    Quantity executed_quantity = calculate_executed_quantity(trades);

    if (executed_quantity == 0) {
        return 0;
    }

    return slippage * executed_quantity;
}

Quantity calculate_liquidity_consumed(std::vector<Trade> trades) {
    Quantity consumed_liquidity = 0;

    if (trades.empty()) {
        return consumed_liquidity;
    }

    for (const Trade& trade: trades) {
        consumed_liquidity += trade.quantity;
    }

    return consumed_liquidity;
}

ExecutionResult calculate_execution_result(std::vector<Trade> trades, Side side, MidPrice ref_price) {
    ExecutionResult result;

    result.executed_quantity = calculate_executed_quantity(trades);
    result.execution_value = calculate_executed_value(trades);
    result.execution_vwap = calculate_VWAP(trades);
    result.reference_price = ref_price;
    result.slippage = calculate_slippage(trades, side, ref_price);
    result.execution_cost = calculate_execution_cost(trades, side, ref_price);
    result.liquidity_consumed = calculate_liquidity_consumed(trades);

    return result;
}

ExecutionResultsByOrder calculate_execution_results_by_order(std::vector<Trade> trades, SidesByOrder sides, ReferencePricesByOrder ref_prices) {
    TradesByIncomingOrder grouped = group_trades_by_incoming_order(trades);

    ExecutionResultsByOrder grouped_execution_results;

    for (const auto& [order_id, group_trades] : grouped) {
        auto order_side = sides.at(order_id);
        auto order_reference = ref_prices.at(order_id);

        grouped_execution_results[order_id] = calculate_execution_result(group_trades, order_side, order_reference);
    }

    return grouped_execution_results;
}
