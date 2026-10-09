# Nasdaq TotalView-ITCH 5.0 Format

## 1. Purpose

This document describes the Nasdaq TotalView-ITCH 5.0 market-data format used by the Hybrid Limit Order Book Simulator.

It covers:

- Binary message structure and field encoding
- Stock Locate and timestamp semantics
- Price representation
- Order lifecycle messages
- The relationship between ITCH messages and the internal replay operations
- The message types supported by the project's displayed-order-book reconstruction path
- Error handling and replay invariants
- The historical dataset used for development and testing

The official protocol reference is the [Nasdaq TotalView-ITCH 5.0 specification](https://www.nasdaqtrader.com/content/technicalsupport/specifications/dataproducts/NQTVITCHSpecification.pdf).

This document describes the protocol concepts and the project's supported subset. It does not replace the official specification for complete field definitions or protocol-version-specific details.

## 2. What Is ITCH?

Nasdaq TotalView-ITCH is a market-data protocol that communicates information about orders, order executions, trading events, and security metadata.

It provides order-level information that can be used to reconstruct a displayed limit order book.

Unlike a CSV file containing rows of prices and quantities, ITCH communicates a sequence of events.

For example:

```text
Add Order
    ↓
Add Order
    ↓
Order Executed
    ↓
Order Cancelled
    ↓
Order Replaced
```

Each event describes a change or information item in the historical market stream.

The receiver processes messages in feed order and updates its local representation of the market.

ITCH is an outbound market-data protocol. It reports exchange activity; it is not a protocol for submitting orders to Nasdaq.

## 3. Binary Message Structure

ITCH messages are binary records. Each message has a type identifier and a type-specific set of fields.

Message lengths vary according to the message type.

The first byte identifies the message type. For the message families documented here, the common header contains:

| Field           | Offset |  Length | Description                                     |
| --------------- | -----: | ------: | ----------------------------------------------- |
| Message Type    |      0 |  1 byte | Identifies the message format                   |
| Stock Locate    |      1 | 2 bytes | Identifies the security within the feed session |
| Tracking Number |      3 | 2 bytes | Nasdaq internal tracking number                 |
| Timestamp       |      5 | 6 bytes | Nanoseconds since midnight                      |

Offsets are zero-based and measured from the beginning of the ITCH message payload.

These common fields occupy the first 11 bytes of the message. The fields that follow depend on the message type.

### Binary encoding rules

The principal encoding rules are:

- Integer fields are big-endian, also called network byte order.
- Integer fields are unsigned unless the specification states otherwise.
- Alpha fields are ASCII, left-justified, and generally padded with spaces on the right.
- Price fields use integer fixed-point encoding with an explicitly specified precision.
- Timestamps are represented as nanoseconds since midnight.

A parser must interpret the field according to its type and width. It must not assume that an integer is encoded as ASCII text or that a message terminates with a null byte.

## 4. File Framing and Message Parsing

The compressed historical file is a container around the recorded feed data. After decompression, the contents should not automatically be treated as ordinary text simply because the filename has a `.txt` extension.

The project separates reading and parsing:

```text
Historical file
      ↓
ItchFileReader
      ↓
Raw message bytes
      ↓
ItchParser
      ↓
ItchMessage or ParseError
```

### `ItchFileReader`

The reader is responsible for reading the historical stream and returning the next raw message.

Its Python binding exposes `next_message()`, which returns a result representing one of:

- Raw message bytes
- `EndOfFile`
- `FileReaderError`

The reader handles the record framing used by the historical input. Higher-level research code should not independently read message bytes or duplicate this framing logic.

### `ItchParser`

The parser interprets a raw message according to its message type and field layout.

Conceptually:

```text
Raw message bytes
       ↓
Read message type
       ↓
Select message-specific parser
       ↓
Decode fields
       ↓
Validate message structure
       ↓
Return typed ITCH message
```

A parsed message is represented by the project's `ItchMessage` variant.

If a record cannot be parsed successfully, the parser returns a `ParseError` rather than pretending that the message is valid.

## 5. Stock Locate

The Stock Locate field identifies a security within the ITCH feed.

Nasdaq assigns locate codes for a trading day and communicates security information through Stock Directory messages. Locate assignments must not be assumed to remain identical across different trading days.

The project uses a configured `stock_locate` when creating `ItchMapper`.

For the development dataset, the selected value is:

```text
Stock Locate: 123
Trading date: 2019-10-18
```

This selects the intended book-reconstruction scope for that historical session.

Do not assume Stock Locate `123` represents the same security on another date. The mapping between a locate code and a symbol is session-specific and should be verified against the appropriate security-directory information.

## 6. Timestamps

ITCH timestamps represent **nanoseconds since midnight**, not Unix timestamps.

For a raw timestamp \(t\_{ns}\), conversion to integer milliseconds is:

$$
t_{ms} = \left\lfloor \frac{t_{ns}}{1{,}000{,}000} \right\rfloor
$$

For example, a raw timestamp of:

```text
34,245,000,000,000 nanoseconds
```

converts to:

```text
34,245,000 milliseconds since midnight
```

This corresponds to 09:30:45.000 in time-of-day notation.

The actual date comes from the historical dataset or its metadata; it is not encoded as part of this time-of-day value.

### Timestamp precision

Converting nanoseconds to milliseconds discards sub-millisecond precision. This is suitable for certain display and sampling purposes, but the raw timestamp should be retained if later research requires finer temporal resolution.

Messages must remain in the original feed order. They should not be reordered merely because two messages share the same timestamp.

The snapshot layer uses the following intended fields:

```text
timestamp_ms
message_index
```

`timestamp_ms` represents the time-of-day in milliseconds, while `message_index` identifies the position of the captured message within the source stream.

The message index and timestamp serve different purposes: the first records stream position, while the second records the market-data time.

## 7. Price Representation

ITCH price fields marked `Price (4)` are integer fixed-point values with four implied decimal places.

For example, the raw ITCH integer:

```text
1,000,000
```

represents:

```text
100.0000
```

in decimal price notation.

The project's C++ core uses its established integer `Price` representation, with the convention:

```text
100.00 → 10000
```

The conversion between the ITCH `Price (4)` representation and the internal `Price` representation belongs at the `ItchMapper` boundary.

Do not pass a raw ITCH price integer directly into the OrderBook without applying the conversion defined by the mapper.

Price handling must remain deterministic and integer-based. Avoid introducing floating-point price conversion into the historical reconstruction path.

**Precision limitation:** the internal price scale and the precision of the input feed must be compatible. If the internal representation supports less precision than the source field, the conversion may lose sub-unit price information. That limitation must be understood before using the reconstructed prices for precision-sensitive research.

## 8. Operational Message Types

The project's current displayed-book reconstruction path uses the following principal message types.

| Type | Message                         | Relevant information                                                            |
| ---- | ------------------------------- | ------------------------------------------------------------------------------- |
| `A`  | Add Order, no MPID attribution  | Order reference, side, shares, stock, price                                     |
| `F`  | Add Order with MPID attribution | Same core order fields as `A`, plus attribution                                 |
| `E`  | Order Executed                  | Existing order reference and executed shares                                    |
| `C`  | Order Executed With Price       | Existing order reference, executed shares, execution price, printable indicator |
| `X`  | Order Cancel                    | Existing order reference and cancelled shares                                   |
| `D`  | Order Delete                    | Existing order reference                                                        |
| `U`  | Order Replace                   | Original and new references, new quantity and price                             |
| `R`  | Stock Directory                 | Security metadata                                                               |
| `S`  | System Event                    | Feed or market-system event                                                     |

This is the project's operational subset, not a list of every message defined by ITCH 5.0.

## 9. Add Order — `A`

An Add Order message introduces a new displayed order.

The message includes:

| Field                  | Offset | Length |
| ---------------------- | -----: | -----: |
| Message Type           |      0 |      1 |
| Stock Locate           |      1 |      2 |
| Tracking Number        |      3 |      2 |
| Timestamp              |      5 |      6 |
| Order Reference Number |     11 |      8 |
| Buy/Sell Indicator     |     19 |      1 |
| Shares                 |     20 |      4 |
| Stock                  |     24 |      8 |
| Price                  |     32 |      4 |

The side indicator is:

```text
B = Buy
S = Sell
```

The project maps this message to an `AddOperation`.

```text
ITCH A
   ↓
Parse fields
   ↓
ItchMapper
   ↓
AddOperation
   ↓
ItchReplay
   ↓
C++ OrderBook
```

The order reference number becomes the identifier used to track the order during subsequent updates.

## 10. Add Order with MPID Attribution — `F`

Message `F` contains the same core order information as message `A`, followed by a market-participant attribution field.

The attribution is four ASCII bytes.

The message-specific fields are:

| Field                  | Offset | Length |
| ---------------------- | -----: | -----: |
| Order Reference Number |     11 |      8 |
| Buy/Sell Indicator     |     19 |      1 |
| Shares                 |     20 |      4 |
| Stock                  |     24 |      8 |
| Price                  |     32 |      4 |
| Attribution            |     36 |      4 |

The project maps `F` to an `AddOperation`, just as it maps `A`.

Attribution is additional message information. It does not change the core responsibility of the OrderBook to maintain order identity, side, quantity, and price.

## 11. Order Executed — `E`

Message `E` indicates that an existing displayed order has been executed in whole or in part.

Its relevant fields are:

| Field                  | Offset | Length |
| ---------------------- | -----: | -----: |
| Order Reference Number |     11 |      8 |
| Executed Shares        |     19 |      4 |
| Match Number           |     23 |      8 |

The message refers to an existing order; it does not introduce a new displayed order.

The project maps `E` to a `ReduceOperation`.

```text
ITCH E
   ↓
Find existing order
   ↓
Reduce displayed quantity
   ↓
Update OrderBook
```

The project must not route this historical execution back through the synthetic matching engine to create another trade. The execution is already an event reported by the historical feed.

## 12. Order Executed With Price — `C`

Message `C` reports an execution of a displayed order at a price different from its initial display price.

Its relevant fields include:

| Field                  | Offset | Length |
| ---------------------- | -----: | -----: |
| Order Reference Number |     11 |      8 |
| Executed Shares        |     19 |      4 |
| Match Number           |     23 |      8 |
| Printable Indicator    |     31 |      1 |
| Execution Price        |     32 |      4 |

The printable indicator describes whether the execution should be reflected in time-and-sales displays and volume calculations.

The project maps `C` to a `ReduceOperation` for book reconstruction. The displayed order's remaining quantity must be updated, while the execution-price and printable fields remain relevant to trade-reporting research.

The displayed-book reconstruction path should not be mistaken for a complete reconstruction of every time-and-sales statistic. Book updates and trade-reporting requirements are related but distinct.

## 13. Order Cancel — `X`

Message `X` removes part of an existing order's displayed quantity.

Its relevant fields are:

| Field                  | Offset | Length |
| ---------------------- | -----: | -----: |
| Order Reference Number |     11 |      8 |
| Cancelled Shares       |     19 |      4 |

For example, suppose an existing order has:

```text
Order reference: 5001
Price: 10000
Quantity: 100
```

A cancellation of 30 shares results in:

```text
Order reference: 5001
Price: 10000
Quantity: 70
```

The project maps `X` to a `ReduceOperation`.

The cancellation quantity must be subtracted from the existing displayed order, not interpreted as the order's new total quantity.

## 14. Order Delete — `D`

Message `D` indicates that an existing order is being removed from the displayed book.

Its relevant field is:

| Field                  | Offset | Length |
| ---------------------- | -----: | -----: |
| Order Reference Number |     11 |      8 |

Unlike a partial cancellation, a deletion removes the order's remaining displayed quantity.

The project maps `D` to a `RemoveOperation`.

```text
ITCH D
   ↓
Locate order
   ↓
Remove order
   ↓
Update price-level and order-tracking state
```

The operation must maintain the C++ OrderBook's existing invariants.

## 15. Order Replace — `U`

Message `U` cancels and replaces an existing order.

Its relevant fields are:

| Field                           | Offset | Length |
| ------------------------------- | -----: | -----: |
| Original Order Reference Number |     11 |      8 |
| New Order Reference Number      |     19 |      8 |
| New Displayed Quantity          |     27 |      4 |
| New Price                       |     31 |      4 |

The message supplies a new order reference number. The new identifier must be used for subsequent messages.

The replacement message does not contain the side field. The replay implementation must therefore retain or recover the side from the original order.

The project maps `U` to a `ReplaceOperation`.

Conceptually:

```text
Existing order
      ↓
Identify old reference
      ↓
Recover existing side
      ↓
Remove old order
      ↓
Create replacement using new reference,
new quantity, and new price
```

This protocol operation is not identical to the synthetic `OrderBook::modify()` interface.

The synthetic interface has its own established FIFO behavior. ITCH replacement must follow the feed's replacement semantics: the original order ceases to exist under its old reference, and the replacement uses the new reference.

If the original order cannot be found, the replacement cannot safely infer the missing side from the replacement message alone. The replay path must report the appropriate error rather than invent an order state.

## 16. Stock Directory — `R`

Message `R` supplies security metadata, including the stock symbol and other directory fields.

It is not an order addition, cancellation, execution, or replacement.

The current project maps `R` to an ignored replay message for the displayed-book reconstruction path.

Ignoring `R` for this narrow purpose does not mean the message is meaningless. Stock Directory information is important when resolving Stock Locate to a security symbol and when displaying or validating security metadata.

## 17. System Event — `S`

Message `S` reports system or feed events, such as start- and end-of-session markers.

The event code appears at offset 11.

This message does not itself create or change an individual displayed order.

The current project maps `S` to an ignored replay message for the OrderBook reconstruction path.

Ignoring it for book mutation does not mean session events should be discarded by every future application. A session-aware replay player may use such events to present trading-session boundaries or validate the completeness of a recorded stream.

## 18. Mapper and Replay Architecture

The historical pipeline is deliberately separate from synthetic event processing.

```text
ITCH file
    ↓
ItchFileReader
    ↓
ItchParser
    ↓
ItchMessage
    ↓
ItchMapper(stock_locate)
    ↓
ReplayOperation
    ↓
ItchReplay
    ↓
C++ OrderBook
```

The central mapping is:

| ITCH type | Internal operation |
| --------- | ------------------ |
| `A`       | `AddOperation`     |
| `F`       | `AddOperation`     |
| `E`       | `ReduceOperation`  |
| `C`       | `ReduceOperation`  |
| `X`       | `ReduceOperation`  |
| `D`       | `RemoveOperation`  |
| `U`       | `ReplaceOperation` |
| `R`       | Ignored message    |
| `S`       | Ignored message    |

The mapper translates protocol messages into the internal operation model. The replay engine applies these operations to the existing C++ OrderBook.

The ITCH replay path must not use the synthetic `Simulator` and matching engine to reproduce historical executions. The recorded feed already states which order-book changes occurred.

## 19. Error Handling

The parser, mapper, and replay engine have separate responsibilities and separate failure modes.

### Parsing errors

A `ParseError` means the raw message could not be interpreted successfully.

Examples include malformed or incomplete message data.

A parse error must not be silently treated as a valid order-book update.

### Mapping errors

The mapper translates parsed messages into supported replay operations.

An unsupported or otherwise non-book message may be represented as an ignored-message result when the implementation explicitly defines that behavior.

Other mapping errors must be surfaced rather than silently converted into an operation.

### Replay errors

Replay errors occur when an operation cannot be applied consistently to the current book state.

Examples include an unknown order reference or an invalid lifecycle operation.

The replay engine must not fabricate missing order state to continue processing.

### End of file and reader errors

The reader distinguishes normal end-of-file from a read failure. Reaching EOF is not itself an error; a file-reader error is.

## 20. Historical Dataset Used by the Project

The development dataset is Nasdaq TotalView-ITCH 5.0 for the trading date:

```text
2019-10-18
```

The compressed source file is:

```text
data/itch/2019-10-18/raw/S101819-v50.txt.gz
```

The decompressed historical feed is stored under:

```text
data/itch/2019-10-18/decompressed/S101819-v50.txt
```

The selected Stock Locate used for the established replay workload is:

```text
123
```

These files are development inputs, not part of the Python package's source code.

Raw market data is excluded from Git as a repository artifact under the project's current data-management convention. The availability and redistribution permissions for a dataset must be verified independently before packaging or distributing it.

## 21. Relationship to Book Snapshots

ITCH messages represent events, whereas a `BookSnapshot` represents the book state at a selected observation point.

```text
ITCH event stream
       ↓
Chronological replay
       ↓
Changing C++ OrderBook
       ↓
Snapshot at chosen point
       ↓
BookSnapshot
       ↓
BookDataset
```

A replay session maintains one live OrderBook as messages are applied. Historical states are not retained automatically.

To inspect historical book states, the application must explicitly capture snapshots during replay.

The plotting/replay layer is intended to use snapshot fields such as:

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

A snapshot interval may be defined in terms of message counts or elapsed time, but these are different sampling policies. Every-N-message sampling is not equivalent to every-N-millisecond sampling because messages do not arrive at a constant rate.

## 22. Replay Invariants

The following invariants govern historical reconstruction.

### Preserve source order

Messages are processed in the order supplied by the recorded stream. Do not sort the stream by timestamp before replay.

### Preserve order identity

Order references are required to associate additions with later executions, cancellations, deletions, and replacements.

### Preserve operation semantics

A partial cancellation or execution reduces remaining quantity. A deletion removes the remaining order. A replacement uses the new reference number.

### Filter the intended security

Only messages relevant to the selected Stock Locate should mutate the selected book, according to the mapper's configured filtering behavior.

### Maintain the established OrderBook design

The C++ OrderBook remains the sole authority for displayed order state, price levels, FIFO order, and order tracking.

### Do not re-match historical executions

Historical execution messages update the reconstructed book. They should not trigger a new synthetic matching event.

### Treat ignored messages explicitly

Ignoring a message means it is not applied as a displayed-book operation. It does not mean that every field in the message is irrelevant to every possible research application.

## 23. Scope and Limitations

The current ITCH reconstruction path focuses on the displayed OrderBook and the message types explicitly mapped by the project.

It should not be described as a full implementation of every ITCH 5.0 message family or every market-data reporting use case.

In particular, reconstructing the displayed book is not automatically equivalent to reconstructing:

- All time-and-sales records
- All non-displayed order activity
- Every auction statistic
- Every security-directory attribute
- Every trading-status transition
- A complete order-entry simulation

Additional message types and analytical outputs should be introduced deliberately when a research requirement calls for them.

## 24. Reference

Primary protocol reference:

[Nasdaq TotalView-ITCH 5.0 specification](https://www.nasdaqtrader.com/content/technicalsupport/specifications/dataproducts/NQTVITCHSpecification.pdf)

The feed specification can evolve. When working with a historical file, use the appropriate protocol and file-format documentation for that feed version, rather than assuming every field added in a newer specification exists in an older recording.

The project's implementation files remain the definitive reference for which message variants are actually parsed, mapped, ignored, and applied by this repository.
