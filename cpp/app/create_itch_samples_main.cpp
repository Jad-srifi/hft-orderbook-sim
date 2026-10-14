#include <itch_file_reader.hpp>
#include <itch_mapper.hpp>
#include <itch_message.hpp>
#include <itch_parser.hpp>
#include <types.hpp>

#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <map>
#include <stdexcept>
#include <string>
#include <type_traits>
#include <variant>
#include <vector>

namespace
{

    constexpr std::uint32_t FILE_MAGIC = 0x484C4F42;
    constexpr std::uint32_t FILE_VERSION = 1;

    constexpr StockLocate SPECIAL_STOCK_LOCATE = 123;

    const std::string SAMPLE_DIRECTORY =
        "data/itch/2019-10-18/sample/";

    enum class DatasetKind : std::uint8_t
    {
        Stock100 = 1,
        Stock1000 = 2,
        Stock10000 = 3,
        Stock100000 = 4,
        Stock123All = 5
    };

    enum class SerializedMessageType : std::uint8_t
    {
        AddOrder = 1,
        AddOrderMPID = 2,
        Execute = 3,
        ExecuteWithPrice = 4,
        Cancel = 5,
        Delete = 6,
        Replace = 7,
        StockDirectory = 8,
        SystemEvent = 9,
        Other = 255
    };

    enum class SerializedOperationType : std::uint8_t
    {
        Add = 1,
        Reduce = 2,
        Remove = 3,
        Replace = 4,
        Ignored = 5,
        Error = 255
    };

    struct DatasetTarget
    {
        DatasetKind kind;
        std::string filename;
        StockLocate stock_locate;
        std::uint64_t target_count;
        std::uint64_t current_count;
        std::ofstream file;
        bool opened;
    };

    class BufferWriter
    {
    private:
        std::vector<Byte> data;

    public:
        void write_u8(
            std::uint8_t value)
        {
            data.push_back(
                static_cast<Byte>(value));
        }

        void write_u32(
            std::uint32_t value)
        {
            data.push_back(
                static_cast<Byte>(
                    (value >> 24) & 0xFF));

            data.push_back(
                static_cast<Byte>(
                    (value >> 16) & 0xFF));

            data.push_back(
                static_cast<Byte>(
                    (value >> 8) & 0xFF));

            data.push_back(
                static_cast<Byte>(
                    value & 0xFF));
        }

        void write_u64(
            std::uint64_t value)
        {
            for (int i = 7; i >= 0; --i)
            {
                data.push_back(
                    static_cast<Byte>(
                        (value >> (8 * i)) & 0xFF));
            }
        }

        void write_i64(
            std::int64_t value)
        {
            write_u64(
                static_cast<std::uint64_t>(
                    value));
        }

        void write_string(
            const std::string &value)
        {
            write_u32(
                static_cast<std::uint32_t>(
                    value.size()));

            data.insert(
                data.end(),
                value.begin(),
                value.end());
        }

        void write_bytes(
            const std::vector<Byte> &bytes)
        {
            write_u32(
                static_cast<std::uint32_t>(
                    bytes.size()));

            data.insert(
                data.end(),
                bytes.begin(),
                bytes.end());
        }

        const std::vector<Byte> &get_data() const
        {
            return data;
        }
    };

    std::string side_name(
        Side side)
    {
        if (side == Side::BUY)
        {
            return "BUY";
        }

        if (side == Side::SELL)
        {
            return "SELL";
        }

        return "NONE";
    }

