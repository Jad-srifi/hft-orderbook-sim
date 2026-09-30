#include <benchmark.hpp>

#include <event.hpp>
#include <execution.hpp>
#include <inventory_model.hpp>
#include <metrics.hpp>
#include <order_book.hpp>
#include <order.hpp>
#include <simulator.hpp>

#include <itch_file_reader.hpp>
#include <itch_mapper.hpp>
#include <itch_parser.hpp>
#include <itch_replay.hpp>

#include <cstddef>
#include <cstdint>
#include <iomanip>
#include <iostream>
#include <stdexcept>
#include <string>
#include <vector>

namespace
{
    constexpr std::size_t warmup_runs = 2;
    constexpr std::size_t measured_runs = 10;

    constexpr bool run_test_1 = true;
    constexpr bool run_test_2 = true;
    constexpr bool run_test_3 = true;

    const std::vector<std::size_t> benchmark_sizes =
        {
            100,
            1000,
            10000,
            100000};

    const std::vector<std::size_t> analysis_sizes =
        {
            1000000,
            5000000,
            10000000,
            25000000};

    constexpr const char *itch_benchmark_file =
        "data/itch/2019-10-18/raw/decompressed/S101819-v50.txt";

    constexpr StockLocate itch_benchmark_stock_locate = 123;

    constexpr std::size_t itch_max_messages =
        302347067ULL;

    BenchmarkConfig benchmark_config()
    {
        BenchmarkConfig config;

        config.warmup_run_count =
            warmup_runs;

        config.measured_run_count =
            measured_runs;

        return config;
    }

    BenchmarkConfig analysis_benchmark_config(
        std::size_t size)
    {
        BenchmarkConfig config;

        if (size >= 25000000ULL)
        {
            config.warmup_run_count = 0;
            config.measured_run_count = 1;
        }
        else if (size >= 10000000ULL)
        {
            config.warmup_run_count = 1;
            config.measured_run_count = 2;
        }
        else
        {
            config.warmup_run_count = 1;
            config.measured_run_count = 3;
        }

        return config;
    }

    BenchmarkConfig replay_benchmark_config(
        std::size_t size)
    {
        BenchmarkConfig config;

        if (size >= 100000000ULL)
        {
            config.warmup_run_count = 0;
            config.measured_run_count = 1;
        }
        else
        {
            config.warmup_run_count = 1;
            config.measured_run_count = 3;
        }

        return config;
    }

    Order make_buy_order(
        OrderId id,
        Price price,
        Quantity quantity)
    {
        return Order{
            id,
            Side::BUY,
            price,
            quantity};
    }

    Order make_sell_order(
        OrderId id,
        Price price,
        Quantity quantity)
    {
        return Order{
            id,
            Side::SELL,
            price,
            quantity};
    }

    Event make_add_event(
        OrderId id,
        Side side,
        Price price,
        Quantity quantity,
        Timestamp timestamp,
        SequenceNumber sequence)
    {
        return Event{
            EventType::ADD,
            timestamp,
            sequence,
            Order{
                id,
                side,
                price,
                quantity},
            0,
            0,
            0};
    }

    void print_result(
        const BenchmarkResult &result,
        const std::string &unit)
    {
        std::cout
            << std::left
            << std::setw(42)
            << result.name
            << std::right
            << std::setw(12)
            << result.operations
            << std::setw(16)
            << result.min_duration.count()
            << std::setw(16)
            << result.median_duration.count()
            << std::setw(16)
            << result.max_duration.count()
            << std::setw(16)
            << std::fixed
            << std::setprecision(2)
            << result.nanoseconds_per_operation
            << std::setw(16)
            << std::fixed
            << std::setprecision(2)
            << result.throughput
            << "  "
            << unit
            << '\n';
    }

    void print_header()
    {
        std::cout << '\n';

        std::cout
            << std::left
            << std::setw(42)
            << "Benchmark"
            << std::right
            << std::setw(12)
            << "Ops"
            << std::setw(16)
            << "Min(ns)"
            << std::setw(16)
            << "Median(ns)"
            << std::setw(16)
            << "Max(ns)"
            << std::setw(16)
            << "ns/op"
            << std::setw(16)
            << "ops/sec"
            << "  Unit"
            << '\n';

        std::cout
            << std::string(156, '-')
            << '\n';
    }

