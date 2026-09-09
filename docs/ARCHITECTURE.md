# Hybrid C++/Python Limit Order Book Simulator

# Chapter 1–2 — Order Book & Matching Engine Architecture

## 1. Overview

This project is a hybrid C++/Python limit order book simulator designed to model financial market microstructure.

Chapter 1 establishes the fundamental data model of the limit order book. It implements orders, price levels, bid and ask sides, best-price queries, spread calculation, and order cancellation.

Chapter 2 extends this foundation with a matching engine capable of processing crossing orders and generating trades according to price-time priority.

The implementation at this stage prioritizes correctness, clear data flow, and testability before introducing more advanced data structures and performance optimizations.

The current architecture is:

```text
Order
  ↓
PriceLevel
  ↓
OrderBook
  ↓
Matching Engine
  ↓
Trade
```

The C++ implementation will serve as the performance-critical core of the project, while Python will later be used for research, analysis, visualization, and quantitative experimentation.

---

## 2. Core Data Model

### 2.1 Order

An `Order` represents a single order submitted to the market.

Each order contains:

* `OrderId` — unique identifier for the order
* `Side` — BUY or SELL
* `Price` — integer price representation
* `Quantity` — number of units currently remaining

Conceptually:

```text
Order

├── ID
├── Side
├── Price
└── Quantity
```

For example:

```text
Order ID: 10
Side:     BUY
Price:    8150
Quantity: 200
```

The order represents a request to buy 200 units at a price of 8150 ticks.

During matching, `Quantity` represents the order's remaining unfilled quantity.

---

## 3. Integer Price Representation

Prices are represented using integers rather than floating-point values.

For example:

```text
$100.00 → 10000
$100.25 → 10025
$105.50 → 10550
```

This avoids floating-point precision problems.

The project therefore defines:

```cpp
using Price = std::int64_t;
```

Using integer ticks also provides a cleaner foundation for later high-performance market simulation.

---

## 4. Price Level

A `PriceLevel` represents all orders currently resting at the same price on the same side of the book.

Each price level contains:

```text
PriceLevel

├── price
├── orders
└── total_quantity
```

For example, if two BUY orders exist:

```text
Order 1 → BUY @ 8000 → 100
Order 6 → BUY @ 8000 → 130
```

they belong to the same price level:

```text
Price: 8000

Orders:
    Order 1 → 100
    Order 6 → 130

Total Quantity: 230
```

The individual orders remain separate because they have different order IDs, while the price level provides an aggregate view of liquidity at that price.

The order sequence inside a price level is significant because it represents FIFO priority.

---

## 5. Order Book

The `OrderBook` contains two independent sides:

```text
OrderBook

├── bids
└── asks
```

### Bids

Bids represent BUY orders.

Buyers prefer lower prices, but the market prioritizes the highest available bid.

Therefore:

```text
8150
8000
7950
7800
```

has:

```text
Best Bid = 8150
```

### Asks

Asks represent SELL orders.

Sellers prefer higher prices, but the market prioritizes the lowest available ask.

Therefore:

```text
8050
8100
8350
```

has:

```text
Best Ask = 8050
```

The best prices represent the immediately available top of the order book.

---

## 6. Best Bid and Best Ask

The order book exposes two read-only queries:

```cpp
Price best_bid() const;
Price best_ask() const;
```

`best_bid()` returns the highest available BUY price.

`best_ask()` returns the lowest available SELL price.

These functions do not modify the order book.

The trailing `const` indicates that these operations are queries rather than mutations.

---

## 7. Bid-Ask Spread

The bid-ask spread measures the price difference between the best ask and best bid.

The formula is:

```text
Spread = Best Ask − Best Bid
```

For example:

```text
Best Bid = 8050
Best Ask = 8100

Spread = 8100 − 8050
       = 50 ticks
```

A smaller spread generally represents a tighter quoted market, while a larger spread represents a wider gap between buyers and sellers.

The spread is calculated from the current top of book.

---

## 8. Adding an Order

When an order is added, the order book determines its side.

