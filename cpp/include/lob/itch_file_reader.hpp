#pragma once

#include <types.hpp>

#include <fstream>
#include <string>
#include <variant>
#include <vector>

enum class FileReaderError {
    CanNotOpenFile,
    IncompleteLengthField,
    TruncatedPayload,
    ReadFailure
};

struct EndOfFile {};

class ItchFileReader {
    private:
        std::ifstream file;
        bool finished = false;
        bool open_failed = false;

    public:
        explicit ItchFileReader(const std::string& file_path);

        std::variant<std::vector<Byte>, FileReaderError, EndOfFile> next_message();
};