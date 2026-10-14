# Plotting and Visualization

## 1. Purpose

The `python/plots/` package provides visualization utilities for the Hybrid Limit Order Book Simulator and its quantitative research layer.

Its purpose is to transform already-calculated market observations, research features, execution measurements, and inventory results into visual representations.

The package supports:

- Order-book depth visualization
- Bid-ask imbalance analysis
- Spread and relative spread visualization
- Midpoint and return visualization
- Inventory and P&L visualization
- Execution slippage visualization
- Execution metrics visualization
- Distribution analysis
- Basic historical snapshot playback control

The plotting package is a consumer of the research infrastructure. It should not duplicate the C++ matching engine, reconstruct ITCH messages, or independently implement research calculations that belong to other modules.

## 2. Architecture

The visualization layer sits above the existing research infrastructure.

```text
C++ OrderBook / ITCH Replay
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
              v
      Calculated Results
              |
              v
       Python Plotting
              |
              v
       Figures / Charts
```

The architectural responsibilities are:

| Component              | Responsibility                                                             |
| ---------------------- | -------------------------------------------------------------------------- |
| C++ core               | Authoritative order-book, matching, execution, inventory, and replay logic |
| `python/backtests/historical_replay.py` | High-level historical ITCH replay and snapshot collection |
| `python/observations.py` | Creates immutable Python-owned book snapshots |
| `python/datasets.py` | Stores ordered book snapshots |
| `python/research/` | Calculates features, statistics, and research measurements |
| `python/plots/` | Visualizes snapshots and already-calculated measurements |
| `webapp/` | Separate Flask dashboard, including the browser replay interface |

The plotting modules should not modify the source snapshots or market state.

## 3. Package Structure

The current plotting package contains:

```text
python/
└── plots/
    ├── __init__.py
    ├── book_depth.py
    ├── imbalance.py
    ├── inventory.py
    ├── slippage.py
    ├── spread.py
    ├── midpoint.py
    ├── execution.py
    ├── distributions.py
    └── replay.py
```

Grouped tests are maintained in:

```text
tests/python/test_plots.py
```

The test suite covers the plotting functions and replay-player controller.

## 4. Design Principles

### 4.1 Visualization only

Plotting functions receive the values they need and visualize them.

For example, `plot_spread()` receives spread observations. It does not replay ITCH messages or calculate spreads from the raw feed.

The intended separation is:

```text
Raw market data
      |
      v
Historical reconstruction
      |
      v
Snapshots / datasets
      |
      v
Research calculations
      |
      v
Plotting functions
```

### 4.2 Return figures instead of displaying them automatically

Most plotting functions return:

```python
fig, ax
```

The inventory summary returns:

```python
fig, axes
```

This lets callers decide whether to display, save, embed, or close a figure.

Example:

```python
import matplotlib.pyplot as plt

from plots.midpoint import plot_midpoint

fig, ax = plot_midpoint([10000, 10005, 10002, 10010])
fig.savefig("midpoint.png", dpi=150, bbox_inches="tight")
plt.close(fig)
```

The plotting functions do not call `plt.show()` automatically.

### 4.3 Keep research calculations elsewhere

The plotting layer should consume calculations from modules such as:

```text
research/microstructure.py
research/features.py
research/execution_analysis.py
research/slippage_analysis.py
research/inventory_analysis.py
research/market_impact.py
research/statistics.py
```

This avoids duplicating analytical logic between research and visualization code.

### 4.4 Aligned observations

When a function compares two series, the caller must supply correctly aligned observations.

For example, `plot_imbalance_vs_return(imbalances, returns)` expects each imbalance to correspond to the appropriate future return under the research design.

A plot by itself does not establish causation or prove that a feature predicts future movement.

## 5. Order-Book Depth — `book_depth.py`

This module visualizes displayed liquidity in the order book.

### `plot_bid_ask_depth(snapshot)`

Displays the bid and ask quantities by price level for one `BookSnapshot`.

The snapshot contains levels represented as:

```text
(price, total_quantity)
```

Bid levels use highest-to-lowest price ordering, and ask levels use lowest-to-highest price ordering.

