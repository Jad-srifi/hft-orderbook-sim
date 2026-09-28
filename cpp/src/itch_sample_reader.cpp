#include <itch_sample_reader.hpp>

#include <iomanip>
#include <iostream>
#include <sstream>
#include <string>
#include <utility>

namespace
{

    constexpr std::uint32_t MAGIC = 0x484C4F42;
    constexpr std::uint32_t VERSION = 1;

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

    void dump_bytes(
        std::ifstream &file,
        std::streampos position,
        std::size_t count)
    {
        const std::streampos original =
            file.tellg();

        file.clear();
        file.seekg(position);

        if (!file)
        {
            file.clear();
            file.seekg(original);
            return;
        }

        std::cerr
            << "[DEBUG] Bytes at offset "
            << static_cast<long long>(position)
            << ": ";

        for (std::size_t i = 0; i < count; ++i)
        {
            char byte = 0;

            file.read(
                &byte,
                1);

            if (!file)
            {
                break;
            }

            std::cerr
                << std::hex
                << std::setw(2)
                << std::setfill('0')
                << static_cast<unsigned int>(
                       static_cast<unsigned char>(byte))
                << ' ';
        }

        std::cerr
            << std::dec
            << std::setfill(' ')
            << '\n';

        file.clear();
        file.seekg(original);
    }

    bool read_u8(
        std::ifstream &file,
        std::uint8_t &value)
    {
        char byte = 0;

        file.read(
            &byte,
            1);

        if (!file)
        {
            return false;
        }

        value =
            static_cast<std::uint8_t>(
                static_cast<unsigned char>(byte));

        return true;
    }

    bool read_u32_be(
        std::ifstream &file,
        std::uint32_t &value)
    {
        std::uint8_t b0 = 0;
        std::uint8_t b1 = 0;
        std::uint8_t b2 = 0;
        std::uint8_t b3 = 0;

        if (!read_u8(file, b0))
            return false;

        if (!read_u8(file, b1))
            return false;

        if (!read_u8(file, b2))
            return false;

        if (!read_u8(file, b3))
            return false;

        value =
            (static_cast<std::uint32_t>(b0) << 24) |
            (static_cast<std::uint32_t>(b1) << 16) |
            (static_cast<std::uint32_t>(b2) << 8) |
            static_cast<std::uint32_t>(b3);

        return true;
    }

    bool read_u64_be(
        std::ifstream &file,
        std::uint64_t &value)
    {
        std::uint8_t bytes[8]{};

        for (std::size_t i = 0; i < 8; ++i)
        {
            if (!read_u8(file, bytes[i]))
            {
                return false;
            }
        }

        value = 0;

        for (std::size_t i = 0; i < 8; ++i)
        {
            value =
                (value << 8) |
                static_cast<std::uint64_t>(
                    bytes[i]);
        }

        return true;
    }

    bool read_i64_be(
        std::ifstream &file,
        std::int64_t &value)
    {
        std::uint64_t raw = 0;

        if (!read_u64_be(
                file,
                raw))
        {
            return false;
        }

        value =
            static_cast<std::int64_t>(
                raw);

        return true;
    }

    bool read_string(
        std::ifstream &file,
        std::string &value)
    {
        std::uint32_t size = 0;

        if (!read_u32_be(
                file,
                size))
        {
            return false;
        }

        if (size > 16U * 1024U * 1024U)
        {
            return false;
        }

        value.resize(size);

        if (size == 0)
        {
            return true;
        }

        file.read(
            value.data(),
            static_cast<std::streamsize>(size));

        return static_cast<bool>(file);
    }

    bool read_bytes(
        std::ifstream &file,
        std::vector<std::uint8_t> &value)
    {
        std::uint32_t size = 0;

        if (!read_u32_be(
                file,
                size))
        {
            return false;
        }

        if (size > 1024U * 1024U)
        {
            return false;
        }

        value.resize(size);

        if (size == 0)
        {
            return true;
        }

        file.read(
            reinterpret_cast<char *>(value.data()),
            static_cast<std::streamsize>(size));

        return static_cast<bool>(file);
    }

