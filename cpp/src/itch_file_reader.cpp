#include <itch_file_reader.hpp>

#include <iostream>
#include <profile.hpp>

ItchFileReader::ItchFileReader(const std::string &file_path)
{
    LOB_PROFILE_SCOPE("ItchFileReader::ItchFileReader");

    this->file.open(file_path, std::ios::binary);

    if (!file.is_open())
    {
        open_failed = true;
    }
}

std::variant<std::vector<Byte>, FileReaderError, EndOfFile> ItchFileReader::next_message()
{
    LOB_PROFILE_SCOPE("ItchFileReader::next_message");

    if (open_failed)
    {
        return FileReaderError::CanNotOpenFile;
    }

    if (finished)
    {
        return EndOfFile{};
    }

    {
        LOB_PROFILE_SCOPE("ItchFileReader::next_message::peek");

        int next_byte = file.peek();

        if (next_byte == std::ifstream::traits_type::eof())
        {
            if (file.bad())
            {
                return FileReaderError::ReadFailure;
            }

            finished = true;
            return EndOfFile{};
        }
    }

    Byte length_bytes[2];

    {
        LOB_PROFILE_SCOPE("ItchFileReader::next_message::length_read");

        file.read(reinterpret_cast<char *>(length_bytes), 2);
    }

    std::streamsize bytes_read = file.gcount();

    if (bytes_read == 0)
    {
        if (file.eof())
        {
            finished = true;
            return EndOfFile{};
        }

        return FileReaderError::ReadFailure;
    }

    if (bytes_read == 1)
    {
        return FileReaderError::IncompleteLengthField;
    }

    std::uint16_t len =
        (static_cast<std::uint16_t>(length_bytes[0]) << 8) |
        static_cast<std::uint16_t>(length_bytes[1]);

    if (len == 0)
    {
        finished = true;
        return EndOfFile{};
    }

    std::vector<Byte> payload(len);

    {
        LOB_PROFILE_SCOPE("ItchFileReader::next_message::payload_read");

        file.read(reinterpret_cast<char *>(payload.data()), len);
    }

    bytes_read = file.gcount();

    if (bytes_read != static_cast<std::streamsize>(len))
    {
        finished = true;
        return FileReaderError::TruncatedPayload;
    }

    return payload;
}
