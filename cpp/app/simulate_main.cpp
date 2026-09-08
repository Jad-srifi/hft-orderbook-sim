#include <types.hpp>
#include <order_book.hpp>
#include <order.hpp>
#include <iostream>


int main() {
    OrderBook book;

    Order order1 = {1, Side::BUY, 10000, 50};
    Order order2 = {2, Side::BUY, 10050, 150};

    Order order3 = {3, Side::SELL, 10550, 230};
    Order order4 = {4, Side::SELL, 10500, 20};

    book.add(order1);
    book.add(order2);

    book.add(order3);
    book.add(order4);

    Price b_a = book.best_ask();
    Price b_b = book.best_bid();

    Price spread = book.spread();

    std::cout << "Best Ask: "<< b_a << std::endl;
    std::cout << "Best Bid: " << b_b << std::endl;
    std::cout << "Spread: " << spread << std::endl;

    book.cancel(2);

    b_a = book.best_ask();
    b_b = book.best_bid();

    spread = book.spread();

    std::cout << "Best Ask: "<< b_a << std::endl;
    std::cout << "Best Bid: " << b_b << std::endl;
    std::cout << "Spread: " << spread << std::endl;
       
}