    bool valid_dataset_kind(
        std::uint8_t value)
    {
        return value ==
                   static_cast<std::uint8_t>(
                       DatasetKind::Stock100) ||
               value ==
                   static_cast<std::uint8_t>(
                       DatasetKind::Stock1000) ||
               value ==
                   static_cast<std::uint8_t>(
                       DatasetKind::Stock10000) ||
               value ==
                   static_cast<std::uint8_t>(
                       DatasetKind::Stock100000) ||
               value ==
                   static_cast<std::uint8_t>(
                       DatasetKind::Stock123All);
    }

    bool valid_message_type(
        std::uint8_t value)
    {
        return value ==
                   static_cast<std::uint8_t>(
                       SerializedMessageType::AddOrder) ||
               value ==
                   static_cast<std::uint8_t>(
                       SerializedMessageType::AddOrderMPID) ||
               value ==
                   static_cast<std::uint8_t>(
                       SerializedMessageType::OrderExecuted) ||
               value ==
                   static_cast<std::uint8_t>(
                       SerializedMessageType::OrderExecutedWithPrice) ||
               value ==
                   static_cast<std::uint8_t>(
                       SerializedMessageType::OrderCancel) ||
               value ==
                   static_cast<std::uint8_t>(
                       SerializedMessageType::OrderDelete) ||
               value ==
                   static_cast<std::uint8_t>(
                       SerializedMessageType::OrderReplace) ||
               value ==
                   static_cast<std::uint8_t>(
                       SerializedMessageType::StockDirectory) ||
               value ==
                   static_cast<std::uint8_t>(
                       SerializedMessageType::SystemEvent) ||
               value ==
                   static_cast<std::uint8_t>(
                       SerializedMessageType::Other);
    }

    bool valid_operation_type(
        std::uint8_t value)
    {
        return value ==
                   static_cast<std::uint8_t>(
                       SerializedOperationType::Add) ||
               value ==
                   static_cast<std::uint8_t>(
                       SerializedOperationType::Reduce) ||
               value ==
                   static_cast<std::uint8_t>(
                       SerializedOperationType::Remove) ||
               value ==
                   static_cast<std::uint8_t>(
                       SerializedOperationType::Replace) ||
               value ==
                   static_cast<std::uint8_t>(
                       SerializedOperationType::Ignored) ||
               value ==
                   static_cast<std::uint8_t>(
                       SerializedOperationType::Error);
    }

    void append_field(
        std::ostringstream &output,
        const std::string &key,
        const std::string &value)
    {
        if (
            output.tellp() !=
            std::streampos(0))
        {
            output << ", ";
        }

        output
            << key
            << "="
            << value;
    }

    bool read_u64_field(
        std::ifstream &file,
        std::ostringstream &output,
        std::string &failed_field)
    {
        std::string key;

        if (!read_string(
                file,
                key))
        {
            failed_field = "field key";
            return false;
        }

        std::uint64_t value = 0;

        if (!read_u64_be(
                file,
                value))
        {
            failed_field = key;
            return false;
        }

        append_field(
            output,
            key,
            std::to_string(value));

        return true;
    }

    bool read_i64_field(
        std::ifstream &file,
        std::ostringstream &output,
        std::string &failed_field)
    {
        std::string key;

        if (!read_string(
                file,
                key))
        {
            failed_field = "field key";
            return false;
        }

        std::int64_t value = 0;

        if (!read_i64_be(
                file,
                value))
        {
            failed_field = key;
            return false;
        }

        append_field(
            output,
            key,
            std::to_string(value));

        return true;
    }

    bool read_u8_field(
        std::ifstream &file,
        std::ostringstream &output,
        std::string &failed_field)
    {
        std::string key;

        if (!read_string(
                file,
                key))
        {
            failed_field = "field key";
            return false;
        }

        std::uint8_t value = 0;

        if (!read_u8(
                file,
                value))
        {
            failed_field = key;
            return false;
        }

        append_field(
            output,
            key,
            std::to_string(value));

        return true;
    }

