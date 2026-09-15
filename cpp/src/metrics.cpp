#include <metrics.hpp>
#include <order_book.hpp>

Price calculate_best_bid(const OrderBook& book) {
    return book.best_bid();
}

Price calculate_best_ask(const OrderBook& book) {
    return book.best_ask();
}

MidPrice calculate_mid_price(const OrderBook& book) {
    Price bid = book.best_bid();
    Price ask = book.best_ask();

    if (bid == 0 || ask == 0) {
        return 0.0;
    }

    return (static_cast<MidPrice>(bid) + static_cast<MidPrice>(ask)) / 2;
}

Price calculate_spread(const OrderBook& book) {
    return book.spread();
}

RelativeSpread calculate_relative_spread(const OrderBook& book) {
    MidPrice mid = calculate_mid_price(book);

    if (mid == 0.0) {
        return 0.0;
    }

    return (book.best_ask() - book.best_bid()) / mid * 100; 
}

Quantity calculate_bid_depth(const OrderBook& book) {
    Quantity total = 0;

    for (const PriceLevel& level : book.bid_levels()) {
        total += level.total_quantity;
    }

    return total;
}

Quantity calculate_ask_depth(const OrderBook& book) {
    Quantity total = 0;

    for (const PriceLevel& level : book.ask_levels()) {
        total += level.total_quantity;
    }

    return total;
}

Imbalance calculate_imbalance(const OrderBook& book) {
    Quantity bid_depth = calculate_bid_depth(book);
    Quantity ask_depth = calculate_ask_depth(book);

    Quantity total_depth = bid_depth + ask_depth;

    if (total_depth == 0) {
        return 0.0;
    }

    return static_cast<Imbalance>((static_cast<double>(bid_depth) - static_cast<double>(ask_depth)) / static_cast<double>(total_depth));
}

TradeCount calculate_trade_count(const std::vector<Trade>& trades) {
    return trades.size();
}

Quantity calculate_trade_volume(const std::vector<Trade>& trades) {
    Quantity total = 0;
    
    for (const Trade& trade : trades) {
        total += trade.quantity;
    }

    return total;
}

MarketMetrics calculate_metrics(const OrderBook& book, const std::vector<Trade>& trades) {
    MarketMetrics metrics;

    metrics.best_bid = calculate_best_bid(book);
    metrics.best_ask = calculate_best_ask(book);

    metrics.mid_price = calculate_mid_price(book);
    metrics.spread = calculate_spread(book);
    metrics.relative_spread = calculate_relative_spread(book);

    metrics.bid_depth = calculate_bid_depth(book);
    metrics.ask_depth = calculate_ask_depth(book);
    metrics.imbalance = calculate_imbalance(book);

    metrics.trade_count = calculate_trade_count(trades);
    metrics.trade_volume = calculate_trade_volume(trades);

    return metrics;
}