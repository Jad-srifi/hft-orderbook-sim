#pragma once

#include <cstdint>
#include <unordered_map>


enum class Side {
    BUY,
    SELL
};

using OrderId = std::uint64_t;
using Price = std::int64_t;
using Quantity = std::uint64_t;

using Timestamp = std::uint64_t;
using SequenceNumber = std::uint64_t;

using RelativeSpread = double;
using Imbalance = double;
using MidPrice = double;
using TradeCount = std::size_t;

using Vwap = double;
using ExecutionValue = std::int64_t;
using Slippage = double;
using ExecutionCost = std::int64_t;
