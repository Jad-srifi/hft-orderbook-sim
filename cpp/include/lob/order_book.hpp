#pragma once

#include <types.hpp>
#include <price_level.hpp>
#include <order_map.hpp>
#include <trade.hpp>
#include <vector>


struct OrderBook {
    std::vector<PriceLevel> bids;
    std::vector<PriceLevel> asks;

    OrderMap order_map;

    void add(const Order &order);

    bool cancel(OrderId order_id);

    void update_shifted_indices(Side side, Price price, std::size_t erased_index);

    const std::vector<PriceLevel>& bid_levels() const;

    const std::vector<PriceLevel>& ask_levels() const;

    Price best_bid() const;
    
    Price best_ask() const; 

    Price spread() const;

    std::vector<Trade> process_order(Order &order);

    void sort_price_levels();

    Order* find_order(OrderId order_id);

    bool modify(OrderId order_id, Price new_price, Quantity new_quantity);
};