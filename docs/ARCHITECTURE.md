# Hybrid C++/Python Limit Order Book Simulator

# Chapter 1–6 — Order Book, Matching Engine, Order Tracking, Modification, Event-Driven Simulation & Market Microstructure Metrics

## 1. Overview

This project is a hybrid C++/Python limit order book simulator designed to model financial market microstructure.

Chapter 1 establishes the fundamental data model of the limit order book. It implements orders, price levels, bid and ask sides, best-price queries, spread calculation, and order cancellation.

Chapter 2 extends this foundation with a matching engine capable of processing crossing orders and generating trades according to price-time priority.

Chapter 3 introduces fast order-ID tracking through an auxiliary hash map. This allows the order book to locate a resting order using its `OrderId` without scanning every price level and order.

Chapter 4 introduces order modification and replace semantics. Orders can have their quantity or price changed while preserving market-matching rules such as FIFO priority. Modifications that change an order's queue priority are implemented through cancellation followed by reinsertion.

Chapter 5 introduces an event-driven simulation layer. Market actions are represented as timestamped and sequenced events, and a `Simulator` processes those events through the existing `OrderBook`. The simulator provides deterministic event processing, chronological validation, sequence-number validation, event storage, and trade-history collection.

Chapter 6 introduces a read-only market microstructure metrics layer. This layer derives observable quantities from the existing `OrderBook` and simulator trade history without duplicating market state or mutating the underlying engine. It provides top-of-book metrics, depth, imbalance, and basic trade statistics.

The implementation prioritizes correctness, explicit state transitions, clear data flow, and testability before introducing more advanced data structures and performance optimizations.

The current architecture is:

```text
                         Event
                           |
                           v
                      Simulator
                      /       \
                     v         v
                OrderBook   Trade History
                /     | \
               /      |  \
          OrderMap  Modify Matching
                            Engine
                               |
                               v
                             Trade
                               |
                               v
                       Microstructure
                           Metrics
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

The metrics layer observes these existing structures:

```text
OrderBook ──────────────┐
                        |
Trade History ──────────┤
                        v
                 Metrics Functions
                        |
                        v
                  MarketMetrics
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

The integer representation remains the canonical representation of quoted prices throughout the current architecture.

Floating-point values are introduced only where a derived metric naturally requires them, such as:

```text
mid-price
relative spread
order-book imbalance
```

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

The current empty-book convention is:

```text
No bid → best_bid() returns 0
No ask → best_ask() returns 0
```

Within the simulator, `0` is reserved as a sentinel for the absence of an available quote and is not used as a valid market price.

This convention was retained rather than introducing `std::optional<Price>` because changing the core representation would unnecessarily propagate through the existing Chapters 1–5 API.

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

When either side is unavailable, the existing `OrderBook` convention returns:

```text
Spread = 0
```

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

Conceptually:

```cpp
struct Trade {
    OrderId incoming_order;
    OrderId resting_order;
    Price price;
    Quantity quantity;
};
```

The order identifiers are stored as `OrderId` values rather than complete `Order` objects.

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

---

## 27. Adding Orders to OrderMap

When an order becomes resting liquidity, the `OrderBook` inserts its location into `OrderMap`.

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

---

## 30. OrderMap Invariant

The central invariant introduced in Chapter 3 is:

```text
Every resting order in the book has exactly one OrderMap entry.

Every OrderMap entry corresponds to exactly one resting order.
```

This invariant remains central to the correctness of the architecture.

When the book changes, the map must remain synchronized with the actual vector-based order storage.

---

## 31. Partial Fills and OrderMap

A partial fill changes an order's quantity but does not change its location inside the price level.

Therefore:

```text
Partial Fill
    ↓
Quantity changes
    ↓
Location unchanged
    ↓
OrderMap unchanged
```

No location update is required unless the order is subsequently removed.

---

## 32. Full Fills and OrderMap

When a resting order is completely filled:

```text
Resting Quantity → 0
```

the order is removed from the book.

The `OrderBook` reuses the cancellation path to remove the fully consumed resting order.

This keeps order-removal logic centralized.

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

The returned pointer is a temporary access mechanism to the actual vector element.

It must not be retained across operations that can erase or reallocate the underlying vector.

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

---

## 35. FIFO Rules for Modification

The current rules are:

### Same Price + Quantity Decrease

The order is modified in place and FIFO is preserved.

### Same Price + Quantity Increase

The order loses queue priority.

