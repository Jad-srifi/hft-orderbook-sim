
#!/usr/bin/env bash
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
PYTHON="${PYTHON:-python}"
ENTRY="$ROOT/examples/historical_replay.py"

if [[ ! -f "$ENTRY" ]]; then
    printf 'Historical replay example not found: %s\n' "$ENTRY" >&2
    exit 1
fi

cd "$ROOT"
export PYTHONPATH="$ROOT/python${PYTHONPATH:+:$PYTHONPATH}"
exec "$PYTHON" "$ENTRY" "$@"