```text
                    Order
                      │
             ┌────────┴────────┐
             │                 │
           BUY               SELL
             │                 │
           bids              asks
```

The order is then placed into the price level corresponding to its price.

If that price level already exists, the order is added to the existing level.

If it does not exist, a new price level is created.

For example:

```text
BUY @ 8000
BUY @ 8000
BUY @ 7950
```

produces:

```text
BIDS

8000
 ├── Order A
 └── Order B

7950
 └── Order C
```

rather than creating a separate price level for every order.

Orders are stored in insertion order within a price level so that FIFO priority can be preserved by the matching engine.

---

## 9. Order Cancellation

Orders can be removed using their `OrderId`.

Conceptually:

```text
cancel(order_id)

       │
       ↓

Find corresponding order

       │
       ↓

Remove order

       │
       ↓

Update total_quantity

       │
       ↓

Is price level empty?

       │
    ┌──┴──┐
   YES    NO
    │      │
 Remove   Keep
 level    level
```

For example:

```text
Price: 8000

Order 1 → 100
Order 6 → 130

Total = 230
```

After cancelling Order 1:

```text
Price: 8000

Order 6 → 130

Total = 130
```

The price level remains because liquidity still exists at that price.

If Order 6 is subsequently cancelled, the price level becomes empty and is removed.

---

## 10. Empty Price Levels

A price level should not remain in the book after its final order has been removed.

For example:

```text
Before:

8000
 └── Order 1

8150
 └── Order 2
```

After cancelling Order 1:

```text
8150
 └── Order 2
```

The `8000` price level disappears.

The matching engine follows the same rule after execution: when the final resting order at a price level is completely filled, the empty price level is removed.

This ensures that the order book represents currently available liquidity rather than historical price levels.

---

# Chapter 2 — Matching Engine

## 11. Matching Engine Overview

Chapter 2 introduces the matching engine.

The matching engine processes an incoming order against the opposite side of the book.

The fundamental rule is:

```text
BUY  → matches against ASKS
SELL → matches against BIDS
```

The incoming order is matched only while its price crosses the current best executable price.

Conceptually:

```text
Incoming Order
      ↓
Determine Side
      ↓
Find Opposite Side
      ↓
Check Price Crossing
      ↓
Match FIFO Orders
      ↓
Generate Trade(s)
      ↓
Update Quantities
      ↓
Remove Empty Orders/Levels
      ↓
Rest Remaining Quantity
```

The matching engine therefore transforms resting liquidity into executed trades.

---

## 12. Trade

A `Trade` represents one executed transaction between an incoming order and a resting order.

The structure contains:

```text
Trade

├── incoming_order
├── resting_order
├── price
└── quantity
```

Conceptually:

```cpp
struct Trade {
    OrderId incoming_order;
    OrderId resting_order;
    Price price;
    Quantity quantity;
};
```

For example:

```text
Incoming Order: 20
Resting Order:  7
Trade Price:    10500
Trade Quantity: 80
```

This means 80 units were executed between the incoming order and resting order at the resting order's price.

A single incoming order can therefore generate multiple `Trade` objects when it consumes liquidity from multiple resting orders.

---

## 13. BUY Matching

For an incoming BUY order, the matching engine examines the ask side.

The lowest ask has highest execution priority.

For example:

```text
ASKS

10000
 ├── Order 1
 └── Order 2

10100
 └── Order 3

10200
 └── Order 4
```

An incoming:

```text
BUY @ 10200
```

can match:

```text
10000
10100
10200
```

The crossing condition is:

```text
incoming BUY price >= resting ASK price
```

As long as this condition remains true, matching continues.

Once the next ask is too expensive:

```text
incoming BUY price < resting ASK price
```

matching stops.

Any remaining quantity of the incoming BUY order becomes resting bid liquidity.

---

## 14. SELL Matching

For an incoming SELL order, the matching engine examines the bid side.

The highest bid has highest execution priority.

For example:

```text
BIDS

10200
 └── Order 1

10100
 └── Order 2

10000
 └── Order 3
```

An incoming:

```text
SELL @ 10000
```

can match:

```text
10200
10100
10000
```

