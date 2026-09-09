#pragma once

#include <order.hpp>
#include <types.hpp>

struct Trade {
    OrderId incoming_order;
    OrderId resting_order;
    Price price;
    Quantity quantity;
};