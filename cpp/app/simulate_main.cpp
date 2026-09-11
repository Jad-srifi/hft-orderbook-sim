#include <types.hpp>
#include <order_book.hpp>
#include <order.hpp>
#include <trade.hpp>

#include <iostream>
#include <vector>

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

    std::cout << "Best Ask: " << book.best_ask() << '\n';
    std::cout << "Best Bid: " << book.best_bid() << '\n';
    std::cout << "Spread: " << book.spread() << '\n';


    // ------------------------------------------------------------
    // 2. Test OrderMap lookup
    // ------------------------------------------------------------

    std::cout << "\n===== ORDER MAP LOOKUP =====\n";

    std::optional<OrderLocation> location = book.order_map.find(2);

    if (location.has_value()) {

        std::cout
            << "Order 2 found\n"
            << "Side: " << (location->side == Side::BUY ? "BUY" : "SELL") << '\n'
            << "Price: " << location->price << '\n'
            << "Index: " << location->index << '\n';
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
        std::cout << "Order 2 successfully removed from OrderMap\n";
    }
    else {
        std::cout << "ERROR: Order 2 still exists in OrderMap\n";
    }

    std::cout << "Best Bid after cancellation: "
              << book.best_bid() << '\n';


    // ------------------------------------------------------------
    // 4. Verify shifted OrderMap index
    // ------------------------------------------------------------

    std::cout << "\n===== SHIFTED INDEX CHECK =====\n";

    location = book.order_map.find(1);

    if (location.has_value()) {

        std::cout
            << "Order 1 found after cancellation\n"
            << "Price: " << location->price << '\n'
            << "Index: " << location->index << '\n';
    }
    else {
        std::cout << "ERROR: Order 1 missing from OrderMap\n";
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
            << "Trade: Incoming Order " << trade.incoming_order
            << " | Resting Order " << trade.resting_order
            << " | Price " << trade.price
            << " | Quantity " << trade.quantity
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
            << "Price: " << location->price
            << " | Index: " << location->index
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
            << "Price: " << location->price
            << " | Index: " << location->index
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
            << "Trade: Incoming Order " << trade.incoming_order
            << " | Resting Order " << trade.resting_order
            << " | Price " << trade.price
            << " | Quantity " << trade.quantity
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
            << "Price: " << location->price
            << " | Index: " << location->index
            << '\n';
    }
    else {
        std::cout << "Order 1 does not exist\n";
    }

    location = book.order_map.find(3);

    if (location.has_value()) {

        std::cout
            << "Order 3 exists\n"
            << "Price: " << location->price
            << " | Index: " << location->index
            << '\n';
    }
    else {
        std::cout << "Order 3 does not exist\n";
    }

    location = book.order_map.find(5);

    if (location.has_value()) {

        std::cout
            << "Order 5 exists in OrderMap\n"
            << "Price: " << location->price
            << " | Index: " << location->index
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
            << "Price: " << location->price
            << " | Index: " << location->index
            << '\n';
    }
    else {
        std::cout
            << "Order 6 is not in OrderMap "
            << "(fully executed)\n";
    }

    return 0;
}