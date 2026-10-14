from __future__ import annotations

import contextlib
import csv
import importlib.util
import inspect
import io
import json
import math
import subprocess
import statistics
import sys
from pathlib import Path
from typing import Any, Callable

from . import file_service, plot_service, replay_service


REPLAY_COUNTER_NAMES = (
    "messages_read",
    "applied_messages",
    "ignored_different_stock_locate",
    "non_applied_messages",
    "failed_applications",
    "parse_errors",
    "mapping_errors",
    "messages_accounted",
    "snapshots_captured",
)


def _get(obj: Any, *names: str) -> Any:
    for name in names:
        if isinstance(obj, dict) and name in obj:
            return obj[name]
        try:
            value = getattr(obj, name, None)
        except Exception:
            value = None
        if value is not None and not callable(value):
            return value
    return None


def _number(value: Any) -> float | None:
    try:
        number = float(value)
    except (TypeError, ValueError, OverflowError):
        return None
    return number if math.isfinite(number) else None


def _accepted_kwargs(callable_obj: Any, values: dict[str, Any]) -> dict[str, Any]:
    try:
        params = inspect.signature(callable_obj).parameters
    except (TypeError, ValueError):
        return {}

    if any(item.kind is inspect.Parameter.VAR_KEYWORD for item in params.values()):
        return values

    aliases = {
        "file_path": values.get("file_path"),
        "path": values.get("file_path"),
        "itch_path": values.get("file_path"),
        "input_path": values.get("file_path"),
        "source_path": values.get("file_path"),
        "source_file": values.get("file_path"),
        "input_file": values.get("file_path"),
        "itch_file": values.get("file_path"),
        "file": values.get("file_path"),
        "data_path": values.get("file_path"),
        "stock_locate": values.get("stock_locate"),
        "max_messages": values.get("max_messages"),
        "snapshot_interval": values.get("snapshot_interval"),
        "output_dir": values.get("output_dir"),
    }
    return {key: aliases[key] for key in params if key in aliases}


def _extract_snapshots(result: Any) -> list[Any]:
    if result is None:
        return []

    if isinstance(result, (list, tuple)):
        # Some workflow functions return (summary, snapshots).
        for item in result:
            candidate = (
                _extract_snapshots(item)
                if isinstance(item, (dict, list, tuple))
                else []
            )
            if candidate and isinstance(item, (dict, list, tuple)):
                return candidate
        return list(result)

    for key in ("snapshots", "observations", "book_snapshots", "records"):
        value = _get(result, key)
        if value is not None:
            try:
                return list(value)
            except TypeError:
                pass

    return []


def _load_workflow(root: Path):
    script = root / "examples" / "itch_research_workflow.py"
    if not script.is_file():
        raise FileNotFoundError(
            "The existing workflow entry point "
            "examples/itch_research_workflow.py is missing. "
            "Move the complete implementation from "
            "tests/python/test_itch_research_workflow.py as specified in "
            "MIGRATE_EXISTING_WORKFLOW.md; the dashboard deliberately does "
            "not execute a test file as an application."
        )

    for path in (root, root / "python", root / "examples"):
        if str(path) not in sys.path:
            sys.path.insert(0, str(path))

    spec = importlib.util.spec_from_file_location(
        "_dashboard_itch_research_workflow", script
    )
    if spec is None or spec.loader is None:
        raise RuntimeError(f"Could not load existing research workflow at {script}.")

    module = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(module)
    return module, script


def _run_existing_workflow(
    *,
    root: Path,
    source_path: Path,
    output_dir: Path,
    config: dict[str, Any],
    log_path: Path,
) -> Any:
    module, script = _load_workflow(root)
    max_messages = (
        None
        if int(config["message_limit"]) == 0
        else int(config["message_limit"])
    )
    function = getattr(module, "run_workflow", None)
    stdout = io.StringIO()

    if callable(function):
        values = {
            "file_path": source_path,
            "stock_locate": int(config["stock_locate"]),
            "max_messages": max_messages,
            "snapshot_interval": int(config["snapshot_interval"]),
            "output_dir": output_dir,
        }
        with contextlib.redirect_stdout(stdout), contextlib.redirect_stderr(stdout):
            result = function(**_accepted_kwargs(function, values))

        log_path.write_text(
            stdout.getvalue(), encoding="utf-8", errors="replace"
        )
        return result

    args = [
        sys.executable,
        str(script),
        "--file",
        str(source_path),
        "--stock-locate",
        str(config["stock_locate"]),
        "--max-messages",
        "0" if max_messages is None else str(max_messages),
        "--snapshot-interval",
        str(config["snapshot_interval"]),
        "--output-dir",
        str(output_dir),
    ]

    with log_path.open("w", encoding="utf-8", errors="replace") as log:
        completed = subprocess.run(
            args,
            cwd=root,
            stdout=log,
            stderr=subprocess.STDOUT,
            check=False,
        )

    if completed.returncode != 0:
        tail = log_path.read_text(
            encoding="utf-8", errors="replace"
        )[-4000:]
        raise RuntimeError(
            "The existing ITCH research workflow exited with code "
            f"{completed.returncode}.\n{tail}"
        )

    return None