The implementation achieves this through cancellation followed by reinsertion.

### Price Change

A price change moves the order into a different price queue and therefore resets its queue priority.

The implementation performs:

```text
cancel(old)

    ↓

add(replacement)
```

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

provided that the order existed.

---

## 37. Order ID and Side Preservation

When an order is replaced, the replacement retains:

```text
Original OrderId
Original Side
```

Only the modified attributes change:

```text
Price
Quantity
```

This preserves order identity while allowing its queue position to change.

---

## 38. OrderMap During Modification

For an in-place modification:

```text
Order
   ↓
Quantity updated
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

The map therefore remains consistent regardless of the modification path.

---

# Part V — Event-Driven Market Simulation

## 39. Event Representation

Chapter 5 introduces an event abstraction above the existing order-book engine.

The event representation is defined in:

```text
cpp/include/lob/event.hpp
```

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

---

## 40. Timestamp and Sequence Number

Each event contains:

```text
timestamp
sequence
```

`timestamp` represents logical simulation time.

`sequence` resolves ordering between multiple events occurring at the same timestamp.

The current model is:

```text
timestamp   sequence

100         0
100         1
100         2

101         0
101         1

102         0
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

The timestamp unit is documented as logical nanoseconds for the current simulator representation.

---

## 41. Simulator

Chapter 5 introduces the `Simulator` abstraction.

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

---

## 42. Simulator Event Dispatch

The simulator dispatches each event to the appropriate `OrderBook` operation.

### ADD

An `ADD` event supplies a complete `Order`.

The simulator rejects duplicate active order IDs.

Otherwise, the simulator calls:

```cpp
OrderBook::process_order()
```

and appends any resulting trades to the simulator's trade history.

### CANCEL

A `CANCEL` event supplies an `OrderId`.

The simulator calls:

```cpp
OrderBook::cancel(order_id)
```

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

The `OrderBook` remains responsible for book-state semantics.

---

## 43. Event Validation

Before an event modifies the order book, the simulator validates temporal ordering and event-specific state.

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

Ordering validation occurs during processing through:

```cpp
process_event()
```

This keeps event-order validation centralized.

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

The vector order is therefore the order in which the simulator attempts to process events.

Timestamp and sequence validation determine whether each event is accepted.

---

## 46. Simulation Clock

The simulator maintains:

```cpp
Timestamp current_time;
Sequence last_sequence;
```

These represent the temporal state of the most recently accepted event.

When a timestamp advances, the sequence is reset according to the event-ordering convention:

```text
timestamp = new timestamp
sequence  = 0
```

This makes the event stream deterministic.

---

## 47. Duplicate Order Protection

The simulator prevents duplicate `ADD` events from creating two active resting orders with the same `OrderId`.

Before processing an `ADD`:

```text
ADD Event
   ↓
Does OrderId already exist?
   ├── YES → Reject
   └── NO  → Process
```

This protects the identity invariant:

```text
One active resting order
    ↔
One unique OrderId
```

---

## 48. Trade History

The simulator maintains:

```cpp
std::vector<Trade> trades;
```

When an accepted `ADD` event generates executions, the resulting trades are appended to this vector.

The trade history represents executions generated during the simulation.

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

This trade history becomes an input to later execution, slippage, risk, and microstructure analysis.

---

# Part VI — Market Microstructure Metrics

## 49. Metrics Layer Overview

Chapter 6 introduces the first analytical layer above the simulation engine.

The metrics layer is designed to answer:

```text
What does the current simulated market state look like?
```

without changing that state.

The architecture is:

```text
OrderBook ──────────────┐
                        |
Trade History ──────────┤
                        v
                 Metric Functions
                        |
                        v
                  MarketMetrics
```

The metrics layer does not own:

* orders
* price levels
* the order map
* simulator events
* trades

It only reads existing data and computes derived quantities.

This prevents duplicated market state and keeps analytical logic separate from execution logic.

---

## 50. Metrics Files

Chapter 6 introduces:

```text
cpp/include/lob/metrics.hpp
cpp/src/metrics.cpp
tests/cpp/test_metrics.cpp
```

The metrics implementation is therefore separated from:

```text
OrderBook
Simulator
Matching Engine
```

The intended responsibility split is:

```text
OrderBook

    Owns market state

Simulator

    Drives event processing
    Stores trade history

Metrics

    Observes existing state
    Computes derived statistics
```

---

## 51. MarketMetrics Data Structure

