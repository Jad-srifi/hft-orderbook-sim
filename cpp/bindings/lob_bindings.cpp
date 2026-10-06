#include <pybind11/pybind11.h>
#include <pybind11/stl.h>

#include "types.hpp"
#include "order.hpp"
#include "trade.hpp"
#include "event.hpp"
#include "metrics.hpp"
#include "execution.hpp"
#include "inventory_model.hpp"
#include "price_level.hpp"
#include "order_book.hpp"
#include "simulator.hpp"
#include "itch_message.hpp"
#include "itch_parser.hpp"
#include "itch_mapper.hpp"
#include "itch_replay.hpp"
#include "itch_file_reader.hpp"

namespace py = pybind11;

PYBIND11_MODULE(lob_cpp, m)
{
    // types.hpp

    py::enum_<Side>(m, "Side")
        .value("BUY", Side::BUY)
        .value("SELL", Side::SELL)
        .value("NONE", Side::NONE);

    // order.hpp

    py::class_<Order>(m, "Order")
        .def(py::init<OrderId, Side, Price, Quantity>())
        .def_readwrite("id", &Order::id)
        .def_readwrite("side", &Order::side)
        .def_readwrite("price", &Order::price)
        .def_readwrite("quantity", &Order::quantity);

    // trade.hpp

    py::class_<Trade>(m, "Trade")
        .def(py::init<OrderId, OrderId, Price, Quantity>())
        .def_readwrite("incoming_order", &Trade::incoming_order)
        .def_readwrite("resting_order", &Trade::resting_order)
        .def_readwrite("price", &Trade::price)
        .def_readwrite("quantity", &Trade::quantity);

    // event.hpp

    py::enum_<EventType>(m, "EventType")
        .value("ADD", EventType::ADD)
        .value("CANCEL", EventType::CANCEL)
        .value("MODIFY", EventType::MODIFY);

    py::class_<Event>(m, "Event")
        .def(py::init<EventType, Timestamp, SequenceNumber, Order, OrderId, Price, Quantity>())
        .def_readwrite("type", &Event::type)
        .def_readwrite("timestamp", &Event::timestamp)
        .def_readwrite("sequence", &Event::sequence)
        .def_readwrite("order", &Event::order)
        .def_readwrite("order_id", &Event::order_id)
        .def_readwrite("new_price", &Event::new_price)
        .def_readwrite("new_quantity", &Event::new_quantity);

    // price_level.hpp

    py::class_<PriceLevel>(m, "PriceLevel")
        .def_readonly("price", &PriceLevel::price)
        .def_readonly("orders", &PriceLevel::orders)
        .def_readonly("total_quantity", &PriceLevel::total_quantity);

    // order_book.hpp

    py::class_<OrderBook>(m, "OrderBook")
        .def(py::init<>())
        .def("add", &OrderBook::add)
        .def("cancel", &OrderBook::cancel)
        .def("best_bid", &OrderBook::best_bid)
        .def("best_ask", &OrderBook::best_ask)
        .def("spread", &OrderBook::spread)
        .def("process_order", &OrderBook::process_order)
        .def("modify", &OrderBook::modify)
        .def(
            "bid_levels",
            &OrderBook::bid_levels,
            py::return_value_policy::reference_internal)
        .def(
            "ask_levels",
            &OrderBook::ask_levels,
            py::return_value_policy::reference_internal);

    // simulator.hpp

    py::class_<Simulator>(m, "Simulator")
        .def(py::init<Cash>())
        .def("add_event", &Simulator::add_event)
        .def("process_event", &Simulator::process_event)
        .def("process_events", &Simulator::process_events)
        .def("calculate_execution_results", &Simulator::calculate_execution_results)
        .def(
            "get_inventory_model",
            &Simulator::get_inventory_model,
            py::return_value_policy::reference_internal);

    // metrics.hpp

    py::class_<MarketMetrics>(m, "MarketMetrics")
        .def_readonly("best_bid", &MarketMetrics::best_bid)
        .def_readonly("best_ask", &MarketMetrics::best_ask)
        .def_readonly("mid_price", &MarketMetrics::mid_price)
        .def_readonly("spread", &MarketMetrics::spread)
        .def_readonly("relative_spread", &MarketMetrics::relative_spread)
        .def_readonly("bid_depth", &MarketMetrics::bid_depth)
        .def_readonly("ask_depth", &MarketMetrics::ask_depth)
        .def_readonly("imbalance", &MarketMetrics::imbalance)
        .def_readonly("trade_count", &MarketMetrics::trade_count)
        .def_readonly("trade_volume", &MarketMetrics::trade_volume);

    m.def("calculate_best_bid", &calculate_best_bid);
    m.def("calculate_best_ask", &calculate_best_ask);
    m.def("calculate_mid_price", &calculate_mid_price);
    m.def("calculate_spread", &calculate_spread);
    m.def("calculate_relative_spread", &calculate_relative_spread);
    m.def("calculate_bid_depth", &calculate_bid_depth);
    m.def("calculate_ask_depth", &calculate_ask_depth);
    m.def("calculate_imbalance", &calculate_imbalance);
    m.def("calculate_trade_count", &calculate_trade_count);
    m.def("calculate_trade_volume", &calculate_trade_volume);
    m.def("calculate_metrics", &calculate_metrics);

    // execution.hpp

    py::class_<ExecutionResult>(m, "ExecutionResult")
        .def_readonly("executed_quantity", &ExecutionResult::executed_quantity)
        .def_readonly("execution_value", &ExecutionResult::execution_value)
        .def_readonly("execution_vwap", &ExecutionResult::execution_vwap)
        .def_readonly("reference_price", &ExecutionResult::reference_price)
        .def_readonly("slippage", &ExecutionResult::slippage)
        .def_readonly("execution_cost", &ExecutionResult::execution_cost)
        .def_readonly("liquidity_consumed", &ExecutionResult::liquidity_consumed);

    m.def("group_trades_by_incoming_order", &group_trades_by_incoming_order);
    m.def("calculate_executed_quantity", &calculate_executed_quantity);
    m.def("calculate_executed_value", &calculate_executed_value);
    m.def("calculate_VWAP", &calculate_VWAP);
    m.def("calculate_slippage", &calculate_slippage);
    m.def("calculate_execution_cost", &calculate_execution_cost);
    m.def("calculate_liquidity_consumed", &calculate_liquidity_consumed);
    m.def("calculate_execution_result", &calculate_execution_result);
    m.def("calculate_execution_results_by_order", &calculate_execution_results_by_order);

    // inventory_model.hpp

    py::class_<InventoryModel>(m, "InventoryModel")
        .def(py::init<Cash>())
        .def("process_trade", &InventoryModel::process_trade)
        .def("get_position", &InventoryModel::get_position)
        .def("get_cash", &InventoryModel::get_cash)
        .def("get_avg_cost", &InventoryModel::get_avg_cost)
        .def("get_realized_pnl", &InventoryModel::get_realized_pnl)
        .def("unrealized_pnl", &InventoryModel::unrealized_pnl)
        .def("portfolio_value", &InventoryModel::portfolio_value)
        .def("inventory_exposure", &InventoryModel::inventory_exposure);

    // itch_message.hpp

    py::enum_<ItchMessageType>(m, "ItchMessageType")
        .value("AddOrder", ItchMessageType::AddOrder)
        .value("AddOrderMPID", ItchMessageType::AddOrderMPID)
        .value("OrderExecuted", ItchMessageType::OrderExecuted)
        .value("OrderExecutedWithPrice", ItchMessageType::OrderExecutedWithPrice)
        .value("OrderCancel", ItchMessageType::OrderCancel)
        .value("OrderDelete", ItchMessageType::OrderDelete)
        .value("OrderReplace", ItchMessageType::OrderReplace)
        .value("StockDirectory", ItchMessageType::StockDirectory)
        .value("SystemEvent", ItchMessageType::SystemEvent);

    py::enum_<ParseError>(m, "ParseError")
        .value("UnknownMessageType", ParseError::UnknownMessageType)
        .value("IncompleteMessage", ParseError::IncompleteMessage)
        .value("InvalidMessageLength", ParseError::InvalidMessageLength)
        .value("MalformedMessage", ParseError::MalformedMessage);

    // itch_message.hpp - AddOrderMessage

    py::class_<AddOrderMessage>(m, "AddOrderMessage")
        .def(py::init<>())
        .def_readwrite("timestamp", &AddOrderMessage::timestamp)
        .def_readwrite("tracking_number", &AddOrderMessage::tracking_number)
        .def_readwrite("order_reference", &AddOrderMessage::order_reference)
        .def_readwrite("side", &AddOrderMessage::side)
        .def_readwrite("shares", &AddOrderMessage::shares)
        .def_readwrite("price", &AddOrderMessage::price)
        .def_readwrite("stock_locate", &AddOrderMessage::stock_locate)
        .def_readwrite("stock_symbol", &AddOrderMessage::stock_symbol);

    // itch_message.hpp - AddOrderMPIDMessage

    py::class_<AddOrderMPIDMessage>(m, "AddOrderMPIDMessage")
        .def(py::init<>())
        .def_readwrite("timestamp", &AddOrderMPIDMessage::timestamp)
        .def_readwrite("tracking_number", &AddOrderMPIDMessage::tracking_number)
        .def_readwrite("order_reference", &AddOrderMPIDMessage::order_reference)
        .def_readwrite("side", &AddOrderMPIDMessage::side)
        .def_readwrite("shares", &AddOrderMPIDMessage::shares)
        .def_readwrite("price", &AddOrderMPIDMessage::price)
        .def_readwrite("stock_locate", &AddOrderMPIDMessage::stock_locate)
        .def_readwrite("stock_symbol", &AddOrderMPIDMessage::stock_symbol)
        .def_readwrite("mpid", &AddOrderMPIDMessage::mpid);

    // itch_message.hpp - ExecuteMessage

    py::class_<ExecuteMessage>(m, "ExecuteMessage")
        .def(py::init<>())
        .def_readwrite("timestamp", &ExecuteMessage::timestamp)
        .def_readwrite("order_reference", &ExecuteMessage::order_reference)
        .def_readwrite("executed_shares", &ExecuteMessage::executed_shares)
        .def_readwrite("match_number", &ExecuteMessage::match_number)
        .def_readwrite("stock_locate", &ExecuteMessage::stock_locate)
        .def_readwrite("tracking_number", &ExecuteMessage::tracking_number);

    // itch_message.hpp - ExecuteWithPriceMessage

    py::class_<ExecuteWithPriceMessage>(m, "ExecuteWithPriceMessage")
        .def(py::init<>())
        .def_readwrite("timestamp", &ExecuteWithPriceMessage::timestamp)
        .def_readwrite("order_reference", &ExecuteWithPriceMessage::order_reference)
        .def_readwrite("executed_shares", &ExecuteWithPriceMessage::executed_shares)
        .def_readwrite("match_number", &ExecuteWithPriceMessage::match_number)
        .def_readwrite("execution_price", &ExecuteWithPriceMessage::execution_price)
        .def_readwrite("stock_locate", &ExecuteWithPriceMessage::stock_locate)
        .def_readwrite("tracking_number", &ExecuteWithPriceMessage::tracking_number)
        .def_readwrite("printability", &ExecuteWithPriceMessage::printability);

    // itch_message.hpp - CancelMessage

    py::class_<CancelMessage>(m, "CancelMessage")
        .def(py::init<>())
        .def_readwrite("timestamp", &CancelMessage::timestamp)
        .def_readwrite("order_reference", &CancelMessage::order_reference)
        .def_readwrite("cancelled_shares", &CancelMessage::cancelled_shares)
        .def_readwrite("stock_locate", &CancelMessage::stock_locate)
        .def_readwrite("tracking_number", &CancelMessage::tracking_number);

    // itch_message.hpp - DeleteMessage

    py::class_<DeleteMessage>(m, "DeleteMessage")
        .def(py::init<>())
        .def_readwrite("timestamp", &DeleteMessage::timestamp)
        .def_readwrite("order_reference", &DeleteMessage::order_reference)
        .def_readwrite("stock_locate", &DeleteMessage::stock_locate)
        .def_readwrite("tracking_number", &DeleteMessage::tracking_number);

    // itch_message.hpp - ReplaceMessage

    py::class_<ReplaceMessage>(m, "ReplaceMessage")
        .def(py::init<>())
        .def_readwrite("timestamp", &ReplaceMessage::timestamp)
        .def_readwrite("old_order_reference", &ReplaceMessage::old_order_reference)
        .def_readwrite("new_order_reference", &ReplaceMessage::new_order_reference)
        .def_readwrite("new_shares", &ReplaceMessage::new_shares)
        .def_readwrite("new_price", &ReplaceMessage::new_price)
        .def_readwrite("stock_locate", &ReplaceMessage::stock_locate)
        .def_readwrite("tracking_number", &ReplaceMessage::tracking_number);

    // itch_message.hpp - StockDirectoryMessage

    py::class_<StockDirectoryMessage>(m, "StockDirectoryMessage")
        .def(py::init<>())
        .def_readwrite("stock_locate", &StockDirectoryMessage::stock_locate)
        .def_readwrite("tracking_number", &StockDirectoryMessage::tracking_number)
        .def_readwrite("timestamp", &StockDirectoryMessage::timestamp)
        .def_readwrite("stock_symbol", &StockDirectoryMessage::stock_symbol)
        .def_readwrite("market_category", &StockDirectoryMessage::market_category)
        .def_readwrite("financial_status_indicator", &StockDirectoryMessage::financial_status_indicator)
        .def_readwrite("round_lot_size", &StockDirectoryMessage::round_lot_size)
        .def_readwrite("round_lots_only", &StockDirectoryMessage::round_lots_only)
        .def_readwrite("issue_clarification", &StockDirectoryMessage::issue_clarification)
        .def_readwrite("issue_sub_type", &StockDirectoryMessage::issue_sub_type)
        .def_readwrite("authenticity", &StockDirectoryMessage::authenticity)
        .def_readwrite("short_sale_threshold", &StockDirectoryMessage::short_sale_threshold)
        .def_readwrite("IPO_flag", &StockDirectoryMessage::IPO_flag)
        .def_readwrite("LULD_reference_price_tier", &StockDirectoryMessage::LULD_reference_price_tier)
        .def_readwrite("ETP_flag", &StockDirectoryMessage::ETP_flag)
        .def_readwrite("ETP_leverage_factor", &StockDirectoryMessage::ETP_leverage_factor)
        .def_readwrite("inverse_indicator", &StockDirectoryMessage::inverse_indicator);

    // itch_message.hpp - SystemEventMessage

    py::class_<SystemEventMessage>(m, "SystemEventMessage")
        .def(py::init<>())
        .def_readwrite("stock_locate", &SystemEventMessage::stock_locate)
        .def_readwrite("tracking_number", &SystemEventMessage::tracking_number)
        .def_readwrite("timestamp", &SystemEventMessage::timestamp)
        .def_readwrite("event_code", &SystemEventMessage::event_code);

    // itch_message.hpp - StockTradingActionMessage

    py::class_<StockTradingActionMessage>(m, "StockTradingActionMessage")
        .def(py::init<>())
        .def_readwrite("stock_locate", &StockTradingActionMessage::stock_locate)
        .def_readwrite("tracking_number", &StockTradingActionMessage::tracking_number)
        .def_readwrite("timestamp", &StockTradingActionMessage::timestamp)
        .def_readwrite("stock_symbol", &StockTradingActionMessage::stock_symbol)
        .def_readwrite("trading_state", &StockTradingActionMessage::trading_state)
        .def_readwrite("reserved", &StockTradingActionMessage::reserved)
        .def_readwrite("reason", &StockTradingActionMessage::reason);

    // itch_message.hpp - RegSHOMessage

    py::class_<RegSHOMessage>(m, "RegSHOMessage")
        .def(py::init<>())
        .def_readwrite("stock_locate", &RegSHOMessage::stock_locate)
        .def_readwrite("tracking_number", &RegSHOMessage::tracking_number)
        .def_readwrite("timestamp", &RegSHOMessage::timestamp)
        .def_readwrite("stock_symbol", &RegSHOMessage::stock_symbol)
        .def_readwrite("reg_sho_action", &RegSHOMessage::reg_sho_action);

    // itch_message.hpp - MarketParticipantPositionMessage

    py::class_<MarketParticipantPositionMessage>(m, "MarketParticipantPositionMessage")
        .def(py::init<>())
        .def_readwrite("stock_locate", &MarketParticipantPositionMessage::stock_locate)
        .def_readwrite("tracking_number", &MarketParticipantPositionMessage::tracking_number)
        .def_readwrite("timestamp", &MarketParticipantPositionMessage::timestamp)
        .def_readwrite("mpid", &MarketParticipantPositionMessage::mpid)
        .def_readwrite("stock_symbol", &MarketParticipantPositionMessage::stock_symbol)
        .def_readwrite("primary_market_maker", &MarketParticipantPositionMessage::primary_market_maker)
        .def_readwrite("market_maker_mode", &MarketParticipantPositionMessage::market_maker_mode)
        .def_readwrite("market_participant_state", &MarketParticipantPositionMessage::market_participant_state);

    // itch_message.hpp - MWCBDeclineLevelMessage

    py::class_<MWCBDeclineLevelMessage>(m, "MWCBDeclineLevelMessage")
        .def(py::init<>())
        .def_readwrite("stock_locate", &MWCBDeclineLevelMessage::stock_locate)
        .def_readwrite("tracking_number", &MWCBDeclineLevelMessage::tracking_number)
        .def_readwrite("timestamp", &MWCBDeclineLevelMessage::timestamp)
        .def_readwrite("level_1", &MWCBDeclineLevelMessage::level_1)
        .def_readwrite("level_2", &MWCBDeclineLevelMessage::level_2)
        .def_readwrite("level_3", &MWCBDeclineLevelMessage::level_3);

    // itch_message.hpp - MWCBStatusMessage

    py::class_<MWCBStatusMessage>(m, "MWCBStatusMessage")
        .def(py::init<>())
        .def_readwrite("stock_locate", &MWCBStatusMessage::stock_locate)
        .def_readwrite("tracking_number", &MWCBStatusMessage::tracking_number)
        .def_readwrite("timestamp", &MWCBStatusMessage::timestamp)
        .def_readwrite("breached_level", &MWCBStatusMessage::breached_level);

    // itch_message.hpp - QuotingPeriodUpdateMessage

    py::class_<QuotingPeriodUpdateMessage>(m, "QuotingPeriodUpdateMessage")
        .def(py::init<>())
        .def_readwrite("stock_locate", &QuotingPeriodUpdateMessage::stock_locate)
        .def_readwrite("tracking_number", &QuotingPeriodUpdateMessage::tracking_number)
        .def_readwrite("timestamp", &QuotingPeriodUpdateMessage::timestamp)
        .def_readwrite("stock_symbol", &QuotingPeriodUpdateMessage::stock_symbol)
        .def_readwrite("ipo_quotation_release_time", &QuotingPeriodUpdateMessage::ipo_quotation_release_time)
        .def_readwrite("ipo_quotation_release_qualifier", &QuotingPeriodUpdateMessage::ipo_quotation_release_qualifier)
        .def_readwrite("ipo_price", &QuotingPeriodUpdateMessage::ipo_price);

    // itch_message.hpp - LULDAuctionCollarMessage

    py::class_<LULDAuctionCollarMessage>(m, "LULDAuctionCollarMessage")
        .def(py::init<>())
        .def_readwrite("stock_locate", &LULDAuctionCollarMessage::stock_locate)
        .def_readwrite("tracking_number", &LULDAuctionCollarMessage::tracking_number)
        .def_readwrite("timestamp", &LULDAuctionCollarMessage::timestamp)
        .def_readwrite("stock_symbol", &LULDAuctionCollarMessage::stock_symbol)
        .def_readwrite("auction_collar_reference_price", &LULDAuctionCollarMessage::auction_collar_reference_price)
        .def_readwrite("upper_auction_collar_price", &LULDAuctionCollarMessage::upper_auction_collar_price)
        .def_readwrite("lower_auction_collar_price", &LULDAuctionCollarMessage::lower_auction_collar_price)
        .def_readwrite("auction_collar_extension", &LULDAuctionCollarMessage::auction_collar_extension);

    // itch_message.hpp - OperationalHaltMessage

    py::class_<OperationalHaltMessage>(m, "OperationalHaltMessage")
        .def(py::init<>())
        .def_readwrite("stock_locate", &OperationalHaltMessage::stock_locate)
        .def_readwrite("tracking_number", &OperationalHaltMessage::tracking_number)
        .def_readwrite("timestamp", &OperationalHaltMessage::timestamp)
        .def_readwrite("stock_symbol", &OperationalHaltMessage::stock_symbol)
        .def_readwrite("market_code", &OperationalHaltMessage::market_code)
        .def_readwrite("operational_halt_action", &OperationalHaltMessage::operational_halt_action);

    // itch_message.hpp - TradeMessage

    py::class_<TradeMessage>(m, "TradeMessage")
        .def(py::init<>())
        .def_readwrite("stock_locate", &TradeMessage::stock_locate)
        .def_readwrite("tracking_number", &TradeMessage::tracking_number)
        .def_readwrite("timestamp", &TradeMessage::timestamp)
        .def_readwrite("order_reference", &TradeMessage::order_reference)
        .def_readwrite("side", &TradeMessage::side)
        .def_readwrite("shares", &TradeMessage::shares)
        .def_readwrite("stock_symbol", &TradeMessage::stock_symbol)
        .def_readwrite("price", &TradeMessage::price)
        .def_readwrite("match_number", &TradeMessage::match_number);

    // itch_message.hpp - CrossTradeMessage

    py::class_<CrossTradeMessage>(m, "CrossTradeMessage")
        .def(py::init<>())
        .def_readwrite("stock_locate", &CrossTradeMessage::stock_locate)
        .def_readwrite("tracking_number", &CrossTradeMessage::tracking_number)
        .def_readwrite("timestamp", &CrossTradeMessage::timestamp)
        .def_readwrite("shares", &CrossTradeMessage::shares)
        .def_readwrite("stock_symbol", &CrossTradeMessage::stock_symbol)
        .def_readwrite("cross_price", &CrossTradeMessage::cross_price)
        .def_readwrite("match_number", &CrossTradeMessage::match_number)
        .def_readwrite("cross_type", &CrossTradeMessage::cross_type);

    // itch_message.hpp - BrokenTradeMessage

    py::class_<BrokenTradeMessage>(m, "BrokenTradeMessage")
        .def(py::init<>())
        .def_readwrite("stock_locate", &BrokenTradeMessage::stock_locate)
        .def_readwrite("tracking_number", &BrokenTradeMessage::tracking_number)
        .def_readwrite("timestamp", &BrokenTradeMessage::timestamp)
        .def_readwrite("match_number", &BrokenTradeMessage::match_number);

    // itch_message.hpp - NOIIMessage

    py::class_<NOIIMessage>(m, "NOIIMessage")
        .def(py::init<>())
        .def_readwrite("stock_locate", &NOIIMessage::stock_locate)
        .def_readwrite("tracking_number", &NOIIMessage::tracking_number)
        .def_readwrite("timestamp", &NOIIMessage::timestamp)
        .def_readwrite("paired_shares", &NOIIMessage::paired_shares)
        .def_readwrite("imbalance_shares", &NOIIMessage::imbalance_shares)
        .def_readwrite("imbalance_direction", &NOIIMessage::imbalance_direction)
        .def_readwrite("stock_symbol", &NOIIMessage::stock_symbol)
        .def_readwrite("far_price", &NOIIMessage::far_price)
        .def_readwrite("near_price", &NOIIMessage::near_price)
        .def_readwrite("current_reference_price", &NOIIMessage::current_reference_price)
        .def_readwrite("cross_type", &NOIIMessage::cross_type)
        .def_readwrite("price_variation_indicator", &NOIIMessage::price_variation_indicator);

    // itch_message.hpp - RetailInterestMessage

    py::class_<RetailInterestMessage>(m, "RetailInterestMessage")
        .def(py::init<>())
        .def_readwrite("stock_locate", &RetailInterestMessage::stock_locate)
        .def_readwrite("tracking_number", &RetailInterestMessage::tracking_number)
        .def_readwrite("timestamp", &RetailInterestMessage::timestamp)
        .def_readwrite("stock_symbol", &RetailInterestMessage::stock_symbol)
        .def_readwrite("interest_flag", &RetailInterestMessage::interest_flag);

    // itch_message.hpp - DirectListingCapitalRaiseMessage

    py::class_<DirectListingCapitalRaiseMessage>(m, "DirectListingCapitalRaiseMessage")
        .def(py::init<>())
        .def_readwrite("stock_locate", &DirectListingCapitalRaiseMessage::stock_locate)
        .def_readwrite("tracking_number", &DirectListingCapitalRaiseMessage::tracking_number)
        .def_readwrite("timestamp", &DirectListingCapitalRaiseMessage::timestamp)
        .def_readwrite("stock_symbol", &DirectListingCapitalRaiseMessage::stock_symbol)
        .def_readwrite("open_eligibility_status", &DirectListingCapitalRaiseMessage::open_eligibility_status)
        .def_readwrite("minimum_allowable_price", &DirectListingCapitalRaiseMessage::minimum_allowable_price)
        .def_readwrite("maximum_allowable_price", &DirectListingCapitalRaiseMessage::maximum_allowable_price)
        .def_readwrite("near_execution_price", &DirectListingCapitalRaiseMessage::near_execution_price)
        .def_readwrite("near_execution_time", &DirectListingCapitalRaiseMessage::near_execution_time)
        .def_readwrite("lower_price_range_collar", &DirectListingCapitalRaiseMessage::lower_price_range_collar)
        .def_readwrite("upper_price_range_collar", &DirectListingCapitalRaiseMessage::upper_price_range_collar);

    // itch_parser.hpp

    m.def("read_bytes", &read_bytes);
    m.def("read_be", &read_be);
    m.def("read_side", &read_side);

    m.def("get_type_parser", &get_type_parser);

    m.def("parse_A", &parse_A);
    m.def("parse_F", &parse_F);
    m.def("parse_E", &parse_E);
    m.def("parse_C", &parse_C);
    m.def("parse_X", &parse_X);
    m.def("parse_D", &parse_D);
    m.def("parse_U", &parse_U);
    m.def("parse_R", &parse_R);
    m.def("parse_S", &parse_S);

    m.def("parse_H", &parse_H);
    m.def("parse_Y", &parse_Y);
    m.def("parse_L", &parse_L);
    m.def("parse_V", &parse_V);
    m.def("parse_W", &parse_W);
    m.def("parse_K", &parse_K);
    m.def("parse_J", &parse_J);
    m.def("parse_h", &parse_h);
    m.def("parse_P", &parse_P);
    m.def("parse_Q", &parse_Q);
    m.def("parse_B", &parse_B);
    m.def("parse_I", &parse_I);
    m.def("parse_N", &parse_N);
    m.def("parse_O", &parse_O);

    // itch_mapper.hpp

    py::enum_<ReplayError>(m, "ReplayError")
        .value("UnknownOrder", UnknownOrder)
        .value("InvalidLifecycle", InvalidLifecycle)
        .value("InvalidQuantity", InvalidQuantity)
        .value("IgnoredMessage", IgnoredMessage);

    py::class_<AddOperation>(m, "AddOperation")
        .def(py::init<>())
        .def_readwrite("order_id", &AddOperation::order_id)
        .def_readwrite("side", &AddOperation::side)
        .def_readwrite("quantity", &AddOperation::quantity)
        .def_readwrite("price", &AddOperation::price);

    py::class_<ReduceOperation>(m, "ReduceOperation")
        .def(py::init<>())
        .def_readwrite("order_id", &ReduceOperation::order_id)
        .def_readwrite("quantity", &ReduceOperation::quantity);

    py::class_<RemoveOperation>(m, "RemoveOperation")
        .def(py::init<>())
        .def_readwrite("order_id", &RemoveOperation::order_id);

    py::class_<ReplaceOperation>(m, "ReplaceOperation")
        .def(py::init<>())
        .def_readwrite("old_order_id", &ReplaceOperation::old_order_id)
        .def_readwrite("new_order_id", &ReplaceOperation::new_order_id)
        .def_readwrite("side", &ReplaceOperation::side)
        .def_readwrite("quantity", &ReplaceOperation::quantity)
        .def_readwrite("price", &ReplaceOperation::price);

    py::class_<ItchMapper>(m, "ItchMapper")
        .def(py::init<StockLocate>())
        .def("is_selected_security", &ItchMapper::is_selected_security)
        .def("map", &ItchMapper::map);

    // itch_replay.hpp

    py::class_<ItchReplay>(m, "ItchReplay")
        .def(
            py::init<OrderBook &>(),
            py::keep_alive<1, 2>())
        .def("apply", &ItchReplay::apply);

    // itch_file_reader.hpp

    py::enum_<FileReaderError>(m, "FileReaderError")
        .value("CanNotOpenFile", FileReaderError::CanNotOpenFile)
        .value("IncompleteLengthField", FileReaderError::IncompleteLengthField)
        .value("TruncatedPayload", FileReaderError::TruncatedPayload)
        .value("ReadFailure", FileReaderError::ReadFailure);

    py::class_<EndOfFile>(m, "EndOfFile")
        .def(py::init<>());

    py::class_<ItchFileReader>(m, "ItchFileReader")
        .def(py::init<const std::string &>())
        .def("next_message", &ItchFileReader::next_message);
}