def _find_observation_csv(output_dir: Path) -> Path | None:
    candidates: list[Path] = []

    for path in Path(output_dir).rglob("*.csv"):
        if not path.is_file():
            continue

        try:
            with path.open("r", newline="", encoding="utf-8-sig") as file:
                reader = csv.DictReader(file)
                fields = {
                    name.strip().lower()
                    for name in (reader.fieldnames or [])
                    if name
                }

            if any(
                required <= fields
                for required in (
                    {"midpoint", "spread", "bid_depth", "ask_depth"},
                    {"best_bid", "best_ask", "bid_depth", "ask_depth"},
                )
            ):
                candidates.append(path)

        except (OSError, UnicodeDecodeError, csv.Error):
            continue

    if not candidates:
        return None

    candidates.sort(key=lambda path: path.stat().st_size, reverse=True)
    return candidates[0]


def _summary_from_document(document: dict[str, Any]) -> dict[str, Any]:
    snapshots = document.get("snapshots", [])

    def values(key: str) -> list[float]:
        return [
            float(item[key])
            for item in snapshots
            if _number(item.get(key)) is not None
        ]

    mids = values("midpoint")
    spreads = values("spread")
    imbalances = values("imbalance")

    return {
        "snapshot_count": len(snapshots),
        "snapshot_count_raw": document.get(
            "snapshot_count_raw", len(snapshots)
        ),
        "depth_available": bool(document.get("depth_available")),
        "best_bid": snapshots[-1].get("best_bid") if snapshots else None,
        "best_ask": snapshots[-1].get("best_ask") if snapshots else None,
        "midpoint_first": mids[0] if mids else None,
        "midpoint_last": mids[-1] if mids else None,
        "spread_median": statistics.median(spreads) if spreads else None,
        "imbalance_mean": (
            sum(imbalances) / len(imbalances) if imbalances else None
        ),
    }


def _diagnostics(
    output_dir: Path,
    metadata: dict[str, Any],
    document: dict[str, Any],
) -> dict[str, Any]:
    result: dict[str, Any] = {
        "run_id": metadata["id"],
        "created_at": metadata["created_at"],
        "input": {
            "filename": metadata.get("original_filename"),
            "kind": metadata.get("input_kind"),
            "bytes": metadata.get("input_size_bytes"),
        },
        "configuration": metadata.get("config", {}),
        "replay": _summary_from_document(document),
        "validation": {
            "snapshot_count_positive": bool(document.get("snapshots")),
            "price_fields_are_finite_or_unavailable": (
                bool(document.get("snapshots"))
                and all(
                    value is None or _number(value) is not None
                    for item in document.get("snapshots", [])
                    for value in (
                        item.get("best_bid"),
                        item.get("best_ask"),
                        item.get("midpoint"),
                        item.get("spread"),
                    )
                )
            ),
            "per_level_data_available": bool(document.get("depth_available")),
            "downsampled_for_browser": bool(document.get("downsampled")),
        },
    }

    path = output_dir / "dashboard_diagnostics.json"
    path.write_text(
        json.dumps(result, ensure_ascii=False, allow_nan=False, indent=2),
        encoding="utf-8",
    )
    return result


def _coerce_document_from_workflow(
    workflow_result: Any,
    output_dir: Path,
    config: dict[str, Any],
) -> dict[str, Any] | None:
    snapshots = _extract_snapshots(workflow_result)

    if snapshots:
        return replay_service.document_from_objects(
            snapshots,
            price_scale=float(config["price_scale"]),
            max_snapshots=int(config["max_replay_snapshots"]),
            levels_per_side=int(config["levels_per_side"]),
            source_kind="itch",
        )

    # Some workflow versions persist snapshots as JSON.
    for name in (
        "snapshots.json",
        "book_states.json",
        "observations.json",
        "replay.json",
    ):
        path = output_dir / name

        if not path.is_file() or path.stat().st_size > 250 * 1024 * 1024:
            continue

        try:
            data = json.loads(path.read_text(encoding="utf-8"))
        except (OSError, json.JSONDecodeError):
            continue

        if isinstance(data, dict):
            data = data.get(
                "snapshots",
                data.get("observations", data.get("records", [])),
            )

        if isinstance(data, list) and data:
            return replay_service.document_from_objects(
                data,
                price_scale=float(config["price_scale"]),
                max_snapshots=int(config["max_replay_snapshots"]),
                levels_per_side=int(config["levels_per_side"]),
                source_kind="itch",
            )

    return None


