#pragma once

#include "order.hpp"
#include <vector>


struct PriceLevel {
    Price price;
    std::vector<Order> orders;
    Quantity total_quantity;
};