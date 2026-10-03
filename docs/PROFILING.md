# Chapter 10 — Profiling

## 1. Purpose

The benchmarking phase establishes the performance baseline. The profiling phase decomposes the measured cost of the existing implementation into its internal operations and identifies the code paths responsible for the observed scaling.

The profiling objective is not to make the implementation faster by changing the architecture. The objective is to establish, with measurements, where execution time is actually spent before any performance optimization is attempted.

The completed profiling targets were:

```text
OrderBook::add

cancel / restore

modify

matching

simulator workloads

end-to-end ITCH replay
```

The development sequence remains:

```text
baseline measurement
        ↓
profiling
        ↓
identify hot path
        ↓
confirm bottleneck
        ↓
one targeted optimization
        ↓
correctness tests
        ↓
determinism checks
        ↓
identical benchmark
        ↓
compare before / after
```

Profiling itself is now complete. The measurements below are the final profiling evidence for the current Chapter 10 state.

---

# 2. Profiling Architecture

The profiler is intentionally separate from the benchmark framework.

Profiler files:

```text
cpp/include/lob/profile.hpp

cpp/src/profile.cpp

tests/cpp/test_profile.cpp

cpp/app/profile_main.cpp
```

The profiler provides:

```text
named scopes

call count

total duration

average duration

minimum duration

maximum duration

share of recorded time
```

Timing uses:

```text
std::chrono::steady_clock
```

RAII scopes are used through:

```cpp
LOB_PROFILE_SCOPE("scope name");
```

The profiler aggregates measurements by scope name and sorts results by total recorded time.

The scope measurements are inclusive when scopes are nested. Therefore a parent such as `OrderBook::process_order` contains the work measured by its child scopes. Reported shares are shares of recorded scope time, not exclusive CPU percentages.

---

# 3. Profiling Methodology

Profiling was performed in three conceptual stages.

### Stage 1 — Workload-level profiling

The major workload boundaries were measured:

```text
OrderBook::add workload

cancel / restore workload

modify workload

single-level matching

multi-level matching

Simulator ADD

Simulator mixed

ITCH end-to-end
```

### Stage 2 — Internal decomposition

The implementation beneath the major workloads was instrumented to expose important internal paths.

For example:

```text
OrderBook::cancel
    ↓
OrderMap lookup
    ↓
price-level search
    ↓
order resolution
    ↓
vector erase
    ↓
shifted-index updates
    ↓
empty-price-level cleanup
```

The same decomposition principle was applied to matching, simulator processing, and the ITCH pipeline.

### Stage 3 — Confirmation

The final profile confirms several concrete hot paths rather than relying on benchmark totals alone.

The strongest confirmed findings are:

```text
matching → shifted-index maintenance

Simulator ADD / mixed → repeated price-level sorting

modify increase → cancel + re-add path

ITCH → parser dispatch / file reading dominate the executed prefix,
       while historical replay itself was not exercised in this prefix
```

---

# 4. Profiling Workloads and Conditions

The final profiling runs used:

```text
OrderBook workload size: 10,000
Matching workload size: 10,000
Simulator workload size: 10,000
ITCH workload size: 1,000,000 messages
```

The workload executions were deterministic and instrumented with the same implementation used for the Chapter 10 baseline benchmarking.

The 1,000,000-message ITCH run parsed the prefix completely, but the selected-security replay path was not exercised, so its timing must be interpreted as a parser/reader/mapper profile rather than a full historical-replay mutation profile.

---

# 5. OrderBook::add

Measured profiling result for 10,000 adds:

```text
OrderBook::add
59,097,900 ns
59.0979 ms
5,909 ns/call average
```

The most visible recorded child scopes were:

```text
OrderBook::add::ordermap_add
3,667,900 ns
5.54% of recorded profile time

OrderBook::add::new_level_insert
1,751,600 ns
2.65%

OrderMap::add
1,642,800 ns
2.48%
```

The workload inserted 10,000 new price levels, so `new_level_insert` was exercised on every operation.

The parent `OrderBook::add` scope is substantially larger than the currently named child scopes. This means the current profile attributes most of the add cost to work outside those child measurements, rather than proving that `OrderMap::add` or new-level insertion is the dominant source.

