from __future__ import annotations

import csv
import json
import math
from pathlib import Path
from typing import Any, Iterable


def _get(obj: Any, *names: str) -> Any:
    for name in names:
        if isinstance(obj, dict) and name in obj:
            return obj[name]
        try:
            value = getattr(obj, name, None)
        except Exception:
            value = None
        if value is not None and not callable(value):
            return value
    return None


def _number(value: Any) -> float | None:
    try:
        number = float(value)
    except (TypeError, ValueError, OverflowError):
        return None
    return number if math.isfinite(number) else None


def _get_or_call(obj: Any, *names: str) -> Any:
    """Read a value or call a known zero-argument C++ getter exposed by pybind11."""
    for name in names:
        if isinstance(obj, dict) and name in obj:
            value = obj[name]
        else:
            try:
                value = getattr(obj, name, None)
            except Exception:
                continue
        if callable(value):
            try:
                value = value()
            except TypeError:
                continue
            except Exception:
                continue
        if value is not None:
            return value
    return None


def _json_value(value: Any) -> Any:
    if value is None or isinstance(value, (str, int, bool)):
        return value
    if isinstance(value, float):
        return value if math.isfinite(value) else None
    return str(value)


def _level_quantity(level: Any) -> float | None:
    quantity = _number(_get_or_call(level, "total_quantity", "quantity", "total_size", "size"))
    if quantity is not None:
        return quantity
    orders = _get_or_call(level, "orders")
    if orders is None:
        return None
    try:
        values = [_number(_get_or_call(order, "quantity", "shares", "size")) or 0.0 for order in orders]
    except TypeError:
        return None
    return float(sum(values))


def _find_levels(snapshot: Any, side: str) -> tuple[Any, bool]:
    if side == "bid":
        direct_names = ("bid_levels", "bids", "bid_book")
    else:
        direct_names = ("ask_levels", "asks", "ask_book")
    value = _get_or_call(snapshot, *direct_names)
    if value is not None:
        try:
            return list(value), True
        except TypeError:
            return None, False
    for parent_name in ("order_book", "book", "orderbook", "lob", "state"):
        parent = _get(snapshot, parent_name)
        if parent is not None:
            value = _get_or_call(parent, *direct_names)
            if value is not None:
                try:
                    return list(value), True
                except TypeError:
                    pass
    return None, False


def _serialize_levels(levels: Any, price_scale: float, limit: int, descending: bool) -> tuple[list[dict[str, float]], bool]:
    if levels is None:
        return [], False
    converted: list[dict[str, float]] = []
    for level in levels:
        price = _number(_get(level, "price", "limit_price"))
        quantity = _level_quantity(level)
        if price is None or quantity is None or quantity < 0:
            continue
        converted.append({"price": price / price_scale, "quantity": quantity})
    converted.sort(key=lambda level: level["price"], reverse=descending)
    truncated = len(converted) > limit
    return converted[:limit], truncated


def _sample_indices(n: int, cap: int) -> list[int]:
    if n <= 0:
        return []
    if n <= cap:
        return list(range(n))
    if cap <= 1:
        return [0]
    # Deterministic, evenly spaced samples including the beginning and end.
    return sorted({round(i * (n - 1) / (cap - 1)) for i in range(cap)})


