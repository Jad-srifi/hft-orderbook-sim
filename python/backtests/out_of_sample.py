class OutOfSampleSplit:
    def __init__(self, train_data, test_data) -> None:
        self.train_data = train_data
        self.test_data = test_data


def out_of_sample_split(data, train_size: int, test_size: int) -> OutOfSampleSplit:
    if train_size < 1:
        raise ValueError("train_size must be at least 1")
    if test_size < 1:
        raise ValueError("test_size must be at least 1")
    if train_size + test_size > len(data):
        raise ValueError("train_size + test_size exceeds data length")

    train_data = data[:train_size]
    test_data = data[train_size:train_size + test_size]

    return OutOfSampleSplit(train_data, test_data)


def rolling_walk_forward(data, train_size: int, test_size: int, step: int | None = None):
    if train_size < 1:
        raise ValueError("train_size must be at least 1")
    if test_size < 1:
        raise ValueError("test_size must be at least 1")

    if step is None:
        step = test_size

    if step < 1:
        raise ValueError("step must be at least 1")

    start = 0

    while start + train_size + test_size <= len(data):
        train_data = data[start:start + train_size]
        test_start = start + train_size
        test_data = data[test_start:test_start + test_size]

        yield OutOfSampleSplit(train_data, test_data)

        start += step


def expanding_walk_forward(data, train_size: int, test_size: int, step: int | None = None):
    if train_size < 1:
        raise ValueError("train_size must be at least 1")
    if test_size < 1:
        raise ValueError("test_size must be at least 1")

    if step is None:
        step = test_size

    if step < 1:
        raise ValueError("step must be at least 1")

    train_end = train_size

    while train_end + test_size <= len(data):
        train_data = data[:train_end]
        test_data = data[train_end:train_end + test_size]

        yield OutOfSampleSplit(train_data, test_data)

        train_end += step