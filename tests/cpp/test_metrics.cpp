#include <metrics.hpp>
#include <order_book.hpp>
#include <order.hpp>
#include <trade.hpp>

#include <cassert>
#include <cmath>
#include <iostream>
#include <vector>

int main() {

    // ============================================================
    // BEST BID / BEST ASK
    // ============================================================

    {
        OrderBook book;

        book.add(Order{1, Side::BUY, 10000, 100});
        book.add(Order{2, Side::BUY, 10100, 50});
        book.add(Order{3, Side::SELL, 10500, 80});
        book.add(Order{4, Side::SELL, 10600, 40});

        assert(calculate_best_bid(book) == 10100);
        assert(calculate_best_ask(book) == 10500);
    }


    // ============================================================
    // MID PRICE
    // ============================================================

    {
        OrderBook book;

        book.add(Order{1, Side::BUY, 10000, 100});
        book.add(Order{2, Side::SELL, 10500, 100});

        assert(calculate_mid_price(book) == 10250.0);
    }


    // ============================================================
    // HALF-TICK MID PRICE
    // ============================================================

    {
        OrderBook book;

        book.add(Order{1, Side::BUY, 10000, 100});
        book.add(Order{2, Side::SELL, 10501, 100});

        assert(calculate_mid_price(book) == 10250.5);
    }


    // ============================================================
    // SPREAD
    // ============================================================

    {
        OrderBook book;

        book.add(Order{1, Side::BUY, 10000, 100});
        book.add(Order{2, Side::SELL, 10500, 100});

        assert(calculate_spread(book) == 500);
    }


    // ============================================================
    // RELATIVE SPREAD
    // ============================================================

    {
        OrderBook book;

        book.add(Order{1, Side::BUY, 10000, 100});
        book.add(Order{2, Side::SELL, 10500, 100});

        double expected = (500.0 / 10250.0) * 100.0;

        assert(std::abs(calculate_relative_spread(book) - expected) < 1e-9);
    }


    // ============================================================
    // BID DEPTH
    // ============================================================

    {
        OrderBook book;

        book.add(Order{1, Side::BUY, 10000, 100});
        book.add(Order{2, Side::BUY, 10000, 50});
        book.add(Order{3, Side::BUY, 9900, 75});

        assert(calculate_bid_depth(book) == 225);
    }


    // ============================================================
    // ASK DEPTH
    // ============================================================

    {
        OrderBook book;

        book.add(Order{1, Side::SELL, 10500, 100});
        book.add(Order{2, Side::SELL, 10500, 50});
        book.add(Order{3, Side::SELL, 10600, 75});

        assert(calculate_ask_depth(book) == 225);
    }


    // ============================================================
    // ORDER BOOK IMBALANCE
    // ============================================================

    {
        OrderBook book;

        book.add(Order{1, Side::BUY, 10000, 100});
        book.add(Order{2, Side::SELL, 10500, 50});

        // (100 - 50) / (100 + 50) = 1/3
        double expected = 1.0 / 3.0;

        assert(std::abs(calculate_imbalance(book) - expected) < 1e-9);
    }


    // ============================================================
    // ZERO DEPTH / EMPTY BOOK
    // ============================================================

    {
        OrderBook book;

        assert(calculate_bid_depth(book) == 0);
        assert(calculate_ask_depth(book) == 0);
        assert(calculate_imbalance(book) == 0.0);
        assert(calculate_mid_price(book) == 0.0);
        assert(calculate_relative_spread(book) == 0.0);
    }


    // ============================================================
    // MISSING BID
    // ============================================================

    {
        OrderBook book;

        book.add(Order{1, Side::SELL, 10500, 100});

        assert(calculate_best_bid(book) == 0);
        assert(calculate_best_ask(book) == 10500);
        assert(calculate_mid_price(book) == 0.0);
        assert(calculate_relative_spread(book) == 0.0);
    }


    // ============================================================
    // MISSING ASK
    // ============================================================

    {
        OrderBook book;

        book.add(Order{1, Side::BUY, 10000, 100});

        assert(calculate_best_bid(book) == 10000);
        assert(calculate_best_ask(book) == 0);
        assert(calculate_mid_price(book) == 0.0);
        assert(calculate_relative_spread(book) == 0.0);
    }


    // ============================================================
    // TRADE COUNT
    // ============================================================

    {
        std::vector<Trade> trades = {
            Trade{1, 2, 10500, 50},
            Trade{3, 4, 10600, 70},
            Trade{5, 6, 10400, 30}
        };

        assert(calculate_trade_count(trades) == 3);
    }


    // ============================================================
    // TRADE VOLUME
    // ============================================================

    {
        std::vector<Trade> trades = {
            Trade{1, 2, 10500, 50},
            Trade{3, 4, 10600, 70},
            Trade{5, 6, 10400, 30}
        };

        assert(calculate_trade_volume(trades) == 150);
    }


    // ============================================================
    // EMPTY TRADE HISTORY
    // ============================================================

    {
        std::vector<Trade> trades;

        assert(calculate_trade_count(trades) == 0);
        assert(calculate_trade_volume(trades) == 0);
    }


    // ============================================================
    // MULTIPLE TRADES
    // ============================================================

    {
        std::vector<Trade> trades = {
            Trade{10, 1, 10000, 25},
            Trade{10, 2, 10050, 35},
            Trade{11, 3, 10100, 40},
            Trade{12, 4, 10150, 10}
        };

        assert(calculate_trade_count(trades) == 4);
        assert(calculate_trade_volume(trades) == 110);
    }


    // ============================================================
    // PARTIAL-FILL STYLE TRADE HISTORY
    // ============================================================

    {
        std::vector<Trade> trades = {
            Trade{20, 1, 10500, 20},
            Trade{20, 2, 10550, 30}
        };

        assert(calculate_trade_count(trades) == 2);
        assert(calculate_trade_volume(trades) == 50);
    }


    // ============================================================
    // COMPLETE MARKET METRICS
    // ============================================================

    {
        OrderBook book;

        book.add(Order{1, Side::BUY, 10000, 20});
        book.add(Order{2, Side::SELL, 10600, 80});

        std::vector<Trade> trades = {
            Trade{105, 104, 10500, 50},
            Trade{105, 103, 10600, 70},
            Trade{106, 101, 10000, 80}
        };

        MarketMetrics metrics = calculate_metrics(book, trades);

        assert(metrics.best_bid == 10000);
        assert(metrics.best_ask == 10600);
        assert(metrics.mid_price == 10300.0);
        assert(metrics.spread == 600);

        double expected_relative_spread =
            (600.0 / 10300.0) * 100.0;

        assert(
            std::abs(
                metrics.relative_spread - expected_relative_spread
            ) < 1e-9
        );

        assert(metrics.bid_depth == 20);
        assert(metrics.ask_depth == 80);
        assert(metrics.imbalance == -0.6);
        assert(metrics.trade_count == 3);
        assert(metrics.trade_volume == 200);
    }


    // ============================================================
    // NON-MUTATION CHECK
    // ============================================================

    {
        OrderBook book;

        book.add(Order{1, Side::BUY, 10000, 100});
        book.add(Order{2, Side::BUY, 9900, 50});
        book.add(Order{3, Side::SELL, 10500, 80});
        book.add(Order{4, Side::SELL, 10600, 40});

        std::vector<Trade> trades = {
            Trade{10, 3, 10500, 20}
        };

        Price original_best_bid = book.best_bid();
        Price original_best_ask = book.best_ask();

        Quantity original_bid_depth = calculate_bid_depth(book);
        Quantity original_ask_depth = calculate_ask_depth(book);

        std::size_t original_trade_count = trades.size();
        Quantity original_trade_volume = calculate_trade_volume(trades);

        calculate_metrics(book, trades);

        assert(book.best_bid() == original_best_bid);
        assert(book.best_ask() == original_best_ask);
        assert(calculate_bid_depth(book) == original_bid_depth);
        assert(calculate_ask_depth(book) == original_ask_depth);

        assert(trades.size() == original_trade_count);
        assert(calculate_trade_volume(trades) == original_trade_volume);
    }


    // ============================================================
    // FINAL RESULT
    // ============================================================

    std::cout << "All Chapter 6 metrics tests passed!\n";

    return 0;
}