    bool read_string_field(
        std::ifstream &file,
        std::ostringstream &output,
        std::string &failed_field)
    {
        std::string key;

        if (!read_string(
                file,
                key))
        {
            failed_field = "field key";
            return false;
        }

        std::string value;

        if (!read_string(
                file,
                value))
        {
            failed_field = key;
            return false;
        }

        append_field(
            output,
            key,
            value);

        return true;
    }

    bool read_message_fields(
        std::ifstream &file,
        SerializedMessageType type,
        std::string &result,
        std::string &failed_field,
        int &failed_field_number)
    {
        std::ostringstream output;
        int field_number = 0;

#define READ_U64_FIELD()                        \
    do                                          \
    {                                           \
        ++field_number;                         \
        if (!read_u64_field(                    \
                file,                           \
                output,                         \
                failed_field))                  \
        {                                       \
            failed_field_number = field_number; \
            return false;                       \
        }                                       \
    } while (false)

#define READ_I64_FIELD()                        \
    do                                          \
    {                                           \
        ++field_number;                         \
        if (!read_i64_field(                    \
                file,                           \
                output,                         \
                failed_field))                  \
        {                                       \
            failed_field_number = field_number; \
            return false;                       \
        }                                       \
    } while (false)

#define READ_U8_FIELD()                         \
    do                                          \
    {                                           \
        ++field_number;                         \
        if (!read_u8_field(                     \
                file,                           \
                output,                         \
                failed_field))                  \
        {                                       \
            failed_field_number = field_number; \
            return false;                       \
        }                                       \
    } while (false)

#define READ_STRING_FIELD()                     \
    do                                          \
    {                                           \
        ++field_number;                         \
        if (!read_string_field(                 \
                file,                           \
                output,                         \
                failed_field))                  \
        {                                       \
            failed_field_number = field_number; \
            return false;                       \
        }                                       \
    } while (false)

        switch (type)
        {
        case SerializedMessageType::AddOrder:
        {
            READ_U64_FIELD();
            READ_U64_FIELD();
            READ_U64_FIELD();
            READ_U64_FIELD();
            READ_STRING_FIELD();
            READ_U64_FIELD();
            READ_STRING_FIELD();
            READ_I64_FIELD();
            break;
        }

        case SerializedMessageType::AddOrderMPID:
        {
            READ_U64_FIELD();
            READ_U64_FIELD();
            READ_U64_FIELD();
            READ_U64_FIELD();
            READ_STRING_FIELD();
            READ_U64_FIELD();
            READ_STRING_FIELD();
            READ_I64_FIELD();
            READ_STRING_FIELD();
            break;
        }

        case SerializedMessageType::OrderExecuted:
        {
            READ_U64_FIELD();
            READ_U64_FIELD();
            READ_U64_FIELD();
            READ_U64_FIELD();
            READ_U64_FIELD();
            READ_U64_FIELD();
            break;
        }

        case SerializedMessageType::OrderExecutedWithPrice:
        {
            READ_U64_FIELD();
            READ_U64_FIELD();
            READ_U64_FIELD();
            READ_U64_FIELD();
            READ_U64_FIELD();
            READ_U64_FIELD();
            READ_U8_FIELD();
            READ_I64_FIELD();
            break;
        }

        case SerializedMessageType::OrderCancel:
        {
            READ_U64_FIELD();
            READ_U64_FIELD();
            READ_U64_FIELD();
            READ_U64_FIELD();
            READ_U64_FIELD();
            break;
        }

        case SerializedMessageType::OrderDelete:
        {
            READ_U64_FIELD();
            READ_U64_FIELD();
            READ_U64_FIELD();
            READ_U64_FIELD();
            break;
        }

        case SerializedMessageType::OrderReplace:
        {
            READ_U64_FIELD();
            READ_U64_FIELD();
            READ_U64_FIELD();
            READ_U64_FIELD();
            READ_U64_FIELD();
            READ_U64_FIELD();
            READ_I64_FIELD();
            break;
        }

        case SerializedMessageType::StockDirectory:
        case SerializedMessageType::SystemEvent:
        case SerializedMessageType::Other:
        {
            READ_STRING_FIELD();
            break;
        }
        }

#undef READ_U64_FIELD
#undef READ_I64_FIELD
#undef READ_U8_FIELD
#undef READ_STRING_FIELD

        result = output.str();
        return true;
    }