    SerializedMessageType serialized_message_type(
        const ItchMessage &message)
    {
        return std::visit(
            [](const auto &msg)
            {
                using T =
                    std::decay_t<decltype(msg)>;

                if constexpr (
                    std::is_same_v<T, AddOrderMessage>)
                {
                    return SerializedMessageType::AddOrder;
                }
                else if constexpr (
                    std::is_same_v<
                        T,
                        AddOrderMPIDMessage>)
                {
                    return SerializedMessageType::AddOrderMPID;
                }
                else if constexpr (
                    std::is_same_v<T, ExecuteMessage>)
                {
                    return SerializedMessageType::Execute;
                }
                else if constexpr (
                    std::is_same_v<
                        T,
                        ExecuteWithPriceMessage>)
                {
                    return SerializedMessageType::ExecuteWithPrice;
                }
                else if constexpr (
                    std::is_same_v<T, CancelMessage>)
                {
                    return SerializedMessageType::Cancel;
                }
                else if constexpr (
                    std::is_same_v<T, DeleteMessage>)
                {
                    return SerializedMessageType::Delete;
                }
                else if constexpr (
                    std::is_same_v<T, ReplaceMessage>)
                {
                    return SerializedMessageType::Replace;
                }
                else if constexpr (
                    std::is_same_v<
                        T,
                        StockDirectoryMessage>)
                {
                    return SerializedMessageType::StockDirectory;
                }
                else if constexpr (
                    std::is_same_v<
                        T,
                        SystemEventMessage>)
                {
                    return SerializedMessageType::SystemEvent;
                }
                else
                {
                    return SerializedMessageType::Other;
                }
            },
            message);
    }

    std::string message_class_name(
        const ItchMessage &message)
    {
        return std::visit(
            [](const auto &msg)
            {
                using T =
                    std::decay_t<decltype(msg)>;

                if constexpr (
                    std::is_same_v<T, AddOrderMessage>)
                {
                    return std::string(
                        "AddOrderMessage");
                }
                else if constexpr (
                    std::is_same_v<
                        T,
                        AddOrderMPIDMessage>)
                {
                    return std::string(
                        "AddOrderMPIDMessage");
                }
                else if constexpr (
                    std::is_same_v<T, ExecuteMessage>)
                {
                    return std::string(
                        "ExecuteMessage");
                }
                else if constexpr (
                    std::is_same_v<
                        T,
                        ExecuteWithPriceMessage>)
                {
                    return std::string(
                        "ExecuteWithPriceMessage");
                }
                else if constexpr (
                    std::is_same_v<T, CancelMessage>)
                {
                    return std::string(
                        "CancelMessage");
                }
                else if constexpr (
                    std::is_same_v<T, DeleteMessage>)
                {
                    return std::string(
                        "DeleteMessage");
                }
                else if constexpr (
                    std::is_same_v<T, ReplaceMessage>)
                {
                    return std::string(
                        "ReplaceMessage");
                }
                else if constexpr (
                    std::is_same_v<
                        T,
                        StockDirectoryMessage>)
                {
                    return std::string(
                        "StockDirectoryMessage");
                }
                else if constexpr (
                    std::is_same_v<
                        T,
                        SystemEventMessage>)
                {
                    return std::string(
                        "SystemEventMessage");
                }
                else
                {
                    return std::string(
                        "OtherMessage");
                }
            },
            message);
    }

