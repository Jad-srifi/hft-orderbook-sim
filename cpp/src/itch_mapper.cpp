#include <itch_mapper.hpp>
#include <type_traits>

ItchMapper::ItchMapper(StockLocate selected_stock_locate)
    : selected_stock_locate(selected_stock_locate)
{
}

bool ItchMapper::is_selected_security(StockLocate stock_locate) {
    return stock_locate == this->selected_stock_locate;
}

std::variant<ReplayOperation, ReplayError> ItchMapper::map_add(const AddOrderMessage& message) {
    AddOperation operation{
        message.order_reference,
        message.side,
        message.shares,
        message.price
    };
    
    return operation;
}

std::variant<ReplayOperation, ReplayError> ItchMapper::map_add_mpid(const AddOrderMPIDMessage& message) {
    AddOperation operation{
        message.order_reference,
        message.side,
        message.shares,
        message.price
    };

    return operation;
}

std::variant<ReplayOperation, ReplayError> ItchMapper::map_execute(const ExecuteMessage& message) {
    ReduceOperation operation{
        message.order_reference,
        message.executed_shares
    };

    return operation;
}

std::variant<ReplayOperation, ReplayError> ItchMapper::map_execute_with_price(const ExecuteWithPriceMessage& message) {
    ReduceOperation operation{
        message.order_reference,
        message.executed_shares
    };

    return operation;
}

std::variant<ReplayOperation, ReplayError> ItchMapper::map_cancel(const CancelMessage& message) {
    ReduceOperation operation{
        message.order_reference,
        message.cancelled_shares
    };

    return operation;
}

std::variant<ReplayOperation, ReplayError> ItchMapper::map_delete(const DeleteMessage& message) {
    RemoveOperation operation{
        message.order_reference
    };

    return operation;
}

std::variant<ReplayOperation, ReplayError> ItchMapper::map_replace(const ReplaceMessage& message) {
    ReplaceOperation operation{
        message.old_order_reference,
        message.new_order_reference,
        Side::NONE,
        message.new_shares,
        message.new_price
    };

    return operation;
}

std::variant<ReplayOperation, ReplayError> ItchMapper::map(const ItchMessage& message) {
    
    return std::visit([this](const auto& msg) -> std::variant<ReplayOperation, ReplayError> {
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
        },
        message
    );
}