The important measured conclusion is therefore:

```text
OrderBook::add is non-trivial at 10,000 operations,

but the currently profiled child scopes do not account for most
of the parent cost.
```

No optimization is inferred from the add result alone.

---

# 6. Cancel / Restore

Measured profiling result for the 10,000 cancel/restore workload:

```text
OrderBook::cancel
304,074,900 ns
304.0749 ms
29.15% of total recorded profile time

OrderBook::add
156,119,400 ns
156.1194 ms
14.97%
```

The internal cancel profile showed:

```text
cancel::price_level_search
293,349,000 ns
28.12%

cancel::empty_level_cleanup
265,337,100 ns
25.43%

cancel::ordermap_remove
5,507,800 ns
0.53%

cancel::shifted_indices
4,859,400 ns
0.47%

cancel::ordermap_find
3,020,700 ns
0.29%

cancel::vector_erase
707,500 ns
0.07%
```

The key interpretation is that the raw vector erase is very small compared with the broader cancellation path. The expensive work is associated with locating the relevant level and handling the resulting state/cleanup.

The `price_level_search` and `empty_level_cleanup` scopes are nested, so their percentages must not be added as independent exclusive costs.

The measured result therefore rules out the simple explanation that `std::vector::erase` itself is the main cancellation bottleneck.

---

# 7. Modify

Measured profiling result for the 10,000-operation modify workload:

```text
OrderBook::modify
509,952,500 ns
509.9525 ms
24.79% of total recorded profile time
```

The increase path was dominated by the established cancel/re-add semantics:

```text
modify::cancel_old_order
302,701,600 ns
14.72%

OrderBook::cancel
298,189,700 ns
14.50%

modify::re_add_order
156,303,400 ns
7.60%

OrderBook::add
153,786,900 ns
7.48%
```

The lookup and in-place update costs were much smaller:

```text
modify::lookup
31,588,600 ns
1.54%

find_order
23,459,500 ns
1.14%

modify::in_place
598,500 ns
0.03%
```

This confirms the expected structural difference:

```text
modify decrease
    → in-place quantity update
    → small cost

modify increase / priority-changing path
    → cancel old order
    → re-add order
    → much larger cost
```

This is consistent with the simulator's established FIFO semantics: an increase in quantity or price change resets queue priority through cancel/re-add behavior.

---

# 8. Matching

## 8.1 Single-level matching

Measured profiling result:

```text
10,000-order workload

OrderBook::process_order
75,099,665,400 ns
≈ 75.100 s
```

The dominant nested path was:

```text
process_order::buy_order_traversal
75,095,784,300 ns

process_order::full_fill_removal
75,085,872,400 ns

OrderBook::cancel
75,081,342,700 ns

cancel::price_level_search
75,069,655,300 ns

cancel::shifted_indices
74,971,377,500 ns
≈ 74.971 s
```

The detailed index-maintenance scopes provide the decisive evidence:

```text
update_shifted_indices
≈ 24.410 s total

update_shifted_indices::ordermap_update
49,995,000 calls
24,410,018,500 ns
≈ 24.410 s

update_shifted_indices::ordermap_find
49,995,000 calls
15,262,885,400 ns
≈ 15.263 s

OrderMap::find
50,005,000 calls
3,804,002,300 ns
≈ 3.804 s

OrderMap::update
49,995,000 calls
3,532,209,200 ns
≈ 3.532 s

cancel::vector_erase
73,079,600 ns
≈ 73.1 ms
```

The exact important observation is that roughly 50 million shifted-order index updates were generated by the workload.

The raw erase operation itself was tiny in comparison. The dominant cost came from preserving the `OrderMap` location invariant after vector elements shifted.

Therefore the single-level matching profile confirms:

```text
full fills
    ↓
vector erasure
    ↓
many subsequent orders shift left
    ↓
OrderMap locations must be repaired
    ↓
repeated find/update operations dominate the cost
```

This is the clearest confirmed microstructure-engine bottleneck in the profiling run.

### 8.2 Multi-level matching

Measured profiling result:

```text
10,000-order workload

OrderBook::process_order
3,745,490,800 ns
≈ 3.745 s
```

The key internal measurements were:

