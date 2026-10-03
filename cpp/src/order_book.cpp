#include <order.hpp>
#include <order_book.hpp>
#include <types.hpp>
#include <trade.hpp>
#include <vector>
#include <iostream>
#include <algorithm>
#include <order_map.hpp>
#include <profile.hpp>

void OrderBook::add(const Order &order)
{
    LOB_PROFILE_SCOPE("OrderBook::add");

    if (order.side == Side::BUY)
    {
        for (size_t i = 0, len = (this->bids).size(); i < len; i++)
        {
            if (order.price == this->bids[i].price)
            {
                OrderLocation location = {Side::BUY, order.price, (this->bids[i].orders).size()};

                {
                    LOB_PROFILE_SCOPE("OrderBook::add::existing_level_insert");

                    this->bids[i].total_quantity += order.quantity;
                    this->bids[i].orders.push_back(order);
                }

                {
                    LOB_PROFILE_SCOPE("OrderBook::add::ordermap_add");

                    this->order_map.add(order.id, location);
                }

                return;
            }
        }

        PriceLevel new_price_level;

        new_price_level.price = order.price;
        new_price_level.total_quantity = order.quantity;
        new_price_level.orders.push_back(order);

        OrderLocation location = {Side::BUY, new_price_level.price, 0};

        {
            LOB_PROFILE_SCOPE("OrderBook::add::ordermap_add");

            this->order_map.add(order.id, location);
        }

        {
            LOB_PROFILE_SCOPE("OrderBook::add::new_level_insert");

            this->bids.push_back(new_price_level);
        }
    }

    else if (order.side == Side::SELL)
    {
        for (size_t i = 0, len = (this->asks).size(); i < len; i++)
        {
            if (order.price == this->asks[i].price)
            {
                OrderLocation location = {Side::SELL, order.price, (this->asks[i].orders).size()};

                {
                    LOB_PROFILE_SCOPE("OrderBook::add::existing_level_insert");

                    this->asks[i].total_quantity += order.quantity;
                    this->asks[i].orders.push_back(order);
                }

                {
                    LOB_PROFILE_SCOPE("OrderBook::add::ordermap_add");

                    this->order_map.add(order.id, location);
                }

                return;
            }
        }

        PriceLevel new_price_level;

        new_price_level.price = order.price;
        new_price_level.total_quantity = order.quantity;
        new_price_level.orders.push_back(order);

        OrderLocation location = {Side::SELL, new_price_level.price, 0};

        {
            LOB_PROFILE_SCOPE("OrderBook::add::ordermap_add");

            this->order_map.add(order.id, location);
        }

        {
            LOB_PROFILE_SCOPE("OrderBook::add::new_level_insert");

            this->asks.push_back(new_price_level);
        }
    }

    else
    {
        return;
    }
}

bool OrderBook::cancel(OrderId order_id)
{
    LOB_PROFILE_SCOPE("OrderBook::cancel");

    std::optional<OrderLocation> location;

    {
        LOB_PROFILE_SCOPE("OrderBook::cancel::ordermap_find");

        location = this->order_map.find(order_id);
    }

    if (!location.has_value())
    {
        return false;
    }

    if (location->side == Side::BUY)
    {
        {
            LOB_PROFILE_SCOPE("OrderBook::cancel::price_level_search");

            for (size_t i = 0; i < this->bids.size(); i++)
            {

                PriceLevel *cur_pl = &this->bids[i];

                if (cur_pl->price == location->price)
                {

                    {
                        LOB_PROFILE_SCOPE("OrderBook::cancel::vector_erase");

                        cur_pl->total_quantity -= cur_pl->orders[location->index].quantity;

                        cur_pl->orders.erase(cur_pl->orders.begin() + location->index);
                    }

                    {
                        LOB_PROFILE_SCOPE("OrderBook::cancel::ordermap_remove");

                        this->order_map.remove(order_id);
                    }

                    {
                        LOB_PROFILE_SCOPE("OrderBook::cancel::shifted_indices");

                        this->update_shifted_indices(location->side, location->price, location->index);
                    }

                    {
                        LOB_PROFILE_SCOPE("OrderBook::cancel::empty_level_cleanup");

                        if (cur_pl->orders.empty())
                        {
                            this->bids.erase(this->bids.begin() + i);
                        }
                    }

                    return true;
                }
            }
        }
    }

    else if (location->side == Side::SELL)
    {
        {
            LOB_PROFILE_SCOPE("OrderBook::cancel::price_level_search");

            for (size_t i = 0; i < this->asks.size(); i++)
            {

                PriceLevel *cur_pl = &this->asks[i];

                if (cur_pl->price == location->price)
                {

                    {
                        LOB_PROFILE_SCOPE("OrderBook::cancel::vector_erase");

                        cur_pl->total_quantity -= cur_pl->orders[location->index].quantity;

                        cur_pl->orders.erase(cur_pl->orders.begin() + location->index);
                    }

                    {
                        LOB_PROFILE_SCOPE("OrderBook::cancel::ordermap_remove");

                        this->order_map.remove(order_id);
                    }

                    {
                        LOB_PROFILE_SCOPE("OrderBook::cancel::shifted_indices");

                        this->update_shifted_indices(location->side, location->price, location->index);
                    }

                    {
                        LOB_PROFILE_SCOPE("OrderBook::cancel::empty_level_cleanup");

                        if (cur_pl->orders.empty())
                        {
                            this->asks.erase(this->asks.begin() + i);
                        }
                    }

                    return true;
                }
            }
        }
    }

    return false;
}

