#!/usr/bin/env bash

set -euo pipefail

SCRIPT_DIR="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)"
ROOT_DIR="$SCRIPT_DIR"
BIN_DIR="$ROOT_DIR/bin"
APP_MAIN="$ROOT_DIR/src/app/editor_main.cpp"
RGFW_IMPL_CPP="$ROOT_DIR/src/third_party/rgfw/rgfw_impl.cpp"
COMMAND="${1:-debug}"
MODE="${2:-debug}"

setup_platform() {
  case "$(uname -s)" in
    Darwin)
      PLATFORM=macos
      VULKAN_LIB_NAME=libvulkan.dylib
      VULKAN_FALLBACK_INCLUDE_DIRS=(/usr/local/include /opt/homebrew/include)
      VULKAN_FALLBACK_LIB_DIRS=(/usr/local/lib /opt/homebrew/lib)
      ;;
    Linux)
      PLATFORM=linux
      VULKAN_LIB_NAME=libvulkan.so
      VULKAN_FALLBACK_INCLUDE_DIRS=(/usr/include /usr/local/include)
      VULKAN_FALLBACK_LIB_DIRS=(
        /usr/lib
        /usr/local/lib
        /usr/lib/x86_64-linux-gnu
        /usr/lib/aarch64-linux-gnu
      )
      ;;
    *)
      printf 'unsupported platform: %s\n' "$(uname -s)" >&2
      exit 1
      ;;
  esac
}

setup_mode() {
  case "$1" in
    debug)
      MODE_FLAGS=(-g -O0)
      ;;
    release)
      MODE_FLAGS=(-O2 -DNDEBUG)
      ;;
    *)
      printf 'usage: %s [debug|release|build|compdb] [debug|release]\n' "$0" >&2
      exit 1
      ;;
  esac
}

compile_shaders() {
  local shader_dir="$ROOT_DIR/assets/shaders"
  local shader_out="$BIN_DIR/shaders"
  local shader_compiler=""

  mkdir -p "$shader_out"

  for candidate in glslangValidator /usr/local/bin/glslangValidator /opt/homebrew/bin/glslangValidator; do
    if [[ -x "$candidate" ]] || command -v "$candidate" >/dev/null 2>&1; then
      shader_compiler="$candidate"
      break
    fi
  done

  if [[ -n "$shader_compiler" ]]; then
    "$shader_compiler" -V "$shader_dir/sprite.vert" -o "$shader_out/sprite.vert.spv"
    "$shader_compiler" -V "$shader_dir/sprite.frag" -o "$shader_out/sprite.frag.spv"
    return
  fi

  if command -v glslc >/dev/null 2>&1 || [[ -x /usr/local/bin/glslc ]] || [[ -x /opt/homebrew/bin/glslc ]]; then
    local glslc_bin
    glslc_bin="$(command -v glslc || true)"
    if [[ -z "$glslc_bin" ]]; then
      if [[ -x /usr/local/bin/glslc ]]; then
        glslc_bin=/usr/local/bin/glslc
      else
        glslc_bin=/opt/homebrew/bin/glslc
      fi
    fi

    "$glslc_bin" "$shader_dir/sprite.vert" -o "$shader_out/sprite.vert.spv"
    "$glslc_bin" "$shader_dir/sprite.frag" -o "$shader_out/sprite.frag.spv"
    return
  fi

  printf 'missing shader compiler: glslangValidator or glslc\n' >&2
  exit 1
}

