#include <simulator.hpp>
#include <metrics.hpp>
#include <event.hpp>
#include <vector>

void Simulator::add_event(Event event) {
    this->events.push_back(event);
}

void Simulator::process_event(const Event& event) {
    if (event.timestamp >= this->current_time) {
        if (this->current_time == event.timestamp) {
            if (this->last_sequence >= event.sequence) {
                return ;
            }
        }

        else {
            if (event.sequence != 0) {
                return ;
            }
        }

        if (event.type == EventType::ADD) {
            bool order_exist = this->order_book.find_order(event.order.id) != nullptr;
            
            if (!order_exist) {
                this->reference_prices_by_order[event.order.id] = calculate_mid_price(this->order_book);
                this->sides_by_order[event.order.id] = event.order.side;

                Order new_order = event.order;
                
                std::vector<Trade> trades = this->order_book.process_order(new_order);
                
                this->trades.insert(this->trades.end(), trades.begin(), trades.end());
            }

            else {
                return ;
            }
        }

        else if (event.type == EventType::CANCEL) {
            bool canceled = this->order_book.cancel(event.order_id);

            if (!canceled) {
                return ;
            }
        }

        else if (event.type == EventType::MODIFY) {
            bool modified = this->order_book.modify(event.order_id, event.new_price, event.new_quantity);

            if (!modified) {
                return ;
            }

        }

        else {
            return ;
        }
        
        this->current_time = event.timestamp;
        this->last_sequence = event.sequence;
    }

    else {
        return ;
    }
}

void Simulator::process_events() {
    for (size_t i = 0; i < this->events.size(); i++) {
        process_event(this->events[i]);
    }
}

ExecutionResultsByOrder Simulator::calculate_execution_results() {
    return calculate_execution_results_by_order(this->trades, this->sides_by_order, this->reference_prices_by_order);
}