### `plot_depth_by_level(snapshot)`

Compares quantities across book levels.

It is useful for examining how displayed depth changes as prices move away from the best bid or best ask.

### `plot_total_depth(snapshots)`

Calculates the total displayed bid and ask quantities for each supplied snapshot and plots both series against observation index.

### `plot_bid_ask_depth_change(snapshots)`

Plots the change in total bid depth and total ask depth between consecutive snapshots.

The first observation is assigned a depth change of zero because no previous snapshot was supplied.

This measures changes in displayed depth. It does not independently identify whether those changes were caused by new orders, cancellations, executions, or other market events.

## 6. Imbalance — `imbalance.py`

This module visualizes order-book imbalance and its relationship with other observations.

### `plot_imbalance(imbalances)`

Plots imbalance over observation order and includes a zero reference line.

The imbalance definition used by the research layer is:

$$
I_t = \frac{D_t^{bid}-D_t^{ask}}{D_t^{bid}+D_t^{ask}}
$$

where \(D_t^{bid}\) and \(D_t^{ask}\) represent the selected bid and ask depth measures.

A positive value indicates more displayed bid depth than ask depth under that definition. A negative value indicates the opposite.

### `plot_bid_ask_depth(bid_depth, ask_depth)`

Compares precomputed bid and ask depth series.

This function has the same name as `book_depth.plot_bid_ask_depth`, but it accepts two series instead of one snapshot. Import it from the intended module to avoid ambiguity.

### `plot_imbalance_distribution(imbalances)`

Creates a histogram of imbalance observations.

It helps reveal concentration near zero, skew, and periods with unusually one-sided displayed liquidity.

### `plot_imbalance_vs_return(imbalances, returns)`

Creates a scatter plot comparing imbalance with a corresponding return series.

The return horizon and alignment must be determined by the research procedure. The plot is descriptive; it does not establish a causal relationship.

## 7. Spread — `spread.py`

This module visualizes the bid-ask spread.

### `plot_spread(spreads)`

Plots the spread across observations.

### `plot_relative_spread(relative_spreads)`

Plots relative spread values. The current research function `get_relative_spread()` returns this measure as a percentage, so its values should be supplied in the same units.

### `plot_spread_distribution(spreads)`

Creates a histogram of spread observations.

These plots help examine spread stability, variability, and changes in displayed transaction-cost conditions.

## 8. Midpoint — `midpoint.py`

This module visualizes midpoint prices and returns.

### `plot_midpoint(midpoints)`

Plots midpoint observations over their supplied order.

### `plot_midpoint_returns(returns)`

Plots a sequence of precomputed midpoint returns.

The research feature `midpoint_return()` returns a decimal return rather than a percentage. For example, `0.01` represents a 1% return.

### `plot_midpoint_distribution(midpoints)`

Creates a histogram of midpoint levels.

### `plot_return_distribution(returns)`

Creates a histogram of return observations.

Midpoint-level distributions and return distributions answer different questions. Price levels describe where the market traded; returns describe price changes relative to earlier observations.

## 9. Inventory — `inventory.py`

This module visualizes inventory accounting outputs.

### `plot_position(position)`

Plots inventory position over observations.

### `plot_inventory_exposure(exposure)`

Plots the supplied inventory-exposure series.

### `plot_realized_pnl(realized_pnl)`

Plots realized profit and loss.

### `plot_unrealized_pnl(unrealized_pnl)`

Plots unrealized profit and loss based on previously calculated inventory marks.

### `plot_portfolio_value(portfolio_value)`

Plots the supplied portfolio-value series.

### `plot_inventory_summary(position, realized_pnl, unrealized_pnl, portfolio_value)`

Creates a four-panel summary figure containing position, realized P&L, unrealized P&L, and portfolio value.

The individual plotting functions return a figure and one axis. The summary function returns the figure and an array of four axes.

Inventory calculations remain in the C++ `InventoryModel` and the Python inventory-analysis layer. These functions only display the resulting values.

## 10. Slippage — `slippage.py`

This module visualizes execution slippage measurements.

