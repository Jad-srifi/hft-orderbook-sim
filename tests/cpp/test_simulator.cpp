#include <simulator.hpp>
#include <event.hpp>

#include <cassert>
#include <iostream>

int main() {

    // ============================================================
    // 1. ADD EVENTS
    // ============================================================

    {
        Simulator simulator;

        Event event1{};
        event1.type = EventType::ADD;
        event1.timestamp = 100;
        event1.sequence = 0;
        event1.order = {1, Side::BUY, 10000, 100};

        Event event2{};
        event2.type = EventType::ADD;
        event2.timestamp = 100;
        event2.sequence = 1;
        event2.order = {2, Side::SELL, 10500, 100};

        simulator.add_event(event1);
        simulator.add_event(event2);

        assert(simulator.events.size() == 2);

        simulator.process_events();

        assert(simulator.current_time == 100);
        assert(simulator.last_sequence == 1);

        assert(simulator.order_book.find_order(1) != nullptr);
        assert(simulator.order_book.find_order(2) != nullptr);
    }


    // ============================================================
    // 2. CANCEL EVENT
    // ============================================================

    {
        Simulator simulator;

        Event add{};
        add.type = EventType::ADD;
        add.timestamp = 100;
        add.sequence = 0;
        add.order = {10, Side::BUY, 10000, 100};

        Event cancel{};
        cancel.type = EventType::CANCEL;
        cancel.timestamp = 101;
        cancel.sequence = 0;
        cancel.order_id = 10;

        simulator.add_event(add);
        simulator.add_event(cancel);

        simulator.process_events();

        assert(simulator.order_book.find_order(10) == nullptr);
        assert(simulator.current_time == 101);
        assert(simulator.last_sequence == 0);
    }


    // ============================================================
    // 3. MODIFY EVENT
    // ============================================================

    {
        Simulator simulator;

        Event add{};
        add.type = EventType::ADD;
        add.timestamp = 100;
        add.sequence = 0;
        add.order = {20, Side::BUY, 10000, 100};

        Event modify{};
        modify.type = EventType::MODIFY;
        modify.timestamp = 101;
        modify.sequence = 0;
        modify.order_id = 20;
        modify.new_price = 10100;
        modify.new_quantity = 150;

        simulator.add_event(add);
        simulator.add_event(modify);

        simulator.process_events();

        Order* order = simulator.order_book.find_order(20);

        assert(order != nullptr);
        assert(order->price == 10100);
        assert(order->quantity == 150);

        assert(simulator.current_time == 101);
        assert(simulator.last_sequence == 0);
    }


    // ============================================================
    // 4. DUPLICATE ADD
    // ============================================================

    {
        Simulator simulator;

        Event add1{};
        add1.type = EventType::ADD;
        add1.timestamp = 100;
        add1.sequence = 0;
        add1.order = {30, Side::BUY, 10000, 100};

        Event add2{};
        add2.type = EventType::ADD;
        add2.timestamp = 101;
        add2.sequence = 0;
        add2.order = {30, Side::BUY, 10100, 200};

        simulator.add_event(add1);
        simulator.add_event(add2);

        simulator.process_events();

        Order* order = simulator.order_book.find_order(30);

        assert(order != nullptr);
        assert(order->price == 10000);
        assert(order->quantity == 100);

        // Rejected event must not advance simulation state.
        assert(simulator.current_time == 100);
        assert(simulator.last_sequence == 0);
    }


    // ============================================================
    // 5. INVALID CANCEL
    // ============================================================

    {
        Simulator simulator;

        Event cancel{};
        cancel.type = EventType::CANCEL;
        cancel.timestamp = 100;
        cancel.sequence = 0;
        cancel.order_id = 999;

        simulator.add_event(cancel);
        simulator.process_events();

        assert(simulator.current_time == 0);
        assert(simulator.last_sequence == 0);
    }


    // ============================================================
    // 6. INVALID MODIFY
    // ============================================================

    {
        Simulator simulator;

        Event modify{};
        modify.type = EventType::MODIFY;
        modify.timestamp = 100;
        modify.sequence = 0;
        modify.order_id = 999;
        modify.new_price = 10100;
        modify.new_quantity = 100;

        simulator.add_event(modify);
        simulator.process_events();

        assert(simulator.current_time == 0);
        assert(simulator.last_sequence == 0);
    }


    // ============================================================
    // 7. BACKWARD TIMESTAMP
    // ============================================================

    {
        Simulator simulator;

        Event event1{};
        event1.type = EventType::ADD;
        event1.timestamp = 100;
        event1.sequence = 0;
        event1.order = {40, Side::BUY, 10000, 100};

        Event event2{};
        event2.type = EventType::ADD;
        event2.timestamp = 99;
        event2.sequence = 0;
        event2.order = {41, Side::BUY, 10100, 100};

        simulator.add_event(event1);
        simulator.add_event(event2);

        simulator.process_events();

        assert(simulator.order_book.find_order(40) != nullptr);
        assert(simulator.order_book.find_order(41) == nullptr);

        assert(simulator.current_time == 100);
        assert(simulator.last_sequence == 0);
    }


    // ============================================================
    // 8. SAME-TIMESTAMP SEQUENCE ORDERING
    // ============================================================

    {
        Simulator simulator;

        Event event1{};
        event1.type = EventType::ADD;
        event1.timestamp = 100;
        event1.sequence = 0;
        event1.order = {50, Side::BUY, 10000, 100};

        Event event2{};
        event2.type = EventType::ADD;
        event2.timestamp = 100;
        event2.sequence = 1;
        event2.order = {51, Side::BUY, 10100, 100};

        Event event3{};
        event3.type = EventType::ADD;
        event3.timestamp = 100;
        event3.sequence = 2;
        event3.order = {52, Side::SELL, 10500, 100};

        simulator.add_event(event1);
        simulator.add_event(event2);
        simulator.add_event(event3);

        simulator.process_events();

        assert(simulator.order_book.find_order(50) != nullptr);
        assert(simulator.order_book.find_order(51) != nullptr);
        assert(simulator.order_book.find_order(52) != nullptr);

        assert(simulator.current_time == 100);
        assert(simulator.last_sequence == 2);
    }


    // ============================================================
    // 9. DUPLICATE SEQUENCE
    // ============================================================

    {
        Simulator simulator;

        Event event1{};
        event1.type = EventType::ADD;
        event1.timestamp = 100;
        event1.sequence = 0;
        event1.order = {60, Side::BUY, 10000, 100};

        Event event2{};
        event2.type = EventType::ADD;
        event2.timestamp = 100;
        event2.sequence = 0;
        event2.order = {61, Side::BUY, 10100, 100};

        simulator.add_event(event1);
        simulator.add_event(event2);

        simulator.process_events();

        assert(simulator.order_book.find_order(60) != nullptr);
        assert(simulator.order_book.find_order(61) == nullptr);

        assert(simulator.current_time == 100);
        assert(simulator.last_sequence == 0);
    }


    // ============================================================
    // 10. LOWER SEQUENCE
    // ============================================================

    {
        Simulator simulator;

        Event event1{};
        event1.type = EventType::ADD;
        event1.timestamp = 100;
        event1.sequence = 2;
        event1.order = {70, Side::BUY, 10000, 100};

        // This first event is invalid because a new timestamp
        // must start with sequence 0.
        simulator.add_event(event1);

        simulator.process_events();

        assert(simulator.order_book.find_order(70) == nullptr);
        assert(simulator.current_time == 0);
        assert(simulator.last_sequence == 0);
    }


    // ============================================================
    // 11. NEW TIMESTAMP REQUIRES SEQUENCE 0
    // ============================================================

    {
        Simulator simulator;

        Event event1{};
        event1.type = EventType::ADD;
        event1.timestamp = 100;
        event1.sequence = 0;
        event1.order = {80, Side::BUY, 10000, 100};

        Event event2{};
        event2.type = EventType::ADD;
        event2.timestamp = 101;
        event2.sequence = 5;
        event2.order = {81, Side::BUY, 10100, 100};

        simulator.add_event(event1);
        simulator.add_event(event2);

        simulator.process_events();

        assert(simulator.order_book.find_order(80) != nullptr);
        assert(simulator.order_book.find_order(81) == nullptr);

        assert(simulator.current_time == 100);
        assert(simulator.last_sequence == 0);
    }


    // ============================================================
    // 12. TRADE GENERATION
    // ============================================================

    {
        Simulator simulator;

        Event sell{};
        sell.type = EventType::ADD;
        sell.timestamp = 100;
        sell.sequence = 0;
        sell.order = {90, Side::SELL, 10500, 100};

        Event buy{};
        buy.type = EventType::ADD;
        buy.timestamp = 101;
        buy.sequence = 0;
        buy.order = {91, Side::BUY, 10600, 60};

        simulator.add_event(sell);
        simulator.add_event(buy);

        simulator.process_events();

        assert(simulator.trades.size() == 1);

        assert(simulator.trades[0].incoming_order == 91);
        assert(simulator.trades[0].resting_order == 90);
        assert(simulator.trades[0].price == 10500);
        assert(simulator.trades[0].quantity == 60);
    }


    // ============================================================
    // 13. MULTIPLE EVENTS / EVENT STREAM
    // ============================================================

    {
        Simulator simulator;

        Event add1{};
        add1.type = EventType::ADD;
        add1.timestamp = 100;
        add1.sequence = 0;
        add1.order = {100, Side::BUY, 10000, 100};

        Event add2{};
        add2.type = EventType::ADD;
        add2.timestamp = 100;
        add2.sequence = 1;
        add2.order = {101, Side::SELL, 10500, 100};

        Event cancel{};
        cancel.type = EventType::CANCEL;
        cancel.timestamp = 101;
        cancel.sequence = 0;
        cancel.order_id = 100;

        Event modify{};
        modify.type = EventType::MODIFY;
        modify.timestamp = 102;
        modify.sequence = 0;
        modify.order_id = 101;
        modify.new_price = 10600;
        modify.new_quantity = 150;

        simulator.add_event(add1);
        simulator.add_event(add2);
        simulator.add_event(cancel);
        simulator.add_event(modify);

        simulator.process_events();

        assert(simulator.order_book.find_order(100) == nullptr);

        Order* order = simulator.order_book.find_order(101);

        assert(order != nullptr);
        assert(order->price == 10600);
        assert(order->quantity == 150);

        assert(simulator.current_time == 102);
        assert(simulator.last_sequence == 0);
    }


    // ============================================================
    // FINAL RESULT
    // ============================================================

    std::cout << "All Chapter 5 simulator tests passed.\n";

    return 0;
}