void OrderBook::update_shifted_indices(Side side, Price price, std::size_t erased_index)
{
    LOB_PROFILE_SCOPE("OrderBook::update_shifted_indices");

    if (side == Side::BUY)
    {
        for (size_t i = 0; i < this->bids.size(); i++)
        {

            PriceLevel &cur_pl = this->bids[i];

            if (cur_pl.price == price)
            {
                for (size_t j = 0; j < cur_pl.orders.size(); j++)
                {

                    Order &cur_or = cur_pl.orders[j];

                    if (j >= erased_index)
                    {
                        std::optional<OrderLocation> location;

                        {
                            LOB_PROFILE_SCOPE("OrderBook::update_shifted_indices::ordermap_find");

                            location = this->order_map.find(cur_or.id);
                        }

                        OrderLocation new_location = location.value();

                        new_location.index -= 1;

                        {
                            LOB_PROFILE_SCOPE("OrderBook::update_shifted_indices::ordermap_update");

                            this->order_map.update(cur_or.id, new_location);
                        }
                    }
                }

                return;
            }
        }
    }

    else if (side == Side::SELL)
    {
        for (size_t i = 0; i < this->asks.size(); i++)
        {

            PriceLevel &cur_pl = this->asks[i];

            if (cur_pl.price == price)
            {
                for (size_t j = 0; j < cur_pl.orders.size(); j++)
                {

                    Order &cur_or = cur_pl.orders[j];

                    if (j >= erased_index)
                    {
                        std::optional<OrderLocation> location;

                        {
                            LOB_PROFILE_SCOPE("OrderBook::update_shifted_indices::ordermap_find");

                            location = this->order_map.find(cur_or.id);
                        }

                        OrderLocation new_location = location.value();

                        new_location.index -= 1;

                        {
                            LOB_PROFILE_SCOPE("OrderBook::update_shifted_indices::ordermap_update");

                            this->order_map.update(cur_or.id, new_location);
                        }
                    }
                }

                return;
            }
        }
    }

    else
    {
        return;
    }
}

const std::vector<PriceLevel> &OrderBook::bid_levels() const
{
    return bids;
}

const std::vector<PriceLevel> &OrderBook::ask_levels() const
{
    return asks;
}

Price OrderBook::best_bid() const
{
    Price best_bid{0};

    for (size_t i = 0, len = (this->bids).size(); i < len; i++)
    {

        Price curr_bid = this->bids[i].price;

        if (best_bid == 0 || curr_bid > best_bid)
        {
            best_bid = curr_bid;
        }
    }

    return best_bid;
}

Price OrderBook::best_ask() const
{
    Price best_ask{0};

    for (size_t i = 0, len = (this->asks).size(); i < len; i++)
    {

        Price curr_ask = this->asks[i].price;

        if (best_ask == 0 || curr_ask < best_ask)
        {
            best_ask = curr_ask;
        }
    }

    return best_ask;
}