`MarketMetrics` is a result container rather than a calculation class.

Conceptually:

```cpp
struct MarketMetrics {
    Price best_bid;
    Price best_ask;

    MidPrice mid_price;
    Price spread;
    RelativeSpread relative_spread;

    Quantity bid_depth;
    Quantity ask_depth;

    Imbalance imbalance;

    TradeCount trade_count;
    Quantity trade_volume;
};
```

The calculation logic remains in standalone functions.

This keeps the result representation simple and prevents analytical behavior from becoming coupled to the state container.

---

## 52. Metric Types

The current derived metric types include:

```cpp
using MidPrice = double;
using RelativeSpread = double;
using Imbalance = double;
using TradeCount = std::size_t;
```

Quoted prices and quantities remain integer-based:

```cpp
Price
Quantity
```

Floating-point representation is used only for metrics that require ratios or fractional prices.

---

## 53. Best Bid and Best Ask Metrics

The metrics layer exposes:

```cpp
Price calculate_best_bid(const OrderBook& book);

Price calculate_best_ask(const OrderBook& book);
```

These functions delegate directly to:

```cpp
book.best_bid();
book.best_ask();
```

No second copy of top-of-book state is maintained.

---

## 54. Mid-Price

The mid-price is defined as:

```text
Mid = (Best Bid + Best Ask) / 2
```

Because prices are integer ticks, the resulting mid-price can fall between ticks.

For example:

```text
Best Bid = 10000
Best Ask = 10501

Mid = (10000 + 10501) / 2
    = 10250.5
```

Therefore:

```cpp
MidPrice
```

is represented using `double`.

When either side of the book is unavailable, the current convention is:

```text
Mid = 0.0
```

---

## 55. Absolute Spread

The absolute spread is:

```text
Spread = Best Ask − Best Bid
```

The metrics layer delegates this calculation to the existing:

```cpp
OrderBook::spread()
```

This avoids duplicating spread logic.

---

## 56. Relative Spread

Relative spread normalizes the absolute spread by the mid-price.

The formula used is:

```text
Relative Spread
    =
(Best Ask − Best Bid)
--------------------- × 100
         Mid
```

The result is expressed as a percentage.

Example:

```text
Best Bid = 10000
Best Ask = 10500
Mid      = 10250

Spread = 500

Relative Spread
    = (500 / 10250) × 100
    ≈ 4.87805%
```

When the mid-price is unavailable:

```text
Relative Spread = 0.0
```

This follows the same sentinel convention already established by the order-book layer.

---

## 57. Bid Depth

Bid depth measures the total quantity currently resting on the bid side.

The current implementation sums:

```text
total_quantity
```

across all bid price levels.

Conceptually:

```text
BIDS

10000 → 20
9900  → 50
9800  → 75

Bid Depth
= 20 + 50 + 75
= 145
```

The metric is derived from existing `PriceLevel` state.

No second depth structure is maintained.

---

## 58. Ask Depth

Ask depth is calculated analogously.

Conceptually:

```text
ASKS

10500 → 40
10600 → 30
10700 → 20

Ask Depth
= 40 + 30 + 20
= 90
```

The calculation reads:

```cpp
book.ask_levels()
```

and aggregates each level's `total_quantity`.

---

## 59. Order Book Imbalance

The current normalized order-book imbalance is:

```text
Imbalance
    =
(Bid Depth − Ask Depth)
------------------------
(Bid Depth + Ask Depth)
```

The result lies conceptually in:

```text
[-1, +1]
```

Interpretation:

```text
Positive
    → more resting bid liquidity

Negative
    → more resting ask liquidity

Near 0
    → relatively balanced depth
```

Example:

```text
Bid Depth = 20
Ask Depth = 80

Imbalance
= (20 − 80) / (20 + 80)
= −60 / 100
= −0.6
```

When both sides contain zero depth:

```text
Imbalance = 0.0
```

This prevents division by zero.

---

## 60. Trade Count

Trade count measures the number of execution records stored in the simulator's trade history.

The current definition is:

```text
Trade Count = number of Trade objects
```

For example:

```text
Trade 1
Trade 2
Trade 3

Trade Count = 3
```

This is distinct from the quantity traded.

A single incoming order consuming three resting orders therefore contributes:

```text
Trade Count = 3
```

even if all three executions are part of one parent order.

---

## 61. Trade Volume

Total traded volume is:

```text
Trade Volume
    =
Σ Trade Quantity
```

