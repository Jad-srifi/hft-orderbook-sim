# Hybrid C++/Python Limit Order Book Simulator

# Chapter 1–5 — Order Book, Matching Engine, Order Tracking, Modification & Event-Driven Simulation Architecture

## 1. Overview

This project is a hybrid C++/Python limit order book simulator designed to model financial market microstructure.

Chapter 1 establishes the fundamental data model of the limit order book. It implements orders, price levels, bid and ask sides, best-price queries, spread calculation, and order cancellation.

Chapter 2 extends this foundation with a matching engine capable of processing crossing orders and generating trades according to price-time priority.

Chapter 3 introduces fast order-ID tracking through an auxiliary hash map. This allows the order book to locate a resting order using its `OrderId` without scanning every price level and order.

Chapter 4 introduces order modification and replace semantics. Orders can have their quantity or price changed while preserving market-matching rules such as FIFO priority. Modifications that change an order's queue priority are implemented through cancellation followed by reinsertion.

Chapter 5 introduces an event-driven simulation layer. Market actions are represented as timestamped and sequenced events, and a `Simulator` processes those events through the existing `OrderBook`. The simulator provides deterministic event processing, chronological validation, sequence-number validation, event storage, and trade-history collection.

The implementation prioritizes correctness, explicit state transitions, clear data flow, and testability before introducing more advanced data structures and performance optimizations.

The current architecture is:

```text
                         Event
                           |
                           v
                      Simulator
                           |
                           v
                       OrderBook
                    /      |       \
               OrderMap  Modify   Matching
                                  Engine
                                     |
                                     v
                                   Trade
```

The underlying order-book architecture remains:

```text
Order
   |
PriceLevel
   |
OrderBook
   |
+--+----------------+
|                   |
OrderMap        Matching Engine
                    |
                  Trade
```

The C++ implementation serves as the performance-critical core of the project, while Python will later be used for research, analysis, visualization, and quantitative experimentation.

---

# Part I — Core Order Book

## 2. Core Data Model

### 2.1 Order

An `Order` represents a single order submitted to the market.

Each order contains:

* `OrderId` — unique identifier for the order
* `Side` — `BUY` or `SELL`
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

For example:

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
├── asks
└── order_map
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

The order book exposes two queries:

```cpp
Price best_bid();
Price best_ask();
```

`best_bid()` returns the highest available BUY price.

`best_ask()` returns the lowest available SELL price.

These operations inspect the current order-book state.

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

The spread is calculated from the current top of book.

---

## 8. Adding an Order

When an order is added, the order book determines its side.

```text
                    Order
                      |
            +---------+---------+
            |                   |
           BUY                SELL
            |                   |
          bids                asks
```

The order is then placed into the price level corresponding to its price.

If that price level already exists, the order is appended to the existing level.

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

Orders are stored in insertion order within a price level so that FIFO priority can be preserved by the matching engine.

---

## 9. Order Cancellation

Orders can be removed using their `OrderId`.

The current implementation uses the `OrderMap` to locate the order before removing it.

Conceptually:

```text
cancel(order_id)

      ↓

Find OrderLocation in OrderMap

      ↓

Locate corresponding PriceLevel

      ↓

Remove order from price level

      ↓

Update total_quantity

      ↓

Remove OrderMap entry

      ↓

Update shifted indices

      ↓

Is price level empty?

 ├── YES → Remove price level
 └── NO  → Keep price level
```

The operation returns `true` when an order is successfully cancelled and `false` when the order does not exist or cannot be located.

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

# Part II — Matching Engine

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

Remove Filled Orders / Levels

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

The order identifiers are stored as `OrderId` values rather than complete `Order` objects.

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

This means 80 units were executed between the incoming order and the resting order at the resting order's price.

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

The matching engine reuses the normal `cancel()` path when a resting order has been completely consumed, keeping order removal logic centralized.

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
Incoming BUY #10 → 60 units
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

The matching engine returns:

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

Every execution updates both order and price-level state.

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

Conceptually:

```text
Trade

 ↓

Update Resting Order

 ↓

Update Price-Level Quantity

 ↓

Filled Order?

 ├── YES → Remove Order
 └── NO  → Keep Order

 ↓

Empty Price Level?

 ├── YES → Remove Level
 └── NO  → Keep Level
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

# Part III — Order ID Tracking

## 23. Motivation for OrderMap

Without additional indexing, finding an order by `OrderId` requires scanning the bid and ask price levels and then searching through the orders stored inside them.

Conceptually:

```text
Order ID

   ↓

Scan price levels

   ↓

Scan orders

   ↓

Find matching ID
```

As the number of orders grows, this becomes increasingly expensive.

Chapter 3 introduces an auxiliary `OrderMap`:

```text
OrderId

   ↓

OrderLocation
```

The purpose of the map is to locate the order's position in the book directly.

The current implementation uses:

```cpp
std::unordered_map<OrderId, OrderLocation>
```

which provides average constant-time lookup complexity under normal hash-table behavior.

The map acts as an index into the actual order-book storage rather than replacing it.

---

## 24. OrderLocation

An `OrderLocation` identifies where an order is stored inside the order book.

It contains:

```text
OrderLocation

├── side
├── price
└── index
```

Conceptually:

```cpp
struct OrderLocation {
    Side side;
    Price price;
    std::size_t index;
};
```

For example:

```text
Order ID: 42

Side:  BUY
Price: 10000
Index: 3
```

means that Order 42 is stored:

```text
BIDS
  ↓
Price Level 10000
  ↓
orders[3]
```

The location does not duplicate the full order.

It only stores enough information to find the actual order inside the existing vector-based book structure.

---

## 25. OrderMap Structure

The current `OrderMap` contains:

```cpp
std::unordered_map<OrderId, OrderLocation> orders;
```

and exposes:

```cpp
void add(OrderId order_id, OrderLocation order_location);

