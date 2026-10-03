#include <profile.hpp>

#include <event.hpp>
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
#include <iostream>
#include <stdexcept>
#include <string>
#include <variant>
#include <vector>

namespace
{
    constexpr std::size_t profile_size = 10000;

    constexpr std::size_t itch_profile_messages = 1000000;

    constexpr const char *itch_profile_file =
        "data/itch/2019-10-18/raw/decompressed/S101819-v50.txt";

    constexpr StockLocate itch_profile_stock_locate = 123;

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

    void print_profile(
        const std::string &title)
    {
        std::cout << '\n';
        std::cout
            << "============================================================\n";
        std::cout
            << title
            << '\n';
        std::cout
            << "============================================================\n";

        Profiler::instance().print_report(std::cout);
    }

    void profile_orderbook_add()
    {
        Profiler::instance().reset();

        OrderBook book;

        for (std::size_t i = 0;
             i < profile_size;
             ++i)
        {
            book.add(
                make_buy_order(
                    static_cast<OrderId>(i + 1),
                    static_cast<Price>(10000 + i),
                    100));
        }

        print_profile(
            "PROFILE 1 - ORDERBOOK ADD");
    }

    void profile_cancel_restore()
    {
        OrderBook book;

        for (std::size_t i = 0;
             i < profile_size;
             ++i)
        {
            book.add(
                make_buy_order(
                    static_cast<OrderId>(i + 1),
                    static_cast<Price>(10000 + i),
                    100));
        }

        Profiler::instance().reset();

        for (std::size_t i = 0;
             i < profile_size;
             ++i)
        {
            const OrderId id =
                static_cast<OrderId>(i + 1);

            const Price price =
                static_cast<Price>(10000 + i);

            book.cancel(id);

            book.add(
                make_buy_order(
                    id,
                    price,
                    100));
        }

        print_profile(
            "PROFILE 2 - CANCEL / RESTORE");
    }

    void profile_modify()
    {
        OrderBook book;

        for (std::size_t i = 0;
             i < profile_size;
             ++i)
        {
            book.add(
                make_buy_order(
                    static_cast<OrderId>(i + 1),
                    static_cast<Price>(10000 + i),
                    100));
        }

        Profiler::instance().reset();

        for (std::size_t i = 0;
             i < profile_size;
             ++i)
        {
            const OrderId id =
                static_cast<OrderId>(i + 1);

            const Price price =
                static_cast<Price>(10000 + i);

            book.modify(
                id,
                price,
                99);

            book.modify(
                id,
                price,
                100);
        }

        print_profile(
            "PROFILE 3 - MODIFY");
    }

    void profile_single_level_matching()
    {
        OrderBook book;

        for (std::size_t i = 0;
             i < profile_size;
             ++i)
        {
            book.add(
                make_sell_order(
                    static_cast<OrderId>(i + 1),
                    10000,
                    100));
        }

        Profiler::instance().reset();

        Order incoming =
            make_buy_order(
                static_cast<OrderId>(
                    profile_size + 1),
                10000,
                static_cast<Quantity>(
                    profile_size * 100));

        std::vector<Trade> trades =
            book.process_order(
                incoming);

        volatile std::size_t trade_count =
            trades.size();

        (void)trade_count;

        print_profile(
            "PROFILE 4A - SINGLE-LEVEL MATCHING");
    }

    void profile_multi_level_matching()
    {
        OrderBook book;

        const std::size_t level_count = 10;

        const std::size_t orders_per_level =
            profile_size / level_count;

        std::size_t order_id = 1;

        for (std::size_t level = 0;
             level < level_count;
             ++level)
        {
            const Price price =
                static_cast<Price>(
                    10000 + level * 100);

            for (std::size_t j = 0;
                 j < orders_per_level;
                 ++j)
            {
                book.add(
                    make_sell_order(
                        static_cast<OrderId>(
                            order_id++),
                        price,
                        100));
            }
        }

        Profiler::instance().reset();

        Order incoming =
            make_buy_order(
                static_cast<OrderId>(
                    order_id),
                static_cast<Price>(
                    10000 +
                    (level_count - 1) * 100),
                static_cast<Quantity>(
                    profile_size * 50));

        std::vector<Trade> trades =
            book.process_order(
                incoming);

        volatile std::size_t trade_count =
            trades.size();

        (void)trade_count;

        print_profile(
            "PROFILE 4B - MULTI-LEVEL MATCHING");
    }

    void profile_simulator_add()
    {
        Simulator simulator(
            100000000.0);

        for (std::size_t i = 0;
             i < profile_size;
             ++i)
        {
            simulator.add_event(
                make_add_event(
                    static_cast<OrderId>(i + 1),
                    Side::BUY,
                    static_cast<Price>(10000 + i),
                    100,
                    static_cast<Timestamp>(i + 1),
                    0));
        }

        Profiler::instance().reset();

        simulator.process_events();

        print_profile(
            "PROFILE 5A - SIMULATOR ADD");
    }