For example:

```text
Trade 1 → 50
Trade 2 → 70
Trade 3 → 80

Total Volume = 200
```

The calculation reads directly from:

```cpp
std::vector<Trade>
```

and does not alter trade history.

---

## 62. Complete Metrics Calculation

The convenience function:

```cpp
MarketMetrics calculate_metrics(
    const OrderBook& book,
    const std::vector<Trade>& trades
);
```

computes all current Chapter 6 metrics in one operation.

The data flow is:

```text
OrderBook
    |
    +-- best bid
    +-- best ask
    +-- spread
    +-- bid levels
    +-- ask levels
    |
    v
Metric Calculations
    |
    v
MarketMetrics

Trade History
    |
    +-- trade count
    +-- trade quantities
    |
    v
MarketMetrics
```

The function returns a value object containing the derived results.

---

## 63. Metrics Layer Does Not Mutate the Book

The metrics API receives the order book as:

```cpp
const OrderBook&
```

and trade history as:

```cpp
const std::vector<Trade>&
```

This reflects the architectural rule:

```text
Metrics observe state.
Metrics do not create or modify market state.
```

Therefore metric calculations cannot intentionally:

* add orders
* cancel orders
* modify orders
* execute trades
* change price levels
* modify the `OrderMap`
* append to trade history

This separation is explicitly tested.

---

## 64. No Duplicated Order-Book State

The metrics layer intentionally does not create:

```text
cached_best_bid
cached_best_ask
cached_bid_depth
cached_ask_depth
```

or another copy of the book.

Instead:

```text
OrderBook
    ↓
Existing source of truth

Metrics
    ↓
Derived observation
```

This avoids creating another synchronization problem.

The order book remains the sole source of truth for market state.

---

## 65. Empty and Missing-Side Conventions

The current metrics layer inherits the existing order-book convention:

```text
best_bid = 0
best_ask = 0
```

when a side is unavailable.

Derived metrics behave as follows:

```text
Empty Book

best bid        = 0
best ask        = 0
mid price       = 0.0
spread          = 0
relative spread = 0.0
bid depth       = 0
ask depth       = 0
imbalance       = 0.0
```

When only one side exists:

```text
best bid / ask
    → existing value on available side

mid price
    → 0.0

relative spread
    → 0.0
```

This convention preserves compatibility with the existing Chapters 1–5 architecture.

---

## 66. Current Metrics API

The Chapter 6 metrics API is:

```cpp
Price calculate_best_bid(const OrderBook& book);

Price calculate_best_ask(const OrderBook& book);

MidPrice calculate_mid_price(const OrderBook& book);

Price calculate_spread(const OrderBook& book);

RelativeSpread calculate_relative_spread(const OrderBook& book);

Quantity calculate_bid_depth(const OrderBook& book);

Quantity calculate_ask_depth(const OrderBook& book);

Imbalance calculate_imbalance(const OrderBook& book);

TradeCount calculate_trade_count(
    const std::vector<Trade>& trades
);

Quantity calculate_trade_volume(
    const std::vector<Trade>& trades
);

MarketMetrics calculate_metrics(
    const OrderBook& book,
    const std::vector<Trade>& trades
);
```

The functions are read-only observations of current market state and recent simulation history.

---

# Part VII — Current Data Structures and Ownership

## 67. Current Data Structures

The current implementation uses:

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

Metrics are represented as:

```cpp
MarketMetrics
```

which contains only derived values.

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

The metrics layer sits above this structure:

```text
OrderBook + Trade History
            |
            v
      Metric Functions
            |
            v
      MarketMetrics
```

---

## 68. Data Ownership

The `OrderBook` owns its bid and ask price levels.

Each `PriceLevel` owns its collection of orders.

Therefore:

```text
OrderBook
    owns PriceLevels
        which own Orders
```

The `OrderMap` is owned by the `OrderBook` and acts as an auxiliary index.

The simulator owns:

```text
events
trades
```

The metrics layer owns neither market state nor trade history.

`MarketMetrics` is a value object containing derived observations only.

This maintains a strict separation:

```text
Market State
    → OrderBook

Simulation History
    → Simulator

Derived Statistics
    → MarketMetrics
```

---

# Part VIII — Current API

## 69. Current OrderBook API

The current `OrderBook` exposes operations equivalent to:

