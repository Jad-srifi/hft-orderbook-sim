#include <itch_file_reader.hpp>
#include <itch_parser.hpp>
#include <itch_mapper.hpp>
#include <itch_replay.hpp>
#include <order_book.hpp>

#include <cstdint>
#include <iomanip>
#include <iostream>
#include <string>
#include <variant>
#include <vector>

enum class ReadMode
{
    Count,
    All
};

struct ReplayConfig
{
    std::string file_path;
    StockLocate stock_locate;
    ReadMode mode;
    std::uint64_t message_limit;
};

static void print_usage()
{
    std::cout
        << "Usage:\n"
        << "  replay_itch_main <file> <stock_locate> <N>\n"
        << "  replay_itch_main <file> <stock_locate> all\n\n"
        << "Examples:\n"
        << "  replay_itch_main "
        << "data/itch/2019-10-18/raw/decompressed/S101819-v50.txt "
        << "123 1000\n\n"
        << "  replay_itch_main "
        << "data/itch/2019-10-18/raw/decompressed/S101819-v50.txt "
        << "123 all\n";
}

static bool parse_uint64(
    const std::string &text,
    std::uint64_t &value)
{
    try
    {

        std::size_t position = 0;

        unsigned long long parsed =
            std::stoull(text, &position);

        if (position != text.size())
        {
            return false;
        }

        value =
            static_cast<std::uint64_t>(parsed);

        return true;
    }
    catch (...)
    {
        return false;
    }
}

static bool parse_config(
    int argc,
    char *argv[],
    ReplayConfig &config)
{
    if (argc != 4)
    {
        print_usage();
        return false;
    }

    config.file_path =
        argv[1];

    std::uint64_t stock_locate_value = 0;

    if (
        !parse_uint64(
            argv[2],
            stock_locate_value))
    {
        std::cerr
            << "Invalid stock locate: "
            << argv[2]
            << '\n';

        return false;
    }

    config.stock_locate =
        static_cast<StockLocate>(
            stock_locate_value);

    const std::string mode =
        argv[3];

    if (mode == "all")
    {

        config.mode =
            ReadMode::All;

        config.message_limit = 0;

        return true;
    }

    std::uint64_t message_count = 0;

    if (
        !parse_uint64(
            mode,
            message_count))
    {
        std::cerr
            << "Invalid message count: "
            << mode
            << '\n';

        return false;
    }

    if (message_count == 0)
    {
        std::cerr
            << "Message count must be greater than zero.\n";

        return false;
    }

    config.mode =
        ReadMode::Count;

    config.message_limit =
        message_count;

    return true;
}

static const char *parse_error_name(
    ParseError error)
{
    switch (error)
    {

    case ParseError::UnknownMessageType:
        return "UnknownMessageType";

    case ParseError::IncompleteMessage:
        return "IncompleteMessage";

    case ParseError::InvalidMessageLength:
        return "InvalidMessageLength";

    case ParseError::MalformedMessage:
        return "MalformedMessage";
    }

    return "UnknownParseError";
}

static const char *replay_error_name(
    ReplayError error)
{
    switch (error)
    {

    case ReplayError::UnknownOrder:
        return "UnknownOrder";

    case ReplayError::InvalidLifecycle:
        return "InvalidLifecycle";

    case ReplayError::InvalidQuantity:
        return "InvalidQuantity";

    case ReplayError::IgnoredMessage:
        return "IgnoredMessage";
    }

    return "UnknownReplayError";
}

static const char *file_reader_error_name(
    FileReaderError error)
{
    switch (error)
    {

    case FileReaderError::CanNotOpenFile:
        return "CanNotOpenFile";

    case FileReaderError::IncompleteLengthField:
        return "IncompleteLengthField";

    case FileReaderError::TruncatedPayload:
        return "TruncatedPayload";

    case FileReaderError::ReadFailure:
        return "ReadFailure";
    }

    return "UnknownFileReaderError";
}

static void print_message_diagnostic(
    const std::vector<Byte> &payload,
    std::uint64_t message_number)
{
    std::cerr
        << "\n===== MESSAGE DIAGNOSTIC =====\n";

    std::cerr
        << "Message number: "
        << message_number
        << '\n';

    std::cerr
        << "Payload size: "
        << payload.size()
        << '\n';

    if (payload.empty())
    {

        std::cerr
            << "Payload is empty.\n";

        return;
    }

    Byte first_byte =
        payload[0];

    std::cerr
        << "First byte decimal: "
        << static_cast<unsigned int>(
               first_byte)
        << '\n';

    std::cerr
        << "First byte hex: 0x"
        << std::hex
        << std::uppercase
        << std::setw(2)
        << std::setfill('0')
        << static_cast<unsigned int>(
               first_byte)
        << std::dec
        << '\n';

    std::cerr
        << "First byte character: '"
        << static_cast<char>(
               first_byte)
        << "'\n";

    std::cerr
        << "First bytes: ";

    const std::size_t bytes_to_print =
        payload.size() < 16
            ? payload.size()
            : 16;

    for (
        std::size_t i = 0;
        i < bytes_to_print;
        ++i)
    {

        std::cerr
            << "0x"
            << std::hex
            << std::uppercase
            << std::setw(2)
            << std::setfill('0')
            << static_cast<unsigned int>(
                   payload[i])
            << ' ';
    }

    std::cerr
        << std::dec
        << '\n';
}

