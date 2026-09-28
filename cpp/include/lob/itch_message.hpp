#pragma once

#include <array>
#include <types.hpp>
#include <variant>

enum class ItchMessageType
{
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

enum class ParseError
{
    UnknownMessageType,
    IncompleteMessage,
    InvalidMessageLength,
    MalformedMessage
};

struct AddOrderMessage
{
    Timestamp timestamp;
    TrackingNumber tracking_number;
    OrderReferenceNumber order_reference;
    Side side;
    Shares shares;
    ItchPrice price;
    StockLocate stock_locate;
    StockSymbol stock_symbol;
};

struct ExecuteMessage
{
    Timestamp timestamp;
    OrderReferenceNumber order_reference;
    Shares executed_shares;
    MatchNumber match_number;
    StockLocate stock_locate;
    TrackingNumber tracking_number;
};

struct CancelMessage
{
    Timestamp timestamp;
    OrderReferenceNumber order_reference;
    Shares cancelled_shares;
    StockLocate stock_locate;
    TrackingNumber tracking_number;
};

struct DeleteMessage
{
    Timestamp timestamp;
    OrderReferenceNumber order_reference;
    StockLocate stock_locate;
    TrackingNumber tracking_number;
};

struct ReplaceMessage
{
    Timestamp timestamp;
    OrderReferenceNumber old_order_reference;
    OrderReferenceNumber new_order_reference;
    Shares new_shares;
    ItchPrice new_price;
    StockLocate stock_locate;
    TrackingNumber tracking_number;
};

struct ExecuteWithPriceMessage
{
    Timestamp timestamp;
    OrderReferenceNumber order_reference;
    Shares executed_shares;
    MatchNumber match_number;
    ItchPrice execution_price;
    StockLocate stock_locate;
    TrackingNumber tracking_number;
    char printability;
};

struct AddOrderMPIDMessage
{
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

struct StockDirectoryMessage
{
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

struct SystemEventMessage
{
    StockLocate stock_locate;
    TrackingNumber tracking_number;
    Timestamp timestamp;
    char event_code;
};

struct StockTradingActionMessage
{
    StockLocate stock_locate;
    TrackingNumber tracking_number;
    Timestamp timestamp;
    StockSymbol stock_symbol;

    char trading_state;
    char reserved;

    std::array<char, 4> reason;
};

struct RegSHOMessage
{
    StockLocate stock_locate;
    TrackingNumber tracking_number;
    Timestamp timestamp;
    StockSymbol stock_symbol;

    char reg_sho_action;
};

struct MarketParticipantPositionMessage
{
    StockLocate stock_locate;
    TrackingNumber tracking_number;
    Timestamp timestamp;
    MPID mpid;
    StockSymbol stock_symbol;

    char primary_market_maker;
    char market_maker_mode;
    char market_participant_state;
};

struct MWCBDeclineLevelMessage
{
    StockLocate stock_locate;
    TrackingNumber tracking_number;
    Timestamp timestamp;

    std::uint64_t level_1;
    std::uint64_t level_2;
    std::uint64_t level_3;
};

struct MWCBStatusMessage
{
    StockLocate stock_locate;
    TrackingNumber tracking_number;
    Timestamp timestamp;

    char breached_level;
};

struct QuotingPeriodUpdateMessage
{
    StockLocate stock_locate;
    TrackingNumber tracking_number;
    Timestamp timestamp;
    StockSymbol stock_symbol;

    std::uint32_t ipo_quotation_release_time;
    char ipo_quotation_release_qualifier;
    ItchPrice ipo_price;
};

struct LULDAuctionCollarMessage
{
    StockLocate stock_locate;
    TrackingNumber tracking_number;
    Timestamp timestamp;
    StockSymbol stock_symbol;

    ItchPrice auction_collar_reference_price;
    ItchPrice upper_auction_collar_price;
    ItchPrice lower_auction_collar_price;

    std::uint32_t auction_collar_extension;
};

struct OperationalHaltMessage
{
    StockLocate stock_locate;
    TrackingNumber tracking_number;
    Timestamp timestamp;
    StockSymbol stock_symbol;

    char market_code;
    char operational_halt_action;
};

struct TradeMessage
{
    StockLocate stock_locate;
    TrackingNumber tracking_number;
    Timestamp timestamp;

    OrderReferenceNumber order_reference;
    Side side;
    Shares shares;
    StockSymbol stock_symbol;
    ItchPrice price;
    MatchNumber match_number;
};

struct CrossTradeMessage
{
    StockLocate stock_locate;
    TrackingNumber tracking_number;
    Timestamp timestamp;

    Shares shares;
    StockSymbol stock_symbol;
    ItchPrice cross_price;
    MatchNumber match_number;

    char cross_type;
};

struct BrokenTradeMessage
{
    StockLocate stock_locate;
    TrackingNumber tracking_number;
    Timestamp timestamp;

    MatchNumber match_number;
};

struct NOIIMessage
{
    StockLocate stock_locate;
    TrackingNumber tracking_number;
    Timestamp timestamp;

    std::uint64_t paired_shares;
    std::uint64_t imbalance_shares;

    char imbalance_direction;

    StockSymbol stock_symbol;

    ItchPrice far_price;
    ItchPrice near_price;
    ItchPrice current_reference_price;

    char cross_type;
    char price_variation_indicator;
};

struct RetailInterestMessage
{
    StockLocate stock_locate;
    TrackingNumber tracking_number;
    Timestamp timestamp;
    StockSymbol stock_symbol;

    char interest_flag;
};

struct DirectListingCapitalRaiseMessage
{
    StockLocate stock_locate;
    TrackingNumber tracking_number;
    Timestamp timestamp;
    StockSymbol stock_symbol;

    char open_eligibility_status;

    ItchPrice minimum_allowable_price;
    ItchPrice maximum_allowable_price;
    ItchPrice near_execution_price;

    Timestamp near_execution_time;

    ItchPrice lower_price_range_collar;
    ItchPrice upper_price_range_collar;
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
    SystemEventMessage,
    StockTradingActionMessage,
    RegSHOMessage,
    MarketParticipantPositionMessage,
    MWCBDeclineLevelMessage,
    MWCBStatusMessage,
    QuotingPeriodUpdateMessage,
    LULDAuctionCollarMessage,
    OperationalHaltMessage,
    TradeMessage,
    CrossTradeMessage,
    BrokenTradeMessage,
    NOIIMessage,
    RetailInterestMessage,
    DirectListingCapitalRaiseMessage>;