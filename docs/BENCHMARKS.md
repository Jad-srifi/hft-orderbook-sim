# Chapter 10 — Performance Benchmark Report

## I. Executive Summary

Chapter 10 establishes the first quantitative performance baseline for the completed C++ market-state, simulation, accounting, and historical ITCH-replay stack.

The session produced four clear performance regimes:

1. **Core OrderBook / Simulator workloads:** several state-mutating workloads become strongly more expensive as book size grows. The clearest measured jump is `OrderBook::add`, from **394.508 ms at 10k** to **38.752 s at 100k**.

2. **Market analysis workloads:** Chapter 6 Metrics and Chapter 7 Execution Analysis remain approximately linear between the measured **1M and 5M** tiers. Metrics reaches **13.43 ns/op** at 5M, while Execution reaches **271.47 ns/op**.

3. **Inventory accounting:** scales close to linearly through **1 billion trades**, completing the largest workload in **14.152 s** at about **70.66M trades/s**.

4. **Historical ITCH replay:** scales approximately linearly through the available **302,347,067-message** stream, completing the maximum run in **400.130 s (6 min 40.130 s)** at about **755.62k messages/s**.

The original 100k core-engine run was stopped after the first four workloads because the remaining repeated measurements would have required an impractical amount of time under the original `2 warmups + 10 measured runs` configuration.

Only measured values are presented as results. Explicitly marked estimates are projections used for planning the next experiment.

---

# II. Benchmark Session at a Glance

| Test                         | Chapters |       Highest measured tier |                                           Result | Status             |
| ---------------------------- | -------- | --------------------------: | -----------------------------------------------: | ------------------ |
| Test 1 — Core Market Engine  | Ch1–5    |                        100k |                  `OrderBook::add` = **38.752 s** | Partially measured |
| Test 2 — Market Analysis     | Ch6–7    |                          5M | Metrics = **67.161 ms**; Execution = **1.357 s** | 1M + 5M measured   |
| Test 3 — Accounting & Replay | Ch8–9    | Ch8 = 1B; Ch9 = 302,347,067 |     Ch8 = **14.152 s**; Ch9 = **6 min 40.130 s** | Complete           |

### Largest measured workloads

```text
Ch8 InventoryModel

1,000,000,000 trades

→ 14.152 s

→ 70.66M trades/s


Ch9 ITCH Replay

302,347,067 messages

→ 400.130 s

→ 755.62k messages/s
```

---

# III. Measurement Methodology

## 3.1 Benchmark framework

The benchmark executable is:

```text
cpp/app/benchmark_main.cpp
```

The generic benchmark layer is:

```text
cpp/include/lob/benchmark.hpp

cpp/src/benchmark.cpp
```

The framework reports:

```text
Benchmark
Ops
Min(ns)
Median(ns)
Max(ns)
ns/op
ops/sec
Unit
```

The median is the primary timing statistic used for comparison.

## 3.2 Original core configuration

```text
Warmup runs:     2

Measured runs: 10
```

Core tiers:

```text
100

1,000

10,000

100,000
```

## 3.3 Large-workload policy

Expensive workloads use fewer repetitions so the benchmark remains practical:

```text
1M       → 3 measured runs

10M      → 3 measured runs

100M     → 2 measured runs

1B       → 1 measured run
```

The maximum ITCH workload is also run once because it already represents several minutes of real historical replay.

## 3.4 Operation-count convention

`Ops` represents the amount of target work performed by one timed invocation.

Examples:

```text
OrderBook::add [10000]

→ 10,000 adds

cancel+restore [10000]

→ 20,000 operations

modify pair [10000]

→ 20,000 operations

Simulator ADD [10000]

→ 20,000 event/processing operations
```

---

# IV. Test 1 — Core Market Engine

Test 1 covers:

```text
Ch1  OrderBook

Ch2  Matching

Ch3  Order tracking

Ch4  Modification

Ch5  Simulator
```

The workloads are:

```text
OrderBook::add

OrderBook::find_order

OrderBook::best_bid

OrderBook::best_ask

OrderBook::cancel+restore

OrderBook::modify pair

Matching single-level

Matching multi-level

Simulator ADD batch

Simulator mixed batch
```

