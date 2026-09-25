#include <itch_replay.hpp>
#include <variant>
#include <type_traits>

ItchReplay::ItchReplay(OrderBook& order_book)
    : order_book(order_book)
{
}

std::variant<std::monostate, ReplayError> ItchReplay::apply_add(const AddOperation& operation) {
    Order *possible_order = this->order_book.find_order(operation.order_id);

    if (possible_order != nullptr) {
        return ReplayError::InvalidLifecycle;
    }

    Order order {
        operation.order_id,
        operation.side,
        operation.price,
        operation.quantity
    };
    
    this->order_book.add(order);

    return std::monostate{};
}

void ItchReplay::apply_remove(const RemoveOperation& operation) {
    this->order_book.cancel(operation.order_id);
}

std::variant<std::monostate, ReplayError> ItchReplay::apply_reduce(const ReduceOperation& operation) {
    Order* order = this->order_book.find_order(operation.order_id);

    if (order == nullptr) {
        return ReplayError::InvalidLifecycle; 
    }

    if (operation.quantity > order->quantity) {
        return ReplayError::InvalidQuantity;
    }

    Quantity new_quantity = order->quantity - operation.quantity;

    this->order_book.modify(operation.order_id, order->price, new_quantity);

    return std::monostate{};
}

std::variant<std::monostate, ReplayError> ItchReplay::apply_replace(const ReplaceOperation& operation) {
    Order* old_order = this->order_book.find_order(operation.old_order_id);

    if (old_order == nullptr) {
        return ReplayError::InvalidLifecycle;
    }

    Order order {
        operation.new_order_id,
        old_order->side,
        operation.price,
        operation.quantity
    };

    this->order_book.cancel(operation.old_order_id);

    this->order_book.add(order);

    return std::monostate{};
}

std::variant<std::monostate, ReplayError> ItchReplay::apply(const ReplayOperation& operation) {

    return std::visit([this](const auto& op) -> std::variant<std::monostate, ReplayError> {
            using T = std::decay_t<decltype(op)>;

            if constexpr (std::is_same_v<T, AddOperation>) {
                return this->apply_add(op);
            }

            else if constexpr (std::is_same_v<T, ReduceOperation>) {
                return this->apply_reduce(op);
            }

            else if constexpr (std::is_same_v<T, ReplaceOperation>) {
                return this->apply_replace(op);
            }

            else if constexpr (std::is_same_v<T, RemoveOperation>) {
                this->apply_remove(op);
                return std::monostate{};
            }
        },
        operation
    );
}