    void write_message_fields(
        BufferWriter &writer,
        const ItchMessage &message)
    {
        std::visit(
            [&writer](const auto &msg)
            {
                using T =
                    std::decay_t<decltype(msg)>;

                if constexpr (
                    std::is_same_v<T, AddOrderMessage>)
                {
                    writer.write_string(
                        "stock_locate");
                    writer.write_u64(
                        msg.stock_locate);

                    writer.write_string(
                        "tracking_number");
                    writer.write_u64(
                        msg.tracking_number);

                    writer.write_string(
                        "timestamp");
                    writer.write_u64(
                        msg.timestamp);

                    writer.write_string(
                        "order_reference_number");
                    writer.write_u64(
                        msg.order_reference);

                    writer.write_string(
                        "side");
                    writer.write_string(
                        side_name(msg.side));

                    writer.write_string(
                        "shares");
                    writer.write_u64(
                        msg.shares);

                    writer.write_string(
                        "stock_symbol");
                    writer.write_string(
                        std::string(
                            msg.stock_symbol.begin(),
                            msg.stock_symbol.end()));

                    writer.write_string(
                        "price");
                    writer.write_i64(
                        msg.price);
                }
                else if constexpr (
                    std::is_same_v<
                        T,
                        AddOrderMPIDMessage>)
                {
                    writer.write_string(
                        "stock_locate");
                    writer.write_u64(
                        msg.stock_locate);

                    writer.write_string(
                        "tracking_number");
                    writer.write_u64(
                        msg.tracking_number);

                    writer.write_string(
                        "timestamp");
                    writer.write_u64(
                        msg.timestamp);

                    writer.write_string(
                        "order_reference_number");
                    writer.write_u64(
                        msg.order_reference);

                    writer.write_string(
                        "side");
                    writer.write_string(
                        side_name(msg.side));

                    writer.write_string(
                        "shares");
                    writer.write_u64(
                        msg.shares);

                    writer.write_string(
                        "stock_symbol");
                    writer.write_string(
                        std::string(
                            msg.stock_symbol.begin(),
                            msg.stock_symbol.end()));

                    writer.write_string(
                        "price");
                    writer.write_i64(
                        msg.price);

                    writer.write_string(
                        "mpid");
                    writer.write_string(
                        std::string(
                            msg.mpid.begin(),
                            msg.mpid.end()));
                }
                else if constexpr (
                    std::is_same_v<T, ExecuteMessage>)
                {
                    writer.write_string(
                        "stock_locate");
                    writer.write_u64(
                        msg.stock_locate);

                    writer.write_string(
                        "tracking_number");
                    writer.write_u64(
                        msg.tracking_number);

                    writer.write_string(
                        "timestamp");
                    writer.write_u64(
                        msg.timestamp);

                    writer.write_string(
                        "order_reference_number");
                    writer.write_u64(
                        msg.order_reference);

                    writer.write_string(
                        "executed_shares");
                    writer.write_u64(
                        msg.executed_shares);

                    writer.write_string(
                        "match_number");
                    writer.write_u64(
                        msg.match_number);
                }
                else if constexpr (
                    std::is_same_v<
                        T,
                        ExecuteWithPriceMessage>)
                {
                    writer.write_string(
                        "stock_locate");
                    writer.write_u64(
                        msg.stock_locate);

                    writer.write_string(
                        "tracking_number");
                    writer.write_u64(
                        msg.tracking_number);

                    writer.write_string(
                        "timestamp");
                    writer.write_u64(
                        msg.timestamp);

                    writer.write_string(
                        "order_reference_number");
                    writer.write_u64(
                        msg.order_reference);

                    writer.write_string(
                        "executed_shares");
                    writer.write_u64(
                        msg.executed_shares);

                    writer.write_string(
                        "match_number");
                    writer.write_u64(
                        msg.match_number);

                    writer.write_string(
                        "printable");
                    writer.write_u8(
                        static_cast<std::uint8_t>(
                            msg.printability));

                    writer.write_string(
                        "execution_price");
                    writer.write_i64(
                        msg.execution_price);
                }
                else if constexpr (
                    std::is_same_v<T, CancelMessage>)
                {
                    writer.write_string(
                        "stock_locate");
                    writer.write_u64(
                        msg.stock_locate);

                    writer.write_string(
                        "tracking_number");
                    writer.write_u64(
                        msg.tracking_number);

                    writer.write_string(
                        "timestamp");
                    writer.write_u64(
                        msg.timestamp);

                    writer.write_string(
                        "order_reference_number");
                    writer.write_u64(
                        msg.order_reference);

                    writer.write_string(
                        "cancelled_shares");
                    writer.write_u64(
                        msg.cancelled_shares);
                }
                else if constexpr (
                    std::is_same_v<T, DeleteMessage>)
                {
                    writer.write_string(
                        "stock_locate");
                    writer.write_u64(
                        msg.stock_locate);

                    writer.write_string(
                        "tracking_number");
                    writer.write_u64(
                        msg.tracking_number);

                    writer.write_string(
                        "timestamp");
                    writer.write_u64(
                        msg.timestamp);

                    writer.write_string(
                        "order_reference_number");
                    writer.write_u64(
                        msg.order_reference);
                }
                else if constexpr (
                    std::is_same_v<T, ReplaceMessage>)
                {
                    writer.write_string(
                        "stock_locate");
                    writer.write_u64(
                        msg.stock_locate);

                    writer.write_string(
                        "tracking_number");
                    writer.write_u64(
                        msg.tracking_number);

                    writer.write_string(
                        "timestamp");
                    writer.write_u64(
                        msg.timestamp);

                    writer.write_string(
                        "old_order_reference_number");
                    writer.write_u64(
                        msg.old_order_reference);

                    writer.write_string(
                        "new_order_reference_number");
                    writer.write_u64(
                        msg.new_order_reference);

                    writer.write_string(
                        "shares");
                    writer.write_u64(
                        msg.new_shares);

                    writer.write_string(
                        "price");
                    writer.write_i64(
                        msg.new_price);
                }
                else
                {
                    writer.write_string(
                        "message_fields");

                    writer.write_string(
                        "not serialized individually");
                }
            },
            message);
    }

