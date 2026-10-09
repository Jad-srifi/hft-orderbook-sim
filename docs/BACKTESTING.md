# Backtesting Infrastructure

## Purpose

Chapter 12.8 provides the infrastructure needed to evaluate research procedures on historical data while preserving chronological order.

The chapter contains three related concepts:

```text
Historical Replay
      ↓
Backtesting
      ↓
Out-of-Sample / Walk-Forward Evaluation
```

These concepts have different responsibilities.

## Historical Replay

Historical replay reconstructs historical market activity.

The existing ITCH pipeline is:

```text
ITCH file
    ↓
ItchFileReader
    ↓
ItchParser
    ↓
ItchMapper
    ↓
ReplayOperation
    ↓
ItchReplay
    ↓
C++ OrderBook
```

`backtests/historical_replay.py` provides a Python-level interface around this process.

Its responsibility is to process historical ITCH data and expose reconstructed market states to the Python research layer.

Historical replay answers:

> What happened in the historical market data?

It does not determine whether a research hypothesis works.

## Historical Snapshots

The live C++ `OrderBook` changes continuously during replay.

A `BookSnapshot` captures the observable state of the book at a particular point.

```text
Historical replay
       ↓
Live OrderBook
       ↓
BookSnapshot
```

Multiple snapshots can be collected into a `BookDataset`.

```text
BookSnapshot
BookSnapshot
BookSnapshot
...
      ↓
BookDataset
```

This creates Python-owned research data that can be analyzed without depending on the continuously changing C++ order book.

## `HistoricalReplay`

`historical_replay.py` provides three main operations.

### Full replay

```python
replay.run()
```

This processes the complete historical file and returns the final `OrderBook`.

### Final snapshot

```python
replay.run_and_snapshot()
```

This processes the complete historical file and returns a `BookSnapshot` of the final book state.

### Snapshot collection

```python
replay.run_and_collect(snapshot_interval)
```

This processes the historical file and captures snapshots at the specified message interval.

For example:

```python
replay.run_and_collect(1000)
```

captures a snapshot every 100 processed messages.

The current interval is message-based. It should not be interpreted as an equal-time interval because ITCH messages do not necessarily arrive at a constant rate.

## Backtesting

Backtesting evaluates a research procedure against historical observations.

The generic structure is:

```text
Historical data
      ↓
Backtest
      ↓
Research procedure
      ↓
Results
```

`backtests/backtest.py` intentionally contains very little domain-specific logic.

Its responsibility is to:

1. store the input data
2. store the research procedure
3. apply the procedure to each observation
4. collect the resulting outputs

Example:

```python
def procedure(x):
    return x * 2

backtest = Backtest([1, 2, 3], procedure)
results = backtest.run()
```

The result is:

```text
[2, 4, 6]
```

The backtest runner does not need to know what the procedure represents.

The procedure could represent:

- a microstructure hypothesis
- a forecasting rule
- a feature transformation
- a trading rule
- another research calculation

This keeps the infrastructure reusable.

## Backtest Responsibilities

`backtest.py` is responsible for:

```text
input data
    ↓
ordered observations
    ↓
research procedure
    ↓
collected results
```

It is not responsible for:

- ITCH parsing
- order-book matching
- execution accounting
- inventory accounting
- statistical formulas
- plotting
- a specific trading strategy

Those responsibilities belong to other modules.

## Out-of-Sample Testing

Out-of-sample (OOS) testing evaluates a research procedure on data that was not used to develop or calibrate it.

A simple chronological split looks like:

```text
TRAIN                         TEST
───────────────────           ───────
A B C D E                     F G H
```

The procedure is developed using the training period and then evaluated on the later test period.

The test period must remain unseen during development.

The key rule is:

```text
training data < test data
```

in chronological order.

## Why OOS Testing Matters

Testing a research idea on the same data used to develop it can produce overly optimistic results.

For example, a researcher might repeatedly modify a threshold after looking at historical results until one value performs unusually well.

That threshold may fit the historical sample rather than represent a stable relationship.

OOS evaluation provides a cleaner test:

```text
Historical development data
        ↓
Develop / calibrate
        ↓
Freeze procedure
        ↓
Later unseen data
        ↓
Evaluate
```

## Data Leakage

The most important invariant in chronological research is:

> Future information must never influence a past decision.

For an observation at time `t`, the research procedure may use information available at or before `t`, depending on the exact research design.

It must not use information from:

```text
t + 1
t + 2
t + 3
...
```

to construct a decision that supposedly existed at time `t`.

