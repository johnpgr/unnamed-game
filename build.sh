#!/usr/bin/env bash

set -euo pipefail

ROOT_DIR="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)"
FLAGS_FILE="$ROOT_DIR/compile_flags.txt"
BUILD_DIR="$ROOT_DIR/build"
MAIN_SOURCES=(
  "$ROOT_DIR/src/main.cpp"
  "$ROOT_DIR/src/renderer/vulkan.cpp"
)
GAME_SOURCE_FILE="$ROOT_DIR/src/game.cpp"
SDL_SOURCE_DIR="$ROOT_DIR/lib/SDL"
GLM_SOURCE_DIR="$ROOT_DIR/lib/glm"
MODE="${BUILD_MODE:-debug}"
BUILD_SDL=0
OUTPUT_FILE=""

for arg in "$@"; do
  case "$arg" in
    debug|release)
      MODE="$arg"
      ;;
    sdl|--sdl)
      BUILD_SDL=1
      ;;
    *)
      if [[ -z "$OUTPUT_FILE" ]]; then
        OUTPUT_FILE="$arg"
      else
        echo "Unexpected argument: $arg" >&2
        echo "Usage: $0 [debug|release] [sdl|--sdl] [output_path]" >&2
        exit 1
      fi
      ;;
  esac
done

OUTPUT_FILE="${OUTPUT_FILE:-$BUILD_DIR/main}"
COMPILER="${CXX:-clang++}"
C_COMPILER="${CC:-cc}"
UNAME_S="$(uname -s)"
CMAKE_BUILD_TYPE="Debug"

if [[ "$MODE" == "release" ]]; then
  CMAKE_BUILD_TYPE="Release"
fi

require_command() {
  if ! command -v "$1" >/dev/null 2>&1; then
    echo "Missing required command: $1" >&2
    exit 1
  fi
}

if [[ ! -f "$FLAGS_FILE" ]]; then
  echo "Missing compile flags file: $FLAGS_FILE" >&2
  exit 1
fi

for source_file in "${MAIN_SOURCES[@]}" "$GAME_SOURCE_FILE"; do
  if [[ ! -f "$source_file" ]]; then
    echo "Missing source file: $source_file" >&2
    exit 1
  fi
done

if [[ ! -f "$GLM_SOURCE_DIR/glm/glm.hpp" ]]; then
  echo "Missing GLM dependency under lib/. Run: git submodule update --init --recursive" >&2
  exit 1
fi

if [[ "$BUILD_SDL" -eq 1 && ! -f "$SDL_SOURCE_DIR/CMakeLists.txt" ]]; then
  echo "Missing SDL dependency under lib/. Run: git submodule update --init --recursive" >&2
  exit 1
fi

require_command "$COMPILER"
require_command "$C_COMPILER"
require_command pkg-config

MAIN_LINK_FLAGS=()
GAME_LINK_FLAGS=()
VULKAN_INCLUDE_FLAGS=()
VULKAN_LINK_FLAGS=()

case "$UNAME_S" in
  Linux)
    GAME_OUTPUT_FILE="$BUILD_DIR/libgame.so"
    GAME_LINK_FLAGS=(-shared -fPIC)
    MAIN_LINK_FLAGS=(-lvulkan -ldl)
    ;;
  Darwin)
    GAME_OUTPUT_FILE="$BUILD_DIR/libgame.dylib"
    GAME_LINK_FLAGS=(-dynamiclib -fPIC)

    VULKAN_CANDIDATE_ROOTS=()
    if [[ -n "${VULKAN_SDK:-}" ]]; then
      VULKAN_CANDIDATE_ROOTS+=("$VULKAN_SDK")
    fi
    VULKAN_CANDIDATE_ROOTS+=("/usr/local" "/opt/homebrew")

    VULKAN_SDK_ROOT=""
    for candidate_root in "${VULKAN_CANDIDATE_ROOTS[@]}"; do
      [[ -n "$candidate_root" ]] || continue
      if [[ -d "$candidate_root/include/vulkan" ]] &&
         [[ -f "$candidate_root/lib/libvulkan.dylib" || -f "$candidate_root/lib/libvulkan.1.dylib" ]]; then
        VULKAN_SDK_ROOT="$candidate_root"
        break
      fi
    done

    if [[ -z "$VULKAN_SDK_ROOT" ]]; then
      echo "Unable to find Vulkan headers and loader on macOS. Set VULKAN_SDK or install a standard SDK layout." >&2
      exit 1
    fi

    VULKAN_INCLUDE_FLAGS=(-I"$VULKAN_SDK_ROOT/include")
    VULKAN_LINK_FLAGS=(-L"$VULKAN_SDK_ROOT/lib" -lvulkan -Wl,-rpath,"$VULKAN_SDK_ROOT/lib")
    MAIN_LINK_FLAGS=("${VULKAN_LINK_FLAGS[@]}")
    ;;
  *)
    echo "Unsupported host OS: $UNAME_S" >&2
    exit 1
    ;;
esac

