#include <order.hpp>
#include <order_book.hpp>
#include <types.hpp>
#include <trade.hpp>
#include <vector>
#include <iostream>
#include <algorithm>

void OrderBook::add(const Order& order) {
    if (order.side == Side::BUY) {
        for (size_t i = 0, len = (this->bids).size(); i < len; i++) {
            if (order.price == this->bids[i].price) {
                this->bids[i].total_quantity += order.quantity;
                this->bids[i].orders.push_back(order);
                return;
            }
        }
        PriceLevel new_price_level;

        new_price_level.price = order.price;
        new_price_level.total_quantity = order.quantity;
        new_price_level.orders.push_back(order);

        this->bids.push_back(new_price_level);
    }

    else if (order.side == Side::SELL) {
        for (size_t i = 0, len = (this->asks).size(); i < len; i++) {
            if (order.price == this->asks[i].price) {
                this->asks[i].total_quantity += order.quantity;
                this->asks[i].orders.push_back(order);
                return;
            }
        }
        PriceLevel new_price_level;

        new_price_level.price = order.price;
        new_price_level.total_quantity = order.quantity;
        new_price_level.orders.push_back(order);

        this->asks.push_back(new_price_level);
    }

    else {return ;}
}

void OrderBook::cancel(OrderId order_id) {
    for (size_t i = 0, len = (this->bids).size(); i < len; i++) {
        PriceLevel* curr_PL = &this->bids[i];
        for (size_t j = 0, PL_len = (curr_PL->orders).size(); j < PL_len; j++) {
            Order* curr_order = &this->bids[i].orders[j];
            if (order_id == curr_order->id) {
                curr_PL->total_quantity -= curr_order->quantity;
                curr_PL->orders.erase(curr_PL->orders.begin() + j);
                if (curr_PL->total_quantity == 0) {
                    this->bids.erase(this->bids.begin() + i);
                } 
                return;
            }
        }
    }

    for (size_t i = 0, len = (this->asks).size(); i < len; i++) {
        PriceLevel* curr_PL = &this->asks[i];
        for (size_t j = 0, PL_len = (curr_PL->orders).size(); j < PL_len; j++) {
            Order* curr_order = &this->asks[i].orders[j];
            if (order_id == curr_order->id) {
                curr_PL->total_quantity -= curr_order->quantity;
                curr_PL->orders.erase(curr_PL->orders.begin() + j);
                if (curr_PL->total_quantity == 0) {
                    this->asks.erase(this->asks.begin() + i);
                } 
                return;
            }
        }
    }
}

Price OrderBook::best_bid() {
    Price best_bid {0};
    for (size_t i = 0, len = (this->bids).size(); i < len; i++) {
        Price curr_bid = this->bids[i].price;
        if (best_bid == 0 || curr_bid > best_bid) {
            best_bid = curr_bid;
        }
    }
    return best_bid;
}

Price OrderBook::best_ask() {
    Price best_ask {0};
    for (size_t i = 0, len = (this->asks).size(); i < len; i++) {
        Price curr_ask = this->asks[i].price;
        if (best_ask == 0 || curr_ask < best_ask) {
            best_ask = curr_ask;
        }
    }
    return best_ask;
}

float OrderBook::spread() {
    Price best_bid = OrderBook::best_bid();
    Price best_ask = OrderBook::best_ask();

    return best_ask - best_bid;
}

void OrderBook::sort_price_levels() {
    std::sort(this->asks.begin(), this->asks.end(),
    [](const PriceLevel& a, const PriceLevel& b) {
            return a.price < b.price;
    });

    std::sort(this->bids.begin(), this->bids.end(),
    [](const PriceLevel& a, const PriceLevel& b) {
            return a.price > b.price;
    });
}

std::vector<Trade> OrderBook::process_order(Order &order) {
    this->sort_price_levels();
    std::vector<Trade> trades;
    
    if (order.side == Side::BUY) {

        size_t i = 0;

        while (i < (this->asks).size()) {

            PriceLevel* cur_pl = &this->asks[i];
            
            if (order.price < cur_pl->price) {
                this->add(order);
                return trades;
            }
            
            size_t initial_size = this->asks.size();

            size_t j = 0;

            while (j < (cur_pl->orders).size()) {
                
                Order* cur_or = &cur_pl->orders[j];
                
                if (order.quantity <= cur_or->quantity) {
                    
                    cur_or->quantity -= order.quantity;
                    cur_pl->total_quantity -= order.quantity;
                    
                    Trade trade = {
                        order.id, 
                        cur_or->id, 
                        cur_or->price, 
                        order.quantity
                    };
                    
                    trades.push_back(trade);
                    
                    order.quantity = 0; 

                    if (cur_or->quantity == 0) {
                        cur_pl->orders.erase(cur_pl->orders.begin() + j);
                    }

                    if (cur_pl->orders.empty()) {
                        this->asks.erase(this->asks.begin() + i);
                    }
                    
                    return trades;
                }

                else if (order.quantity > cur_or->quantity) {

                    order.quantity -= cur_or->quantity;
                    cur_pl->total_quantity -= cur_or->quantity;

                    Trade trade = {
                        order.id, 
                        cur_or->id, 
                        cur_or->price, 
                        cur_or->quantity
                    };

                    trades.push_back(trade);
                    
                    cur_pl->orders.erase(cur_pl->orders.begin() + j);
                    
                    if (cur_pl->orders.empty()) {
                        this->asks.erase(this->asks.begin() + i);
                        break;
                    }

                }
            }
            if (this->asks.size() == initial_size) {
                i++;
            }
        }
        this->add(order);
        return trades;
    }

    else if (order.side == Side::SELL) {

        size_t i = 0;

        while (i < (this->bids).size()) {

            PriceLevel* cur_pl = &this->bids[i];
            
            if (order.price > cur_pl->price) {
                this->add(order);
                return trades;
            }
            
            size_t initial_size = this->bids.size();

            size_t j = 0;

            while (j < (cur_pl->orders).size()) {
                
                Order* cur_or = &cur_pl->orders[j];
                
                if (order.quantity <= cur_or->quantity) {
            
                    cur_or->quantity -= order.quantity;
                    cur_pl->total_quantity -= order.quantity;
                    
                    
                    Trade trade = {
                        order.id, 
                        cur_or->id, 
                        cur_or->price, 
                        order.quantity
                    };
                    
                    trades.push_back(trade);
                    
                    order.quantity = 0;
                    
                    if (cur_or->quantity == 0) {
                        cur_pl->orders.erase(cur_pl->orders.begin() + j);
                    }

                    if (cur_pl->orders.empty()) {
                        this->bids.erase(this->bids.begin() + i);
                    }
                    
                    return trades;
                }

                else if (order.quantity > cur_or->quantity) {

                    order.quantity -= cur_or->quantity;
                    cur_pl->total_quantity -= cur_or->quantity;

                    Trade trade = {
                        order.id, 
                        cur_or->id, 
                        cur_or->price, 
                        cur_or->quantity
                    };

                    trades.push_back(trade);

                    cur_pl->orders.erase(cur_pl->orders.begin() + j);

                    if (cur_pl->orders.empty()) {
                        this->bids.erase(this->bids.begin() + i);
                        break;
                    }

                }
            }
            if (this->bids.size() == initial_size) {
                i++;
            }
        }
        this->add(order);
        return trades;
    }
    else {return trades;}
}