```text
process_order::buy_order_traversal
≈ 3.744 s

process_order::full_fill_removal
≈ 3.739 s

OrderBook::cancel
≈ 3.737 s

cancel::price_level_search
≈ 3.732 s

cancel::shifted_indices
≈ 3.718 s

update_shifted_indices::ordermap_update
2,497,500 calls
≈ 1.221 s

update_shifted_indices::ordermap_find
2,497,500 calls
≈ 0.733 s
```

The multi-level workload generated only about 2.5 million shifted-index operations in each direction, far fewer than the approximately 50 million generated by the single-level case.

This explains the large workload-level difference:

```text
single-level matching
≈ 75.100 s

multi-level matching
≈ 3.745 s
```

The profiling evidence therefore links the scaling difference primarily to the amount of index maintenance induced by repeated removals from a densely populated price level, rather than to trade construction itself.

---

# 9. Simulator

## 9.1 Simulator ADD

Measured profiling result:

```text
Simulator::process_events
5,839,651,300 ns
≈ 5.840 s
```

The dominant internal path was:

```text
process_event
5,835,974,100 ns

ADD
5,827,246,100 ns

ADD::process_order
5,742,215,700 ns

OrderBook::process_order
5,737,630,700 ns

process_order::sort
5,653,012,400 ns

OrderBook::sort_price_levels
5,648,032,400 ns

sort_price_levels::bids
5,634,764,900 ns
```

The measured result is unambiguous: repeated price-level sorting dominates the simulator ADD workload.

The expensive path is therefore:

```text
Simulator ADD
    ↓
OrderBook::process_order
    ↓
sort_price_levels
    ↓
bid-side sorting
```

The profiling evidence shows that simulator ADD is not primarily expensive because of simulator bookkeeping. Most of the recorded time is inherited from repeated sorting in the underlying OrderBook processing path.

## 9.2 Simulator Mixed

Measured profiling result:

```text
Simulator::process_events
2,438,097,800 ns
≈ 2.438 s
```

The workload contained approximately:

```text
6,666 ADD events
3,334 CANCEL events
```

The dominant path was again sorting:

```text
ADD
2,383,129,500 ns

ADD::process_order
2,343,809,200 ns

OrderBook::process_order
2,341,020,500 ns

process_order::sort
2,301,225,200 ns

OrderBook::sort_price_levels
2,298,168,800 ns

sort_price_levels::bids
2,290,357,900 ns
```

The CANCEL path was much smaller:

```text
CANCEL
42,923,300 ns

OrderBook::cancel
39,879,300 ns
```

The mixed workload therefore confirms the same hot path observed in Simulator ADD:

```text
repeated sorting inside OrderBook::process_order
```

---

# 10. End-to-End ITCH Replay

The final ITCH profiling run processed:

```text
1,000,000 messages read
1,000,000 messages parsed
1,000,000 messages ignored by the mapper/replay selection path
0 Add operations
0 Reduce operations
0 Remove operations
0 Replace operations
0 replay successes
0 replay errors
```

The end-to-end timing was:

```text
ITCH::end_to_end
6,363,542,400 ns
≈ 6.364 s
```

The dominant executed scopes were:

```text
ITCH::get_type_parser
3,611,470,600 ns
≈ 3.611 s

ItchFileReader::next_message
1,490,624,100 ns
≈ 1.491 s

ITCH::parse_A
1,413,963,100 ns
≈ 1.414 s

ITCH::parse_D
794,603,600 ns
≈ 0.795 s

ItchMapper::map
376,153,100 ns
≈ 0.376 s

ITCH::parse_L
317,916,200 ns
≈ 0.318 s
```

Additional parser helper costs included:

```text
read_be
296,608,500 ns
≈ 0.297 s

check_validity
237,735,100 ns
≈ 0.238 s

ITCH::parse_X
227,647,800 ns
≈ 0.228 s

ITCH::parse_U
164,343,000 ns
≈ 0.164 s

payload_read
124,197,700 ns
≈ 0.124 s

length_read
100,330,300 ns
≈ 0.100 s
```

The total of all recorded scopes was:

```text
15,836,479,300 ns
≈ 15.836 s
```

