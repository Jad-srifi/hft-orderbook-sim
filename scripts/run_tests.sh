
#!/usr/bin/env bash
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
PYTHON="${PYTHON:-python}"

if [[ "${MSYSTEM:-}" != "UCRT64" ]]; then
    printf 'Error: run this script from the MSYS2 UCRT64 terminal. MSYSTEM=%s\n' "${MSYSTEM:-unset}" >&2
    exit 1
fi

cd "$ROOT"
export PYTHONPATH="$ROOT/python${PYTHONPATH:+:$PYTHONPATH}"

"$ROOT/scripts/build.sh"

printf '\nRunning Python tests...\n'
"$PYTHON" -m pytest -q tests/python

printf '\nRunning C++ tests...\n'
shopt -s nullglob
TEST_BINARIES=("$ROOT"/build/test_*.exe)
if [[ ${#TEST_BINARIES[@]} -eq 0 ]]; then
    printf 'No C++ test executables were produced.\n' >&2
    exit 1
fi

for executable in "${TEST_BINARIES[@]}"; do
    printf '\n%s\n' "${executable#"$ROOT"/}"
    "$executable"
done

printf '\nAll Python and C++ test commands completed successfully.\n'