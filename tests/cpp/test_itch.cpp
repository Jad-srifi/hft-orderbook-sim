#include <types.hpp>

#include <itch_message.hpp>
#include <itch_parser.hpp>
#include <itch_mapper.hpp>
#include <itch_replay.hpp>
#include <order_book.hpp>

#include <array>
#include <cassert>
#include <cstdint>
#include <iostream>
#include <string>
#include <variant>
#include <vector>

// ============================================================
// TEST HELPERS
// ============================================================

void append_be(
    std::vector<Byte>& bytes,
    std::uint64_t value,
    std::size_t count
)
{
    for (std::size_t i = count; i > 0; --i) {
        bytes.push_back(
            static_cast<Byte>(
                (value >> ((i - 1) * 8)) & 0xFF
            )
        );
    }
}

template <std::size_t N>
void append_chars(
    std::vector<Byte>& bytes,
    const std::array<char, N>& chars
)
{
    for (char c : chars) {
        bytes.push_back(static_cast<Byte>(c));
    }
}

bool is_parse_error(
    const std::vector<Byte>& bytes,
    ParseError expected
)
{
    auto result = get_type_parser(bytes);

    return std::holds_alternative<ParseError>(result) &&
           std::get<ParseError>(result) == expected;
}

// ============================================================
// SYNTHETIC ITCH MESSAGE BUILDERS
// ============================================================

std::array<char, 8> make_symbol()
{
    return {'T', 'E', 'S', 'T', ' ', ' ', ' ', ' '};
}

std::array<char, 4> make_mpid()
{
    return {'M', 'P', 'I', 'D'};
}

std::vector<Byte> make_A()
{
    std::vector<Byte> bytes;

    bytes.push_back('A');

    append_be(bytes, 10, 2);
    append_be(bytes, 1, 2);
    append_be(bytes, 100000000, 6);
    append_be(bytes, 5001, 8);

    bytes.push_back('B');

    append_be(bytes, 100, 4);
    append_chars(bytes, make_symbol());
    append_be(bytes, 10050, 4);

    return bytes;
}

std::vector<Byte> make_F()
{
    std::vector<Byte> bytes = make_A();

    bytes[0] = 'F';

    append_chars(bytes, make_mpid());

    return bytes;
}

std::vector<Byte> make_E()
{
    std::vector<Byte> bytes;

    bytes.push_back('E');

    append_be(bytes, 10, 2);
    append_be(bytes, 2, 2);
    append_be(bytes, 100000001, 6);
    append_be(bytes, 5001, 8);
    append_be(bytes, 40, 4);
    append_be(bytes, 9001, 8);

    return bytes;
}

std::vector<Byte> make_C()
{
    std::vector<Byte> bytes = make_E();

    bytes[0] = 'C';

    bytes.push_back('Y');
    append_be(bytes, 10040, 4);

    return bytes;
}

std::vector<Byte> make_X()
{
    std::vector<Byte> bytes;

    bytes.push_back('X');

    append_be(bytes, 10, 2);
    append_be(bytes, 3, 2);
    append_be(bytes, 100000002, 6);
    append_be(bytes, 5001, 8);
    append_be(bytes, 30, 4);

    return bytes;
}

std::vector<Byte> make_D()
{
    std::vector<Byte> bytes;

    bytes.push_back('D');

    append_be(bytes, 10, 2);
    append_be(bytes, 4, 2);
    append_be(bytes, 100000003, 6);
    append_be(bytes, 5001, 8);

    return bytes;
}

std::vector<Byte> make_U()
{
    std::vector<Byte> bytes;

    bytes.push_back('U');

    append_be(bytes, 10, 2);
    append_be(bytes, 5, 2);
    append_be(bytes, 100000004, 6);

    append_be(bytes, 5001, 8);
    append_be(bytes, 6001, 8);

    append_be(bytes, 120, 4);
    append_be(bytes, 10100, 4);

    return bytes;
}

std::vector<Byte> make_R()
{
    std::vector<Byte> bytes;

    bytes.push_back('R');

    append_be(bytes, 10, 2);
    append_be(bytes, 6, 2);
    append_be(bytes, 100000005, 6);
    append_chars(bytes, make_symbol());

    bytes.push_back('Q');
    bytes.push_back('N');

    append_be(bytes, 100, 4);

    bytes.push_back('Y');
    bytes.push_back('N');

    bytes.push_back('A');
    bytes.push_back('B');

    bytes.push_back('N');
    bytes.push_back('N');
    bytes.push_back('N');
    bytes.push_back('1');
    bytes.push_back('N');

    append_be(bytes, 2, 4);

    bytes.push_back('N');

    return bytes;
}

