import sys
from pathlib import Path

import pytest

PROJECT_ROOT = Path(__file__).resolve().parents[2]
PYTHON_DIR = PROJECT_ROOT / "python"
sys.path.insert(0, str(PYTHON_DIR))

from experiment import Experiment


def test_experiment_creation():
    experiment = Experiment("test", "Test experiment")

    assert experiment.name == "test"
    assert experiment.description == "Test experiment"
    assert experiment.parameters == {}
    assert experiment.metadata == {}
    assert experiment.results == {}


def test_experiment_initial_data():
    experiment = Experiment("test", "Test experiment", {"levels": 5}, {"dataset": "test"})

    assert experiment.parameters == {"levels": 5}
    assert experiment.metadata == {"dataset": "test"}


def test_set_parameter():
    experiment = Experiment("test")

    experiment.set_parameter("levels", 5)
    experiment.set_parameter("interval", 1000)

    assert experiment.parameters == {"levels": 5, "interval": 1000}


def test_set_metadata():
    experiment = Experiment("test")

    experiment.set_metadata("dataset", "2019-10-18")
    experiment.set_metadata("stock_locate", 123)

    assert experiment.metadata == {"dataset": "2019-10-18", "stock_locate": 123}


def test_set_result():
    experiment = Experiment("test")

    experiment.set_result("mean_spread", 12.5)
    experiment.set_result("mean_imbalance", 0.15)

    assert experiment.results == {"mean_spread": 12.5, "mean_imbalance": 0.15}


def test_get_result():
    experiment = Experiment("test")
    experiment.set_result("mean_spread", 12.5)

    assert experiment.get_result("mean_spread") == 12.5


def test_to_dict():
    experiment = Experiment("test", "Test experiment", {"levels": 5}, {"dataset": "test"})
    experiment.set_result("mean_spread", 12.5)

    assert experiment.to_dict() == {
        "name": "test",
        "description": "Test experiment",
        "parameters": {"levels": 5},
        "metadata": {"dataset": "test"},
        "results": {"mean_spread": 12.5},
    }


def test_save_and_load(tmp_path: Path):
    experiment = Experiment("test", "Test experiment", {"levels": 5}, {"dataset": "test"})
    experiment.set_result("mean_spread", 12.5)

    file_path = tmp_path / "experiment.json"
    experiment.save(file_path)

    loaded = Experiment.load(file_path)

    assert loaded.to_dict() == experiment.to_dict()


def test_empty_name_rejected():
    with pytest.raises(ValueError):
        Experiment("")


def test_invalid_description_rejected():
    with pytest.raises(TypeError):
        Experiment("test", 123)


def test_invalid_parameters_rejected():
    with pytest.raises(TypeError):
        Experiment("test", parameters=[])


def test_invalid_metadata_rejected():
    with pytest.raises(TypeError):
        Experiment("test", metadata=[])


def test_invalid_parameter_key_rejected():
    experiment = Experiment("test")

    with pytest.raises(ValueError):
        experiment.set_parameter("", 5)


def test_invalid_metadata_key_rejected():
    experiment = Experiment("test")

    with pytest.raises(ValueError):
        experiment.set_metadata("", "value")


def test_invalid_result_key_rejected():
    experiment = Experiment("test")

    with pytest.raises(ValueError):
        experiment.set_result("", 5)


def test_missing_result_rejected():
    experiment = Experiment("test")

    with pytest.raises(KeyError):
        experiment.get_result("missing")


def test_invalid_experiment_file(tmp_path: Path):
    file_path = tmp_path / "invalid.json"
    file_path.write_text('{"name": "test"}', encoding="utf-8")

    with pytest.raises(ValueError):
        Experiment.load(file_path)