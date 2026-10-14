# Profiling Notes

## 1. Purpose

Profiling is used to identify costs in a defined workload before changing data structures or algorithms. The findings below describe the measured implementation; they do not imply that every hotspot has been optimized away.

## 2. Observed bottlenecks

### Removal-heavy order-book workloads

Orders and price levels are held in vectors. Erasing an element can move later elements, and the `OrderMap` must repair the stored indices for affected orders. In the profiled removal-heavy workload, this index-repair work—not merely the erase statement itself—was a major cost.

The invariant remains:

```text
OrderMap: OrderId -> OrderLocation{side, price, index}
```

`OrderMap` is a lookup index, not an owner of `Order` objects. Avoid introducing pointer ownership or a second order index without a deliberate architecture change and tests for consistency.

### ADD-heavy synthetic simulation

The profiled ADD-heavy workload showed repeated sorting of price levels inside the synthetic order-processing path as a dominant cost. This is a workload-specific observation. It should not be generalized to all operations or confused with ITCH replay throughput.

## 3. How to report a profile

For a useful profile, record:

1. The exact executable/commit and compiler/build flags.
2. The workload (synthetic order mix, cancellation/removal mix, or historical ITCH file), input size, Stock Locate, and message limit where relevant.
3. Whether the profile includes file I/O, parsing, Python binding overhead, snapshot extraction, or plotting.
4. The measured hot functions/counters and the tool/command used.
5. A matching baseline and repeatable validation after any change.

C++ engine benchmarks, ITCH replay, Python feature generation, and dashboard end-to-end runs are different scopes; profile and report them separately.

## 4. Keep correctness invariants fixed

Performance work must preserve these behaviors unless a change is explicitly designed and tested:

- `OrderBook` owns live market state.
- `PriceLevel` owns orders by value and maintains the aggregate quantity.
- `OrderMap` locations agree with actual order vector positions after insertion, cancellation, fill, and modification.
- Matching observes price-time priority and executes at the resting order's price.
- The simulator validates chronological/sequence constraints.
- Historical ITCH state changes are applied directly; they are not synthetic incoming orders to be re-matched.
- Metrics, execution analysis, and inventory accounting observe the shared core rather than maintaining a divergent second book.

A faster profile that violates a state invariant is not a valid optimization.

## 5. What the current profile does not claim

The project notes identify hotspots; they do not establish that the implementation achieves sub-microsecond end-to-end latency, zero allocation, lock-free operation, exchange-grade throughput, or “zero-copy” integration. Do not add any of those claims to documentation unless the actual relevant path supports the claim and a reproducible measurement is retained.

The historical ITCH full-file measurement of about 755,622 messages/second is described separately in [Benchmarks](benchmark.md). It measures end-to-end historical ingestion/replay for one environment, not a universal engine throughput figure.