## 4.1 Measured 100-tier

| Benchmark             | Ops |  Min (ns) | Median (ns) |  Max (ns) |     ns/op |      ops/sec |
| --------------------- | --: | --------: | ----------: | --------: | --------: | -----------: |
| OrderBook::add        | 100 |   142,300 |     167,200 |   222,700 |  1,672.00 |   598,086.12 |
| find_order            |   1 |     2,100 |       2,150 |     2,200 |  2,150.00 |   465,116.28 |
| best_bid              |   1 |     1,000 |       1,000 |     1,100 |  1,000.00 | 1,000,000.00 |
| best_ask              |   1 |     1,000 |       1,000 |     1,100 |  1,000.00 | 1,000,000.00 |
| cancel+restore        | 200 | 1,112,200 |   1,235,400 | 1,845,300 |  6,177.00 |   161,890.89 |
| modify pair           | 200 | 1,040,600 |   1,054,150 | 1,691,400 |  5,270.75 |   189,726.32 |
| single-level matching | 100 | 1,265,900 |   1,384,250 | 1,760,200 | 13,842.50 |    72,241.29 |
| multi-level matching  | 100 |   151,400 |     161,700 |   227,900 |  1,617.00 |   618,429.19 |
| Simulator ADD         | 200 | 3,645,600 |   4,081,400 | 4,506,200 | 20,407.00 |    49,002.79 |
| Simulator mixed       | 200 | 1,340,600 |   1,417,950 | 2,381,500 |  7,089.75 |   141,048.70 |

## 4.2 Measured 1k-tier

| Benchmark             |   Ops |    Min (ns) | Median (ns) |    Max (ns) |      ns/op |    ops/sec |
| --------------------- | ----: | ----------: | ----------: | ----------: | ---------: | ---------: |
| OrderBook::add        | 1,000 |   5,371,800 |   9,801,650 |  12,008,200 |   9,801.65 | 102,023.64 |
| find_order            |     1 |      19,300 |      19,700 |      19,800 |  19,700.00 |  50,761.42 |
| best_bid              |     1 |      16,600 |      17,000 |      17,900 |  17,000.00 |  58,823.53 |
| best_ask              |     1 |      12,400 |      13,200 |      14,800 |  13,200.00 |  75,757.58 |
| cancel+restore        | 2,000 |  88,885,500 |  90,094,750 | 102,583,300 |  45,047.38 |  22,198.85 |
| modify pair           | 2,000 |  88,558,800 |  89,246,200 |  90,518,400 |  44,623.10 |  22,409.92 |
| single-level matching | 1,000 | 115,302,200 | 117,656,750 | 119,490,000 | 117,656.75 |   8,499.30 |
| multi-level matching  | 1,000 |   6,425,400 |   6,634,450 |   7,332,800 |   6,634.45 | 150,728.39 |
| Simulator ADD         | 2,000 | 667,688,600 | 685,164,150 | 724,213,200 | 342,582.08 |   2,919.01 |
| Simulator mixed       | 2,000 | 278,182,700 | 280,767,300 | 303,808,500 | 140,383.65 |   7,123.34 |

## 4.3 Measured 10k-tier

