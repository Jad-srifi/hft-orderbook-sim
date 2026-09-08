#pragma once

#include <types.hpp>


struct Order {
    OrderId id;
    Side side;
    Price price;
    Quantity quantity;
};