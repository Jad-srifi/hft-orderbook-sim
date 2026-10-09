class Backtest:
    def __init__(self, data, procedure) -> None:
        if not callable(procedure):
            raise TypeError("procedure must be callable")

        self.data = data
        self.procedure = procedure
        self.results = []

    def run(self):
        self.results = []

        for observation in self.data:
            result = self.procedure(observation)
            self.results.append(result)

        return self.results

    def reset(self) -> None:
        self.results = []

    def __len__(self) -> int:
        return len(self.results)