def _book_record(snapshot: Any, index: int, price_scale: float, levels_limit: int) -> dict[str, Any]:
    bid_raw, bid_detail = _find_levels(snapshot, "bid")
    ask_raw, ask_detail = _find_levels(snapshot, "ask")
    bids, bid_truncated = _serialize_levels(bid_raw, price_scale, levels_limit, True)
    asks, ask_truncated = _serialize_levels(ask_raw, price_scale, levels_limit, False)

    raw_best_bid = _number(_get_or_call(snapshot, "best_bid", "bid_price", "bid"))
    raw_best_ask = _number(_get_or_call(snapshot, "best_ask", "ask_price", "ask"))
    # In the established C++ API, zero is the empty-quote sentinel, not a $0.00 quote.
    best_bid = (raw_best_bid / price_scale) if raw_best_bid not in (None, 0.0) else (bids[0]["price"] if bids else None)
    best_ask = (raw_best_ask / price_scale) if raw_best_ask not in (None, 0.0) else (asks[0]["price"] if asks else None)
    midpoint_raw = _number(_get_or_call(snapshot, "midpoint", "mid_price", "mid"))
    spread_raw = _number(_get_or_call(snapshot, "spread"))
    two_sided = best_bid is not None and best_ask is not None
    midpoint = (midpoint_raw / price_scale) if midpoint_raw not in (None, 0.0) and two_sided else (
        (best_bid + best_ask) / 2 if two_sided else None
    )
    spread = (spread_raw / price_scale) if spread_raw is not None and two_sided else (
        best_ask - best_bid if two_sided else None
    )
    bid_depth = _number(_get_or_call(snapshot, "bid_depth", "total_bid_quantity", "bid_depth_quantity"))
    ask_depth = _number(_get_or_call(snapshot, "ask_depth", "total_ask_quantity", "ask_depth_quantity"))
    if bid_depth is None and bid_detail and not bid_truncated:
        bid_depth = sum(level["quantity"] for level in bids)
    if ask_depth is None and ask_detail and not ask_truncated:
        ask_depth = sum(level["quantity"] for level in asks)
    imbalance = _number(_get_or_call(snapshot, "imbalance"))
    if imbalance is None and bid_depth is not None and ask_depth is not None and bid_depth + ask_depth > 0:
        imbalance = (bid_depth - ask_depth) / (bid_depth + ask_depth)

    timestamp = _get_or_call(snapshot, "timestamp_ms", "time_ms", "timestamp", "time")
    message_index = _get_or_call(snapshot, "message_index", "message_number", "message_count", "index")
    return {
        "index": index,
        "timestamp": _json_value(timestamp),
        "message_index": int(message_index) if _number(message_index) is not None else index,
        "best_bid": best_bid,
        "best_ask": best_ask,
        "midpoint": midpoint,
        "spread": spread,
        "relative_spread": spread / midpoint if spread is not None and midpoint not in (None, 0) else None,
        "bid_depth": bid_depth,
        "ask_depth": ask_depth,
        "imbalance": imbalance,
        "bids": bids,
        "asks": asks,
        "levels_available": bool(bid_detail or ask_detail),
        "bids_available": bool(bid_detail),
        "asks_available": bool(ask_detail),
        "levels_truncated": bool(bid_truncated or ask_truncated),
        "bids_truncated": bool(bid_truncated),
        "asks_truncated": bool(ask_truncated),
        "bids_complete": bool(bid_detail and not bid_truncated),
        "asks_complete": bool(ask_detail and not ask_truncated),
    }


def document_from_objects(
    snapshots: list[Any],
    *,
    price_scale: float = 100.0,
    max_snapshots: int = 10_000,
    levels_per_side: int = 50,
    source_kind: str = "itch",
    raw_snapshot_count: int | None = None,
) -> dict[str, Any]:
    if price_scale <= 0 or not math.isfinite(price_scale):
        raise ValueError("price_scale must be a finite positive number")
    indices = _sample_indices(len(snapshots), max_snapshots)
    records = [_book_record(snapshots[i], i, price_scale, levels_per_side) for i in indices]
    has_levels = any(record["levels_available"] for record in records)
    return {
        "schema_version": 1,
        "source_kind": source_kind,
        "price_scale": price_scale,
        "snapshot_count_raw": len(snapshots) if raw_snapshot_count is None else int(raw_snapshot_count),
        "snapshot_count": len(records),
        "downsampled": len(records) < len(snapshots) or (raw_snapshot_count is not None and int(raw_snapshot_count) > len(snapshots)),
        "depth_available": has_levels,
        "snapshots": records,
    }


def _parse_levels_json(row: dict[str, str], candidates: tuple[str, ...]) -> list[Any] | None:
    for name in candidates:
        value = row.get(name)
        if not value:
            continue
        try:
            parsed = json.loads(value)
        except json.JSONDecodeError:
            continue
        if isinstance(parsed, list):
            return parsed
    return None


