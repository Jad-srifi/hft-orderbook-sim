#include <variant>
#include <vector>
#include <itch_parser.hpp>

using Byte = std::uint8_t;

std::vector<Byte> read_bytes(const std::vector<Byte>& bytes, size_t start, size_t count) {
    if (start + count > bytes.size()) {
        return {};
    }

    std::vector<Byte> result;

    for (size_t i = 0; i < count; i++) {
        result.push_back(bytes[start + i]);
    }

    return result;
}

int64_t read_be(const std::vector<Byte>& bytes, size_t start, size_t count) {
    if (start + count > bytes.size()) {
        return -1;
    }

    std::int64_t result = 0;

    for (size_t i = 0; i < count; i++) {
        result = result << 8 | bytes[start + i];
    }

    return static_cast<int64_t>(result);
}

Side read_side(Byte byte) {
    if (byte == 'B') {
        return Side::BUY;
    }
    
    if (byte == 'S') {
        return Side::SELL;
    }
    
    return Side::NONE;
}

bool check_validity(int64_t var) {
    if (var == -1) {
        return false;
    }
    return true;
}

template <std::size_t N>
std::array<char, N> read_chars(const std::vector<Byte>& bytes, std::size_t start) {
    if (start + N > bytes.size()) {
        return {};
    }

    std::array<char, N> result{};

    for (std::size_t i = 0; i < N; i++) {
        result[i] = static_cast<char>(bytes[start + i]);
    }

    return result;
}

template <typename SpecificMessage>
std::variant<ItchMessage, ParseError> promote_message(const std::variant<SpecificMessage, ParseError>& result) {
    if (std::holds_alternative<ParseError>(result)) {
        return std::get<ParseError>(result);
    }
    return ItchMessage(std::get<SpecificMessage>(result)); 
}

std::variant<ItchMessage, ParseError> get_type_parser(const std::vector<Byte>& bytes) {
    if (bytes.empty()) {
        return std::variant<ItchMessage, ParseError>(std::in_place_type<ParseError>, ParseError::IncompleteMessage);
    }
    
    switch (bytes[0]) {
        case 'A':
            return promote_message(parse_A(bytes));
        case 'F':
            return promote_message(parse_F(bytes));
        case 'E':
            return promote_message(parse_E(bytes));
        case 'C':
            return promote_message(parse_C(bytes));
        case 'X':
            return promote_message(parse_X(bytes));
        case 'D':
            return promote_message(parse_D(bytes));
        case 'U':
            return promote_message(parse_U(bytes));
        case 'R':
            return promote_message(parse_R(bytes));
        case 'S':
            return promote_message(parse_S(bytes));
        case 'H':
            return promote_message(parse_H(bytes));
        case 'Y':
            return promote_message(parse_Y(bytes));
        case 'L':
            return promote_message(parse_L(bytes));
        case 'V':
            return promote_message(parse_V(bytes));
        case 'W':
            return promote_message(parse_W(bytes));
        case 'K':
            return promote_message(parse_K(bytes));
        case 'P':
            return promote_message(parse_P(bytes));
        case 'Q':
            return promote_message(parse_Q(bytes));
        case 'N':
            return promote_message(parse_N(bytes));
        case 'O':
            return promote_message(parse_O(bytes));
        case 'I':
            return promote_message(parse_I(bytes));
        case 'B':
            return promote_message(parse_B(bytes));
        case 'h':
            return promote_message(parse_h(bytes));
        case 'J':
            return promote_message(parse_J(bytes));
        default:
            return ParseError::UnknownMessageType;
    }
}

