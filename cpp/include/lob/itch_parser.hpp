#pragma once

#include <itch_message.hpp>
#include <variant>
#include <vector>
#include <cstdint>

std::vector<Byte> read_bytes(const std::vector<Byte>& bytes, size_t start, size_t count);
int64_t read_be(const std::vector<Byte>& bytes, size_t start, size_t count);
StockSymbol read_chars(const std::vector<Byte>& bytes, size_t start,size_t count);
Side read_side(Byte byte);

std::variant<ItchMessage, ParseError> get_type_parser(const std::vector<Byte>& bytes);

std::variant<AddOrderMessage, ParseError> parse_A(const std::vector<Byte>& bytes);
std::variant<AddOrderMPIDMessage, ParseError> parse_F(const std::vector<Byte>& bytes);

std::variant<ExecuteMessage, ParseError> parse_E(const std::vector<Byte>& bytes);
std::variant<ExecuteWithPriceMessage, ParseError> parse_C(const std::vector<Byte>& bytes);

std::variant<CancelMessage, ParseError> parse_X(const std::vector<Byte>& bytes);
std::variant<DeleteMessage, ParseError> parse_D(const std::vector<Byte>& bytes);
std::variant<ReplaceMessage, ParseError> parse_U(const std::vector<Byte>& bytes);

std::variant<StockDirectoryMessage, ParseError> parse_R(const std::vector<Byte>& bytes);
std::variant<SystemEventMessage, ParseError> parse_S(const std::vector<Byte>& bytes);

std::variant<StockTradingActionMessage, ParseError> parse_H(const std::vector<Byte>& bytes);
std::variant<RegSHOMessage, ParseError> parse_Y(const std::vector<Byte>& bytes);
std::variant<MarketParticipantPositionMessage, ParseError> parse_L(const std::vector<Byte>& bytes);
std::variant<MWCBDeclineLevelMessage, ParseError> parse_V(const std::vector<Byte>& bytes);
std::variant<MWCBStatusMessage, ParseError> parse_W(const std::vector<Byte>& bytes);
std::variant<QuotingPeriodUpdateMessage, ParseError> parse_K(const std::vector<Byte>& bytes);
std::variant<LULDAuctionCollarMessage, ParseError> parse_J(const std::vector<Byte>& bytes);
std::variant<OperationalHaltMessage, ParseError> parse_h(const std::vector<Byte>& bytes);
std::variant<TradeMessage, ParseError> parse_P(const std::vector<Byte>& bytes);
std::variant<CrossTradeMessage, ParseError> parse_Q(const std::vector<Byte>& bytes);
std::variant<BrokenTradeMessage, ParseError> parse_B(const std::vector<Byte>& bytes);
std::variant<NOIIMessage, ParseError> parse_I(const std::vector<Byte>& bytes);
std::variant<RetailInterestMessage, ParseError> parse_N(const std::vector<Byte>& bytes);
std::variant<DirectListingCapitalRaiseMessage, ParseError> parse_O(const std::vector<Byte>& bytes);