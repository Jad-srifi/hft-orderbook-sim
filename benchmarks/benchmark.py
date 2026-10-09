from __future__ import annotations

import argparse
import json
import math
import platform
import random
import statistics
import sys
import time
from datetime import datetime, timezone
from pathlib import Path
from typing import Callable

from benchmark_naive import (
    FEATURE_NAMES,
    FeatureOutput,
    Observation,
    calculate_features_naive,
)
from benchmark_optimized import calculate_features_optimized, np

BENCHMARKS_DIR = Path(__file__).resolve().parent
DEFAULT_OUTPUT = (
    BENCHMARKS_DIR / "results" / "python" / "python_feature_benchmark.json"
)


def generate_observations(
    count: int,
    seed: int,
) -> list[Observation]:
    if count < 1:
        raise ValueError("count must be at least 1")

    rng = random.Random(seed)
    midpoint = 100.0
    observations: list[Observation] = []

    for _ in range(count):
        midpoint = max(
            1.0,
            midpoint + rng.gauss(0.0, 0.005),
        )

        spread = rng.choice((0.01, 0.02, 0.03, 0.05))
        bid = midpoint - spread / 2.0
        ask = midpoint + spread / 2.0
        bid_depth = float(rng.randint(0, 10_000))
        ask_depth = float(rng.randint(0, 10_000))

        observations.append(
            (bid, ask, bid_depth, ask_depth)
        )

    return observations


def validate_observations(
    observations: list[Observation],
) -> None:
    for index, observation in enumerate(observations):
        if len(observation) != 4:
            raise ValueError(
                f"Observation {index} must contain exactly four values."
            )

        bid, ask, bid_depth, ask_depth = map(float, observation)
        values = (bid, ask, bid_depth, ask_depth)

        if not all(math.isfinite(value) for value in values):
            raise ValueError(
                f"Observation {index} contains a non-finite value."
            )

        if bid <= 0.0 or ask <= bid:
            raise ValueError(
                f"Observation {index} must satisfy 0 < bid < ask."
            )

        if bid_depth < 0.0 or ask_depth < 0.0:
            raise ValueError(
                f"Observation {index} has negative depth."
            )


def assert_equivalent(
    naive: FeatureOutput,
    optimized: FeatureOutput,
) -> None:
    if set(naive) != set(optimized):
        raise AssertionError(
            "The implementations returned different feature names."
        )

    for name in FEATURE_NAMES:
        left = naive[name]
        right = optimized[name]

        if len(left) != len(right):
            raise AssertionError(
                f"Feature {name!r} has different output lengths."
            )

        for index, (a, b) in enumerate(zip(left, right)):
            if not math.isclose(
                a,
                b,
                rel_tol=1e-11,
                abs_tol=1e-11,
            ):
                raise AssertionError(
                    f"Feature mismatch in {name!r} at index "
                    f"{index}: {a!r} != {b!r}"
                )


def measure(
    function: Callable[
        [list[Observation]],
        FeatureOutput,
    ],
    observations: list[Observation],
    repeat: int,
    warmup: int,
) -> tuple[dict[str, float | list[float]], FeatureOutput]:
    result: FeatureOutput = {}

    for _ in range(warmup):
        result = function(observations)

    durations: list[float] = []

    for _ in range(repeat):
        start = time.perf_counter()
        result = function(observations)
        durations.append(time.perf_counter() - start)

    median_seconds = statistics.median(durations)

    summary: dict[str, float | list[float]] = {
        "runs_seconds": durations,
        "min_seconds": min(durations),
        "median_seconds": median_seconds,
        "max_seconds": max(durations),
        "snapshots_per_second": (
            len(observations) / median_seconds
        ),
    }

    return summary, result


