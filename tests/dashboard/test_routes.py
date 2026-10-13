from __future__ import annotations

import io
import time
from pathlib import Path

import pytest

pytest.importorskip("flask")
from webapp import create_app


@pytest.fixture()
def app(tmp_path):
    root = Path(__file__).resolve().parents[2]
    application = create_app({
        "TESTING": True,
        "ROOT": tmp_path,
        "WEB_ROOT": root / "web",
        "RUNS_ROOT": tmp_path / "runs",
        "UPLOADS_ROOT": tmp_path / "uploads",
        "MAX_CONTENT_LENGTH": 1024 * 1024,
        "MAX_REPLAY_SNAPSHOTS": 100,
        "MAX_CSV_ROWS_TO_SCAN": 1000,
        "MAX_REPLAY_MESSAGES": 1000,
    })
    yield application


@pytest.fixture()
def client(app):
    return app.test_client()


def test_health_and_workspace_routes(client):
    assert client.get("/api/health").json["status"] == "ok"
    assert client.get("/research").status_code == 200
    assert client.get("/analysis").status_code == 200
    assert client.get("/replay").status_code == 200


def test_unsafe_or_unsupported_filename_rejected(client):
    response = client.post("/api/runs", data={"file": (io.BytesIO(b"hello"), "../../evil.exe")})
    assert response.status_code == 400
    assert "Unsupported" in response.json["error"]


def test_invalid_parameters_rejected_before_run_creation(client):
    csv_data = b"best_bid,best_ask,bid_depth,ask_depth\n100,101,2,3\n"
    response = client.post("/api/runs", data={
        "file": (io.BytesIO(csv_data), "sample.csv"),
        "stock_locate": "-1",
    }, content_type="multipart/form-data")
    assert response.status_code == 400
    assert "Stock Locate" in response.json["error"]
    assert client.get("/api/runs").json["runs"] == []


def test_small_csv_upload_runs_and_writes_replay_and_plots(client):
    csv_data = (
        b"timestamp_ms,message_index,best_bid,best_ask,midpoint,spread,bid_depth,ask_depth,imbalance\n"
        b"1000,10,10000,10100,10050,100,400,350,0.0666667\n"
        b"2000,20,10000,10100,10050,100,410,340,0.0933333\n"
        b"3000,30,10000,10100,10050,100,420,330,0.12\n"
    )
    response = client.post("/api/runs", data={
        "file": (io.BytesIO(csv_data), "sample.csv"),
        "stock_locate": "14",
        "message_limit": "100",
        "snapshot_interval": "10",
        "price_scale": "100",
    }, content_type="multipart/form-data")
    assert response.status_code == 202, response.get_data(as_text=True)
    run_id = response.json["run"]["id"]
    deadline = time.time() + 10
    status = None
    while time.time() < deadline:
        status_response = client.get(f"/api/runs/{run_id}")
        status = status_response.json["run"]
        if status["status"] in {"completed", "failed"}:
            break
        time.sleep(0.05)
    assert status is not None
    assert status["status"] == "completed", status.get("error")
    assert client.get(f"/api/runs/{run_id}/snapshots").status_code == 200
    plots = client.get(f"/api/runs/{run_id}/plots").json["plots"]
    assert plots
    artifacts = client.get(f"/api/runs/{run_id}/artifacts").json["artifacts"]
    assert any("dashboard_diagnostics" in item["path"] for item in artifacts)


def test_malformed_run_identifier_returns_not_found_not_server_error(client):
    response = client.get("/api/runs/not-a-safe-id!")
    assert response.status_code == 404
    assert response.json["error"] == "Run not found."



def test_local_input_listing_and_existing_csv_run(client, app, tmp_path):
    source = tmp_path / "results" / "existing_observations.csv"
    source.parent.mkdir(parents=True, exist_ok=True)
    source.write_text(
        "timestamp_ms,message_index,best_bid,best_ask,midpoint,spread,bid_depth,ask_depth\n"
        "1000,10,10000,10100,10050,100,400,350\n"
        "2000,20,10010,10110,10060,100,410,340\n",
        encoding="utf-8",
    )
    listing = client.get("/api/local-inputs")
    assert listing.status_code == 200
    assert any(item["path"] == "results/existing_observations.csv" for item in listing.json["files"])

    response = client.post("/api/runs/local", json={
        "path": "results/existing_observations.csv",
        "stock_locate": 14,
        "message_limit": 100,
        "snapshot_interval": 10,
        "price_scale": 100,
    })
    assert response.status_code == 202, response.get_data(as_text=True)
    run_id = response.json["run"]["id"]
    deadline = time.time() + 10
    status = None
    while time.time() < deadline:
        status = client.get(f"/api/runs/{run_id}").json["run"]
        if status["status"] in {"completed", "failed"}:
            break
        time.sleep(0.05)
    assert status and status["status"] == "completed", status
    assert status["input_mode"] == "local_file"
    assert status["source_relative_path"] == "results/existing_observations.csv"
    run_dir = app.config["RUNS_ROOT"] / run_id
    assert list((run_dir / "input").iterdir()) == []


def test_local_run_rejects_traversal_before_creating_run(client):
    response = client.post("/api/runs/local", json={"path": "../../etc/passwd"})
    assert response.status_code == 400
    assert client.get("/api/runs").json["runs"] == []
