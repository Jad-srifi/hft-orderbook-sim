from __future__ import annotations

import shutil
import math
import threading
import traceback
from pathlib import Path
from functools import lru_cache
import json
from typing import Any

from flask import Flask, jsonify, request, send_from_directory
from werkzeug.utils import secure_filename

from .config import Config
from .services import analysis_service, file_service, plot_service, replay_service
from .services.run_store import RunStore, public_run


def _integer(value: Any, name: str, *, minimum: int, maximum: int) -> int:
    try:
        number = int(str(value))
    except (TypeError, ValueError) as exc:
        raise ValueError(f"{name} must be an integer.") from exc
    if number < minimum or number > maximum:
        raise ValueError(f"{name} must be between {minimum:,} and {maximum:,}.")
    return number


def _price_scale(value: Any) -> float:
    try:
        scale = float(value)
    except (TypeError, ValueError) as exc:
        raise ValueError("Price scale must be numeric.") from exc
    if not math.isfinite(scale) or scale <= 0 or scale > 1_000_000:
        raise ValueError("Price scale must be finite and between 0 and 1,000,000.")
    return scale


@lru_cache(maxsize=2)
def _load_replay_cached(
    path_string: str,
    file_size: int,
    modified_ns: int,
) -> dict[str, Any] | None:
    # Keyed by size + mtime_ns so a replaced replay document invalidates the cached version.
    try:
        value = json.loads(Path(path_string).read_text(encoding="utf-8"))
    except (OSError, ValueError):
        return None
    return value if isinstance(value, dict) else None


def _load_replay(run_dir: Path) -> dict[str, Any] | None:
    path = run_dir / "output" / "replay.json"
    try:
        stat = path.stat()
    except OSError:
        return None
    if not path.is_file():
        return None
    return _load_replay_cached(str(path.resolve()), stat.st_size, stat.st_mtime_ns)


