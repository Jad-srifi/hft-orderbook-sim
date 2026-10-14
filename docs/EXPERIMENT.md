# Experiment Infrastructure

## Purpose

`experiment.py` provides a small reproducibility layer for quantitative research experiments.

An experiment records:

- the experiment name
- a description
- parameters
- metadata
- results

It does not perform the research itself. It stores the configuration and outputs of a research run so that the work can be inspected, saved, and reproduced later.

## Architecture

```text
Research procedure
       ↓
    Experiment
       ↓
┌──────┼─────────┐
│      │         │
Name Parameters Metadata
          │
          ↓
       Results
          │
          ↓
       JSON file
```

The experiment layer sits above the research modules.

```text
Research data
      ↓
Features / Analysis
      ↓
Experiment
      ↓
Saved results
```

## `Experiment`

The main class is:

```python
Experiment(name, description="", parameters=None, metadata=None)
```

It stores:

```text
name
description
parameters
metadata
results
```

### Name

The experiment name identifies the research experiment.

It must be a non-empty string.

Example:

```python
Experiment("imbalance_prediction")
```

### Description

The description explains what the experiment investigates.

Example:

```python
Experiment(
    "imbalance_prediction",
    "Study whether order-book imbalance predicts short-term midpoint returns."
)
```

### Parameters

Parameters contain choices that define the experiment.

Example:

```python
{
    "levels": 5,
    "horizon": 100,
    "threshold": 0.5
}
```

Parameters should describe the experiment configuration rather than contain the dataset itself.

### Metadata

Metadata describes contextual information about the experiment.

Example:

```python
{
    "dataset": "2019-10-18",
    "stock_locate": 123
}
```

Metadata can identify the historical dataset, instrument, date, or other contextual information needed to understand the run.

### Results

Results contain outputs produced by the research procedure.

Example:

```python
{
    "mean_return": 0.0004,
    "correlation": 0.21
}
```

Results are stored separately from parameters and metadata because they represent the outcome rather than the configuration.

## Updating an Experiment

Parameters can be added or changed with:

```python
experiment.set_parameter("levels", 5)
```

Metadata can be added or changed with:

```python
experiment.set_metadata("dataset", "2019-10-18")
```

Research results can be stored with:

```python
experiment.set_result("correlation", 0.21)
```

A stored result can be retrieved with:

```python
experiment.get_result("correlation")
```

## Serialization

Experiments can be converted into a Python dictionary:

```python
experiment.to_dict()
```

The resulting structure is:

```text
{
    "name": ...,
    "description": ...,
    "parameters": ...,
    "metadata": ...,
    "results": ...
}
```

Experiments can also be saved as JSON:

```python
experiment.save("results/experiment.json")
```

The directory is created automatically when necessary.

An experiment can later be loaded:

```python
experiment = Experiment.load("results/experiment.json")
```

The saved experiment therefore provides a persistent record of the research configuration and results.

## Design Principles

### Separation of concerns

`Experiment` does not calculate research statistics, generate features, replay ITCH data, or run a trading strategy.

Those responsibilities belong to other modules.

```text
historical_replay.py
    → historical data

microstructure.py
features.py
    → research variables

statistics.py
    → statistical calculations

backtest.py
    → evaluation procedure

experiment.py
    → configuration and result record
```

### Reproducibility

An experiment should contain enough information to understand what was run.

Important information should therefore be placed in parameters or metadata rather than left implicit.

### No raw datasets inside experiment objects

An experiment should record information about a dataset rather than embedding an entire dataset inside the JSON file.

Large datasets and market data remain separate research artifacts.

### Simple serialization

The current implementation uses JSON because it is human-readable, portable, and easy to inspect.

## Example

The example imports `experiment` from `python/experiment.py`. From the repository root, make the `python/` directory importable before running it. In MSYS2 Bash:

```bash
export PYTHONPATH="$PWD/python${PYTHONPATH:+:$PYTHONPATH}"
```

In PowerShell:

```powershell
$env:PYTHONPATH = "$PWD\python"
```

Then run:

```python
from experiment import Experiment

experiment = Experiment(
    "imbalance_prediction",
    "Test whether order-book imbalance predicts short-term midpoint returns.",
    {"levels": 5, "horizon": 100},
    {"dataset": "2019-10-18", "stock_locate": 123},
)

experiment.set_result("correlation", 0.21)
experiment.set_result("mean_return", 0.0004)

experiment.save("results/imbalance_prediction.json")
```

The resulting file preserves the configuration and results explicitly recorded in the object. It does **not** automatically capture the source commit, environment/dependency versions, dataset checksum, random seeds, or research procedure. Record any such details needed for reproduction in `metadata`; saving an experiment object alone does not guarantee full reproducibility.

## Scope

The current experiment layer intentionally remains small.

It does not provide:

- experiment scheduling
- experiment databases
- automatic version-control integration
- distributed execution
- automatic environment detection
- model management

Those features can be introduced later only if actual research requirements justify them.
