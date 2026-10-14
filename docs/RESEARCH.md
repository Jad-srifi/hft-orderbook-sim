# Quantitative Research Infrastructure

## 1. Purpose

The `python/research/` package provides reusable quantitative analysis functions for the Hybrid Limit Order Book Simulator.

It transforms book snapshots, execution results, inventory measurements, and other market observations into research variables and summaries.

The package supports:

- Basic market-microstructure measurements
- Feature extraction and changes between observations
- Execution analysis through the existing C++ execution API
- Descriptive statistics and time-series diagnostics
- Slippage analysis
- Inventory and P&L summaries
- Comparisons of market state before and after an observation interval

The purpose is to make research calculations modular and reusable rather than embedding them in a single script.

The package is a research layer, not a trading strategy, predictive model, or live trading system.

## 2. Architecture

The research package sits between the market-data infrastructure and the experiment, evaluation, and visualization layers.

```text
Historical ITCH
      |
      v
Historical Replay
      |
      v
C++ OrderBook
      |
      v
BookSnapshot
      |
      v
BookDataset
      |
      v
Research Modules
      |
      +----------------------+
      |                      |
      v                      v
Features / Statistics   Execution / Inventory
      |                      |
      +-----------+----------+
                  |
                  v
           Research Results
                  |
          +-------+-------+
          |               |
          v               v
      Experiment        Plots
          |
          v
     Saved Results
```

The main responsibilities are:

| Component                        | Responsibility                                                       |
| -------------------------------- | -------------------------------------------------------------------- |
| C++ core                         | Authoritative order-book, execution, and inventory calculations      |
| `observations.py`                | Captures immutable Python-owned book snapshots                       |
| `datasets.py`                    | Stores ordered snapshots                                             |
| `research/microstructure.py`     | Measures observable book properties                                  |
| `research/features.py`           | Calculates changes, returns, and feature dictionaries                |
| `research/execution_analysis.py` | Exposes the existing C++ execution-analysis functions                |
| `research/statistics.py`         | Provides descriptive statistical utilities                           |
| `research/slippage_analysis.py`  | Organizes slippage results for analysis                              |
| `research/inventory_analysis.py` | Exposes inventory measurements from the C++ model                    |
| `research/market_impact.py`      | Compares market-state measurements across observations               |
| `experiment.py`                  | Records experiment configuration, metadata, and results              |
| `backtests/`                     | Provides basic evaluation and chronological splitting infrastructure |
| `plots/`                         | Visualizes precomputed data and research outputs                     |

## 3. Package Structure

```text
python/
└── research/
    ├── __init__.py
    ├── microstructure.py
    ├── features.py
    ├── execution_analysis.py
    ├── statistics.py
    ├── slippage_analysis.py
    ├── inventory_analysis.py
    └── market_impact.py
```

The grouped research tests are located in:

```text
tests/python/test_research.py
```

Each module has a defined responsibility. Research calculations should not be duplicated across multiple modules simply because different experiments need the same variable.

## 4. Design Principles

### 4.1 Keep the C++ core authoritative

The C++ implementation remains responsible for the market mechanics already implemented there.

Python must not independently recreate the matching engine, order tracking, execution accounting, or inventory accounting.

The intended pattern is:

```text
Python research interface
          |
          v
      lob_cpp
          |
          v
       C++ core
```

This avoids maintaining two competing implementations of the same calculation.

### 4.2 Separate measurement from interpretation

A research function may calculate a spread, imbalance, return, or correlation. These quantities do not automatically establish an economic explanation.

For example:

- Positive order-book imbalance means that the selected bid-depth measure exceeds the selected ask-depth measure.
- Positive correlation indicates an observed linear association in the supplied sample.
- A change in midpoint after an event does not prove that the event caused the change.

Interpretation belongs in the research design and final analysis, not in the name of a utility function.

### 4.3 Keep functions reusable

The research modules should work with ordinary Python observations, sequences, mappings, and C++-bound objects where appropriate.

A research function should not require the full historical replay process when its input already contains the measurements it needs.

