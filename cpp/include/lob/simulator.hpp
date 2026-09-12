#pragma once

#include <event.hpp>
#include <order_book.hpp>
#include <vector>
#include <trade.hpp>

struct Simulator {
    OrderBook order_book;
    
    Timestamp current_time = 0;

    SequenceNumber last_sequence = 0;

    std::vector<Event> events;

    std::vector<Trade> trades;

    void add_event(Event event);

    void process_event(const Event& event);

    void process_events();
};