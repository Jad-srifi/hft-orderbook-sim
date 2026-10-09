
#!/usr/bin/env bash
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
BUILD_DIR="$ROOT/build"
OBJ_DIR="$BUILD_DIR/obj"
APP_OBJ_DIR="$BUILD_DIR/app_obj"
TEST_OBJ_DIR="$BUILD_DIR/test_obj"
PYTHON_DIR="$ROOT/python"
CXX="${CXX:-g++}"
PYTHON="${PYTHON:-python}"
CXXFLAGS=(-std=c++17 -Wall -Wextra -pedantic -O2)

if [[ "${MSYSTEM:-}" != "UCRT64" ]]; then
    printf 'Error: run this script from the MSYS2 UCRT64 terminal. MSYSTEM=%s\n' "${MSYSTEM:-unset}" >&2
    exit 1
fi

command -v "$CXX" >/dev/null 2>&1 || { printf 'Compiler not found: %s\n' "$CXX" >&2; exit 1; }
command -v "$PYTHON" >/dev/null 2>&1 || { printf 'Python not found: %s\n' "$PYTHON" >&2; exit 1; }

cd "$ROOT"
mkdir -p "$BUILD_DIR" "$OBJ_DIR" "$APP_OBJ_DIR" "$TEST_OBJ_DIR" "$PYTHON_DIR"
rm -f "$OBJ_DIR"/*.o "$APP_OBJ_DIR"/*.o "$TEST_OBJ_DIR"/*.o

printf '\n[1/4] Compiling C++ source files...\n'
for source in "$ROOT"/cpp/src/*.cpp; do
    [[ -f "$source" ]] || continue
    name="$(basename "${source%.cpp}")"
    printf '  %s\n' "${source#"$ROOT"/}"
    "$CXX" "${CXXFLAGS[@]}" -I"$ROOT/cpp/include/lob" -c "$source" -o "$OBJ_DIR/$name.o"
done

shopt -s nullglob
SRC_OBJECTS=("$OBJ_DIR"/*.o)
if [[ ${#SRC_OBJECTS[@]} -eq 0 ]]; then
    printf 'No C++ source files were compiled.\n' >&2
    exit 1
fi

printf '\n[2/4] Building C++ applications...\n'
APP_SPECS=(
    'simulate_main.cpp:simulate.exe'
    'replay_itch_main.cpp:replay_itch_main.exe'
    'create_itch_samples_main.cpp:create_itch_samples_main.exe'
    'inspect_itch_sample_main.cpp:inspect_itch_sample_main.exe'
    'benchmark_main.cpp:benchmark_main.exe'
    'profile_main.cpp:profile_main.exe'
)

for spec in "${APP_SPECS[@]}"; do
    source_name="${spec%%:*}"
    output_name="${spec##*:}"
    source="$ROOT/cpp/app/$source_name"
    [[ -f "$source" ]] || { printf 'Required app source not found: %s\n' "$source" >&2; exit 1; }
    object="$APP_OBJ_DIR/${source_name%.cpp}.o"
    printf '  %s -> build/%s\n' "$source_name" "$output_name"
    "$CXX" "${CXXFLAGS[@]}" -I"$ROOT/cpp/include/lob" -c "$source" -o "$object"
    "$CXX" "${CXXFLAGS[@]}" "$object" "${SRC_OBJECTS[@]}" -o "$BUILD_DIR/$output_name"
done

printf '\n[3/4] Building C++ test executables...\n'
found_tests=0
for source in "$ROOT"/tests/cpp/test_*.cpp; do
    [[ -f "$source" ]] || continue
    found_tests=1
    name="$(basename "${source%.cpp}")"
    object="$TEST_OBJ_DIR/$name.o"
    printf '  %s -> build/%s.exe\n' "${source#"$ROOT"/}" "$name"
    "$CXX" "${CXXFLAGS[@]}" -I"$ROOT/cpp/include/lob" -c "$source" -o "$object"
    "$CXX" "${CXXFLAGS[@]}" "$object" "${SRC_OBJECTS[@]}" -o "$BUILD_DIR/$name.exe"
done
if [[ "$found_tests" -eq 0 ]]; then
    printf 'No C++ test sources found under tests/cpp.\n'
fi

printf '\n[4/4] Building the Python extension...\n'
BINDING_SOURCE="$ROOT/cpp/bindings/lob_bindings.cpp"
[[ -f "$BINDING_SOURCE" ]] || { printf 'Binding source not found: %s\n' "$BINDING_SOURCE" >&2; exit 1; }

PYTHON_VERSION="$("$PYTHON" -c 'import sys; print(f"{sys.version_info.major}.{sys.version_info.minor}")')"
EXT_SUFFIX="$("$PYTHON" -c 'import sysconfig; print(sysconfig.get_config_var("EXT_SUFFIX") or ".pyd")')"
PYBIND11_INCLUDES_TEXT="$("$PYTHON" -m pybind11 --includes)"
read -r -a PYBIND11_INCLUDES <<< "$PYBIND11_INCLUDES_TEXT"
BINDING_OBJECT="$APP_OBJ_DIR/lob_bindings.o"
EXTENSION_PATH="$PYTHON_DIR/lob_cpp$EXT_SUFFIX"

"$CXX" "${CXXFLAGS[@]}" "${PYBIND11_INCLUDES[@]}" -I"$ROOT/cpp/include/lob" -c "$BINDING_SOURCE" -o "$BINDING_OBJECT"

BINDING_OBJECTS=(
    "$BINDING_OBJECT"
    "$OBJ_DIR/order_book.o"
    "$OBJ_DIR/order_map.o"
    "$OBJ_DIR/execution.o"
    "$OBJ_DIR/inventory_model.o"
    "$OBJ_DIR/simulator.o"
    "$OBJ_DIR/metrics.o"
    "$OBJ_DIR/itch_parser.o"
    "$OBJ_DIR/itch_mapper.o"
    "$OBJ_DIR/itch_replay.o"
    "$OBJ_DIR/itch_file_reader.o"
    "$OBJ_DIR/profile.o"
)

for object in "${BINDING_OBJECTS[@]}"; do
    [[ -f "$object" ]] || { printf 'Required binding dependency not found: %s\n' "$object" >&2; exit 1; }
done

"$CXX" -shared "${BINDING_OBJECTS[@]}" -L/ucrt64/lib "-lpython$PYTHON_VERSION" -o "$EXTENSION_PATH"

export PYTHONPATH="$PYTHON_DIR${PYTHONPATH:+:$PYTHONPATH}"
"$PYTHON" -c 'import lob_cpp; print("lob_cpp import OK:", lob_cpp.__file__)'

printf '\nBuild complete.\n'