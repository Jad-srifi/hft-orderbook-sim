
#!/usr/bin/env bash
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
PYTHON="${PYTHON:-python}"
ENTRY="$ROOT/examples/itch_research_workflow.py"

if [[ ! -f "$ENTRY" ]]; then
    printf 'Error: ITCH research example not found:\n%s\n' "$ENTRY" >&2
    printf 'Move your workflow to examples/itch_research_workflow.py first.\n' >&2
    exit 1
fi

cd "$ROOT"
export PYTHONPATH="$ROOT/python${PYTHONPATH:+:$PYTHONPATH}"

exec "$PYTHON" "$ENTRY" "$@"