    void write_replay_operation(
        BufferWriter &writer,
        const std::variant<
            ReplayOperation,
            ReplayError> &result)
    {
        if (
            std::holds_alternative<ReplayError>(
                result))
        {
            const ReplayError error =
                std::get<ReplayError>(
                    result);

            if (
                error ==
                ReplayError::IgnoredMessage)
            {
                writer.write_u8(
                    static_cast<std::uint8_t>(
                        SerializedOperationType::Ignored));

                writer.write_string(
                    "IgnoredMessage");
            }
            else
            {
                writer.write_u8(
                    static_cast<std::uint8_t>(
                        SerializedOperationType::Error));

                writer.write_string(
                    "ReplayError");
            }

            return;
        }

        const ReplayOperation &operation =
            std::get<ReplayOperation>(
                result);

        std::visit(
            [&writer](const auto &op)
            {
                using T =
                    std::decay_t<decltype(op)>;

                if constexpr (
                    std::is_same_v<
                        T,
                        AddOperation>)
                {
                    writer.write_u8(
                        static_cast<std::uint8_t>(
                            SerializedOperationType::Add));

                    writer.write_string(
                        "AddOperation");

                    writer.write_string(
                        "order_id");
                    writer.write_u64(
                        op.order_id);

                    writer.write_string(
                        "side");
                    writer.write_string(
                        side_name(op.side));

                    writer.write_string(
                        "quantity");
                    writer.write_u64(
                        op.quantity);

                    writer.write_string(
                        "price");
                    writer.write_i64(
                        op.price);
                }
                else if constexpr (
                    std::is_same_v<
                        T,
                        ReduceOperation>)
                {
                    writer.write_u8(
                        static_cast<std::uint8_t>(
                            SerializedOperationType::Reduce));

                    writer.write_string(
                        "ReduceOperation");

                    writer.write_string(
                        "order_id");
                    writer.write_u64(
                        op.order_id);

                    writer.write_string(
                        "quantity");
                    writer.write_u64(
                        op.quantity);
                }
                else if constexpr (
                    std::is_same_v<
                        T,
                        RemoveOperation>)
                {
                    writer.write_u8(
                        static_cast<std::uint8_t>(
                            SerializedOperationType::Remove));

                    writer.write_string(
                        "RemoveOperation");

                    writer.write_string(
                        "order_id");
                    writer.write_u64(
                        op.order_id);
                }
                else if constexpr (
                    std::is_same_v<
                        T,
                        ReplaceOperation>)
                {
                    writer.write_u8(
                        static_cast<std::uint8_t>(
                            SerializedOperationType::Replace));

                    writer.write_string(
                        "ReplaceOperation");

                    writer.write_string(
                        "old_order_id");
                    writer.write_u64(
                        op.old_order_id);

                    writer.write_string(
                        "new_order_id");
                    writer.write_u64(
                        op.new_order_id);

                    writer.write_string(
                        "side");
                    writer.write_string(
                        side_name(op.side));

                    writer.write_string(
                        "quantity");
                    writer.write_u64(
                        op.quantity);

                    writer.write_string(
                        "price");
                    writer.write_i64(
                        op.price);
                }
            },
            operation);
    }

    void write_header(
        BufferWriter &writer,
        const DatasetTarget &dataset)
    {
        writer.write_u32(
            FILE_MAGIC);

        writer.write_u32(
            FILE_VERSION);

        writer.write_u8(
            static_cast<std::uint8_t>(
                dataset.kind));

        writer.write_u8(0);
        writer.write_u8(0);
        writer.write_u8(0);

        writer.write_u64(
            static_cast<std::uint64_t>(
                dataset.stock_locate));

        writer.write_u64(
            dataset.target_count);
    }

