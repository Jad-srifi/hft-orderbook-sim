#pragma once

#include <types.hpp>
#include <variant>

enum class ItchMessageType {
    AddOrder,
    AddOrderMPID,
    OrderExecuted,
    OrderExecutedWithPrice,
    OrderCancel,
    OrderDelete,
    OrderReplace,
    StockDirectory,
    SystemEvent
};

enum class ParseError {
    UnknownMessageType,
    IncompleteMessage,
    InvalidMessageLength,
    MalformedMessage
};

struct AddOrderMessage {
    Timestamp timestamp;
    TrackingNumber tracking_number;
    OrderReferenceNumber order_reference;
    Side side;
    Shares shares;
    ItchPrice price;
    StockLocate stock_locate;
    StockSymbol stock_symbol;
};

struct ExecuteMessage {
    Timestamp timestamp;
    OrderReferenceNumber order_reference;
    Shares executed_shares;
    MatchNumber match_number;
    StockLocate stock_locate;
    TrackingNumber tracking_number;
};

struct CancelMessage {
    Timestamp timestamp;
    OrderReferenceNumber order_reference;
    Shares cancelled_shares;
    StockLocate stock_locate;
    TrackingNumber tracking_number;
};

struct DeleteMessage {
    Timestamp timestamp;
    OrderReferenceNumber order_reference;
    StockLocate stock_locate;
    TrackingNumber tracking_number;
};

struct ReplaceMessage {
    Timestamp timestamp;
    OrderReferenceNumber old_order_reference;
    OrderReferenceNumber new_order_reference;
    Shares new_shares;
    ItchPrice new_price;
    StockLocate stock_locate;
    TrackingNumber tracking_number;
};

struct ExecuteWithPriceMessage {
    Timestamp timestamp;
    OrderReferenceNumber order_reference;
    Shares executed_shares;
    MatchNumber match_number;
    ItchPrice execution_price;
    StockLocate stock_locate;
    TrackingNumber tracking_number;
    char printability;
};

struct AddOrderMPIDMessage {
    Timestamp timestamp;
    TrackingNumber tracking_number;
    OrderReferenceNumber order_reference;
    Side side;
    Shares shares;
    ItchPrice price;
    StockLocate stock_locate;
    StockSymbol stock_symbol;
    MPID mpid;
};

struct StockDirectoryMessage {
    StockLocate stock_locate;
    TrackingNumber tracking_number;
    Timestamp timestamp;
    StockSymbol stock_symbol;
    char market_category;
    char financial_status_indicator;
    Shares round_lot_size;
    char round_lots_only;
    char issue_clarification;
    std::array<char, 2> issue_sub_type;
    char authenticity;
    char short_sale_threshold;
    char IPO_flag;
    char LULD_reference_price_tier;
    char ETP_flag;
    std::uint32_t ETP_leverage_factor;
    char inverse_indicator;
};

struct SystemEventMessage {
    StockLocate stock_locate;
    TrackingNumber tracking_number;
    Timestamp timestamp;
    char event_code;
};

using ItchMessage = std::variant<
    AddOrderMessage,
    AddOrderMPIDMessage,
    ExecuteMessage,
    ExecuteWithPriceMessage,
    CancelMessage,
    DeleteMessage,
    ReplaceMessage,
    StockDirectoryMessage,
    SystemEventMessage
>;