std::vector<Byte> make_S()
{
    std::vector<Byte> bytes;

    bytes.push_back('S');

    append_be(bytes, 10, 2);
    append_be(bytes, 7, 2);
    append_be(bytes, 100000006, 6);

    bytes.push_back('O');

    return bytes;
}

// ============================================================
// PARSER HELPER TESTS
// ============================================================

void test_read_bytes()
{
    std::vector<Byte> bytes = {10, 20, 30, 40};

    auto result = read_bytes(bytes, 1, 2);

    assert(result.size() == 2);
    assert(result[0] == 20);
    assert(result[1] == 30);

    auto invalid = read_bytes(bytes, 3, 5);

    assert(invalid.empty());
}

void test_read_be()
{
    std::vector<Byte> bytes = {
        0x01, 0x02, 0x03, 0x04
    };

    assert(read_be(bytes, 0, 2) == 0x0102);
    assert(read_be(bytes, 0, 4) == 0x01020304);
    assert(read_be(bytes, 3, 2) == -1);
}

void test_read_side()
{
    assert(read_side('B') == Side::BUY);
    assert(read_side('S') == Side::SELL);
    assert(read_side('X') == Side::NONE);
}

// ============================================================
// PARSER TESTS
// ============================================================

void test_A()
{
    auto raw = make_A();
    auto result = get_type_parser(raw);

    assert(std::holds_alternative<ItchMessage>(result));

    const auto& message =
        std::get<ItchMessage>(result);

    assert(std::holds_alternative<AddOrderMessage>(message));

    const auto& parsed =
        std::get<AddOrderMessage>(message);

    assert(parsed.stock_locate == 10);
    assert(parsed.tracking_number == 1);
    assert(parsed.timestamp == 100000000);
    assert(parsed.order_reference == 5001);
    assert(parsed.side == Side::BUY);
    assert(parsed.shares == 100);
    assert(parsed.price == 10050);
    assert(parsed.stock_symbol == make_symbol());
}

void test_F()
{
    auto raw = make_F();
    auto result = get_type_parser(raw);

    assert(std::holds_alternative<ItchMessage>(result));

    const auto& message =
        std::get<ItchMessage>(result);

    assert(std::holds_alternative<AddOrderMPIDMessage>(message));

    const auto& parsed =
        std::get<AddOrderMPIDMessage>(message);

    assert(parsed.stock_locate == 10);
    assert(parsed.tracking_number == 1);
    assert(parsed.timestamp == 100000000);
    assert(parsed.order_reference == 5001);
    assert(parsed.side == Side::BUY);
    assert(parsed.shares == 100);
    assert(parsed.price == 10050);
    assert(parsed.stock_symbol == make_symbol());
    assert(parsed.mpid == make_mpid());
}

void test_E()
{
    auto raw = make_E();
    auto result = get_type_parser(raw);

    assert(std::holds_alternative<ItchMessage>(result));

    const auto& message =
        std::get<ItchMessage>(result);

    assert(std::holds_alternative<ExecuteMessage>(message));

    const auto& parsed =
        std::get<ExecuteMessage>(message);

    assert(parsed.stock_locate == 10);
    assert(parsed.tracking_number == 2);
    assert(parsed.timestamp == 100000001);
    assert(parsed.order_reference == 5001);
    assert(parsed.executed_shares == 40);
    assert(parsed.match_number == 9001);
}

void test_C()
{
    auto raw = make_C();
    auto result = get_type_parser(raw);

    assert(std::holds_alternative<ItchMessage>(result));

    const auto& message =
        std::get<ItchMessage>(result);

    assert(
        std::holds_alternative<ExecuteWithPriceMessage>(message)
    );

    const auto& parsed =
        std::get<ExecuteWithPriceMessage>(message);

    assert(parsed.stock_locate == 10);
    assert(parsed.tracking_number == 2);
    assert(parsed.timestamp == 100000001);
    assert(parsed.order_reference == 5001);
    assert(parsed.executed_shares == 40);
    assert(parsed.match_number == 9001);
    assert(parsed.execution_price == 10040);
    assert(parsed.printability == 'Y');
}

