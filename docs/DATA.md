# Data

This directory contains the historical market-data inputs, decompressed data, diagnostic sample datasets, and processed outputs used by the Hybrid C++/Python Limit Order Book Simulator.

The data layer is intentionally separate from the C++ source tree. The source code defines how data is decoded, mapped, replayed, and analyzed; this directory contains the actual market-data artifacts.

---

## 1. Dataset Overview

The current historical dataset is:

```text
Date:          2019-10-18
Venue:         Nasdaq
Feed:          Nasdaq TotalView-ITCH 5.0
Primary file:  S101819-v50.txt.gz
```

The dataset is used to test and drive the historical ITCH replay pipeline.

The main conceptual flow is:

```text
Historical ITCH data
        ↓
File reader
        ↓
ITCH parser
        ↓
Typed ItchMessage
        ↓
ITCH mapper
        ↓
ReplayOperation
        ↓
ITCH replay
        ↓
OrderBook
        ↓
Microstructure / execution / inventory analysis
```

---

## 2. Directory Structure

Current layout:

```text
data/
└── itch/
    └── 2019-10-18/
        ├── raw/
        │   ├── S101819-v50.txt.gz
        │   └── decompressed/
        │       └── S101819-v50.txt
        │
        ├── sample/
        │   ├── stock_100.bin
        │   ├── stock_1000.bin
        │   ├── stock_10000.bin
        │   ├── stock_100000.bin
        │   └── stock_123_all.bin
        │
        └── processed/
```

The exact contents of `processed/` may evolve as research and dataset-construction stages are added.

---

## 3. `raw/`

Path:

```text
data/itch/2019-10-18/raw/
```

Purpose:

Contains the original historical ITCH input and its decompressed representation.

Primary compressed file:

```text
S101819-v50.txt.gz
```

Compressed input is kept as the original archival artifact.

The raw dataset should be treated as immutable source data.

---

## 4. `raw/decompressed/`

Path:

```text
data/itch/2019-10-18/raw/decompressed/
```

Current file:

```text
S101819-v50.txt
```

Purpose:

Provides the decompressed ITCH data used directly by the C++ file reader.

The replay and sample-generation applications consume this form rather than requiring repeated decompression.

Conceptually:

```text
S101819-v50.txt.gz
        ↓
decompression
        ↓
S101819-v50.txt
```

---

## 5. Real ITCH Replay Input

The real historical replay pipeline consumes:

```text
data/itch/2019-10-18/raw/decompressed/S101819-v50.txt
```

The production path is:

```text
S101819-v50.txt
        ↓
itch_file_reader
        ↓
raw ITCH message payload
        ↓
itch_parser
        ↓
ItchMessage
        ↓
itch_mapper
        ↓
ReplayOperation / ReplayError
        ↓
itch_replay
        ↓
OrderBook
```

The sample `.bin` files are not replacements for this pipeline.

---

## 6. `sample/`

Path:

```text
data/itch/2019-10-18/sample/
```

Purpose:

Contains compact, reproducible diagnostic datasets generated from the real ITCH stream.

These files exist because inspecting and repeatedly processing the complete ITCH dataset is inconvenient during development.

The sample pipeline is:

```text
Real ITCH
        ↓
create_itch_samples_main
        ↓
custom .bin dataset
        ↓
itch_sample_reader
        ↓
inspect_itch_sample_main
```

The sample datasets are diagnostic and benchmarking support infrastructure.

They are not the primary historical-data format.

---

## 7. Finite Sample Datasets

The current generator creates four controlled finite datasets.

### `stock_100.bin`

```text
Target: 100 records
Dataset kind: Stock100
```

Contains the first 100 selected records for a stock chosen by the generator that has at least 100 messages.

### `stock_1000.bin`

```text
Target: 1,000 records
Dataset kind: Stock1000
```

Contains the first 1,000 selected records for a stock chosen by the generator that has at least 1,000 messages.

### `stock_10000.bin`

```text
Target: 10,000 records
Dataset kind: Stock10000
```