    BenchmarkResult benchmark_orderbook_add(
        std::size_t size)
    {
        return run_benchmark(
            "OrderBook::add [" +
                std::to_string(size) +
                "]",
            [size]()
            {
                OrderBook book;

                for (std::size_t i = 0;
                     i < size;
                     ++i)
                {
                    Order order =
                        make_buy_order(
                            static_cast<OrderId>(
                                i + 1),
                            static_cast<Price>(
                                10000 + i),
                            100);

                    book.add(order);
                }

                volatile Price sink =
                    book.best_bid();

                (void)sink;
            },
            size,
            benchmark_config());
    }

    BenchmarkResult benchmark_orderbook_find(
        std::size_t size)
    {
        OrderBook book;

        for (std::size_t i = 0;
             i < size;
             ++i)
        {
            Order order =
                make_buy_order(
                    static_cast<OrderId>(
                        i + 1),
                    static_cast<Price>(
                        10000 + i),
                    100);

            book.add(order);
        }

        const OrderId target =
            static_cast<OrderId>(size);

        return run_benchmark(
            "OrderBook::find_order [" +
                std::to_string(size) +
                "]",
            [&book, target]()
            {
                volatile Order *order =
                    book.find_order(target);

                (void)order;
            },
            1,
            benchmark_config());
    }

    BenchmarkResult benchmark_orderbook_best_bid(
        std::size_t size)
    {
        OrderBook book;

        for (std::size_t i = 0;
             i < size;
             ++i)
        {
            Order order =
                make_buy_order(
                    static_cast<OrderId>(
                        i + 1),
                    static_cast<Price>(
                        10000 + i),
                    100);

            book.add(order);
        }

        return run_benchmark(
            "OrderBook::best_bid [" +
                std::to_string(size) +
                "]",
            [&book]()
            {
                volatile Price value =
                    book.best_bid();

                (void)value;
            },
            1,
            benchmark_config());
    }

    BenchmarkResult benchmark_orderbook_best_ask(
        std::size_t size)
    {
        OrderBook book;

        for (std::size_t i = 0;
             i < size;
             ++i)
        {
            Order order =
                make_sell_order(
                    static_cast<OrderId>(
                        i + 1),
                    static_cast<Price>(
                        11000 + i),
                    100);

            book.add(order);
        }

        return run_benchmark(
            "OrderBook::best_ask [" +
                std::to_string(size) +
                "]",
            [&book]()
            {
                volatile Price value =
                    book.best_ask();

                (void)value;
            },
            1,
            benchmark_config());
    }

    BenchmarkResult benchmark_orderbook_cancel_restore(
        std::size_t size)
    {
        OrderBook book;

        for (std::size_t i = 0;
             i < size;
             ++i)
        {
            Order order =
                make_buy_order(
                    static_cast<OrderId>(
                        i + 1),
                    static_cast<Price>(
                        10000 + i),
                    100);

            book.add(order);
        }

        return run_benchmark(
            "OrderBook::cancel+restore [" +
                std::to_string(size) +
                "]",
            [&book, size]()
            {
                for (std::size_t i = 0;
                     i < size;
                     ++i)
                {
                    const OrderId id =
                        static_cast<OrderId>(
                            i + 1);

                    const Price price =
                        static_cast<Price>(
                            10000 + i);

                    book.cancel(id);

                    Order order =
                        make_buy_order(
                            id,
                            price,
                            100);

                    book.add(order);
                }

                volatile Price sink =
                    book.best_bid();

                (void)sink;
            },
            size * 2,
            benchmark_config());
    }

    BenchmarkResult benchmark_orderbook_modify_pair(
        std::size_t size)
    {
        OrderBook book;

        for (std::size_t i = 0;
             i < size;
             ++i)
        {
            Order order =
                make_buy_order(
                    static_cast<OrderId>(
                        i + 1),
                    static_cast<Price>(
                        10000 + i),
                    100);

            book.add(order);
        }

        return run_benchmark(
            "OrderBook::modify pair [" +
                std::to_string(size) +
                "]",
            [&book, size]()
            {
                for (std::size_t i = 0;
                     i < size;
                     ++i)
                {
                    const OrderId id =
                        static_cast<OrderId>(
                            i + 1);

                    const Price price =
                        static_cast<Price>(
                            10000 + i);

                    book.modify(
                        id,
                        price,
                        99);

                    book.modify(
                        id,
                        price,
                        100);
                }

                volatile Price sink =
                    book.best_bid();

                (void)sink;
            },
            size * 2,
            benchmark_config());
    }

