#include <order_map.hpp>
#include <profile.hpp>

void OrderMap::add(OrderId order_id, OrderLocation order_location)
{
    LOB_PROFILE_SCOPE("OrderMap::add");

    orders[order_id] = order_location;
}

void OrderMap::remove(OrderId order_id)
{
    LOB_PROFILE_SCOPE("OrderMap::remove");

    orders.erase(order_id);
}

std::optional<OrderLocation> OrderMap::find(OrderId order_id)
{
    LOB_PROFILE_SCOPE("OrderMap::find");

    auto it = orders.find(order_id);

    if (it != orders.end())
    {
        return it->second;
    }

    return std::nullopt;
}

void OrderMap::update(OrderId order_id, OrderLocation new_location)
{
    LOB_PROFILE_SCOPE("OrderMap::update");

    auto it = orders.find(order_id);

    if (it != orders.end())
    {
        it->second = new_location;
    }
}