Contains the first 10,000 selected records for a stock chosen by the generator that has at least 10,000 messages.

### `stock_100000.bin`

```text
Target: 100,000 records
Dataset kind: Stock100000
```

Contains the first 100,000 selected records for a stock chosen by the generator that has at least 100,000 messages.

The selected stock IDs are discovered from the actual parsed ITCH messages.

---

## 8. Stock 123 Dataset

Current special dataset:

```text
stock_123_all.bin
```

Dataset kind:

```text
Stock123All
```

Selected stock:

```text
Stock Locate = 123
```

This dataset has:

```text
Target Count = 0
```

In the custom sample format, a target count of zero means that the dataset is intended to contain all matching records rather than a fixed maximum.

The actual number of records depends on the number of parsed ITCH messages whose decoded `stock_locate` equals 123.

The generator determines this from the decoded `ItchMessage`, not by blindly interpreting arbitrary payload bytes as a stock locate.

---

## 9. Important Stock Locate Rule

Stock Locate must be obtained from the parsed message representation.

The correct conceptual sequence is:

```text
raw payload
    ↓
ITCH parser
    ↓
typed message
    ↓
message.stock_locate
```

It must not be assumed that:

```text
payload[1]
payload[2]
```

represent Stock Locate for every possible ITCH message type.

Different ITCH message layouts do not share a universal field position for Stock Locate.

This distinction is important for correct dataset selection.

---

## 10. Custom `.bin` File Format

The sample format is a project-specific diagnostic serialization format.

It is not Nasdaq's native ITCH format.

The format uses explicit serialization so that it is deterministic and independent of C++ struct layout.

All multi-byte integers are serialized in big-endian order.

---

## 11. `.bin` Header

Each sample file starts with a 28-byte header:

```text
uint32 magic
uint32 version
uint8  dataset_kind
uint8  reserved
uint8  reserved
uint8  reserved
uint64 stock_locate
uint64 target_count
```

Total:

```text
28 bytes
```

Current values:

```text
Magic:   0x484C4F42
Version: 1
```

---

## 12. Dataset Kind Encoding

Serialized dataset kinds:

```text
Stock100       = 1
Stock1000      = 2
Stock10000     = 3
Stock100000    = 4
Stock123All    = 5
```

The serialized representation is:

```cpp
std::uint8_t
```

---

## 13. Record Layout

Each record contains:

```text
record_number
message_type
message_class_name
raw_payload
message_fields
operation
mapping_result
```

The record fields are serialized in exactly that order.

This ordering is an invariant shared by the sample generator and sample reader.

---

## 14. Serialized Message Types

Current serialized message classifications:

```text
AddOrder                 = 1
AddOrderMPID             = 2
OrderExecuted            = 3
OrderExecutedWithPrice   = 4
OrderCancel              = 5
OrderDelete              = 6
OrderReplace             = 7
StockDirectory           = 8
SystemEvent              = 9
Other                    = 255
```

The serialized representation is:

```cpp
std::uint8_t
```

`Other` means the diagnostic serializer does not currently assign a specialized field schema to that message variant.

The raw ITCH payload is still preserved.

---

## 15. Serialized Operation Types

Current operation encoding:

```text
Add       = 1
Reduce    = 2
Remove    = 3
Replace   = 4
Ignored   = 5
Error     = 255
```

The serialized representation is:

```cpp
std::uint8_t
```

These values are part of the `.bin` format contract.

Do not change them independently in the reader or generator.

---

## 16. Mapping Semantics

The existing `ItchMapper` returns:

```text
ReplayOperation
```

or:

```text
ReplayError
```

The diagnostic serializer preserves an important distinction.

### Valid but unsupported message

```text
ReplayError::IgnoredMessage
        ↓
SerializedOperationType::Ignored
        ↓
5
        ↓
"IgnoredMessage"
```

### Actual replay error

Examples:

```text
ReplayError::UnknownOrder
ReplayError::InvalidLifecycle
ReplayError::InvalidQuantity
```