```cpp
void add(const Order& order);

bool cancel(OrderId order_id);

Price best_bid() const;

Price best_ask() const;

Price spread() const;

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

const std::vector<PriceLevel>& bid_levels() const;

const std::vector<PriceLevel>& ask_levels() const;
```

### Mutations

```text
add()
cancel()
process_order()
modify()
```

### Queries / Accessors

```text
best_bid()
best_ask()
spread()
find_order()
bid_levels()
ask_levels()
```

The `bid_levels()` and `ask_levels()` accessors provide read-only access to existing price-level state for analytical calculations such as depth.

---

## 70. Current OrderMap API

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

The map remains an auxiliary index rather than an ownership structure.

---

## 71. Current Simulator API

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

Responsibilities are:

```text
Simulator

add_event()
    Stores an event

process_event()
    Validates and executes one event

process_events()
    Processes the stored event stream sequentially
```

---

## 72. Current Metrics API

The Chapter 6 metrics layer exposes:

```cpp
Price calculate_best_bid(const OrderBook& book);

Price calculate_best_ask(const OrderBook& book);

MidPrice calculate_mid_price(const OrderBook& book);

Price calculate_spread(const OrderBook& book);

RelativeSpread calculate_relative_spread(const OrderBook& book);

Quantity calculate_bid_depth(const OrderBook& book);

Quantity calculate_ask_depth(const OrderBook& book);

Imbalance calculate_imbalance(const OrderBook& book);

TradeCount calculate_trade_count(
    const std::vector<Trade>& trades
);

Quantity calculate_trade_volume(
    const std::vector<Trade>& trades
);

MarketMetrics calculate_metrics(
    const OrderBook& book,
    const std::vector<Trade>& trades
);
```

The metrics layer has no market-state mutation operations.

---

# Part IX — Order Lifecycle

## 73. Resting Order Lifecycle

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

## 74. Incoming Executable Order

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

---

## 75. Modified Order Lifecycle

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

---

## 76. Event-Driven Order Lifecycle

The complete event-driven path is now:

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

---

## 77. Metrics Observation Lifecycle

Chapter 6 adds a read-only analytical path:

```text
OrderBook
    |
    +── Best Bid
    +── Best Ask
    +── Spread
    +── Price Levels
    |
    v
Metrics Layer

Trade History
    |
    +── Trade Count
    +── Trade Quantities
    |
    v
Metrics Layer

Metrics Layer
    |
    +── Mid Price
    +── Relative Spread
    +── Depth
    +── Imbalance
    +── Trade Statistics
    |
    v
MarketMetrics
```

Unlike the order lifecycle, the metrics lifecycle does not modify state.

---

# Part X — Testing

## 78. Chapter 1 Testing

Chapter 1 includes:

```text
tests/cpp/test_order_book.cpp
```

The tests verify:

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

## 79. Chapter 2 Testing

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

## 80. Chapter 3 Integration Testing

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

---

## 81. Chapter 4 Testing

Chapter 4 extends the same integration suite with direct tests for order lookup and modification.

The tests verify:

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

---

## 82. Chapter 5 Simulator Testing

Chapter 5 introduces:

```text
tests/cpp/test_simulator.cpp
```

The simulator test suite verifies:

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

The current Chapter 5 test output is:

```text
All Chapter 5 simulator tests passed.
```

---

## 83. Chapter 6 Metrics Testing

Chapter 6 introduces:

```text
tests/cpp/test_metrics.cpp
```

The metrics test suite verifies:

* best bid
* best ask
* mid-price
* half-tick mid-price
* absolute spread
* relative spread
* bid depth
* ask depth
* order-book imbalance
* empty-book behavior
* zero-depth behavior
* missing bid behavior
* missing ask behavior
* trade count
* trade volume
* empty trade history
* multiple trades
* partial-fill-style trade histories
* complete `MarketMetrics` calculation
* non-mutation of order-book state
* non-mutation of trade history

The Chapter 6 test suite passed successfully:

```text
All Chapter 6 metrics tests passed!
```

This establishes the metrics layer as a verified analytical layer above the existing simulator.

---

## 84. Test Philosophy

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

Chapter 6 extends it again to the analytical layer.

The metrics tests verify that:

```text
Existing Market State
        ↓
Metric Calculation
        ↓
Derived Results
```

does not alter the original:

```text
OrderBook
Trade History
```

This confirms that analytical code remains separated from simulation state.

---

# Part XI — Simulator Demonstration

## 85. Simulator Executable

The simulator executable is:

```text
cpp/app/simulate_main.cpp
```

