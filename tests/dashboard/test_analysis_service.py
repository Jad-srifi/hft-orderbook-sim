from __future__ import annotations

import json
from pathlib import Path

from webapp.services import analysis_service


def test_missing_pytest_in_legacy_workflow_falls_back_to_bounded_cpp_replay(tmp_path, monkeypatch):
    root = tmp_path / "repo"
    (root / "examples").mkdir(parents=True)
    source = root / "data" / "sample.txt"
    source.parent.mkdir(parents=True)
    source.write_bytes(b"binary-ish ITCH fixture")
    run_dir = tmp_path / "runs" / "test-run"
    output = run_dir / "output"
    output.mkdir(parents=True)

    def missing_test_dependency(**kwargs):
        raise ModuleNotFoundError("No module named 'pytest'", name="pytest")

    monkeypatch.setattr(analysis_service, "_run_existing_workflow", missing_test_dependency)
    from webapp.services import itch_runner
    def fake_bounded_replay(*args, **kwargs):
        kwargs["progress_callback"](10, 10, 1, 88)
        return ([{
            "timestamp": 1000, "message_index": 10, "best_bid": 10000,
            "best_ask": 10100, "spread": 100, "bid_depth": 17, "ask_depth": 11,
            "imbalance": 6 / 28,
            "bid_levels": [{"price": 10000, "quantity": 12}, {"price": 9900, "quantity": 5}],
            "ask_levels": [{"price": 10100, "quantity": 8}, {"price": 10200, "quantity": 3}],
        }], 1, 10)

    monkeypatch.setattr(itch_runner, "run_bounded_itch_replay", fake_bounded_replay)

    metadata = {
        "id": "test-run", "created_at": "2026-10-13T00:00:00+00:00",
        "original_filename": "data/sample.txt", "input_kind": "itch", "input_size_bytes": source.stat().st_size,
        "config": {
            "stock_locate": 14, "message_limit": 10, "snapshot_interval": 10,
            "price_scale": 100, "max_replay_snapshots": 100, "levels_per_side": 50,
        },
    }
    progress_events = []
    summary, diagnostics = analysis_service.run_itch_analysis(
        root=root, run_dir=run_dir, source_path=source, metadata=metadata,
        progress=lambda *args: progress_events.append(args),
    )

    assert summary["workflow_used"] is False
    assert "imports pytest" in summary["workflow_warning"]
    assert summary["snapshot_count"] == 1
    assert summary["depth_available"] is True
    assert summary["dashboard_plot_count"] >= 1
    assert (output / "replay.json").is_file()
    assert "pytest" in (output / "workflow.log").read_text(encoding="utf-8")
    persisted = json.loads((output / "dashboard_diagnostics.json").read_text(encoding="utf-8"))
    assert persisted["workflow"]["used"] is False
    assert "pytest" in persisted["workflow"]["warning"]
    replay_progress = next(event for event in progress_events if len(event) == 4)
    assert replay_progress[3] == {"messages_read": 10, "snapshots_captured": 1}