std::variant<AddOrderMessage, ParseError> parse_A(const std::vector<Byte>& bytes) {
    if (bytes.size() < 36) {
        return ParseError::InvalidMessageLength;
    }

    AddOrderMessage message;

    auto stock_locate = read_be(bytes, 1, 2);
    if (!check_validity(stock_locate)) {
        return ParseError::MalformedMessage;
    }
    message.stock_locate = static_cast<StockLocate>(stock_locate);

    auto tracking_number = read_be(bytes, 3, 2);
    if (!check_validity(tracking_number)) {
        return ParseError::MalformedMessage;
    }
    message.tracking_number = static_cast<TrackingNumber>(tracking_number);

    auto timestamp = read_be(bytes, 5, 6);
    if (!check_validity(timestamp)) {
        return ParseError::MalformedMessage;
    }
    message.timestamp = static_cast<Timestamp>(timestamp);

    auto order_reference = read_be(bytes, 11, 8);
    if (!check_validity(order_reference)) {
        return ParseError::MalformedMessage;
    }
    message.order_reference =
        static_cast<OrderReferenceNumber>(order_reference);

    message.side = read_side(bytes[19]);
    if (message.side == Side::NONE) {
        return ParseError::MalformedMessage;
    }

    auto shares = read_be(bytes, 20, 4);
    if (!check_validity(shares)) {
        return ParseError::MalformedMessage;
    }
    message.shares = static_cast<Shares>(shares);

    message.stock_symbol = read_chars<8>(bytes, 24);

    auto price = read_be(bytes, 32, 4);
    if (!check_validity(price)) {
        return ParseError::MalformedMessage;
    }
    message.price = static_cast<ItchPrice>(price);

    return message;
}

std::variant<AddOrderMPIDMessage, ParseError> parse_F(const std::vector<Byte>& bytes) {
    if (bytes.size() < 40) {
        return ParseError::InvalidMessageLength;
    }

    std::variant<AddOrderMessage, ParseError> msg = parse_A(bytes);

    if (std::holds_alternative<ParseError>(msg)) {
        return std::get<ParseError>(msg);
    }

    AddOrderMessage a = std::get<AddOrderMessage>(msg);

    AddOrderMPIDMessage message;

    message.order_reference = a.order_reference;
    message.tracking_number = a.tracking_number;
    message.price = a.price;
    message.shares = a.shares;
    message.side = a.side;
    message.stock_locate = a.stock_locate;
    message.stock_symbol = a.stock_symbol;
    message.timestamp = a.timestamp;
    message.mpid = read_chars<4>(bytes, 36);

    return message;
}

std::variant<ExecuteMessage, ParseError> parse_E(const std::vector<Byte>& bytes) {
    if (bytes.size() < 31) {
        return ParseError::InvalidMessageLength;
    }

    ExecuteMessage message;

    auto stock_locate = read_be(bytes, 1, 2);
    if (!check_validity(stock_locate)) {
        return ParseError::MalformedMessage;
    }

    message.stock_locate = static_cast<StockLocate>(stock_locate);

    auto tracking_number = read_be(bytes, 3, 2);
    if (!check_validity(tracking_number)) {
        return ParseError::MalformedMessage;
    }

    message.tracking_number = static_cast<TrackingNumber>(tracking_number);

    auto timestamp = read_be(bytes, 5, 6);
    if (!check_validity(timestamp)) {
        return ParseError::MalformedMessage;
    }

    message.timestamp = static_cast<Timestamp>(timestamp);

    auto order_reference = read_be(bytes, 11, 8);
    if (!check_validity(order_reference)) {
        return ParseError::MalformedMessage;
    }

    message.order_reference = static_cast<OrderReferenceNumber>(order_reference);

    auto executed_shares = read_be(bytes, 19, 4);
    if (!check_validity(executed_shares)) {
        return ParseError::MalformedMessage;
    }

    message.executed_shares = static_cast<Shares>(executed_shares);

    auto match_number = read_be(bytes, 23, 8);
    if (!check_validity(match_number)) {
        return ParseError::MalformedMessage;
    }

    message.match_number = static_cast<MatchNumber>(match_number);

    return message;
}

std::variant<ExecuteWithPriceMessage, ParseError> parse_C(const std::vector<Byte>& bytes) {

    if (bytes.size() < 36) {
        return ParseError::InvalidMessageLength;
    }

    std::variant<ExecuteMessage, ParseError> msg = parse_E(bytes);

    if (std::holds_alternative<ParseError>(msg)) {
        return std::get<ParseError>(msg);
    }

    ExecuteMessage a = std::get<ExecuteMessage>(msg);

    ExecuteWithPriceMessage message;

    message.stock_locate = a.stock_locate;

    message.tracking_number = a.tracking_number;

    message.timestamp = a.timestamp;

    message.order_reference = a.order_reference;

    message.executed_shares = a.executed_shares;

    message.match_number = a.match_number;

    auto printable = static_cast<char>(bytes[31]);

    if (printable != 'Y' && printable != 'N') {
        return ParseError::MalformedMessage;
    }

    message.printability = printable;

    auto execution_price = read_be(bytes, 32, 4);

    if (!check_validity(execution_price)) {
        return ParseError::MalformedMessage;
    }

    message.execution_price = static_cast<ItchPrice>(execution_price);

    return message;
}

