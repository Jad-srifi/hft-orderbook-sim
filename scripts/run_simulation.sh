
#!/usr/bin/env bash
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
PYTHON="${PYTHON:-python}"

cd "$ROOT"
export PYTHONPATH="$ROOT/python${PYTHONPATH:+:$PYTHONPATH}"

ENTRY="$ROOT/examples/basic_simulation.py"
if [[ -f "$ENTRY" ]]; then
    exec "$PYTHON" "$ENTRY" "$@"
fi

EXECUTABLE="$ROOT/build/simulate.exe"
if [[ -x "$EXECUTABLE" ]]; then
    exec "$EXECUTABLE" "$@"
fi

printf 'Neither examples/basic_simulation.py nor build/simulate.exe exists. Run scripts/build.sh first.\n' >&2
exit 1