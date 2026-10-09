# Mathematical Foundations

## 1. Purpose

This document defines the mathematical concepts and conventions used by the Hybrid Limit Order Book Simulator.

It provides a reference for:

- Limit order book state and price-time priority
- Best bid, best ask, spread, and midpoint
- Displayed depth and order-book imbalance
- Midpoint changes and returns
- Trade value and volume-weighted average price
- Execution slippage and execution cost
- Inventory, exposure, and profit and loss
- Descriptive statistics and time-series diagnostics
- Historical snapshot sampling
- Out-of-sample evaluation and walk-forward analysis

The purpose is to keep mathematical definitions consistent across the C++ engine, Python research utilities, experiments, and visualizations.

The C++ implementation remains authoritative for calculations already implemented there. Where an edge case, rounding rule, or accounting convention is implementation-specific, this document distinguishes the general mathematical definition from the project's actual behavior.

---

## 2. Notation

The following notation is used throughout this document.

| Symbol        | Meaning                                                     |
| ------------- | ----------------------------------------------------------- |
| \(t\)         | Observation time or observation index, depending on context |
| \(B_t\)       | Best bid at observation \(t\)                               |
| \(A_t\)       | Best ask at observation \(t\)                               |
| \(M_t\)       | Midpoint at observation \(t\)                               |
| \(S_t\)       | Bid-ask spread at observation \(t\)                         |
| \(Q_i\)       | Quantity associated with order or trade \(i\)               |
| \(P_i\)       | Price associated with order or trade \(i\)                  |
| \(D_t^{bid}\) | Selected total displayed bid depth                          |
| \(D_t^{ask}\) | Selected total displayed ask depth                          |
| \(I_t\)       | Order-book imbalance                                        |
| \(R_t\)       | Midpoint return                                             |
| \(n\)         | Number of observations                                      |

In this project, price, quantity, timestamp, order identifier, and sequence-related types use the established C++ representations. Their numerical values should not be interpreted without accounting for their units.

## 3. Price Representation and Units

### 3.1 Integer prices

The C++ core stores prices as integer values rather than floating-point values.

The established convention is:

```text
Decimal price:       100.00
Internal price:      10000
```

Under this convention:

$$
P_{\text{decimal}} = \frac{P_{\text{internal}}}{100}
$$

For example:

$$
\frac{10000}{100} = 100.00
$$

This representation avoids introducing floating-point rounding into ordinary price comparisons and matching decisions.

The internal unit is a price unit equal to one hundredth of the decimal price unit. It should not be confused with the feed's raw integer encoding.

### 3.2 ITCH price conversion

Nasdaq ITCH 5.0 uses fixed-point price fields with four implied decimal places for the relevant message types.

For example, an ITCH raw price of:

$$
1{,}000{,}000
$$

represents:

$$
100.0000
$$

The project's mapper must convert protocol prices into the established internal representation using exact integer arithmetic whenever the supported precision permits it.

The conversion must not treat the raw ITCH integer as if it were already an internal C++ price.

If a source price cannot be represented exactly at the internal scale, the conversion policy must be explicit. Silent precision loss is not acceptable in precision-sensitive research.

### 3.3 Dimensional consistency

When an internal price is multiplied by quantity, the resulting value is in:

```text
internal price units × quantity units
```

It is not automatically a currency amount expressed in dollars.

A conversion to currency units requires the applicable price scale and quantity convention to be taken into account.

---

## 4. Limit Order Book State

A limit order specifies:

- An identifier
- A side
- A price
- A quantity

A buy order is part of the bid side. A sell order is part of the ask side.

Let \(Q*{p}^{bid}\) denote the total remaining displayed quantity at bid price \(p\). Similarly, let \(Q*{p}^{ask}\) denote the total remaining displayed quantity at ask price \(p\).

The displayed book aggregates orders by price level while preserving the established order-level tracking and FIFO rules.

### 4.1 Best bid

The best bid is the highest available bid price:

$$
B_t = \max\{p : p \text{ has displayed bid quantity}\}
$$

### 4.2 Best ask

The best ask is the lowest available ask price:

$$
A_t = \min\{p : p \text{ has displayed ask quantity}\}
$$

### 4.3 Empty quotes

The established C++ API represents an empty bid or ask with the value zero.