std::variant<CancelMessage, ParseError> parse_X(const std::vector<Byte>& bytes) {

    if (bytes.size() < 23) {
        return ParseError::InvalidMessageLength;
    }

    CancelMessage message;

    auto stock_locate = read_be(bytes, 1, 2);

    if (!check_validity(stock_locate)) {
        return ParseError::MalformedMessage;
    }

    message.stock_locate = static_cast<StockLocate>(stock_locate);

    auto tracking_number = read_be(bytes, 3, 2);

    if (!check_validity(tracking_number)) {
        return ParseError::MalformedMessage;
    }

    message.tracking_number = static_cast<TrackingNumber>(tracking_number);

    auto timestamp = read_be(bytes, 5, 6);

    if (!check_validity(timestamp)) {
        return ParseError::MalformedMessage;
    }

    message.timestamp =
        static_cast<Timestamp>(timestamp);

    auto order_reference = read_be(bytes, 11, 8);

    if (!check_validity(order_reference)) {
        return ParseError::MalformedMessage;
    }

    message.order_reference =
        static_cast<OrderReferenceNumber>(order_reference);

    auto cancelled_shares = read_be(bytes, 19, 4);

    if (!check_validity(cancelled_shares)) {
        return ParseError::MalformedMessage;
    }

    message.cancelled_shares =
        static_cast<Shares>(cancelled_shares);

    return message;

}

std::variant<DeleteMessage, ParseError> parse_D(const std::vector<Byte>& bytes) {

    if (bytes.size() < 19) {
        return ParseError::InvalidMessageLength;
    }

    DeleteMessage message;

    auto stock_locate = read_be(bytes, 1, 2);

    if (!check_validity(stock_locate)) {
        return ParseError::MalformedMessage;
    }

    message.stock_locate = static_cast<StockLocate>(stock_locate);

    auto tracking_number = read_be(bytes, 3, 2);

    if (!check_validity(tracking_number)) {
        return ParseError::MalformedMessage;
    }

    message.tracking_number = static_cast<TrackingNumber>(tracking_number);

    auto timestamp = read_be(bytes, 5, 6);

    if (!check_validity(timestamp)) {
        return ParseError::MalformedMessage;
    }

    message.timestamp = static_cast<Timestamp>(timestamp);

    auto order_reference = read_be(bytes, 11, 8);

    if (!check_validity(order_reference)) {
        return ParseError::MalformedMessage;
    }

    message.order_reference = static_cast<OrderReferenceNumber>(order_reference);

    return message;

}

std::variant<ReplaceMessage, ParseError> parse_U(const std::vector<Byte>& bytes) {

    if (bytes.size() < 35) {
        return ParseError::InvalidMessageLength;
    }

    ReplaceMessage message;

    auto stock_locate = read_be(bytes, 1, 2);

    if (!check_validity(stock_locate)) {
        return ParseError::MalformedMessage;
    }

    message.stock_locate = static_cast<StockLocate>(stock_locate);

    auto tracking_number = read_be(bytes, 3, 2);

    if (!check_validity(tracking_number)) {
        return ParseError::MalformedMessage;
    }

    message.tracking_number = static_cast<TrackingNumber>(tracking_number);

    auto timestamp = read_be(bytes, 5, 6);

    if (!check_validity(timestamp)) {
        return ParseError::MalformedMessage;
    }

    message.timestamp = static_cast<Timestamp>(timestamp);

    auto old_order_reference = read_be(bytes, 11, 8);

    if (!check_validity(old_order_reference)) {
        return ParseError::MalformedMessage;
    }

    message.old_order_reference = static_cast<OrderReferenceNumber>(old_order_reference);

    auto new_order_reference = read_be(bytes, 19, 8);

    if (!check_validity(new_order_reference)) {
        return ParseError::MalformedMessage;
    }

    message.new_order_reference = static_cast<OrderReferenceNumber>(new_order_reference);

    auto new_shares = read_be(bytes, 27, 4);

    if (!check_validity(new_shares)) {
        return ParseError::MalformedMessage;
    }

    message.new_shares = static_cast<Shares>(new_shares);

    auto new_price = read_be(bytes, 31, 4);

    if (!check_validity(new_price)) {
        return ParseError::MalformedMessage;
    }

    message.new_price = static_cast<ItchPrice>(new_price);

    return message;

}

