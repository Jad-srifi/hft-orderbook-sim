#include <types.hpp>
#include <order_book.hpp>
#include <order.hpp>
#include <trade.hpp>
#include <simulator.hpp>
#include <event.hpp>
#include <metrics.hpp>
#include <execution.hpp>

#include <iostream>
#include <vector>
#include <optional>
#include <unordered_map>

int main() {

    OrderBook book;

    // ------------------------------------------------------------
    // 1. Add initial resting orders
    // ------------------------------------------------------------

    Order order1 = {1, Side::BUY, 10000, 50};
    Order order2 = {2, Side::BUY, 10050, 150};
    Order order3 = {3, Side::SELL, 10550, 230};
    Order order4 = {4, Side::SELL, 10500, 20};

    book.add(order1);
    book.add(order2);
    book.add(order3);
    book.add(order4);

    std::cout << "===== INITIAL BOOK =====\n";

    std::cout << "Best Ask: "
              << book.best_ask() << '\n';

    std::cout << "Best Bid: "
              << book.best_bid() << '\n';

    std::cout << "Spread: "
              << book.spread() << '\n';


    // ------------------------------------------------------------
    // 2. Test OrderMap lookup
    // ------------------------------------------------------------

    std::cout << "\n===== ORDER MAP LOOKUP =====\n";

    std::optional<OrderLocation> location =
        book.order_map.find(2);

    if (location.has_value()) {

        std::cout
            << "Order 2 found\n"
            << "Side: "
            << (location->side == Side::BUY ? "BUY" : "SELL")
            << '\n'
            << "Price: "
            << location->price
            << '\n'
            << "Index: "
            << location->index
            << '\n';
    }
    else {

        std::cout << "Order 2 not found\n";
    }


    // ------------------------------------------------------------
    // 3. Cancel an order using OrderMap
    // ------------------------------------------------------------

    std::cout << "\n===== CANCEL ORDER 2 =====\n";

    book.cancel(2);

    location = book.order_map.find(2);

    if (!location.has_value()) {

        std::cout
            << "Order 2 successfully removed from OrderMap\n";
    }
    else {

        std::cout
            << "ERROR: Order 2 still exists in OrderMap\n";
    }

    std::cout
        << "Best Bid after cancellation: "
        << book.best_bid()
        << '\n';


    // ------------------------------------------------------------
    // 4. Verify shifted OrderMap index
    // ------------------------------------------------------------

    std::cout << "\n===== SHIFTED INDEX CHECK =====\n";

    location = book.order_map.find(1);

    if (location.has_value()) {

        std::cout
            << "Order 1 found after cancellation\n"
            << "Price: "
            << location->price
            << '\n'
            << "Index: "
            << location->index
            << '\n';
    }
    else {

        std::cout
            << "ERROR: Order 1 missing from OrderMap\n";
    }


    // ------------------------------------------------------------
    // 5. BUY matching
    // ------------------------------------------------------------

    std::cout << "\n===== BUY MATCHING =====\n";

    Order buy_order = {5, Side::BUY, 10550, 100};

    std::vector<Trade> buy_trades =
        book.process_order(buy_order);

    for (const Trade& trade : buy_trades) {

        std::cout
            << "Trade: Incoming Order "
            << trade.incoming_order
            << " | Resting Order "
            << trade.resting_order
            << " | Price "
            << trade.price
            << " | Quantity "
            << trade.quantity
            << '\n';
    }

    std::cout
        << "Remaining BUY quantity: "
        << buy_order.quantity
        << '\n';

    std::cout
        << "Best Ask: "
        << book.best_ask()
        << '\n';

    std::cout
        << "Best Bid: "
        << book.best_bid()
        << '\n';

    std::cout
        << "Spread: "
        << book.spread()
        << '\n';


    // ------------------------------------------------------------
    // 6. Check OrderMap after matching
    // ------------------------------------------------------------

    std::cout << "\n===== ORDER MAP AFTER BUY MATCH =====\n";

    location = book.order_map.find(4);

    if (location.has_value()) {

        std::cout
            << "Order 4 still exists\n"
            << "Price: "
            << location->price
            << " | Index: "
            << location->index
            << '\n';
    }
    else {

        std::cout
            << "Order 4 was fully filled and removed\n";
    }

    location = book.order_map.find(3);

    if (location.has_value()) {

        std::cout
            << "Order 3 still exists\n"
            << "Price: "
            << location->price
            << " | Index: "
            << location->index
            << '\n';
    }
    else {

        std::cout
            << "ERROR: Order 3 missing from OrderMap\n";
    }


    // ------------------------------------------------------------
    // 7. SELL matching
    // ------------------------------------------------------------

    std::cout << "\n===== SELL MATCHING =====\n";

    Order sell_order = {6, Side::SELL, 10000, 100};

    std::vector<Trade> sell_trades =
        book.process_order(sell_order);

    for (const Trade& trade : sell_trades) {

        std::cout
            << "Trade: Incoming Order "
            << trade.incoming_order
            << " | Resting Order "
            << trade.resting_order
            << " | Price "
            << trade.price
            << " | Quantity "
            << trade.quantity
            << '\n';
    }

    std::cout
        << "Remaining SELL quantity: "
        << sell_order.quantity
        << '\n';

    std::cout
        << "Best Ask: "
        << book.best_ask()
        << '\n';

    std::cout
        << "Best Bid: "
        << book.best_bid()
        << '\n';

    std::cout
        << "Spread: "
        << book.spread()
        << '\n';


    // ------------------------------------------------------------
    // 8. Final OrderMap checks
    // ------------------------------------------------------------

    std::cout << "\n===== FINAL ORDER MAP STATE =====\n";

    location = book.order_map.find(1);

    if (location.has_value()) {

        std::cout
            << "Order 1 exists\n"
            << "Price: "
            << location->price
            << " | Index: "
            << location->index
            << '\n';
    }
    else {

        std::cout
            << "Order 1 does not exist\n";
    }

    location = book.order_map.find(3);

    if (location.has_value()) {

        std::cout
            << "Order 3 exists\n"
            << "Price: "
            << location->price
            << " | Index: "
            << location->index
            << '\n';
    }
    else {

        std::cout
            << "Order 3 does not exist\n";
    }

    location = book.order_map.find(5);

    if (location.has_value()) {

        std::cout
            << "Order 5 exists in OrderMap\n"
            << "Price: "
            << location->price
            << " | Index: "
            << location->index
            << '\n';
    }
    else {

        std::cout
            << "Order 5 is not in OrderMap "
            << "(fully executed)\n";
    }

    location = book.order_map.find(6);

    if (location.has_value()) {

        std::cout
            << "Order 6 exists in OrderMap\n"
            << "Price: "
            << location->price
            << " | Index: "
            << location->index
            << '\n';
    }
    else {

        std::cout
            << "Order 6 is not in OrderMap "
            << "(fully executed)\n";
    }


    // ------------------------------------------------------------
    // 9. Order modification / replace
    // ------------------------------------------------------------

    std::cout << "\n===== ORDER MODIFICATION =====\n";

    Order order7 = {7, Side::BUY, 9900, 100};
    Order order8 = {8, Side::BUY, 9900, 200};

    book.add(order7);
    book.add(order8);

    std::cout << "Before modification:\n";

    location = book.order_map.find(7);

    if (location.has_value()) {

        std::cout
            << "Order 7 -> Price: "
            << location->price
            << " | Index: "
            << location->index
            << '\n';
    }

    location = book.order_map.find(8);

    if (location.has_value()) {

        std::cout
            << "Order 8 -> Price: "
            << location->price
            << " | Index: "
            << location->index
            << '\n';
    }


    // Decrease quantity of Order 8.
    // Same price + smaller quantity -> FIFO preserved.

    book.modify(8, 9900, 150);

    std::cout
        << "\nAfter quantity decrease of Order 8:\n";

    Order* modified_order = book.find_order(8);

    if (modified_order != nullptr) {

        std::cout
            << "Order 8 -> Quantity: "
            << modified_order->quantity
            << '\n';
    }

    location = book.order_map.find(8);

    if (location.has_value()) {

        std::cout
            << "Order 8 -> Price: "
            << location->price
            << " | Index: "
            << location->index
            << '\n';
    }


    // Increase quantity of Order 7.
    // Same price + larger quantity -> cancel/re-add -> FIFO lost.

    book.modify(7, 9900, 200);

    std::cout
        << "\nAfter quantity increase of Order 7:\n";

    location = book.order_map.find(7);

    if (location.has_value()) {

        std::cout
            << "Order 7 -> Price: "
            << location->price
            << " | Index: "
            << location->index
            << '\n';
    }

    location = book.order_map.find(8);

    if (location.has_value()) {

        std::cout
            << "Order 8 -> Price: "
            << location->price
            << " | Index: "
            << location->index
            << '\n';
    }


    // Change price of Order 8.
    // Price change -> cancel/re-add -> FIFO lost.

    book.modify(8, 9800, 150);

    std::cout
        << "\nAfter price modification of Order 8:\n";

    modified_order = book.find_order(8);

    if (modified_order != nullptr) {

        std::cout
            << "Order 8 -> Price: "
            << modified_order->price
            << " | Quantity: "
            << modified_order->quantity
            << '\n';
    }

    location = book.order_map.find(8);

    if (location.has_value()) {

        std::cout
            << "Order 8 -> Price: "
            << location->price
            << " | Index: "
            << location->index
            << '\n';
    }


    // Zero quantity -> cancellation

    book.modify(7, 9900, 0);

    std::cout
        << "\nAfter setting Order 7 quantity to zero:\n";

    location = book.order_map.find(7);

    if (!location.has_value()) {

        std::cout
            << "Order 7 successfully cancelled\n";
    }
    else {

        std::cout
            << "ERROR: Order 7 still exists\n";
    }


    // ============================================================
    // CHAPTER 5 — EVENT-DRIVEN SIMULATION
    // ============================================================

    std::cout << "\n\n";
    std::cout << "============================================================\n";
    std::cout << "        CHAPTER 5 — EVENT-DRIVEN SIMULATION\n";
    std::cout << "============================================================\n";


    // ------------------------------------------------------------
    // 10. Create a fresh Simulator
    // ------------------------------------------------------------

    Simulator simulator;


    // ------------------------------------------------------------
    // 11. Create initial ADD events
    // ------------------------------------------------------------

    std::cout << "\n===== ADD EVENTS =====\n";

    Event event1{};
    event1.type = EventType::ADD;
    event1.timestamp = 100;
    event1.sequence = 0;
    event1.order = {101, Side::BUY, 10000, 100};

    Event event2{};
    event2.type = EventType::ADD;
    event2.timestamp = 100;
    event2.sequence = 1;
    event2.order = {102, Side::BUY, 10050, 150};

    Event event3{};
    event3.type = EventType::ADD;
    event3.timestamp = 100;
    event3.sequence = 2;
    event3.order = {103, Side::SELL, 10550, 200};

    Event event4{};
    event4.type = EventType::ADD;
    event4.timestamp = 100;
    event4.sequence = 3;
    event4.order = {104, Side::SELL, 10500, 50};

    simulator.add_event(event1);
    simulator.add_event(event2);
    simulator.add_event(event3);
    simulator.add_event(event4);


    // ------------------------------------------------------------
    // 12. CANCEL event
    // ------------------------------------------------------------

    std::cout << "\n===== CANCEL EVENT =====\n";

    Event cancel_event{};
    cancel_event.type = EventType::CANCEL;
    cancel_event.timestamp = 101;
    cancel_event.sequence = 0;
    cancel_event.order_id = 102;

    simulator.add_event(cancel_event);


    // ------------------------------------------------------------
    // 13. MODIFY event
    // ------------------------------------------------------------

    std::cout << "\n===== MODIFY EVENT =====\n";

    Event modify_event{};
    modify_event.type = EventType::MODIFY;
    modify_event.timestamp = 101;
    modify_event.sequence = 1;
    modify_event.order_id = 103;
    modify_event.new_price = 10600;
    modify_event.new_quantity = 150;

    simulator.add_event(modify_event);


    // ------------------------------------------------------------
    // 14. Aggressive BUY event
    // ------------------------------------------------------------

    std::cout << "\n===== AGGRESSIVE BUY EVENT =====\n";

    Event buy_event{};
    buy_event.type = EventType::ADD;
    buy_event.timestamp = 102;
    buy_event.sequence = 0;
    buy_event.order = {105, Side::BUY, 10600, 120};

    simulator.add_event(buy_event);


    // ------------------------------------------------------------
    // 15. Aggressive SELL event
    // ------------------------------------------------------------

    std::cout << "\n===== AGGRESSIVE SELL EVENT =====\n";

    Event sell_event{};
    sell_event.type = EventType::ADD;
    sell_event.timestamp = 103;
    sell_event.sequence = 0;
    sell_event.order = {106, Side::SELL, 10000, 80};

    simulator.add_event(sell_event);


    // ------------------------------------------------------------
    // 16. Process entire event stream
    // ------------------------------------------------------------

    std::cout << "\n===== PROCESSING EVENT STREAM =====\n";

    simulator.process_events();


    // ------------------------------------------------------------
    // 17. Print final simulated book
    // ------------------------------------------------------------

    std::cout << "\n===== FINAL SIMULATED BOOK =====\n";

    std::cout
        << "Best Bid: "
        << simulator.order_book.best_bid()
        << '\n';

    std::cout
        << "Best Ask: "
        << simulator.order_book.best_ask()
        << '\n';

    std::cout
        << "Spread: "
        << simulator.order_book.spread()
        << '\n';


    // ------------------------------------------------------------
    // 18. Print generated trades
    // ------------------------------------------------------------

    std::cout << "\n===== GENERATED TRADES =====\n";

    for (const Trade& trade : simulator.trades) {

        std::cout
            << "Incoming Order: "
            << trade.incoming_order
            << " | Resting Order: "
            << trade.resting_order
            << " | Price: "
            << trade.price
            << " | Quantity: "
            << trade.quantity
            << '\n';
    }


    // ------------------------------------------------------------
    // 19. Print simulation clock
    // ------------------------------------------------------------

    std::cout << "\n===== SIMULATION STATE =====\n";

    std::cout
        << "Current Timestamp: "
        << simulator.current_time
        << '\n';

    std::cout
        << "Last Sequence: "
        << simulator.last_sequence
        << '\n';


    // ============================================================
    // CHAPTER 6 — MARKET MICROSTRUCTURE METRICS
    // ============================================================

    std::cout << "\n\n";
    std::cout << "============================================================\n";
    std::cout << "       CHAPTER 6 — MARKET MICROSTRUCTURE METRICS\n";
    std::cout << "============================================================\n";

    MarketMetrics metrics = calculate_metrics(
        simulator.order_book,
        simulator.trades
    );

    std::cout << "\n===== MARKET METRICS =====\n";

    std::cout << "Best Bid: "
              << metrics.best_bid << '\n';

    std::cout << "Best Ask: "
              << metrics.best_ask << '\n';

    std::cout << "Mid Price: "
              << metrics.mid_price << '\n';

    std::cout << "Spread: "
              << metrics.spread << '\n';

    std::cout << "Relative Spread: "
              << metrics.relative_spread << "%\n";

    std::cout << "Bid Depth: "
              << metrics.bid_depth << '\n';

    std::cout << "Ask Depth: "
              << metrics.ask_depth << '\n';

    std::cout << "Order Book Imbalance: "
              << metrics.imbalance << '\n';

    std::cout << "Trade Count: "
              << metrics.trade_count << '\n';

    std::cout << "Total Traded Volume: "
              << metrics.trade_volume << '\n';


    // ============================================================
    // CHAPTER 7 — EXECUTION ANALYSIS
    // ============================================================

    std::cout << "\n\n";
    std::cout << "============================================================\n";
    std::cout << "          CHAPTER 7 — EXECUTION ANALYSIS\n";
    std::cout << "============================================================\n";


    // ------------------------------------------------------------
    // 20. Calculate execution results
    // ------------------------------------------------------------

    ExecutionResultsByOrder execution_results =
        simulator.calculate_execution_results();


    // ------------------------------------------------------------
    // 21. Print execution results
    // ------------------------------------------------------------

    std::cout << "\n===== EXECUTION RESULTS =====\n";

    for (const auto& entry : execution_results) {

        OrderId order_id = entry.first;
        const ExecutionResult& result = entry.second;

        Side side = simulator.sides_by_order.at(order_id);

        std::cout
            << "\nIncoming Order: "
            << order_id
            << '\n';

        std::cout
            << "Side: "
            << (side == Side::BUY ? "BUY" : "SELL")
            << '\n';

        std::cout
            << "Executed Quantity: "
            << result.executed_quantity
            << '\n';

        std::cout
            << "Execution Value: "
            << result.execution_value
            << '\n';

        std::cout
            << "Execution VWAP: "
            << result.execution_vwap
            << '\n';

        std::cout
            << "Arrival Reference Price: "
            << result.reference_price
            << '\n';

        std::cout
            << "Slippage: "
            << result.slippage
            << '\n';

        std::cout
            << "Execution Cost: "
            << result.execution_cost
            << '\n';

        std::cout
            << "Liquidity Consumed: "
            << result.liquidity_consumed
            << '\n';
    }


    // ------------------------------------------------------------
    // 22. Verify orders with no executions
    // ------------------------------------------------------------

    std::cout << "\n===== EXECUTION CONTEXT =====\n";

    for (const auto& entry : simulator.sides_by_order) {

        OrderId order_id = entry.first;

        Side side = entry.second;

        std::cout
            << "Order "
            << order_id
            << " | Side: "
            << (side == Side::BUY ? "BUY" : "SELL")
            << " | Arrival Midpoint: "
            << simulator.reference_prices_by_order.at(order_id)
            << '\n';
    }


    return 0;
}