This is larger than the end-to-end value because the profiler records nested inclusive scopes. The values must therefore not be summed as though they were exclusive pipeline stages.

The critical interpretation is:

```text
Parser / FileReader / Mapper work was actually executed.

Historical replay mutation was not executed in this 1,000,000-message prefix.
```

The counters establish that the run reached the parser and mapper for one million messages but produced no selected `AddOperation`, `ReduceOperation`, `RemoveOperation`, or `ReplaceOperation` for the replay stage.

Therefore the ITCH profile provides valid measurements of the executed ingestion path, but it does not provide evidence about the performance of `ItchReplay` or the OrderBook mutation cost under historical replay for this prefix.

---

# 11. Important Interpretation Rule

The current profile contains both top-level workload scopes and nested internal scopes.

For example:

```text
ITCH::end_to_end
    ↓
FileReader
    ↓
Parser
    ↓
Mapper
```

and:

```text
OrderBook::process_order
    ↓
process_order::sort
    ↓
OrderBook::sort_price_levels
```

Therefore a parent timing already contains the work measured by its children.

A percentage such as:

```text
75% of parent time
```

is not equivalent to a mutually exclusive CPU percentage when the parent and child are both reported.

The correct use of the profile is to trace the call hierarchy and identify the dominant path, while avoiding double-counting nested scopes.

This is particularly important for the matching results, where the total recorded profiler time is much larger than the top-level `OrderBook::process_order` time because the same work appears in nested inclusive scopes.

---

# 12. Profiling Conclusions

The completed profiling phase establishes the following measured findings.

### OrderBook::add

```text
10,000 adds
59.098 ms in the top-level add scope
```

The currently named child scopes account for only a small part of that parent measurement. The add profile therefore establishes a meaningful workload cost without attributing it to one single child operation.

### Cancel / restore

```text
cancel dominates the workload

price-level search and cleanup account for the large measured path

raw vector erase is comparatively small
```

The profile does not support treating `vector::erase` by itself as the cancellation bottleneck.

### Modify

```text
modify increase is much more expensive than modify decrease
```

The increase path is expensive because the established semantics require cancel + re-add, while the decrease path remains an in-place update.

### Matching

```text
single-level matching ≈ 75.100 s
multi-level matching ≈ 3.745 s
```

The dominant confirmed mechanism is shifted-index maintenance after removals from densely populated vector-backed price levels. Approximately 50 million `OrderMap` find/update operations were generated in the single-level workload.

### Simulator

```text
Simulator ADD ≈ 5.840 s
Simulator mixed ≈ 2.438 s
```

Repeated sorting inside `OrderBook::process_order`, especially bid-side sorting, dominates both simulator workloads.

### ITCH

```text
1,000,000 messages read
1,000,000 parsed
0 replay operations
```

Parser dispatch and file reading account for most of the executed ITCH pipeline work in this prefix. Historical replay mutation was not exercised, so no `ItchReplay` bottleneck conclusion is made from this run.

These are profiling findings from measured execution paths. They identify where the current implementation spends time without changing the architecture or implementation semantics.

---

# 13. Current Chapter Status

Chapter 10 profiling is complete.

The project now has both:

```text
benchmark evidence
    → what is slow

profiling evidence
    → where the time goes
```

The strongest confirmed hot paths in the current implementation are:

```text
1. Matching:
   shifted-index maintenance and repeated OrderMap find/update work

2. Simulator ADD / mixed:
   repeated price-level sorting inside OrderBook::process_order

3. Modify increase:
   established cancel + re-add behavior

4. ITCH ingestion:
   parser dispatch and message parsing dominate the executed prefix
```

The profiling phase does not change the existing architecture:

```text
OrderBook owns market state.

OrderMap remains an index, not an owner.

Price-time priority remains unchanged.

FIFO semantics remain unchanged.

Simulator remains the orchestration layer.

ITCH replay remains historical state reconstruction.

No historical execution is re-matched as a synthetic incoming order.
```

The measured evidence is now sufficient to move from profiling into the subsequent bottleneck-driven optimization stage without relying on speculation.

---

# 14. Final Profiling Principle

The benchmark tells us **what is slow**.

The profiler tells us **where the time goes**.