The crossing condition is:

```text
incoming SELL price <= resting BID price
```

As long as this condition remains true, matching continues.

Once the next bid is too low:

```text
incoming SELL price > resting BID price
```

matching stops.

Any remaining quantity of the incoming SELL order becomes resting ask liquidity.

---

## 15. Price-Time Priority

The matching engine follows price-time priority.

Price priority determines which price level executes first.

For BUY orders:

```text
Lowest ASK first
```

For SELL orders:

```text
Highest BID first
```

Within a single price level, time priority is represented by the order sequence inside the `orders` vector.

Therefore:

```text
Price: 10000

Order 1 → 30
Order 2 → 40
```

means Order 1 has priority over Order 2.

An incoming order consuming 50 units executes:

```text
Order 1 → 30
Order 2 → 20
```

rather than:

```text
Order 2 → 40
Order 1 → 10
```

This preserves FIFO execution within each price level.

---

## 16. Full Fill

A full fill occurs when the incoming order's remaining quantity is less than or equal to the resting order's quantity.

Example:

```text
Resting SELL → 100
Incoming BUY  → 40
```

The resulting trade is:

```text
Quantity = 40
```

The incoming order becomes completely filled:

```text
Remaining incoming quantity = 0
```

The resting order remains on the book with:

```text
Remaining resting quantity = 60
```

If the incoming quantity exactly equals the resting quantity, both become fully filled and the resting order is removed.

---

## 17. Partial Fill

A partial fill occurs when the incoming order is larger than the available quantity of the current resting order.

Example:

```text
Resting SELL → 30
Incoming BUY  → 50
```

The trade is:

```text
Quantity = 30
```

The resting order is completely filled and removed.

The incoming order retains:

```text
Remaining quantity = 20
```

The matching engine then continues searching for additional executable liquidity.

If no more crossing liquidity exists, the remaining 20 units are added to the book.

---

## 18. Multiple Price Levels

An incoming order can consume liquidity across multiple price levels.

Example:

```text
ASKS

10000 → 20
10100 → 30
10200 → 40
```

Incoming:

```text
BUY @ 10200 → 60
```

Execution becomes:

```text
20 @ 10000
30 @ 10100
10 @ 10200
```

The final state is:

```text
10200 → 30 remaining
```

The incoming order is completely filled.

This establishes the foundation for later market-impact and slippage calculations.

---

## 19. Trade Generation

Each individual execution creates a `Trade` object.

For example:

```text
Incoming BUY  #10 → 60 units
```

consuming:

```text
ASK #1 → 20
ASK #2 → 30
ASK #3 → 10
```

produces:

```text
Trade 1:
Incoming Order = 10
Resting Order  = 1
Quantity       = 20

Trade 2:
Incoming Order = 10
Resting Order  = 2
Quantity       = 30

Trade 3:
Incoming Order = 10
Resting Order  = 3
Quantity       = 10
```

The matching engine returns the collection of generated trades:

```cpp
std::vector<Trade>
```

The vector preserves execution order.

The first trade corresponds to the first matched resting order, and subsequent trades correspond to subsequent executions.

---

## 20. Order Quantity During Matching

The incoming order's `Quantity` represents its remaining unfilled amount.

During execution:

```text
Initial Quantity
      ↓
Match
      ↓
Subtract Executed Quantity
      ↓
Remaining Quantity
      ↓
Continue Matching
```

For example:

```text
Incoming BUY = 100

Execute 20
Remaining = 80

Execute 50
Remaining = 30

Execute 30
Remaining = 0
```

When the remaining quantity reaches zero, matching stops because the incoming order has been completely filled.

If positive quantity remains after all executable liquidity has been consumed, that remaining quantity is added to the appropriate side of the order book.

---

## 21. Matching and Book State Updates

Every execution updates both the order and price-level state.

For a trade:

```text
Trade Quantity = Q
```

the resting order is reduced by:

```text
Resting Quantity -= Q
```

and the corresponding price level is reduced by:

```text
Total Quantity -= Q
```

If the resting order reaches zero, it is removed.