int main(
    int argc,
    char *argv[])
{
    ReplayConfig config{};

    if (
        !parse_config(
            argc,
            argv,
            config))
    {
        return 1;
    }

    std::cout
        << "===== ITCH REPLAY =====\n";

    std::cout
        << "File: "
        << config.file_path
        << '\n';

    std::cout
        << "Selected stock locate: "
        << config.stock_locate
        << '\n';

    if (
        config.mode ==
        ReadMode::All)
    {

        std::cout
            << "Read mode: ALL\n";
    }
    else
    {

        std::cout
            << "Read mode: "
            << config.message_limit
            << " messages\n";
    }

    ItchFileReader reader(
        config.file_path);

    OrderBook order_book;

    ItchMapper mapper(
        config.stock_locate);

    ItchReplay replay(
        order_book);

    std::uint64_t messages_read = 0;
    std::uint64_t messages_parsed = 0;
    std::uint64_t messages_mapped = 0;
    std::uint64_t messages_applied = 0;
    std::uint64_t messages_ignored = 0;
    std::uint64_t parse_errors = 0;
    std::uint64_t replay_errors = 0;

    while (true)
    {

        if (
            config.mode ==
                ReadMode::Count &&
            messages_read >=
                config.message_limit)
        {
            break;
        }

        auto read_result =
            reader.next_message();

        if (
            std::holds_alternative<EndOfFile>(
                read_result))
        {
            break;
        }

        if (
            std::holds_alternative<FileReaderError>(
                read_result))
        {
            FileReaderError error =
                std::get<FileReaderError>(
                    read_result);

            std::cerr
                << "File reader error after "
                << messages_read
                << " messages: "
                << file_reader_error_name(
                       error)
                << '\n';

            return 1;
        }

        const std::vector<Byte> &payload =
            std::get<std::vector<Byte>>(
                read_result);

        ++messages_read;

        auto parse_result =
            get_type_parser(
                payload);

        if (
            std::holds_alternative<ParseError>(
                parse_result))
        {
            ParseError error =
                std::get<ParseError>(
                    parse_result);

            ++parse_errors;

            std::cerr
                << "Parse error at message "
                << messages_read
                << ": "
                << parse_error_name(error)
                << '\n';

            if (
                error ==
                    ParseError::UnknownMessageType ||
                error ==
                    ParseError::InvalidMessageLength)
            {
                print_message_diagnostic(
                    payload,
                    messages_read);
            }

            return 1;
        }

        ++messages_parsed;

        const ItchMessage &message =
            std::get<ItchMessage>(
                parse_result);

        auto map_result =
            mapper.map(
                message);

        if (
            std::holds_alternative<ReplayError>(
                map_result))
        {
            ReplayError error =
                std::get<ReplayError>(
                    map_result);

            if (
                error ==
                ReplayError::IgnoredMessage)
            {
                ++messages_ignored;
                continue;
            }

            ++replay_errors;

            std::cerr
                << "Mapper error at message "
                << messages_read
                << ": "
                << replay_error_name(error)
                << '\n';

            return 1;
        }

        ++messages_mapped;

        const ReplayOperation &operation =
            std::get<ReplayOperation>(
                map_result);

        auto replay_result =
            replay.apply(
                operation);

        if (
            std::holds_alternative<ReplayError>(
                replay_result))
        {
            ReplayError error =
                std::get<ReplayError>(
                    replay_result);

            ++replay_errors;

            std::cerr
                << "Replay error at message "
                << messages_read
                << ": "
                << replay_error_name(error)
                << '\n';

            return 1;
        }

        ++messages_applied;
    }

    std::cout
        << "\n===== REPLAY SUMMARY =====\n";

    std::cout
        << "Messages read:      "
        << messages_read
        << '\n';

    std::cout
        << "Messages parsed:    "
        << messages_parsed
        << '\n';

    std::cout
        << "Messages mapped:    "
        << messages_mapped
        << '\n';

    std::cout
        << "Messages applied:   "
        << messages_applied
        << '\n';

    std::cout
        << "Messages ignored:   "
        << messages_ignored
        << '\n';

    std::cout
        << "Parse errors:       "
        << parse_errors
        << '\n';

    std::cout
        << "Replay errors:      "
        << replay_errors
        << '\n';

    std::cout
        << "\nReplay completed successfully.\n";

    return 0;
}