### 4.4 Maintain temporal alignment

Time-series research depends on matching observations to the correct times.

For example, a relationship between current imbalance and future return requires each imbalance observation to be aligned with the return measured over the intended future horizon.

The plotting and statistical functions cannot infer whether the alignment is economically correct. The calling research procedure must establish it.

## 5. Microstructure Measurements — `microstructure.py`

This module calculates basic properties of a `BookSnapshot`.

The current functions are:

```python
get_midpoint(snapshot)
get_spread(snapshot)
get_relative_spread(snapshot)
get_bid_depth(snapshot)
get_ask_depth(snapshot)
get_imbalance(snapshot)
```

### 5.1 Midpoint

The midpoint is the average of the best bid and best ask:

$$
M_t = \frac{B_t + A_t}{2}
$$

where:

- \(B_t\) is the best bid
- \(A_t\) is the best ask

If either quote is zero under the project's established empty-quote convention, the current research implementation returns `0.0`.

This is a project convention for handling an unavailable midpoint. It is not a market price observation and must not be treated as an ordinary zero-priced instrument.

### 5.2 Spread

The spread is the difference between the best ask and best bid:

$$
S_t = A_t - B_t
$$

The function returns the snapshot's existing spread value rather than reconstructing the order book.

The spread's units are the project's internal price units. Convert them to decimal currency units only when the specific price scale is known and the conversion is explicitly applied.

### 5.3 Relative Spread

Relative spread expresses spread relative to the midpoint:

$$
RS_t = \frac{S_t}{M_t} \times 100
$$

The current implementation returns a percentage.

If the midpoint is zero, it returns `0.0` under the project's missing-quote convention. Researchers should distinguish that fallback from a genuine zero relative spread.

### 5.4 Bid and Ask Depth

The current depth functions sum the quantities across all price levels contained in the snapshot:

$$
D_t^{bid} = \sum_i Q_{t,i}^{bid}
$$

$$
D_t^{ask} = \sum_i Q_{t,i}^{ask}
$$

These measurements describe the displayed quantities included in the snapshot. They do not automatically represent hidden liquidity or liquidity available across other venues.

### 5.5 Imbalance

The current imbalance definition is:

$$
I_t =
\frac{D_t^{bid}-D_t^{ask}}
{D_t^{bid}+D_t^{ask}}
$$

If total depth is zero, the current implementation returns `0.0`.

For a positive total depth, this measure lies between \(-1\) and \(+1\).

- Positive imbalance: more displayed bid depth than ask depth
- Negative imbalance: more displayed ask depth than bid depth
- Zero imbalance: equal displayed bid and ask depth, or the current zero-total-depth fallback

The imbalance measure depends on the depth definition. If the snapshot contains all stored levels, this is total stored depth rather than necessarily a top-of-book-only imbalance.

## 6. Feature Engineering — `features.py`

This module provides measurements derived from comparing two snapshots or from extracting variables from one snapshot.

The current functions are:

```python
midpoint_change(current, previous)
midpoint_return(current, previous)
spread_change(current, previous)
imbalance_change(current, previous)
extract_features(snapshot)
```

### 6.1 Midpoint Change

$$
\Delta M_t = M_t - M_{t-1}
$$

This is an absolute price change in the internal price units.

### 6.2 Midpoint Return

$$
R_t = \frac{M_t}{M_{t-1}} - 1
$$

If the previous midpoint is zero under the project convention, the function returns `0.0`.

Otherwise, it returns a decimal return. For example, `0.01` represents a 1% return.

A zero fallback should not be confused with an observed zero return. Research procedures may need to exclude observations for which the midpoint was unavailable.

### 6.3 Spread Change

$$
\Delta S_t = S_t - S_{t-1}
$$

Positive values indicate spread widening; negative values indicate spread narrowing.

### 6.4 Imbalance Change

$$
\Delta I_t = I_t - I_{t-1}
$$

This measures a change in the depth imbalance between the two supplied snapshots.