def _csv_record(row: dict[str, str], index: int, scale: float, levels_limit: int) -> dict[str, Any]:
    row = {str(key).strip().lower(): value for key, value in row.items() if key is not None}

    def value(*names: str) -> float | None:
        for name in names:
            raw = row.get(name)
            if raw not in (None, ""):
                number = _number(raw)
                if number is not None:
                    return number
        return None

    raw_bid = value("best_bid", "bid_price", "bid")
    raw_ask = value("best_ask", "ask_price", "ask")
    best_bid = raw_bid / scale if raw_bid not in (None, 0.0) else None
    best_ask = raw_ask / scale if raw_ask not in (None, 0.0) else None
    mid_raw = value("midpoint", "mid_price", "mid")
    spread_raw = value("spread")
    two_sided = best_bid is not None and best_ask is not None
    # The supported aggregate CSV schema may contain midpoint/spread without
    # individual best-quote columns. Preserve those observed fields instead
    # of silently discarding them. When quotes are present, retain the usual
    # two-sided-book rule so a one-sided book never gets a fabricated midpoint.
    aggregate_only = raw_bid is None and raw_ask is None
    midpoint = (
        mid_raw / scale
        if mid_raw not in (None, 0.0) and (two_sided or aggregate_only)
        else ((best_bid + best_ask) / 2 if two_sided else None)
    )
    spread = (
        spread_raw / scale
        if spread_raw is not None and (two_sided or aggregate_only)
        else (best_ask - best_bid if two_sided else None)
    )
    bids_json = _parse_levels_json(row, ("bid_levels_json", "bids_json", "bid_levels"))
    asks_json = _parse_levels_json(row, ("ask_levels_json", "asks_json", "ask_levels"))

    def levels(source: list[Any] | None, descending: bool) -> tuple[list[dict[str, float]], bool]:
        if source is None:
            return [], False
        found: list[dict[str, float]] = []
        for level in source:
            if isinstance(level, dict):
                p = _number(level.get("price"))
                q = _number(level.get("quantity", level.get("total_quantity", level.get("size"))))
            elif isinstance(level, (list, tuple)) and len(level) >= 2:
                p, q = _number(level[0]), _number(level[1])
            else:
                continue
            if p is not None and q is not None and q >= 0:
                found.append({"price": p / scale, "quantity": q})
        found.sort(key=lambda item: item["price"], reverse=descending)
        return found[:levels_limit], len(found) > levels_limit

    bids, bid_trunc = levels(bids_json, True)
    asks, ask_trunc = levels(asks_json, False)
    bd = value("bid_depth", "total_bid_quantity", "bid_depth_quantity")
    ad = value("ask_depth", "total_ask_quantity", "ask_depth_quantity")
    imb = value("imbalance")
    if imb is None and bd is not None and ad is not None and bd + ad > 0:
        imb = (bd - ad) / (bd + ad)
    message_index = value("message_index", "message_number", "index")
    timestamp = row.get("timestamp_ms", row.get("timestamp", row.get("time_ms", "")))
    return {
        "index": index,
        "timestamp": _json_value(timestamp),
        "message_index": int(message_index) if message_index is not None else index,
        "best_bid": best_bid if best_bid is not None else (bids[0]["price"] if bids else None),
        "best_ask": best_ask if best_ask is not None else (asks[0]["price"] if asks else None),
        "midpoint": midpoint,
        "spread": spread,
        "relative_spread": spread / midpoint if spread is not None and midpoint not in (None, 0) else None,
        "bid_depth": bd,
        "ask_depth": ad,
        "imbalance": imb,
        "bids": bids,
        "asks": asks,
        "levels_available": bids_json is not None or asks_json is not None,
        "bids_available": bids_json is not None,
        "asks_available": asks_json is not None,
        "levels_truncated": bid_trunc or ask_trunc,
        "bids_truncated": bid_trunc,
        "asks_truncated": ask_trunc,
        "bids_complete": bids_json is not None and not bid_trunc,
        "asks_complete": asks_json is not None and not ask_trunc,
    }