std::variant<StockDirectoryMessage, ParseError> parse_R(const std::vector<Byte>& bytes) {

    if (bytes.size() < 39) {
        return ParseError::InvalidMessageLength;
    }

    StockDirectoryMessage message;

    auto stock_locate = read_be(bytes, 1, 2);

    if (!check_validity(stock_locate)) {
        return ParseError::MalformedMessage;
    }

    message.stock_locate = static_cast<StockLocate>(stock_locate);

    auto tracking_number = read_be(bytes, 3, 2);

    if (!check_validity(tracking_number)) {
        return ParseError::MalformedMessage;
    }

    message.tracking_number = static_cast<TrackingNumber>(tracking_number);

    auto timestamp = read_be(bytes, 5, 6);

    if (!check_validity(timestamp)) {
        return ParseError::MalformedMessage;
    }

    message.timestamp = static_cast<Timestamp>(timestamp);

    message.stock_symbol = read_chars<8>(bytes, 11);

    message.market_category = static_cast<char>(bytes[19]);

    message.financial_status_indicator = static_cast<char>(bytes[20]);

    auto round_lot_size = read_be(bytes, 21, 4);

    if (!check_validity(round_lot_size)) {
        return ParseError::MalformedMessage;
    }

    message.round_lot_size = static_cast<Shares>(round_lot_size);

    message.round_lots_only = static_cast<char>(bytes[25]);

    message.issue_clarification = static_cast<char>(bytes[26]);

    message.issue_sub_type = read_chars<2>(bytes, 27);

    message.authenticity = static_cast<char>(bytes[29]);

    message.short_sale_threshold = static_cast<char>(bytes[30]);

    message.IPO_flag = static_cast<char>(bytes[31]);

    message.LULD_reference_price_tier = static_cast<char>(bytes[32]);

    message.ETP_flag = static_cast<char>(bytes[33]);

    auto ETP_leverage_factor = read_be(bytes, 34, 4);

    if (!check_validity(ETP_leverage_factor)) {
        return ParseError::MalformedMessage;
    }

    message.ETP_leverage_factor = static_cast<std::uint32_t>(ETP_leverage_factor);

    message.inverse_indicator = static_cast<char>(bytes[38]);

    return message;

}

std::variant<SystemEventMessage, ParseError> parse_S(const std::vector<Byte>& bytes) {

    if (bytes.size() < 12) {
        return ParseError::InvalidMessageLength;
    }

    SystemEventMessage message;

    auto stock_locate = read_be(bytes, 1, 2);

    if (!check_validity(stock_locate)) {
        return ParseError::MalformedMessage;
    }

    message.stock_locate = static_cast<StockLocate>(stock_locate);

    auto tracking_number = read_be(bytes, 3, 2);

    if (!check_validity(tracking_number)) {
        return ParseError::MalformedMessage;
    }

    message.tracking_number = static_cast<TrackingNumber>(tracking_number);

    auto timestamp = read_be(bytes, 5, 6);

    if (!check_validity(timestamp)) {
        return ParseError::MalformedMessage;
    }

    message.timestamp = static_cast<Timestamp>(timestamp);

    auto event_code = static_cast<char>(bytes[11]);

    if (event_code != 'O' &&
        event_code != 'S' &&
        event_code != 'Q' &&
        event_code != 'M' &&
        event_code != 'E' &&
        event_code != 'C') {

        return ParseError::MalformedMessage;

    }

    message.event_code = event_code;

    return message;

}

std::variant<StockTradingActionMessage, ParseError> parse_H(const std::vector<Byte>& bytes) {
    if (bytes.size() != 25) {
        return ParseError::InvalidMessageLength;
    }

    StockTradingActionMessage message{};

    message.stock_locate =
        static_cast<StockLocate>(
            read_be(bytes, 1, 2)
        );

    message.tracking_number =
        static_cast<TrackingNumber>(
            read_be(bytes, 3, 2)
        );

    message.timestamp =
        static_cast<Timestamp>(
            read_be(bytes, 5, 6)
        );

    message.stock_symbol =
        read_chars<8>(bytes, 11);

    message.trading_state =
        static_cast<char>(bytes[19]);

    message.reserved =
        static_cast<char>(bytes[20]);

    for (std::size_t i = 0; i < 4; ++i) {
        message.reason[i] =
            static_cast<char>(bytes[21 + i]);
    }

    return message;
}

