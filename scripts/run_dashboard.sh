#!/usr/bin/env bash
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
PYTHON="${PYTHON:-python}"
PORT="${PORT:-8000}"
HOST="${DASHBOARD_HOST:-127.0.0.1}"

if [[ ! -f "$ROOT/web/index.html" || ! -f "$ROOT/webapp/app.py" ]]; then
    printf 'Error: dashboard source files are missing under %s\n' "$ROOT" >&2
    exit 1
fi

if ! "$PYTHON" -c 'import flask' >/dev/null 2>&1; then
    printf 'Flask is missing. Install dashboard requirements first:\n' >&2
    printf '  %q -m pip install -r "%s/webapp/requirements.txt"\n' "$PYTHON" "$ROOT" >&2
    exit 1
fi

printf '\nHybrid LOB Simulator — local research dashboard\n'
printf 'Open: http://%s:%s/research\n' "$HOST" "$PORT"
printf 'The overview and roadmap remain at http://%s:%s/\n' "$HOST" "$PORT"
printf 'Uploads and results are stored locally under results/dashboard/.\n'
printf 'Stop the server with Ctrl+C.\n\n'
cd "$ROOT"
export PORT DASHBOARD_HOST="$HOST"
exec "$PYTHON" -m webapp.app