void remove(OrderId order_id);

std::optional<OrderLocation> find(OrderId order_id);

void update(OrderId order_id, OrderLocation new_location);
```

Conceptually:

```text
OrderBook

├── bids
│   └── PriceLevels
│       └── Orders
│
├── asks
│   └── PriceLevels
│       └── Orders
│
└── OrderMap
    └── OrderId → OrderLocation
```

The actual order remains stored in its `PriceLevel`.

The `OrderMap` simply provides a fast index to that order.

---

## 26. Why the Map Stores Location Rather Than Order Pointers

The current book stores orders as values inside:

```cpp
std::vector<Order>
```

Using:

```text
OrderId → Order*
```

would create pointer-stability problems because vector operations such as reallocation and erasure can invalidate pointers and references.

The current design instead stores:

```text
OrderId
    ↓
Side + Price + Index
```

This keeps ownership simple:

```text
OrderBook
    owns PriceLevels
        which own Orders
```

while the `OrderMap` remains an auxiliary lookup structure.

This avoids introducing pointer-based ownership solely for order lookup.

---

## 27. Adding Orders to OrderMap

When an order becomes resting liquidity, the `OrderBook` inserts its location into `OrderMap`.

For example:

```text
BUY Order 10

Price = 10000
Index = 2
```

produces:

```text
OrderMap

10 → { BUY, 10000, 2 }
```

The same process is used for SELL orders.

The map entry is created only for orders that are actually stored in the book.

An incoming order that becomes completely filled during matching is never added to the `OrderMap`.

---

## 28. Removing Orders from OrderMap

When a resting order is cancelled or completely filled, its `OrderMap` entry is removed.

Conceptually:

```text
Resting Order
      ↓
Cancellation / Full Fill
      ↓
Remove from PriceLevel
      ↓
Remove OrderMap entry
```

Because the underlying vector may shift, the locations of subsequent orders may also need to be updated.

---

## 29. Shifted Indices

The order storage uses:

```cpp
std::vector<Order>
```

Erasing an order from the middle of the vector shifts all subsequent orders one position toward the front.

For example:

```text
Before:

Index 0 → Order 10
Index 1 → Order 11
Index 2 → Order 12
Index 3 → Order 13
```

Removing Order 11 results in:

```text
After:

Index 0 → Order 10
Index 1 → Order 12
Index 2 → Order 13
```

Therefore the `OrderMap` entries for shifted orders must be updated.

The `update_shifted_indices()` operation performs this synchronization.

Conceptually:

```text
Erase index k
      ↓
Orders after k shift
      ↓
Find each shifted order's map entry
      ↓
Decrease its stored index by 1
```

This is necessary to preserve the relationship between the `OrderMap` and the vector-based order storage.

---

## 30. OrderMap Invariant

The central invariant introduced in Chapter 3 is:

```text
Every resting order in the book has exactly one OrderMap entry.

Every OrderMap entry corresponds to exactly one resting order.
```

More explicitly:

```text
Book Order

    ↕

OrderMap Entry
```

Both representations must describe the same currently resting order.

When an order is:

* added → its map entry is added
* cancelled → its map entry is removed
* fully filled → its map entry is removed
* shifted inside a vector → its stored index is updated
* partially filled → its existing map entry remains valid because its location does not change
* replaced → its map location is removed and recreated at the new location

This invariant remains central to the correctness of the architecture.

---

## 31. Partial Fills and OrderMap

A partial fill changes an order's quantity but does not change its location inside the price level.

For example:

```text
Price: 10500

Index 0 → Order 7 → Quantity 100
```

After executing 40 units:

```text
Index 0 → Order 7 → Quantity 60
```

The `OrderMap` remains:

```text
Order 7 → { side, 10500, 0 }
```

No location update is required because the order remains at the same vector index.

---

## 32. Full Fills and OrderMap

When a resting order is completely filled:

```text
Resting Quantity → 0
```

the order is removed from the book.

The `OrderBook` reuses the cancellation path to remove the fully consumed resting order.

Conceptually:

```text
Full Fill
   ↓
cancel(resting_order_id)
   ↓
Remove Order
   ↓
Remove OrderMap Entry
   ↓
Update shifted indices
   ↓
Remove empty price level if necessary
```

This keeps order-removal logic centralized rather than maintaining separate removal implementations for cancellation and matching.

---

# Part IV — Order Lookup and Modification

## 33. Direct Order Lookup

Chapter 4 introduces a helper for resolving an `OrderId` into the actual stored `Order`.

The public operation is:

```cpp
Order* find_order(OrderId order_id);
```

The lookup path is:

```text
OrderId
   ↓
OrderMap::find()
   ↓
OrderLocation
   ↓
Select bids / asks using side
   ↓
Find PriceLevel using price
   ↓
Access orders[index]
   ↓
Return Order*
```

The `OrderMap` therefore provides the first stage of the lookup while `find_order()` resolves that location into the actual order stored inside the book.

The returned pointer is a temporary access mechanism to the actual vector element.

It must not be retained across operations that can erase or reallocate the underlying vector.

The helper is primarily intended for controlled internal operations such as order modification and direct order inspection.

---

## 34. Order Modification / Replace

Chapter 4 introduces:

```cpp
bool modify(
    OrderId order_id,
    Price new_price,
    Quantity new_quantity
);
```

The purpose of modification is to change the state of a resting order while respecting FIFO queue rules.

The modification process is:

```text
modify(order_id, new_price, new_quantity)

                ↓

          find_order()

                ↓

       Does order exist?

          ├── NO → false
          │
          └── YES

                ↓

       new_quantity == 0?

          ├── YES
          │
          ↓
       cancel(order_id)
          │
          ↓
         true

          └── NO
                ↓

   Same price AND new quantity
      <= current quantity?

          ├── YES
          │
          ↓
     Modify in place
     Preserve FIFO
          │
          ↓
         true

          └── NO
                ↓
     Create replacement
     with same ID and side
                ↓
          cancel(old)
                ↓
             add(new)
                ↓
         FIFO priority lost
                ↓
              true