| Benchmark             |    Ops |       Min (ns) |    Median (ns) |       Max (ns) |        ns/op |   ops/sec |
| --------------------- | -----: | -------------: | -------------: | -------------: | -----------: | --------: |
| OrderBook::add        | 10,000 |    392,100,400 |    394,507,500 |    428,750,200 |    39,450.75 | 25,348.06 |
| find_order            |      1 |        167,200 |        167,600 |        168,100 |   167,600.00 |  5,966.59 |
| best_bid              |      1 |         83,000 |         83,050 |        110,500 |    83,050.00 | 12,040.94 |
| best_ask              |      1 |         80,800 |         81,100 |         81,300 |    81,100.00 | 12,330.46 |
| cancel+restore        | 20,000 |  8,801,151,000 |  8,836,952,000 |  9,169,898,300 |   441,847.60 |  2,263.22 |
| modify pair           | 20,000 |  8,758,851,900 |  8,803,231,550 |  9,533,606,000 |   440,161.58 |  2,271.89 |
| single-level matching | 10,000 | 11,690,653,800 | 11,733,402,500 | 11,943,593,300 | 1,173,340.25 |    852.27 |
| multi-level matching  | 10,000 |    583,214,800 |    584,981,150 |    590,169,700 |    58,498.11 | 17,094.57 |
| Simulator ADD         | 20,000 | 89,327,986,000 | 89,543,655,200 | 89,905,338,500 | 4,477,182.76 |    223.35 |
| Simulator mixed       | 20,000 | 38,611,044,500 | 38,678,273,100 | 41,013,078,600 | 1,933,913.66 |    517.09 |

## 4.4 Measured 100k-tier — completed portion

| Benchmark      |     Ops |       Min (ns) |    Median (ns) |       Max (ns) |        ns/op |  ops/sec |
| -------------- | ------: | -------------: | -------------: | -------------: | -----------: | -------: |
| OrderBook::add | 100,000 | 38,483,471,000 | 38,751,535,850 | 41,100,004,100 |   387,515.36 | 2,580.54 |
| find_order     |       1 |      1,671,400 |      1,673,150 |      1,799,700 | 1,673,150.00 |   597.68 |
| best_bid       |       1 |        828,900 |        829,100 |        994,300 |   829,100.00 | 1,206.13 |
| best_ask       |       1 |        808,200 |        811,550 |        811,700 |   811,550.00 | 1,232.21 |

The remaining 100k workloads were not measured under the original repeated-run policy.

---

# V. Test 1 — Scaling Findings

## 5.1 OrderBook insertion

Measured:

```text
10,000 adds

→ 394.508 ms

100,000 adds

→ 38.752 s
```

The workload increased by `10×`, while median time increased by approximately `98.2×`.

That is strong empirical evidence of superlinear scaling in the current benchmark path.

## 5.2 Order lookup and top-of-book queries

The 10k→100k transition is close to a 10× increase for all three prepared-book queries:

```text
find_order

167.6 µs → 1.673 ms

best_bid

83.05 µs → 829.1 µs

best_ask

81.10 µs → 811.55 µs
```

These results are much closer to linear growth with book size than `OrderBook::add`.

## 5.3 What has not yet been proven

The benchmark identifies the scaling behavior, not its exact internal cause.

Candidate mechanisms such as:

```text
vector element movement

price-level maintenance

OrderMap updates

order removal mechanics
```

remain hypotheses until profiling confirms the hot path.

---

# VI. Estimated 100k Completion — Core Workloads

The following values are planning estimates only. They are not measurements.

The measured values for the first four workloads are shown separately because those four were actually completed.

| Benchmark             | 100k status |               Median time |
| --------------------- | ----------- | ------------------------: |
| OrderBook::add        | Measured    |               **38.75 s** |
| find_order            | Measured    |               **1.67 ms** |
| best_bid              | Measured    |              **0.829 ms** |
| best_ask              | Measured    |              **0.812 ms** |
| cancel+restore        | Estimated   |  **~866.8 s ≈ 14.45 min** |
| modify pair           | Estimated   |  **~868.3 s ≈ 14.47 min** |
| single-level matching | Estimated   | **~1170.1 s ≈ 19.50 min** |
| multi-level matching  | Estimated   |               **~51.6 s** |
| Simulator ADD         | Estimated   |    **~11,702 s ≈ 3.25 h** |
| Simulator mixed       | Estimated   |     **~5,328 s ≈ 1.48 h** |

These projections explain why continuing the original 100k run with ten measured repetitions would have been impractical.

---

# VII. Test 2 — Market Analysis

Test 2 covers:

```text
Ch6  Metrics

Ch7  Execution Analysis
```

Both the **1M and 5M tiers were measured successfully**.

The measured 5M results provide a second high-scale data point, allowing the report to assess scaling without relying solely on extrapolation.