If that removal leaves the price level empty, the price level is also removed.

Therefore:

```text
Trade
  ↓
Update Resting Order
  ↓
Update Price-Level Quantity
  ↓
Remove Filled Order if Necessary
  ↓
Remove Empty Price Level if Necessary
```

The order book therefore remains synchronized with executed trades.

---

## 22. Crossing Versus Resting

An incoming order has two possible outcomes.

### Crossing Order

If the incoming price crosses the opposite side:

```text
Incoming Order
      ↓
Executable Liquidity Exists
      ↓
Matching
      ↓
Trade(s)
```

The order may become completely filled or may retain a remaining quantity.

### Non-Crossing Order

If the incoming price does not cross the opposite side:

```text
Incoming Order
      ↓
No Executable Liquidity
      ↓
No Trade
      ↓
Order Rests on Book
```

This preserves the fundamental behavior of a limit order book.

---

## 23. Current Data Structures

Chapter 1 and Chapter 2 currently use simple `std::vector` containers:

```cpp
std::vector<PriceLevel> bids;
std::vector<PriceLevel> asks;
```

Each `PriceLevel` contains:

```cpp
std::vector<Order> orders;
```

Trades are returned as value objects:

```cpp
std::vector<Trade>
```

Therefore the current hierarchy is:

```text
OrderBook

│
├── bids: vector<PriceLevel>
│     │
│     ├── PriceLevel
│     │     └── vector<Order>
│     │
│     └── PriceLevel
│           └── vector<Order>
│
└── asks: vector<PriceLevel>
      │
      ├── PriceLevel
      │     └── vector<Order>
      │
      └── PriceLevel
            └── vector<Order>
```

The matching engine operates directly on these structures.

This remains intentionally simple before the order-ID tracking and performance stages of the project.

---

## 24. Data Ownership

The `OrderBook` owns its bid and ask price levels.

Each `PriceLevel` owns its collection of orders.

Therefore:

```text
OrderBook
    owns PriceLevels
        which own Orders
```

Trade objects returned by `process_order()` are value objects contained in the returned vector.

They represent execution results rather than resting book state.

The incoming order is supplied to the matching engine by reference, allowing its remaining quantity to be updated during execution.

---

## 25. Current API

The current `OrderBook` exposes:

```cpp
void add(const Order& order);

void cancel(OrderId order_id);

Price best_bid() const;

Price best_ask() const;

Price spread() const;

std::vector<Trade> process_order(Order& order);
```

### Mutations

```cpp
add()
cancel()
process_order()
```

These operations can modify the order-book state.

`process_order()` may:

* execute trades
* modify resting quantities
* remove filled orders
* remove empty price levels
* modify the incoming order's remaining quantity
* add remaining incoming quantity to the book

### Queries

```cpp
best_bid()
best_ask()
spread()
```

These inspect the current state without modifying it.

---

## 26. Order Lifecycle

The lifecycle has now been extended to include execution.

### Resting order lifecycle

```text
Order Creation
      ↓
Order Added
      ↓
Side Determined
      ↓
Price Level Located/Created
      ↓
Order Stored
      ↓
Order Becomes Resting Liquidity
      ↓
Cancellation OR Matching
      ↓
Quantity Updated / Order Removed
      ↓
Price Level Removed if Empty
```

### Incoming executable order

```text
Order Creation
      ↓
process_order()
      ↓
Check Opposite Side
      ↓
Price Crossing
      ↓
FIFO Matching
      ↓
Trade Generated
      ↓
Quantity Updated
      ↓
Fully Filled OR Remaining Quantity Rests
```

This creates the fundamental execution lifecycle required for later market microstructure simulation.

---

## 27. Testing

Chapter 1 includes:

```text
tests/cpp/test_order_book.cpp
```

These tests verify:

* BUY order insertion
* SELL order insertion
* Multiple orders at the same price
* Price-level aggregation
* Total quantity calculation
* Best bid
* Best ask
* Spread
* Order cancellation
* Quantity updates after cancellation
* Empty price-level removal
* Empty book state
* Invalid/nonexistent order cancellation

Chapter 2 adds:

