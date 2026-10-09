from dataclasses import dataclass

from observations import BookSnapshot


@dataclass
class ReplayPlayer:
    snapshots: tuple[BookSnapshot, ...]
    interval_ms: int = 100
    index: int = 0

    def __post_init__(self) -> None:
        if self.interval_ms < 1:
            raise ValueError("interval_ms must be at least 1")

        if not isinstance(self.snapshots, tuple):
            self.snapshots = tuple(self.snapshots)

        if not all(isinstance(snapshot, BookSnapshot) for snapshot in self.snapshots):
            raise TypeError("snapshots must contain only BookSnapshot objects")

        if self.index < 0 or self.index >= len(self.snapshots):
            if self.snapshots:
                raise ValueError("index is outside the snapshot range")
            self.index = 0

    @property
    def current(self) -> BookSnapshot | None:
        if not self.snapshots:
            return None
        return self.snapshots[self.index]

    @property
    def total(self) -> int:
        return len(self.snapshots)

    @property
    def finished(self) -> bool:
        return not self.snapshots or self.index >= len(self.snapshots) - 1

    def step_forward(self) -> BookSnapshot | None:
        if not self.snapshots:
            return None

        if self.index < len(self.snapshots) - 1:
            self.index += 1

        return self.current

    def step_backward(self) -> BookSnapshot | None:
        if not self.snapshots:
            return None

        if self.index > 0:
            self.index -= 1

        return self.current

    def reset(self) -> BookSnapshot | None:
        self.index = 0
        return self.current

    def seek(self, index: int) -> BookSnapshot | None:
        if index < 0 or index >= len(self.snapshots):
            raise IndexError("snapshot index is outside the replay range")

        self.index = index
        return self.current

    def set_interval(self, interval_ms: int) -> None:
        if interval_ms < 1:
            raise ValueError("interval_ms must be at least 1")

        self.interval_ms = interval_ms


def create_replay(snapshots, interval_ms: int = 100) -> ReplayPlayer:
    return ReplayPlayer(tuple(snapshots), interval_ms)


def replay_frame(snapshot: BookSnapshot | None) -> dict:
    if snapshot is None:
        return {
            "timestamp_ms": 0,
            "message_index": 0,
            "best_bid": 0,
            "best_ask": 0,
            "spread": 0,
            "midpoint": 0.0,
            "bid_levels": [],
            "ask_levels": [],
        }

    return {
        "timestamp_ms": snapshot.timestamp_ms,
        "message_index": snapshot.message_index,
        "best_bid": snapshot.best_bid,
        "best_ask": snapshot.best_ask,
        "spread": snapshot.spread,
        "midpoint": snapshot.midpoint,
        "bid_levels": list(snapshot.bid_levels),
        "ask_levels": list(snapshot.ask_levels),
    }


def replay_state(player: ReplayPlayer) -> dict:
    return {
        "index": player.index,
        "total": player.total,
        "finished": player.finished,
        "interval_ms": player.interval_ms,
        "snapshot": replay_frame(player.current),
    }