```

This creates explicit and deterministic modification semantics.

---

## 35. FIFO Rules for Modification

Not every modification should preserve queue priority.

The current rules are:

### Same Price + Quantity Decrease

If the price remains unchanged and the new quantity is less than or equal to the current quantity, the order is modified in place.

Example:

```text
Before:

Price 9900

Order 7 → 100
Order 8 → 200
```

Modify:

```text
Order 8 → 150
```

Result:

```text
Price 9900

Order 7 → 100
Order 8 → 150
```

Order 8 keeps its original position.

Therefore:

```text
FIFO preserved
```

### Same Price + Quantity Increase

Increasing quantity gives the order additional demand after it already exists in the queue.

Therefore the order loses its original time priority.

Example:

```text
Before:

Order 7 → index 0
Order 8 → index 1
```

After increasing Order 7:

```text
Order 8 → index 0
Order 7 → index 1
```

The implementation achieves this through cancellation followed by reinsertion.

Therefore:

```text
FIFO lost
```

### Price Change

A price change places the order into a different price queue.

The order therefore loses its previous time priority.

The implementation performs:

```text
cancel(old)

    ↓

add(replacement)
```

The same `OrderId` and `Side` are preserved.

---

## 36. Quantity Zero as Cancellation

A modification with:

```text
new_quantity = 0
```

is treated as cancellation.

Therefore:

```cpp
modify(order_id, new_price, 0);
```

performs:

```text
cancel(order_id)
```

and returns:

```text
true
```

provided that the order existed.

This ensures that zero-quantity resting orders do not remain in the book.

---

## 37. Order ID and Side Preservation

When an order is replaced because its price or quantity increase changes queue priority, the replacement retains:

```text
Original OrderId
Original Side
```

Only the modified attributes change:

```text
Price
Quantity
```

For example:

```text
Before:

Order 17
Side     = BUY
Price    = 10100
Quantity = 100
```

After:

```text
Order 17
Side     = BUY
Price    = 10200
Quantity = 100
```

The identity of the order remains unchanged even though its position in the book changes.

---

## 38. OrderMap During Modification

Because modification may involve vector erase and reinsertion, the `OrderMap` must remain synchronized.

For an in-place modification:

```text
Order
   ↓
Quantity / price updated
   ↓
Location unchanged
   ↓
OrderMap remains valid
```

For a replacement:

```text
OrderMap entry exists
       ↓
cancel()
       ↓
OrderMap entry removed
       ↓
Order reinserted
       ↓
New location created
       ↓
OrderMap entry recreated
```

The map therefore remains consistent regardless of whether the modification preserves or loses FIFO priority.

---

# Part V — Event-Driven Market Simulation

## 39. Event Representation

Chapter 5 introduces an event abstraction above the existing order-book engine.

The event representation is defined in:

```text
cpp/include/lob/event.hpp
```

An `Event` represents one market action that the simulator can process.

The current event types are:

```cpp
enum class EventType {
    ADD,
    CANCEL,
    MODIFY
};
```

Each event contains:

```text
Event

├── type
├── timestamp
├── sequence
├── order
├── order_id
├── new_price
└── new_quantity
```

Conceptually:

```cpp
struct Event {
    EventType type;

    Timestamp timestamp;
    Sequence sequence;

    Order order;

    OrderId order_id;

    Price new_price;
    Quantity new_quantity;
};
```

The event contains all possible information required by the three supported event types.

The simulator uses only the fields relevant to the current event:

```text
ADD

→ order

CANCEL

→ order_id

MODIFY

→ order_id
→ new_price
→ new_quantity
```

This keeps the event representation simple while allowing one event stream to contain different market actions.

---

## 40. Timestamp and Sequence Number

Each event contains two ordering fields:

```text
timestamp
sequence
```

`timestamp` represents the logical time at which the event occurs.

`sequence` resolves ordering between multiple events occurring at the same timestamp.

The current sequence model is:

```text
timestamp   sequence

100         0
100         1
100         2
100         3

101         0
101         1
101         2

102         0
102         1
```

The rules are:

```text
If timestamp < current_time
    → reject

If timestamp == current_time
    → sequence must be greater than last_sequence

If timestamp > current_time
    → sequence must be 0
```

After an accepted event:

```text
current_time = event.timestamp
last_sequence = event.sequence
```

Rejected events do not modify simulator state.

This provides deterministic ordering without relying on wall-clock execution time.

The exact timestamp unit is documented separately in the architecture documentation.

---

## 41. Simulator

Chapter 5 introduces the `Simulator` abstraction.

The simulator is defined in:

```text
cpp/include/lob/simulator.hpp
```

with implementation in:

```text
cpp/src/simulator.cpp
```

The simulator contains:

```text
Simulator

├── OrderBook
├── current_time
├── last_sequence
├── events
└── trades
```

Conceptually:

```cpp
struct Simulator {

    OrderBook order_book;

    Timestamp current_time = 0;

    Sequence last_sequence = 0;

    std::vector<Event> events;

    std::vector<Trade> trades;

    void add_event(Event event);

    void process_event(const Event& event);

    void process_events();
};
```

The simulator does not replace the `OrderBook`.

Instead, it provides an event-processing layer above it.

The architecture is:

```text
Event
  ↓
Simulator
  ↓
OrderBook
  ↓
Matching / Cancellation / Modification
  ↓
Updated Book
  ↓
Trade History
```

---

## 42. Simulator Event Dispatch

The simulator dispatches each event to the appropriate `OrderBook` operation.

### ADD

An `ADD` event supplies a complete `Order`.

The simulator first rejects duplicate order IDs.

If the ID is not already present, the simulator passes the order to:

```cpp
OrderBook::process_order()
```

The returned trades are appended to the simulator's trade history.

Conceptually:

```text
ADD Event
   ↓