Consequently, a zero quote indicates unavailable book-side information under the project convention. It must not be automatically interpreted as a genuine market price of zero.

Functions that calculate midpoint or relative spread must account for this convention.

### 4.4 Price-time priority

The matching engine follows price-time priority.

For incoming orders:

- A buy order can match sell orders priced at or below its limit price.
- A sell order can match buy orders priced at or above its limit price.
- Better-priced resting orders have priority.
- At a given price level, earlier resting orders have FIFO priority.
- Trades execute at the resting order's price under the established matching rules.

Price-time priority describes matching behavior. It does not determine the exact sampling frequency of historical observations.

---

## 5. Spread and Midpoint

### 5.1 Bid-ask spread

The quoted spread is:

$$
S_t = A_t - B_t
$$

For example:

```text
Best bid = 10000
Best ask = 10010

Spread = 10010 - 10000 = 10 internal price units
```

The spread is measured using the project's internal integer price scale.

A wider spread indicates a larger gap between the best displayed buying and selling prices.

### 5.2 Midpoint

The midpoint is:

$$
M_t = \frac{B_t + A_t}{2}
$$

For example:

$$
M_t = \frac{10000 + 10010}{2} = 10005
$$

The midpoint can be fractional in internal price units when the best bid and ask differ by an odd number of internal units. The research layer represents midpoint as a floating-point value for this reason.

### 5.3 Relative spread

Relative spread expresses the spread as a percentage of the midpoint:

$$
RS_t = \frac{S_t}{M_t}\times100
$$

For example, if:

$$
S_t = 10,\qquad M_t = 10005
$$

then:

$$
RS_t \approx 0.09995\%
$$

The percentage form allows spreads to be compared across instruments or price levels more meaningfully than raw spread alone.

The current Python implementation returns zero when the midpoint is zero under the empty-quote convention. That fallback is not an observed zero relative spread.

---

## 6. Displayed Depth

Displayed depth measures the remaining quantity represented by selected price levels.

### 6.1 Bid depth

For a set of bid price levels \(\mathcal{B}\_t\):

$$
D_t^{bid} = \sum_{p\in\mathcal{B}_t} Q_p^{bid}
$$

### 6.2 Ask depth

For a set of ask price levels \(\mathcal{A}\_t\):

$$
D_t^{ask} = \sum_{p\in\mathcal{A}_t} Q_p^{ask}
$$

### 6.3 Interpretation

Suppose a snapshot contains:

```text
Bids:
10000 × 500
 9990 × 300
 9980 × 200

Asks:
10010 × 400
10020 × 250
10030 × 150
```

Then:

$$
D_t^{bid} = 500+300+200=1000
$$

$$
D_t^{ask} = 400+250+150=800
$$

These figures measure the displayed quantities in the supplied levels.

**Important:** depth depends on which levels are included. The current research functions sum all levels contained in the snapshot. If a future study uses top-five-level depth or a fixed price band, that restriction must be explicitly implemented and documented.

Displayed depth is not the same as total market liquidity. It excludes information not represented in the selected displayed book.

---

## 7. Order-Book Imbalance

The current research layer defines imbalance as:

$$
I_t =
\frac{D_t^{bid}-D_t^{ask}}
{D_t^{bid}+D_t^{ask}}
$$

This is a normalized measure comparing selected displayed bid and ask depth.

Using the preceding example:

$$
I_t = \frac{1000-800}{1000+800}
$$

$$
I_t \approx 0.1111
$$

Interpretation:

- \(I_t>0\): displayed bid depth exceeds displayed ask depth.
- \(I_t<0\): displayed ask depth exceeds displayed bid depth.
- \(I_t=0\): bid and ask depth are equal, provided total depth is positive.

For positive total depth, the measure lies between \(-1\) and \(+1\).

If total depth is zero, the current Python implementation returns `0.0`. This is a defined project fallback, not evidence of balanced positive liquidity.

Imbalance describes the observed book configuration. It does not establish that the midpoint will move in a particular direction.

---

## 8. Changes Between Observations

Let \(X*t\) denote a measurement at the current observation and \(X*{t-1}\) the corresponding measurement at the previous observation.

### 8.1 Midpoint change

$$
\Delta M_t = M_t-M_{t-1}
$$

This is an absolute price change, measured in internal price units.

### 8.2 Spread change

