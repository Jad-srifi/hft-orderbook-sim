#pragma once

#include <order.hpp>
#include <unordered_map>
#include <order.hpp>
#include <optional>

struct OrderLocation {
    Side side;
    Price price;
    std::size_t index;
};

struct OrderMap {
    std::unordered_map<OrderId, OrderLocation> orders;

    void add(OrderId order_id, OrderLocation order_location);

    void remove(OrderId order_id);

    std::optional<OrderLocation> find(OrderId order_id);

    void update(OrderId order_id, OrderLocation new_location);
};