    void profile_simulator_mixed()
    {
        Simulator simulator(
            100000000.0);

        const std::size_t add_count =
            profile_size * 2 / 3;

        for (std::size_t i = 0;
             i < add_count;
             ++i)
        {
            simulator.add_event(
                make_add_event(
                    static_cast<OrderId>(i + 1),
                    Side::BUY,
                    static_cast<Price>(10000 + i),
                    100,
                    static_cast<Timestamp>(i + 1),
                    0));
        }

        for (std::size_t i = 0;
             i < profile_size - add_count;
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

        Profiler::instance().reset();

        simulator.process_events();

        print_profile(
            "PROFILE 5B - SIMULATOR MIXED");
    }

    void profile_itch()
    {
        ItchFileReader reader(
            itch_profile_file);

        OrderBook order_book;

        ItchMapper mapper(
            itch_profile_stock_locate);

        ItchReplay replay(
            order_book);

        std::size_t messages_read = 0;
        std::size_t parsed_messages = 0;
        std::size_t ignored_messages = 0;

        std::size_t add_operations = 0;
        std::size_t reduce_operations = 0;
        std::size_t remove_operations = 0;
        std::size_t replace_operations = 0;

        std::size_t replay_successes = 0;
        std::size_t replay_errors = 0;

        Profiler::instance().reset();

        {
            LOB_PROFILE_SCOPE(
                "ITCH::end_to_end");

            while (
                messages_read <
                itch_profile_messages)
            {
                auto read_result =
                    reader.next_message();

                if (
                    std::holds_alternative<
                        EndOfFile>(
                        read_result))
                {
                    throw std::runtime_error(
                        "ITCH file ended before requested message count.");
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

                ++messages_read;

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

                ++parsed_messages;

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
                    ReplayError error =
                        std::get<ReplayError>(
                            map_result);

                    if (
                        error ==
                        ReplayError::IgnoredMessage)
                    {
                        ++ignored_messages;
                        continue;
                    }

                    ++replay_errors;

                    continue;
                }

                const ReplayOperation &operation =
                    std::get<ReplayOperation>(
                        map_result);

                std::visit(
                    [](const auto &op)
                    {
                        using T =
                            std::decay_t<
                                decltype(op)>;

                        if constexpr (
                            std::is_same_v<
                                T,
                                AddOperation>)
                        {
                        }
                    },
                    operation);

                if (
                    std::holds_alternative<
                        AddOperation>(
                        operation))
                {
                    ++add_operations;
                }

                else if (
                    std::holds_alternative<
                        ReduceOperation>(
                        operation))
                {
                    ++reduce_operations;
                }

                else if (
                    std::holds_alternative<
                        RemoveOperation>(
                        operation))
                {
                    ++remove_operations;
                }

                else if (
                    std::holds_alternative<
                        ReplaceOperation>(
                        operation))
                {
                    ++replace_operations;
                }

                auto replay_result =
                    replay.apply(
                        operation);

                if (
                    std::holds_alternative<
                        ReplayError>(
                        replay_result))
                {
                    ++replay_errors;
                }

                else
                {
                    ++replay_successes;
                }
            }
        }

        std::cout << '\n';
        std::cout
            << "============================================================\n";
        std::cout
            << "ITCH PROFILE WORKLOAD\n";
        std::cout
            << "============================================================\n";

        std::cout
            << "Messages read:       "
            << messages_read
            << '\n';

        std::cout
            << "Messages parsed:     "
            << parsed_messages
            << '\n';

        std::cout
            << "Ignored messages:    "
            << ignored_messages
            << '\n';

        std::cout
            << "Add operations:      "
            << add_operations
            << '\n';

        std::cout
            << "Reduce operations:   "
            << reduce_operations
            << '\n';

        std::cout
            << "Remove operations:   "
            << remove_operations
            << '\n';

        std::cout
            << "Replace operations:  "
            << replace_operations
            << '\n';

        std::cout
            << "Replay successes:    "
            << replay_successes
            << '\n';

        std::cout
            << "Replay errors:       "
            << replay_errors
            << '\n';

        Profiler::instance().print_report(
            std::cout);
    }
}

int main()
{
    try
    {
        profile_orderbook_add();

        profile_cancel_restore();

        profile_modify();

        profile_single_level_matching();

        profile_multi_level_matching();

        profile_simulator_add();

        profile_simulator_mixed();

        profile_itch();
    }
    catch (const std::exception &error)
    {
        std::cerr
            << "PROFILE ERROR: "
            << error.what()
            << '\n';

        return 1;
    }

    return 0;
}