import sys
from pathlib import Path

import pytest

PROJECT_ROOT = Path(__file__).resolve().parents[2]
PYTHON_DIR = PROJECT_ROOT / "python"
sys.path.insert(0, str(PYTHON_DIR))

from datasets import BookDataset
from observations import BookSnapshot


def make_snapshot(
    best_bid: int,
    best_ask: int,
    spread: int,
    midpoint: float,
) -> BookSnapshot:
    return BookSnapshot(
        best_bid=best_bid,
        best_ask=best_ask,
        spread=spread,
        midpoint=midpoint,
        bid_levels=((best_bid, 10),) if best_bid else (),
        ask_levels=((best_ask, 10),) if best_ask else (),
    )


def test_empty_dataset():
    dataset = BookDataset()

    assert len(dataset) == 0
    assert dataset.snapshots == ()


def test_add_one_snapshot():
    snapshot = make_snapshot(10000, 10100, 100, 10050.0)

    dataset = BookDataset()
    dataset.add(snapshot)

    assert len(dataset) == 1
    assert dataset[0] == snapshot
    assert dataset.snapshots == (snapshot,)


def test_add_multiple_snapshots():
    snapshot_1 = make_snapshot(10000, 10100, 100, 10050.0)
    snapshot_2 = make_snapshot(10050, 10150, 100, 10100.0)
    snapshot_3 = make_snapshot(10100, 10200, 100, 10150.0)

    dataset = BookDataset()

    dataset.add(snapshot_1)
    dataset.add(snapshot_2)
    dataset.add(snapshot_3)

    assert len(dataset) == 3
    assert dataset[0] == snapshot_1
    assert dataset[1] == snapshot_2
    assert dataset[2] == snapshot_3


def test_from_snapshots():
    snapshots = (
        make_snapshot(10000, 10100, 100, 10050.0),
        make_snapshot(10050, 10150, 100, 10100.0),
    )

    dataset = BookDataset.from_snapshots(snapshots)

    assert len(dataset) == 2
    assert dataset.snapshots == snapshots


def test_preserves_observation_order():
    snapshot_1 = make_snapshot(10000, 10100, 100, 10050.0)
    snapshot_2 = make_snapshot(9900, 10000, 100, 9950.0)
    snapshot_3 = make_snapshot(10100, 10200, 100, 10150.0)

    dataset = BookDataset.from_snapshots(
        [snapshot_1, snapshot_2, snapshot_3]
    )

    assert list(dataset) == [
        snapshot_1,
        snapshot_2,
        snapshot_3,
    ]


def test_indexing_and_slicing():
    snapshots = (
        make_snapshot(10000, 10100, 100, 10050.0),
        make_snapshot(10050, 10150, 100, 10100.0),
        make_snapshot(10100, 10200, 100, 10150.0),
    )

    dataset = BookDataset.from_snapshots(snapshots)

    assert dataset[0] == snapshots[0]
    assert dataset[-1] == snapshots[-1]
    assert dataset[1:] == snapshots[1:]


def test_snapshots_property_is_read_only_copy():
    snapshot = make_snapshot(10000, 10100, 100, 10050.0)

    dataset = BookDataset([snapshot])
    returned = dataset.snapshots

    assert isinstance(returned, tuple)
    assert returned == (snapshot,)


def test_rejects_invalid_snapshot():
    dataset = BookDataset()

    with pytest.raises(TypeError):
        dataset.add("not a snapshot")


def test_dataset_does_not_depend_on_orderbook():
    import lob_cpp
    from observations import BookSnapshot

    book = lob_cpp.OrderBook()

    book.add(
        lob_cpp.Order(
            1,
            lob_cpp.Side.BUY,
            10000,
            10,
        )
    )

    book.add(
        lob_cpp.Order(
            2,
            lob_cpp.Side.SELL,
            10100,
            10,
        )
    )

    snapshot = BookSnapshot.from_book(book)
    dataset = BookDataset([snapshot])

    book.modify(1, 10000, 5)

    assert dataset[0].bid_levels == ((10000, 10),)
    assert dataset[0].best_bid == 10000
    assert dataset[0].best_ask == 10100
    assert dataset[0].midpoint == 10050.0