    BenchmarkResult benchmark_single_level_match(
        std::size_t size)
    {
        return run_benchmark(
            "Matching single-level [" +
                std::to_string(size) +
                "]",
            [size]()
            {
                OrderBook book;

                for (std::size_t i = 0;
                     i < size;
                     ++i)
                {
                    Order order =
                        make_sell_order(
                            static_cast<OrderId>(
                                i + 1),
                            10000,
                            100);

                    book.add(order);
                }

                Order incoming =
                    make_buy_order(
                        static_cast<OrderId>(
                            size + 1),
                        10000,
                        static_cast<Quantity>(
                            size * 100));

                std::vector<Trade> trades =
                    book.process_order(
                        incoming);

                volatile std::size_t trade_count =
                    trades.size();

                (void)trade_count;
            },
            size,
            benchmark_config());
    }

    BenchmarkResult benchmark_multi_level_match(
        std::size_t size)
    {
        return run_benchmark(
            "Matching multi-level [" +
                std::to_string(size) +
                "]",
            [size]()
            {
                OrderBook book;

                const std::size_t level_count =
                    size < 10
                        ? size
                        : 10;

                const std::size_t orders_per_level =
                    size / level_count == 0
                        ? 1
                        : size / level_count;

                std::size_t order_id = 1;

                for (std::size_t level = 0;
                     level < level_count;
                     ++level)
                {
                    const Price price =
                        static_cast<Price>(
                            10000 +
                            level * 100);

                    for (std::size_t j = 0;
                         j < orders_per_level;
                         ++j)
                    {
                        Order order =
                            make_sell_order(
                                static_cast<OrderId>(
                                    order_id++),
                                price,
                                100);

                        book.add(order);
                    }
                }

                Order incoming =
                    make_buy_order(
                        static_cast<OrderId>(
                            order_id),
                        static_cast<Price>(
                            10000 +
                            (level_count - 1) * 100),
                        static_cast<Quantity>(
                            size * 50));

                std::vector<Trade> trades =
                    book.process_order(
                        incoming);

                volatile std::size_t trade_count =
                    trades.size();

                (void)trade_count;
            },
            size,
            benchmark_config());
    }

    BenchmarkResult benchmark_simulator_adds(
        std::size_t size)
    {
        return run_benchmark(
            "Simulator ADD batch [" +
                std::to_string(size) +
                "]",
            [size]()
            {
                Simulator simulator(
                    100000000.0);

                for (std::size_t i = 0;
                     i < size;
                     ++i)
                {
                    simulator.add_event(
                        make_add_event(
                            static_cast<OrderId>(
                                i + 1),
                            Side::BUY,
                            static_cast<Price>(
                                10000 + i),
                            100,
                            static_cast<Timestamp>(
                                i + 1),
                            0));
                }

                simulator.process_events();
            },
            size * 2,
            benchmark_config());
    }

    BenchmarkResult benchmark_simulator_mixed(
        std::size_t size)
    {
        return run_benchmark(
            "Simulator mixed batch [" +
                std::to_string(size) +
                "]",
            [size]()
            {
                Simulator simulator(
                    100000000.0);

                const std::size_t add_count =
                    size * 2 / 3;

                for (std::size_t i = 0;
                     i < add_count;
                     ++i)
                {
                    simulator.add_event(
                        make_add_event(
                            static_cast<OrderId>(
                                i + 1),
                            Side::BUY,
                            static_cast<Price>(
                                10000 + i),
                            100,
                            static_cast<Timestamp>(
                                i + 1),
                            0));
                }

                for (std::size_t i = 0;
                     i < size - add_count;
                     ++i)
                {
                    simulator.add_event(
                        Event{
                            EventType::CANCEL,
                            static_cast<Timestamp>(
                                add_count + i + 1),
                            0,
                            Order{
                                0,
                                Side::BUY,
                                0,
                                0},
                            static_cast<OrderId>(
                                i + 1),
                            0,
                            0});
                }

                simulator.process_events();
            },
            size * 2,
            benchmark_config());
    }

    void benchmark_core_engine()
    {
        std::cout << '\n';

        std::cout
            << "============================================================\n";

        std::cout
            << "TEST 1 - CORE MARKET ENGINE\n";

        std::cout
            << "Chapters 1-5\n";

        std::cout
            << "============================================================\n";

        for (std::size_t size :
             benchmark_sizes)
        {
            print_header();

            print_result(
                benchmark_orderbook_add(size),
                "batch operations");

            print_result(
                benchmark_orderbook_find(size),
                "lookup");

            print_result(
                benchmark_orderbook_best_bid(size),
                "query");

            print_result(
                benchmark_orderbook_best_ask(size),
                "query");

            print_result(
                benchmark_orderbook_cancel_restore(size),
                "cancel+add");

            print_result(
                benchmark_orderbook_modify_pair(size),
                "modify pair");

            print_result(
                benchmark_single_level_match(size),
                "batch");

            print_result(
                benchmark_multi_level_match(size),
                "batch");

            print_result(
                benchmark_simulator_adds(size),
                "event+processing");

            print_result(
                benchmark_simulator_mixed(size),
                "event+processing");
        }
    }

