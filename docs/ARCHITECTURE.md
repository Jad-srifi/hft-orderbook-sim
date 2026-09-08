# Hybrid C++/Python Limit Order Book Simulator

## Chapter 1 — Order Book Architecture

## 1. Overview

This project is a hybrid C++/Python limit order book simulator designed to model financial market microstructure.

Chapter 1 establishes the fundamental data model of the limit order book. It implements orders, price levels, bid and ask sides, best-price queries, spread calculation, and order cancellation.

The implementation at this stage intentionally prioritizes clarity and correctness over performance.

The current architecture is:

```text
Order
  ↓
PriceLevel
  ↓
OrderBook
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
* `Quantity` — number of units available

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

Buyers prefer lower prices, but the market prioritizes the **highest available bid**.

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

Sellers prefer higher prices, but the market prioritizes the **lowest available ask**.

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

A smaller spread generally represents tighter market liquidity, while a larger spread represents a wider gap between buyers and sellers.

At this stage, the project only calculates the quoted spread. It does not yet model execution, slippage, or market impact.

---

## 8. Adding an Order

When an order is added, the order book determines its side.

```text
                    Order
                      │
             ┌────────┴────────┐
             │                 │
           BUY                SELL
             │                 │
           bids               asks
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

This ensures that the order book represents currently available liquidity rather than historical price levels.

---

## 11. Current Data Structures

Chapter 1 uses simple `std::vector` containers:

```cpp
std::vector<PriceLevel> bids;
std::vector<PriceLevel> asks;
```

Each `PriceLevel` contains:

```cpp
std::vector<Order> orders;
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

This structure is intentionally simple for the first chapter.

It makes the underlying model easy to understand and verify before introducing more sophisticated data structures.

---

## 12. Data Ownership

The `OrderBook` owns its bid and ask price levels.

Each `PriceLevel` owns its collection of orders.

Therefore:

```text
OrderBook
    owns PriceLevels
        which own Orders
```

An order added to the book becomes part of the corresponding price level's order collection.

The architecture therefore has clear ownership boundaries.

---

## 13. Current API

The Chapter 1 `OrderBook` exposes:

```cpp
void add(const Order& order);
void cancel(OrderId order_id);

Price best_bid() const;
Price best_ask() const;
Price spread() const;
```

The API separates mutations from queries.

### Mutations

```cpp
add()
cancel()
```

These modify the state of the book.

### Queries

```cpp
best_bid()
best_ask()
spread()
```

These inspect the current state without modifying it.

---

## 14. Order Lifecycle

The current order lifecycle is:

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
Cancellation
      ↓
Quantity Updated
      ↓
Price Level Removed if Empty
```

This represents the basic resting-order lifecycle.

Matching and execution are intentionally not part of Chapter 1.

---

## 15. Testing

Chapter 1 includes a dedicated test executable:

```text
tests/cpp/test_order_book.cpp
```

The tests verify:

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

All Chapter 1 tests pass successfully.

The test suite uses a lightweight custom checking mechanism rather than a third-party testing framework.

---

## 16. Scope of Chapter 1

Chapter 1 intentionally does **not** implement:

* Order matching
* Market orders
* Trade execution
* Partial fills
* Order-ID lookup structures
* NASDAQ ITCH parsing
* Historical replay
* Queue-position modeling
* Market impact
* Slippage
* Inventory management
* P&L
* Risk metrics
* Python bindings
* Performance optimization

These features will be introduced progressively in later chapters.

---

## 17. Performance Considerations

The current vector-based implementation is designed primarily for correctness and conceptual clarity.

It is **not yet the final high-performance architecture**.

Operations that require searching through vectors can become expensive as the number of price levels and orders increases.

This is intentional.

Later chapters will introduce data structures better suited for high-frequency order-book workloads, including efficient order-ID tracking and faster price-level access.

Performance will eventually be measured rather than assumed.

---

## 18. Architectural Progression

The project will evolve from the simple Chapter 1 model toward a more realistic architecture.

Current:

```text
Order
  ↓
PriceLevel
  ↓
OrderBook
```

Later:

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

The architecture is therefore intentionally incremental.

Each stage establishes functionality that later stages depend on.

---

## 19. Chapter 1 Design Principle

The primary objective of Chapter 1 is to establish a correct mental and software model of a limit order book before optimizing it.

The key abstraction is:

```text
Individual Order
        ↓
Price Level
        ↓
Order Book
```

Once this hierarchy is understood and tested, more sophisticated components can be added without changing the fundamental concept of how resting liquidity is represented.

Chapter 1 therefore serves as the foundation for the matching engine, market microstructure analysis, and performance work that follow.

````

Put that file at:

```text
hybrid-lob-simulator/
└── docs/
    └── ARCHITECTURE.md
````

Then Chapter 1 is essentially down to the **understanding gate**: you should be able to explain the architecture yourself before we touch Chapter 2.
