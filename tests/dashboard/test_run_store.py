from __future__ import annotations

from webapp.services.run_store import RunStore, public_run


def test_malformed_identifier_is_a_miss_not_an_exception(tmp_path):
    store = RunStore(tmp_path / "runs")
    assert store.get("../../outside") is None
    assert store.get("!invalid!") is None


def test_run_metadata_is_persisted_and_paths_are_hidden(tmp_path):
    store = RunStore(tmp_path / "runs")
    record = store.create({
        "source_path": "/private/input.txt",
        "run_dir": "/private/run",
        "output_dir": "/private/run/output",
        "original_filename": "source.txt",
    })
    loaded = store.get(record["id"])
    assert loaded is not None
    assert loaded["id"] == record["id"]
    public = public_run(loaded)
    assert "source_path" not in public
    assert "run_dir" not in public
    assert "output_dir" not in public