    std::vector<Byte> build_record(
        std::uint64_t record_number,
        const std::vector<Byte> &payload,
        const ItchMessage &message,
        const std::variant<
            ReplayOperation,
            ReplayError> &mapping)
    {
        BufferWriter writer;

        writer.write_u64(
            record_number);

        writer.write_u8(
            static_cast<std::uint8_t>(
                serialized_message_type(
                    message)));

        writer.write_string(
            message_class_name(
                message));

        writer.write_bytes(
            payload);

        write_message_fields(
            writer,
            message);

        write_replay_operation(
            writer,
            mapping);

        return writer.get_data();
    }

    StockLocate extract_stock_locate(
        const ItchMessage &message)
    {
        return std::visit(
            [](const auto &msg)
            {
                using T =
                    std::decay_t<decltype(msg)>;

                if constexpr (
                    requires {
                        msg.stock_locate;
                    })
                {
                    return static_cast<StockLocate>(
                        msg.stock_locate);
                }
                else
                {
                    return static_cast<StockLocate>(
                        0);
                }
            },
            message);
    }

    bool is_target_stock(
        StockLocate stock_locate,
        const std::vector<DatasetTarget *> &datasets)
    {
        for (
            const DatasetTarget *dataset :
            datasets)
        {
            if (
                dataset->stock_locate ==
                stock_locate)
            {
                return true;
            }
        }

        return false;
    }

    void print_progress(
        std::uint64_t messages_read,
        const std::map<
            StockLocate,
            std::uint64_t> &counts)
    {
        if (
            messages_read == 0 ||
            messages_read % 1000000 != 0)
        {
            return;
        }

        std::cout
            << "Messages read: "
            << messages_read
            << '\n';

        for (
            const auto &[stock, count] :
            counts)
        {
            std::cout
                << "  Stock "
                << stock
                << ": "
                << count
                << '\n';
        }

        std::cout << '\n';
    }

    bool open_dataset_files(
        std::vector<DatasetTarget> &datasets)
    {
        for (
            DatasetTarget &dataset :
            datasets)
        {
            dataset.file.open(
                dataset.filename,
                std::ios::binary |
                    std::ios::trunc);

            if (!dataset.file.is_open())
            {
                std::cerr
                    << "Could not create:\n"
                    << dataset.filename
                    << '\n';

                return false;
            }

            dataset.opened = true;

            BufferWriter header_writer;

            write_header(
                header_writer,
                dataset);

            const auto &header_data =
                header_writer.get_data();

            dataset.file.write(
                reinterpret_cast<const char *>(
                    header_data.data()),
                static_cast<std::streamsize>(
                    header_data.size()));

            if (!dataset.file)
            {
                std::cerr
                    << "Failed writing header:\n"
                    << dataset.filename
                    << '\n';

                return false;
            }
        }

        return true;
    }

    void close_dataset_files(
        std::vector<DatasetTarget> &datasets)
    {
        for (
            DatasetTarget &dataset :
            datasets)
        {
            if (dataset.opened)
            {
                dataset.file.flush();
                dataset.file.close();
                dataset.opened = false;
            }
        }
    }

    bool write_complete_record(
        DatasetTarget &dataset,
        const std::vector<Byte> &record)
    {
        if (record.empty())
        {
            return false;
        }

        dataset.file.write(
            reinterpret_cast<const char *>(
                record.data()),
            static_cast<std::streamsize>(
                record.size()));

        return static_cast<bool>(
            dataset.file);
    }

}