void test_X()
{
    auto raw = make_X();
    auto result = get_type_parser(raw);

    assert(std::holds_alternative<ItchMessage>(result));

    const auto& message =
        std::get<ItchMessage>(result);

    assert(std::holds_alternative<CancelMessage>(message));

    const auto& parsed =
        std::get<CancelMessage>(message);

    assert(parsed.stock_locate == 10);
    assert(parsed.tracking_number == 3);
    assert(parsed.timestamp == 100000002);
    assert(parsed.order_reference == 5001);
    assert(parsed.cancelled_shares == 30);
}

void test_D()
{
    auto raw = make_D();
    auto result = get_type_parser(raw);

    assert(std::holds_alternative<ItchMessage>(result));

    const auto& message =
        std::get<ItchMessage>(result);

    assert(std::holds_alternative<DeleteMessage>(message));

    const auto& parsed =
        std::get<DeleteMessage>(message);

    assert(parsed.stock_locate == 10);
    assert(parsed.tracking_number == 4);
    assert(parsed.timestamp == 100000003);
    assert(parsed.order_reference == 5001);
}

void test_U()
{
    auto raw = make_U();
    auto result = get_type_parser(raw);

    assert(std::holds_alternative<ItchMessage>(result));

    const auto& message =
        std::get<ItchMessage>(result);

    assert(std::holds_alternative<ReplaceMessage>(message));

    const auto& parsed =
        std::get<ReplaceMessage>(message);

    assert(parsed.stock_locate == 10);
    assert(parsed.tracking_number == 5);
    assert(parsed.timestamp == 100000004);
    assert(parsed.old_order_reference == 5001);
    assert(parsed.new_order_reference == 6001);
    assert(parsed.new_shares == 120);
    assert(parsed.new_price == 10100);
}

void test_R()
{
    auto raw = make_R();
    auto result = get_type_parser(raw);

    assert(std::holds_alternative<ItchMessage>(result));

    const auto& message =
        std::get<ItchMessage>(result);

    assert(
        std::holds_alternative<StockDirectoryMessage>(message)
    );

    const auto& parsed =
        std::get<StockDirectoryMessage>(message);

    assert(parsed.stock_locate == 10);
    assert(parsed.tracking_number == 6);
    assert(parsed.timestamp == 100000005);
    assert(parsed.stock_symbol == make_symbol());

    assert(parsed.market_category == 'Q');
    assert(parsed.financial_status_indicator == 'N');
    assert(parsed.round_lot_size == 100);
    assert(parsed.round_lots_only == 'Y');
    assert(parsed.issue_clarification == 'N');

    assert(parsed.issue_sub_type[0] == 'A');
    assert(parsed.issue_sub_type[1] == 'B');

    assert(parsed.authenticity == 'N');
    assert(parsed.short_sale_threshold == 'N');
    assert(parsed.IPO_flag == 'N');
    assert(parsed.LULD_reference_price_tier == '1');
    assert(parsed.ETP_flag == 'N');
    assert(parsed.ETP_leverage_factor == 2);
    assert(parsed.inverse_indicator == 'N');
}

void test_S()
{
    auto raw = make_S();
    auto result = get_type_parser(raw);

    assert(std::holds_alternative<ItchMessage>(result));

    const auto& message =
        std::get<ItchMessage>(result);

    assert(std::holds_alternative<SystemEventMessage>(message));

    const auto& parsed =
        std::get<SystemEventMessage>(message);

    assert(parsed.stock_locate == 10);
    assert(parsed.tracking_number == 7);
    assert(parsed.timestamp == 100000006);
    assert(parsed.event_code == 'O');
}

// ============================================================
// PARSER ERROR TESTS
// ============================================================

void test_empty_message()
{
    std::vector<Byte> raw;

    assert(
        is_parse_error(
            raw,
            ParseError::IncompleteMessage
        )
    );
}

void test_unknown_message_type()
{
    std::vector<Byte> raw = {'Z'};

    assert(
        is_parse_error(
            raw,
            ParseError::UnknownMessageType
        )
    );

    assert(
        is_parse_error(
            std::vector<Byte>{'P'},
            ParseError::UnknownMessageType
        )
    );

    assert(
        is_parse_error(
            std::vector<Byte>{'Q'},
            ParseError::UnknownMessageType
        )
    );

    assert(
        is_parse_error(
            std::vector<Byte>{'B'},
            ParseError::UnknownMessageType
        )
    );
}

