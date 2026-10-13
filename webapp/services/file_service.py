from __future__ import annotations

import csv
import gzip
import math
import os
import re
from pathlib import Path
from typing import Any, BinaryIO

SUPPORTED_SUFFIXES = {".txt", ".gz", ".csv"}
OBSERVATION_FIELDS = (
    "timestamp_ms", "message_index", "best_bid", "best_ask", "midpoint",
    "spread", "relative_spread", "bid_depth", "ask_depth", "imbalance",
)
_REQUIRED_GROUPS = (
    {"midpoint", "spread", "bid_depth", "ask_depth"},
    {"best_bid", "best_ask", "bid_depth", "ask_depth"},
    {"bid", "ask", "bid_depth", "ask_depth"},
)
_NUMERIC_FIELDS = {
    "timestamp_ms", "time_ms", "message_index", "message_number",
    "best_bid", "best_ask", "bid", "ask", "bid_price", "ask_price", "midpoint",
    "mid_price", "mid", "spread", "relative_spread", "bid_depth", "ask_depth",
    "total_bid_quantity", "total_ask_quantity", "imbalance", "trade_volume", "trade_count",
}


class InputValidationError(ValueError):
    """The uploaded file is not a supported or valid research input."""


def _display_filename(name: str | None) -> str:
    raw = (name or "upload").replace("\\", "/").split("/")[-1]
    raw = "".join(ch for ch in raw if ch.isprintable() and ch not in "\r\n\t")
    raw = re.sub(r"[^A-Za-z0-9._ ()-]", "_", raw).strip(" .")
    return raw[:120] or "upload"


def save_stream(
    stream: BinaryIO,
    original_name: str,
    destination: Path,
    max_bytes: int,
    *,
    max_csv_rows: int = 5_000_000,
) -> tuple[Path, str, int, dict[str, Any]]:
    """Stream an upload to a generated path; never use the client filename as a path."""
    display_name = _display_filename(original_name)
    suffix = Path(display_name).suffix.lower()
    if suffix not in SUPPORTED_SUFFIXES:
        raise InputValidationError("Unsupported file type. Upload a .txt, .gz, or .csv file.")

    destination.parent.mkdir(parents=True, exist_ok=True)
    written = 0
    try:
        with destination.open("wb") as output:
            while True:
                chunk = stream.read(1024 * 1024)
                if not chunk:
                    break
                written += len(chunk)
                if written > max_bytes:
                    raise InputValidationError(f"Upload exceeds the configured {max_bytes // (1024 * 1024)} MB limit.")
                output.write(chunk)
    except Exception:
        destination.unlink(missing_ok=True)
        raise

    if written == 0:
        destination.unlink(missing_ok=True)
        raise InputValidationError("The uploaded file is empty.")

    try:
        inspected = validate_input(destination, suffix, max_csv_rows=max_csv_rows)
    except Exception:
        destination.unlink(missing_ok=True)
        raise
    return destination, display_name, written, inspected


def _read_itch_prefix(path: Path, suffix: str) -> bytes:
    opener = gzip.open if suffix == ".gz" else open
    try:
        with opener(path, "rb") as file:
            prefix = file.read(4096)
    except (OSError, EOFError) as exc:
        raise InputValidationError(
            f"Could not read the {'gzip-compressed ' if suffix == '.gz' else ''}ITCH input: {exc}"
        ) from exc
    if not prefix:
        raise InputValidationError("The ITCH input contains no readable payload.")
    return prefix


def validate_input(path: Path, suffix: str, max_csv_rows: int = 5_000_000) -> dict[str, Any]:
    suffix = suffix.lower()
    if suffix == ".csv":
        return validate_observation_csv(path, max_rows=max_csv_rows)
    if suffix == ".gz":
        with path.open("rb") as file:
            if file.read(2) != b"\x1f\x8b":
                raise InputValidationError("The file has a .gz extension but is not gzip data.")
    # TotalView-ITCH is a binary protocol even when the upstream file has a .txt extension.
    # Validate readability and gzip framing here; leave message framing/semantics to the existing C++ parser.
    prefix = _read_itch_prefix(path, suffix)
    return {"kind": "itch", "format": suffix, "prefix_bytes_checked": len(prefix)}