def _counter_details(
    messages_read: int,
    snapshots_captured: int,
    counters: dict[str, int] | None = None,
) -> dict[str, int]:
    details = {
        name: 0
        for name in REPLAY_COUNTER_NAMES
    }
    details["messages_read"] = messages_read
    details["snapshots_captured"] = snapshots_captured

    if counters:
        for name in REPLAY_COUNTER_NAMES:
            if name in counters:
                details[name] = int(counters[name])

    # The runner's authoritative read count and retained snapshot count win.
    details["messages_read"] = messages_read
    details["snapshots_captured"] = snapshots_captured
    return details


def run_itch_analysis(
    *,
    root: Path,
    run_dir: Path,
    source_path: Path,
    metadata: dict[str, Any],
    progress: Callable[..., None],
) -> tuple[dict[str, Any], dict[str, Any]]:
    config = metadata["config"]
    output_dir = run_dir / "output"
    workflow_dir = output_dir / "workflow"
    workflow_dir.mkdir(parents=True, exist_ok=True)

    # Do not run the legacy workflow before replay. It can block the dashboard
    # without exposing raw-message progress. The bounded C++ reader/replay loop
    # is the source of the live replay counters and snapshot sequence.
    workflow_warning = (
        "The legacy ITCH research workflow was skipped for this dashboard run. "
        "The dashboard uses bounded C++ replay directly so that message "
        "progress and outcome counters can be reported live."
    )
    (output_dir / "workflow.log").write_text(
        workflow_warning + "\n",
        encoding="utf-8",
    )

    progress(
        "replay_states",
        8,
        "Starting bounded C++ ITCH replay.",
        _counter_details(0, 0),
    )

    for path in (root, root / "python", root / "examples"):
        if str(path) not in sys.path:
            sys.path.insert(0, str(path))

    from .itch_runner import run_bounded_itch_replay

    max_messages = (
        None
        if int(config["message_limit"]) == 0
        else int(config["message_limit"])
    )

    def report_replay_progress(
        messages_read: int,
        total: int | None,
        snapshots_captured: int,
        percent: int,
        counters: dict[str, int] | None = None,
    ) -> None:
        limit_label = (
            "full file"
            if total is None
            else f"{total:,} message limit"
        )
        details = _counter_details(
            messages_read,
            snapshots_captured,
            counters,
        )

        progress(
            "replay_states",
            percent,
            (
                f"Read {messages_read:,} / {limit_label}; "
                f"applied {details['applied_messages']:,}; "
                f"captured {snapshots_captured:,} snapshots."
            ),
            details,
        )

    replay_result = run_bounded_itch_replay(
        source_path,
        stock_locate=int(config["stock_locate"]),
        max_messages=max_messages,
        snapshot_interval=int(config["snapshot_interval"]),
        levels_per_side=int(config["levels_per_side"]),
        max_snapshots=int(config["max_replay_snapshots"]),
        progress_callback=report_replay_progress,
    )

    if not isinstance(replay_result, tuple) or len(replay_result) != 3:
        raise RuntimeError(
            "run_bounded_itch_replay() must return "
            "(snapshots, raw_snapshot_count, messages_read)."
        )

    snapshots, raw_snapshot_count, messages_read_count = replay_result

    if not snapshots:
        raise RuntimeError(
            "The bounded C++ replay returned no snapshots. "
            "Check the selected Stock Locate, input file, and snapshot interval."
        )

    document = replay_service.document_from_objects(
        snapshots,
        price_scale=float(config["price_scale"]),
        max_snapshots=int(config["max_replay_snapshots"]),
        levels_per_side=int(config["levels_per_side"]),
        source_kind="itch",
        raw_snapshot_count=raw_snapshot_count,
    )

    replay_path = replay_service.write_replay_document(
        document,
        output_dir / "replay.json",
    )

    progress(
        "plots",
        90,
        "Generating dashboard plots from the replay observations.",
        {
            "messages_read": messages_read_count,
            "snapshots_captured": raw_snapshot_count,
        },
    )

    plot_service.generate_dashboard_plots(document, output_dir)

    summary = _summary_from_document(document)
    summary.update({
        "input_kind": "itch",
        "workflow_used": False,
        "workflow_warning": workflow_warning,
        "workflow_plot_count": 0,
        "dashboard_plot_count": len(
            plot_service.list_plots(output_dir / "dashboard_generated")
        ),
        "diagnostics_count": len(
            plot_service.list_json_artifacts(output_dir)
        ),
        "replay_file": replay_path.name,
        "depth_available": document["depth_available"],
        "messages_read": messages_read_count,
        "snapshots_captured": raw_snapshot_count,
    })

    diagnostics = _diagnostics(output_dir, metadata, document)
    diagnostics["workflow"] = {
        "used": False,
        "warning": workflow_warning,
        "workflow_plot_count": 0,
        "dashboard_plot_count": summary["dashboard_plot_count"],
    }

    diagnostics["replay_counters"] = {
        "messages_read": messages_read_count,
        "snapshots_captured": raw_snapshot_count,
    }

    (output_dir / "dashboard_diagnostics.json").write_text(
        json.dumps(
            diagnostics,
            ensure_ascii=False,
            allow_nan=False,
            indent=2,
        ),
        encoding="utf-8",
    )

    progress(
        "complete",
        100,
        "ITCH replay and dashboard analysis completed.",
        {
            "messages_read": messages_read_count,
            "snapshots_captured": raw_snapshot_count,
        },
    )

    return summary, diagnostics