void test_invalid_lengths()
{
    std::vector<std::vector<Byte>> messages = {
        make_A(),
        make_F(),
        make_E(),
        make_C(),
        make_X(),
        make_D(),
        make_U(),
        make_R(),
        make_S()
    };

    for (auto message : messages) {

        assert(!message.empty());

        message.pop_back();

        auto result = get_type_parser(message);

        assert(
            std::holds_alternative<ParseError>(result)
        );

        assert(
            std::get<ParseError>(result) ==
            ParseError::InvalidMessageLength
        );
    }
}

void test_invalid_side()
{
    auto raw = make_A();

    raw[19] = 'X';

    auto result = get_type_parser(raw);

    assert(
        std::holds_alternative<ParseError>(result)
    );

    assert(
        std::get<ParseError>(result) ==
        ParseError::MalformedMessage
    );
}

void test_truncated_messages()
{
    std::vector<Byte> A = {'A'};
    std::vector<Byte> F = {'F'};
    std::vector<Byte> E = {'E'};
    std::vector<Byte> C = {'C'};
    std::vector<Byte> X = {'X'};
    std::vector<Byte> D = {'D'};
    std::vector<Byte> U = {'U'};
    std::vector<Byte> R = {'R'};
    std::vector<Byte> S = {'S'};

    assert(
        is_parse_error(
            A,
            ParseError::InvalidMessageLength
        )
    );

    assert(
        is_parse_error(
            F,
            ParseError::InvalidMessageLength
        )
    );

    assert(
        is_parse_error(
            E,
            ParseError::InvalidMessageLength
        )
    );

    assert(
        is_parse_error(
            C,
            ParseError::InvalidMessageLength
        )
    );

    assert(
        is_parse_error(
            X,
            ParseError::InvalidMessageLength
        )
    );

    assert(
        is_parse_error(
            D,
            ParseError::InvalidMessageLength
        )
    );

    assert(
        is_parse_error(
            U,
            ParseError::InvalidMessageLength
        )
    );

    assert(
        is_parse_error(
            R,
            ParseError::InvalidMessageLength
        )
    );

    assert(
        is_parse_error(
            S,
            ParseError::InvalidMessageLength
        )
    );
}

// ============================================================
// MAPPER TESTS
// ============================================================

void test_mapper_A()
{
    ItchMapper mapper(10);

    auto parsed = get_type_parser(make_A());

    assert(
        std::holds_alternative<ItchMessage>(parsed)
    );

    auto mapped =
        mapper.map(std::get<ItchMessage>(parsed));

    assert(
        std::holds_alternative<ReplayOperation>(mapped)
    );

    const auto& operation =
        std::get<ReplayOperation>(mapped);

    assert(
        std::holds_alternative<AddOperation>(operation)
    );

    const auto& add =
        std::get<AddOperation>(operation);

    assert(add.order_id == 5001);
    assert(add.side == Side::BUY);
    assert(add.quantity == 100);
    assert(add.price == 10050);
}

void test_mapper_F()
{
    ItchMapper mapper(10);

    auto parsed = get_type_parser(make_F());

    auto mapped =
        mapper.map(std::get<ItchMessage>(parsed));

    assert(
        std::holds_alternative<ReplayOperation>(mapped)
    );

    const auto& operation =
        std::get<ReplayOperation>(mapped);

    assert(
        std::holds_alternative<AddOperation>(operation)
    );

    const auto& add =
        std::get<AddOperation>(operation);

    assert(add.order_id == 5001);
    assert(add.side == Side::BUY);
    assert(add.quantity == 100);
    assert(add.price == 10050);
}

void test_mapper_E()
{
    ItchMapper mapper(10);

    auto parsed = get_type_parser(make_E());

    auto mapped =
        mapper.map(std::get<ItchMessage>(parsed));

    assert(
        std::holds_alternative<ReplayOperation>(mapped)
    );

    const auto& operation =
        std::get<ReplayOperation>(mapped);

    assert(
        std::holds_alternative<ReduceOperation>(operation)
    );

    const auto& reduce =
        std::get<ReduceOperation>(operation);

    assert(reduce.order_id == 5001);
    assert(reduce.quantity == 40);
}