$$
\Delta S_t = S_t-S_{t-1}
$$

A positive value means the spread widened. A negative value means it narrowed.

### 8.3 Imbalance change

$$
\Delta I_t = I_t-I_{t-1}
$$

A positive value means the measured imbalance increased. It does not necessarily mean bid depth increased: the same result can occur if ask depth decreases.

### 8.4 Depth changes

Bid depth change:

$$
\Delta D_t^{bid}=D_t^{bid}-D_{t-1}^{bid}
$$

Ask depth change:

$$
\Delta D_t^{ask}=D_t^{ask}-D_{t-1}^{ask}
$$

These describe changes in total displayed depth. They do not identify the particular messages responsible for those changes.

---

## 9. Returns

Returns express price changes relative to an earlier price.

### 9.1 Simple midpoint return

The current feature implementation defines midpoint return as:

$$
R_t = \frac{M_t}{M_{t-1}}-1
$$

For example, if:

$$
M_{t-1}=10000,\qquad M_t=10010
$$

then:

$$
R_t=\frac{10010}{10000}-1=0.001
$$

This corresponds to a return of \(0.1\%\).

The Python function returns decimal returns rather than percentage returns. To express the result as a percentage:

$$
R_t^{\%}=100R_t
$$

### 9.2 Invalid or unavailable midpoint

The current implementation returns `0.0` when the previous midpoint is zero.

This is a project-level fallback. In empirical research, observations with unavailable quotes may instead need to be excluded or labelled missing so that they are not mistaken for genuine zero returns.

### 9.3 Log returns

A log return is defined mathematically as:

$$
r_t=\ln\left(\frac{M_t}{M_{t-1}}\right)
$$

Log returns are useful in various time-series and volatility analyses, but the current `midpoint_return()` function calculates simple returns, not log returns.

A log-return feature should be implemented explicitly if a research study requires it.

---

## 10. Trades, Executed Value, and VWAP

Let \(P_i\) be the execution price of trade \(i\) and \(Q_i\) its executed quantity.

### 10.1 Executed quantity

For a collection of trades:

$$
Q_{\text{exec}}=\sum_{i=1}^{n} Q_i
$$

This is the total quantity executed across the supplied trades.

### 10.2 Executed value

$$
V_{\text{exec}}=\sum_{i=1}^{n}P_iQ_i
$$

The result uses the project's internal price and quantity units. Conversion to currency units requires the applicable price scale.

### 10.3 Volume-weighted average price

VWAP weights each execution price by executed quantity:

$$
VWAP =
\frac{\sum_{i=1}^{n}P_iQ_i}
{\sum_{i=1}^{n}Q_i}
$$

provided that total executed quantity is positive.

Example:

|     Price | Quantity | Price × Quantity |
| --------: | -------: | ---------------: |
|     10000 |       10 |           100000 |
|     10100 |       20 |           202000 |
| **Total** |   **30** |       **302000** |

Therefore:

$$
VWAP = \frac{302000}{30}\approx10066.6667
$$

VWAP summarizes the average execution price weighted by volume. It does not reveal the order in which those executions occurred.

For empty trade collections or zero total quantity, behavior follows the established C++ execution API.

---

## 11. Execution Slippage

Slippage compares an execution outcome with a reference price.

The reference price must be defined by the research design. One possible benchmark is the midpoint observed when an order arrives.

Let:

- \(P\_{\text{ref}}\) be the reference price
- \(VWAP\) be the average execution price

The current C++ sign convention is:

### 11.1 Buy-order slippage

$$
Slippage_{buy}=VWAP-P_{\text{ref}}
$$

Positive values indicate that the execution occurred above the reference price and was unfavorable to the buyer.

### 11.2 Sell-order slippage

$$
Slippage_{sell}=P_{\text{ref}}-VWAP
$$

Positive values indicate that the execution occurred below the reference price and was unfavorable to the seller.

### 11.3 Example

Suppose a buy order has:

$$
P_{\text{ref}}=10000,\qquad VWAP=10020
$$

Then:

$$
Slippage_{buy}=10020-10000=20
$$

The order's VWAP is 20 internal price units above the reference price.

Slippage is reference-dependent. Results measured against arrival midpoint are not interchangeable with results measured against the arrival ask, a later midpoint, or another benchmark.

### 11.4 Execution cost