Examples of leakage include:

- using future observations to calculate a feature
- selecting parameters using the test period
- calculating normalization statistics using future data
- using future returns to construct a current signal
- randomly shuffling chronological observations when temporal ordering matters

The backtesting infrastructure should preserve chronological ordering so that these mistakes are harder to introduce.

# Walk-Forward Evaluation

A single OOS split tests one later period.

Walk-forward evaluation performs multiple OOS tests as time moves forward.

Conceptually:

```text
Train → Test
Train → Test
Train → Test
Train → Test
```

This allows a research procedure to be evaluated across multiple historical periods rather than relying on one test period.

## Rolling Walk-Forward

A rolling window keeps the training size fixed.

Example:

```text
TRAIN       TEST
A B C       D E

  TRAIN       TEST
  B C D       E F

    TRAIN       TEST
    C D E       F G
```

The training window moves forward while keeping approximately the same size.

This can be useful when older observations should eventually stop influencing the model.

## Expanding Walk-Forward

An expanding window keeps earlier observations while adding newer training data.

Example:

```text
TRAIN       TEST
A B C       D

TRAIN          TEST
A B C D        E

TRAIN             TEST
A B C D E         F
```

The training set grows as new historical information becomes available.

## Rolling vs Expanding

The difference is:

```text
Rolling:
fixed-size training window

Expanding:
growing training window
```

Neither is universally correct.

The appropriate choice depends on the research question and assumptions about how the underlying process changes through time.

## `walk_forward.py`

`walk_forward.py` provides the chronological splitting infrastructure.

The main concepts are:

```text
OutOfSampleSplit
rolling_walk_forward()
expanding_walk_forward()
```

`OutOfSampleSplit` represents one train/test split.

```text
train_data
test_data
```

The rolling and expanding functions generate sequences of these splits.

They should not contain a particular research model or strategy.

## Chronological Invariants

Every generated split must satisfy the following:

```text
training observations occur before testing observations
```

A walk-forward procedure must never produce:

```text
future training data
        ↓
past test data
```

The exact window boundaries must also be deterministic.

## Backtesting Architecture

The Chapter 12.8 architecture is:

```text
                 Historical ITCH
                       ↓
              historical_replay.py
                       ↓
                   OrderBook
                       ↓
                 BookSnapshot
                       ↓
                 BookDataset
                       ↓
                   Backtest
                       ↓
              Research Procedure
                       ↓
                    Results
```

For OOS research:

```text
BookDataset
     ↓
Chronological split
     ↓
Train / Test
     ↓
Research procedure
     ↓
Out-of-sample result
```

For walk-forward research:

```text
BookDataset
     ↓
┌─────────────────────────┐
│ Train → Test            │
│ Train → Test            │
│ Train → Test            │
│ Train → Test            │
└─────────────────────────┘
     ↓
Multiple OOS results
```

## Relationship to `experiment.py`

`backtest.py` and `walk_forward.py` perform the evaluation.

`experiment.py` records what was done and what was obtained.

The layers therefore work together:

```text
Data
 ↓
Backtest / Walk-Forward
 ↓
Results
 ↓
Experiment
 ↓
Saved JSON
```

For example, an experiment can record:

```text
dataset
parameters
training window
test window
model settings
results
```

This makes the research process easier to reproduce.

## Relationship to the C++ Core

The Python backtesting infrastructure does not replace the C++ market engine.

The responsibilities remain separated:

```text
C++
├── OrderBook
├── matching
├── order tracking
├── ITCH replay
├── execution
└── inventory

Python
├── snapshots
├── datasets
├── features
├── statistics
├── experiments
├── backtests
└── walk-forward analysis
```

C++ remains the authoritative implementation for the market mechanics already built there.

Python provides the research and evaluation layer above it.

## Scope

The current Chapter 12.8 implementation is intentionally simple.

It does not yet provide:

- a full trading-strategy framework
- realistic portfolio simulation
- broker integration
- multi-asset portfolio management
- transaction-cost optimization
- machine-learning model management
- distributed backtesting

Those capabilities should only be added when a concrete research project requires them.

## Goal of Chapter 12.8

The goal is not to build the world's largest backtesting framework.

The goal is to establish a clean research process:

```text
Historical data
      ↓
Reconstruct
      ↓
Observe
      ↓
Analyze
      ↓
Test chronologically
      ↓
Evaluate out-of-sample
      ↓
Walk forward through time
      ↓
Record results
```

This provides the methodological foundation for the empirical research performed later in the project.