void test_mapper_C()
{
    ItchMapper mapper(10);

    auto parsed = get_type_parser(make_C());

    auto mapped =
        mapper.map(std::get<ItchMessage>(parsed));

    assert(
        std::holds_alternative<ReplayOperation>(mapped)
    );

    const auto& operation =
        std::get<ReplayOperation>(mapped);

    assert(
        std::holds_alternative<ReduceOperation>(operation)
    );

    const auto& reduce =
        std::get<ReduceOperation>(operation);

    assert(reduce.order_id == 5001);
    assert(reduce.quantity == 40);
}

void test_mapper_X()
{
    ItchMapper mapper(10);

    auto parsed = get_type_parser(make_X());

    auto mapped =
        mapper.map(std::get<ItchMessage>(parsed));

    assert(
        std::holds_alternative<ReplayOperation>(mapped)
    );

    const auto& operation =
        std::get<ReplayOperation>(mapped);

    assert(
        std::holds_alternative<ReduceOperation>(operation)
    );

    const auto& reduce =
        std::get<ReduceOperation>(operation);

    assert(reduce.order_id == 5001);
    assert(reduce.quantity == 30);
}

void test_mapper_D()
{
    ItchMapper mapper(10);

    auto parsed = get_type_parser(make_D());

    auto mapped =
        mapper.map(std::get<ItchMessage>(parsed));

    assert(
        std::holds_alternative<ReplayOperation>(mapped)
    );

    const auto& operation =
        std::get<ReplayOperation>(mapped);

    assert(
        std::holds_alternative<RemoveOperation>(operation)
    );

    const auto& remove =
        std::get<RemoveOperation>(operation);

    assert(remove.order_id == 5001);
}

void test_mapper_U()
{
    ItchMapper mapper(10);

    auto parsed = get_type_parser(make_U());

    auto mapped =
        mapper.map(std::get<ItchMessage>(parsed));

    assert(
        std::holds_alternative<ReplayOperation>(mapped)
    );

    const auto& operation =
        std::get<ReplayOperation>(mapped);

    assert(
        std::holds_alternative<ReplaceOperation>(operation)
    );

    const auto& replace =
        std::get<ReplaceOperation>(operation);

    assert(replace.old_order_id == 5001);
    assert(replace.new_order_id == 6001);
    assert(replace.quantity == 120);
    assert(replace.price == 10100);
    assert(replace.side == Side::NONE);
}

void test_mapper_security_filter()
{
    ItchMapper mapper(10);

    auto raw = make_A();

    raw[1] = 0;
    raw[2] = 20;

    auto parsed = get_type_parser(raw);

    auto mapped =
        mapper.map(std::get<ItchMessage>(parsed));

    assert(
        std::holds_alternative<ReplayError>(mapped)
    );

    assert(
        std::get<ReplayError>(mapped) ==
        ReplayError::IgnoredMessage
    );
}

void test_mapper_R_S_ignored()
{
    ItchMapper mapper(10);

    auto parsed_R = get_type_parser(make_R());

    auto mapped_R =
        mapper.map(std::get<ItchMessage>(parsed_R));

    assert(
        std::holds_alternative<ReplayError>(mapped_R)
    );

    assert(
        std::get<ReplayError>(mapped_R) ==
        ReplayError::IgnoredMessage
    );

    auto parsed_S = get_type_parser(make_S());

    auto mapped_S =
        mapper.map(std::get<ItchMessage>(parsed_S));

    assert(
        std::holds_alternative<ReplayError>(mapped_S)
    );

    assert(
        std::get<ReplayError>(mapped_S) ==
        ReplayError::IgnoredMessage
    );
}

// ============================================================
// REPLAY TESTS
// ============================================================

void test_replay_add()
{
    OrderBook book;
    ItchReplay replay(book);

    AddOperation operation{
        5001,
        Side::BUY,
        100,
        10050
    };

    auto result = replay.apply(operation);

    assert(
        std::holds_alternative<std::monostate>(result)
    );

    Order* order = book.find_order(5001);

    assert(order != nullptr);
    assert(order->side == Side::BUY);
    assert(order->quantity == 100);
    assert(order->price == 10050);
}

