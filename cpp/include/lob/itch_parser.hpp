#pragma once

#include <itch_message.hpp>
#include <variant>
#include <vector>
#include <cstdint>

using Byte = std::uint8_t;

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