## 7.1 Chapter 6 — Metrics

### 1M measured workload

```text
Metrics::calculate_metrics [1000000]
```

```text
Ops:

1,000,000

Min:

14,855,200 ns

Median:

14,933,000 ns

Max:

15,240,300 ns

ns/op:

14.93

ops/sec:

66,965,780.49
```

Median elapsed time:

```text
14.933 ms
```

### 5M measured workload

```text
Metrics::calculate_metrics [5000000]
```

```text
Ops:

5,000,000

Min:

64,119,300 ns

Median:

67,160,800 ns

Max:

74,541,600 ns

ns/op:

13.43

ops/sec:

74,448,190.02
```

Median elapsed time:

```text
67.161 ms
```

## 7.2 Chapter 7 — Execution Analysis

### 1M measured workload

```text
Execution::calculate_execution_result [1000000]
```

```text
Ops:

1,000,000

Min:

277,546,500 ns

Median:

306,000,500 ns

Max:

312,983,600 ns

ns/op:

306.00

ops/sec:

3,267,968.52
```

Median elapsed time:

```text
306.001 ms
```

### 5M measured workload

```text
Execution::calculate_execution_result [5000000]
```

```text
Ops:

5,000,000

Min:

1,241,910,700 ns

Median:

1,357,371,700 ns

Max:

1,369,858,000 ns

ns/op:

271.47

ops/sec:

3,683,589.40
```

Median elapsed time:

```text
1.357 s
```

## 7.3 Test 2 scaling result

The measured 1M→5M transition is approximately linear for both workloads.

| Benchmark |  1M median | 5M median | 1M ns/op | 5M ns/op |
| --------- | ---------: | --------: | -------: | -------: |
| Metrics   |  14.933 ms | 67.161 ms |    14.93 |    13.43 |
| Execution | 306.001 ms |   1.357 s |   306.00 |   271.47 |

The workload increased by `5×`.

Measured median elapsed time increased by approximately:

```text
Metrics

14.933 ms → 67.161 ms

≈ 4.50×


Execution

306.001 ms → 1.357 s

≈ 4.44×
```

Per-operation cost remained in the same general range and did not exhibit the kind of large superlinear growth observed in the core OrderBook workloads.

The 5M results therefore provide measured evidence that these two analytical workloads scale approximately linearly across the tested range.

## 7.4 Remaining Test 2 planning tiers

The original high-scale design included:

```text
1M

5M

10M

25M
```

The current measured state is:

```text
1M  → measured

5M  → measured

10M → not measured

25M → not measured
```

The previous projections for 10M and 25M are no longer necessary as primary results because the benchmark now has two measured points. If required for future planning, additional tiers can be run later.

---

# VIII. Test 3 — Accounting and Historical Replay

Test 3 covers:

```text
Ch8  Inventory / P&L

Ch9  ITCH Replay
```

The high-scale tiers were intentionally much larger than the early microbenchmarks.

---

# IX. Chapter 8 — Inventory / P&L

The benchmark exercises the real:

```cpp
InventoryModel::process_trade()
```

path.

The 1B workload does not preallocate a billion-element `std::vector<Trade>`. Trades are generated deterministically during the timed workload.

The benchmark pattern uses:

```text
incoming order id = 1

testing resting id = i + 2

price = 10000 + (i % 50)

quantity = 1 + (i % 100)

side = BUY

initial cash = 100,000,000
```

## 9.1 Measured results

| Tier |    Median (ns) |  Median time |     ns/op |           ops/sec |
| ---: | -------------: | -----------: | --------: | ----------------: |
|   1M |     16,835,500 |    16.836 ms |     16.84 |     59,398,295.27 |
|  10M |    142,950,500 |   142.951 ms |     14.30 |     69,954,284.87 |
| 100M |  1,408,595,850 |      1.409 s |     14.09 |     70,992,683.96 |
|   1B | 14,151,828,400 | **14.152 s** | **14.15** | **70,662,247.43** |

## 9.2 Scaling result

The per-operation cost stabilizes around:

```text
14–16 ns/trade
```