It demonstrates the current order-book, matching, tracking, modification, event-driven simulation, and market-metrics behavior.

The demonstration includes:

* `ADD` events
* `CANCEL` events
* `MODIFY` events
* aggressive BUY events
* aggressive SELL events
* timestamps
* sequence numbers
* final market metrics

The simulator processes the events as one deterministic stream.

---

## 86. Representative Final Simulation State

The Chapter 5 demonstration produces:

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

---

## 87. Chapter 6 Metrics Demonstration

The simulator now also computes:

```text
===== MARKET METRICS =====
Best Bid: 10000
Best Ask: 10600
Mid Price: 10300
Spread: 600
Relative Spread: 5.82524%
Bid Depth: 20
Ask Depth: 80
Order Book Imbalance: -0.6
Trade Count: 3
Total Traded Volume: 200
```

These values are internally consistent with the final simulated state:

```text
Mid Price
= (10000 + 10600) / 2
= 10300
```

```text
Relative Spread
= (600 / 10300) × 100
≈ 5.82524%
```

```text
Imbalance
= (20 − 80) / (20 + 80)
= −0.6
```

```text
Trade Volume
= 50 + 70 + 80
= 200
```

The demonstration therefore validates the connection between:

```text
Simulator
    ↓
Final OrderBook
    +
Trade History
    ↓
Metrics Layer
    ↓
MarketMetrics
```

---

# Part XII — Current Scope

## 88. Scope of Chapters 1–6

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
* best bid / ask metrics
* mid-price
* half-tick mid-price support
* absolute spread metric
* relative spread metric
* bid depth
* ask depth
* normalized order-book imbalance
* trade count
* total trade volume
* empty-book metric handling
* missing-side metric handling
* read-only metrics calculations
* aggregated `MarketMetrics` result
* metrics non-mutation guarantees
* dedicated Chapter 6 metrics tests

The project intentionally does not yet implement:

* high-volume event generation
* NASDAQ ITCH parsing
* historical replay
* queue-position analytics
* detailed market-order execution modeling
* slippage models
* market-impact models
* inventory management
* P&L
* risk metrics
* Python bindings
* zero-copy research pipelines
* performance benchmarking at scale
* production-level performance optimization
* time-series snapshot metrics
* VWAP/TWAP
* realized volatility
* volatility forecasting
* HAR/GARCH integration

These features will be introduced progressively in later chapters.

---

# Part XIII — Current Performance Model

## 89. Current Performance Model

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

Bid / ask depth calculation
    → traversal of all corresponding price levels

Trade count
    → O(1) from vector size

Trade volume
    → linear in trade-history length

Complete metrics calculation
    → dominated by depth and trade-volume traversal
```

The metrics layer does not introduce a new mutable cache or duplicate structure for performance.

Future optimization decisions will therefore be measured rather than assumed.

---

# Part XIV — Architectural Progression

## 90. Architecture After Chapter 6

The architecture has progressed from basic book representation to execution, indexed order tracking, explicit modification semantics, deterministic event-driven simulation, and finally read-only market microstructure analysis.

```text
                         Event
                           |
                           v
                      Simulator
                      /       \
                     v         v
                OrderBook   Trade History
                /     | \
               /      |  \
          OrderMap  Modify Matching
                            Engine
                               |
                               v
                             Trade
                               |
                  +------------+------------+
                  |                         |
                  v                         v
             Book State              Trade History
                  |                         |
                  +------------+------------+
                               |
                               v
                       Microstructure
                           Metrics
                               |
                               v
                        MarketMetrics
```

The division of responsibilities is now:

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

Metrics

    Observes existing market state and trade history

MarketMetrics

    Stores derived market statistics
```

---

## 91. Six Fundamental Layers

The first six chapters establish six fundamental functions of the simulator.

### Chapter 1 — Represent Liquidity

```text
Individual Order
        ↓
Price Level
        ↓
Order Book
```

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

### Chapter 5 — Drive Liquidity Through Events

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

### Chapter 6 — Measure Market State

```text
OrderBook + Trade History
            ↓
      Metrics Functions
            ↓
      MarketMetrics
```

The six chapters therefore establish:

```text
Chapter 1 → Represent Liquidity
Chapter 2 → Consume Liquidity
Chapter 3 → Locate Liquidity
Chapter 4 → Modify Liquidity
Chapter 5 → Drive Liquidity Through Events
Chapter 6 → Measure Liquidity and Executions
```

