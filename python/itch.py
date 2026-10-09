import lob_cpp


class HistoricalReplay:
    def __init__(self, file_path: str, stock_locate: int, book: lob_cpp.OrderBook) -> None:
        self.file_path = file_path
        self.stock_locate = stock_locate
        self.book = book

        self.itch_reader = lob_cpp.ItchFileReader(file_path)
        self.itch_mapper = lob_cpp.ItchMapper(stock_locate)
        self.itch_replay = lob_cpp.ItchReplay(book)

    def run(self) -> lob_cpp.OrderBook:
        while True:
            result = self.itch_reader.next_message()

            if isinstance(result, lob_cpp.EndOfFile):
                break

            if isinstance(result, lob_cpp.FileReaderError):
                raise RuntimeError(f"ITCH file reader error: {result}")

            parsed = lob_cpp.get_type_parser(result)

            if isinstance(parsed, lob_cpp.ParseError):
                raise RuntimeError(f"ITCH parse error: {parsed}")

            operation = self.itch_mapper.map(parsed)

            if isinstance(operation, lob_cpp.ReplayError):
                if operation == lob_cpp.ReplayError.IgnoredMessage:
                    continue

                raise RuntimeError(f"ITCH replay mapping error: {operation}")

            replay_result = self.itch_replay.apply(operation)

            if replay_result is not None:
                raise RuntimeError(f"ITCH replay error: {replay_result}")

        return self.book