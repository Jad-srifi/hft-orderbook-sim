#include <itch_sample_reader.hpp>

#include <cstdint>
#include <iostream>
#include <string>

namespace
{

    const char *dataset_name(
        DatasetKind kind)
    {
        switch (kind)
        {
        case DatasetKind::Stock100:
            return "Stock100";

        case DatasetKind::Stock1000:
            return "Stock1000";

        case DatasetKind::Stock10000:
            return "Stock10000";

        case DatasetKind::Stock100000:
            return "Stock100000";

        case DatasetKind::Stock123All:
            return "Stock123All";
        }

        return "Unknown";
    }

    const char *message_type_name(
        SerializedMessageType type)
    {
        switch (type)
        {
        case SerializedMessageType::AddOrder:
            return "AddOrder";

        case SerializedMessageType::AddOrderMPID:
            return "AddOrderMPID";

        case SerializedMessageType::OrderExecuted:
            return "OrderExecuted";

        case SerializedMessageType::OrderExecutedWithPrice:
            return "OrderExecutedWithPrice";

        case SerializedMessageType::OrderCancel:
            return "OrderCancel";

        case SerializedMessageType::OrderDelete:
            return "OrderDelete";

        case SerializedMessageType::OrderReplace:
            return "OrderReplace";

        case SerializedMessageType::StockDirectory:
            return "StockDirectory";

        case SerializedMessageType::SystemEvent:
            return "SystemEvent";

        case SerializedMessageType::Other:
            return "Other";
        }

        return "Unknown";
    }

    const char *operation_type_name(
        SerializedOperationType type)
    {
        switch (type)
        {
        case SerializedOperationType::Add:
            return "Add";

        case SerializedOperationType::Reduce:
            return "Reduce";

        case SerializedOperationType::Remove:
            return "Remove";

        case SerializedOperationType::Replace:
            return "Replace";

        case SerializedOperationType::Ignored:
            return "Ignored";

        case SerializedOperationType::Error:
            return "Error";
        }

        return "Unknown";
    }

    const char *error_name(
        SampleReaderError error)
    {
        switch (error)
        {
        case SampleReaderError::CanNotOpenFile:
            return "CanNotOpenFile";

        case SampleReaderError::InvalidMagic:
            return "InvalidMagic";

        case SampleReaderError::UnsupportedVersion:
            return "UnsupportedVersion";

        case SampleReaderError::InvalidDatasetKind:
            return "InvalidDatasetKind";

        case SampleReaderError::UnexpectedEOF:
            return "UnexpectedEOF";

        case SampleReaderError::InvalidMessageType:
            return "InvalidMessageType";

        case SampleReaderError::InvalidOperationType:
            return "InvalidOperationType";

        case SampleReaderError::InvalidFieldLength:
            return "InvalidFieldLength";

        case SampleReaderError::ReadFailure:
            return "ReadFailure";
        }

        return "UnknownError";
    }

    bool parse_record_limit(
        const std::string &value,
        bool &read_all,
        std::uint64_t &limit)
    {
        if (value == "all")
        {
            read_all = true;
            limit = 0;
            return true;
        }

        if (value.empty())
        {
            return false;
        }

        std::uint64_t result = 0;

        for (char c : value)
        {
            if (c < '0' || c > '9')
            {
                return false;
            }

            const std::uint64_t digit =
                static_cast<std::uint64_t>(
                    c - '0');

            if (
                result >
                (UINT64_MAX - digit) / 10)
            {
                return false;
            }

            result =
                result * 10 + digit;
        }

        if (result == 0)
        {
            return false;
        }

        read_all = false;
        limit = result;

        return true;
    }

    bool parse_debug_mode(
        const std::string &value,
        bool &debug_mode)
    {
        if (value == "on")
        {
            debug_mode = true;
            return true;
        }

        if (value == "off")
        {
            debug_mode = false;
            return true;
        }

        return false;
    }

    void print_usage()
    {
        std::cerr
            << "Usage:\n"
            << " "
            << "inspect_itch_sample_main.exe"
            << " <sample_file> <N|all> [on|off]\n"
            << '\n'
            << "Examples:\n"
            << " inspect_itch_sample_main.exe stock_100.bin 10\n"
            << " inspect_itch_sample_main.exe stock_100.bin 10 on\n"
            << " inspect_itch_sample_main.exe stock_100.bin 10 off\n"
            << " inspect_itch_sample_main.exe stock_123_all.bin all\n"
            << " inspect_itch_sample_main.exe stock_123_all.bin all on\n";
    }

}

int main(
    int argc,
    char *argv[])
{
    if (argc < 3 || argc > 4)
    {
        print_usage();
        return 1;
    }

    const std::string file_path =
        argv[1];

    bool read_all = false;
    std::uint64_t record_limit = 0;

    if (
        !parse_record_limit(
            argv[2],
            read_all,
            record_limit))
    {
        std::cerr
            << "Invalid record count: "
            << argv[2]
            << '\n';

        print_usage();

        return 1;
    }

    bool debug_mode = false;

    if (argc == 4)
    {
        if (
            !parse_debug_mode(
                argv[3],
                debug_mode))
        {
            std::cerr
                << "Invalid debug mode: "
                << argv[3]
                << '\n';

            print_usage();

            return 1;
        }
    }

    ItchSampleReader reader(
        file_path,
        debug_mode);

    auto header_result =
        reader.read_header();

    if (
        std::holds_alternative<SampleReaderError>(
            header_result))
    {
        const SampleReaderError error =
            std::get<SampleReaderError>(
                header_result);

        std::cerr
            << "Failed to read header: "
            << error_name(error)
            << '\n';

        return 1;
    }

    const SampleHeader &header =
        std::get<SampleHeader>(
            header_result);

    std::cout
        << "===== ITCH SAMPLE =====\n"
        << "File: "
        << file_path
        << '\n'
        << "Dataset: "
        << dataset_name(
               header.dataset_kind)
        << '\n'
        << "Stock Locate: "
        << header.stock_locate
        << '\n'
        << "Target Count: "
        << header.target_count
        << '\n';

    if (read_all)
    {
        std::cout
            << "Inspection Limit: all\n";
    }
    else
    {
        std::cout
            << "Inspection Limit: "
            << record_limit
            << '\n';
    }

    std::cout
        << "Debug Mode: "
        << (debug_mode ? "ON" : "OFF")
        << "\n\n";

    std::uint64_t records_read = 0;

    while (
        read_all ||
        records_read < record_limit)
    {
        auto result =
            reader.next_record();

        if (
            std::holds_alternative<SampleReaderError>(
                result))
        {
            const SampleReaderError error =
                std::get<SampleReaderError>(
                    result);

            if (
                error ==
                SampleReaderError::UnexpectedEOF)
            {
                break;
            }

            std::cerr
                << "Failed while reading record "
                << (records_read + 1)
                << ": "
                << error_name(error)
                << '\n';

            return 1;
        }

        const SampleRecord &record =
            std::get<SampleRecord>(
                result);

        std::cout
            << "Record: "
            << record.record_number
            << '\n'
            << "Message type: "
            << message_type_name(
                   record.message_type)
            << '\n'
            << "Class: "
            << record.message_class_name
            << '\n'
            << "Raw payload bytes: "
            << record.raw_payload.size()
            << '\n'
            << "Decoded: "
            << record.decoded_fields
            << '\n'
            << "Operation: "
            << operation_type_name(
                   record.operation_type)
            << '\n'
            << "Mapping: "
            << record.mapping_result
            << "\n\n";

        ++records_read;
    }

    std::cout
        << "Records inspected: "
        << records_read
        << '\n';

    return 0;
}