from __future__ import annotations

import csv
import inspect
import sys
from pathlib import Path
from typing import Any, Iterable

ROOT = Path(__file__).resolve().parents[1]
PYTHON_DIR = ROOT / "python"
if str(PYTHON_DIR) not in sys.path:
    sys.path.insert(0, str(PYTHON_DIR))

OBSERVATION_FIELDS = [
    "timestamp_ms",
    "message_index",
    "best_bid",
    "best_ask",
    "midpoint",
    "spread",
    "relative_spread",
    "bid_depth",
    "ask_depth",
    "imbalance",
]


def resolve_itch_file(value: str | Path | None = None) -> Path:
    if value is not None:
        path = Path(value).expanduser()
        if not path.is_absolute():
            path = ROOT / path
        if not path.is_file():
            raise FileNotFoundError(f"ITCH file not found: {path}")
        return path

    candidates = [
        ROOT / "data/itch/2019-10-18/raw/decompressed/S101819-v50.txt",
        ROOT / "data/itch/2019-10-18/decompressed/S101819-v50.txt",
        ROOT / "data/itch/2019-10-18/raw/S101819-v50.txt.gz",
        ROOT / "data/itch/2019-10-18/S101819-v50.txt",
    ]
    for candidate in candidates:
        if candidate.is_file():
            return candidate
    searched = "\n".join(str(path) for path in candidates)
    raise FileNotFoundError(
        "No default ITCH file was found. Pass --file PATH. Searched:\n" + searched
    )


def _accepted_kwargs(callable_obj: Any, values: dict[str, Any]) -> dict[str, Any]:
    try:
        parameters = inspect.signature(callable_obj).parameters
    except (TypeError, ValueError):
        return {}
    if any(p.kind is inspect.Parameter.VAR_KEYWORD for p in parameters.values()):
        return values
    return {key: value for key, value in values.items() if key in parameters}


def _construct_replay(file_path: Path, stock_locate: int, max_messages: int | None, snapshot_interval: int) -> Any:
    try:
        from backtests.historical_replay import HistoricalReplay
    except ImportError as exc:
        raise ImportError(
            "Could not import HistoricalReplay from python/backtests/historical_replay.py. "
            "Run from the repository or add the python directory to PYTHONPATH."
        ) from exc

    options = {
        "stock_locate": stock_locate,
        "max_messages": max_messages,
        "snapshot_interval": snapshot_interval,
    }
    try:
        parameters = inspect.signature(HistoricalReplay).parameters
    except (TypeError, ValueError):
        parameters = {}

    path_parameter = next(
        (name for name in ("file_path", "path", "itch_path", "filename") if name in parameters),
        None,
    )
    kwargs = _accepted_kwargs(HistoricalReplay, options)
    if path_parameter:
        kwargs[path_parameter] = str(file_path)
        return HistoricalReplay(**kwargs)

    try:
        return HistoricalReplay(str(file_path), **kwargs)
    except TypeError as first_error:
        try:
            return HistoricalReplay(str(file_path), stock_locate)
        except TypeError:
            raise first_error


def _extract_snapshots(result: Any, runner: Any) -> list[Any]:
    candidates: list[Any] = [result, runner]
    for candidate in candidates:
        if candidate is None:
            continue
        if isinstance(candidate, dict):
            for key in ("snapshots", "observations", "book_snapshots", "records"):
                value = candidate.get(key)
                if value is not None:
                    return list(value)
        for name in ("snapshots", "observations", "book_snapshots", "records"):
            value = getattr(candidate, name, None)
            if value is not None and not callable(value):
                try:
                    return list(value)
                except TypeError:
                    pass
        if isinstance(candidate, (list, tuple)):
            for item in candidate:
                if isinstance(item, (list, tuple)):
                    try:
                        return list(item)
                    except TypeError:
                        pass
            return list(candidate)
    return []


def run_historical_replay(
    file_path: str | Path | None,
    stock_locate: int = 123,
    max_messages: int | None = 20_000_000,
    snapshot_interval: int = 10,
) -> tuple[Any, Any, list[Any], Path]:
    path = resolve_itch_file(file_path)
    runner = _construct_replay(path, stock_locate, max_messages, snapshot_interval)
    method = next(
        (
            getattr(runner, name)
            for name in ("run", "replay", "execute", "run_replay")
            if callable(getattr(runner, name, None))
        ),
        None,
    )
    if method is None:
        raise AttributeError(
            "HistoricalReplay exposes no run/replay/execute method. "
            "Check python/backtests/historical_replay.py and update this example adapter."
        )

    options = {
        "max_messages": max_messages,
        "snapshot_interval": snapshot_interval,
        "stock_locate": stock_locate,
    }
    result = method(**_accepted_kwargs(method, options))
    snapshots = _extract_snapshots(result, runner)
    return runner, result, snapshots, path