are serialized as:

```text
SerializedOperationType::Error
        ↓
255
        ↓
"ReplayError"
```

Therefore:

```text
IgnoredMessage != actual replay error
```

This distinction is required for correct diagnostics.

---

## 17. Message Field Serialization

Known message variants use specialized field serialization.

### AddOrder

```text
stock_locate
tracking_number
timestamp
order_reference_number
side
shares
stock_symbol
price
```

Total:

```text
8 fields
```

### AddOrderMPID

```text
stock_locate
tracking_number
timestamp
order_reference_number
side
shares
stock_symbol
price
mpid
```

Total:

```text
9 fields
```

### OrderExecuted

```text
stock_locate
tracking_number
timestamp
order_reference_number
executed_shares
match_number
```

Total:

```text
6 fields
```

### OrderExecutedWithPrice

```text
stock_locate
tracking_number
timestamp
order_reference_number
executed_shares
match_number
printable
execution_price
```

Total:

```text
8 fields
```

### OrderCancel

```text
stock_locate
tracking_number
timestamp
order_reference_number
cancelled_shares
```

Total:

```text
5 fields
```

### OrderDelete

```text
stock_locate
tracking_number
timestamp
order_reference_number
```

Total:

```text
4 fields
```

### OrderReplace

```text
stock_locate
tracking_number
timestamp
old_order_reference_number
new_order_reference_number
shares
price
```

Total:

```text
7 fields
```

### Unsupported / Other

Exactly one generic key/value pair:

```text
message_fields
not serialized individually
```

The raw payload remains available for deeper inspection.

---

## 18. Raw Payload Preservation

Every generated sample record retains the original ITCH payload bytes.

Conceptually:

```text
raw_payload
    ↓
length-prefixed byte sequence
```

This means the custom diagnostic representation can be used to inspect the original message bytes without having to reopen the full historical ITCH file.

---

## 19. Why the Sample Format Exists

The custom sample datasets support:

```text
debugging
regression testing
reproducible experiments
manual inspection
controlled benchmarks
```

They provide much smaller inputs than the full historical file.

Example progression:

```text
100
1,000
10,000
100,000
all selected records
```

This makes it possible to validate correctness before running large historical workloads.

---

## 20. Sample Generator

Application:

```text
cpp/app/create_itch_samples_main.cpp
```

Role:

```text
real ITCH
    ↓
parse
    ↓
identify stock
    ↓
map
    ↓
serialize selected records
```

The generator currently performs two passes.

### First pass

The generator:

```text
reads the real ITCH file
parses messages
extracts decoded Stock Locate
counts messages by stock
selects stocks satisfying the target sizes
```

### Second pass

The generator:

```text
reads the real ITCH file again
parses selected messages
maps them through ItchMapper
serializes the resulting diagnostic records
```

The two-pass design allows sample stock selection based on the actual available message counts.

---

## 21. Sample Reader

Files:

```text
cpp/include/lob/itch_sample_reader.hpp
cpp/src/itch_sample_reader.cpp
```

Role:

```text
custom .bin
    ↓
binary deserialization
    ↓
SampleHeader / SampleRecord
```

The reader validates:

```text
magic
version
dataset kind
message type
operation type
field structure
```

It also reconstructs human-readable field strings for the inspector.

The reader must remain exactly aligned with the generator's serialization schema.

---

## 22. Sample Inspector

Application:

```text
cpp/app/inspect_itch_sample_main.cpp
```

Role:

```text
.bin file
    ↓
ItchSampleReader
    ↓
human-readable output
```

Usage:

```text
inspect_itch_sample_main.exe <sample_file> N
inspect_itch_sample_main.exe <sample_file> all
```

Behavior:

```text
N
→ inspect up to N records

N larger than the file
→ inspect until EOF

all
→ inspect every record
```

Example:

```text
inspect_itch_sample_main.exe data/itch/2019-10-18/sample/stock_100.bin 100
```

or:

```text
inspect_itch_sample_main.exe data/itch/2019-10-18/sample/stock_123_all.bin all
```

---

## 23. Data Integrity Rules

The following invariants should hold.

### Source integrity

The original raw ITCH archive should not be modified.

### Parser integrity

Sample generation should use the existing ITCH parser rather than duplicating protocol decoding.

### Stock selection integrity

Stock selection should use the decoded message representation.

### Serialization integrity

The writer and reader must use the same field ordering and field types.

### Operation integrity

```text
IgnoredMessage → Ignored = 5
other ReplayError → Error = 255
```

### Payload integrity

The raw payload stored in the sample must remain unchanged.

### Record integrity

A sample file must contain complete records. A record should not be partially written.

---

## 24. Data Artifacts vs Source Code

The data directory contains generated and historical artifacts.

They should not be treated as application source code.

Conceptually:

```text
cpp/
    algorithms and implementation

data/
    historical and derived datasets
```

Large raw data files should generally remain outside normal source-control workflows unless intentionally versioned.

---

## 25. Git / Repository Handling

The raw and generated datasets are development artifacts.

Typical repository handling is:

```text
source code
→ tracked

small reproducible metadata
→ optionally tracked

large raw market-data files
→ generally ignored

large generated sample data
→ generally ignored unless intentionally committed
```

The exact `.gitignore` policy should be kept consistent with the repository's current data-management strategy.

---

## 26. Current Research Role

The historical data layer exists to support:

```text
market-data reconstruction
market microstructure research
execution analysis
inventory / P&L analysis
feature construction
dataset construction
benchmarking
out-of-sample research
Python-based statistical analysis
```

The data is not being used as input to a claimed profitable trading strategy.

The architecture is focused on reproducible market-data processing and quantitative research.

---

## 27. Relationship to the Simulator

The historical-data layer feeds the existing simulator architecture rather than replacing it.

```text
Historical ITCH
        ↓
protocol decoding
        ↓
semantic mapping
        ↓
historical replay operations
        ↓
existing OrderBook
        ↓
metrics / execution / inventory analysis
```

The sample tooling exists alongside this pipeline:

```text
Historical ITCH
        ↓
sample generator
        ↓
diagnostic .bin
        ↓
sample reader
        ↓
sample inspector
```

The two paths should remain conceptually separate.

---

## 28. Development Workflow

When changing the historical-data layer, validate in this order:

```text
raw data readability
        ↓
parser correctness
        ↓
message representation
        ↓
stock selection
        ↓
mapping
        ↓
sample serialization
        ↓
sample deserialization
        ↓
human-readable inspection
        ↓
historical replay
```

A diagnostic sample failure should not automatically trigger a redesign of the core OrderBook.

---

## 29. Current Development Status

The project currently has:

```text
✅ Raw Nasdaq TotalView-ITCH dataset
✅ Decompressed ITCH input
✅ ITCH file reader
✅ ITCH parser
✅ ITCH message representation
✅ ITCH mapper
✅ ITCH replay layer
✅ Custom sample generator
✅ Custom sample reader
✅ Custom sample inspector
```

The sample tooling has been rebuilt to keep the serialized writer and reader contracts aligned.

The current sample-generation logic extracts Stock Locate from parsed `ItchMessage` objects.

The current sample reader successfully decodes at least the tested `AddOrder` and `AddOrderMPID` record schemas.

---

## 30. Future `processed/` Data

Path:

```text
data/itch/2019-10-18/processed/
```

This area can later contain research-ready derived datasets such as:

```text
normalized event streams
reconstructed book snapshots
trade datasets
quote datasets
feature datasets
execution datasets
train / validation / test splits
```

These should be derived from validated replay outputs rather than manually edited.

---

## 31. Principle

The data architecture should remain:

```text
raw historical data
        ↓
validated decoding
        ↓
validated semantic mapping
        ↓
validated replay
        ↓
derived research data
```

The raw data is the source.

The sample datasets are controlled diagnostic views of that source.

The processed datasets are derived research artifacts.
