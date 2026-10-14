#include <simulator.hpp>
#include <execution.hpp>
#include <metrics.hpp>
#include <profile.hpp>

Simulator::Simulator(Cash initial_cash)
    : inventory_model(initial_cash)
{
}

void Simulator::add_event(Event event)
{
    this->events.push_back(event);
}

void Simulator::process_event(const Event &event)
{
    LOB_PROFILE_SCOPE("Simulator::process_event");

    if (event.timestamp < this->current_time)
    {
        return;
    }

    if (event.timestamp == this->current_time &&
        event.sequence <= this->last_sequence)
    {
        return;
    }

    if (event.timestamp > this->current_time &&
        event.sequence != 0)
    {
        return;
    }

    if (event.type == EventType::ADD)
    {
        LOB_PROFILE_SCOPE("Simulator::process_event::ADD");

        {
            LOB_PROFILE_SCOPE("Simulator::process_event::ADD::order_lookup");

            if (this->order_book.find_order(event.order.id) != nullptr)
            {
                return;
            }
        }

        {
            LOB_PROFILE_SCOPE("Simulator::process_event::ADD::execution_context");

            this->reference_prices_by_order[event.order.id] =
                calculate_mid_price(this->order_book);

            this->sides_by_order[event.order.id] =
                event.order.side;
        }

        Order new_order = event.order;

        std::vector<Trade> new_trades;

        {
            LOB_PROFILE_SCOPE("Simulator::process_event::ADD::process_order");

            new_trades = this->order_book.process_order(new_order);
        }

        for (const Trade &trade : new_trades)
        {
            {
                LOB_PROFILE_SCOPE("Simulator::process_event::ADD::trade_handling");

                this->trades.push_back(trade);
            }

            {
                LOB_PROFILE_SCOPE("Simulator::process_event::ADD::inventory_update");

                this->inventory_model.process_trade(
                    trade,
                    event.order.side);
            }
        }
    }

    else if (event.type == EventType::CANCEL)
    {
        LOB_PROFILE_SCOPE("Simulator::process_event::CANCEL");

        {
            LOB_PROFILE_SCOPE("Simulator::process_event::CANCEL::order_book_cancel");

            this->order_book.cancel(event.order_id);
        }
    }

    else if (event.type == EventType::MODIFY)
    {
        LOB_PROFILE_SCOPE("Simulator::process_event::MODIFY");

        {
            LOB_PROFILE_SCOPE("Simulator::process_event::MODIFY::order_book_modify");

            this->order_book.modify(
                event.order_id,
                event.new_price,
                event.new_quantity);
        }
    }

    this->current_time = event.timestamp;
    this->last_sequence = event.sequence;
}

void Simulator::process_events()
{
    LOB_PROFILE_SCOPE("Simulator::process_events");

    for (size_t i = 0; i < this->events.size(); i++)
    {

        {
            LOB_PROFILE_SCOPE("Simulator::process_events::process_event");

            this->process_event(this->events[i]);
        }
    }
}

ExecutionResultsByOrder Simulator::calculate_execution_results()
{
    return calculate_execution_results_by_order(
        this->trades,
        this->sides_by_order,
        this->reference_prices_by_order);
}

InventoryModel &Simulator::get_inventory_model()
{
    return this->inventory_model;
}