def run_benchmark(
    rows: int = 100_000,
    repeat: int = 5,
    warmup: int = 1,
    seed: int = 20261009,
    output_path: Path | None = DEFAULT_OUTPUT,
) -> dict[str, object]:
    if rows < 1:
        raise ValueError("--rows must be at least 1")

    if repeat < 1:
        raise ValueError("--repeat must be at least 1")

    if warmup < 0:
        raise ValueError("--warmup cannot be negative")

    observations = generate_observations(rows, seed)
    validate_observations(observations)

    naive_function = calculate_features_naive
    optimized_function = calculate_features_optimized

    naive_check = naive_function(observations)
    optimized_check = optimized_function(observations)

    assert_equivalent(naive_check, optimized_check)

    naive_summary, naive_output = measure(
        naive_function,
        observations,
        repeat,
        warmup,
    )

    optimized_summary, optimized_output = measure(
        optimized_function,
        observations,
        repeat,
        warmup,
    )

    assert_equivalent(naive_output, optimized_output)

    naive_median = float(naive_summary["median_seconds"])
    optimized_median = float(optimized_summary["median_seconds"])

    report: dict[str, object] = {
        "benchmark": "Python microstructure feature generation",
        "created_at_utc": datetime.now(timezone.utc).isoformat(),
        "input": {
            "kind": (
                "deterministically generated synthetic "
                "order-book snapshots"
            ),
            "snapshot_count": rows,
            "seed": seed,
            "snapshot_schema": [
                "bid",
                "ask",
                "bid_depth",
                "ask_depth",
            ],
        },
        "configuration": {
            "repeat": repeat,
            "warmup": warmup,
            "timed_scope": (
                "feature calculation plus conversion of "
                "vectorized results to Python lists"
            ),
        },
        "environment": {
            "python": sys.version,
            "platform": platform.platform(),
            "numpy": np.__version__,
        },
        "correctness": {
            "outputs_equivalent": True,
            "features": list(FEATURE_NAMES),
        },
        "results": {
            "naive": naive_summary,
            "optimized": optimized_summary,
            "speedup_naive_over_optimized": (
                naive_median / optimized_median
            ),
        },
    }

    if output_path is not None:
        output_path = output_path.expanduser().resolve()
        output_path.parent.mkdir(parents=True, exist_ok=True)
        report["output_file"] = str(output_path)

        output_path.write_text(
            json.dumps(report, indent=2),
            encoding="utf-8",
        )

    return report


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(
        description=(
            "Compare pure-Python and NumPy-vectorized "
            "microstructure feature generation."
        )
    )

    parser.add_argument("--rows", type=int, default=100_000)
    parser.add_argument("--repeat", type=int, default=5)
    parser.add_argument("--warmup", type=int, default=1)
    parser.add_argument("--seed", type=int, default=20261009)
    parser.add_argument("--output", type=Path, default=DEFAULT_OUTPUT)
    parser.add_argument("--no-save", action="store_true")

    return parser.parse_args()


def main() -> int:
    args = parse_args()

    try:
        report = run_benchmark(
            rows=args.rows,
            repeat=args.repeat,
            warmup=args.warmup,
            seed=args.seed,
            output_path=None if args.no_save else args.output,
        )
    except (ValueError, AssertionError, OSError) as exc:
        print(f"Benchmark failed: {exc}", file=sys.stderr)
        return 1

    results = report["results"]
    assert isinstance(results, dict)

    naive = results["naive"]
    optimized = results["optimized"]
    assert isinstance(naive, dict)
    assert isinstance(optimized, dict)

    snapshot_count = report["input"]["snapshot_count"]
    outputs_match = report["correctness"]["outputs_equivalent"]

    print(f"Snapshots: {snapshot_count}")
    print(f"Features equivalent: {outputs_match}")

    print(
        "Naive median:     "
        f"{naive['median_seconds']:.6f} s "
        f"({naive['snapshots_per_second']:,.0f} snapshots/s)"
    )

    print(
        "Optimized median: "
        f"{optimized['median_seconds']:.6f} s "
        f"({optimized['snapshots_per_second']:,.0f} snapshots/s)"
    )

    print(
        "Speedup: "
        f"{results['speedup_naive_over_optimized']:.2f}x"
    )

    if "output_file" in report:
        print(f"JSON results: {report['output_file']}")

    return 0


if __name__ == "__main__":
    raise SystemExit(main())