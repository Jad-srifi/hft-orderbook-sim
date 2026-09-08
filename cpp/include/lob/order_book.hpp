#pragma once

#include <types.hpp>
#include <price_level.hpp>
#include <vector>


struct OrderBook {
    std::vector<PriceLevel> bids;
    std::vector<PriceLevel> asks;

    void add(const Order &order);

    void cancel(OrderId order_id);

    Price best_bid();
    
    Price best_ask(); 

    float spread();
};