int main(
    int argc,
    char *argv[])
{
    if (argc != 2)
    {
        std::cerr
            << "Usage:\n"
            << "  "
            << argv[0]
            << " <itch_file>\n";

        return 1;
    }

    const std::string input_file =
        argv[1];

    std::cout
        << "===== ITCH SAMPLE GENERATOR =====\n";

    std::cout
        << "Input:\n"
        << input_file
        << "\n\n";

    std::error_code directory_error;

    std::filesystem::create_directories(
        SAMPLE_DIRECTORY,
        directory_error);

    if (directory_error)
    {
        std::cerr
            << "Could not create sample directory:\n"
            << SAMPLE_DIRECTORY
            << '\n'
            << "Error: "
            << directory_error.message()
            << '\n';

        return 1;
    }

    ItchFileReader first_pass(
        input_file);

    std::map<
        StockLocate,
        std::uint64_t>
        stock_counts;

    std::uint64_t messages_read = 0;

    while (true)
    {
        auto result =
            first_pass.next_message();

        if (
            std::holds_alternative<EndOfFile>(
                result))
        {
            break;
        }

        if (
            std::holds_alternative<FileReaderError>(
                result))
        {
            std::cerr
                << "File reader error during first pass.\n";

            return 1;
        }

        const auto &payload =
            std::get<std::vector<Byte>>(
                result);

        auto parsed =
            get_type_parser(
                payload);

        if (
            std::holds_alternative<ParseError>(
                parsed))
        {
            std::cerr
                << "Parse error during first pass at message "
                << (messages_read + 1)
                << ".\n";

            return 1;
        }

        const ItchMessage &message =
            std::get<ItchMessage>(
                parsed);

        const StockLocate stock =
            extract_stock_locate(
                message);

        if (stock != 0)
        {
            ++stock_counts[stock];
        }

        ++messages_read;

        print_progress(
            messages_read,
            stock_counts);
    }

    StockLocate stock_100 = 0;
    StockLocate stock_1000 = 0;
    StockLocate stock_10000 = 0;
    StockLocate stock_100000 = 0;

    for (
        const auto &[stock, count] :
        stock_counts)
    {
        if (
            stock == SPECIAL_STOCK_LOCATE)
        {
            continue;
        }

        if (
            stock_100 == 0 &&
            count >= 100)
        {
            stock_100 = stock;
            continue;
        }

        if (
            stock_1000 == 0 &&
            count >= 1000 &&
            stock != stock_100)
        {
            stock_1000 = stock;
            continue;
        }

        if (
            stock_10000 == 0 &&
            count >= 10000 &&
            stock != stock_100 &&
            stock != stock_1000)
        {
            stock_10000 = stock;
            continue;
        }

        if (
            stock_100000 == 0 &&
            count >= 100000 &&
            stock != stock_100 &&
            stock != stock_1000 &&
            stock != stock_10000)
        {
            stock_100000 = stock;
            break;
        }
    }

    if (stock_100 == 0)
    {
        std::cerr
            << "Could not find a stock with 100 messages.\n";

        return 1;
    }

    if (stock_1000 == 0)
    {
        std::cerr
            << "Could not find a stock with 1,000 messages.\n";

        return 1;
    }

    if (stock_10000 == 0)
    {
        std::cerr
            << "Could not find a stock with 10,000 messages.\n";

        return 1;
    }

    if (stock_100000 == 0)
    {
        std::cerr
            << "Could not find a stock with 100,000 messages.\n";

        return 1;
    }

    std::cout
        << "===== SELECTED STOCKS =====\n";

    std::cout
        << "100 messages:     "
        << stock_100
        << '\n';

    std::cout
        << "1,000 messages:   "
        << stock_1000
        << '\n';

    std::cout
        << "10,000 messages:  "
        << stock_10000
        << '\n';

    std::cout
        << "100,000 messages: "
        << stock_100000
        << '\n';

    std::cout
        << "All Stock 123:    "
        << SPECIAL_STOCK_LOCATE
        << "\n\n";

    std::vector<DatasetTarget> datasets;

    datasets.push_back(
        DatasetTarget{
            DatasetKind::Stock100,
            SAMPLE_DIRECTORY + "stock_100.bin",
            stock_100,
            100,
            0,
            std::ofstream{},
            false});

    datasets.push_back(
        DatasetTarget{
            DatasetKind::Stock1000,
            SAMPLE_DIRECTORY + "stock_1000.bin",
            stock_1000,
            1000,
            0,
            std::ofstream{},
            false});

    datasets.push_back(
        DatasetTarget{
            DatasetKind::Stock10000,
            SAMPLE_DIRECTORY + "stock_10000.bin",
            stock_10000,
            10000,
            0,
            std::ofstream{},
            false});

    datasets.push_back(
        DatasetTarget{
            DatasetKind::Stock100000,
            SAMPLE_DIRECTORY + "stock_100000.bin",
            stock_100000,
            100000,
            0,
            std::ofstream{},
            false});

    datasets.push_back(
        DatasetTarget{
            DatasetKind::Stock123All,
            SAMPLE_DIRECTORY + "stock_123_all.bin",
            SPECIAL_STOCK_LOCATE,
            0,
            0,
            std::ofstream{},
            false});

    if (!open_dataset_files(
            datasets))
    {
        close_dataset_files(
            datasets);

        return 1;
    }

    ItchMapper mapper_100(
        stock_100);

    ItchMapper mapper_1000(
        stock_1000);

    ItchMapper mapper_10000(
        stock_10000);

    ItchMapper mapper_100000(
        stock_100000);

    ItchMapper mapper_123(
        SPECIAL_STOCK_LOCATE);

    ItchFileReader second_pass(
        input_file);

    std::uint64_t global_message_number = 0;

    while (true)
    {
        auto result =
            second_pass.next_message();

        if (
            std::holds_alternative<EndOfFile>(
                result))
        {
            break;
        }

        if (
            std::holds_alternative<FileReaderError>(
                result))
        {
            std::cerr
                << "File reader error during second pass.\n";

            close_dataset_files(
                datasets);

            return 1;
        }

        const auto &payload =
            std::get<std::vector<Byte>>(
                result);

        ++global_message_number;

        auto parsed =
            get_type_parser(
                payload);

        if (
            std::holds_alternative<ParseError>(
                parsed))
        {
            std::cerr
                << "Parse error at message "
                << global_message_number
                << ".\n";

            close_dataset_files(
                datasets);

            return 1;
        }

        const ItchMessage &message =
            std::get<ItchMessage>(
                parsed);

        const StockLocate stock =
            extract_stock_locate(
                message);

        if (
            !is_target_stock(
                stock,
                {&datasets[0],
                 &datasets[1],
                 &datasets[2],
                 &datasets[3],
                 &datasets[4]}))
        {
            continue;
        }

        std::variant<
            ReplayOperation,
            ReplayError>
            mapped;

        if (stock == stock_100)
        {
            mapped =
                mapper_100.map(
                    message);
        }
        else if (stock == stock_1000)
        {
            mapped =
                mapper_1000.map(
                    message);
        }
        else if (stock == stock_10000)
        {
            mapped =
                mapper_10000.map(
                    message);
        }
        else if (stock == stock_100000)
        {
            mapped =
                mapper_100000.map(
                    message);
        }
        else
        {
            mapped =
                mapper_123.map(
                    message);
        }

        const std::vector<Byte> record =
            build_record(
                0,
                payload,
                message,
                mapped);

        if (record.size() < 9)
        {
            std::cerr
                << "Generated record is invalid at message "
                << global_message_number
                << ".\n";

            close_dataset_files(
                datasets);

            return 1;
        }

        for (
            DatasetTarget &dataset :
            datasets)
        {
            if (
                dataset.stock_locate !=
                stock)
            {
                continue;
            }

            if (
                dataset.target_count != 0 &&
                dataset.current_count >=
                    dataset.target_count)
            {
                continue;
            }

            ++dataset.current_count;

            const std::vector<Byte> final_record =
                build_record(
                    dataset.current_count,
                    payload,
                    message,
                    mapped);

            if (!write_complete_record(
                    dataset,
                    final_record))
            {
                std::cerr
                    << "Write failure for dataset:\n"
                    << dataset.filename
                    << "\n"
                    << "Record: "
                    << dataset.current_count
                    << "\n"
                    << "Global message: "
                    << global_message_number
                    << "\n"
                    << "Message type: "
                    << static_cast<unsigned>(
                           static_cast<std::uint8_t>(
                               serialized_message_type(
                                   message)))
                    << "\n"
                    << "Record bytes: "
                    << final_record.size()
                    << "\n";

                close_dataset_files(
                    datasets);

                return 1;
            }
        }
    }

    close_dataset_files(
        datasets);

    std::cout
        << "===== SAMPLE GENERATION COMPLETE =====\n";

    for (
        const DatasetTarget &dataset :
        datasets)
    {
        std::cout
            << std::left
            << std::setw(52)
            << dataset.filename
            << "Stock Locate: "
            << std::setw(6)
            << dataset.stock_locate
            << "Records: "
            << dataset.current_count
            << '\n';
    }

    std::cout
        << "\nDone.\n";

    return 0;
}