    bool read_operation(
        std::ifstream &file,
        SerializedOperationType &operation_type,
        std::string &mapping_result)
    {
        std::uint8_t raw_type = 0;

        if (!read_u8(
                file,
                raw_type))
        {
            return false;
        }

        if (!valid_operation_type(
                raw_type))
        {
            return false;
        }

        operation_type =
            static_cast<SerializedOperationType>(
                raw_type);

        std::string operation_name;

        if (!read_string(
                file,
                operation_name))
        {
            return false;
        }

        std::ostringstream output;

        output << operation_name;

        switch (operation_type)
        {
        case SerializedOperationType::Add:
        {
            std::ostringstream fields;
            std::string failed_field;

            if (!read_u64_field(
                    file,
                    fields,
                    failed_field))
            {
                return false;
            }

            if (!read_string_field(
                    file,
                    fields,
                    failed_field))
            {
                return false;
            }

            if (!read_u64_field(
                    file,
                    fields,
                    failed_field))
            {
                return false;
            }

            if (!read_i64_field(
                    file,
                    fields,
                    failed_field))
            {
                return false;
            }

            output
                << "("
                << fields.str()
                << ")";

            break;
        }

        case SerializedOperationType::Reduce:
        {
            std::ostringstream fields;
            std::string failed_field;

            if (!read_u64_field(
                    file,
                    fields,
                    failed_field))
            {
                return false;
            }

            if (!read_u64_field(
                    file,
                    fields,
                    failed_field))
            {
                return false;
            }

            output
                << "("
                << fields.str()
                << ")";

            break;
        }

        case SerializedOperationType::Remove:
        {
            std::ostringstream fields;
            std::string failed_field;

            if (!read_u64_field(
                    file,
                    fields,
                    failed_field))
            {
                return false;
            }

            output
                << "("
                << fields.str()
                << ")";

            break;
        }

        case SerializedOperationType::Replace:
        {
            std::ostringstream fields;
            std::string failed_field;

            if (!read_u64_field(
                    file,
                    fields,
                    failed_field))
            {
                return false;
            }

            if (!read_u64_field(
                    file,
                    fields,
                    failed_field))
            {
                return false;
            }

            if (!read_string_field(
                    file,
                    fields,
                    failed_field))
            {
                return false;
            }

            if (!read_u64_field(
                    file,
                    fields,
                    failed_field))
            {
                return false;
            }

            if (!read_i64_field(
                    file,
                    fields,
                    failed_field))
            {
                return false;
            }

            output
                << "("
                << fields.str()
                << ")";

            break;
        }

        case SerializedOperationType::Ignored:
        case SerializedOperationType::Error:
        {
            break;
        }
        }

        mapping_result =
            output.str();

        return true;
    }

}

ItchSampleReader::ItchSampleReader(
    const std::string &file_path,
    bool debug_mode)
    : debug_mode(debug_mode)
{
    file.open(
        file_path,
        std::ios::binary);

    if (!file.is_open())
    {
        open_failed = true;
    }
}

std::variant<
    SampleHeader,
    SampleReaderError>
