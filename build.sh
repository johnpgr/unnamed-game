#!/usr/bin/env bash

set -euo pipefail

ROOT_DIR="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)"
FLAGS_FILE="$ROOT_DIR/compile_flags.txt"
SOURCE_FILE="$ROOT_DIR/src/main.cpp"
BUILD_DIR="$ROOT_DIR/build"
MODE="${BUILD_MODE:-debug}"

if [[ $# -gt 0 ]]; then
  case "$1" in
    debug|release)
      MODE="$1"
      shift
      ;;
  esac
fi

OUTPUT_FILE="${1:-$BUILD_DIR/main}"

COMPILER="${CXX:-clang++}"

if [[ ! -f "$FLAGS_FILE" ]]; then
  echo "Missing compile flags file: $FLAGS_FILE" >&2
  exit 1
fi

if [[ ! -f "$SOURCE_FILE" ]]; then
  echo "Missing source file: $SOURCE_FILE" >&2
  exit 1
fi

FLAG_LINES="$(sed -E 's/[[:space:]]*#.*$//; /^[[:space:]]*$/d' "$FLAGS_FILE")"

if [[ "$MODE" == "release" ]]; then
  FLAG_LINES="$(printf '%s\n' "$FLAG_LINES" | sed -E '/^-fsanitize(=|$)/d')"
  MODE_FLAGS="-O3 -DNDEBUG"
else
  MODE_FLAGS="-O0 -g3 -DDEBUG"
fi

FLAGS="$(printf '%s\n' "$FLAG_LINES" | tr '\n' ' ')"

mkdir -p "$BUILD_DIR"

# Intentionally uses normal shell expansion of env flags from your shell profile.
# shellcheck disable=SC2086
$COMPILER ${CPPFLAGS:-} ${CFLAGS:-} ${CXXFLAGS:-} $FLAGS $MODE_FLAGS "$SOURCE_FILE" ${LDFLAGS:-} -o "$OUTPUT_FILE"

echo "Built ($MODE): $OUTPUT_FILE"