The basic slippage-based execution-cost relationship is:

$$
Cost = Slippage \times Q_{\text{exec}}
$$

where \(Q\_{\text{exec}}\) is executed quantity.

The project's actual result uses the numerical type and rounding behavior of the C++ execution implementation. In particular, the current C++ execution-cost representation may truncate fractional costs.

Researchers should not assume that the reported cost retains arbitrary floating-point precision.

This cost also represents only the defined slippage-based component. It does not automatically include commissions, exchange fees, financing, or other trading costs.

---

## 12. Inventory, Mark-to-Market, and P&L

The C++ `InventoryModel` is authoritative for inventory accounting in this project.

The Python inventory-analysis layer exposes the resulting measurements:

```text
position
cash
average cost
realized P&L
unrealized P&L
portfolio value
inventory exposure
```

### 12.1 Position

Let \(q_t\) be the signed inventory position.

Under the usual convention:

- \(q_t>0\): long inventory
- \(q_t<0\): short inventory
- \(q_t=0\): flat inventory

The exact updates are determined by executed trades and the existing C++ inventory implementation.

### 12.2 Mark-to-market

The purpose of mark-to-market accounting is to value the existing position at a specified market price.

Let \(M_t\) be the chosen mark price. This may be the midpoint, but the marking convention must be explicit.

A common signed market-value representation is:

$$
Exposure_t=q_tM_t
$$

This convention makes positive exposure correspond to a long position and negative exposure correspond to a short position. The project's `inventory_exposure()` method remains authoritative for its actual definition.

### 12.3 Unrealized P&L

Unrealized P&L represents the mark-to-market gain or loss associated with the position that remains open.

For a simple position with a compatible average-cost convention, the usual relationship is:

$$
UPnL_t=q_t(M_t-\overline{P}_t)
$$

where \(\overline{P}\_t\) represents the applicable average cost.

The expression is illustrative of the standard signed-position convention. The exact formula, treatment of short positions, and behavior when a position crosses through zero are determined by the existing C++ inventory-accounting implementation.

### 12.4 Realized P&L

Realized P&L records gains and losses recognized by the inventory model when inventory is closed or reduced, according to its accounting rules.

Its value depends on the executed trade sequence and the model's cost-basis convention.

It is not generally equal to total cash change alone, because cash changes also depend on the value of the purchases and sales that establish the position.

### 12.5 Portfolio value

Portfolio value combines the model's cash balance with the value of its inventory at a specified mark.

A common marked-portfolio identity is:

$$
V_t=Cash_t+q_tM_t
$$

The current C++ `portfolio_value()` method defines the actual project behavior.

### 12.6 Important distinction

These quantities answer different questions:

- **Position:** how many units are held?
- **Cash:** what cash balance does the model record?
- **Realized P&L:** what gains and losses have been realized?
- **Unrealized P&L:** what is the marked gain or loss on open inventory?
- **Exposure:** what value is associated with the current position?
- **Portfolio value:** what is the marked value of cash and inventory under the chosen accounting convention?

They should not be treated as interchangeable.

---

## 13. Descriptive Statistics

The Python research module uses NumPy to calculate basic statistics.

Let \(x_1,x_2,\ldots,x_n\) be observations.

### 13.1 Mean

$$
\bar{x}=\frac{1}{n}\sum_{i=1}^{n}x_i
$$

The mean is the arithmetic average.

### 13.2 Median

The median is the middle ordered observation, or the average of the two central observations when the sample size is even.

It is less sensitive to extreme values than the mean in many distributions.

### 13.3 Sample variance

The current implementation uses sample variance with `ddof=1`:

$$
s^2=\frac{1}{n-1}\sum_{i=1}^{n}(x_i-\bar{x})^2
$$

This denominator differs from the population-variance denominator \(n\).

### 13.4 Sample standard deviation

$$
s=\sqrt{s^2}
$$

Standard deviation expresses dispersion in the same units as the original observations.

### 13.5 Quantiles

The quantile function estimates a value below which a specified fraction of observations falls.

For a requested quantile \(q\):

$$
0\le q\le1
$$

The implementation delegates the numerical calculation to `numpy.quantile()`. The exact estimate therefore follows NumPy's quantile convention.

The median corresponds to \(q=0.5\).

### 13.6 Empty and undersized samples

The current project implementation returns `0.0` for:

