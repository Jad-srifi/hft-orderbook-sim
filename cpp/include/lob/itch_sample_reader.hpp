#pragma once

#include <cstdint>
#include <fstream>
#include <string>
#include <variant>
#include <vector>

enum class SampleReaderError : std::uint8_t
{
    CanNotOpenFile,
    InvalidMagic,
    UnsupportedVersion,
    InvalidDatasetKind,
    UnexpectedEOF,
    InvalidMessageType,
    InvalidOperationType,
    InvalidFieldLength,
    ReadFailure
};

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
    OrderExecuted = 3,
    OrderExecutedWithPrice = 4,
    OrderCancel = 5,
    OrderDelete = 6,
    OrderReplace = 7,
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

struct SampleHeader
{
    std::uint32_t magic;
    std::uint32_t version;
    DatasetKind dataset_kind;
    std::uint64_t stock_locate;
    std::uint64_t target_count;
};

struct SampleRecord
{
    std::uint64_t record_number;
    SerializedMessageType message_type;
    std::string message_class_name;
    std::vector<std::uint8_t> raw_payload;
    std::string decoded_fields;
    SerializedOperationType operation_type;
    std::string mapping_result;
};

class ItchSampleReader
{
private:
    std::ifstream file;
    bool finished = false;
    bool open_failed = false;
    bool debug_mode = false;

public:
    explicit ItchSampleReader(
        const std::string &file_path,
        bool debug_mode = false);

    std::variant<
        SampleHeader,
        SampleReaderError>
    read_header();

    std::variant<
        SampleRecord,
        SampleReaderError>
    next_record();
};