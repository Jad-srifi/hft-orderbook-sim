from __future__ import annotations

import os
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
WEB_ROOT = ROOT / "web"
RUNS_ROOT = ROOT / "results" / "dashboard" / "runs"
UPLOADS_ROOT = ROOT / "results" / "dashboard" / "uploads"


class Config:
    SECRET_KEY = os.environ.get("DASHBOARD_SECRET_KEY", "local-only-dashboard")
    MAX_CONTENT_LENGTH = int(os.environ.get("DASHBOARD_MAX_UPLOAD_MB", "512")) * 1024 * 1024
    MAX_STREAM_BYTES = MAX_CONTENT_LENGTH
    MAX_REPLAY_SNAPSHOTS = int(os.environ.get("DASHBOARD_MAX_REPLAY_SNAPSHOTS", "10000"))
    MAX_LEVELS_PER_SIDE = int(os.environ.get("DASHBOARD_MAX_LEVELS_PER_SIDE", "50"))
    MAX_HEATMAP_FRAMES = int(os.environ.get("DASHBOARD_MAX_HEATMAP_FRAMES", "240"))
    MAX_HEATMAP_PRICES = int(os.environ.get("DASHBOARD_MAX_HEATMAP_PRICES", "100"))
    MAX_CSV_ROWS_TO_SCAN = int(os.environ.get("DASHBOARD_MAX_CSV_ROWS_TO_SCAN", "5000000"))
    MAX_REPLAY_MESSAGES = int(os.environ.get("DASHBOARD_MAX_REPLAY_MESSAGES", "20000000"))
    PRICE_SCALE_DEFAULT = 100
    RUNS_ROOT = RUNS_ROOT
    UPLOADS_ROOT = UPLOADS_ROOT
    WEB_ROOT = WEB_ROOT
    ROOT = ROOT
