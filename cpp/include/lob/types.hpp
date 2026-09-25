#pragma once

#include <cstdint>
#include <unordered_map>
#include <array>


enum class Side {
    BUY,
    SELL,
    NONE
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

using Position = std::int64_t;
using AvgCost = double;
using Cash = double;
using Pnl = double;

using Timestamp = std::uint64_t;
using OrderReferenceNumber = std::uint64_t;
using MatchNumber = std::uint64_t;
using StockLocate = std::uint64_t;
using Shares = std::uint64_t;
using StockSymbol = std::array<char, 8>;
using ItchPrice = std::uint32_t;
using MPID = std::array<char, 4>;
using TrackingNumber = std::uint16_t;