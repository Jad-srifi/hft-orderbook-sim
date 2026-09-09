#include <types.hpp>
#include <order_book.hpp>
#include <order.hpp>
#include <trade.hpp>

#include <iostream>
#include <vector>

int main() {

    OrderBook book;

    // Initial resting orders
    Order order1 = {1, Side::BUY, 10000, 50};
    Order order2 = {2, Side::BUY, 10050, 150};
    Order order3 = {3, Side::SELL, 10550, 230};
    Order order4 = {4, Side::SELL, 10500, 20};

    book.add(order1);
    book.add(order2);
    book.add(order3);
    book.add(order4);

    std::cout << "Initial Book\n";
    std::cout << "Best Ask: " << book.best_ask() << std::endl;
    std::cout << "Best Bid: " << book.best_bid() << std::endl;
    std::cout << "Spread: " << book.spread() << std::endl;

    // Crossing BUY order
    Order buy_order = {5, Side::BUY, 10550, 100};

    std::vector<Trade> buy_trades = book.process_order(buy_order);

    std::cout << "\nBUY Order 5 Executed\n";

    for (const Trade& trade : buy_trades) {
        std::cout
            << "Trade: Incoming Order " << trade.incoming_order
            << " | Resting Order " << trade.resting_order
            << " | Price " << trade.price
            << " | Quantity " << trade.quantity
            << std::endl;
    }

    std::cout << "Remaining BUY quantity: "
              << buy_order.quantity << std::endl;

    std::cout << "Best Ask: " << book.best_ask() << std::endl;
    std::cout << "Best Bid: " << book.best_bid() << std::endl;
    std::cout << "Spread: " << book.spread() << std::endl;

    // Crossing SELL order
    Order sell_order = {6, Side::SELL, 10000, 100};

    std::vector<Trade> sell_trades = book.process_order(sell_order);

    std::cout << "\nSELL Order 6 Executed\n";

    for (const Trade& trade : sell_trades) {
        std::cout
            << "Trade: Incoming Order " << trade.incoming_order
            << " | Resting Order " << trade.resting_order
            << " | Price " << trade.price
            << " | Quantity " << trade.quantity
            << std::endl;
    }

    std::cout << "Remaining SELL quantity: "
              << sell_order.quantity << std::endl;

    std::cout << "Best Ask: " << book.best_ask() << std::endl;
    std::cout << "Best Bid: " << book.best_bid() << std::endl;
    std::cout << "Spread: " << book.spread() << std::endl;

    return 0;
}
