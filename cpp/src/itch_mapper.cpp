#include <itch_mapper.hpp>

#include <type_traits>
#include <profile.hpp>

ItchMapper::ItchMapper(StockLocate selected_stock_locate)
    : selected_stock_locate(selected_stock_locate)
{
}

bool ItchMapper::is_selected_security(StockLocate stock_locate)
{
    LOB_PROFILE_SCOPE("ItchMapper::is_selected_security");

    return stock_locate == this->selected_stock_locate;
}

std::variant<ReplayOperation, ReplayError> ItchMapper::map_add(const AddOrderMessage &message)
{
    LOB_PROFILE_SCOPE("ItchMapper::map_add");

    AddOperation operation{
        message.order_reference,
        message.side,
        message.shares,
        message.price};

    return operation;
}

std::variant<ReplayOperation, ReplayError> ItchMapper::map_add_mpid(const AddOrderMPIDMessage &message)
{
    LOB_PROFILE_SCOPE("ItchMapper::map_add_mpid");

    AddOperation operation{
        message.order_reference,
        message.side,
        message.shares,
        message.price};

    return operation;
}

std::variant<ReplayOperation, ReplayError> ItchMapper::map_execute(const ExecuteMessage &message)
{
    LOB_PROFILE_SCOPE("ItchMapper::map_execute");

    ReduceOperation operation{
        message.order_reference,
        message.executed_shares};

    return operation;
}

std::variant<ReplayOperation, ReplayError> ItchMapper::map_execute_with_price(const ExecuteWithPriceMessage &message)
{
    LOB_PROFILE_SCOPE("ItchMapper::map_execute_with_price");

    ReduceOperation operation{
        message.order_reference,
        message.executed_shares};

    return operation;
}

std::variant<ReplayOperation, ReplayError> ItchMapper::map_cancel(const CancelMessage &message)
{
    LOB_PROFILE_SCOPE("ItchMapper::map_cancel");

    ReduceOperation operation{
        message.order_reference,
        message.cancelled_shares};

    return operation;
}

std::variant<ReplayOperation, ReplayError> ItchMapper::map_delete(const DeleteMessage &message)
{
    LOB_PROFILE_SCOPE("ItchMapper::map_delete");

    RemoveOperation operation{
        message.order_reference};

    return operation;
}

std::variant<ReplayOperation, ReplayError> ItchMapper::map_replace(const ReplaceMessage &message)
{
    LOB_PROFILE_SCOPE("ItchMapper::map_replace");

    ReplaceOperation operation{
        message.old_order_reference,
        message.new_order_reference,
        Side::NONE,
        message.new_shares,
        message.new_price};

    return operation;
}

std::variant<ReplayOperation, ReplayError> ItchMapper::map(const ItchMessage &message)
{
    LOB_PROFILE_SCOPE("ItchMapper::map");

    return std::visit([this](const auto &msg) -> std::variant<ReplayOperation, ReplayError>
                      {
            using T = std::decay_t<decltype(msg)>;

            if constexpr (std::is_same_v<T, AddOrderMessage>) {
                if (!is_selected_security(msg.stock_locate))
                    return ReplayError::IgnoredMessage;
                return map_add(msg);
            }
            else if constexpr (std::is_same_v<T, AddOrderMPIDMessage>) {
                if (!is_selected_security(msg.stock_locate))
                    return ReplayError::IgnoredMessage;
                return map_add_mpid(msg);
            }
            else if constexpr (std::is_same_v<T, ExecuteMessage>) {
                if (!is_selected_security(msg.stock_locate))
                    return ReplayError::IgnoredMessage;
                return map_execute(msg);
            }
            else if constexpr (std::is_same_v<T, ExecuteWithPriceMessage>) {
                if (!is_selected_security(msg.stock_locate))
                    return ReplayError::IgnoredMessage;
                return map_execute_with_price(msg);
            }
            else if constexpr (std::is_same_v<T, CancelMessage>) {
                if (!is_selected_security(msg.stock_locate))
                    return ReplayError::IgnoredMessage;
                return map_cancel(msg);
            }
            else if constexpr (std::is_same_v<T, DeleteMessage>) {
                if (!is_selected_security(msg.stock_locate))
                    return ReplayError::IgnoredMessage;
                return map_delete(msg);
            }
            else if constexpr (std::is_same_v<T, ReplaceMessage>) {
                if (!is_selected_security(msg.stock_locate))
                    return ReplayError::IgnoredMessage;
                return map_replace(msg);
            }
            else if constexpr (std::is_same_v<T, StockDirectoryMessage>) {
                return ReplayError::IgnoredMessage;
            }
            else if constexpr (std::is_same_v<T, SystemEventMessage>) {
                return ReplayError::IgnoredMessage;
            }
            else if constexpr (std::is_same_v<T, StockTradingActionMessage>) {
                return ReplayError::IgnoredMessage;
            }
            else if constexpr (std::is_same_v<T, RegSHOMessage>) {
                return ReplayError::IgnoredMessage;
            }
            else if constexpr (std::is_same_v<T, MarketParticipantPositionMessage>) {
                return ReplayError::IgnoredMessage;
            }
            else if constexpr (std::is_same_v<T, MWCBDeclineLevelMessage>) {
                return ReplayError::IgnoredMessage;
            }
            else if constexpr (std::is_same_v<T, MWCBStatusMessage>) {
                return ReplayError::IgnoredMessage;
            }
            else if constexpr (std::is_same_v<T, QuotingPeriodUpdateMessage>) {
                return ReplayError::IgnoredMessage;
            }
            else if constexpr (std::is_same_v<T, LULDAuctionCollarMessage>) {
                return ReplayError::IgnoredMessage;
            }
            else if constexpr (std::is_same_v<T, OperationalHaltMessage>) {
                return ReplayError::IgnoredMessage;
            }
            else if constexpr (std::is_same_v<T, TradeMessage>) {
                return ReplayError::IgnoredMessage;
            }
            else if constexpr (std::is_same_v<T, CrossTradeMessage>) {
                return ReplayError::IgnoredMessage;
            }
            else if constexpr (std::is_same_v<T, BrokenTradeMessage>) {
                return ReplayError::IgnoredMessage;
            }
            else if constexpr (std::is_same_v<T, NOIIMessage>) {
                return ReplayError::IgnoredMessage;
            }
            else if constexpr (std::is_same_v<T, RetailInterestMessage>) {
                return ReplayError::IgnoredMessage;
            }
            else if constexpr (std::is_same_v<T, DirectListingCapitalRaiseMessage>) {
                return ReplayError::IgnoredMessage;
            } },
                      message);
}