void test_replay_reduce()
{
    OrderBook book;
    ItchReplay replay(book);

    replay.apply(
        AddOperation{
            5001,
            Side::BUY,
            100,
            10050
        }
    );

    auto result =
        replay.apply(
            ReduceOperation{
                5001,
                40
            }
        );

    assert(
        std::holds_alternative<std::monostate>(result)
    );

    Order* order = book.find_order(5001);

    assert(order != nullptr);
    assert(order->quantity == 60);
}

void test_replay_reduce_to_zero()
{
    OrderBook book;
    ItchReplay replay(book);

    replay.apply(
        AddOperation{
            5001,
            Side::BUY,
            100,
            10050
        }
    );

    auto result =
        replay.apply(
            ReduceOperation{
                5001,
                100
            }
        );

    assert(
        std::holds_alternative<std::monostate>(result)
    );

    assert(
        book.find_order(5001) == nullptr
    );
}

void test_replay_reduce_unknown_order()
{
    OrderBook book;
    ItchReplay replay(book);

    auto result =
        replay.apply(
            ReduceOperation{
                9999,
                10
            }
        );

    assert(
        std::holds_alternative<ReplayError>(result)
    );

    assert(
        std::get<ReplayError>(result) ==
        ReplayError::InvalidLifecycle
    );
}

void test_replay_reduce_invalid_quantity()
{
    OrderBook book;
    ItchReplay replay(book);

    replay.apply(
        AddOperation{
            5001,
            Side::BUY,
            100,
            10050
        }
    );

    auto result =
        replay.apply(
            ReduceOperation{
                5001,
                101
            }
        );

    assert(
        std::holds_alternative<ReplayError>(result)
    );

    assert(
        std::get<ReplayError>(result) ==
        ReplayError::InvalidQuantity
    );

    Order* order = book.find_order(5001);

    assert(order != nullptr);
    assert(order->quantity == 100);
}

void test_replay_remove()
{
    OrderBook book;
    ItchReplay replay(book);

    replay.apply(
        AddOperation{
            5001,
            Side::BUY,
            100,
            10050
        }
    );

    auto result =
        replay.apply(
            RemoveOperation{
                5001
            }
        );

    assert(
        std::holds_alternative<std::monostate>(result)
    );

    assert(
        book.find_order(5001) == nullptr
    );
}

void test_replay_replace()
{
    OrderBook book;
    ItchReplay replay(book);

    replay.apply(
        AddOperation{
            5001,
            Side::BUY,
            100,
            10050
        }
    );

    auto result =
        replay.apply(
            ReplaceOperation{
                5001,
                6001,
                Side::NONE,
                120,
                10100
            }
        );

    assert(
        std::holds_alternative<std::monostate>(result)
    );

    assert(
        book.find_order(5001) == nullptr
    );

    Order* new_order =
        book.find_order(6001);

    assert(new_order != nullptr);
    assert(new_order->side == Side::BUY);
    assert(new_order->quantity == 120);
    assert(new_order->price == 10100);
}

void test_replay_duplicate_add_is_rejected_by_orderbook()
{
    OrderBook book;
    ItchReplay replay(book);

    replay.apply(
        AddOperation{
            5001,
            Side::BUY,
            100,
            10050
        }
    );

    replay.apply(
        AddOperation{
            5001,
            Side::SELL,
            200,
            10100
        }
    );

    Order* order = book.find_order(5001);

    assert(order != nullptr);

    assert(order->side == Side::BUY);
    assert(order->quantity == 100);
    assert(order->price == 10050);
}

// ============================================================
// END-TO-END REPLAY TEST
// ============================================================

void test_end_to_end_replay()
{
    OrderBook book;
    ItchMapper mapper(10);
    ItchReplay replay(book);

    auto parsed_A =
        get_type_parser(make_A());

    assert(
        std::holds_alternative<ItchMessage>(parsed_A)
    );

    auto mapped_A =
        mapper.map(
            std::get<ItchMessage>(parsed_A)
        );

    assert(
        std::holds_alternative<ReplayOperation>(mapped_A)
    );

    auto result_A =
        replay.apply(
            std::get<ReplayOperation>(mapped_A)
        );

    assert(
        std::holds_alternative<std::monostate>(result_A)
    );

    Order* order =
        book.find_order(5001);

    assert(order != nullptr);
    assert(order->quantity == 100);

    auto parsed_X =
        get_type_parser(make_X());

    assert(
        std::holds_alternative<ItchMessage>(parsed_X)
    );

    auto mapped_X =
        mapper.map(
            std::get<ItchMessage>(parsed_X)
        );

    assert(
        std::holds_alternative<ReplayOperation>(mapped_X)
    );

    auto result_X =
        replay.apply(
            std::get<ReplayOperation>(mapped_X)
        );

    assert(
        std::holds_alternative<std::monostate>(result_X)
    );

    order =
        book.find_order(5001);

    assert(order != nullptr);
    assert(order->quantity == 70);

    assert(book.best_bid() == 10050);
}