- Mean and median of empty inputs
- Variance and standard deviation with fewer than two observations
- Quantile of empty input
- Correlation with fewer than two observations or a zero-variance input

These are implementation fallbacks, not general mathematical identities.

In particular, a zero returned for an undersized sample should not be interpreted as proof that the underlying quantity is genuinely zero.

---

## 14. Correlation and Autocorrelation

### 14.1 Pearson correlation

For two equal-length, non-degenerate sequences \(x\) and \(y\), Pearson correlation is:

$$
\rho_{xy}=
\frac{\sum_{i=1}^{n}(x_i-\bar{x})(y_i-\bar{y})}
{\sqrt{\sum_{i=1}^{n}(x_i-\bar{x})^2}
 \sqrt{\sum_{i=1}^{n}(y_i-\bar{y})^2}}
$$

Its value lies between \(-1\) and \(+1\) for valid non-degenerate inputs.

- Positive correlation indicates that higher observations of one variable tend to occur with higher observations of the other.
- Negative correlation indicates an inverse linear association.
- A value near zero indicates little linear association in the observed sample, not necessarily the absence of every possible relationship.

The current function is a descriptive statistic. It does not calculate a p-value, confidence interval, or formal hypothesis test.

### 14.2 Lagged correlation

The current autocorrelation function compares a sequence with a lagged version of itself:

$$
Corr(x_{1},\ldots,x_{n-k},
     x_{1+k},\ldots,x_n)
$$

More precisely, the Python implementation calculates the Pearson correlation between:

```text
x[:-lag]
x[lag:]
```

This is a lagged pairwise-correlation calculation. It does not implement every possible time-series autocorrelation convention or estimator.

The requested lag must be at least one and smaller than the sequence length.

### 14.3 Interpretation limitations

Correlation does not establish causality.

For market data, observations may also be serially dependent, heteroskedastic, or influenced by common events. Standard independent-observation interpretations should not be applied automatically to such data.

---

## 15. Empirical Distributions

A distribution describes how observed values are spread across their possible range.

The plotting utilities provide histograms, box plots, and empirical cumulative distributions.

### 15.1 Histogram

A histogram divides the value range into bins and counts observations within each bin.

The observed shape depends on the bin count and bin boundaries. Different histogram configurations can make the same sample look different.

### 15.2 Empirical cumulative distribution

Let the sorted sample be:

$$
x_{(1)}\le x_{(2)}\le\cdots\le x_{(n)}
$$

The current cumulative-distribution plotting function assigns the probability level:

$$
F_n(x_{(i)})=\frac{i}{n}
$$

for the \(i\)-th sorted observation.

This is an empirical representation of the sample, not a fitted theoretical probability distribution.

### 15.3 Box plots

Box plots summarize selected distribution characteristics, including the median and quartiles. Their whiskers and treatment of outliers follow the plotting library's configuration.

A box plot does not replace examination of sample size, tails, or multimodality.

---

## 16. Historical Time and Sampling

Historical replay processes the source stream in its authoritative order.

A `BookSnapshot` captures the observable order-book state at a selected point.

The plotting/player layer uses fields such as:

```text
timestamp_ms
message_index
```

### 16.1 Timestamp conversion

For an ITCH timestamp expressed in nanoseconds since midnight:

$$
t_{ms}=\left\lfloor\frac{t_{ns}}{10^6}\right\rfloor
$$

This conversion loses sub-millisecond precision. The raw timestamp should be preserved whenever a research design needs the original precision.

### 16.2 Message-based sampling

If a snapshot is captured every \(k\) processed operations, the sampling rule is based on the operation count.

It does not imply equal elapsed time between snapshots.

### 16.3 Time-based sampling

If snapshots are sampled at fixed time intervals, the sampling rule depends on historical timestamps.

Because multiple events can occur within the same millisecond, a time-based policy must define how it selects the state associated with each sampling time.

For instance, a causal state sample should normally reflect the last processed book state available at or before the sampling time, rather than a state created by a later event.

### 16.4 Do not sort by timestamp alone

Historical replay must preserve source-stream order. Equal or rounded timestamps do not justify reordering messages.

This is necessary for deterministic book reconstruction and valid temporal analysis.

---

## 17. Out-of-Sample Evaluation

