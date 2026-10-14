from pathlib import Path

import pytest

from examples.itch_research_workflow import resolve_itch_file


def test_gzip_itch_input_is_rejected_with_decompression_guidance(tmp_path: Path):
    compressed_file = tmp_path / "feed.txt.gz"
    compressed_file.write_bytes(b"not a real feed")

    with pytest.raises(ValueError, match="Decompress the file first"):
        resolve_itch_file(compressed_file)