def document_from_csv(
    path: Path,
    *,
    row_count: int,
    price_scale: float = 100.0,
    max_snapshots: int = 10_000,
    levels_per_side: int = 50,
) -> dict[str, Any]:
    if price_scale <= 0 or not math.isfinite(price_scale):
        raise ValueError("price_scale must be a finite positive number")
    # Select evenly spaced rows without retaining a full CSV in browser-oriented memory.
    indices = _sample_indices(row_count, max_snapshots)
    wanted = set(indices)
    records: list[dict[str, Any]] = []
    source_index = 0
    with Path(path).open("r", newline="", encoding="utf-8-sig") as file:
        reader = csv.DictReader(file)
        for row in reader:
            if not row or not any(value not in (None, "") for value in row.values()):
                continue
            if source_index in wanted:
                records.append(_csv_record(row, source_index, price_scale, levels_per_side))
            source_index += 1
    has_levels = any(record["levels_available"] for record in records)
    return {
        "schema_version": 1,
        "source_kind": "csv",
        "price_scale": price_scale,
        "snapshot_count_raw": row_count,
        "snapshot_count": len(records),
        "downsampled": len(records) < row_count,
        "depth_available": has_levels,
        "snapshots": records,
    }


def write_replay_document(document: dict[str, Any], destination: Path) -> Path:
    path = Path(destination)
    path.parent.mkdir(parents=True, exist_ok=True)
    temporary = path.with_suffix(path.suffix + ".tmp")
    temporary.write_text(json.dumps(document, ensure_ascii=False, allow_nan=False), encoding="utf-8")
    temporary.replace(path)
    return path


def heatmap_payload(document: dict[str, Any], max_frames: int = 240, max_prices: int = 100) -> dict[str, Any]:
    snapshots: list[dict[str, Any]] = document.get("snapshots", [])
    if not document.get("depth_available") or not snapshots:
        return {"available": False, "reason": "Per-level book snapshots are not present in this dataset.", "frames": [], "prices": [], "values": []}
    frame_indices = _sample_indices(len(snapshots), max_frames)
    frames = [snapshots[index] for index in frame_indices]

    occurrences: dict[float, int] = {}
    for frame in frames:
        for side in ("bids", "asks"):
            for level in frame.get(side, []):
                p = float(level["price"])
                occurrences[p] = occurrences.get(p, 0) + 1
    price_candidates = sorted(occurrences)
    if len(price_candidates) > max_prices:
        # Keep the most repeatedly observed levels, then restore price order.
        keep = sorted(occurrences, key=lambda p: (-occurrences[p], p))[:max_prices]
        price_candidates = sorted(keep)
    prices = price_candidates
    matrix: list[list[float | None]] = []
    for price in prices:
        line: list[float | None] = []
        for frame in frames:
            level_values: dict[float, float] = {}
            for side in ("bids", "asks"):
                for level in frame.get(side, []):
                    level_values[float(level["price"])] = float(level["quantity"])
            if price in level_values:
                line.append(level_values[price])
            elif not frame.get("levels_available", bool(frame.get("bids") or frame.get("asks"))):
                # This frame has no level-level state at all; absence is not observed zero liquidity.
                line.append(None)
            else:
                # A price is only known empty on a side when that side's levels were observed.
                # None means the side/range was not observed, not that quantity equals zero.
                midpoint = _number(frame.get("midpoint"))
                if midpoint is None and frame.get("best_bid") is not None and frame.get("best_ask") is not None:
                    midpoint = (float(frame["best_bid"]) + float(frame["best_ask"])) / 2.0
                side = "bids" if midpoint is not None and price <= midpoint else "asks" if midpoint is not None else None
                side_available = bool(frame.get(f"{side}_available", bool(frame.get(side or "", [])))) if side else False
                all_bids = [float(x["price"]) for x in frame.get("bids", [])]
                all_asks = [float(x["price"]) for x in frame.get("asks", [])]
                floor_bid = min(all_bids) if all_bids else None
                ceil_ask = max(all_asks) if all_asks else None
                unobserved_bid = side == "bids" and bool(frame.get("bids_truncated")) and floor_bid is not None and price < floor_bid
                unobserved_ask = side == "asks" and bool(frame.get("asks_truncated")) and ceil_ask is not None and price > ceil_ask
                line.append(0.0 if side_available and not (unobserved_bid or unobserved_ask) else None)
        matrix.append(line)
    return {
        "available": True,
        "frames": [
            {"index": f.get("index"), "timestamp": f.get("timestamp"), "message_index": f.get("message_index"),
             "midpoint": f.get("midpoint"), "best_bid": f.get("best_bid"), "best_ask": f.get("best_ask")}
            for f in frames
        ],
        "prices": prices,
        "values": matrix,
        "quantity_max": max((value for line in matrix for value in line if value is not None), default=0.0),
        "frame_indices": frame_indices,
    }