std::variant<RegSHOMessage, ParseError> parse_Y(const std::vector<Byte>& bytes) {
    if (bytes.size() != 20) {
        return ParseError::InvalidMessageLength;
    }

    RegSHOMessage message{};

    message.stock_locate =
        static_cast<StockLocate>(
            read_be(bytes, 1, 2)
        );

    message.tracking_number =
        static_cast<TrackingNumber>(
            read_be(bytes, 3, 2)
        );

    message.timestamp =
        static_cast<Timestamp>(
            read_be(bytes, 5, 6)
        );

    message.stock_symbol =
        read_chars<8>(bytes, 11);

    message.reg_sho_action =
        static_cast<char>(bytes[19]);

    return message;
}

std::variant<MarketParticipantPositionMessage, ParseError> parse_L(const std::vector<Byte>& bytes) {
    if (bytes.size() != 26) {
        return ParseError::InvalidMessageLength;
    }

    MarketParticipantPositionMessage message{};

    message.stock_locate =
        static_cast<StockLocate>(
            read_be(bytes, 1, 2)
        );

    message.tracking_number =
        static_cast<TrackingNumber>(
            read_be(bytes, 3, 2)
        );

    message.timestamp =
        static_cast<Timestamp>(
            read_be(bytes, 5, 6)
        );

    message.mpid =
        read_chars<4>(
            bytes,
            11
        );

    message.stock_symbol =
        read_chars<8>(
            bytes,
            15
        );

    message.primary_market_maker =
        static_cast<char>(
            bytes[23]
        );

    message.market_maker_mode =
        static_cast<char>(
            bytes[24]
        );

    message.market_participant_state =
        static_cast<char>(
            bytes[25]
        );

    return message;
}

std::variant<MWCBDeclineLevelMessage, ParseError>parse_V(const std::vector<Byte>& bytes) {
    if (bytes.size() != 35) {
        return ParseError::InvalidMessageLength;
    }

    MWCBDeclineLevelMessage message{};

    message.stock_locate =
        static_cast<StockLocate>(
            read_be(bytes, 1, 2)
        );

    message.tracking_number =
        static_cast<TrackingNumber>(
            read_be(bytes, 3, 2)
        );

    message.timestamp =
        static_cast<Timestamp>(
            read_be(bytes, 5, 6)
        );

    message.level_1 =
        static_cast<std::uint64_t>(
            read_be(bytes, 11, 8)
        );

    message.level_2 =
        static_cast<std::uint64_t>(
            read_be(bytes, 19, 8)
        );

    message.level_3 =
        static_cast<std::uint64_t>(
            read_be(bytes, 27, 8)
        );

    return message;
}

std::variant<MWCBStatusMessage, ParseError> parse_W(const std::vector<Byte>& bytes) {
    if (bytes.size() != 12) {
        return ParseError::InvalidMessageLength;
    }

    MWCBStatusMessage message{};

    message.stock_locate =
        static_cast<StockLocate>(
            read_be(bytes, 1, 2)
        );

    message.tracking_number =
        static_cast<TrackingNumber>(
            read_be(bytes, 3, 2)
        );

    message.timestamp =
        static_cast<Timestamp>(
            read_be(bytes, 5, 6)
        );

    message.breached_level =
        static_cast<char>(
            bytes[11]
        );

    return message;
}

std::variant<QuotingPeriodUpdateMessage, ParseError> parse_K(const std::vector<Byte>& bytes) {
    if (bytes.size() != 28) {
        return ParseError::InvalidMessageLength;
    }

    QuotingPeriodUpdateMessage message{};

    message.stock_locate =
        static_cast<StockLocate>(
            read_be(bytes, 1, 2)
        );

    message.tracking_number =
        static_cast<TrackingNumber>(
            read_be(bytes, 3, 2)
        );

    message.timestamp =
        static_cast<Timestamp>(
            read_be(bytes, 5, 6)
        );

    message.stock_symbol =
        read_chars<8>(
            bytes,
            11
        );

    message.ipo_quotation_release_time =
        static_cast<std::uint32_t>(
            read_be(bytes, 19, 4)
        );

    message.ipo_quotation_release_qualifier =
        static_cast<char>(
            bytes[23]
        );

    message.ipo_price =
        static_cast<ItchPrice>(
            read_be(bytes, 24, 4)
        );

    return message;
}