### `plot_slippage(slippages)`

Plots slippage over the supplied observation order.

### `plot_slippage_vs_quantity(quantities, slippages)`

Creates a scatter plot comparing executed quantity and slippage.

### `plot_slippage_by_side(buy_slippages, sell_slippages)`

Compares supplied BUY and SELL slippage series.

The observations must be aligned meaningfully for the comparison being made. The two series do not need to be interpreted as paired trades unless the caller constructed them that way.

### `plot_slippage_distribution(slippages)`

Creates a histogram of slippage observations.

Slippage conventions must be consistent with the execution-analysis layer. In the current C++ execution API, positive slippage represents unfavorable execution relative to the specified reference price.

## 11. Execution — `execution.py`

This module visualizes execution-analysis outputs.

### `plot_vwap(vwaps)`

Plots volume-weighted average execution prices.

### `plot_vwap_vs_reference(vwaps, reference_prices)`

Plots VWAP and reference-price series together.

The reference-price definition should be documented by the experiment using the chart, such as arrival midpoint or another predefined benchmark.

### `plot_execution_cost(execution_costs)`

Plots precomputed execution-cost values.

The interpretation and units depend on the underlying execution-cost calculation. The plot does not recalculate those values.

### `plot_executed_quantity(executed_quantities)`

Plots executed quantity across observations or incoming orders, according to the supplied series.

### `plot_executed_value(executed_values)`

Plots executed value.

### `plot_liquidity_consumed(liquidity_consumed)`

Plots the supplied liquidity-consumption measurements.

All execution calculations remain in the existing C++ execution API and its Python wrapper. This module only renders those calculations.

## 12. Distribution Utilities — `distributions.py`

This module provides generic distribution charts that can be reused for different research variables.

### `plot_distribution(values, title="Distribution", xlabel="Value", bins=30)`

Creates a histogram using the given values, title, x-axis label, and number of bins.

### `plot_comparison_distribution(first, second, first_label="First", second_label="Second", bins=30)`

Overlays two histograms so their distributions can be compared.

The labels identify the two input series.

### `plot_box_distribution(values, labels=None)`

Creates a box plot for one or more groups of observations.

The implementation uses Matplotlib's `tick_labels` parameter for category labels.

### `plot_cumulative_distribution(values)`

Sorts the observations and plots their empirical cumulative probabilities.

For \(n\) sorted observations, the cumulative probability assigned to observation \(i\), using one-based indexing, is:

$$
F_n(x_i) = \frac{i}{n}
$$

The current implementation expects a non-empty input sequence.

These utilities can be used with return, spread, imbalance, slippage, or other numeric series. Their inputs should already be prepared by the relevant research module.

## 13. Replay Controller — `replay.py`

The replay module provides a small state controller for stepping through an existing sequence of `BookSnapshot` objects.

It is not the ITCH parser or replay engine, and it does not run a Flask server.

### `ReplayPlayer`

`ReplayPlayer` stores:

```text
snapshots
interval_ms
index
```

The current snapshot is available through `current`.

The main properties are:

- `total`: number of snapshots
- `finished`: whether the current position is at the final snapshot, or the dataset is empty

The main methods are:

- `step_forward()`: advance by one snapshot, stopping at the last snapshot
- `step_backward()`: move back by one snapshot, stopping at the first snapshot
- `reset()`: return to the first snapshot
- `seek(index)`: jump to a particular snapshot index
- `set_interval(interval_ms)`: update the playback interval setting

The playback interval is a presentation setting in milliseconds. In this controller alone, it does not schedule automatic playback or change the timestamps contained in historical snapshots.

### `create_replay(snapshots, interval_ms=100)`

Creates a `ReplayPlayer` from an iterable of snapshots.

### `replay_frame(snapshot)`

Converts a snapshot into a dictionary containing:

```text
timestamp_ms
message_index
best_bid
best_ask
spread
midpoint
bid_levels
ask_levels
```

This dictionary is convenient for later serialization to a browser interface.

### `replay_state(player)`

Returns the current playback index, snapshot count, completion state, interval, and serialized current snapshot.

