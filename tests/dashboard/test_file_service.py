from __future__ import annotations

import gzip
from io import BytesIO

import pytest

from webapp.services import file_service


def test_valid_observation_csv_and_row_count(tmp_path) -> None:
    path = tmp_path / "observations.csv"
    path.write_text(
        "timestamp_ms,message_index,best_bid,best_ask,bid_depth,ask_depth\n"
        "1000,1,10000,10100,12,14\n"
        "2000,2,10000,10100,10,15\n",
        encoding="utf-8",
    )
    meta = file_service.validate_input(path, ".csv")
    assert meta["kind"] == "csv"
    assert meta["row_count"] == 2
    assert "best_bid" in meta["fields"]


def test_invalid_csv_schema_rejected(tmp_path) -> None:
    path = tmp_path / "bad.csv"
    path.write_text("date,price\n2026-01-01,100\n", encoding="utf-8")
    with pytest.raises(file_service.InputValidationError, match="supported observation schema"):
        file_service.validate_input(path, ".csv")


def test_non_finite_csv_value_rejected(tmp_path) -> None:
    path = tmp_path / "bad.csv"
    path.write_text("best_bid,best_ask,bid_depth,ask_depth\n100,101,nan,10\n", encoding="utf-8")
    with pytest.raises(file_service.InputValidationError, match="finite"):
        file_service.validate_input(path, ".csv")


def test_gzip_itch_text_validates_without_decompressing_full_file(tmp_path) -> None:
    path = tmp_path / "feed.gz"
    path.write_bytes(gzip.compress(b"A\nF\nE\n"))
    result = file_service.validate_input(path, ".gz")
    assert result["kind"] == "itch"


def test_corrupt_gzip_rejected(tmp_path) -> None:
    path = tmp_path / "corrupt.gz"
    path.write_bytes(b"not gzip")
    with pytest.raises(file_service.InputValidationError):
        file_service.validate_input(path, ".gz")


def test_filename_is_not_used_as_destination_and_size_limit_is_enforced(tmp_path) -> None:
    destination = tmp_path / "run" / "input" / "source.txt"
    path, displayed, size, inspected = file_service.save_stream(
        BytesIO(b"A\nF\n"), "../../arbitrary.txt", destination, max_bytes=1024
    )
    assert path == destination
    assert displayed == "arbitrary.txt"
    assert size == 4
    assert inspected["kind"] == "itch"
    with pytest.raises(file_service.InputValidationError, match="exceeds"):
        file_service.save_stream(BytesIO(b"A" * 2048), "feed.txt", tmp_path / "limited.txt", max_bytes=100)
    assert not (tmp_path / "limited.txt").exists()


def test_binary_itch_payload_is_not_misclassified_as_corrupt_text(tmp_path) -> None:
    # ITCH is a binary wire protocol; the historical filename may nevertheless end in .txt.
    path = tmp_path / "S101819-v50.txt"
    path.write_bytes(bytes([0, 10, ord("A"), 0, 17, ord("F"), 255, 0]))
    result = file_service.validate_input(path, ".txt")
    assert result["kind"] == "itch"
    assert result["prefix_bytes_checked"] == path.stat().st_size



def test_local_input_discovery_and_resolution_do_not_copy_large_sources(tmp_path) -> None:
    source = tmp_path / "data" / "itch" / "S101819-v50.txt"
    source.parent.mkdir(parents=True)
    source.write_bytes(bytes([0, 10, ord("A")]) * 32)

    discovered = file_service.list_local_inputs(tmp_path)
    assert any(item["path"] == "data/itch/S101819-v50.txt" for item in discovered)
    resolved = file_service.resolve_local_input(tmp_path, "data/itch/S101819-v50.txt")
    assert resolved == source.resolve()
    assert resolved.stat().st_size == 96


def test_local_input_rejects_traversal_unsupported_and_outside_paths(tmp_path) -> None:
    data_dir = tmp_path / "data" / "itch"
    data_dir.mkdir(parents=True)
    outside = tmp_path.parent / "outside.txt"
    outside.write_bytes(b"ITCH")
    with pytest.raises(file_service.InputValidationError, match="project-relative"):
        file_service.resolve_local_input(tmp_path, "../outside.txt")
    with pytest.raises(file_service.InputValidationError, match="project-relative"):
        file_service.resolve_local_input(tmp_path, str(outside.resolve()))
    (data_dir / "bad.exe").write_bytes(b"not a supported input")
    with pytest.raises(file_service.InputValidationError, match="Unsupported"):
        file_service.resolve_local_input(tmp_path, "data/itch/bad.exe")