def _value(obj: Any, *names: str) -> Any:
    for name in names:
        if isinstance(obj, dict) and name in obj:
            return obj[name]
        value = getattr(obj, name, None)
        if value is not None and not callable(value):
            return value
    return None


def _level_quantity(levels: Any) -> float | None:
    if levels is None:
        return None
    total = 0.0
    found = False
    try:
        iterator: Iterable[Any] = iter(levels)
    except TypeError:
        return None
    for level in iterator:
        quantity = _value(level, "total_quantity", "quantity", "total_size")
        if quantity is None:
            orders = _value(level, "orders")
            if orders is not None:
                quantity = sum(float(_value(order, "quantity", "shares") or 0) for order in orders)
        if quantity is not None:
            total += float(quantity)
            found = True
    return total if found else None


def snapshot_to_row(snapshot: Any, index: int) -> dict[str, Any]:
    best_bid = _value(snapshot, "best_bid", "bid_price", "bid")
    best_ask = _value(snapshot, "best_ask", "ask_price", "ask")
    midpoint = _value(snapshot, "midpoint", "mid_price", "mid")
    spread = _value(snapshot, "spread")
    bid_depth = _value(snapshot, "bid_depth", "total_bid_quantity", "bid_depth_quantity")
    ask_depth = _value(snapshot, "ask_depth", "total_ask_quantity", "ask_depth_quantity")

    if bid_depth is None:
        bid_depth = _level_quantity(_value(snapshot, "bid_levels", "bids"))
    if ask_depth is None:
        ask_depth = _level_quantity(_value(snapshot, "ask_levels", "asks"))
    if midpoint is None and best_bid is not None and best_ask is not None:
        midpoint = (float(best_bid) + float(best_ask)) / 2.0
    if spread is None and best_bid is not None and best_ask is not None:
        spread = float(best_ask) - float(best_bid)
    relative_spread = None
    if spread is not None and midpoint not in (None, 0, 0.0):
        relative_spread = float(spread) / float(midpoint)
    imbalance = None
    if bid_depth is not None and ask_depth is not None:
        denominator = float(bid_depth) + float(ask_depth)
        if denominator > 0:
            imbalance = (float(bid_depth) - float(ask_depth)) / denominator

    row = {
        "timestamp_ms": _value(snapshot, "timestamp_ms", "time_ms", "timestamp"),
        "message_index": _value(snapshot, "message_index", "message_number", "index") or index,
        "best_bid": best_bid,
        "best_ask": best_ask,
        "midpoint": midpoint,
        "spread": spread,
        "relative_spread": relative_spread,
        "bid_depth": bid_depth,
        "ask_depth": ask_depth,
        "imbalance": _value(snapshot, "imbalance") if _value(snapshot, "imbalance") is not None else imbalance,
    }
    return {key: "" if value is None else value for key, value in row.items()}


def write_observations_csv(snapshots: list[Any], destination: str | Path) -> Path:
    path = Path(destination)
    path.parent.mkdir(parents=True, exist_ok=True)
    with path.open("w", newline="", encoding="utf-8") as file:
        writer = csv.DictWriter(file, fieldnames=OBSERVATION_FIELDS)
        writer.writeheader()
        for index, snapshot in enumerate(snapshots):
            writer.writerow(snapshot_to_row(snapshot, index))
    return path


def read_csv_rows(path: str | Path) -> tuple[list[str], list[dict[str, str]]]:
    input_path = Path(path)
    if not input_path.is_file():
        raise FileNotFoundError(input_path)
    with input_path.open("r", newline="", encoding="utf-8-sig") as file:
        reader = csv.DictReader(file)
        if not reader.fieldnames:
            raise ValueError(f"CSV has no header: {input_path}")
        return list(reader.fieldnames), list(reader)


def as_float(row: dict[str, Any], key: str) -> float | None:
    value = row.get(key)
    if value in (None, ""):
        return None
    try:
        number = float(value)
    except (TypeError, ValueError):
        return None
    return number if number == number and abs(number) != float("inf") else None