    void prepare_metrics_book(
        OrderBook &book,
        std::size_t level_count)
    {
        for (std::size_t i = 0;
             i < level_count;
             ++i)
        {
            const Price bid_price =
                static_cast<Price>(
                    10000 + i);

            const Price ask_price =
                static_cast<Price>(
                    11000 + i);

            book.add(
                make_buy_order(
                    static_cast<OrderId>(
                        i + 1),
                    bid_price,
                    100));

            book.add(
                make_sell_order(
                    static_cast<OrderId>(
                        level_count + i + 1),
                    ask_price,
                    100));
        }
    }

    std::vector<Trade> make_execution_trades(
        std::size_t trade_count)
    {
        std::vector<Trade> trades;

        trades.reserve(
            trade_count);

        for (std::size_t i = 0;
             i < trade_count;
             ++i)
        {
            trades.push_back(
                Trade{
                    1,
                    static_cast<OrderId>(
                        i + 2),
                    static_cast<Price>(
                        10000 + (i % 50)),
                    static_cast<Quantity>(
                        1 + (i % 100))});
        }

        return trades;
    }

    BenchmarkResult benchmark_chapter_6(
        std::size_t size)
    {
        const std::size_t level_count =
            size < 10
                ? 10
                : size / 10;

        OrderBook book;

        prepare_metrics_book(
            book,
            level_count);

        std::vector<Trade> trades =
            make_execution_trades(
                size);

        return run_benchmark(
            "Metrics::calculate_metrics [" +
                std::to_string(size) +
                "]",
            [&book, &trades]()
            {
                MarketMetrics metrics =
                    calculate_metrics(
                        book,
                        trades);

                volatile double sink =

                    static_cast<double>(
                        metrics.best_bid)

                    +

                    static_cast<double>(
                        metrics.best_ask)

                    +

                    metrics.mid_price

                    +

                    static_cast<double>(
                        metrics.spread)

                    +

                    metrics.relative_spread

                    +

                    static_cast<double>(
                        metrics.bid_depth)

                    +

                    static_cast<double>(
                        metrics.ask_depth)

                    +

                    metrics.imbalance

                    +

                    static_cast<double>(
                        metrics.trade_count)

                    +

                    static_cast<double>(
                        metrics.trade_volume);

                (void)sink;
            },
            size,
            analysis_benchmark_config(size));
    }

    BenchmarkResult benchmark_chapter_7(
        std::size_t size)
    {
        std::vector<Trade> trades =
            make_execution_trades(
                size);

        const MidPrice reference_price =
            10500.0;

        return run_benchmark(
            "Execution::calculate_execution_result [" +
                std::to_string(size) +
                "]",
            [&trades, reference_price]()
            {
                ExecutionResult result =
                    calculate_execution_result(
                        trades,
                        Side::BUY,
                        reference_price);

                volatile double sink =

                    static_cast<double>(
                        result.executed_quantity)

                    +

                    static_cast<double>(
                        result.execution_value)

                    +

                    result.execution_vwap

                    +

                    result.reference_price

                    +

                    result.slippage

                    +

                    static_cast<double>(
                        result.execution_cost)

                    +

                    static_cast<double>(
                        result.liquidity_consumed);

                (void)sink;
            },
            size,
            analysis_benchmark_config(size));
    }

    void benchmark_market_analysis()
    {
        std::cout << '\n';

        std::cout
            << "============================================================\n";

        std::cout
            << "TEST 2 - MARKET ANALYSIS\n";

        std::cout
            << "Chapters 6-7\n";

        std::cout
            << "============================================================\n";

        for (std::size_t size :
             analysis_sizes)
        {
            print_header();

            print_result(
                benchmark_chapter_6(size),
                "metric workload");

            print_result(
                benchmark_chapter_7(size),
                "trade workload");
        }
    }

    BenchmarkResult benchmark_chapter_8(
        std::size_t size)
    {
        return run_benchmark(
            "InventoryModel [" +
                std::to_string(size) +
                "]",
            [size]()
            {
                InventoryModel inventory(
                    100000000.0);

                for (std::size_t i = 0;
                     i < size;
                     ++i)
                {
                    Trade trade{
                        1,
                        static_cast<OrderId>(
                            i + 2),
                        static_cast<Price>(
                            10000 + (i % 50)),
                        static_cast<Quantity>(
                            1 + (i % 100))};

                    inventory.process_trade(
                        trade,
                        Side::BUY);
                }

                volatile double sink =

                    static_cast<double>(
                        inventory.get_position())

                    +

                    inventory.get_cash()

                    +

                    inventory.get_avg_cost()

                    +

                    inventory.get_realized_pnl();

                (void)sink;
            },
            size,
            analysis_benchmark_config(size));
    }

