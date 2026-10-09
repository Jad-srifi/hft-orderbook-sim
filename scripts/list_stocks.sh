
#!/usr/bin/env bash
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
PYTHON="${PYTHON:-python}"

if [[ -f "$ROOT/tests/python/test_itch_research_workflow.py" ]]; then
    ENTRY="$ROOT/tests/python/test_itch_research_workflow.py"
elif [[ -f "$ROOT/examples/itch_research_workflow.py" ]]; then
    ENTRY="$ROOT/examples/itch_research_workflow.py"
else
    printf 'ITCH research workflow not found; cannot call --list-stocks.\n' >&2
    exit 1
fi

cd "$ROOT"
export PYTHONPATH="$ROOT/python${PYTHONPATH:+:$PYTHONPATH}"
exec "$PYTHON" "$ENTRY" --list-stocks "$@"