std::variant<LULDAuctionCollarMessage, ParseError> parse_J(const std::vector<Byte>& bytes) {
    if (bytes.size() != 35) {
        return ParseError::InvalidMessageLength;
    }

    LULDAuctionCollarMessage message{};

    message.stock_locate =
        static_cast<StockLocate>(
            read_be(bytes, 1, 2)
        );

    message.tracking_number =
        static_cast<TrackingNumber>(
            read_be(bytes, 3, 2)
        );

    message.timestamp =
        static_cast<Timestamp>(
            read_be(bytes, 5, 6)
        );

    message.stock_symbol =
        read_chars<8>(
            bytes,
            11
        );

    message.auction_collar_reference_price =
        static_cast<ItchPrice>(
            read_be(bytes, 19, 4)
        );

    message.upper_auction_collar_price =
        static_cast<ItchPrice>(
            read_be(bytes, 23, 4)
        );

    message.lower_auction_collar_price =
        static_cast<ItchPrice>(
            read_be(bytes, 27, 4)
        );

    message.auction_collar_extension =
        static_cast<std::uint32_t>(
            read_be(bytes, 31, 4)
        );

    return message;
}

std::variant<OperationalHaltMessage, ParseError> parse_h(const std::vector<Byte>& bytes) {
    if (bytes.size() != 21) {
        return ParseError::InvalidMessageLength;
    }

    OperationalHaltMessage message{};

    message.stock_locate =
        static_cast<StockLocate>(
            read_be(bytes, 1, 2)
        );

    message.tracking_number =
        static_cast<TrackingNumber>(
            read_be(bytes, 3, 2)
        );

    message.timestamp =
        static_cast<Timestamp>(
            read_be(bytes, 5, 6)
        );

    message.stock_symbol =
        read_chars<8>(
            bytes,
            11
        );

    message.market_code =
        static_cast<char>(
            bytes[19]
        );

    message.operational_halt_action =
        static_cast<char>(
            bytes[20]
        );

    return message;
}

std::variant<TradeMessage, ParseError> parse_P(const std::vector<Byte>& bytes) {
    if (bytes.size() != 44) {
        return ParseError::InvalidMessageLength;
    }

    TradeMessage message{};

    message.stock_locate =
        static_cast<StockLocate>(
            read_be(bytes, 1, 2)
        );

    message.tracking_number =
        static_cast<TrackingNumber>(
            read_be(bytes, 3, 2)
        );

    message.timestamp =
        static_cast<Timestamp>(
            read_be(bytes, 5, 6)
        );

    message.order_reference =
        static_cast<OrderReferenceNumber>(
            read_be(bytes, 11, 8)
        );

    message.side =
        read_side(
            bytes[19]
        );

    message.shares =
        static_cast<Shares>(
            read_be(bytes, 20, 4)
        );

    message.stock_symbol =
        read_chars<8>(
            bytes,
            24
        );

    message.price =
        static_cast<ItchPrice>(
            read_be(bytes, 32, 4)
        );

    message.match_number =
        static_cast<MatchNumber>(
            read_be(bytes, 36, 8)
        );

    return message;
}

std::variant<CrossTradeMessage, ParseError> parse_Q(const std::vector<Byte>& bytes) {
    if (bytes.size() != 40) {
        return ParseError::InvalidMessageLength;
    }

    CrossTradeMessage message{};

    message.stock_locate =
        static_cast<StockLocate>(
            read_be(bytes, 1, 2)
        );

    message.tracking_number =
        static_cast<TrackingNumber>(
            read_be(bytes, 3, 2)
        );

    message.timestamp =
        static_cast<Timestamp>(
            read_be(bytes, 5, 6)
        );

    message.shares =
        static_cast<Shares>(
            read_be(bytes, 11, 8)
        );

    message.stock_symbol =
        read_chars<8>(
            bytes,
            19
        );

    message.cross_price =
        static_cast<ItchPrice>(
            read_be(bytes, 27, 4)
        );

    message.match_number =
        static_cast<MatchNumber>(
            read_be(bytes, 31, 8)
        );

    message.cross_type =
        static_cast<char>(
            bytes[39]
        );

    return message;
}