It does not identify the particular order additions, cancellations, or executions responsible for the change.

### 6.5 Feature Extraction

`extract_features(snapshot)` produces a dictionary containing:

```text
midpoint
spread
relative_spread
bid_depth
ask_depth
imbalance
```

The values are derived using the existing microstructure functions.

This provides a consistent research-facing representation of the basic snapshot variables.

## 7. Execution Analysis — `execution_analysis.py`

The execution-analysis module is a thin bridge to functions exposed through `lob_cpp`.

The current functions are:

```python
group_trades_by_incoming_order(trades)
calculate_executed_quantity(trades)
calculate_executed_value(trades)
calculate_VWAP(trades)
calculate_slippage(trades, side, reference_price)
calculate_execution_cost(trades, side, reference_price)
calculate_liquidity_consumed(trades)
calculate_execution_result(trades, side, reference_price)
calculate_execution_results_by_order(trades, sides, reference_prices)
```

These functions delegate the underlying execution calculations to C++.

### 7.1 Executed Quantity and Value

Executed quantity describes the total quantity filled across the supplied trades.

Executed value describes the total value of those executions, using the price and quantity represented by each trade.

The units depend on the project's price and quantity conventions.

### 7.2 Volume-Weighted Average Price

VWAP is:

$$
VWAP = \frac{\sum_i P_i Q_i}{\sum_i Q_i}
$$

where:

- \(P_i\) is an execution price
- \(Q_i\) is the associated executed quantity

Behavior for empty trade collections follows the existing C++ implementation. Research code should preserve those established semantics instead of independently defining a second VWAP implementation.

### 7.3 Slippage and Execution Cost

Slippage compares the execution outcome against a supplied reference price.

The reference price must be explicitly defined by the research procedure, for example, the arrival midpoint.

The current C++ convention treats positive slippage as unfavorable execution.

Execution cost is provided by the existing C++ API. Its numerical representation and rounding behavior are inherited from that implementation.

### 7.4 Grouping by Incoming Order

The grouping functions organize trades associated with their incoming orders. This allows partially filled orders to be analyzed as combined execution outcomes rather than treating each individual trade as a completely separate order-level observation.

The Python wrapper does not replace the underlying grouping or execution calculations.

## 8. Statistical Utilities — `statistics.py`

This module provides reusable descriptive statistics and basic time-series calculations using NumPy.

Its current functions are:

```python
mean(x)
median(x)
variance(x)
standard_deviation(x)
quantile(x, q)
correlation(x, y)
autocorrelation(x, lag)
describe(x, y=None, q=0.5, lag=1)
```

### 8.1 Mean and Median

The mean describes the arithmetic average.

The median describes the central observation under the usual ordering of the data, reducing sensitivity to extreme values relative to the mean.

The current implementation returns `0.0` for empty inputs.

### 8.2 Variance and Standard Deviation

The current implementation uses sample variance and sample standard deviation with `ddof=1`.

For observations \(x_1,\ldots,x_n\), the sample variance is:

$$
s^2 = \frac{1}{n-1}\sum_{i=1}^{n}(x_i-\bar{x})^2
$$

The current functions return `0.0` when fewer than two observations are provided.

### 8.3 Quantile

`quantile(x, q)` calculates the requested NumPy quantile.

The parameter \(q\) must lie between zero and one, inclusive. Empty input returns `0.0` under the current implementation.

For example, \(q=0.5\) requests the median quantile.

### 8.4 Correlation

`correlation(x, y)` calculates the Pearson correlation coefficient for two equal-length input sequences.

The current implementation returns `0.0` when:

- The sequences contain fewer than two observations
- Either sequence has zero standard deviation

Mismatched input lengths raise `ValueError`.

The zero fallback is a deliberate project convention. A returned zero in a degenerate case does not mean that the variables were statistically demonstrated to be uncorrelated.

### 8.5 Autocorrelation

`autocorrelation(x, lag)` compares a sequence with a lagged version of itself.

For lag \(k\), the current implementation computes the correlation between:

```text
x[:-k]
x[k:]
```