def _csv_summary(
    path: Path,
    validation: dict[str, Any],
    document: dict[str, Any],
) -> dict[str, Any]:
    summary = _summary_from_document(document)
    summary.update({
        "input_kind": "csv",
        "rows_validated": validation.get("row_count"),
        "field_count": len(validation.get("fields", [])),
        "fields": validation.get("fields", []),
        "workflow_plot_count": 0,
        "depth_available": bool(document.get("depth_available")),
    })
    return summary


def run_csv_analysis(
    *,
    run_dir: Path,
    source_path: Path,
    metadata: dict[str, Any],
    progress: Callable[..., None],
) -> tuple[dict[str, Any], dict[str, Any]]:
    config = metadata["config"]
    output_dir = run_dir / "output"

    progress(
        "csv_analysis",
        15,
        "Validating the dataset and extracting a bounded replay sequence.",
    )

    validation = metadata.get("validation")
    if (
        not isinstance(validation, dict)
        or validation.get("kind") != "csv"
        or not validation.get("row_count")
    ):
        validation = file_service.validate_observation_csv(
            source_path,
            max_rows=int(config["max_csv_rows_to_scan"]),
        )

    document = replay_service.document_from_csv(
        source_path,
        row_count=int(validation["row_count"]),
        price_scale=float(config["price_scale"]),
        max_snapshots=int(config["max_replay_snapshots"]),
        levels_per_side=int(config["levels_per_side"]),
    )

    replay_service.write_replay_document(
        document,
        output_dir / "replay.json",
    )

    progress(
        "plots",
        85,
        "Generating plots from the uploaded observations.",
    )

    generated = plot_service.generate_dashboard_plots(document, output_dir)
    if not generated:
        raise RuntimeError(
            "No usable numeric observation series were found for plotting."
        )

    summary = _csv_summary(source_path, validation, document)
    diagnostics = {
        "run_id": metadata["id"],
        "created_at": metadata["created_at"],
        "input": {
            "filename": metadata.get("original_filename"),
            "kind": "csv",
            "bytes": metadata.get("input_size_bytes"),
        },
        "configuration": config,
        "csv_validation": validation,
        "replay": _summary_from_document(document),
        "validation_checks": {
            "rows_present": validation["row_count"] > 0,
            "schema_supported": True,
            "per_level_data_available": bool(document.get("depth_available")),
            "dashboard_sequence_downsampled": bool(document.get("downsampled")),
        },
    }

    (output_dir / "dashboard_diagnostics.json").write_text(
        json.dumps(
            diagnostics,
            ensure_ascii=False,
            allow_nan=False,
            indent=2,
        ),
        encoding="utf-8",
    )

    return summary, diagnostics


def execute_run(
    *,
    root: Path,
    run_dir: Path,
    source_path: Path,
    metadata: dict[str, Any],
    progress: Callable[..., None],
) -> tuple[dict[str, Any], dict[str, Any]]:
    if metadata.get("input_kind") == "csv":
        return run_csv_analysis(
            run_dir=run_dir,
            source_path=source_path,
            metadata=metadata,
            progress=progress,
        )

    return run_itch_analysis(
        root=root,
        run_dir=run_dir,
        source_path=source_path,
        metadata=metadata,
        progress=progress,
    )
