import json
from pathlib import Path


class Experiment:
    def __init__(self, name: str, description: str = "", parameters: dict | None = None, metadata: dict | None = None) -> None:
        if not isinstance(name, str) or not name:
            raise ValueError("name must be a non-empty string")
        if not isinstance(description, str):
            raise TypeError("description must be a string")
        if parameters is not None and not isinstance(parameters, dict):
            raise TypeError("parameters must be a dictionary")
        if metadata is not None and not isinstance(metadata, dict):
            raise TypeError("metadata must be a dictionary")

        self.name = name
        self.description = description
        self.parameters = dict(parameters) if parameters is not None else {}
        self.metadata = dict(metadata) if metadata is not None else {}
        self.results = {}

    def set_parameter(self, key: str, value) -> None:
        if not isinstance(key, str) or not key:
            raise ValueError("parameter key must be a non-empty string")
        self.parameters[key] = value

    def set_metadata(self, key: str, value) -> None:
        if not isinstance(key, str) or not key:
            raise ValueError("metadata key must be a non-empty string")
        self.metadata[key] = value

    def set_result(self, key: str, value) -> None:
        if not isinstance(key, str) or not key:
            raise ValueError("result key must be a non-empty string")
        self.results[key] = value

    def get_result(self, key: str):
        return self.results[key]

    def to_dict(self) -> dict:
        return {
            "name": self.name,
            "description": self.description,
            "parameters": dict(self.parameters),
            "metadata": dict(self.metadata),
            "results": dict(self.results),
        }

    def save(self, file_path: str | Path) -> None:
        path = Path(file_path)
        path.parent.mkdir(parents=True, exist_ok=True)

        with path.open("w", encoding="utf-8") as file:
            json.dump(self.to_dict(), file, indent=2)

    @classmethod
    def load(cls, file_path: str | Path) -> "Experiment":
        path = Path(file_path)

        with path.open("r", encoding="utf-8") as file:
            data = json.load(file)

        required = {"name", "description", "parameters", "metadata", "results"}
        if not required.issubset(data):
            raise ValueError("invalid experiment file")

        experiment = cls(
            name=data["name"],
            description=data["description"],
            parameters=data["parameters"],
            metadata=data["metadata"],
        )
        experiment.results = dict(data["results"])
        return experiment