Out-of-sample (OOS) evaluation tests a procedure on data not used to develop or calibrate it.

Suppose observations are arranged chronologically:

$$
x_1,x_2,\ldots,x_n
$$

For a training size \(a\) and test size \(b\), a basic split is:

$$
Train=(x_1,\ldots,x_a)
$$

$$
Test=(x_{a+1},\ldots,x_{a+b})
$$

The basic chronological requirement is:

$$
\max(TrainTime)<\min(TestTime)
$$

when training and test observations have strictly ordered timestamps and no overlapping time boundaries.

This prevents test observations from being placed into the training set. It does not, by itself, prevent every type of leakage: features and labels must also be aligned correctly, and parameter selection must not use the final test results.

### 17.1 Rolling walk-forward

A rolling approach keeps a fixed training size while moving forward through the dataset.

For example:

```text
Train: A B C     Test: D
Train: B C D     Test: E
Train: C D E     Test: F
```

### 17.2 Expanding walk-forward

An expanding approach keeps historical training observations and adds newer ones as time advances:

```text
Train: A B C       Test: D
Train: A B C D     Test: E
Train: A B C D E   Test: F
```

The appropriate approach depends on the research question and assumptions about the stability of the data-generating process.

### 17.3 What OOS does not guarantee

OOS evaluation does not guarantee profitability, statistical significance, causality, or generalization to every market regime.

Repeatedly adjusting a model after inspecting the same OOS period compromises its independence as a final test.

---

## 18. Relationship to the Project Modules

The principal mathematical operations are implemented across the existing architecture.

| Module                           | Mathematical responsibility                                     |
| -------------------------------- | --------------------------------------------------------------- |
| C++ OrderBook                    | Price levels, order quantities, and market state                |
| C++ matching engine              | Price-time matching and trade generation                        |
| C++ execution API                | VWAP, slippage, execution cost, and liquidity measurements      |
| C++ InventoryModel               | Position and P&L accounting                                     |
| `research/microstructure.py`     | Midpoint, spread, relative spread, depth, imbalance             |
| `research/features.py`           | Changes, returns, and feature extraction                        |
| `research/statistics.py`         | Descriptive statistics, quantiles, correlation, autocorrelation |
| `research/slippage_analysis.py`  | Slippage summaries and grouping                                 |
| `research/inventory_analysis.py` | Inventory measurements and summaries                            |
| `research/market_impact.py`      | Comparisons of market-state measurements                        |
| `backtests/walk_forward.py`      | Chronological train/test-window generation                      |
| `plots/`                         | Visualization of measurements and outputs                       |

The same mathematical definition should not be independently reimplemented in several places unless an explicit comparison or alternative method is being tested.

## 19. Numerical and Research Limitations

Several conventions matter when interpreting results.

### Internal units

Prices use the established integer scale, while midpoint and some research calculations use floating-point values. Results must be converted to meaningful decimal or currency units before interpretation.

### Fallback values

Some current functions return zero for missing quotes, empty sequences, or undersized statistical samples. These values should not automatically be treated as valid market observations.

### Rounding

The C++ execution-cost result may truncate fractional values according to its current numeric representation. Research reports should reflect the actual implementation.

### Sampling

Message-based and time-based sampling are not equivalent. Sampling changes can affect measured spread, depth, imbalance, return, and volatility distributions.

### Statistical dependence

Market observations are not automatically independent. Correlations and descriptive statistics alone do not supply a complete inference procedure.

### Causality

A change in a measurement after another event does not prove that the first event caused the change.

### OOS discipline

A chronological split is necessary for time-series validation, but correct feature timing, outcome alignment, and parameter-selection discipline are also required.

## 20. Mathematical Design Goal

The goal of this document is to provide consistent definitions for the project's engine, research utilities, and experiments.

The intended workflow is:

```text
Historical Events
       |
       v
Reconstructed Market State
       |
       v
Snapshot Measurements
       |
       v
Feature Construction
       |
       v
Statistical Analysis
       |
       v
Controlled Experiment
       |
       v
Out-of-Sample Evaluation
       |
       v
Research Interpretation
```

The mathematical utilities establish how a quantity is calculated. They do not, by themselves, establish that a hypothesis is true.

A defensible research conclusion requires accurate inputs, consistent units, correctly aligned observations, appropriate statistical methods, and transparent discussion of limitations.