The module provides the state-control foundation for a future interactive viewer. Play/pause scheduling, ITCH-file selection, replay configuration, HTTP routes, and browser rendering belong in the application layer, not in this module.

## 14. Timestamps and Message Indices

`BookSnapshot` includes:

```text
timestamp_ms
message_index
```

These fields are intended to identify the snapshot's historical time and position in the source message stream.

A manually created snapshot may still have default values of zero. Such a snapshot has no meaningful historical timestamp unless one is explicitly provided.

For replay-derived snapshots, timestamp conversion must follow the units exposed by the existing ITCH parser. Message indices should represent source-stream position, not wall-clock processing time.

Snapshots should remain in original stream order. They should not be reordered merely because timestamps are equal or because a visual plot would look smoother.

## 15. Dependencies

The Python plotting modules use Matplotlib to create static figures. The current Flask dashboard declares Flask and Matplotlib in `webapp/requirements.txt`; the browser replay interface uses the repository's HTML, CSS, and JavaScript rather than Plotly.

Install the dashboard's declared dependencies from the repository root:

```bash
python -m pip install -r webapp/requirements.txt
```

The root `pyproject.toml` currently declares no runtime dependencies. If you use `python/plots/` outside the dashboard environment, ensure Matplotlib is installed. The browser dashboard and the Python plotting package are separate layers.

## 16. Testing

All plotting tests are grouped in:

```text
tests/python/test_plots.py
```

The tests verify that plotting functions can create figures and axes using representative inputs. Replay-controller tests cover creation, stepping, resetting, seeking, changing the interval, frame serialization, and empty replay behavior.

For headless testing, the test module selects Matplotlib's `Agg` backend:

```python
import matplotlib
matplotlib.use("Agg")
```

Figures are explicitly closed with `plt.close(fig)` to avoid accumulating open figures during the test run.

Run the plotting tests:

```bash
python -m pytest tests/python/test_plots.py
```

Run the complete Python test suite:

```bash
python -m pytest tests/python
```

A successful plot test indicates that the functions can construct their figures and return the expected objects for the tested inputs. It does not by itself validate the scientific interpretation, market-data alignment, or economic significance of a chart.

## 17. Current Scope and Limitations

The Python plotting modules provide static chart construction and a basic snapshot replay controller. The repository separately includes a current Flask dashboard under `webapp/`; its `/replay` page provides browser playback of a bounded, server-generated snapshot sequence. The Python `ReplayPlayer` and the browser replay page are different implementations and should not be conflated.

The current end-to-end browser workflow is:

```text
Load ITCH file
      |
      v
Configure Stock Locate
      |
      v
Replay historical messages
      |
      v
Sample timestamped snapshots
      |
      v
Create or load a snapshot dataset
      |
      v
Control playback in the browser
      |
      v
Inspect synchronized charts
```

The future interface may include:

- ITCH-file selection
- Stock Locate selection
- Processed, replayed, and ignored message counters
- Message-based snapshot sampling
- Time-based snapshot sampling
- Play, pause, stop, and single-step controls
- A timeline and replay-speed setting
- Order-book depth, imbalance, spread, and midpoint charts
- Additional execution or inventory charts when the corresponding data exists

For large ITCH files, the application should replay and sample the stream rather than hold every raw message or every possible book state in browser memory.

The actual replay-sampling behavior must be explicitly defined and tested. Message-based sampling and time-based sampling are different policies and should not be treated as interchangeable.

## 18. Design Goal

The goal of `python/plots/` is to let users inspect market state and research outputs without putting visualization logic into the C++ engine or duplicating calculations from the research modules. The separate Flask dashboard in `webapp/` provides the browser-based interface.

The intended direction is:

```text
Historical Market Data
         |
         v
Historical Reconstruction
         |
         v
Timestamped Snapshots
         |
         v
Research Features and Metrics
         |
         v
Plotting and Playback
         |
         v
Interactive Research Dashboard
```

The plotting package should remain modular, testable, and separate from market mechanics. Its static charts remain reusable by Python research workflows, while the Flask dashboard provides a separate browser-based interface.
