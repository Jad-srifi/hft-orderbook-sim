#pragma once

#include <cstdint>


enum class Side {
    BUY,
    SELL
};

using OrderId = std::uint64_t;
using Price = std::int64_t;
using Quantity = std::uint64_t;