#!/usr/bin/env bash
set -eu
cd "$(dirname "$0")"

# --- Unpack Arguments --------------------------------------------------------
for arg in "$@"; do
  if echo "$arg" | grep -qE '^[a-z_]+$'; then
    eval "$arg=1"
  else
    echo "unknown argument: $arg" >&2; exit 1
  fi
done
if [ "${release:-}" != "1" ]; then debug=1; fi
if [ "${debug:-}" = "1" ];   then echo "[debug mode]"; fi
if [ "${release:-}" = "1" ]; then echo "[release mode]"; fi

# --- Paths -------------------------------------------------------------------
root_dir="$(pwd)"
bin_dir="$root_dir/bin"
src_dir="$root_dir/src"

# --- Detect Platform ---------------------------------------------------------
case "$(uname -s)" in
  Darwin) platform=macos ;;
  Linux)  platform=linux ;;
  *)      echo "unsupported platform: $(uname -s)" >&2; exit 1 ;;
esac

# --- Find Vulkan -------------------------------------------------------------
vk_inc="$(pkg-config --variable=includedir vulkan 2>/dev/null || true)"
vk_lib="$(pkg-config --variable=libdir vulkan 2>/dev/null || true)"
for d in /usr/local/include /opt/homebrew/include /usr/include; do
  [ -f "$d/vulkan/vulkan.h" ] && vk_inc="$d" && break
done
for d in /usr/local/lib /opt/homebrew/lib /usr/lib /usr/lib/x86_64-linux-gnu /usr/lib/aarch64-linux-gnu; do
  [ -f "$d/libvulkan.dylib" ] 2>/dev/null && vk_lib="$d" && break
  [ -f "$d/libvulkan.so" ] 2>/dev/null && vk_lib="$d" && break
done

# --- Per-Platform Settings ---------------------------------------------------
link_glfw="-lglfw"
link_platform=""
if [ "$platform" = "macos" ]; then
  glfw_prefix="$(brew --prefix glfw 2>/dev/null || true)"
  if [ -z "$glfw_prefix" ] || [ ! -f "$glfw_prefix/include/GLFW/glfw3.h" ]; then
    echo "glfw not found. install with: brew install glfw" >&2; exit 1
  fi
  link_glfw="-I$glfw_prefix/include -L$glfw_prefix/lib -lglfw"
  link_vulkan="-I$vk_inc -L$vk_lib -Wl,-rpath,$vk_lib -lvulkan"
else
  link_vulkan="-I$vk_inc -L$vk_lib -lvulkan"
  link_platform="-lX11 -lXrandr -lm"
fi

# --- Compile/Link Line Definitions -------------------------------------------
compiler="${CXX:-clang++}"
common="-std=c++11 -Wall -Wextra -Werror -Wno-unused-function -Wno-missing-field-initializers -I$src_dir -DASSET_DIR=\"$bin_dir\""
if [ "${debug:-}" = "1" ];   then compile="$compiler -g -O0 $common"; fi
if [ "${release:-}" = "1" ]; then compile="$compiler -O2 -DNDEBUG $common"; fi
link="$link_vulkan $link_glfw $link_platform -pthread"

# --- Prep Directories --------------------------------------------------------
mkdir -p "$bin_dir"

# --- Shaders -----------------------------------------------------------------
if [ "${shaders:-}" = "1" ]; then
  shader_dir="$root_dir/assets/shaders"
  shader_out="$bin_dir/shaders"
  mkdir -p "$shader_out"

  shader_compiler=""
  for c in glslangValidator /usr/local/bin/glslangValidator /opt/homebrew/bin/glslangValidator; do
    command -v "$c" >/dev/null 2>&1 && shader_compiler="$c" && break
  done

  if [ -n "$shader_compiler" ]; then
    "$shader_compiler" -V "$shader_dir/sprite.vert" -o "$shader_out/sprite.vert.spv"
    "$shader_compiler" -V "$shader_dir/sprite.frag" -o "$shader_out/sprite.frag.spv"
  else
    glslc_bin=""
    for c in glslc /usr/local/bin/glslc /opt/homebrew/bin/glslc; do
      command -v "$c" >/dev/null 2>&1 && glslc_bin="$c" && break
    done
    if [ -z "$glslc_bin" ]; then
      echo "missing shader compiler: glslangValidator or glslc" >&2; exit 1
    fi
    "$glslc_bin" "$shader_dir/sprite.vert" -o "$shader_out/sprite.vert.spv"
    "$glslc_bin" "$shader_dir/sprite.frag" -o "$shader_out/sprite.frag.spv"
  fi
  echo "compiled shaders"
fi

# --- Build Targets -----------------------------------------------------------
didbuild=""
if [ -z "${editor:-}" ] && [ -z "${compdb:-}" ]; then editor=1; fi

if [ "${editor:-}" = "1" ]; then
  didbuild=1
  $compile "$src_dir/app/editor_main.cpp" $link -o "$bin_dir/main"
  echo "built $bin_dir/main"
fi

# --- Compdb ------------------------------------------------------------------
if [ "${compdb:-}" = "1" ]; then
  didbuild=1
  build_log="$(mktemp)"
  trap 'rm -f "$build_log"' EXIT
  args="editor"
  [ "${debug:-}" = "1" ] && args="$args debug"
  [ "${release:-}" = "1" ] && args="$args release"
  [ "${shaders:-}" = "1" ] && args="$args shaders"
  PS4='' bash -x "$0" $args >"$build_log" 2>&1
  compiledb -f -o "$root_dir/compile_commands.json" -p "$build_log"
  echo "generated $root_dir/compile_commands.json"
fi

# --- Warn On No Builds -------------------------------------------------------
if [ -z "$didbuild" ]; then
  echo "[WARNING] no valid build target. usage: ./build.sh [editor] [debug|release] [shaders|compdb]" >&2
  exit 1
fi