This creates a complete first analytical layer on top of the core market mechanism.

---

# Part XV — Event-Driven Architecture

## 92. Event-Driven Architecture

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

Chapter 6 observes the resulting state:

```text
Book State
     +
Trade History
     ↓
Microstructure Metrics
```

This creates a clean separation between:

```text
What happened?

    Event
```

```text
What does the event do?

    OrderBook
```

and:

```text
What does the resulting market state look like?

    Metrics
```

---

# Part XVI — Metrics Architecture Principles

## 93. Metrics Are Derived State, Not Primary State

The order book is the source of truth for:

```text
best prices
price levels
orders
resting quantities
book liquidity
```

Trade history is the source of truth for:

```text
executions
trade quantities
trade count
```

Metrics are derived from these sources:

```text
Primary State
     ↓
Derived Observation
```

The metrics layer should therefore not become a second market-state store.

---

## 94. Read-Only Analytical Layer

The Chapter 6 architecture intentionally uses:

```cpp
const OrderBook&
const std::vector<Trade>&
```

to reinforce the rule that metrics observe rather than mutate.

This makes the analytical layer easier to reason about and reduces the possibility of analytical code corrupting simulation state.

---

## 95. Integer Prices With Floating Derived Metrics

The project keeps:

```text
Price
```

as an integer tick representation.

Metrics that require fractional values use:

```text
double
```

This produces the desired separation:

```text
Quoted Market State
    → integer ticks

Derived Continuous Value
    → floating point
```

The most important example is the mid-price, which may occur at a half-tick.

---

## 96. No Optional Price Migration Yet

The current implementation uses:

```text
0
```

as the sentinel for an unavailable bid or ask.

The project intentionally does not migrate the core price API to:

```cpp
std::optional<Price>
```

at this stage.

Changing that representation would require broader API changes across the existing order-book, matching, simulator, and testing layers without providing a necessary benefit for the current scope.

---

# Part XVII — Deterministic Replay Principle

## 97. Deterministic Replay Principle

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

and therefore the same derived Chapter 6 metrics.

The full deterministic chain is:

```text
Initial State
     +
Event Stream
     ↓
Simulator
     ↓
OrderBook
     ↓
Trade History
     ↓
Metrics
     ↓
MarketMetrics
```

This becomes increasingly important as later chapters introduce historical replay and research.

---

# Part XVIII — Planned Architecture

## 98. Planned Architecture

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

The current architecture now has the first version of:

```text
LOB State
     ↓
Microstructure Features
```

The next stage will build execution-cost analysis on top of that layer.

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
              ↓
       Execution / Risk
              ↓
        Python Research
```

will allow the same simulation engine to support increasingly realistic quantitative research workflows.

---

# Part XIX — Future Chapter Roadmap

## 99. Chapter Roadmap

The planned development sequence is:

```text
Chapter 1
Basic Limit Order Book
        ↓
Chapter 2
Matching & Execution Engine
        ↓
Chapter 3
Individual Order Tracking
        ↓
Chapter 4
Order Modification / Replace
        ↓
Chapter 5
Event-Driven Market Simulation
        ↓
Chapter 6
Market Microstructure Metrics
        ↓
Chapter 7
Slippage / Cost / Market Impact
        ↓
Chapter 8
Inventory / P&L / Risk
        ↓
Chapter 9
Historical Data / ITCH Replay
        ↓
Chapter 10
Performance / Benchmarking
        ↓
Chapter 11
C++ → Python Integration
        ↓
Chapter 12
Quant Research / Out-of-Sample Integration
```

### Chapter 7 — Slippage / Cost / Market Impact

The next analytical layer will use the existing:

```text
Trade History
OrderBook
```

to study:

* execution price relative to a reference price
* spread costs
* realized execution cost
* liquidity consumption
* multi-level execution
* market impact
* adverse price movement after execution

This chapter will build on Chapter 6 rather than duplicating its metrics.

### Chapter 8 — Inventory / P&L / Risk

This stage will introduce:

```text
position
cash
mark-to-market value
realized P&L
unrealized P&L
inventory limits
risk statistics
```

### Chapter 9 — Historical Data / ITCH Replay

The event representation will eventually be connected to real historical market-data formats.

The goal is to transform external market-data messages into the simulator's internal event representation and process them through the same deterministic engine.

### Chapter 10 — Performance / Benchmarking

Performance work will be based on measurement:

```text
Benchmark
    ↓