ItchSampleReader::read_header()
{
    if (open_failed)
    {
        return SampleReaderError::CanNotOpenFile;
    }

    std::uint32_t magic = 0;
    std::uint32_t version = 0;

    std::uint8_t dataset_kind = 0;
    std::uint8_t reserved_1 = 0;
    std::uint8_t reserved_2 = 0;
    std::uint8_t reserved_3 = 0;

    std::uint64_t stock_locate = 0;
    std::uint64_t target_count = 0;

    if (!read_u32_be(
            file,
            magic))
    {
        return SampleReaderError::UnexpectedEOF;
    }

    if (magic != MAGIC)
    {
        return SampleReaderError::InvalidMagic;
    }

    if (!read_u32_be(
            file,
            version))
    {
        return SampleReaderError::UnexpectedEOF;
    }

    if (version != VERSION)
    {
        return SampleReaderError::UnsupportedVersion;
    }

    if (!read_u8(
            file,
            dataset_kind))
    {
        return SampleReaderError::UnexpectedEOF;
    }

    if (!read_u8(
            file,
            reserved_1))
    {
        return SampleReaderError::UnexpectedEOF;
    }

    if (!read_u8(
            file,
            reserved_2))
    {
        return SampleReaderError::UnexpectedEOF;
    }

    if (!read_u8(
            file,
            reserved_3))
    {
        return SampleReaderError::UnexpectedEOF;
    }

    if (!valid_dataset_kind(
            dataset_kind))
    {
        return SampleReaderError::InvalidDatasetKind;
    }

    if (!read_u64_be(
            file,
            stock_locate))
    {
        return SampleReaderError::UnexpectedEOF;
    }

    if (!read_u64_be(
            file,
            target_count))
    {
        return SampleReaderError::UnexpectedEOF;
    }

    return SampleHeader{
        magic,
        version,
        static_cast<DatasetKind>(
            dataset_kind),
        stock_locate,
        target_count};
}

std::variant<
    SampleRecord,
    SampleReaderError>
ItchSampleReader::next_record()
{
    if (open_failed)
    {
        return SampleReaderError::CanNotOpenFile;
    }

    if (finished)
    {
        return SampleReaderError::UnexpectedEOF;
    }

    const std::streampos record_start =
        file.tellg();

    std::uint64_t record_number = 0;

    if (!read_u64_be(
            file,
            record_number))
    {
        finished = true;
        return SampleReaderError::UnexpectedEOF;
    }

    std::uint8_t raw_message_type = 0;

    if (!read_u8(
            file,
            raw_message_type))
    {
        return SampleReaderError::UnexpectedEOF;
    }

    if (!valid_message_type(
            raw_message_type))
    {
        return SampleReaderError::InvalidMessageType;
    }

    const SerializedMessageType message_type =
        static_cast<SerializedMessageType>(
            raw_message_type);

    std::string message_class_name;

    if (!read_string(
            file,
            message_class_name))
    {
        return SampleReaderError::UnexpectedEOF;
    }

    std::vector<std::uint8_t> raw_payload;

    if (!read_bytes(
            file,
            raw_payload))
    {
        return SampleReaderError::UnexpectedEOF;
    }

    if (debug_mode)
    {
        std::cerr
            << "[DEBUG] Record "
            << record_number
            << " offset before fields: "
            << static_cast<long long>(
                   file.tellg())
            << '\n';

        std::cerr
            << "        Message type: "
            << message_type_name(
                   message_type)
            << '\n';

        std::cerr
            << "        Raw payload bytes: "
            << raw_payload.size()
            << '\n';

        std::cerr
            << "        Class: "
            << message_class_name
            << '\n';

        dump_bytes(
            file,
            file.tellg(),
            96);
    }

    std::string decoded_fields;
    std::string failed_field;
    int failed_field_number = 0;

    if (!read_message_fields(
            file,
            message_type,
            decoded_fields,
            failed_field,
            failed_field_number))
    {
        if (debug_mode)
        {
            std::cerr
                << "        Message field decoding FAILED\n"
                << "        Field number: "
                << failed_field_number
                << '\n'
                << "        Field: "
                << failed_field
                << '\n';
        }

        return SampleReaderError::InvalidFieldLength;
    }

    if (debug_mode)
    {
        std::cerr
            << "        Message field decoding OK\n";
    }

    SerializedOperationType operation_type;

    std::string mapping_result;

    if (!read_operation(
            file,
            operation_type,
            mapping_result))
    {
        if (debug_mode)
        {
            std::cerr
                << "        Operation decoding FAILED\n";
        }

        return SampleReaderError::InvalidOperationType;
    }

    if (debug_mode)
    {
        std::cerr
            << "        Operation decoding OK\n"
            << "        Record end offset: "
            << static_cast<long long>(
                   file.tellg())
            << '\n';
    }

    return SampleRecord{
        record_number,
        message_type,
        std::move(message_class_name),
        std::move(raw_payload),
        std::move(decoded_fields),
        operation_type,
        std::move(mapping_result)};
}