and throughput stabilizes around:

```text
~70M trade-accounting operations/sec
```

The 1B result is therefore strong evidence of approximately linear scaling for this workload.

---

# X. Chapter 9 — Historical ITCH Replay

## 10.1 Dataset

```text
Nasdaq TotalView-ITCH 5.0

Date: 2019-10-18

File:

data/itch/2019-10-18/raw/decompressed/S101819-v50.txt

Selected Stock Locate:

123
```

## 10.2 Pipeline under test

```text
ITCH file

    ↓

ItchFileReader

    ↓

get_type_parser()

    ↓

ItchMessage

    ↓

ItchMapper::map()

    ↓

ReplayOperation

    ↓

ItchReplay::apply()

    ↓

OrderBook
```

This is the real Chapter 9 replay path, not a synthetic replacement.

Ignored selected-security messages still exercise the reader/parser/mapper path before being ignored.

Historical executions, cancellations, deletes, and replacements are processed using the replay semantics rather than re-matching them as synthetic incoming orders.

## 10.3 Maximum available workload

The benchmark attempted a target of:

```text
1,000,000,000 messages
```

The available selected stream ended at:

```text
302,347,067 messages
```

Therefore the actual maximum benchmark tier is:

```text
302,347,067 messages
```

This is approximately `302.3 million` messages, not 300 billion.

## 10.4 Measured results

|        Tier |     Median (ns) |        Median time |        ns/op |        ops/sec |
| ----------: | --------------: | -----------------: | -----------: | -------------: |
|          1M |   1,350,153,100 |            1.350 s |     1,350.15 |     740,656.74 |
|         10M |  13,921,173,100 |           13.921 s |     1,392.12 |     718,330.27 |
|        100M | 137,520,390,650 |     2 min 17.520 s |     1,375.20 |     727,164.89 |
| 302,347,067 | 400,129,848,100 | **6 min 40.130 s** | **1,323.41** | **755,622.38** |

## 10.5 Scaling result

Measured per-message cost remains tightly grouped:

```text
1M        → 1,350.15 ns

10M       → 1,392.12 ns

100M      → 1,375.20 ns

302.347M  → 1,323.41 ns
```

Throughput remains approximately:

```text
~0.72–0.76 million messages/sec
```

This is consistent with approximately linear scaling across the measured historical replay range.

---

# XI. Inventory vs ITCH Replay

The two large-scale results measure very different workloads.

### Inventory

```text
1B trades

→ 14.152 s

→ 14.15 ns/trade

→ 70.66M trades/sec
```

### ITCH Replay

```text
302.347M messages

→ 400.130 s

→ 1.323 µs/message

→ 755.62k messages/sec
```

The replay path performs substantially more work per input record because it includes:

```text
file I/O

message framing

binary parsing

message construction

variant dispatch

security filtering

semantic mapping

replay validation

OrderBook mutation
```

These are therefore not interchangeable speed metrics.

---

# XII. Why the 100k Core Run Was Stopped

The original core policy was:

```text
2 warmups

+

10 measured runs
```

At 100k, the measured `OrderBook::add` workload already required approximately:

```text
38.75 s per measured invocation
```

Twelve full executions would therefore take roughly:

```text
38.75 × 12

≈ 465 s

≈ 7.75 min
```

for that one benchmark alone.

The more expensive workloads had much worse projected scaling, with estimated single-invocation times of minutes to hours.

The stop was therefore a deliberate experimental decision, not a benchmark failure.

---

# XIII. Measurement Caveats

## 13.1 Setup cost

Several Test 1 workloads construct substantial benchmark state inside the timed invocation.

For example:

```text
OrderBook::add

single-level matching

multi-level matching

Simulator batches
```

therefore measure:

```text
state construction

+

target processing
```

rather than a perfectly isolated single-function latency.

This is acceptable for the first baseline as long as the interpretation remains explicit.

## 13.2 Large analytical vectors

The current Ch6–Ch7 interfaces operate on `std::vector<Trade>` inputs. Extremely large future tests should therefore be treated as data-materialization experiments unless the benchmark is deliberately redesigned around streaming or reused prepared data.

