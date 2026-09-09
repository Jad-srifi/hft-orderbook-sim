#include <iostream>
#include <string>
#include <cstdlib>
#include <vector>

#include <types.hpp>
#include <order.hpp>
#include <order_book.hpp>
#include <trade.hpp>

void check(bool condition, const std::string& message) {
    if (!condition) {
        std::cerr << "TEST FAILED: " << message << std::endl;
        std::exit(1);
    }
}

int main() {

    // =========================================================
    // TEST 1: BUY fully matches one SELL order
    // =========================================================

    {
        OrderBook book;

        Order sell = {1, Side::SELL, 10000, 50};
        book.add(sell);

        Order buy = {2, Side::BUY, 10000, 50};

        std::vector<Trade> trades = book.process_order(buy);

        check(trades.size() == 1, "BUY should generate one trade");
        check(trades[0].incoming_order == 2, "Wrong incoming order ID");
        check(trades[0].resting_order == 1, "Wrong resting order ID");
        check(trades[0].price == 10000, "Wrong trade price");
        check(trades[0].quantity == 50, "Wrong trade quantity");

        check(buy.quantity == 0, "BUY should be fully filled");
        check(book.asks.empty(), "ASK should be empty after full fill");
    }

    // =========================================================
    // TEST 2: SELL fully matches one BUY order
    // =========================================================

    {
        OrderBook book;

        Order buy = {1, Side::BUY, 10000, 50};
        book.add(buy);

        Order sell = {2, Side::SELL, 10000, 50};

        std::vector<Trade> trades = book.process_order(sell);

        check(trades.size() == 1, "SELL should generate one trade");
        check(trades[0].incoming_order == 2, "Wrong incoming order ID");
        check(trades[0].resting_order == 1, "Wrong resting order ID");
        check(trades[0].price == 10000, "Wrong trade price");
        check(trades[0].quantity == 50, "Wrong trade quantity");

        check(sell.quantity == 0, "SELL should be fully filled");
        check(book.bids.empty(), "BID should be empty after full fill");
    }

    // =========================================================
    // TEST 3: BUY partial fill against larger resting SELL
    // =========================================================

    {
        OrderBook book;

        Order sell = {1, Side::SELL, 10000, 100};
        book.add(sell);

        Order buy = {2, Side::BUY, 10000, 40};

        std::vector<Trade> trades = book.process_order(buy);

        check(trades.size() == 1, "Should generate one trade");
        check(trades[0].quantity == 40, "Wrong partial fill quantity");

        check(buy.quantity == 0, "BUY should be fully filled");
        check(book.asks.size() == 1, "ASK level should remain");
        check(book.asks[0].orders[0].quantity == 60,
              "Resting SELL should have 60 remaining");
    }

    // =========================================================
    // TEST 4: SELL partial fill against larger resting BUY
    // =========================================================

    {
        OrderBook book;

        Order buy = {1, Side::BUY, 10000, 100};
        book.add(buy);

        Order sell = {2, Side::SELL, 10000, 40};

        std::vector<Trade> trades = book.process_order(sell);

        check(trades.size() == 1, "Should generate one trade");
        check(trades[0].quantity == 40, "Wrong partial fill quantity");

        check(sell.quantity == 0, "SELL should be fully filled");
        check(book.bids.size() == 1, "BID level should remain");
        check(book.bids[0].orders[0].quantity == 60,
              "Resting BUY should have 60 remaining");
    }

    // =========================================================
    // TEST 5: BUY consumes multiple orders with FIFO
    // =========================================================

    {
        OrderBook book;

        Order sell1 = {1, Side::SELL, 10000, 30};
        Order sell2 = {2, Side::SELL, 10000, 40};

        book.add(sell1);
        book.add(sell2);

        Order buy = {3, Side::BUY, 10000, 60};

        std::vector<Trade> trades = book.process_order(buy);

        check(trades.size() == 2,
              "BUY should generate two trades");

        check(trades[0].resting_order == 1,
              "FIFO: first resting order should execute first");

        check(trades[0].quantity == 30,
              "First trade quantity incorrect");

        check(trades[1].resting_order == 2,
              "FIFO: second resting order should execute second");

        check(trades[1].quantity == 30,
              "Second trade quantity incorrect");

        check(buy.quantity == 0,
              "BUY should be fully filled");

        check(book.asks.size() == 1,
              "One price level should remain");

        check(book.asks[0].orders[0].id == 2,
              "Order 2 should remain");

        check(book.asks[0].orders[0].quantity == 10,
              "Order 2 should have 10 remaining");
    }

    // =========================================================
    // TEST 6: BUY consumes multiple price levels
    // =========================================================

    {
        OrderBook book;

        Order sell1 = {1, Side::SELL, 10000, 20};
        Order sell2 = {2, Side::SELL, 10100, 30};
        Order sell3 = {3, Side::SELL, 10200, 40};

        book.add(sell1);
        book.add(sell2);
        book.add(sell3);

        Order buy = {4, Side::BUY, 10200, 60};

        std::vector<Trade> trades = book.process_order(buy);

        check(trades.size() == 3,
              "BUY should execute across three price levels");

        check(trades[0].price == 10000,
              "First trade should execute at lowest ASK");

        check(trades[1].price == 10100,
              "Second trade should execute at next ASK");

        check(trades[2].price == 10200,
              "Third trade should execute at final ASK");

        check(trades[0].quantity == 20,
              "Wrong first trade quantity");

        check(trades[1].quantity == 30,
              "Wrong second trade quantity");

        check(trades[2].quantity == 10,
              "Wrong third trade quantity");

        check(buy.quantity == 0,
              "BUY should be fully filled");

        check(book.asks.size() == 1,
              "Only final price level should remain");

        check(book.asks[0].price == 10200,
              "Wrong remaining price level");

        check(book.asks[0].orders[0].quantity == 30,
              "Wrong remaining quantity");
    }

    // =========================================================
    // TEST 7: BUY partially fills then rests remaining quantity
    // =========================================================

    {
        OrderBook book;

        Order sell = {1, Side::SELL, 10000, 30};
        book.add(sell);

        Order buy = {2, Side::BUY, 10000, 50};

        std::vector<Trade> trades = book.process_order(buy);

        check(trades.size() == 1,
              "Should generate one trade");

        check(trades[0].quantity == 30,
              "Trade should consume all 30 resting quantity");

        check(buy.quantity == 20,
              "BUY should have 20 remaining");

        check(book.bids.size() == 1,
              "Remaining BUY should rest on book");

        check(book.bids[0].orders[0].id == 2,
              "Remaining BUY should be order 2");

        check(book.bids[0].orders[0].quantity == 20,
              "Remaining BUY quantity should be 20");
    }

    // =========================================================
    // TEST 8: SELL partially fills then rests remaining quantity
    // =========================================================

    {
        OrderBook book;

        Order buy = {1, Side::BUY, 10000, 30};
        book.add(buy);

        Order sell = {2, Side::SELL, 10000, 50};

        std::vector<Trade> trades = book.process_order(sell);

        check(trades.size() == 1,
              "Should generate one trade");

        check(trades[0].quantity == 30,
              "Trade should consume all 30 resting quantity");

        check(sell.quantity == 20,
              "SELL should have 20 remaining");

        check(book.asks.size() == 1,
              "Remaining SELL should rest on book");

        check(book.asks[0].orders[0].id == 2,
              "Remaining SELL should be order 2");

        check(book.asks[0].orders[0].quantity == 20,
              "Remaining SELL quantity should be 20");
    }

    // =========================================================
    // TEST 9: Non-crossing BUY should not trade
    // =========================================================

    {
        OrderBook book;

        Order sell = {1, Side::SELL, 10500, 50};
        book.add(sell);

        Order buy = {2, Side::BUY, 10000, 50};

        std::vector<Trade> trades = book.process_order(buy);

        check(trades.empty(),
              "Non-crossing BUY should generate no trades");

        check(book.bids.size() == 1,
              "Non-crossing BUY should rest on book");

        check(book.bids[0].orders[0].id == 2,
              "BUY should remain on book");

        check(book.bids[0].orders[0].quantity == 50,
              "BUY quantity should remain unchanged");
    }

    // =========================================================
    // TEST 10: Non-crossing SELL should not trade
    // =========================================================

    {
        OrderBook book;

        Order buy = {1, Side::BUY, 10000, 50};
        book.add(buy);

        Order sell = {2, Side::SELL, 10500, 50};

        std::vector<Trade> trades = book.process_order(sell);

        check(trades.empty(),
              "Non-crossing SELL should generate no trades");

        check(book.asks.size() == 1,
              "Non-crossing SELL should rest on book");

        check(book.asks[0].orders[0].id == 2,
              "SELL should remain on book");

        check(book.asks[0].orders[0].quantity == 50,
              "SELL quantity should remain unchanged");
    }

    // =========================================================
    // TEST 11: SELL consumes multiple price levels
    // =========================================================

    {
        OrderBook book;

        Order buy1 = {1, Side::BUY, 10200, 20};
        Order buy2 = {2, Side::BUY, 10100, 30};
        Order buy3 = {3, Side::BUY, 10000, 40};

        book.add(buy1);
        book.add(buy2);
        book.add(buy3);

        Order sell = {4, Side::SELL, 10000, 60};

        std::vector<Trade> trades = book.process_order(sell);

        check(trades.size() == 3,
              "SELL should execute across three price levels");

        check(trades[0].price == 10200,
              "First trade should execute at highest BID");

        check(trades[1].price == 10100,
              "Second trade should execute at next BID");

        check(trades[2].price == 10000,
              "Third trade should execute at final BID");

        check(trades[0].quantity == 20,
              "Wrong first trade quantity");

        check(trades[1].quantity == 30,
              "Wrong second trade quantity");

        check(trades[2].quantity == 10,
              "Wrong third trade quantity");

        check(sell.quantity == 0,
              "SELL should be fully filled");

        check(book.bids.size() == 1,
              "Only final BID level should remain");

        check(book.bids[0].price == 10000,
              "Wrong remaining BID price");

        check(book.bids[0].orders[0].quantity == 30,
              "Wrong remaining BID quantity");
    }

    std::cout << "All Matching Engine tests passed." << std::endl;

    return 0;
}