Profile
    ↓
Identify bottleneck
    ↓
Optimize
    ↓
Benchmark again
    ↓
Verify correctness
```

No major container redesign will be introduced merely for theoretical complexity improvements.

### Chapter 11 — C++ → Python Integration

The C++ engine will eventually expose data and computations to Python for:

```text
research
visualization
statistics
feature analysis
experimentation
```

### Chapter 12 — Quant Research / Out-of-Sample Integration

The final stage will connect the simulator with rigorous quantitative experimentation, including:

```text
historical data
feature generation
hypothesis testing
train/test separation
out-of-sample evaluation
parameter stability
failure-mode analysis
```

---

# Part XX — Current Repository Structure

## 100. Repository Structure

The current repository structure is:

```text
hybrid-lob-simulator/
├── cpp/
│   ├── include/lob/
│   │   ├── types.hpp
│   │   ├── order.hpp
│   │   ├── price_level.hpp
│   │   ├── order_book.hpp
│   │   ├── order_map.hpp
│   │   ├── trade.hpp
│   │   ├── event.hpp
│   │   ├── simulator.hpp
│   │   └── metrics.hpp
│   │
│   ├── src/
│   │   ├── order_book.cpp
│   │   ├── order_map.cpp
│   │   ├── simulator.cpp
│   │   └── metrics.cpp
│   │
│   └── app/
│       └── simulate_main.cpp
│
├── tests/
│   └── cpp/
│       ├── test_order_book.cpp
│       ├── test_matching_engine.cpp
│       ├── test_simulator.cpp
│       └── test_metrics.cpp
│
└── docs/
    └── ARCHITECTURE.md
```

The metrics layer is therefore integrated as a first-class analytical component without altering the ownership model established in Chapters 1–5.

---

# Part XXI — Design Principles

## 101. Correctness Before Optimization

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

      +

Correct Microstructure Metrics

      =

Reliable Simulation Core
```

Only after these invariants are stable should the project introduce more specialized data structures or performance optimizations.

---

## 102. Single Source of Truth

The system maintains clear ownership boundaries:

```text
OrderBook
    → source of truth for market state

Simulator
    → source of truth for event stream and generated trade history

Metrics
    → derived observations only
```

This principle prevents unnecessary duplication and synchronization problems.

---

## 103. Explicit Invariants

The main invariants established so far are:

```text
OrderMap invariant

Every resting order
    ↔
Exactly one OrderMap entry
```

```text
FIFO invariant

Orders at the same price
    →
Stored and matched in queue order
```

```text
Event invariant

Accepted events
    →
Follow timestamp / sequence ordering
```

```text
Metrics invariant

Metric calculations
    →
Do not mutate market state
```

These invariants provide the foundation for later replay and research work.

---

## 104. Measured Optimization

The architecture intentionally avoids premature optimization.

Future changes should follow:

```text
Measure

   ↓

Profile

   ↓

Find bottleneck

   ↓

Change implementation

   ↓

Benchmark

   ↓

Verify correctness
```

The project should become faster because a measured bottleneck was addressed, not because a more complicated data structure merely appears faster in theory.

---

## 105. Architectural Summary

The first six chapters now establish the complete initial simulation stack:

```text
                         Event
                           |
                           v
                      Simulator
                      /       \
                     v         v
                OrderBook   Trade History
                /     | \
               /      |  \
          OrderMap  Modify Matching
                            Engine
                               |
                               v
                             Trade
                               |
                               v
                     Microstructure Metrics
                               |
                               v
                        MarketMetrics
```

The architectural progression is:

```text
Chapter 1
Represent Liquidity

Chapter 2
Consume Liquidity

Chapter 3
Locate Liquidity

Chapter 4
Modify Liquidity

Chapter 5
Drive Liquidity Through Events

Chapter 6
Measure Liquidity and Executions
```

Together, these chapters establish a correct and deterministic simulation core with a first analytical layer above it.

The implementation deliberately favors:

```text
correctness
determinism
explicit ownership
transparent state transitions
testability
measured optimization
```

over premature complexity.

Future chapters will extend this foundation toward:

```text
execution-cost modeling
risk
historical market-data replay
performance engineering
Python integration
quantitative research
out-of-sample validation
```

while preserving the fundamental ownership model, `OrderMap` synchronization invariant, FIFO semantics, deterministic event-processing rules, and read-only metrics architecture established through Chapters 1–6.
