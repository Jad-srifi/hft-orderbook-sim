#include <order_book.hpp>
#include <itch_mapper.hpp>

class ItchReplay {
    private:
        OrderBook& order_book;

        std::variant<std::monostate, ReplayError> apply_add(const AddOperation& operation);
        std::variant<std::monostate, ReplayError> apply_reduce(const ReduceOperation& operation);
        void apply_remove(const RemoveOperation& operation);
        std::variant<std::monostate, ReplayError> apply_replace(const ReplaceOperation& operation);

    public:
        ItchReplay(OrderBook& order_book);

        std::variant<std::monostate, ReplayError> apply(const ReplayOperation& operation);
};