```text
tests/cpp/test_matching_engine.cpp
```

The matching-engine test suite verifies:

* BUY execution
* SELL execution
* Full fills
* Partial fills
* Multiple resting orders
* FIFO execution
* Multiple price levels
* Remaining incoming quantity
* Remaining resting quantity
* Empty price-level removal
* Non-crossing BUY orders
* Non-crossing SELL orders
* Correct incoming order ID
* Correct resting order ID
* Correct execution price
* Correct execution quantity
* Correct final book state

Both Chapter 1 and Chapter 2 test suites pass successfully.

The test suites use a lightweight custom checking mechanism rather than a third-party testing framework.

---

## 28. Simulator

The simulator executable is:

```text
cpp/app/simulate_main.cpp
```

It demonstrates the current order-book and matching behavior.

The Chapter 2 simulator verifies:

* Initial book construction
* Best bid
* Best ask
* Spread
* Crossing BUY orders
* Crossing SELL orders
* Trade generation
* Trade IDs
* Execution prices
* Execution quantities
* Remaining incoming quantity
* Resulting best bid and ask

The simulator is intended as a small deterministic demonstration of the engine rather than a full market-event generator.

Large-scale event simulation will be introduced in a later chapter.

---

## 29. Scope of Chapters 1–2

The current implementation includes:

* Order representation
* Integer price representation
* Price levels
* Bid and ask sides
* Best bid
* Best ask
* Spread
* Order insertion
* Order cancellation
* FIFO order storage
* Limit-order matching
* BUY execution
* SELL execution
* Full fills
* Partial fills
* Multiple price levels
* Trade generation
* Remaining order quantity
* Empty order and price-level removal
* Deterministic simulator
* Dedicated unit tests

The project intentionally does not yet implement:

* Fast order-ID lookup structures
* Modify-order operations
* High-volume event generation
* NASDAQ ITCH parsing
* Historical replay
* Queue-position analytics
* Market-order execution modeling beyond the current limit-order matching engine
* Market impact models
* Slippage models
* Inventory management
* P&L
* Risk metrics
* Python bindings
* Zero-copy research pipelines
* Performance optimization

These features will be introduced progressively in later chapters.

---

## 30. Performance Considerations

The current vector-based implementation is designed primarily for correctness and conceptual clarity.

It is not yet the final high-performance architecture.

Operations that require searching through vectors can become expensive as the number of price levels and orders increases.

Matching also currently performs linear traversal through price levels and orders.

This is intentional.

The project first establishes a correct matching model before introducing more sophisticated structures.

Later chapters will introduce:

* Efficient order-ID tracking
* Faster order lookup
* More efficient price-level access
* Large-scale event simulation
* Benchmarking
* Profiling
* Performance optimization

Performance will eventually be measured rather than assumed.

---

## 31. Architectural Progression

The architecture has now progressed from basic book representation to execution.

### Current architecture

```text
Order
  ↓
PriceLevel
  ↓
OrderBook
  ↓
Matching Engine
  ↓
Trade
```

### Planned architecture

```text
Market Data
     ↓
Order Ingestion
     ↓
Order ID Tracking
     ↓
Price-Level Structure
     ↓
Matching Engine
     ↓
LOB State
     ↓
Microstructure Features
     ↓
Execution / Slippage / Risk
     ↓
Python Research Layer
```

The architecture is intentionally incremental.

Each stage establishes functionality that later stages depend on.

---

## 32. Chapter 1–2 Design Principle

The primary objective of the first two chapters is to establish a correct model of both resting liquidity and execution.

The fundamental abstraction is:

```text
Individual Order
        ↓
Price Level
        ↓
Order Book
        ↓
Matching Engine
        ↓
Trade
```

Chapter 1 establishes how liquidity is represented.

Chapter 2 establishes how that liquidity is consumed when incoming orders cross the book.

This creates the core market mechanism on which later microstructure, execution, risk, historical replay, and performance components will depend.

The implementation deliberately favors correctness and transparency before optimization.

Future chapters will build on this foundation without changing the fundamental concept of the order book itself.