std::variant<BrokenTradeMessage, ParseError> parse_B(const std::vector<Byte>& bytes) {
    if (bytes.size() != 19) {
        return ParseError::InvalidMessageLength;
    }

    BrokenTradeMessage message{};

    message.stock_locate =
        static_cast<StockLocate>(
            read_be(bytes, 1, 2)
        );

    message.tracking_number =
        static_cast<TrackingNumber>(
            read_be(bytes, 3, 2)
        );

    message.timestamp =
        static_cast<Timestamp>(
            read_be(bytes, 5, 6)
        );

    message.match_number =
        static_cast<MatchNumber>(
            read_be(bytes, 11, 8)
        );

    return message;
}

std::variant<NOIIMessage, ParseError> parse_I(const std::vector<Byte>& bytes) {
    if (bytes.size() != 50) {
        return ParseError::InvalidMessageLength;
    }

    NOIIMessage message{};

    message.stock_locate =
        static_cast<StockLocate>(
            read_be(bytes, 1, 2)
        );

    message.tracking_number =
        static_cast<TrackingNumber>(
            read_be(bytes, 3, 2)
        );

    message.timestamp =
        static_cast<Timestamp>(
            read_be(bytes, 5, 6)
        );

    message.paired_shares =
        static_cast<std::uint64_t>(
            read_be(bytes, 11, 8)
        );

    message.imbalance_shares =
        static_cast<std::uint64_t>(
            read_be(bytes, 19, 8)
        );

    message.imbalance_direction =
        static_cast<char>(
            bytes[27]
        );

    message.stock_symbol =
        read_chars<8>(
            bytes,
            28
        );

    message.far_price =
        static_cast<ItchPrice>(
            read_be(bytes, 36, 4)
        );

    message.near_price =
        static_cast<ItchPrice>(
            read_be(bytes, 40, 4)
        );

    message.current_reference_price =
        static_cast<ItchPrice>(
            read_be(bytes, 44, 4)
        );

    message.cross_type =
        static_cast<char>(
            bytes[48]
        );

    message.price_variation_indicator =
        static_cast<char>(
            bytes[49]
        );

    return message;
}

std::variant<RetailInterestMessage, ParseError> parse_N(const std::vector<Byte>& bytes) {
    if (bytes.size() != 20) {
        return ParseError::InvalidMessageLength;
    }

    RetailInterestMessage message{};

    message.stock_locate =
        static_cast<StockLocate>(
            read_be(bytes, 1, 2)
        );

    message.tracking_number =
        static_cast<TrackingNumber>(
            read_be(bytes, 3, 2)
        );

    message.timestamp =
        static_cast<Timestamp>(
            read_be(bytes, 5, 6)
        );

    message.stock_symbol =
        read_chars<8>(
            bytes,
            11
        );

    message.interest_flag =
        static_cast<char>(
            bytes[19]
        );

    return message;
}

std::variant<DirectListingCapitalRaiseMessage, ParseError> parse_O(const std::vector<Byte>& bytes) {
    if (bytes.size() != 48) {
        return ParseError::InvalidMessageLength;
    }

    DirectListingCapitalRaiseMessage message{};

    message.stock_locate =
        static_cast<StockLocate>(
            read_be(bytes, 1, 2)
        );

    message.tracking_number =
        static_cast<TrackingNumber>(
            read_be(bytes, 3, 2)
        );

    message.timestamp =
        static_cast<Timestamp>(
            read_be(bytes, 5, 6)
        );

    message.stock_symbol =
        read_chars<8>(
            bytes,
            11
        );

    message.open_eligibility_status =
        static_cast<char>(
            bytes[19]
        );

    message.minimum_allowable_price =
        static_cast<ItchPrice>(
            read_be(bytes, 20, 4)
        );

    message.maximum_allowable_price =
        static_cast<ItchPrice>(
            read_be(bytes, 24, 4)
        );

    message.near_execution_price =
        static_cast<ItchPrice>(
            read_be(bytes, 28, 4)
        );

    message.near_execution_time =
        static_cast<Timestamp>(
            read_be(bytes, 32, 8)
        );

    message.lower_price_range_collar =
        static_cast<ItchPrice>(
            read_be(bytes, 40, 4)
        );

    message.upper_price_range_collar =
        static_cast<ItchPrice>(
            read_be(bytes, 44, 4)
        );

    return message;
}

