#include <vector>
#include <trade.hpp>
#include <types.hpp>
#include <order_book.hpp>

struct MarketMetrics {
    Price best_bid;
    Price best_ask;

    MidPrice mid_price;
    Price spread;
    RelativeSpread relative_spread;
    
    Quantity bid_depth;
    Quantity ask_depth;
    Imbalance imbalance;

    TradeCount trade_count;
    Quantity trade_volume;
};

Price calculate_best_bid(const OrderBook& book);
Price calculate_best_ask(const OrderBook& book);

MidPrice calculate_mid_price(const OrderBook& book);
Price calculate_spread(const OrderBook& book);
RelativeSpread calculate_relative_spread(const OrderBook& book);

Quantity calculate_bid_depth(const OrderBook& book);
Quantity calculate_ask_depth(const OrderBook& book);
Imbalance calculate_imbalance(const OrderBook& book);

TradeCount calculate_trade_count(const std::vector<Trade>& trades);
Quantity calculate_trade_volume(const std::vector<Trade>& trades);

MarketMetrics calculate_metrics(const OrderBook& book, const std::vector<Trade>& trades);