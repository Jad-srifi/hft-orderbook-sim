from __future__ import annotations

from dataclasses import dataclass

from webapp.services import replay_service


@dataclass
class Level:
    price: int
    total_quantity: int


@dataclass
class Snapshot:
    timestamp_ms: int
    message_index: int
    best_bid: int | None
    best_ask: int | None
    spread: int | None
    bid_depth: int | None
    ask_depth: int | None
    imbalance: float | None
    bid_levels: list[Level]
    ask_levels: list[Level]


def sample_snapshots() -> list[Snapshot]:
    return [
        Snapshot(
            timestamp_ms=1000, message_index=10,
            best_bid=10000, best_ask=10100, spread=100,
            bid_depth=17, ask_depth=9, imbalance=8 / 26,
            bid_levels=[Level(10000, 10), Level(9900, 7)],
            ask_levels=[Level(10100, 6), Level(10200, 3)],
        ),
        Snapshot(
            timestamp_ms=2000, message_index=20,
            best_bid=10000, best_ask=10100, spread=100,
            bid_depth=14, ask_depth=11, imbalance=3 / 25,
            bid_levels=[Level(10000, 8), Level(9900, 6)],
            ask_levels=[Level(10100, 8), Level(10200, 3)],
        ),
    ]


def test_depth_ladder_uses_actual_levels_and_converts_integer_ticks() -> None:
    document = replay_service.document_from_objects(sample_snapshots(), price_scale=100, max_snapshots=100)
    assert document["depth_available"] is True
    first = document["snapshots"][0]
    assert first["best_bid"] == 100.0
    assert first["best_ask"] == 101.0
    assert first["spread"] == 1.0
    assert first["bids"] == [{"price": 100.0, "quantity": 10.0}, {"price": 99.0, "quantity": 7.0}]
    assert first["asks"] == [{"price": 101.0, "quantity": 6.0}, {"price": 102.0, "quantity": 3.0}]


def test_heatmap_contains_actual_quantity_and_missing_as_zero_when_observed() -> None:
    document = replay_service.document_from_objects(sample_snapshots(), price_scale=100)
    heatmap = replay_service.heatmap_payload(document, max_frames=20, max_prices=20)
    assert heatmap["available"] is True
    p99_index = heatmap["prices"].index(99.0)
    p100_index = heatmap["prices"].index(100.0)
    assert heatmap["values"][p99_index] == [7.0, 6.0]
    assert heatmap["values"][p100_index] == [10.0, 8.0]
    assert heatmap["frames"][0]["message_index"] == 10


def test_truncated_price_level_range_is_unknown_not_zero() -> None:
    # Frame 0 has only the top level saved on either side. Frame 1 establishes
    # that 98 and 104 are real levels elsewhere in the sequence, not fabricated prices.
    document = {
        "depth_available": True,
        "snapshots": [
            {"index": 0, "timestamp": 1000, "message_index": 1, "midpoint": 101.0,
             "best_bid": 100.0, "best_ask": 102.0,
             "bids": [{"price": 100.0, "quantity": 10.0}],
             "asks": [{"price": 102.0, "quantity": 10.0}],
             "levels_truncated": True, "bids_truncated": True, "asks_truncated": True},
            {"index": 1, "timestamp": 2000, "message_index": 2, "midpoint": 101.0,
             "best_bid": 100.0, "best_ask": 102.0,
             "bids": [{"price": 100.0, "quantity": 10.0}, {"price": 98.0, "quantity": 4.0}],
             "asks": [{"price": 102.0, "quantity": 10.0}, {"price": 104.0, "quantity": 5.0}],
             "levels_truncated": False, "bids_truncated": False, "asks_truncated": False},
        ],
    }
    heatmap = replay_service.heatmap_payload(document)
    p98_index = heatmap["prices"].index(98.0)
    p104_index = heatmap["prices"].index(104.0)
    assert heatmap["values"][p98_index][0] is None
    assert heatmap["values"][p104_index][0] is None
    assert heatmap["values"][p98_index][1] == 4.0
    assert heatmap["values"][p104_index][1] == 5.0


def test_aggregate_observations_do_not_invent_per_level_data(tmp_path) -> None:
    source = tmp_path / "observations.csv"
    source.write_text(
        "timestamp_ms,message_index,best_bid,best_ask,midpoint,spread,bid_depth,ask_depth,imbalance\n"
        "1000,10,10000,10100,10050,100,400,350,0.0666667\n"
        "2000,20,10000,10100,10050,100,410,340,0.0933333\n",
        encoding="utf-8",
    )
    doc = replay_service.document_from_csv(source, row_count=2, price_scale=100)
    assert doc["depth_available"] is False
    heatmap = replay_service.heatmap_payload(doc)
    assert heatmap["available"] is False
    assert heatmap["values"] == []


def test_csv_with_level_json_is_replayable(tmp_path) -> None:
    source = tmp_path / "levels.csv"
    source.write_text(
        'message_index,best_bid,best_ask,bid_depth,ask_depth,bid_levels_json,ask_levels_json\n'
        '10,10000,10100,10,8,"[{""price"":10000,""quantity"":10}]","[{""price"":10100,""quantity"":8}]"\n',
        encoding="utf-8",
    )
    doc = replay_service.document_from_csv(source, row_count=1, price_scale=100)
    assert doc["depth_available"] is True
    assert doc["snapshots"][0]["bids"] == [{"price": 100.0, "quantity": 10.0}]


def test_zero_quote_sentinel_displays_one_sided_book_without_fake_midpoint() -> None:
    snapshot = Snapshot(
        timestamp_ms=1000, message_index=1,
        best_bid=0, best_ask=10100, spread=0,
        bid_depth=0, ask_depth=12, imbalance=-1.0,
        bid_levels=[], ask_levels=[Level(10100, 12)],
    )
    document = replay_service.document_from_objects([snapshot], price_scale=100)
    record = document["snapshots"][0]
    assert record["best_bid"] is None
    assert record["best_ask"] == 101.0
    assert record["midpoint"] is None
    assert record["spread"] is None
    assert record["levels_available"] is True
    assert record["bids"] == []
    assert record["asks"] == [{"price": 101.0, "quantity": 12.0}]


def test_aggregate_csv_schema_preserves_midpoint_and_spread_without_best_quotes(tmp_path) -> None:
    source = tmp_path / "aggregate.csv"
    source.write_text(
        "timestamp_ms,message_index,midpoint,spread,bid_depth,ask_depth\n"
        "1000,10,10050,100,400,350\n"
        "2000,20,10075,125,410,340\n",
        encoding="utf-8",
    )
    doc = replay_service.document_from_csv(source, row_count=2, price_scale=100)
    assert doc["snapshots"][0]["midpoint"] == 100.5
    assert doc["snapshots"][0]["spread"] == 1.0
    assert doc["snapshots"][1]["midpoint"] == 100.75
    assert doc["snapshots"][1]["spread"] == 1.25
    assert doc["depth_available"] is False