Check duplicate OrderId
   ↓
process_order()
   ↓
Generate Trade(s)
   ↓
Store Trade(s)
```

### CANCEL

A `CANCEL` event supplies an `OrderId`.

The simulator calls:

```cpp
OrderBook::cancel(order_id)
```

The `OrderBook` remains responsible for determining whether the order exists and performing the cancellation.

If cancellation fails, the event is rejected.

### MODIFY

A `MODIFY` event supplies:

```text
OrderId
New Price
New Quantity
```

The simulator calls:

```cpp
OrderBook::modify(
    order_id,
    new_price,
    new_quantity
);
```

The `OrderBook` remains responsible for the modification rules.

If modification fails, the event is rejected.

This keeps market-state logic inside `OrderBook` instead of duplicating it inside the simulator.

---

## 43. Event Validation

Before an event modifies the order book, the simulator validates its ordering state.

The validation sequence is:

```text
Event
  ↓
Check timestamp
  ↓
Check sequence
  ↓
Check event type
  ↓
Check event-specific validity
  ↓
Execute
  ↓
Update simulator clock
```

The simulator rejects:

* backward timestamps
* duplicate or decreasing sequences at the same timestamp
* non-zero sequence numbers at a new timestamp
* duplicate ADD order IDs
* invalid CANCEL operations
* invalid MODIFY operations
* unsupported event types

Rejected events do not advance:

```text
current_time
last_sequence
```

and do not intentionally alter the order book.

This makes event processing deterministic and easier to reason about.

---

## 44. Event Stream

The simulator stores events internally using:

```cpp
std::vector<Event> events;
```

Events are added through:

```cpp
void add_event(Event event);
```

The method stores events in insertion order.

The simulator does not automatically sort events during insertion.

The event stream is expected to already be supplied in chronological order.

Ordering validation occurs during processing through:

```cpp
process_event()
```

This keeps one function responsible for event-order validation rather than duplicating the same rules in `add_event()`.

The stream-processing flow is:

```text
Simulator

├── events
│    ├── Event 1
│    ├── Event 2
│    ├── Event 3
│    └── ...
│
└── process_events()
          ↓
    process_event()
          ↓
       OrderBook
```

---

## 45. Processing an Event Stream

The simulator exposes:

```cpp
void process_events();
```

This processes the stored event stream sequentially.

Conceptually:

```text
events[0]
   ↓
process_event()

events[1]
   ↓
process_event()

events[2]
   ↓
process_event()

...
```

The order in the vector is therefore the order in which the simulator attempts to process the events.

Timestamp and sequence validation determine whether each event is accepted.

This creates a deterministic replay mechanism for the current event representation.

---

## 46. Simulation Clock

The simulator maintains:

```cpp
Timestamp current_time;
Sequence last_sequence;
```

These represent the temporal state of the most recently accepted event.

For example:

```text
Event 1:
timestamp = 100
sequence  = 0

Simulator:
current_time = 100
last_sequence = 0
```

Then:

```text
Event 2:
timestamp = 100
sequence  = 1

Simulator:
current_time = 100
last_sequence = 1
```

Then:

```text
Event 3:
timestamp = 101
sequence  = 0

Simulator:
current_time = 101
last_sequence = 0
```

The sequence number therefore resets whenever simulation time advances.

---

## 47. Duplicate Order Protection

The simulator prevents duplicate `ADD` events from creating two resting orders with the same `OrderId`.

Before processing an `ADD`, the simulator checks whether the order ID already exists in the order book.

Conceptually:

```text
ADD Event
   ↓
Does OrderId already exist?
   ├── YES → Reject
   └── NO  → Process
```

This protects the fundamental identity invariant:

```text
One active resting order
    ↔
One unique OrderId
```

The check is performed at the event layer because duplicate `ADD` is an invalid market event, while the existing `OrderBook` remains responsible for ordinary book mutations.

---

## 48. Trade History

The simulator maintains:

```cpp
std::vector<Trade> trades;
```

When an accepted `ADD` event generates executions, the resulting trades are appended to this vector.

The trade history therefore represents executions generated during the simulation.

For example:

```text
Event stream
    ↓
ADD Order 105
    ↓
Matching
    ↓
Trade 1
Trade 2
    ↓
Simulator::trades
```

The trade objects retain:

```text
incoming_order
resting_order
price
quantity
```

and preserve execution order.

This provides the foundation for later:

* execution analysis
* slippage measurement
* market-impact analysis
* liquidity analysis
* P&L calculation
* microstructure research

---

# Part VI — Current Data Structures and Ownership

## 49. Current Data Structures

The current implementation uses simple `std::vector` containers:

```cpp
std::vector<PriceLevel> bids;
std::vector<PriceLevel> asks;
```

Each `PriceLevel` contains:

```cpp
std::vector<Order> orders;
```

The `OrderBook` also contains:

```cpp
OrderMap order_map;
```

Trades are returned as value objects:

```cpp
std::vector<Trade>
```

The simulator stores:

```cpp
std::vector<Event> events;
std::vector<Trade> trades;
```

Therefore the current hierarchy is:

```text
Simulator

├── events
│   └── Event
│
├── trades
│   └── Trade
│
└── order_book
    │
    ├── bids
    │   └── PriceLevel
    │       └── vector<Order>
    │
    ├── asks
    │   └── PriceLevel
    │       └── vector<Order>
    │
    └── order_map
        └── unordered_map<OrderId, OrderLocation>
```

The matching engine operates directly on the bid and ask structures.

The `OrderMap` provides indexed access to existing orders without changing ownership.

The `Simulator` owns the event stream and generated trade history while delegating actual market-state mutations to the `OrderBook`.

---

## 50. Data Ownership

The `OrderBook` owns its bid and ask price levels.

Each `PriceLevel` owns its collection of orders.

Therefore:

```text
OrderBook
    owns PriceLevels
        which own Orders