The lag must be at least one and smaller than the input length.

This is a basic lagged-correlation diagnostic. It should not be interpreted automatically as a complete time-series model or a formal test of serial dependence.

### 8.6 Descriptive Summary

`describe(x, y=None, q=0.5, lag=1)` returns a text summary containing the supported descriptive statistics and autocorrelation. When `y` is supplied, it also includes the correlation between `x` and `y`.

The current method assumes valid input for the requested quantile, lag, and any optional correlation calculation. Empty or very short inputs may trigger the underlying validation rules for some operations.

## 9. Slippage Analysis — `slippage_analysis.py`

This module organizes execution results for slippage-focused research.

The current functions are:

```python
extract_slippages(results)
mean_slippage(results)
median_slippage(results)
slippage_by_side(results_by_order, sides)
mean_slippage_by_side(results_by_order, sides)
slippage_by_quantity(results)
slippage_by_order(results_by_order)
```

### 9.1 Extracting Slippages

`extract_slippages(results)` converts the slippage values in a sequence of execution results into a list of floating-point values.

### 9.2 Mean and Median Slippage

`mean_slippage(results)` and `median_slippage(results)` apply the statistical helpers to the extracted slippage values.

Empty-input behavior follows the current statistics implementation.

### 9.3 Slippage by Side

`slippage_by_side(results_by_order, sides)` groups slippage by the side associated with each order identifier.

If a side is missing from the supplied mapping, that order is skipped by the current implementation.

`mean_slippage_by_side()` calculates the mean within each available side group.

The groups must be interpreted using the same slippage convention and reference-price methodology.

### 9.4 Slippage by Quantity and Order

`slippage_by_quantity(results)` returns pairs of executed quantity and slippage.

`slippage_by_order(results_by_order)` returns a mapping from order identifier to slippage.

These are analysis inputs, not evidence that slippage necessarily increases with quantity or differs systematically between BUY and SELL orders. Those questions require examination of the actual data.

## 10. Inventory Analysis — `inventory_analysis.py`

This module provides Python-facing access to the existing C++ `InventoryModel`.

The current functions are:

```python
get_position(inventory)
get_cash(inventory)
get_average_cost(inventory)
get_realized_pnl(inventory)
get_unrealized_pnl(inventory, mark_price)
get_portfolio_value(inventory, mark_price)
get_inventory_exposure(inventory, mark_price)
summarize_inventory(inventory, mark_price)
```

### 10.1 Position and Cash

Position represents the current inventory quantity.

Cash represents the cash balance maintained by the C++ inventory model.

### 10.2 Average Cost and P&L

Average cost describes the model's average acquisition cost according to its accounting implementation.

Realized P&L reports realized profit and loss.

Unrealized P&L evaluates the open position using the supplied mark price.

### 10.3 Portfolio Value and Inventory Exposure

Portfolio value and inventory exposure are returned by the existing C++ model using the supplied mark price.

The Python layer does not independently implement these accounting formulas.

### 10.4 Inventory Summary

`summarize_inventory(inventory, mark_price)` returns a dictionary containing:

```text
position
cash
average_cost
realized_pnl
unrealized_pnl
portfolio_value
inventory_exposure
```

The summary is useful as an input to experiments and plotting. Results are only comparable when mark prices, price units, and inventory-accounting conventions are consistent.

## 11. Market-State Comparison — `market_impact.py`

This module compares measurements from two `BookSnapshot` objects.

Its current functions are:

```python
bid_depth_change(current, previous)
ask_depth_change(current, previous)
summarize_market_impact(current, previous)
```

The summary contains:

```text
midpoint_change
midpoint_return
spread_change
bid_depth_change
ask_depth_change
imbalance_change
```

Depth changes are calculated by subtracting previous total displayed depth from current total displayed depth.

The other changes reuse functions from `features.py` and `microstructure.py`.

### Important limitation

The current module performs **market-state comparison**, not causal market-impact estimation.

