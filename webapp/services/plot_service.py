from __future__ import annotations

import math
import re
from pathlib import Path
from typing import Any


def _finite(values: list[Any]) -> list[float | None]:
    result: list[float | None] = []
    for value in values:
        try:
            number = float(value)
            result.append(number if math.isfinite(number) else None)
        except (TypeError, ValueError, OverflowError):
            result.append(None)
    return result


def _series(document: dict[str, Any], key: str) -> list[float | None]:
    return _finite([item.get(key) for item in document.get("snapshots", [])])


def generate_dashboard_plots(document: dict[str, Any], destination: Path) -> list[str]:
    """Create useful fallback plots from actual observations, without replacing existing workflow figures."""
    try:
        import matplotlib
        matplotlib.use("Agg")
        import matplotlib.pyplot as plt
    except ImportError:
        return []

    root = Path(destination) / "dashboard_generated"
    root.mkdir(parents=True, exist_ok=True)
    snapshots = document.get("snapshots", [])
    if not snapshots:
        return []
    x = [item.get("message_index", item.get("index", i)) for i, item in enumerate(snapshots)]
    plots: list[str] = []
    specs = [
        ("midpoint", "Midpoint price", "Price", "midpoint"),
        ("spread", "Quoted spread", "Price units", "spread"),
        ("relative_spread", "Relative spread", "Relative spread", "relative_spread"),
        ("bid_depth", "Bid depth", "Displayed quantity", "bid_depth"),
        ("ask_depth", "Ask depth", "Displayed quantity", "ask_depth"),
        ("imbalance", "Depth imbalance", "Imbalance", "imbalance"),
    ]
    for filename, title, ylabel, field in specs:
        y = _series(document, field)
        if not any(value is not None for value in y):
            continue
        fig, ax = plt.subplots(figsize=(10.4, 3.9), constrained_layout=True)
        ax.plot(x, [float("nan") if value is None else value for value in y], linewidth=1.15)
        ax.set_title(title, loc="left", fontsize=12, fontweight="semibold")
        ax.set_xlabel("Message index")
        ax.set_ylabel(ylabel)
        ax.grid(True, alpha=0.22, linewidth=0.6)
        ax.spines[["top", "right"]].set_visible(False)
        path = root / f"{filename}.png"
        fig.savefig(path, dpi=145, facecolor="white")
        plt.close(fig)
        plots.append(path.relative_to(destination).as_posix())

    mids = _series(document, "midpoint")
    returns: list[float | None] = [None]
    for previous, current in zip(mids, mids[1:]):
        if previous not in (None, 0) and current is not None:
            returns.append(current / previous - 1.0)
        else:
            returns.append(None)
    if len(returns) == len(x) and any(value is not None for value in returns):
        fig, ax = plt.subplots(figsize=(10.4, 3.9), constrained_layout=True)
        ax.plot(x, [float("nan") if value is None else value for value in returns], linewidth=0.9)
        ax.set_title("One-snapshot midpoint return", loc="left", fontsize=12, fontweight="semibold")
        ax.set_xlabel("Message index")
        ax.set_ylabel("Return")
        ax.grid(True, alpha=0.22, linewidth=0.6)
        ax.spines[["top", "right"]].set_visible(False)
        path = root / "midpoint_return.png"
        fig.savefig(path, dpi=145, facecolor="white")
        plt.close(fig)
        plots.append(path.relative_to(destination).as_posix())
    return plots


def _title(filename: str) -> str:
    stem = Path(filename).stem
    stem = re.sub(r"[_-]+", " ", stem)
    replacements = {
        "mid": "Midpoint", "midpoint": "Midpoint", "spread": "Spread",
        "depth": "Depth", "imbalance": "Imbalance", "return": "Returns",
        "returns": "Returns", "heatmap": "Heatmap", "diagnostic": "Diagnostics",
    }
    words = [replacements.get(word.lower(), word.title()) for word in stem.split()]
    return " ".join(words)


def list_plots(output_dir: Path) -> list[dict[str, str]]:
    root = Path(output_dir).resolve()
    if not root.is_dir():
        return []
    candidates: list[dict[str, str]] = []
    supported = {".png", ".jpg", ".jpeg", ".svg", ".webp"}
    for path in root.rglob("*"):
        if not path.is_file() or path.suffix.lower() not in supported:
            continue
        try:
            resolved = path.resolve()
            relative = resolved.relative_to(root).as_posix()
        except (ValueError, OSError):
            continue
        category = "diagnostics" if any(token in relative.lower() for token in ("diagnostic", "validation", "quality")) else "analysis"
        lowered = relative.lower()
        for token, name in (("price", "Price"), ("mid", "Price"), ("spread", "Liquidity"), ("depth", "Liquidity"), ("imbalance", "Liquidity"), ("return", "Returns"), ("feature", "Features"), ("trade", "Trades"), ("inventory", "Inventory"), ("pnl", "Inventory")):
            if token in lowered:
                category = name
                break
        candidates.append({"path": relative, "title": _title(path.name), "category": category, "format": path.suffix[1:].lower()})
    candidates.sort(key=lambda item: (item["category"].lower(), item["title"].lower(), item["path"]))
    return candidates


def list_json_artifacts(output_dir: Path) -> list[dict[str, str]]:
    root = Path(output_dir).resolve()
    artifacts: list[dict[str, str]] = []
    for path in root.rglob("*.json"):
        if not path.is_file() or path.stat().st_size > 2 * 1024 * 1024:
            continue
        try:
            rel = path.resolve().relative_to(root).as_posix()
        except (ValueError, OSError):
            continue
        artifacts.append({"path": rel, "title": _title(path.name)})
    artifacts.sort(key=lambda item: item["path"])
    return artifacts