def register_routes(app: Flask) -> None:
    store = RunStore(Path(app.config["RUNS_ROOT"]))
    app.extensions["dashboard_run_store"] = store
    worker_lock = threading.Lock()
    active_workers: set[str] = set()

    def worker(run_id: str) -> None:
        with worker_lock:
            active_workers.add(run_id)
        meta = store.get(run_id)
        run_dir = store.directory(run_id)
        if meta is None or run_dir is None:
            with worker_lock:
                active_workers.discard(run_id)
            return

        def progress(
            stage: str,
            percent: int | None,
            message: str,
            counters: dict[str, int] | None = None,
        ) -> None:
            updates = counters or {}
            store.update(
                run_id,
                status="running",
                stage=stage,
                progress=percent,
                message=message,
                **updates,
            )

        try:
            store.update(
                run_id,
                status="running",
                stage="starting",
                progress=2,
                message="Validating input and preparing local outputs.",
            )
            source_path = Path(meta["source_path"])
            summary, diagnostics = analysis_service.execute_run(
                root=Path(app.config["ROOT"]),
                run_dir=run_dir,
                source_path=source_path,
                metadata=meta,
                progress=progress,
            )
            store.update(
                run_id,
                status="completed",
                stage="completed",
                progress=100,
                message="Analysis completed.",
                error=None,
                summary=summary,
                completed_at=store.now(),
                diagnostics=diagnostics,
            )
        except Exception as exc:
            error_text = str(exc).strip() or type(exc).__name__
            trace_path = run_dir / "output" / "dashboard_error.log"
            trace_path.write_text(
                traceback.format_exc(),
                encoding="utf-8",
                errors="replace",
            )
            store.update(
                run_id,
                status="failed",
                stage="failed",
                progress=None,
                message=(
                    "Analysis failed. Read the error below and inspect "
                    "dashboard_error.log for local details."
                ),
                error=error_text[-5000:],
                completed_at=store.now(),
                internal_traceback=str(trace_path),
            )
        finally:
            with worker_lock:
                active_workers.discard(run_id)

    @app.get("/api/health")
    def health():
        return jsonify({
            "status": "ok",
            "mode": "local",
            "host_policy": "127.0.0.1 by default",
        })

    @app.get("/api/runs")
    def list_runs():
        try:
            limit = _integer(
                request.args.get("limit", "50"),
                "limit",
                minimum=1,
                maximum=200,
            )
        except ValueError as exc:
            return jsonify({"error": str(exc)}), 400
        runs = [public_run(item) for item in store.list(limit=limit)]
        return jsonify({"runs": runs})

    @app.post("/api/runs")
    def create_run():
        upload = request.files.get("file")
        if upload is None or not upload.filename:
            return jsonify({
                "error": "Choose a .txt, .gz, or observation/dataset .csv file first."
            }), 400

        filename = secure_filename(upload.filename) or "upload"
        suffix = Path(filename).suffix.lower()
        if suffix not in file_service.SUPPORTED_SUFFIXES:
            return jsonify({
                "error": "Unsupported file extension. Use .txt, .gz, or .csv."
            }), 400

        try:
            stock_locate = _integer(
                request.form.get("stock_locate", "14"),
                "Stock Locate",
                minimum=0,
                maximum=65535,
            )
            message_limit = _integer(
                request.form.get("message_limit", "100000"),
                "Message limit (0 means full file)",
                minimum=0,
                maximum=int(app.config["MAX_REPLAY_MESSAGES"]),
            )
            snapshot_interval = _integer(
                request.form.get("snapshot_interval", "20"),
                "Snapshot interval",
                minimum=1,
                maximum=100_000_000,
            )
            price_scale = _price_scale(
                request.form.get(
                    "price_scale",
                    str(app.config["PRICE_SCALE_DEFAULT"]),
                )
            )
        except ValueError as exc:
            return jsonify({"error": str(exc)}), 400

        max_snapshots = int(app.config["MAX_REPLAY_SNAPSHOTS"])

        # Snapshot retention is bounded by max_snapshots in the replay runner.
        # snapshot_interval counts successful book operations, not raw feed messages.
        # Do not estimate snapshot counts from message_limit here.

        run_meta = store.create({
            "original_filename": filename[:120],
            "input_mode": "upload",
            "input_kind": "csv" if suffix == ".csv" else "itch",
            "input_size_bytes": 0,
            "source_path": "",
            "run_dir": "",
            "output_dir": "",
            "config": {
                "stock_locate": stock_locate,
                "message_limit": message_limit,
                "snapshot_interval": snapshot_interval,
                "price_scale": price_scale,
                "max_replay_snapshots": max_snapshots,
                "levels_per_side": int(app.config["MAX_LEVELS_PER_SIDE"]),
                "max_csv_rows_to_scan": int(app.config["MAX_CSV_ROWS_TO_SCAN"]),
            },
        })

        run_id = run_meta["id"]
        run_dir = store.directory(run_id)
        assert run_dir is not None
        source_path = run_dir / "input" / f"source{suffix}"

        try:
            saved_path, display_name, size_bytes, inspected = file_service.save_stream(
                upload.stream,
                upload.filename,
                source_path,
                int(app.config["MAX_CONTENT_LENGTH"]),
                max_csv_rows=int(app.config["MAX_CSV_ROWS_TO_SCAN"]),
            )
            config = dict(run_meta["config"])
            if suffix == ".csv":
                # CSV source rows are already observations. Message replay parameters are not applied.
                config["csv_fields"] = inspected.get("fields", [])
                config["csv_row_count"] = inspected.get("row_count")

            store.update(
                run_id,
                original_filename=display_name,
                input_size_bytes=size_bytes,
                source_path=str(saved_path.resolve()),
                run_dir=str(run_dir.resolve()),
                output_dir=str((run_dir / "output").resolve()),
                config=config,
                validation={
                    key: value
                    for key, value in inspected.items()
                    if key != "fields" or suffix == ".csv"
                },
            )
        except (file_service.InputValidationError, OSError) as exc:
            shutil.rmtree(run_dir, ignore_errors=True)
            return jsonify({"error": str(exc)}), 400

        thread = threading.Thread(
            target=worker,
            args=(run_id,),
            daemon=True,
            name=f"lob-dashboard-{run_id}",
        )
        thread.start()
        return jsonify({"run": public_run(store.get(run_id) or run_meta)}), 202

    @app.get("/api/local-inputs")
    def list_local_inputs():
        """Discover project-local data files; large files stay on disk and are not uploaded."""
        root = Path(app.config["ROOT"]).resolve()
        try:
            files = file_service.list_local_inputs(root)
        except OSError as exc:
            return jsonify({
                "error": f"Could not scan project-local inputs: {exc}"
            }), 500
        return jsonify({
            "files": files,
            "roots": [
                name
                for name in ("data", "results")
                if (root / name).is_dir()
            ],
        })

    @app.post("/api/runs/local")
    def create_local_run():
        payload = request.get_json(silent=True) or {}
        root = Path(app.config["ROOT"]).resolve()

        try:
            source_path = file_service.resolve_local_input(
                root,
                payload.get("path", ""),
            )
            suffix = source_path.suffix.lower()
            stock_locate = _integer(
                payload.get("stock_locate", 14),
                "Stock Locate",
                minimum=0,
                maximum=65535,
            )
            message_limit = _integer(
                payload.get("message_limit", 100000),
                "Message limit (0 means full file)",
                minimum=0,
                maximum=int(app.config["MAX_REPLAY_MESSAGES"]),
            )
            snapshot_interval = _integer(
                payload.get("snapshot_interval", 20),
                "Snapshot interval",
                minimum=1,
                maximum=100_000_000,
            )
            price_scale = _price_scale(
                payload.get("price_scale", app.config["PRICE_SCALE_DEFAULT"])
            )
            inspected = file_service.validate_input(
                source_path,
                suffix,
                max_csv_rows=int(app.config["MAX_CSV_ROWS_TO_SCAN"]),
            )
        except (ValueError, file_service.InputValidationError) as exc:
            return jsonify({"error": str(exc)}), 400
        except OSError as exc:
            return jsonify({
                "error": f"Could not read the selected local file: {exc}"
            }), 400

        max_snapshots = int(app.config["MAX_REPLAY_SNAPSHOTS"])

        # Snapshot retention is bounded by max_snapshots in the replay runner.
        # snapshot_interval counts successful book operations, not raw feed messages.
        # Do not estimate snapshot counts from message_limit here.

        relative_path = source_path.relative_to(root).as_posix()
        stat = source_path.stat()
        input_kind = "csv" if suffix == ".csv" else "itch"

        config = {
            "stock_locate": stock_locate,
            "message_limit": message_limit,
            "snapshot_interval": snapshot_interval,
            "price_scale": price_scale,
            "max_replay_snapshots": max_snapshots,
            "levels_per_side": int(app.config["MAX_LEVELS_PER_SIDE"]),
            "max_csv_rows_to_scan": int(app.config["MAX_CSV_ROWS_TO_SCAN"]),
        }

        if input_kind == "csv":
            config["csv_fields"] = inspected.get("fields", [])
            config["csv_row_count"] = inspected.get("row_count")

        run_meta = store.create({
            "original_filename": relative_path[:240],
            "source_relative_path": relative_path,
            "input_mode": "local_file",
            "input_kind": input_kind,
            "input_size_bytes": stat.st_size,
            "source_path": str(source_path),
            "run_dir": "",
            "output_dir": "",
            "config": config,
            "validation": inspected,
        })

        run_id = run_meta["id"]
        run_dir = store.directory(run_id)
        assert run_dir is not None
        store.update(
            run_id,
            run_dir=str(run_dir.resolve()),
            output_dir=str((run_dir / "output").resolve()),
        )

        thread = threading.Thread(
            target=worker,
            args=(run_id,),
            daemon=True,
            name=f"lob-dashboard-{run_id}",
        )
        thread.start()
        return jsonify({"run": public_run(store.get(run_id) or run_meta)}), 202

    @app.get("/api/runs/<run_id>")
    def get_run(run_id: str):
        meta = store.get(run_id)
        if meta is None:
            return jsonify({"error": "Run not found."}), 404
        return jsonify({"run": public_run(meta)})

    @app.get("/api/runs/<run_id>/plots")
    def get_plots(run_id: str):
        run_dir = store.directory(run_id)
        if run_dir is None:
            return jsonify({"error": "Run not found."}), 404
        output = run_dir / "output"
        return jsonify({"plots": plot_service.list_plots(output)})

    @app.get("/api/runs/<run_id>/artifacts")
    def get_artifacts(run_id: str):
        run_dir = store.directory(run_id)
        if run_dir is None:
            return jsonify({"error": "Run not found."}), 404
        output = run_dir / "output"
        return jsonify({"artifacts": plot_service.list_json_artifacts(output)})

    @app.get("/api/runs/<run_id>/plots/<path:filename>")
    def serve_plot(run_id: str, filename: str):
        run_dir = store.directory(run_id)
        if run_dir is None:
            return jsonify({"error": "Run not found."}), 404
        output = (run_dir / "output").resolve()
        target = (output / filename).resolve()
        if (
            output not in target.parents
            or not target.is_file()
            or target.suffix.lower() not in {".png", ".jpg", ".jpeg", ".svg", ".webp"}
        ):
            return jsonify({"error": "Plot not found."}), 404
        return send_from_directory(output, filename, conditional=True)

    @app.get("/api/runs/<run_id>/artifacts/<path:filename>")
    def serve_artifact(run_id: str, filename: str):
        run_dir = store.directory(run_id)
        if run_dir is None:
            return jsonify({"error": "Run not found."}), 404
        output = (run_dir / "output").resolve()
        target = (output / filename).resolve()
        if (
            output not in target.parents
            or not target.is_file()
            or target.suffix.lower() not in {".json", ".csv", ".txt", ".log", ".md"}
        ):
            return jsonify({"error": "Artifact not found."}), 404
        return send_from_directory(
            output,
            filename,
            conditional=True,
            as_attachment=target.suffix.lower() in {".csv", ".txt", ".log"},
        )

    @app.get("/api/runs/<run_id>/snapshots")
    def snapshots(run_id: str):
        run_dir = store.directory(run_id)
        meta = store.get(run_id)
        if run_dir is None or meta is None:
            return jsonify({"error": "Run not found."}), 404
        if meta.get("status") != "completed":
            return jsonify({
                "error": "Replay data is not ready.",
                "status": meta.get("status"),
            }), 409

        document = _load_replay(run_dir)
        if document is None:
            return jsonify({"error": "This run has no replay sequence."}), 404

        total = len(document.get("snapshots", []))
        try:
            start = _integer(
                request.args.get("start", "0"),
                "start",
                minimum=0,
                maximum=max(total, 1),
            )
            limit = _integer(
                request.args.get("limit", "5000"),
                "limit",
                minimum=1,
                maximum=5000,
            )
        except ValueError as exc:
            return jsonify({"error": str(exc)}), 400

        snapshots_data = document.get("snapshots", [])[start:start + limit]
        return jsonify({
            "run_id": run_id,
            "total": total,
            "start": start,
            "depth_available": document.get("depth_available", False),
            "downsampled": document.get("downsampled", False),
            "snapshots": snapshots_data,
        })

    @app.get("/api/runs/<run_id>/snapshots/<int:index>")
    def snapshot_at_index(run_id: str, index: int):
        run_dir = store.directory(run_id)
        meta = store.get(run_id)
        if run_dir is None or meta is None:
            return jsonify({"error": "Run not found."}), 404
        if meta.get("status") != "completed":
            return jsonify({"error": "Replay data is not ready."}), 409

        document = _load_replay(run_dir)
        records = document.get("snapshots", []) if document else []
        if index < 0 or index >= len(records):
            return jsonify({"error": "Snapshot index out of range."}), 404
        return jsonify({"snapshot": records[index], "total": len(records)})

    @app.get("/api/runs/<run_id>/heatmap")
    def get_heatmap(run_id: str):
        run_dir = store.directory(run_id)
        meta = store.get(run_id)
        if run_dir is None or meta is None:
            return jsonify({"error": "Run not found."}), 404
        if meta.get("status") != "completed":
            return jsonify({"error": "Replay data is not ready."}), 409

        document = _load_replay(run_dir)
        if document is None:
            return jsonify({
                "available": False,
                "reason": "Replay data is not available.",
            })

        payload = replay_service.heatmap_payload(
            document,
            max_frames=int(app.config["MAX_HEATMAP_FRAMES"]),
            max_prices=int(app.config["MAX_HEATMAP_PRICES"]),
        )
        return jsonify(payload)