## 13.3 Timing variability

The benchmark reports min/median/max because runtime noise is expected. The median is used as the primary comparison statistic.

---

# XIV. Engineering Conclusions

## 14.1 What is established

The benchmark establishes that:

```text
Inventory accounting scales approximately linearly through 1B trades.

Historical ITCH replay scales approximately linearly through 302.347M messages.

Metrics and Execution Analysis scale approximately linearly across the measured 1M→5M range.

Several current core OrderBook/Simulator workloads scale strongly worse than linearly at large book sizes.
```

## 14.2 What remains unknown

The benchmark does not yet identify the exact cause of the core-engine scaling.

The next evidence must come from profiling.

Possible mechanisms include:

```text
vector movement

price-level maintenance

OrderMap maintenance

order removal

other state-management costs
```

No one mechanism should be declared the bottleneck until measured.

---

# XV. Optimization Protocol

The project remains on the following sequence:

```text
baseline

→ profile

→ identify hot path

→ confirm bottleneck

→ make one targeted change

→ compile

→ correctness tests

→ determinism checks

→ identical benchmark

→ compare before/after
```

Every optimization must preserve:

```text
OrderMap consistency

FIFO behavior

price-time priority

matching semantics

modification semantics

simulator ordering

historical replay semantics

inventory accounting

execution analysis
```

A faster implementation with changed semantics is not a successful optimization.

---

# XVI. Reproducibility Record

## Build environment

```text
Windows

MSYS2 UCRT64

C++17

g++

-Wall

-Wextra

-pedantic

-Icpp/include/lob
```

The exact compiler version, CPU model, RAM, and final machine metadata should be recorded with the final archived benchmark run.

## Relevant source files

```text
cpp/app/benchmark_main.cpp

cpp/include/lob/benchmark.hpp

cpp/src/benchmark.cpp

cpp/src/order_book.cpp

cpp/src/order_map.cpp

cpp/src/simulator.cpp

cpp/src/metrics.cpp

cpp/src/execution.cpp

cpp/src/inventory_model.cpp

cpp/src/itch_file_reader.cpp

cpp/src/itch_parser.cpp

cpp/src/itch_mapper.cpp

cpp/src/itch_replay.cpp
```

## Correctness baseline

The Chapter 9 test suite had previously reached:

```text
37 / 37 tests passing
```

before the Chapter 10 performance work.

---

# XVII. Session Outcome

The benchmark session succeeded in establishing a real baseline rather than forcing every workload to complete at the same repetition count.

The most important evidence is:

```text
OrderBook::add

10k → 394.508 ms

100k → 38.752 s
```

```text
Metrics

1M → 14.933 ms

5M → 67.161 ms

→ approximately linear scaling
```

```text
Execution

1M → 306.001 ms

5M → 1.357 s

→ approximately linear scaling
```

```text
InventoryModel

1B → 14.152 s
```

```text
ITCH Replay

302.347M → 400.130 s
```

These measurements now provide a concrete reference point for profiling and optimization.

---

# XVIII. Final Recap Table — All Chapters / All Tiers

