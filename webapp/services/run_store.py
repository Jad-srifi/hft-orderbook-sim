from __future__ import annotations

import json
import os
import tempfile
import threading
import time
import uuid
from datetime import datetime, timezone
from pathlib import Path
from typing import Any


ITCH_COUNTER_DEFAULTS = {
    "messages_read": 0,
    "applied_messages": 0,
    "ignored_different_stock_locate": 0,
    "non_applied_messages": 0,
    "failed_applications": 0,
    "parse_errors": 0,
    "mapping_errors": 0,
    "messages_accounted": 0,
    "snapshots_captured": 0,
}


# Share locks between RunStore instances using the same root directory.
_ROOT_LOCKS_GUARD = threading.Lock()
_ROOT_LOCKS: dict[str, Any] = {}

_REPLACE_ATTEMPTS = 8
_INITIAL_RETRY_DELAY = 0.025
_MAX_RETRY_DELAY = 0.2


def _lock_for_root(root: Path) -> threading.RLock:
    key = os.path.normcase(str(root.resolve()))

    with _ROOT_LOCKS_GUARD:
        lock = _ROOT_LOCKS.get(key)

        if lock is None:
            lock = threading.RLock()
            _ROOT_LOCKS[key] = lock

        return lock


class RunStore:
    """Local run registry with atomic, Windows-safe metadata persistence."""

    def __init__(self, root: Path):
        self.root = Path(root)
        self.root.mkdir(parents=True, exist_ok=True)

        self._lock = _lock_for_root(self.root)

        self._repair_interrupted_runs()

    @staticmethod
    def now() -> str:
        return datetime.now(timezone.utc).isoformat()

    def _metadata_path(self, run_id: str) -> Path:
        if not run_id or any(
            ch not in "0123456789abcdef-"
            for ch in run_id.lower()
        ):
            raise ValueError("Invalid run identifier.")

        path = (self.root / run_id / "metadata.json").resolve()

        if self.root.resolve() not in path.parents:
            raise ValueError("Invalid run identifier.")

        return path

    def create(self, fields: dict[str, Any]) -> dict[str, Any]:
        with self._lock:
            run_id = uuid.uuid4().hex[:16]
            directory = self.root / run_id

            directory.mkdir(parents=True, exist_ok=False)
            (directory / "input").mkdir()
            (directory / "output").mkdir()

            now = self.now()

            metadata = {
                "id": run_id,
                "created_at": now,
                "updated_at": now,
                "status": "queued",
                "stage": "queued",
                "progress": 0,
                **ITCH_COUNTER_DEFAULTS,
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

    def update(
        self,
        run_id: str,
        **fields: Any,
    ) -> dict[str, Any]:
        with self._lock:
            current = self.get(run_id)

            if current is None:
                raise KeyError(run_id)

            current.update(fields)
            current["updated_at"] = self.now()

            self._write(
                self._metadata_path(run_id),
                current,
            )

            return current

    def list(self, limit: int = 50) -> list[dict[str, Any]]:
        with self._lock:
            found: list[dict[str, Any]] = []

            for path in self.root.glob("*/metadata.json"):
                try:
                    item = json.loads(
                        path.read_text(encoding="utf-8")
                    )
                except (OSError, json.JSONDecodeError):
                    continue

                found.append(item)

            found.sort(
                key=lambda item: item.get("created_at", ""),
                reverse=True,
            )

            return found[: max(1, min(int(limit), 200))]

    def directory(self, run_id: str) -> Path | None:
        with self._lock:
            item = self.get(run_id)

            if item is None:
                return None

            return (self.root / run_id).resolve()

    def _write(
        self,
        path: Path,
        data: dict[str, Any],
    ) -> None:
        """Atomically replace a JSON file, retrying transient Windows locks.

        A unique temporary file prevents concurrent writers from colliding
        on the same .tmp path. The temporary file is created in the target
        directory so the final replacement stays on the same filesystem.
        """

        with self._lock:
            path.parent.mkdir(parents=True, exist_ok=True)

            payload = json.dumps(
                data,
                ensure_ascii=False,
                allow_nan=False,
                indent=2,
            ) + "\n"

            temporary_path: Path | None = None

            try:
                # Windows cannot replace a file while this temporary file
                # remains open, so close it before calling os.replace().
                with tempfile.NamedTemporaryFile(
                    mode="w",
                    encoding="utf-8",
                    dir=path.parent,
                    prefix=f".{path.name}.",
                    suffix=".tmp",
                    delete=False,
                ) as temporary_file:
                    temporary_path = Path(temporary_file.name)
                    temporary_file.write(payload)
                    temporary_file.flush()

                delay = _INITIAL_RETRY_DELAY

                for attempt in range(_REPLACE_ATTEMPTS):
                    try:
                        os.replace(temporary_path, path)

                        # Replacement succeeded; the temporary path no
                        # longer exists.
                        temporary_path = None
                        return

                    except OSError as exc:
                        winerror = getattr(exc, "winerror", None)

                        retryable = (
                            isinstance(exc, PermissionError)
                            or winerror in (5, 32, 33)
                        )

                        if (
                            not retryable
                            or attempt == _REPLACE_ATTEMPTS - 1
                        ):
                            raise

                        time.sleep(delay)
                        delay = min(
                            delay * 2,
                            _MAX_RETRY_DELAY,
                        )

            finally:
                # Clean up temporary files after serialization or
                # replacement failures.
                if temporary_path is not None:
                    try:
                        temporary_path.unlink(missing_ok=True)
                    except OSError:
                        pass

    def _repair_interrupted_runs(self) -> None:
        with self._lock:
            for path in self.root.glob("*/metadata.json"):
                try:
                    meta = json.loads(
                        path.read_text(encoding="utf-8")
                    )
                except (OSError, json.JSONDecodeError):
                    continue

                if meta.get("status") in {"queued", "running"}:
                    meta.update(
                        {
                            "status": "failed",
                            "stage": "interrupted",
                            "progress": None,
                            "message": (
                                "The dashboard process restarted before "
                                "this run completed."
                            ),
                            "error": (
                                "Interrupted local run; submit the input again."
                            ),
                            "updated_at": self.now(),
                        }
                    )

                    try:
                        self._write(path, meta)
                    except OSError:
                        pass


def public_run(meta: dict[str, Any]) -> dict[str, Any]:
    """Remove filesystem paths and internal traces before returning API data."""

    hidden = {
        "source_path",
        "run_dir",
        "output_dir",
        "internal_traceback",
    }

    return {
        key: value
        for key, value in meta.items()
        if key not in hidden
    }