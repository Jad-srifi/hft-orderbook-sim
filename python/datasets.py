from collections.abc import Iterable, Iterator
from typing import overload

from observations import BookSnapshot


class BookDataset:
    def __init__(self, snapshots: Iterable[BookSnapshot] = ()) -> None:
        self._snapshots: list[BookSnapshot] = []

        for snapshot in snapshots:
            self.add(snapshot)

    def add(self, snapshot: BookSnapshot) -> None:
        if not isinstance(snapshot, BookSnapshot):
            raise TypeError("snapshot must be a BookSnapshot")

        self._snapshots.append(snapshot)

    @classmethod
    def from_snapshots(cls, snapshots: Iterable[BookSnapshot]) -> "BookDataset":
        return cls(snapshots)

    @property
    def snapshots(self) -> tuple[BookSnapshot, ...]:
        return tuple(self._snapshots)

    def __len__(self) -> int:
        return len(self._snapshots)

    @overload
    def __getitem__(self, index: int) -> BookSnapshot:
        ...

    @overload
    def __getitem__(
        self,
        index: slice,
    ) -> tuple[BookSnapshot, ...]:
        ...

    def __getitem__(
        self,
        index: int | slice,
    ) -> BookSnapshot | tuple[BookSnapshot, ...]:
        if isinstance(index, slice):
            return tuple(self._snapshots[index])

        return self._snapshots[index]

    def __iter__(self) -> Iterator[BookSnapshot]:
        return iter(self._snapshots)