If a midpoint rises after a large execution, the observed price change does not prove that the execution caused the movement. Other orders, cancellations, market-wide changes, or the arrival of new information may contribute.

A causal or event-study-style investigation requires an explicit research design, appropriate baselines, temporal alignment, and consideration of confounding effects.

## 12. Relationship to Experiments and Backtests

Research functions calculate measurements. They do not automatically define an experiment or establish out-of-sample validity.

A typical workflow is:

```text
BookDataset
     |
     v
Extract Features
     |
     v
Define Research Hypothesis
     |
     v
Calculate Outcomes
     |
     v
Statistical Analysis
     |
     v
Record Experiment
     |
     v
Evaluate Chronologically / OOS
     |
     v
Visualize Results
```

`experiment.py` records the experiment configuration, metadata, and results.

The `backtests/` package supplies basic procedure execution and chronological splitting infrastructure.

Plotting functions visualize the resulting measurements.

These responsibilities should remain separate. A utility function returning a statistic is not equivalent to an experiment that establishes a tested and reproducible research result.

## 13. Data Alignment and Leakage

Many research errors arise from incorrectly aligning variables rather than from incorrect formulas.

For example, a hypothesis that current imbalance predicts the midpoint return over the following 100 milliseconds requires:

1. A snapshot at the current time.
2. An imbalance computed from information available at that time.
3. A future midpoint observation at the intended horizon.
4. A return calculated from the appropriate starting and ending midpoint.
5. A clear policy for missing quotes, duplicate timestamps, and insufficient future observations.

The future return is an outcome. It must not be used to construct the current imbalance or a signal that supposedly existed at the starting time.

Research procedures must also avoid selecting parameters using the out-of-sample period that will later be reported as an independent evaluation.

The current feature utilities do not enforce a particular time horizon, purge overlapping labels, or implement a complete leakage-proof forecasting pipeline. Such controls must be implemented in the research procedure and evaluation design that use these utilities.

## 14. Testing

The research statistics module imports NumPy. The root `pyproject.toml` currently declares no runtime dependencies, so install NumPy if it is not already available:

```bash
python -m pip install numpy
```

The research tests also import `lob_cpp`; build the C++ extension and ensure it is importable before running the suite.

All current research tests are grouped in:

```text
tests/python/test_research.py
```

Run the grouped research tests:

```bash
python -m pytest tests/python/test_research.py
```

Run the entire Python suite:

```bash
python -m pytest tests/python
```

The current `tests/python/test_research.py` file contains **61 test functions** on the audited `main` branch. This is a source count, not a claim that the suite was executed during this documentation audit. Run the commands above in your configured environment to verify the current result.

Tests cover the current microstructure, feature, execution-analysis, statistics, slippage, inventory, and market-state-comparison functions.

A successful test run checks the implemented behavior against the test inputs. It does not prove that a research hypothesis is economically valid or that a reported relationship generalizes to unseen market data.

## 15. Scope and Limitations

The current research package is a collection of reusable analytical building blocks.

It does not automatically provide:

- A complete predictive modelling pipeline
- A general causal-inference framework
- A fully specified event-study methodology
- Statistical significance testing and confidence intervals for every output
- Multiple-hypothesis correction
- Automatic handling of overlapping future-return labels
- A complete trading-strategy or portfolio backtester
- Guaranteed leakage-free parameter selection
- Automatic interpretation of results

Those capabilities should be implemented when a specific research question requires them, with a clearly documented methodology.

## 16. Design Goal

The goal of the research package is to make quantitative investigations easier to implement without weakening the project's separation of concerns.

The intended progression is:

```text
Historical Market Data
         |
         v
Reliable Book Reconstruction
         |
         v
Snapshots and Datasets
         |
         v
Features and Measurements
         |
         v
Statistical Analysis
         |
         v
Controlled Research Experiments
         |
         v
Out-of-Sample Evaluation
         |
         v
Interpretation and Documentation
```

The package supplies reusable tools for this progression. The validity of the final conclusion still depends on the quality of the data, the research design, the statistical methodology, and the interpretation of the evidence.