setup_toolchain() {
  VULKAN_INCLUDE_DIR="$(pkg-config --variable=includedir vulkan)"
  VULKAN_LIB_DIR="$(pkg-config --variable=libdir vulkan)"

  if [[ ! -f "$VULKAN_INCLUDE_DIR/vulkan/vulkan.h" ]]; then
    for candidate in "${VULKAN_FALLBACK_INCLUDE_DIRS[@]}"; do
      if [[ -f "$candidate/vulkan/vulkan.h" ]]; then
        VULKAN_INCLUDE_DIR="$candidate"
        break
      fi
    done
  fi

  if [[ ! -f "$VULKAN_LIB_DIR/$VULKAN_LIB_NAME" ]]; then
    for candidate in "${VULKAN_FALLBACK_LIB_DIRS[@]}"; do
      if [[ -f "$candidate/$VULKAN_LIB_NAME" ]]; then
        VULKAN_LIB_DIR="$candidate"
        break
      fi
    done
  fi

  VULKAN_CFLAGS=(-I"$VULKAN_INCLUDE_DIR")
  if [[ "$PLATFORM" == "macos" ]]; then
    VULKAN_LIBS=(-L"$VULKAN_LIB_DIR" -Wl,-rpath,"$VULKAN_LIB_DIR" -lvulkan)
    PLATFORM_LIBS=(-framework Cocoa -framework CoreVideo -framework IOKit)
  else
    VULKAN_LIBS=(-L"$VULKAN_LIB_DIR" -lvulkan)
    PLATFORM_LIBS=(-lX11 -lXrandr -lm)
  fi

  COMMON_FLAGS=(
    -std=c++11
    -Wall
    -Wextra
    -Werror
    -Wno-unused-function
    -Wno-missing-field-initializers
    -I"$ROOT_DIR/src"
    "-DASSET_DIR=\"$BIN_DIR\""
  )
}

syntax_check_source() {
  "$CXX" \
    "${COMMON_FLAGS[@]}" \
    "${MODE_FLAGS[@]}" \
    "${VULKAN_CFLAGS[@]}" \
    -fsyntax-only \
    "$1"
}

syntax_check_header() {
  "$CXX" \
    "${COMMON_FLAGS[@]}" \
    "${MODE_FLAGS[@]}" \
    "${VULKAN_CFLAGS[@]}" \
    -x c++-header \
    -fsyntax-only \
    "$1"
}

collect_sources() {
  SYNTAX_SOURCES=()
  while IFS= read -r file; do
    SYNTAX_SOURCES+=("$file")
  done < <(
    find "$ROOT_DIR/src" -type f -name '*.cpp' \
      ! -path "$ROOT_DIR/src/app/*_main.cpp" \
      ! -path "$ROOT_DIR/src/os/os_memory_win32.cpp" \
      ! -path "$ROOT_DIR/src/os/os_threads_win32.cpp" \
      | sort
  )

  PUBLIC_HEADERS=()
  while IFS= read -r file; do
    PUBLIC_HEADERS+=("$file")
  done < <(find "$ROOT_DIR/src" -type f -name '*.h' | sort)
}

do_build() {
  local mode="$1"

  setup_platform
  setup_mode "$mode"

  CXX="${CXX:-clang++}"
  mkdir -p "$BIN_DIR"
  compile_shaders
  setup_toolchain
  collect_sources

  for source in "${SYNTAX_SOURCES[@]}"; do
    syntax_check_source "$source"
  done

  for header in "${PUBLIC_HEADERS[@]}"; do
    syntax_check_header "$header"
  done

  "$CXX" \
    "${COMMON_FLAGS[@]}" \
    "${MODE_FLAGS[@]}" \
    "${VULKAN_CFLAGS[@]}" \
    "$APP_MAIN" \
    "$RGFW_IMPL_CPP" \
    "${VULKAN_LIBS[@]}" \
    "${PLATFORM_LIBS[@]}" \
    -pthread \
    -o "$BIN_DIR/main"

  printf 'built %s/main\n' "$BIN_DIR"
}

case "$COMMAND" in
  build)
    do_build "$MODE"
    ;;
  debug|release)
    do_build "$COMMAND"
    ;;
  compdb)
    BUILD_LOG="$(mktemp)"
    trap 'rm -f "$BUILD_LOG"' EXIT

    PS4='' bash -x "$0" __build "$MODE" >"$BUILD_LOG" 2>&1
    compiledb -f -o "$ROOT_DIR/compile_commands.json" -p "$BUILD_LOG"
    printf 'generated %s/compile_commands.json\n' "$ROOT_DIR"
    ;;
  __build)
    do_build "$MODE"
    ;;
  *)
    printf 'usage: %s [debug|release|build|compdb] [debug|release]\n' "$0" >&2
    exit 1
    ;;
esac