```

The `OrderMap` is owned by the `OrderBook` and acts as an auxiliary index.

It does not own or duplicate the orders themselves.

Trade objects returned by `process_order()` are value objects contained in the returned vector.

The simulator stores copies of those trade results in its trade history.

Events are also stored as values inside the simulator's event vector.

The incoming order is supplied to the matching engine by reference, allowing its remaining quantity to be updated during execution.

---

# Part VII — Current API

## 51. Current OrderBook API

The current `OrderBook` exposes operations equivalent to:

```cpp
void add(const Order& order);

bool cancel(OrderId order_id);

Price best_bid();

Price best_ask();

float spread();

std::vector<Trade> process_order(Order& order);

Order* find_order(OrderId order_id);

bool modify(
    OrderId order_id,
    Price new_price,
    Quantity new_quantity
);

void sort_price_levels();

void update_shifted_indices(
    Side side,
    Price price,
    std::size_t erased_index
);
```

### Mutations

```cpp
add()
cancel()
process_order()
modify()
```

These operations can modify the order-book state.

`process_order()` may:

* execute trades
* modify resting quantities
* remove filled orders
* remove empty price levels
* modify the incoming order's remaining quantity
* add remaining incoming quantity to the book

`modify()` may:

* change an order in place
* cancel and replace an order
* preserve or reset FIFO priority
* update the `OrderMap`
* remove an order when quantity becomes zero

### Queries / Accessors

```cpp
best_bid()
best_ask()
spread()
find_order()
```

These inspect or resolve the current order-book state.

---

## 52. Current OrderMap API

The current `OrderMap` exposes:

```cpp
void add(
    OrderId order_id,
    OrderLocation order_location
);

void remove(OrderId order_id);

std::optional<OrderLocation> find(OrderId order_id);

void update(
    OrderId order_id,
    OrderLocation new_location
);
```

The responsibilities are:

```text
add()

    Add a new OrderId → location mapping

remove()

    Delete a mapping

find()

    Retrieve an order's current location

update()

    Replace a stored location after vector shifts
```

The `find()` operation returns:

```cpp
std::optional<OrderLocation>
```

This allows a missing `OrderId` to be represented explicitly by `std::nullopt`.

---

## 53. Current Simulator API

The current `Simulator` exposes:

```cpp
void add_event(Event event);

void process_event(const Event& event);

void process_events();
```

Its main state is:

```cpp
OrderBook order_book;

Timestamp current_time = 0;

Sequence last_sequence = 0;

std::vector<Event> events;

std::vector<Trade> trades;
```

Responsibilities are divided as follows:

```text
Simulator

add_event()
    Stores an event

process_event()
    Validates and executes one event

process_events()
    Processes the stored event stream sequentially
```

The simulator does not directly implement matching, cancellation, or modification semantics.

Those remain responsibilities of the `OrderBook`.

---

# Part VIII — Order Lifecycle

## 54. Resting Order Lifecycle

```text
Order Creation
      ↓
Order Added
      ↓
Side Determined
      ↓
Price Level Located / Created
      ↓
Order Stored
      ↓
OrderMap Entry Created
      ↓
Order Becomes Resting Liquidity
      ↓
Cancellation / Matching / Modification
      ↓
Quantity Updated / Order Removed / Order Replaced
      ↓
OrderMap Updated
      ↓
Price Level Removed if Empty
```

---

## 55. Incoming Executable Order

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
Fully Filled OR Remaining Quantity

     ┌─────────────────────┐
     │                     │
Fully Filled       Remaining Quantity
     │                     │
     ↓                     ↓
Not inserted              add()
into OrderMap               │
                            ↓
                       OrderMap Entry
```

This creates the fundamental execution lifecycle required for later market microstructure simulation.

---

## 56. Modified Order Lifecycle

An order modification has one of three main paths.

```text
Existing Resting Order

        ↓

     modify()

        ↓

 ┌───────────────┬────────────────────┐
 │               │                    │
Decrease /       Increase or          Quantity
equal quantity   price change         becomes 0
 │               │                    │
 ↓               ↓                    ↓
Modify in        cancel + add         cancel
place            replacement         order
 │               │                    │
 ↓               ↓                    ↓
FIFO preserved   FIFO lost            Removed
```

The `OrderId` remains unchanged during replacement.

The `OrderMap` must continue to identify the currently active resting representation of that order.

---

## 57. Event-Driven Order Lifecycle

Chapter 5 places the existing order lifecycle inside an event stream.

The complete path is now:

```text
Event
  ↓
Simulator
  ↓
Validate timestamp / sequence
  ↓
Validate event-specific state
  ↓
OrderBook
  ↓
ADD / CANCEL / MODIFY
  ↓
Updated OrderBook
  ↓
Generated Trade(s)
  ↓
Simulator Trade History
```

For an `ADD` event:

```text
ADD Event
   ↓
Duplicate ID Check
   ↓
process_order()
   ↓
Matching
   ↓
Trade(s)
   ↓
Remaining Order
   ↓
Book / OrderMap
```

For a `CANCEL` event:

```text
CANCEL Event
   ↓
Order ID
   ↓
OrderBook::cancel()
   ↓
OrderMap / Book Updated
```

For a `MODIFY` event:

```text
MODIFY Event
   ↓
Order ID + New State
   ↓
OrderBook::modify()
   ↓
In-place modification
OR
Cancel + Replace
   ↓
Updated Book + OrderMap
```

---

# Part IX — Testing

## 58. Chapter 1 Testing

Chapter 1 includes:

```text
tests/cpp/test_order_book.cpp
```

The original tests verify:

* BUY order insertion
* SELL order insertion
* multiple orders at the same price
* price-level aggregation
* total quantity calculation
* best bid
* best ask
* spread
* order cancellation
* quantity updates after cancellation
* empty price-level removal
* empty-book state
* invalid/nonexistent order cancellation

---

## 59. Chapter 2 Testing

Chapter 2 includes:

```text
tests/cpp/test_matching_engine.cpp
```

The matching-engine test suite verifies:

* BUY execution
* SELL execution
* full fills
* partial fills
* multiple resting orders
* FIFO execution
* multiple price levels
* remaining incoming quantity
* remaining resting quantity
* empty price-level removal
* non-crossing BUY orders
* non-crossing SELL orders
* correct incoming order ID
* correct resting order ID
* correct execution price
* correct execution quantity
* correct final book state

---

## 60. Chapter 3 Integration Testing

Chapter 3 extends:

```text
tests/cpp/test_order_book.cpp
```

with integration tests covering the interaction between the order book, matching engine, and `OrderMap`.

The integration suite verifies:

* initial `OrderMap` population
* BUY order tracking
* SELL order tracking
* middle-order cancellation
* shifted index updates after cancellation
* first-order cancellation
* empty price-level removal
* partial fills preserving `OrderMap` entries
* full fills removing orders from `OrderMap`
* shifted indices after full fills
* multi-level BUY matching
* new orders inserted after previous deletions
* multi-level SELL matching
* SELL partial fills preserving correct indices
* cancellation after matching
* nonexistent cancellation not corrupting the book
* final consistency between the book and `OrderMap`

These tests establish the central Chapter 3 synchronization invariant.

---

## 61. Chapter 4 Testing

Chapter 4 extends the same integration test suite with direct tests for order lookup and modification.

The current Chapter 4 tests verify:

* direct `find_order()` lookup
* nonexistent `find_order()` returns `nullptr`
* same-price quantity decrease preserves FIFO
* same-price quantity increase loses FIFO
* price modification loses FIFO
* modified quantity is correct
* modified price is correct
* Order ID is preserved
* side is preserved
* `OrderMap` price remains correct
* `OrderMap` index remains correct
* shifted indices remain correct after replacement
* old price-level membership is removed when required
* zero quantity acts as cancellation
* nonexistent modification returns `false`
* modification after partial fill
* modified orders remain executable
* fully matched modified orders are removed from `OrderMap`
* FIFO resets after replacement

The Chapter 4 test suite also verifies that modifications performed after previous matching and cancellation operations do not corrupt the existing book state.

---

## 62. Chapter 5 Simulator Testing

Chapter 5 introduces:

```text
tests/cpp/test_simulator.cpp
```

The simulator test suite verifies the event-driven layer.

It covers:

* ADD events
* CANCEL events
* MODIFY events
* duplicate ADD rejection
* invalid CANCEL rejection
* invalid MODIFY rejection
* backward timestamp rejection
* same-timestamp sequence ordering
* duplicate sequence rejection
* lower sequence rejection
* new timestamp requiring sequence `0`
* generated trade history
* multiple event processing
* complete event-stream processing
* final simulator timestamp
* final simulator sequence state

The trade-generation test verifies that `Trade` fields contain the correct order identifiers:

```text
incoming_order
resting_order
price
quantity
```

For example:

```text
Incoming Order: 91
Resting Order: 90
Price:          10500
Quantity:       60
```

The current Chapter 5 test output is:

```text
All Chapter 5 simulator tests passed.
```

This establishes the simulator as the next correctness layer above the existing order-book tests.

---

## 63. Test Philosophy

The test suites use a lightweight custom checking mechanism rather than a third-party testing framework.

The tests are designed to verify both individual functionality and system invariants.

For Chapters 3 and 4, correctness is not limited to checking whether an order can be found or modified.

The integration tests verify that the index and order-book state remain synchronized after mutations such as:

```text
insertions
cancellations
vector erasures
partial fills
full fills
multi-level matching
quantity modifications
price modifications
order replacement
```

Chapter 5 extends this philosophy to the event layer.

The simulator tests verify that:

```text
Event ordering
      ↓
Simulator state
      ↓
OrderBook mutations
      ↓
Trade history
```

remain consistent.

This is important because the simulator is an additional stateful layer above the order book. A correct event-driven simulation must preserve the correctness guarantees already established by Chapters 1–4.

---

# Part X — Simulator Demonstration

## 64. Simulator Executable

The simulator executable is:

```text
cpp/app/simulate_main.cpp
```

It demonstrates the current order-book, matching, tracking, modification, and event-driven simulation behavior.

The Chapter 5 demonstration creates:

* `ADD` events
* `CANCEL` events
* `MODIFY` events
* aggressive BUY events
* aggressive SELL events
* timestamps
* sequence numbers

The events are added to the simulator and processed as one event stream.

The demonstration verifies:

* event storage
* chronological event processing
* sequence-number handling
* order cancellation
* order modification
* trade generation
* trade IDs
* execution prices
* execution quantities
* final book state
* simulator timestamp
* simulator sequence state

A representative final state is:

```text
===== FINAL SIMULATED BOOK =====
Best Bid: 10000
Best Ask: 10600
Spread: 600

===== GENERATED TRADES =====
Incoming Order: 105 | Resting Order: 104 | Price: 10500 | Quantity: 50
Incoming Order: 105 | Resting Order: 103 | Price: 10600 | Quantity: 70
Incoming Order: 106 | Resting Order: 101 | Price: 10000 | Quantity: 80

===== SIMULATION STATE =====
Current Timestamp: 103
Last Sequence: 0
```

This demonstrates that the event stream successfully drives the existing matching and order-management system.

The simulator remains a deterministic demonstration rather than a large-scale market-data generator.

---

# Part XI — Current Scope

## 65. Scope of Chapters 1–5

The current implementation includes:

* order representation
* integer price representation
* price levels
* bid and ask sides
* best bid
* best ask
* spread
* order insertion
* order cancellation
* FIFO order storage
* limit-order matching
* BUY execution
* SELL execution
* full fills
* partial fills
* multiple price levels
* trade generation
* remaining order quantity
* empty order and price-level removal
* deterministic order-book behavior
* dedicated tests
* `OrderId` tracking
* `OrderLocation`
* `OrderMap`
* shifted-index synchronization
* OrderMap/book consistency invariant
* direct `OrderId` → `Order*` resolution through `find_order()`
* order quantity modification
* order price modification
* FIFO-preserving quantity decreases
* FIFO-resetting quantity increases
* FIFO-resetting price changes
* order replacement
* Order ID preservation during replacement
* side preservation during replacement
* zero-quantity modification as cancellation
* modification-related OrderMap synchronization
* event representation
* event types
* event timestamps
* event sequence numbers
* event storage
* event-stream processing
* simulator state
* chronological event validation
* sequence-number validation
* duplicate ADD protection
* rejected-event handling
* simulator trade history
* deterministic event processing
* dedicated Chapter 5 simulator tests

The project intentionally does not yet implement:

* high-volume event generation
* NASDAQ ITCH parsing
* historical replay
* queue-position analytics
* market-order execution modeling beyond the current limit-order matching engine
* market-impact models
* slippage models
* inventory management
* P&L
* risk metrics
* Python bindings
* zero-copy research pipelines
* performance benchmarking at scale
* production-level performance optimization

These features will be introduced progressively in later chapters.

---

# Part XII — Current Performance Model

## 66. Current Performance Model

The current vector-based implementation is designed primarily for correctness and conceptual clarity.

It is not yet the final high-performance architecture.

The current computational characteristics include:

```text
Price-level search
    → linear in number of levels

Order insertion into level
    → vector append

Order removal
    → vector erase + shifted-index updates

Order ID lookup
    → average O(1) through unordered_map

Direct order resolution
    → map lookup + price-level search

Matching
    → traversal of price levels and orders

FIFO-preserving modification
    → in-place mutation

Priority-changing modification
    → cancel + reinsert

Event storage
    → vector append

Event processing
    → sequential event traversal
```

The introduction of `OrderMap` removes the need for a full book-wide search when locating an order by ID.

However, the current architecture still requires index maintenance when erasing from a price-level vector.

For example:

```text
erase(order at index k)
        ↓
orders after k shift
        ↓
OrderMap indices must be updated
```

Similarly, replacement operations that lose priority intentionally pay the cost of cancellation and reinsertion.

The event-driven layer currently prioritizes deterministic processing and clear state transitions rather than maximum throughput.

Therefore the `OrderMap`, modification system, and simulator improve functionality without pretending that all operations are already optimal.

The project establishes a correct architecture first and will optimize specific bottlenecks later using measured performance data.

---

# Part XIII — Architectural Progression

## 67. Architecture After Chapter 5

The architecture has progressed from basic book representation to execution, indexed order tracking, explicit modification semantics, and finally event-driven simulation.

```text
                         Event
                           |
                           v
                      Simulator
                           |
                           v
                       OrderBook
                    /      |       \
               OrderMap  Modify   Matching
                                  Engine
                                     |
                                     v
                                   Trade
```

The `OrderMap` is an auxiliary index over the underlying vector-based order storage.

It does not replace the order book.

`find_order()` converts the indexed location into access to the actual stored order.

`modify()` uses that lookup to implement deterministic modification semantics while preserving the core ownership model.

The `Simulator` provides the event-driven layer above the order book.

The resulting division of responsibilities is:

```text
Order

    Represents individual order state

PriceLevel

    Groups orders at the same price

OrderBook

    Owns bids, asks, and overall book state

OrderMap

    Provides OrderId → location lookup

find_order()

    Resolves OrderId → actual stored Order

modify()

    Changes order state while enforcing FIFO rules

Matching Engine

    Determines executions and generates trades

Trade

    Represents execution results

Event

    Represents one timestamped market action

Simulator

    Processes events and maintains simulation state
```

---

## 68. Event-Driven Architecture

Chapter 5 adds an additional layer above the existing market mechanism:

```text
Event Stream
     ↓
Simulator
     ↓
OrderBook
     ↓
Matching / Cancellation / Modification
     ↓
Book State
     ↓
Trade History
```

This creates a separation between:

```text
What happened?

    Event
```

and:

```text
What does that event do to the market?

    OrderBook
```

The `Simulator` coordinates the two.

This separation is important for later historical replay because external market data can eventually be transformed into the same `Event` representation and passed through the same simulation engine.

---

## 69. Deterministic Replay Principle

Chapter 5 establishes the first version of deterministic event replay.

Given the same:

```text
Initial Book State
+
Event Stream
```

the simulator should process events in the same order and produce the same:

```text
Final Book State
+
Trade History
+
Simulation Time
```

The ordering mechanism is:

```text
Timestamp
    ↓
Sequence
```

The simulator therefore does not depend on real-world wall-clock timing to determine event order.

This establishes an important foundation for later:

* historical replay
* market-data reconstruction
* strategy backtesting
* execution simulation
* out-of-sample experiments

---

# Part XIV — Planned Architecture

## 70. Planned Architecture

The longer-term architecture is:

```text
Market Data

     ↓

Order / Event Ingestion

     ↓

Order ID Tracking

     ↓

Price-Level Structure

     ↓

Order Modification / Cancellation

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

The current event-driven layer now provides the interface between future market-data ingestion and the existing order-book engine.

Eventually:

```text
Historical / Synthetic Market Data
              ↓
         Event Stream
              ↓
          Simulator
              ↓
          OrderBook
              ↓
      Market Microstructure