| Test | Chapter                   |        Tier | Status       |        Median time |        ns/op |    Throughput |
| ---- | ------------------------- | ----------: | ------------ | -----------------: | -----------: | ------------: |
| I    | Ch1 OrderBook::add        |         100 | Measured     |           0.167 ms |        1,672 |     598.09k/s |
| I    | Ch1 OrderBook::add        |          1k | Measured     |           9.802 ms |        9,802 |     102.02k/s |
| I    | Ch1 OrderBook::add        |         10k | Measured     |         394.508 ms |       39,451 |      25.35k/s |
| I    | Ch1 OrderBook::add        |        100k | Measured     |       **38.752 s** |  **387,515** |   **2.58k/s** |
| I    | Ch3 find_order            |         100 | Measured     |            2.15 µs |        2,150 |     465.12k/s |
| I    | Ch3 find_order            |          1k | Measured     |            19.7 µs |       19,700 |      50.76k/s |
| I    | Ch3 find_order            |         10k | Measured     |           167.6 µs |      167,600 |       5.97k/s |
| I    | Ch3 find_order            |        100k | Measured     |           1.673 ms |    1,673,150 |      597.68/s |
| I    | Ch1 best_bid              |         100 | Measured     |            1.00 µs |        1,000 |       1.00M/s |
| I    | Ch1 best_bid              |          1k | Measured     |            17.0 µs |       17,000 |      58.82k/s |
| I    | Ch1 best_bid              |         10k | Measured     |           83.05 µs |       83,050 |      12.04k/s |
| I    | Ch1 best_bid              |        100k | Measured     |           0.829 ms |      829,100 |       1.21k/s |
| I    | Ch1 best_ask              |         100 | Measured     |            1.00 µs |        1,000 |       1.00M/s |
| I    | Ch1 best_ask              |          1k | Measured     |            13.2 µs |       13,200 |      75.76k/s |
| I    | Ch1 best_ask              |         10k | Measured     |           81.10 µs |       81,100 |      12.33k/s |
| I    | Ch1 best_ask              |        100k | Measured     |           0.812 ms |      811,550 |       1.23k/s |
| I    | Ch1/3 cancel+restore      |        100k | Estimated    |         ~14.45 min |            — |             — |
| I    | Ch4 modify pair           |        100k | Estimated    |         ~14.47 min |            — |             — |
| I    | Ch2 single-level matching |        100k | Estimated    |         ~19.50 min |            — |             — |
| I    | Ch2 multi-level matching  |        100k | Estimated    |            ~51.6 s |            — |             — |
| I    | Ch5 Simulator ADD         |        100k | Estimated    |            ~3.25 h |            — |             — |
| I    | Ch5 Simulator mixed       |        100k | Estimated    |            ~1.48 h |            — |             — |
| II   | Ch6 Metrics               |          1M | Measured     |      **14.933 ms** |    **14.93** |  **66.97M/s** |
| II   | Ch6 Metrics               |          5M | Measured     |      **67.161 ms** |    **13.43** |  **74.45M/s** |
| II   | Ch7 Execution             |          1M | Measured     |     **306.001 ms** |   **306.00** |   **3.27M/s** |
| II   | Ch7 Execution             |          5M | Measured     |        **1.357 s** |   **271.47** |   **3.68M/s** |
| II   | Ch6 Metrics               |         10M | Not measured |                  — |            — |             — |
| II   | Ch6 Metrics               |         25M | Not measured |                  — |            — |             — |
| II   | Ch7 Execution             |         10M | Not measured |                  — |            — |             — |
| II   | Ch7 Execution             |         25M | Not measured |                  — |            — |             — |
| III  | Ch8 Inventory             |          1M | Measured     |          16.836 ms |        16.84 |      59.40M/s |
| III  | Ch8 Inventory             |         10M | Measured     |         142.951 ms |        14.30 |      69.95M/s |
| III  | Ch8 Inventory             |        100M | Measured     |            1.409 s |        14.09 |      70.99M/s |
| III  | Ch8 Inventory             |          1B | Measured     |       **14.152 s** |    **14.15** |  **70.66M/s** |
| III  | Ch9 ITCH Replay           |          1M | Measured     |            1.350 s |     1,350.15 |     740.66k/s |
| III  | Ch9 ITCH Replay           |         10M | Measured     |           13.921 s |     1,392.12 |     718.33k/s |
| III  | Ch9 ITCH Replay           |        100M | Measured     |     2 min 17.520 s |     1,375.20 |     727.16k/s |
| III  | Ch9 ITCH Replay           | 302,347,067 | Measured     | **6 min 40.130 s** | **1,323.41** | **755.62k/s** |

### Final status

```text
Test I   — Ch1–5 core baseline: partially measured

Test II  — Ch6–7 market analysis: 1M + 5M measured

Test III — Ch8–9 large-scale baseline: complete

1B Inventory benchmark: complete

302,347,067-message ITCH benchmark: complete

100k core completion: partially measured + explicitly estimated remainder

Next step: profiling
```
