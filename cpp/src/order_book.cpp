#include <order.hpp>
#include <order_book.hpp>
#include <types.hpp>
#include <vector>

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