#include <itch_file_reader.hpp>

#include <cassert>
#include <cstdint>
#include <fstream>
#include <iostream>
#include <string>
#include <variant>
#include <vector>

void write_binary_file(
    const std::string& path,
    const std::vector<Byte>& data
)
{
    std::ofstream file(path, std::ios::binary);

    assert(file.is_open());

    file.write(
        reinterpret_cast<const char*>(data.data()),
        static_cast<std::streamsize>(data.size())
    );

    assert(file.good());
}

void create_test_file(
    const std::string& path,
    const std::vector<std::vector<Byte>>& messages,
    bool add_end_marker = false
)
{
    std::ofstream file(path, std::ios::binary);

    assert(file.is_open());

    for (const auto& message : messages) {

        std::uint16_t length =
            static_cast<std::uint16_t>(message.size());

        Byte length_bytes[2] = {
            static_cast<Byte>((length >> 8) & 0xFF),
            static_cast<Byte>(length & 0xFF)
        };

        file.write(
            reinterpret_cast<const char*>(length_bytes),
            2
        );

        file.write(
            reinterpret_cast<const char*>(message.data()),
            static_cast<std::streamsize>(message.size())
        );
    }

    if (add_end_marker) {

        Byte end_marker[2] = {
            0x00,
            0x00
        };

        file.write(
            reinterpret_cast<const char*>(end_marker),
            2
        );
    }

    file.close();

    assert(file.good() || file.eof());
}

void test_one_message()
{
    const std::string path =
        "test_one_message.bin";

    std::vector<Byte> message = {
        0x41,
        0x01
    };

    create_test_file(
        path,
        {message}
    );

    ItchFileReader reader(path);

    auto result = reader.next_message();

    assert(
        std::holds_alternative<std::vector<Byte>>(result)
    );

    const auto& payload =
        std::get<std::vector<Byte>>(result);

    assert(payload == message);
}

void test_multiple_messages()
{
    const std::string path =
        "test_multiple_messages.bin";

    std::vector<Byte> message1 = {
        0x41,
        0x01
    };

    std::vector<Byte> message2 = {
        0x58,
        0x02,
        0x03
    };

    create_test_file(
        path,
        {message1, message2}
    );

    ItchFileReader reader(path);

    auto result1 = reader.next_message();

    assert(
        std::holds_alternative<std::vector<Byte>>(result1)
    );

    assert(
        std::get<std::vector<Byte>>(result1) == message1
    );

    auto result2 = reader.next_message();

    assert(
        std::holds_alternative<std::vector<Byte>>(result2)
    );

    assert(
        std::get<std::vector<Byte>>(result2) == message2
    );

    auto result3 = reader.next_message();

    assert(
        std::holds_alternative<EndOfFile>(result3)
    );
}

void test_clean_eof()
{
    const std::string path =
        "test_clean_eof.bin";

    std::vector<Byte> message = {
        0x41,
        0x01
    };

    create_test_file(
        path,
        {message}
    );

    ItchFileReader reader(path);

    auto result1 = reader.next_message();

    assert(
        std::holds_alternative<std::vector<Byte>>(result1)
    );

    auto result2 = reader.next_message();

    assert(
        std::holds_alternative<EndOfFile>(result2)
    );
}

void test_zero_length_end_marker()
{
    const std::string path =
        "test_zero_length.bin";

    std::vector<Byte> message = {
        0x41,
        0x01
    };

    create_test_file(
        path,
        {message},
        true
    );

    ItchFileReader reader(path);

    auto result1 = reader.next_message();

    assert(
        std::holds_alternative<std::vector<Byte>>(result1)
    );

    auto result2 = reader.next_message();

    assert(
        std::holds_alternative<EndOfFile>(result2)
    );
}

void test_empty_file()
{
    const std::string path =
        "test_empty.bin";

    create_test_file(
        path,
        {}
    );

    ItchFileReader reader(path);

    auto result = reader.next_message();

    assert(
        std::holds_alternative<EndOfFile>(result)
    );
}

void test_incomplete_length()
{
    const std::string path =
        "test_incomplete_length.bin";

    std::vector<Byte> data = {
        0x00
    };

    write_binary_file(
        path,
        data
    );

    ItchFileReader reader(path);

    auto result = reader.next_message();

    assert(
        std::holds_alternative<FileReaderError>(result)
    );

    assert(
        std::get<FileReaderError>(result)
        == FileReaderError::IncompleteLengthField
    );
}

void test_truncated_payload()
{
    const std::string path =
        "test_truncated_payload.bin";

    std::vector<Byte> data = {
        0x00,
        0x04,
        0x41,
        0x42
    };

    write_binary_file(
        path,
        data
    );

    ItchFileReader reader(path);

    auto result = reader.next_message();

    assert(
        std::holds_alternative<FileReaderError>(result)
    );

    assert(
        std::get<FileReaderError>(result)
        == FileReaderError::TruncatedPayload
    );
}

void test_file_cannot_open()
{
    const std::string path =
        "file_that_does_not_exist.bin";

    ItchFileReader reader(path);

    auto result = reader.next_message();

    assert(
        std::holds_alternative<FileReaderError>(result)
    );

    assert(
        std::get<FileReaderError>(result)
        == FileReaderError::CanNotOpenFile
    );
}

void test_finished_state()
{
    const std::string path =
        "test_finished_state.bin";

    create_test_file(
        path,
        {}
    );

    ItchFileReader reader(path);

    auto result1 = reader.next_message();

    assert(
        std::holds_alternative<EndOfFile>(result1)
    );

    auto result2 = reader.next_message();

    assert(
        std::holds_alternative<EndOfFile>(result2)
    );
}

void test_payload_unchanged()
{
    const std::string path =
        "test_payload_unchanged.bin";

    std::vector<Byte> message = {
        0x41,
        0x00,
        0xFF,
        0x12,
        0x34
    };

    create_test_file(
        path,
        {message}
    );

    ItchFileReader reader(path);

    auto result = reader.next_message();

    assert(
        std::holds_alternative<std::vector<Byte>>(result)
    );

    assert(
        std::get<std::vector<Byte>>(result) == message
    );
}

int main()
{
    test_one_message();
    test_multiple_messages();
    test_clean_eof();
    test_zero_length_end_marker();
    test_empty_file();
    test_incomplete_length();
    test_truncated_payload();
    test_file_cannot_open();
    test_finished_state();
    test_payload_unchanged();

    std::cout
        << "All ITCH file reader tests passed.\n";

    return 0;
}