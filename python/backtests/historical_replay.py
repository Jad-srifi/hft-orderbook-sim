import lob_cpp

from datasets import BookDataset
from observations import BookSnapshot


class HistoricalReplay:
    def __init__(self, file_path: str, stock_locate: int) -> None:
        self.file_path = file_path
        self.stock_locate = stock_locate
        self.book = lob_cpp.OrderBook()
        self.itch_reader = lob_cpp.ItchFileReader(str(file_path))
        self.itch_mapper = lob_cpp.ItchMapper(stock_locate)
        self.itch_replay = lob_cpp.ItchReplay(self.book)
        self.messages_seen = 0
        self.replay_operations = 0
        self.ignored_messages = 0
        self.last_timestamp_ms = 0

    def _timestamp_ms(self, message) -> int:
        timestamp = getattr(message, "timestamp", None)

        if timestamp is None:
            raise RuntimeError("ITCH message does not expose a timestamp")

        return int(timestamp) // 1_000_000

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

            self.messages_seen += 1
            self.last_timestamp_ms = self._timestamp_ms(parsed)

            operation = self.itch_mapper.map(parsed)

            if isinstance(operation, lob_cpp.ReplayError):
                if operation == lob_cpp.ReplayError.IgnoredMessage:
                    self.ignored_messages += 1
                    continue

                raise RuntimeError(f"ITCH replay mapping error: {operation}")

            replay_result = self.itch_replay.apply(operation)

            if replay_result is not None:
                raise RuntimeError(f"ITCH replay error: {replay_result}")

            self.replay_operations += 1

        return self.book

    def snapshot(self) -> BookSnapshot:
        return BookSnapshot.from_book(self.book, self.last_timestamp_ms, self.messages_seen)

    def run_and_snapshot(self) -> BookSnapshot:
        self.run()
        return self.snapshot()

    def run_and_collect(self, snapshot_interval: int, max_messages: int | None = None) -> BookDataset:
        if snapshot_interval < 1:
            raise ValueError("snapshot_interval must be at least 1")
        if max_messages is not None and max_messages < 1:
            raise ValueError("max_messages must be at least 1 or None")

        snapshots = []
        processed_messages = 0
        operations_since_snapshot = 0
        last_applied_timestamp_ms = None
        last_applied_message_index = None
        last_captured_message_index = None

        while max_messages is None or processed_messages < max_messages:
            result = self.itch_reader.next_message()

            if isinstance(result, lob_cpp.EndOfFile):
                break

            if isinstance(result, lob_cpp.FileReaderError):
                raise RuntimeError(f"ITCH file reader error: {result}")

            parsed = lob_cpp.get_type_parser(result)

            if isinstance(parsed, lob_cpp.ParseError):
                raise RuntimeError(f"ITCH parse error: {parsed}")

            processed_messages += 1
            self.messages_seen += 1

            timestamp_ms = self._timestamp_ms(parsed)
            self.last_timestamp_ms = timestamp_ms

            operation = self.itch_mapper.map(parsed)

            if isinstance(operation, lob_cpp.ReplayError):
                if operation == lob_cpp.ReplayError.IgnoredMessage:
                    self.ignored_messages += 1
                    continue
                raise RuntimeError(f"ITCH replay mapping error: {operation}")

            replay_result = self.itch_replay.apply(operation)

            if replay_result is not None:
                raise RuntimeError(f"ITCH replay error: {replay_result}")

            self.replay_operations += 1
            operations_since_snapshot += 1
            last_applied_timestamp_ms = timestamp_ms
            last_applied_message_index = self.messages_seen

            if operations_since_snapshot >= snapshot_interval:
                snapshots.append(BookSnapshot.from_book(self.book, timestamp_ms, self.messages_seen))
                operations_since_snapshot = 0
                last_captured_message_index = self.messages_seen

        if last_applied_message_index is not None and last_captured_message_index != last_applied_message_index:
            snapshots.append(BookSnapshot.from_book(self.book, last_applied_timestamp_ms, last_applied_message_index))

        return BookDataset.from_snapshots(snapshots)