    BenchmarkResult benchmark_chapter_9(
        std::size_t size)
    {
        return run_benchmark(
            "ITCH Replay [" +
                std::to_string(size) +
                "]",
            [size]()
            {
                ItchFileReader reader(
                    itch_benchmark_file);

                OrderBook order_book;

                ItchMapper mapper(
                    itch_benchmark_stock_locate);

                ItchReplay replay(
                    order_book);

                std::size_t messages_processed = 0;

                while (
                    messages_processed <
                    size)
                {
                    auto read_result =
                        reader.next_message();

                    if (
                        std::holds_alternative<
                            EndOfFile>(
                            read_result))
                    {
                        throw std::runtime_error(
                            "ITCH file ended before "
                            "requested message count.");
                    }

                    if (
                        std::holds_alternative<
                            FileReaderError>(
                            read_result))
                    {
                        throw std::runtime_error(
                            "ITCH file reader error.");
                    }

                    const std::vector<Byte> &payload =
                        std::get<
                            std::vector<Byte>>(
                            read_result);

                    auto parse_result =
                        get_type_parser(
                            payload);

                    if (
                        std::holds_alternative<
                            ParseError>(
                            parse_result))
                    {
                        throw std::runtime_error(
                            "ITCH parser error.");
                    }

                    const ItchMessage &message =
                        std::get<ItchMessage>(
                            parse_result);

                    auto map_result =
                        mapper.map(
                            message);

                    if (
                        std::holds_alternative<
                            ReplayError>(
                            map_result))
                    {
                        const ReplayError error =
                            std::get<ReplayError>(
                                map_result);

                        if (
                            error ==
                            ReplayError::IgnoredMessage)
                        {
                            ++messages_processed;
                            continue;
                        }

                        throw std::runtime_error(
                            "ITCH mapper error.");
                    }

                    const ReplayOperation &operation =
                        std::get<ReplayOperation>(
                            map_result);

                    auto replay_result =
                        replay.apply(
                            operation);

                    if (
                        std::holds_alternative<
                            ReplayError>(
                            replay_result))
                    {
                        throw std::runtime_error(
                            "ITCH replay error.");
                    }

                    ++messages_processed;
                }

                volatile Price sink_bid =
                    order_book.best_bid();

                volatile Price sink_ask =
                    order_book.best_ask();

                (void)sink_bid;
                (void)sink_ask;
            },
            size,
            replay_benchmark_config(size));
    }

    void benchmark_accounting_and_replay()
    {
        std::cout << '\n';

        std::cout
            << "============================================================\n";

        std::cout
            << "TEST 3 - ACCOUNTING AND HISTORICAL REPLAY\n";

        std::cout
            << "Chapters 8-9\n";

        std::cout
            << "============================================================\n";

        const std::vector<std::size_t>
            inventory_sizes =
                {
                    1000000,
                    10000000,
                    100000000,
                    1000000000ULL};

        std::cout
            << "\nCHAPTER 8 - INVENTORY / P&L\n";

        for (std::size_t size :
             inventory_sizes)
        {
            print_header();

            print_result(
                benchmark_chapter_8(size),
                "trade accounting");
        }

        const std::vector<std::size_t>
            itch_sizes =
                {
                    1000000,
                    10000000,
                    100000000,
                    itch_max_messages};

        std::cout
            << "\nCHAPTER 9 - ITCH REPLAY\n";

        std::cout
            << "File: "
            << itch_benchmark_file
            << '\n';

        std::cout
            << "Selected stock locate: "
            << itch_benchmark_stock_locate
            << '\n';

        std::cout
            << "Maximum available messages: "
            << itch_max_messages
            << '\n';

        for (std::size_t size :
             itch_sizes)
        {
            print_header();

            print_result(
                benchmark_chapter_9(size),
                "raw ITCH messages");
        }
    }
}

int main()
{
    std::cout
        << "Chapter 10 - Performance Baseline\n";

    if (run_test_1)
    {
        benchmark_core_engine();
    }

    if (run_test_2)
    {
        benchmark_market_analysis();
    }

    if (run_test_3)
    {
        benchmark_accounting_and_replay();
    }

    std::cout
        << "\nBenchmark completed.\n";

    return 0;
}