#include <order_map.hpp>

void OrderMap::add(OrderId order_id, OrderLocation order_location) {
    orders[order_id] = order_location;
}

void OrderMap::remove(OrderId order_id) {
    orders.erase(order_id);
}

std::optional<OrderLocation> OrderMap::find(OrderId order_id) {
    auto it = orders.find(order_id);
    
    if (it != orders.end()) {
        return it->second;
    }
    
    return std::nullopt;
}

void OrderMap::update(OrderId order_id, OrderLocation new_location) {
    auto it = orders.find(order_id);
    
    if (it != orders.end()) {
        it->second = new_location;
    }
}