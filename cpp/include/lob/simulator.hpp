#pragma once

#include <types.hpp>
#include <execution.hpp>
#include <event.hpp>
#include <order_book.hpp>
#include <vector>
#include <trade.hpp>
#include <inventory_model.hpp>

struct Simulator {
    Simulator(Cash initial_cash);

    OrderBook order_book;
    
    Timestamp current_time = 0;

    SequenceNumber last_sequence = 0;

    InventoryModel inventory_model;

    std::vector<Event> events;

    std::vector<Trade> trades;

    SidesByOrder sides_by_order;

    ReferencePricesByOrder reference_prices_by_order;

    void add_event(Event event);

    void process_event(const Event& event);

    void process_events();

    ExecutionResultsByOrder calculate_execution_results();

    InventoryModel& get_inventory_model();
};