Price OrderBook::spread() const
{
    Price best_bid = OrderBook::best_bid();
    Price best_ask = OrderBook::best_ask();

    if (best_ask == 0 || best_bid == 0)
    {
        return 0;
    }

    return best_ask - best_bid;
}

void OrderBook::sort_price_levels()
{
    LOB_PROFILE_SCOPE("OrderBook::sort_price_levels");

    {
        LOB_PROFILE_SCOPE("OrderBook::sort_price_levels::asks");

        std::sort(this->asks.begin(), this->asks.end(),
                  [](const PriceLevel &a, const PriceLevel &b)
                  {
                      return a.price < b.price;
                  });
    }

    {
        LOB_PROFILE_SCOPE("OrderBook::sort_price_levels::bids");

        std::sort(this->bids.begin(), this->bids.end(),
                  [](const PriceLevel &a, const PriceLevel &b)
                  {
                      return a.price > b.price;
                  });
    }
}

std::vector<Trade> OrderBook::process_order(Order &order)
{
    LOB_PROFILE_SCOPE("OrderBook::process_order");

    {
        LOB_PROFILE_SCOPE("OrderBook::process_order::sort");

        this->sort_price_levels();
    }

    std::vector<Trade> trades;

    if (order.side == Side::BUY)
    {

        size_t i = 0;

        while (i < (this->asks).size())
        {

            PriceLevel *cur_pl = &this->asks[i];

            {
                LOB_PROFILE_SCOPE("OrderBook::process_order::buy_price_level_traversal");

                if (order.price < cur_pl->price)
                {
                    this->add(order);
                    return trades;
                }
            }

            size_t initial_size = this->asks.size();

            size_t j = 0;

            while (j < (cur_pl->orders).size())
            {

                Order *cur_or = &cur_pl->orders[j];

                {
                    LOB_PROFILE_SCOPE("OrderBook::process_order::buy_order_traversal");

                    if (order.quantity <= cur_or->quantity)
                    {

                        {
                            LOB_PROFILE_SCOPE("OrderBook::process_order::trade_creation");

                            Trade trade = {
                                order.id,
                                cur_or->id,
                                cur_or->price,
                                order.quantity};

                            trades.push_back(trade);
                        }

                        if (order.quantity == cur_or->quantity)
                        {
                            OrderId resting_id = cur_or->id;
                            order.quantity = 0;

                            {
                                LOB_PROFILE_SCOPE("OrderBook::process_order::full_fill_removal");

                                this->cancel(resting_id);
                            }
                        }

                        else
                        {
                            {
                                LOB_PROFILE_SCOPE("OrderBook::process_order::partial_fill_update");

                                cur_or->quantity -= order.quantity;
                                cur_pl->total_quantity -= order.quantity;
                                order.quantity = 0;
                            }
                        }

                        return trades;
                    }

                    else if (order.quantity > cur_or->quantity)
                    {

                        {
                            LOB_PROFILE_SCOPE("OrderBook::process_order::trade_creation");

                            Trade trade = {
                                order.id,
                                cur_or->id,
                                cur_or->price,
                                cur_or->quantity};

                            trades.push_back(trade);
                        }

                        order.quantity -= cur_or->quantity;

                        bool level_empty = (cur_pl->orders.size() == 1);

                        {
                            LOB_PROFILE_SCOPE("OrderBook::process_order::full_fill_removal");

                            this->cancel(cur_or->id);
                        }

                        if (level_empty)
                        {
                            break;
                        }
                    }
                }
            }

            if (this->asks.size() == initial_size)
            {
                i++;
            }
        }

        {
            LOB_PROFILE_SCOPE("OrderBook::process_order::rest_remaining");

            this->add(order);
        }

        return trades;
    }

    else if (order.side == Side::SELL)
    {

        size_t i = 0;

        while (i < (this->bids).size())
        {

            PriceLevel *cur_pl = &this->bids[i];

            {
                LOB_PROFILE_SCOPE("OrderBook::process_order::sell_price_level_traversal");

                if (order.price > cur_pl->price)
                {
                    this->add(order);
                    return trades;
                }
            }

            size_t initial_size = this->bids.size();

            size_t j = 0;

            while (j < (cur_pl->orders).size())
            {

                Order *cur_or = &cur_pl->orders[j];

                {
                    LOB_PROFILE_SCOPE("OrderBook::process_order::sell_order_traversal");

                    if (order.quantity <= cur_or->quantity)
                    {

                        {
                            LOB_PROFILE_SCOPE("OrderBook::process_order::trade_creation");

                            Trade trade = {
                                order.id,
                                cur_or->id,
                                cur_or->price,
                                order.quantity};

                            trades.push_back(trade);
                        }

                        if (order.quantity == cur_or->quantity)
                        {
                            OrderId resting_id = cur_or->id;
                            order.quantity = 0;

                            {
                                LOB_PROFILE_SCOPE("OrderBook::process_order::full_fill_removal");

                                this->cancel(resting_id);
                            }
                        }

                        else
                        {
                            {
                                LOB_PROFILE_SCOPE("OrderBook::process_order::partial_fill_update");

                                cur_or->quantity -= order.quantity;
                                cur_pl->total_quantity -= order.quantity;
                                order.quantity = 0;
                            }
                        }

                        return trades;
                    }

                    else if (order.quantity > cur_or->quantity)
                    {

                        {
                            LOB_PROFILE_SCOPE("OrderBook::process_order::trade_creation");

                            Trade trade = {
                                order.id,
                                cur_or->id,
                                cur_or->price,
                                cur_or->quantity};

                            trades.push_back(trade);
                        }

                        order.quantity -= cur_or->quantity;

                        bool level_empty = (cur_pl->orders.size() == 1);

                        {
                            LOB_PROFILE_SCOPE("OrderBook::process_order::full_fill_removal");

                            this->cancel(cur_or->id);
                        }

                        if (level_empty)
                        {
                            break;
                        }
                    }
                }
            }

            if (this->bids.size() == initial_size)
            {
                i++;
            }
        }

        {
            LOB_PROFILE_SCOPE("OrderBook::process_order::rest_remaining");

            this->add(order);
        }

        return trades;
    }

    else
    {
        return trades;
    }
}

