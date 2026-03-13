#!/usr/bin/env bash

set -euo pipefail

ROOT_DIR="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)"
FLAGS_FILE="$ROOT_DIR/compile_flags.txt"
MAIN_SOURCE_FILE="$ROOT_DIR/src/main.cpp"
GAME_SOURCE_FILE="$ROOT_DIR/src/game.cpp"
BUILD_DIR="$ROOT_DIR/build"
MODE="${BUILD_MODE:-debug}"
MAIN_LINK_FLAGS=""
MAIN_SOURCE_FLAGS=""
VULKAN_INCLUDE_FLAGS=""
VULKAN_LINK_FLAGS=""

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
UNAME_S="$(uname -s)"

case "$UNAME_S" in
  Linux)
    GAME_OUTPUT_FILE="$BUILD_DIR/libgame.so"
    GAME_LINK_FLAGS="-shared -fPIC"
    MAIN_LINK_FLAGS="-lvulkan -lxcb -ldl"
    ;;
  Darwin)
    GAME_OUTPUT_FILE="$BUILD_DIR/libgame.dylib"
    GAME_LINK_FLAGS="-dynamiclib -fPIC"
    MAIN_SOURCE_FLAGS="-x objective-c++ \"$MAIN_SOURCE_FILE\""
    MAIN_LINK_FLAGS="-framework AppKit -framework Foundation -framework QuartzCore -ldl"
    ;;
  MINGW*|MSYS*|CYGWIN*)
    GAME_OUTPUT_FILE="$BUILD_DIR/game.dll"
    GAME_LINK_FLAGS="-shared"
    MAIN_LINK_FLAGS="-lgdi32"
    ;;
  *)
    GAME_OUTPUT_FILE="$BUILD_DIR/libgame.so"
    GAME_LINK_FLAGS="-shared -fPIC"
    MAIN_LINK_FLAGS="-lvulkan -lxcb -ldl"
    ;;
esac

if [[ "$UNAME_S" == "Darwin" ]]; then
  VULKAN_SDK_ROOT="/usr/local"
  VULKAN_INCLUDE_DIR="$VULKAN_SDK_ROOT/include"
  VULKAN_LIB_DIR="$VULKAN_SDK_ROOT/lib"

  if [[ ! -d "$VULKAN_INCLUDE_DIR/vulkan" ]]; then
    echo "Missing Vulkan headers at hardcoded macOS path: $VULKAN_INCLUDE_DIR" >&2
    exit 1
  fi

  if [[ ! -f "$VULKAN_LIB_DIR/libvulkan.dylib" ]]; then
    echo "Missing Vulkan loader at hardcoded macOS path: $VULKAN_LIB_DIR/libvulkan.dylib" >&2
    exit 1
  fi

  VULKAN_INCLUDE_FLAGS="-I$VULKAN_INCLUDE_DIR"
  VULKAN_LINK_FLAGS="-L$VULKAN_LIB_DIR -lvulkan -Wl,-rpath,$VULKAN_LIB_DIR"
fi

if [[ ! -f "$FLAGS_FILE" ]]; then
  echo "Missing compile flags file: $FLAGS_FILE" >&2
  exit 1
fi

if [[ ! -f "$MAIN_SOURCE_FILE" ]]; then
  echo "Missing source file: $MAIN_SOURCE_FILE" >&2
  exit 1
fi

if [[ ! -f "$GAME_SOURCE_FILE" ]]; then
  echo "Missing source file: $GAME_SOURCE_FILE" >&2
  exit 1
fi

FLAG_LINES="$(sed -E 's/[[:space:]]*#.*$//; /^[[:space:]]*$/d' "$FLAGS_FILE")"

if [[ "$UNAME_S" != "Darwin" ]]; then
  FLAG_LINES="$(printf '%s\n' "$FLAG_LINES" | awk '
    skip_next { skip_next = 0; next }
    $0 == "-isysroot" { skip_next = 1; next }
    { print }
  ')"
fi

if [[ "$MODE" == "release" ]]; then
  FLAG_LINES="$(printf '%s\n' "$FLAG_LINES" | sed -E '/^-fsanitize(=|$)/d')"
  MODE_FLAGS="-O3 -DNDEBUG"
else
  MODE_FLAGS="-Og -g3 -DDEBUG"
fi

FLAGS="$(printf '%s\n' "$FLAG_LINES" | tr '\n' ' ')"
FLAGS="$FLAGS $VULKAN_INCLUDE_FLAGS"
MAIN_LINK_FLAGS="$MAIN_LINK_FLAGS $VULKAN_LINK_FLAGS"

mkdir -p "$BUILD_DIR"

if [[ "$UNAME_S" == "Darwin" ]]; then
  # Intentionally uses normal shell expansion of env flags from your shell profile.
  # shellcheck disable=SC2086
  eval "$COMPILER \
    ${CPPFLAGS:-} ${CFLAGS:-} ${CXXFLAGS:-} \
    $FLAGS $MODE_FLAGS \
    $MAIN_SOURCE_FLAGS \
    ${LDFLAGS:-} $MAIN_LINK_FLAGS \
    -o \"$OUTPUT_FILE\""
else
  # Intentionally uses normal shell expansion of env flags from your shell profile.
  # shellcheck disable=SC2086
  $COMPILER \
    ${CPPFLAGS:-} ${CFLAGS:-} ${CXXFLAGS:-} \
    $FLAGS $MODE_FLAGS \
    "$MAIN_SOURCE_FILE" \
    ${LDFLAGS:-} $MAIN_LINK_FLAGS \
    -o "$OUTPUT_FILE"
fi

$COMPILER \
  ${CPPFLAGS:-} ${CFLAGS:-} ${CXXFLAGS:-} \
  $FLAGS $MODE_FLAGS \
  "$GAME_SOURCE_FILE" \
  ${LDFLAGS:-} $GAME_LINK_FLAGS \
  -o "$GAME_OUTPUT_FILE"

echo "Built ($MODE): $OUTPUT_FILE"
echo "Built ($MODE): $GAME_OUTPUT_FILE"