// ============================================================
// NO HISTORICAL RE-MATCHING
// ============================================================

void test_no_historical_rematching()
{
    OrderBook book;
    ItchReplay replay(book);

    replay.apply(
        AddOperation{
            5001,
            Side::SELL,
            100,
            10100
        }
    );

    auto result =
        replay.apply(
            ReduceOperation{
                5001,
                40
            }
        );

    assert(
        std::holds_alternative<std::monostate>(result)
    );

    Order* order =
        book.find_order(5001);

    assert(order != nullptr);
    assert(order->quantity == 60);

    assert(book.best_ask() == 10100);
}

// ============================================================
// DETERMINISTIC REPLAY
// ============================================================

void test_deterministic_replay()
{
    OrderBook book1;
    OrderBook book2;

    ItchReplay replay1(book1);
    ItchReplay replay2(book2);

    std::vector<ReplayOperation> operations = {

        AddOperation{
            1001,
            Side::BUY,
            100,
            10000
        },

        AddOperation{
            1002,
            Side::SELL,
            80,
            10100
        },

        ReduceOperation{
            1001,
            40
        },

        ReplaceOperation{
            1002,
            1003,
            Side::NONE,
            70,
            10200
        },

        RemoveOperation{
            1001
        }
    };

    for (const auto& operation : operations) {

        auto result1 =
            replay1.apply(operation);

        auto result2 =
            replay2.apply(operation);

        assert(
            std::holds_alternative<std::monostate>(result1)
        );

        assert(
            std::holds_alternative<std::monostate>(result2)
        );
    }

    assert(
        book1.best_bid() ==
        book2.best_bid()
    );

    assert(
        book1.best_ask() ==
        book2.best_ask()
    );

    assert(
        book1.spread() ==
        book2.spread()
    );

    Order* order1 =
        book1.find_order(1003);

    Order* order2 =
        book2.find_order(1003);

    assert(order1 != nullptr);
    assert(order2 != nullptr);

    assert(
        order1->side ==
        order2->side
    );

    assert(
        order1->price ==
        order2->price
    );

    assert(
        order1->quantity ==
        order2->quantity
    );

    assert(
        book1.find_order(1001) == nullptr
    );

    assert(
        book2.find_order(1001) == nullptr
    );

    assert(
        book1.find_order(1002) == nullptr
    );

    assert(
        book2.find_order(1002) == nullptr
    );
}

// ============================================================
// SIMPLE TEST RUNNER
// ============================================================

template <typename Function>
void run_test(
    const std::string& name,
    Function function,
    int& passed
)
{
    function();

    std::cout
        << "[PASS] "
        << name
        << '\n';

    ++passed;
}

// ============================================================
// MAIN
// ============================================================

int main()
{
    test_read_bytes();
    test_read_be();
    test_read_side();

    test_A();
    test_F();
    test_E();
    test_C();
    test_X();
    test_D();
    test_U();
    test_R();
    test_S();

    test_empty_message();
    test_unknown_message_type();
    test_invalid_lengths();
    test_invalid_side();
    test_truncated_messages();

    test_mapper_A();
    test_mapper_F();
    test_mapper_E();
    test_mapper_C();
    test_mapper_X();
    test_mapper_D();
    test_mapper_U();
    test_mapper_security_filter();
    test_mapper_R_S_ignored();

    test_replay_add();
    test_replay_reduce();
    test_replay_reduce_to_zero();
    test_replay_reduce_unknown_order();
    test_replay_reduce_invalid_quantity();
    test_replay_remove();
    test_replay_replace();
    test_replay_duplicate_add_is_rejected_by_orderbook();

    test_end_to_end_replay();
    test_no_historical_rematching();
    test_deterministic_replay();

    std::cout << "All ITCH tests passed." << std::endl;

    return 0;
}