readarray -t FLAG_ARRAY < <(sed -E '/^[[:space:]]*$/d' "$FLAGS_FILE")
if [[ "$UNAME_S" != "Darwin" ]]; then
  readarray -t FLAG_ARRAY < <(printf '%s\n' "${FLAG_ARRAY[@]}" | awk '
    skip_next { skip_next = 0; next }
    $0 == "-isysroot" { skip_next = 1; next }
    { print }
  ')
fi

MODE_FLAGS=(-Og -g3 -DDEBUG)
if [[ "$MODE" == "release" ]]; then
  readarray -t FLAG_ARRAY < <(printf '%s\n' "${FLAG_ARRAY[@]}" | sed -E '/^-fsanitize(=|$)/d')
  MODE_FLAGS=(-O3 -DNDEBUG)
fi

split_env_flags() {
  local value="$1"
  local -n output_ref="$2"
  output_ref=()
  if [[ -n "$value" ]]; then
    read -r -a output_ref <<<"$value"
  fi
}

CPPFLAGS_ARRAY=()
CFLAGS_ARRAY=()
CXXFLAGS_ARRAY=()
LDFLAGS_ARRAY=()
split_env_flags "${CPPFLAGS:-}" CPPFLAGS_ARRAY
split_env_flags "${CFLAGS:-}" CFLAGS_ARRAY
split_env_flags "${CXXFLAGS:-}" CXXFLAGS_ARRAY
split_env_flags "${LDFLAGS:-}" LDFLAGS_ARRAY

mkdir -p "$BUILD_DIR"

SDL_BUILD_DIR="$BUILD_DIR/sdl-${UNAME_S,,}-$MODE"
SDL_INSTALL_DIR="$SDL_BUILD_DIR/install"
if [[ "$BUILD_SDL" -eq 1 ]]; then
  require_command cmake
  require_command ninja

  cmake \
    -S "$SDL_SOURCE_DIR" \
    -B "$SDL_BUILD_DIR" \
    -G Ninja \
    -DCMAKE_BUILD_TYPE="$CMAKE_BUILD_TYPE" \
    -DCMAKE_INSTALL_PREFIX="$SDL_INSTALL_DIR" \
    -DCMAKE_POSITION_INDEPENDENT_CODE=ON \
    -DCMAKE_C_COMPILER="$C_COMPILER" \
    -DCMAKE_CXX_COMPILER="$COMPILER" \
    -DSDL_SHARED=OFF \
    -DSDL_STATIC=ON \
    -DSDL_STATIC_PIC=ON \
    -DSDL_INSTALL=ON \
    -DSDL_TESTS=OFF \
    -DSDL_TEST_LIBRARY=OFF \
    -DSDL_EXAMPLES=OFF

  cmake --build "$SDL_BUILD_DIR" --config "$CMAKE_BUILD_TYPE" --target SDL3-static
  cmake --install "$SDL_BUILD_DIR" --config "$CMAKE_BUILD_TYPE"
fi

SDL_PKGCONFIG_FILE="$(find "$SDL_INSTALL_DIR" -path '*/pkgconfig/sdl3.pc' -print -quit)"
if [[ -z "$SDL_PKGCONFIG_FILE" ]]; then
  echo "Missing built SDL3 artifacts in $SDL_INSTALL_DIR. Run: $0 $MODE sdl" >&2
  exit 1
fi

SDL_PKGCONFIG_DIR="$(dirname "$SDL_PKGCONFIG_FILE")"
SDL_CFLAGS="$(PKG_CONFIG_PATH="$SDL_PKGCONFIG_DIR" pkg-config --cflags sdl3)"
SDL_LIBS="$(PKG_CONFIG_PATH="$SDL_PKGCONFIG_DIR" pkg-config --static --libs sdl3)"

SDL_CFLAGS_ARRAY=()
SDL_LIBS_ARRAY=()
read -r -a SDL_CFLAGS_ARRAY <<<"$SDL_CFLAGS"
read -r -a SDL_LIBS_ARRAY <<<"$SDL_LIBS"

MAIN_COMMAND=("$COMPILER")
MAIN_COMMAND+=("${CPPFLAGS_ARRAY[@]}")
MAIN_COMMAND+=("${CFLAGS_ARRAY[@]}")
MAIN_COMMAND+=("${CXXFLAGS_ARRAY[@]}")
MAIN_COMMAND+=("${FLAG_ARRAY[@]}")
MAIN_COMMAND+=("${MODE_FLAGS[@]}")
MAIN_COMMAND+=("${VULKAN_INCLUDE_FLAGS[@]}")
MAIN_COMMAND+=("${SDL_CFLAGS_ARRAY[@]}")
MAIN_COMMAND+=(-I"$ROOT_DIR/src" -I"$GLM_SOURCE_DIR")
MAIN_COMMAND+=("${MAIN_SOURCES[@]}")
MAIN_COMMAND+=("${LDFLAGS_ARRAY[@]}")
MAIN_COMMAND+=("${SDL_LIBS_ARRAY[@]}")
MAIN_COMMAND+=("${MAIN_LINK_FLAGS[@]}")
MAIN_COMMAND+=(-o "$OUTPUT_FILE")
"${MAIN_COMMAND[@]}"

GAME_COMMAND=("$COMPILER")
GAME_COMMAND+=("${CPPFLAGS_ARRAY[@]}")
GAME_COMMAND+=("${CFLAGS_ARRAY[@]}")
GAME_COMMAND+=("${CXXFLAGS_ARRAY[@]}")
GAME_COMMAND+=("${FLAG_ARRAY[@]}")
GAME_COMMAND+=("${MODE_FLAGS[@]}")
GAME_COMMAND+=(-I"$ROOT_DIR/src" -I"$GLM_SOURCE_DIR")
GAME_COMMAND+=("$GAME_SOURCE_FILE")
GAME_COMMAND+=("${LDFLAGS_ARRAY[@]}")
GAME_COMMAND+=("${GAME_LINK_FLAGS[@]}")
GAME_COMMAND+=(-o "$GAME_OUTPUT_FILE")
"${GAME_COMMAND[@]}"

echo "Built ($MODE): $OUTPUT_FILE"
echo "Built ($MODE): $GAME_OUTPUT_FILE"
