#include <iostream>
#include <string>

#include <types.hpp>
#include <order.hpp>
#include <order_book.hpp>

void check(bool condition, const std::string& message) {
    if (!condition) {
        std::cerr << "TEST FAILED: " << message << std::endl;
        std::exit(1);
    }
}

void test_buy_orders() {
    OrderBook book;

    Order order1 = {1, Side::BUY, 8000, 100};
    Order order3 = {3, Side::BUY, 7950, 180};
    Order order7 = {7, Side::BUY, 7800, 50};
    Order order10 = {10, Side::BUY, 8150, 200};

    book.add(order1);
    book.add(order3);
    book.add(order7);
    book.add(order10);

    check(book.bids.size() == 4, "BUY orders should create four price levels");
    check(book.best_bid() == 8150, "Best bid should be 8150");
}

void test_sell_orders() {
    OrderBook book;

    Order order2 = {2, Side::SELL, 8050, 100};
    Order order5 = {5, Side::SELL, 8100, 120};
    Order order8 = {8, Side::SELL, 8350, 230};

    book.add(order2);
    book.add(order5);
    book.add(order8);

    check(book.asks.size() == 3, "SELL orders should create three price levels");
    check(book.best_ask() == 8050, "Best ask should be 8050");
}

void test_same_price_buy_orders() {
    OrderBook book;

    Order order1 = {1, Side::BUY, 8000, 100};
    Order order6 = {6, Side::BUY, 8000, 130};

    book.add(order1);
    book.add(order6);

    check(book.bids.size() == 1, "Same-price BUY orders should share one price level");
    check(book.bids[0].price == 8000, "Price level should be 8000");
    check(book.bids[0].orders.size() == 2, "Price level should contain two orders");
    check(book.bids[0].total_quantity == 230,
          "Total quantity should equal 230");
}

void test_same_price_sell_orders() {
    OrderBook book;

    Order order2 = {2, Side::SELL, 8050, 100};
    Order order4 = {4, Side::SELL, 8050, 30};

    book.add(order2);
    book.add(order4);

    check(book.asks.size() == 1, "Same-price SELL orders should share one price level");
    check(book.asks[0].price == 8050, "Price level should be 8050");
    check(book.asks[0].orders.size() == 2, "Price level should contain two orders");
    check(book.asks[0].total_quantity == 130,
          "Total quantity should equal 130");
}

void test_best_prices() {
    OrderBook book;

    Order bid1 = {1, Side::BUY, 8000, 100};
    Order bid2 = {2, Side::BUY, 8050, 150};

    Order ask1 = {3, Side::SELL, 8100, 100};
    Order ask2 = {4, Side::SELL, 8150, 200};

    book.add(bid1);
    book.add(bid2);
    book.add(ask1);
    book.add(ask2);

    check(book.best_bid() == 8050, "Best bid should be 8050");
    check(book.best_ask() == 8100, "Best ask should be 8100");
}

void test_spread() {
    OrderBook book;

    Order bid = {1, Side::BUY, 8050, 100};
    Order ask = {2, Side::SELL, 8100, 100};

    book.add(bid);
    book.add(ask);

    check(book.spread() == 50, "Spread should be 50 ticks");
}

void test_cancellation() {
    OrderBook book;

    Order order1 = {1, Side::BUY, 8000, 100};
    Order order6 = {6, Side::BUY, 8000, 130};
    Order order10 = {10, Side::BUY, 8150, 200};

    book.add(order1);
    book.add(order6);
    book.add(order10);

    check(book.best_bid() == 8150,
          "Best bid should initially be 8150");

    book.cancel(10);

    check(book.best_bid() == 8000,
          "Best bid should become 8000 after cancelling order 10");
}

void test_quantity_after_cancellation() {
    OrderBook book;

    Order order1 = {1, Side::BUY, 8000, 100};
    Order order6 = {6, Side::BUY, 8000, 130};

    book.add(order1);
    book.add(order6);

    check(book.bids.size() == 1,
          "Both orders should share one price level");

    check(book.bids[0].total_quantity == 230,
          "Initial total quantity should be 230");

    book.cancel(1);

    check(book.bids.size() == 1,
          "Price level should remain after cancelling one order");

    check(book.bids[0].total_quantity == 130,
          "Total quantity should become 130");

    check(book.bids[0].orders.size() == 1,
          "One order should remain");
}

void test_empty_price_level_removal() {
    OrderBook book;

    Order order1 = {1, Side::BUY, 8000, 100};
    Order order2 = {2, Side::BUY, 8150, 200};

    book.add(order1);
    book.add(order2);

    check(book.bids.size() == 2,
          "Book should initially contain two bid levels");

    book.cancel(1);

    check(book.bids.size() == 1,
          "Empty price level should be removed");

    check(book.best_bid() == 8150,
          "Remaining best bid should be 8150");

    book.cancel(2);

    check(book.bids.empty(),
          "All bid levels should be removed");
}

void test_empty_book() {
    OrderBook book;

    check(book.bids.empty(),
          "New book should have no bid levels");

    check(book.asks.empty(),
          "New book should have no ask levels");
}

void test_nonexistent_cancellation() {
    OrderBook book;

    Order order = {1, Side::BUY, 8000, 100};

    book.add(order);

    book.cancel(999);

    check(book.bids.size() == 1,
          "Invalid cancellation should not remove a valid price level");

    check(book.bids[0].orders.size() == 1,
          "Invalid cancellation should not remove an order");

    check(book.bids[0].total_quantity == 100,
          "Invalid cancellation should not change quantity");

    check(book.best_bid() == 8000,
          "Invalid cancellation should not change best bid");
}

int main() {
    test_buy_orders();
    test_sell_orders();
    test_same_price_buy_orders();
    test_same_price_sell_orders();
    test_best_prices();
    test_spread();
    test_cancellation();
    test_quantity_after_cancellation();
    test_empty_price_level_removal();
    test_empty_book();
    test_nonexistent_cancellation();

    std::cout << "All OrderBook tests passed." << std::endl;

    return 0;
}