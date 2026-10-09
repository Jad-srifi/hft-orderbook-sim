import sys
from pathlib import Path

import pytest

PROJECT_ROOT = Path(__file__).resolve().parents[2]
PYTHON_DIR = PROJECT_ROOT / "python"
sys.path.insert(0, str(PYTHON_DIR))

from backtests.backtest import Backtest


def test_backtest_creation():
    data = [1, 2, 3]
    procedure = lambda x: x * 2

    backtest = Backtest(data, procedure)

    assert backtest.data == data
    assert backtest.procedure is procedure
    assert backtest.results == []


def test_backtest_run():
    data = [1, 2, 3, 4]
    procedure = lambda x: x * 2

    backtest = Backtest(data, procedure)

    results = backtest.run()

    assert results == [2, 4, 6, 8]
    assert backtest.results == [2, 4, 6, 8]


def test_backtest_run_calls_procedure_for_each_observation():
    data = [1, 2, 3]
    observed = []

    def procedure(x):
        observed.append(x)
        return x + 1

    backtest = Backtest(data, procedure)

    backtest.run()

    assert observed == [1, 2, 3]


def test_backtest_empty_data():
    backtest = Backtest([], lambda x: x)

    assert backtest.run() == []
    assert len(backtest) == 0


def test_backtest_reset():
    backtest = Backtest([1, 2, 3], lambda x: x * 2)

    backtest.run()
    backtest.reset()

    assert backtest.results == []
    assert len(backtest) == 0


def test_backtest_length():
    backtest = Backtest([1, 2, 3], lambda x: x * 2)

    backtest.run()

    assert len(backtest) == 3


def test_invalid_procedure():
    with pytest.raises(TypeError):
        Backtest([1, 2, 3], None)


def test_backtest_rerun_replaces_previous_results():
    backtest = Backtest([1, 2, 3], lambda x: x * 2)

    first_results = backtest.run()

    backtest.data = [4, 5]
    second_results = backtest.run()

    assert first_results == [2, 4, 6]
    assert second_results == [8, 10]
    assert backtest.results == [8, 10]


def test_backtest_procedure_exception_propagates():
    def procedure(x):
        if x == 2:
            raise ValueError("test error")
        return x

    backtest = Backtest([1, 2, 3], procedure)

    with pytest.raises(ValueError, match="test error"):
        backtest.run()