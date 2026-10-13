from __future__ import annotations

import json
import threading
import uuid
from datetime import datetime, timezone
from pathlib import Path
from typing import Any


class RunStore:
    """Small local run registry persisted as one JSON file per run."""

    def __init__(self, root: Path):
        self.root = Path(root)
        self.root.mkdir(parents=True, exist_ok=True)
        self._lock = threading.RLock()
        self._repair_interrupted_runs()

    @staticmethod
    def now() -> str:
        return datetime.now(timezone.utc).isoformat()

    def _metadata_path(self, run_id: str) -> Path:
        if not run_id or any(ch not in "0123456789abcdef-" for ch in run_id.lower()):
            raise ValueError("Invalid run identifier.")
        path = (self.root / run_id / "metadata.json").resolve()
        if self.root.resolve() not in path.parents:
            raise ValueError("Invalid run identifier.")
        return path

    def create(self, fields: dict[str, Any]) -> dict[str, Any]:
        run_id = uuid.uuid4().hex[:16]
        directory = self.root / run_id
        directory.mkdir(parents=True, exist_ok=False)
        (directory / "input").mkdir()
        (directory / "output").mkdir()
        metadata = {
            "id": run_id,
            "created_at": self.now(),
            "updated_at": self.now(),
            "status": "queued",
            "stage": "queued",
            "progress": 0,
            "messages_read": 0,
            "snapshots_captured": 0,
            "message": "Run queued.",
            "error": None,
            "summary": None,
            "config": {},
            **fields,
        }
        self._write(directory / "metadata.json", metadata)
        return metadata

    def get(self, run_id: str) -> dict[str, Any] | None:
        with self._lock:
            try:
                path = self._metadata_path(run_id)
            except (TypeError, ValueError):
                return None
            if not path.is_file():
                return None
            try:
                return json.loads(path.read_text(encoding="utf-8"))
            except (OSError, json.JSONDecodeError):
                return None

    def update(self, run_id: str, **fields: Any) -> dict[str, Any]:
        with self._lock:
            current = self.get(run_id)
            if current is None:
                raise KeyError(run_id)
            current.update(fields)
            current["updated_at"] = self.now()
            self._write(self._metadata_path(run_id), current)
            return current

    def list(self, limit: int = 50) -> list[dict[str, Any]]:
        found: list[dict[str, Any]] = []
        for path in self.root.glob("*/metadata.json"):
            try:
                item = json.loads(path.read_text(encoding="utf-8"))
            except (OSError, json.JSONDecodeError):
                continue
            found.append(item)
        found.sort(key=lambda item: item.get("created_at", ""), reverse=True)
        return found[:max(1, min(int(limit), 200))]

    def directory(self, run_id: str) -> Path | None:
        item = self.get(run_id)
        if item is None:
            return None
        return (self.root / run_id).resolve()

    def _write(self, path: Path, data: dict[str, Any]) -> None:
        path.parent.mkdir(parents=True, exist_ok=True)
        temporary = path.with_suffix(".json.tmp")
        temporary.write_text(json.dumps(data, ensure_ascii=False, allow_nan=False, indent=2), encoding="utf-8")
        temporary.replace(path)

    def _repair_interrupted_runs(self) -> None:
        for path in self.root.glob("*/metadata.json"):
            try:
                meta = json.loads(path.read_text(encoding="utf-8"))
            except (OSError, json.JSONDecodeError):
                continue
            if meta.get("status") in {"queued", "running"}:
                meta.update({
                    "status": "failed",
                    "stage": "interrupted",
                    "progress": None,
                    "message": "The dashboard process restarted before this run completed.",
                    "error": "Interrupted local run; submit the input again.",
                    "updated_at": self.now(),
                })
                try:
                    self._write(path, meta)
                except OSError:
                    pass


def public_run(meta: dict[str, Any]) -> dict[str, Any]:
    """Remove all filesystem paths and internal error traces before returning API data."""
    hidden = {"source_path", "run_dir", "output_dir", "internal_traceback"}
    return {key: value for key, value in meta.items() if key not in hidden}