Order *OrderBook::find_order(OrderId order_id)
{
    LOB_PROFILE_SCOPE("OrderBook::find_order");

    std::optional<OrderLocation> location;

    {
        LOB_PROFILE_SCOPE("OrderBook::find_order::ordermap_find");

        location = this->order_map.find(order_id);
    }

    if (!location.has_value())
    {
        return nullptr;
    }

    if (location->side == Side::BUY)
    {
        {
            LOB_PROFILE_SCOPE("OrderBook::find_order::price_level_search");

            for (size_t i = 0; i < this->bids.size(); i++)
            {

                PriceLevel &cur_pl = this->bids[i];

                if (location->price == cur_pl.price)
                {
                    return &cur_pl.orders[location->index];
                }
            }
        }
    }

    else if (location->side == Side::SELL)
    {
        {
            LOB_PROFILE_SCOPE("OrderBook::find_order::price_level_search");

            for (size_t i = 0; i < this->asks.size(); i++)
            {

                PriceLevel &cur_pl = this->asks[i];

                if (location->price == cur_pl.price)
                {
                    return &cur_pl.orders[location->index];
                }
            }
        }
    }

    return nullptr;
}

bool OrderBook::modify(OrderId order_id, Price new_price, Quantity new_quantity)
{
    LOB_PROFILE_SCOPE("OrderBook::modify");

    Order *order;

    {
        LOB_PROFILE_SCOPE("OrderBook::modify::lookup");

        order = this->find_order(order_id);
    }

    if (order == nullptr)
    {
        return false;
    }

    if (new_quantity == 0)
    {
        {
            LOB_PROFILE_SCOPE("OrderBook::modify::cancel");

            this->cancel(order_id);
        }

        return true;
    }

    bool lose_priority = new_price != order->price || new_quantity > order->quantity;

    if (!lose_priority)
    {
        {
            LOB_PROFILE_SCOPE("OrderBook::modify::in_place");

            order->price = new_price;
            order->quantity = new_quantity;
        }

        return true;
    }

    Order new_order = {order_id, order->side, new_price, new_quantity};

    {
        LOB_PROFILE_SCOPE("OrderBook::modify::cancel_old_order");

        this->cancel(order_id);
    }

    {
        LOB_PROFILE_SCOPE("OrderBook::modify::re_add_order");

        this->add(new_order);
    }

    return true;
}