```

will allow the same engine to process both synthetic and historical event streams.

The architecture is intentionally incremental.

Each stage establishes functionality that later stages depend on.

---

# Part XV — Chapter 1–5 Design Principles

## 71. Auxiliary Index Rather Than Replacement Structure

The central design decision of Chapter 3 is that `OrderMap` is an index rather than a second copy of the order book.

The source of truth for order state remains:

```text
OrderBook
    ↓
PriceLevel
    ↓
Order
```

The map stores only:

```text
OrderId
    ↓
OrderLocation
```

This avoids duplicating complete order objects and keeps ownership straightforward.

---

## 72. Synchronization Invariant

The correctness of the architecture depends on maintaining:

```text
Book state ↔ OrderMap state
```

Whenever the book changes, the map must be updated accordingly.

The major synchronization events are:

```text
add()

    → create map entry

cancel()

    → remove map entry
    → update shifted entries

partial fill

    → quantity changes
    → location remains valid

full fill

    → remove map entry
    → update shifted entries

quantity decrease without priority loss

    → order modified in place
    → location remains valid

quantity increase

    → cancel existing order
    → reinsert replacement
    → create new map location

price change

    → cancel existing order
    → insert at new price level
    → create new map location
```

The integration suite explicitly tests these transitions.

---

## 73. Modification Semantics Are Part of Market Structure

Order modification is not treated as a generic field update.

The modification rules encode queue-priority behavior:

```text
Same price + smaller/equal quantity
        ↓
Modify in place
        ↓
FIFO preserved
```

versus:

```text
Quantity increase OR price change
        ↓
Cancel + replace
        ↓
FIFO lost
```

This distinction is essential because modifying an order can change its position relative to other participants at the same price.

The simulator therefore models modification as a market-structure event rather than merely a data mutation.

---

## 74. Event Ordering Is Part of Simulation State

Chapter 5 introduces a new invariant:

```text
Accepted events must follow the simulator's temporal ordering rules.
```

For events at the same timestamp:

```text
sequence must increase strictly
```

When time advances:

```text
sequence must restart at 0
```

Therefore:

```text
Timestamp + Sequence
```

define the deterministic ordering of accepted events.

Rejected events do not advance the simulation clock.

This makes temporal behavior explicit rather than implicit.

---

## 75. Simulator as an Orchestration Layer

The `Simulator` does not duplicate the responsibilities of the `OrderBook`.

Instead:

```text
Simulator
    |
    +-- validates event ordering
    |
    +-- dispatches event
    |
    +-- records generated trades
    |
    +-- maintains simulation clock
```

while:

```text
OrderBook
    |
    +-- owns book state
    |
    +-- matches orders
    |
    +-- cancels orders
    |
    +-- modifies orders
    |
    +-- maintains OrderMap
```

This separation prevents event-processing logic from becoming tightly coupled to the underlying order-book implementation.

---

## 76. Why Correctness Comes Before Optimization

The current architecture deliberately uses straightforward containers and explicit synchronization.

The objective is first to establish:

```text
Correct Order Book

      +

Correct Matching

      +

Correct Order Tracking

      +

Correct Modification Semantics

      +

Correct Event Processing

      =

Reliable Simulation Core
```

Only after these invariants are stable should the project introduce more specialized data structures or performance optimizations.

Future optimization decisions will be justified through:

```text
Benchmarking

    ↓

Profiling

    ↓

Identify bottleneck

    ↓

Change data structure / algorithm

    ↓

Benchmark again

    ↓

Verify correctness
```

Performance improvements will therefore be measured rather than assumed.

---

# Part XVI — Chapter 1–5 Design Principle

## 77. Five Fundamental Layers

The first five chapters establish five fundamental layers of the simulator.

### Chapter 1 — Represent Liquidity

```text
Individual Order
        ↓
Price Level
        ↓
Order Book
```

Chapter 1 establishes how resting liquidity is stored and queried.

### Chapter 2 — Consume Liquidity

```text
Incoming Order
        ↓
Matching Engine
        ↓
Trade(s)
        ↓
Updated Book
```

Chapter 2 establishes how liquidity is consumed when incoming orders cross the book.

### Chapter 3 — Locate Liquidity

```text
OrderId
   ↓
OrderMap
   ↓
OrderLocation
   ↓
Actual Order in Book
```

Chapter 3 establishes efficient order identification while preserving the existing ownership model.

### Chapter 4 — Modify Liquidity

```text
OrderId
   ↓
find_order()
   ↓
modify()
   ↓
Preserve FIFO

OR

Cancel + Replace
   ↓
Updated Book + OrderMap
```

Chapter 4 establishes explicit order-modification semantics and connects order identity, queue priority, and book mutation.

### Chapter 5 — Drive the Market Through Events

```text
Event
   ↓
Simulator
   ↓
Timestamp / Sequence Validation
   ↓
OrderBook
   ↓
ADD / CANCEL / MODIFY
   ↓
Trade(s) / Updated Book
   ↓
Simulation State
```

Chapter 5 establishes the event-driven execution layer that can later consume synthetic or historical market events.

Together:

```text
                         Event
                           |
                           v
                      Simulator
                           |
                           v
                    +--------------+
                    |  OrderBook   |
                    +--------------+
                     /      |      \
                    /       |       \
                   v        v        v
              OrderMap   Modify   Matching
                                    Engine
                                       |
                                       v
                                     Trade
```

The five chapters therefore establish:

```text
Chapter 1 → Represent Liquidity
Chapter 2 → Consume Liquidity
Chapter 3 → Locate Liquidity
Chapter 4 → Modify Liquidity
Chapter 5 → Drive Liquidity Through Events
```

This creates the core market mechanism on which later microstructure, execution, risk, historical replay, Python research, and performance components will depend.

The implementation deliberately favors correctness, deterministic behavior, and transparency before optimization.

Future chapters will build on this foundation while preserving the fundamental ownership model, OrderMap synchronization invariant, FIFO semantics, and deterministic event-processing rules established in the first five chapters.
