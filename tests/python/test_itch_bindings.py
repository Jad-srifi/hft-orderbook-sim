import sys
from pathlib import Path

import pytest

PROJECT_ROOT = Path(__file__).resolve().parents[2]
PYTHON_DIR = PROJECT_ROOT / "python"

sys.path.insert(0, str(PYTHON_DIR))

import lob_cpp


ITCH_FILE = (
    PROJECT_ROOT
    / "data"
    / "itch"
    / "2019-10-18"
    / "raw"
    / "decompressed"
    / "S101819-v50.txt"
)

pytestmark = pytest.mark.skipif(
    not ITCH_FILE.is_file(),
    reason="Optional Nasdaq ITCH dataset is not available in this checkout.",
)


def get_first_message():
    reader = lob_cpp.ItchFileReader(str(ITCH_FILE))

    while True:
        result = reader.next_message()

        if isinstance(result, lob_cpp.EndOfFile):
            raise RuntimeError("Reached end of ITCH file without finding a message.")

        if isinstance(result, lob_cpp.FileReaderError):
            raise RuntimeError(f"ITCH file reader error: {result}")

        return result


def test_file_reader():
    assert ITCH_FILE.exists(), f"ITCH file not found: {ITCH_FILE}"

    reader = lob_cpp.ItchFileReader(str(ITCH_FILE))
    result = reader.next_message()

    assert not isinstance(result, lob_cpp.EndOfFile)
    assert not isinstance(result, lob_cpp.FileReaderError)
    assert isinstance(result, list)
    assert len(result) > 0


def test_parser():
    message_bytes = get_first_message()

    parsed = lob_cpp.get_type_parser(message_bytes)

    assert not isinstance(parsed, lob_cpp.ParseError)


def test_parse_add_order_message():
    reader = lob_cpp.ItchFileReader(str(ITCH_FILE))

    add_message = None

    while True:
        result = reader.next_message()

        if isinstance(result, lob_cpp.EndOfFile):
            break

        if isinstance(result, lob_cpp.FileReaderError):
            raise RuntimeError(f"ITCH file reader error: {result}")

        parsed = lob_cpp.get_type_parser(result)

        if isinstance(parsed, lob_cpp.ParseError):
            continue

        if isinstance(parsed, lob_cpp.AddOrderMessage):
            add_message = parsed
            break

        if isinstance(parsed, lob_cpp.AddOrderMPIDMessage):
            add_message = parsed
            break

    assert add_message is not None

    assert add_message.order_reference > 0
    assert add_message.shares > 0
    assert add_message.price >= 0


def test_mapper():
    reader = lob_cpp.ItchFileReader(str(ITCH_FILE))

    selected_message = None
    selected_stock = None

    while True:
        result = reader.next_message()

        if isinstance(result, lob_cpp.EndOfFile):
            break

        if isinstance(result, lob_cpp.FileReaderError):
            raise RuntimeError(f"ITCH file reader error: {result}")

        parsed = lob_cpp.get_type_parser(result)

        if isinstance(parsed, lob_cpp.ParseError):
            continue

        if isinstance(parsed, lob_cpp.AddOrderMessage):
            selected_message = parsed
            selected_stock = parsed.stock_locate
            break

        if isinstance(parsed, lob_cpp.AddOrderMPIDMessage):
            selected_message = parsed
            selected_stock = parsed.stock_locate
            break

    assert selected_message is not None
    assert selected_stock is not None

    mapper = lob_cpp.ItchMapper(selected_stock)

    assert mapper.is_selected_security(selected_stock)

    mapped = mapper.map(selected_message)

    assert mapped is not None


def test_replay():
    reader = lob_cpp.ItchFileReader(str(ITCH_FILE))

    add_message = None

    while True:
        result = reader.next_message()

        if isinstance(result, lob_cpp.EndOfFile):
            break

        if isinstance(result, lob_cpp.FileReaderError):
            raise RuntimeError(f"ITCH file reader error: {result}")

        parsed = lob_cpp.get_type_parser(result)

        if isinstance(parsed, lob_cpp.ParseError):
            continue

        if isinstance(parsed, lob_cpp.AddOrderMessage):
            add_message = parsed
            break

        if isinstance(parsed, lob_cpp.AddOrderMPIDMessage):
            add_message = parsed
            break

    assert add_message is not None

    mapper = lob_cpp.ItchMapper(add_message.stock_locate)
    operation = mapper.map(add_message)

    assert isinstance(operation, lob_cpp.AddOperation)

    book = lob_cpp.OrderBook()
    replay = lob_cpp.ItchReplay(book)

    replay_result = replay.apply(operation)

    assert replay_result is None

    if add_message.side == lob_cpp.Side.BUY:
        assert book.best_bid() == add_message.price
    elif add_message.side == lob_cpp.Side.SELL:
        assert book.best_ask() == add_message.price
    else:
        raise AssertionError("Add order has Side.NONE.")


def main():
    if not ITCH_FILE.is_file():
        print(
            "Skipping ITCH binding tests: optional Nasdaq ITCH dataset is not "
            f"available at {ITCH_FILE}."
        )
        return

    test_file_reader()
    test_parser()
    test_parse_add_order_message()
    test_mapper()
    test_replay()

    print("All ITCH binding tests passed.")


if __name__ == "__main__":
    main()
