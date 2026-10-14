# Benchmarking

## 1. Measure the right thing

This repository has more than one performance workload. Their outputs are not interchangeable.

| Workload | What is timed | What it does **not** establish |
|---|---|---|
| C++ synthetic benchmark | A defined synthetic matching/simulation workload | Historical replay speed or dashboard latency |
| Historical ITCH replay benchmark | Reading, parsing, mapping/filtering, and applying messages to reconstruct a book | Matching throughput for hypothetical orders, market realism, or live exchange latency |
| Python feature benchmark | Calculating microstructure features on synthetic observation rows in pure Python versus NumPy-vectorized code | C++ matching speed, ITCH throughput, or end-to-end dashboard speed |

Always state the workload, input size, build mode/toolchain, machine, repetitions, and whether setup, parsing, conversion, plotting, and output I/O are included. Do not compare values that time different scopes.

## 2. Historical ITCH baseline recorded in project notes

The previous end-to-end replay measurement recorded approximately:

| Raw messages processed | Elapsed time |
|---:|---:|
| 1,000,000 | 1.350 s |
| 10,000,000 | 13.921 s |
| 100,000,000 | 137.520 s |
| 302,347,067 (full source file) | 400.130 s |

The full-file result corresponds to approximately **755,622 raw messages/second** in that measured environment. This is a retained historical baseline, not a promise for another machine or the current compiled build. The raw feed size was about 302 million messages—not 300 billion synthetic events. Re-run the benchmark on the release checkout and capture the compiler/runtime/environment before presenting this number as a current result.

## 3. Python feature benchmark

`benchmarks/benchmark.py` compares `calculate_features_naive` and `calculate_features_optimized` on deterministic synthetic order-book observations. Each observation includes `bid`, `ask`, `bid_depth`, and `ask_depth`. The benchmark validates the input and checks numerical equivalence before timing; it uses warm-ups and repeated runs, reports median/min/max timing and snapshots per second, and can write environment/configuration/result details to JSON.

Example from the script's CLI:

```bash
python benchmarks/benchmark.py \
  --rows 100000 \
  --repeat 5 \
  --warmup 1 \
  --output benchmarks/results/python/python_feature_benchmark.json
```

This benchmark includes feature calculation and conversion of vectorized outputs back to Python lists. Its measured speedup is workload- and environment-dependent. It should not be described as an improvement to the C++ matching engine.

## 4. C++ benchmark entry point

From the MSYS2 UCRT64 terminal, the repository's script is:

```bash
bash scripts/run_benchmark.sh
```

The script expects `MSYSTEM=UCRT64`, builds the benchmark executable if needed, and then forwards any supplied arguments to `build/benchmark_main.exe`. Check the current native benchmark program's accepted arguments before passing options.

## 5. Reporting rules

- Report measured values, not estimates dressed up as measurements.
- Include the exact data file/message count and whether messages were filtered by Stock Locate.
- Keep `messages_read`, successfully applied book updates, and retained snapshots separate.
- Do not treat snapshot throughput as message throughput.
- Do not describe this project as “zero-copy” solely because it uses pybind11. State a zero-copy property only for a specific verified path and with supporting code/measurements.
- A benchmark is not evidence of trading profitability, exchange fidelity outside the implemented message subset, or production HFT readiness.

## 6. Output hygiene

The Python feature benchmark defaults to a JSON file under `benchmarks/results/python/`. Keep generated results out of source control unless the result is intentionally retained, labelled with the environment, and useful as a reproducible reference. Do not overwrite a reported baseline without recording the conditions that produced the new result.
