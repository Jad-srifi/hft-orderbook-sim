#!/usr/bin/env bash
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
WEB_DIR="$ROOT/web"
PYTHON="${PYTHON:-python}"
PORT="${PORT:-8000}"

if [[ ! -f "$WEB_DIR/index.html" ]]; then
    printf 'Error: dashboard page not found: %s/index.html\n' "$WEB_DIR" >&2
    printf 'Ensure the web/ directory is at the repository root.\n' >&2
    exit 1
fi

printf '\nHybrid LOB Simulator dashboard\n'
printf 'Open: http://127.0.0.1:%s\n' "$PORT"
printf 'Stop the server with Ctrl+C.\n\n'

cd "$ROOT"
exec "$PYTHON" -m http.server "$PORT" --bind 127.0.0.1 --directory "$WEB_DIR"
