#pragma once

#include <types.hpp>
#include <variant>
#include <itch_message.hpp>

enum ReplayError{
    UnknownOrder,
    InvalidLifecycle,
    InvalidQuantity,
    IgnoredMessage
};

struct AddOperation {
    OrderId order_id;
    Side side;
    Quantity quantity;
    Price price;
};

struct ReduceOperation {
    OrderId order_id;
    Quantity quantity;
};

struct RemoveOperation {
    OrderId order_id;
};

struct ReplaceOperation {
    OrderId old_order_id;
    OrderId new_order_id;
    Side side;
    Quantity quantity;
    Price price;
};

using ReplayOperation = std::variant<AddOperation, ReduceOperation, RemoveOperation, ReplaceOperation>;

class ItchMapper{
    private:
        StockLocate selected_stock_locate;

        std::variant<ReplayOperation, ReplayError> map_add(const AddOrderMessage& message);

        std::variant<ReplayOperation, ReplayError> map_add_mpid(const AddOrderMPIDMessage& message);

        std::variant<ReplayOperation, ReplayError> map_execute(const ExecuteMessage& message);

        std::variant<ReplayOperation, ReplayError> map_execute_with_price(const ExecuteWithPriceMessage& message);

        std::variant<ReplayOperation, ReplayError> map_cancel(const CancelMessage& message);

        std::variant<ReplayOperation, ReplayError> map_delete(const DeleteMessage& message);

        std::variant<ReplayOperation, ReplayError> map_replace(const ReplaceMessage& message);

    public:
        ItchMapper(StockLocate selected_stock_locate);

        bool is_selected_security(StockLocate stock_locate);

        std::variant<ReplayOperation, ReplayError> map(const ItchMessage& message);
};