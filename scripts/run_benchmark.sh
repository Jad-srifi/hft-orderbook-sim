
#!/usr/bin/env bash
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
EXECUTABLE="$ROOT/build/benchmark_main.exe"

if [[ "${MSYSTEM:-}" != "UCRT64" ]]; then
    printf 'Error: run this script from the MSYS2 UCRT64 terminal. MSYSTEM=%s\n' "${MSYSTEM:-unset}" >&2
    exit 1
fi

if [[ ! -x "$EXECUTABLE" ]]; then
    "$ROOT/scripts/build.sh"
fi

if [[ ! -x "$EXECUTABLE" ]]; then
    printf 'Benchmark executable not found after build: %s\n' "$EXECUTABLE" >&2
    exit 1
fi

cd "$ROOT"
exec "$EXECUTABLE" "$@"