def validate_observation_csv(path: Path, max_rows: int = 5_000_000) -> dict[str, Any]:
    """Validate documented aggregate observation/dataset CSV schemas without loading it all."""
    try:
        with path.open("r", newline="", encoding="utf-8-sig") as file:
            reader = csv.DictReader(file)
            fields = [name.strip() for name in (reader.fieldnames or []) if name and name.strip()]
            lower_fields = {name.lower() for name in fields}
            if not fields:
                raise InputValidationError("CSV has no header row.")
            if not any(required <= lower_fields for required in _REQUIRED_GROUPS):
                expected = "; or ".join(", ".join(sorted(group)) for group in _REQUIRED_GROUPS)
                raise InputValidationError(
                    "CSV does not match a supported observation schema. Required field groups: " + expected
                )
            rows = 0
            for row in reader:
                if not row or not any(value not in (None, "") for value in row.values()):
                    continue
                rows += 1
                if rows > max_rows:
                    raise InputValidationError(f"CSV exceeds the configured scan limit of {max_rows:,} data rows.")
                for key, value in row.items():
                    if key is None or value in (None, "") or key.strip().lower() not in _NUMERIC_FIELDS:
                        continue
                    try:
                        number = float(value)
                    except (TypeError, ValueError) as exc:
                        raise InputValidationError(f"CSV row {rows + 1}: field {key!r} must be numeric or blank.") from exc
                    if not math.isfinite(number):
                        raise InputValidationError(f"CSV row {rows + 1}: field {key!r} must be finite.")
                    if key.strip().lower() in {
                        "timestamp_ms", "time_ms", "message_index", "message_number",
                        "best_bid", "best_ask", "bid", "ask", "bid_price", "ask_price", "midpoint",
                        "mid_price", "mid", "spread", "bid_depth", "ask_depth", "total_bid_quantity",
                        "total_ask_quantity", "trade_volume", "trade_count",
                    } and number < 0:
                        raise InputValidationError(f"CSV row {rows + 1}: field {key!r} cannot be negative.")
            if rows == 0:
                raise InputValidationError("CSV contains a header but no observation rows.")
            return {"kind": "csv", "format": ".csv", "fields": fields, "row_count": rows}
    except UnicodeDecodeError as exc:
        raise InputValidationError("CSV must be UTF-8 or UTF-8 with BOM encoded.") from exc
    except csv.Error as exc:
        raise InputValidationError(f"Malformed CSV: {exc}") from exc


_LOCAL_EXCLUDED_DIRS = {
    ".git", ".venv", "venv", "build", "node_modules", "__pycache__",
    ".pytest_cache", "dashboard", "uploads",
}


def list_local_inputs(root: Path, *, limit: int = 500, scan_limit: int = 20_000) -> list[dict[str, Any]]:
    """List supported files under project data/ and results/ without reading large payloads."""
    root = Path(root).resolve()
    allowed_roots = [
        candidate.resolve()
        for candidate in (root / "data", root / "results")
        if candidate.is_dir() and _is_within(candidate.resolve(), root)
    ]
    found: list[dict[str, Any]] = []
    scanned = 0
    for allowed in allowed_roots:
        for current, directories, filenames in os.walk(allowed, followlinks=False):
            directories[:] = sorted(
                name for name in directories
                if name not in _LOCAL_EXCLUDED_DIRS and not name.startswith(".")
            )
            base = Path(current)
            for name in sorted(filenames):
                scanned += 1
                if scanned > scan_limit or len(found) >= limit:
                    break
                path = base / name
                if path.suffix.lower() not in SUPPORTED_SUFFIXES or not path.is_file():
                    continue
                try:
                    resolved = path.resolve(strict=True)
                    resolved.relative_to(allowed)
                    stat = resolved.stat()
                except (OSError, ValueError):
                    continue
                relative = resolved.relative_to(root).as_posix()
                found.append({
                    "path": relative,
                    "name": path.name,
                    "size_bytes": stat.st_size,
                    "modified_at": stat.st_mtime,
                    "kind": "csv" if path.suffix.lower() == ".csv" else "itch",
                })
            if scanned > scan_limit or len(found) >= limit:
                break
        if scanned > scan_limit or len(found) >= limit:
            break
    found.sort(key=lambda item: (item["path"].lower(), item["name"].lower()))
    return found


def resolve_local_input(root: Path, relative_path: str) -> Path:
    """Resolve a user-selected project-relative source confined to data/ or results/."""
    root = Path(root).resolve()
    if not isinstance(relative_path, str) or not relative_path.strip():
        raise InputValidationError("Select a local project file first.")
    candidate_rel = Path(relative_path)
    if candidate_rel.is_absolute() or ".." in candidate_rel.parts:
        raise InputValidationError("Choose a project-relative path under data/ or results/.")
    try:
        candidate = (root / candidate_rel).resolve(strict=True)
    except (OSError, RuntimeError) as exc:
        raise InputValidationError("The selected local file does not exist or cannot be resolved.") from exc
    allowed_roots = [
        path.resolve()
        for path in (root / "data", root / "results")
        if path.is_dir() and _is_within(path.resolve(), root)
    ]
    if not candidate.is_file() or not any(_is_within(candidate, allowed) for allowed in allowed_roots):
        raise InputValidationError("The selected file must be inside the project data/ or results/ directory.")
    if candidate.suffix.lower() not in SUPPORTED_SUFFIXES:
        raise InputValidationError("Unsupported file extension. Choose .txt, .gz, or .csv.")
    return candidate


def _is_within(path: Path, parent: Path) -> bool:
    try:
        path.relative_to(parent)
        return True
    except ValueError:
        return False
