#pragma once

#include <cstdint>
#include <types.hpp>
#include <order.hpp>

using Timestamp = std::uint64_t;
using SequenceNumber = std::uint64_t;

enum class EventType {
    ADD,
    CANCEL,
    MODIFY
};

struct Event {
    EventType type;
    Timestamp timestamp;
    SequenceNumber sequence;
    Order order;
    OrderId order_id;
    Price new_price;
    Quantity new_quantity;
};