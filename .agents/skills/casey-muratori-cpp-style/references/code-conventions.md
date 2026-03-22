# Code Conventions Reference

Concrete formatting, structural, and idiomatic patterns for Casey Muratori-style C++.

## Table of Contents
1. [File Structure](#file-structure)
2. [Brace Style and Formatting](#brace-style-and-formatting)
3. [Type Definitions](#type-definitions)
4. [Function Patterns](#function-patterns)
5. [Memory Arena Patterns](#memory-arena-patterns)
6. [Debug and Development Macros](#debug-and-development-macros)
7. [Immediate-Mode Patterns](#immediate-mode-patterns)
8. [Entity System Patterns](#entity-system-patterns)
9. [Common Idioms](#common-idioms)
10. [Build Setup](#build-setup)

---

## File Structure

Casey uses a **unity build** (single compilation unit). There are no separately compiled `.cpp` files linked
together. Instead, one root file `#include`s everything:

```cpp
// win32_handmade.cpp — the platform layer, also the build target

#include "handmade_platform.h"  // Shared types between platform and game
#include "handmade.cpp"         // Yes, including a .cpp — this is the unity build

// ... Win32 platform code follows
```

The game code itself may also chain-include:
```cpp
// handmade.cpp
#include "handmade.h"
#include "handmade_math.h"
#include "handmade_intrinsics.h"
#include "handmade_world.cpp"
#include "handmade_entity.cpp"
#include "handmade_render.cpp"
#include "handmade_audio.cpp"
```

Benefits: no linker issues, no header-guards needed (each file is included exactly once), the compiler
sees everything and can inline aggressively, and build times stay fast because there's only one
translation unit.

File extensions:
- `.h` for declarations, shared types, macros
- `.cpp` for implementation (even though they get `#include`d)

---

## Brace Style and Formatting

We use **K&R style** — opening brace on the same line. The one exception is the function return type,
which still goes on its own line above the function name (a Casey convention).

```cpp
internal void
draw_rectangle(LoadedBitmap *buffer, V2 min, V2 max, f32 r, f32 g, f32 b) {
    // Return type on its own line, function name + opening brace on the next

    i32 min_x = round_f32_to_s32(min.x);
    i32 min_y = round_f32_to_s32(min.y);
    i32 max_x = round_f32_to_s32(max.x);
    i32 max_y = round_f32_to_s32(max.y);

    if(min_x < 0) {
        min_x = 0;
    }

    for(i32 y = min_y; y < max_y; ++y) {
        for(i32 x = min_x; x < max_x; ++x) {
            // ...
        }
    }
}
```

Key formatting details:
- **Return type on its own line** before the function name. The function name and `{` share a line.
- **No space between `if`/`for`/`while` and the parenthesis:** `if(condition)` not `if (condition)`.
- **4-space indentation** (no tabs).
- **One blank line** between functions or logical sections.
- **Short single-statement blocks** can stay on one line when it aids readability:
  ```cpp
  if(pressed) { do_auto_snap(editor); }
  ```
- **Pointer declarations:** `Type *name` (space before the asterisk, asterisk attached to name):
  ```cpp
  Entity *entity = get_entity(state, index);
  void *result = arena->base + arena->used;
  ```
- **Structs, enums, unions** — brace on same line:
  ```cpp
  struct Entity {
      EntityType type;
      V2 p;
  };

  enum EntityType {
      entity_type_none,
      entity_type_hero,
  };
  ```

---

## Type Definitions

### Base Types
Always define short aliases for fixed-width types at the top of your platform header:

```cpp
#include <stdint.h>

typedef int8_t    i8;
typedef int16_t   i16;
typedef int32_t   i32;
typedef int64_t   i64;

typedef uint8_t   u8;
typedef uint16_t  u16;
typedef uint32_t  u32;
typedef uint64_t  u64;

typedef float     f32;
typedef double    f64;

typedef size_t    MemoryIndex;
```

### Math Types
Use `union` with anonymous structs for flexible access. Types are `PascalCase`:

```cpp
union V2 {
    struct {
        f32 x, y;
    };
    f32 e[2];
};

union V3 {
    struct {
        f32 x, y, z;
    };
    struct {
        f32 r, g, b;
    };
    struct {
        V2 xy;
        f32 _ignored0;
    };
    f32 e[3];
};

union V4 {
    struct {
        f32 x, y, z, w;
    };
    struct {
        f32 r, g, b, a;
    };
    struct {
        V3 xyz;
        f32 _ignored0;
    };
    struct {
        V3 rgb;
        f32 _ignored1;
    };
    f32 e[4];
};
```

Constructor-like free functions (snake_case, not constructors):
```cpp
inline V2
v2(f32 x, f32 y) {
    V2 result = {x, y};
    return(result);
}

inline V3
v3(f32 x, f32 y, f32 z) {
    V3 result = {x, y, z};
    return(result);
}
```

Operators (defined as free functions or `inline` in the header):
```cpp
inline V2 operator+(V2 a, V2 b) { return v2(a.x + b.x, a.y + b.y); }
inline V2 operator-(V2 a, V2 b) { return v2(a.x - b.x, a.y - b.y); }
inline V2 operator*(f32 s, V2 a) { return v2(s * a.x, s * a.y); }
inline V2 operator*(V2 a, f32 s) { return s * a; }
inline V2 &operator+=(V2 &a, V2 b) { a = a + b; return a; }
inline V2 &operator*=(V2 &a, f32 s) { a = a * s; return a; }
inline V2 operator-(V2 a) { return v2(-a.x, -a.y); }

inline f32 inner(V2 a, V2 b) { return a.x*b.x + a.y*b.y; }
inline f32 length_sq(V2 a) { return inner(a, a); }
```

### Enums
Type name is `PascalCase`, values are `snake_case` with a type prefix:

```cpp
enum EntityType {
    entity_type_none,
    entity_type_hero,
    entity_type_wall,
    entity_type_familiar,
    entity_type_sword,
    entity_type_stairwell,
};
```

### Struct Initialization
Use C-style aggregate initialization or zero-init:
```cpp
Entity entity = {};              // Zero-initialize everything
V2 direction = {1.0f, 0.0f};    // Specific values
*entity = {};                    // Zero out via pointer
```

---

## Function Patterns

### Return-type-on-its-own-line
Always put the return type (and any qualifiers like `internal`, `inline`) on the line above the function
name. The function name and opening brace share a line (K&R):

```cpp
internal void
game_update_and_render(GameMemory *memory, GameInput *input, GameOffscreenBuffer *buffer) {
    // ...
}

inline f32
square(f32 a) {
    f32 result = a * a;
    return(result);
}
```

### The `result` Variable Pattern
Declare a `result` variable at the top, set it, and return it at the end. This makes it easy to set
breakpoints on the return and inspect the value:

```cpp
internal Entity *
get_entity(GameState *state, u32 index) {
    Entity *result = 0;

    if((index > 0) && (index < state->entity_count)) {
        result = state->entities + index;
    }

    return(result);
}
```

### Parenthesized Return
Wrap `return` values in parentheses: `return(result);` not `return result;`. This is a style preference
that makes `return` visually consistent with function calls.

### `array_count` Macro
A ubiquitous macro for getting the element count of a static array:
```cpp
#define array_count(array) (sizeof(array) / sizeof((array)[0]))
```

---

## Memory Arena Patterns

### Full Arena Implementation

```cpp
struct MemoryArena {
    MemoryIndex size;
    u8 *base;
    MemoryIndex used;

    i32 temp_count;  // Track outstanding temporary memory blocks
};

struct TemporaryMemory {
    MemoryArena *arena;
    MemoryIndex used;  // Saved position to restore to
};

internal void
initialize_arena(MemoryArena *arena, MemoryIndex size, void *base) {
    arena->size = size;
    arena->base = (u8 *)base;
    arena->used = 0;
    arena->temp_count = 0;
}

#define push_struct(arena, type) (type *)push_size_(arena, sizeof(type))
#define push_array(arena, count, type) (type *)push_size_(arena, (count)*sizeof(type))
#define push_size(arena, size) push_size_(arena, size)

internal void *
push_size_(MemoryArena *arena, MemoryIndex size) {
    assert((arena->used + size) <= arena->size);
    void *result = arena->base + arena->used;
    arena->used += size;
    return(result);
}

internal TemporaryMemory
begin_temporary_memory(MemoryArena *arena) {
    TemporaryMemory result;
    result.arena = arena;
    result.used = arena->used;
    ++arena->temp_count;
    return(result);
}

internal void
end_temporary_memory(TemporaryMemory temp_mem) {
    MemoryArena *arena = temp_mem.arena;
    assert(arena->used >= temp_mem.used);
    arena->used = temp_mem.used;
    assert(arena->temp_count > 0);
    --arena->temp_count;
}
```

### Usage
```cpp
// Permanent allocations
GameState *game_state = push_struct(&game_memory->permanent_arena, GameState);
game_state->entities = push_array(&game_state->world_arena, MAX_ENTITY_COUNT, Entity);

// Temporary allocations (e.g., per-frame scratch work)
TemporaryMemory temp_mem = begin_temporary_memory(&transient_arena);
{
    LoadedBitmap *temp_bitmap = push_struct(&transient_arena, LoadedBitmap);
    // ... use temp_bitmap ...
}
end_temporary_memory(temp_mem);
// Memory is now available for reuse
```

---

## Debug and Development Macros

```cpp
// Scope control macros
#define internal        static
#define local_persist   static
#define global_variable static

// Build configuration
#define HANDMADE_INTERNAL 1   // Enable development tools
#define HANDMADE_SLOW    1    // Enable slow-path assertions

// Assert: crash into debugger on failure
#if HANDMADE_SLOW
#define assert(expression) if(!(expression)) {*(volatile int *)0 = 0;}
#else
#define assert(expression)
#endif

// Invalid code path marker
#define invalid_code_path assert(!"invalid_code_path")
#define invalid_default_case default: {invalid_code_path;} break

// Kilobytes/Megabytes/Gigabytes helpers
#define kilobytes(value) ((value)*1024LL)
#define megabytes(value) (kilobytes(value)*1024LL)
#define gigabytes(value) (megabytes(value)*1024LL)
#define terabytes(value) (gigabytes(value)*1024LL)
```

---

## Immediate-Mode Patterns

Casey coined the term "Immediate Mode GUI" (IMGUI). The core idea: UI is drawn every frame based on current
state. No persistent widget objects. No retained-mode tree. Just function calls that both draw and handle
input:

```cpp
// Immediate-mode button: returns true the frame it's clicked
internal bool
do_button(UiContext *ui, char *text, V2 position, V2 size) {
    bool result = false;

    V2 mouse_p = ui->mouse_p;
    bool is_hot = (mouse_p.x >= position.x && mouse_p.x < position.x + size.x &&
                  mouse_p.y >= position.y && mouse_p.y < position.y + size.y);

    V4 color = is_hot ? v4(0.8f, 0.8f, 0.2f, 1.0f) : v4(0.5f, 0.5f, 0.5f, 1.0f);

    if(is_hot && ui->mouse_button_pressed) {
        result = true;
        color = v4(1.0f, 1.0f, 0.0f, 1.0f);
    }

    draw_rectangle(ui->buffer, position, position + size, color);
    draw_text(ui->buffer, text, position, ui->font);

    return(result);
}

// Usage — reads exactly like a description of the UI
layout.row();
if(do_button(&ui, "Auto Snap", layout.p, layout.row_size)) {
    do_auto_snap(editor);
}

layout.row();
if(do_button(&ui, "Reset Orientation", layout.p, layout.row_size)) {
    reset_entity_orientation(selected_entity);
}
```

---

## Entity System Patterns

Casey avoids ECS frameworks and inheritance-based entity systems. His approach in Handmade Hero uses a
"fat struct" — a single entity struct that contains all possible fields, with an enum discriminating type:

```cpp
struct Entity {
    EntityType type;

    // Spatial
    V2 p;
    V2 dp;
    f32 z;
    f32 dz;

    // Visual
    f32 width;
    f32 height;
    u32 facing_direction;
    f32 t_bob;

    // Gameplay
    i32 hit_point_max;
    i32 hit_points;
    bool collides;
    f32 distance_limit;  // For swords, projectiles, etc.
};
```

Behavior is handled by `switch` on type:

```cpp
internal void
update_entity(GameState *state, Entity *entity, f32 dt) {
    switch(entity->type) {
        case entity_type_hero: {
            // Hero-specific update
        } break;

        case entity_type_sword: {
            // Sword-specific update
            entity->distance_limit -= length(entity->dp) * dt;
            if(entity->distance_limit <= 0.0f) {
                // Sword has traveled max distance
                entity->dp = {};
            }
        } break;

        case entity_type_familiar: {
            // Follow the hero
        } break;

        invalid_default_case;
    }
}
```

This is simple, explicit, debuggable, and cache-friendly (all entities in a flat array).

---

## Common Idioms

### Safe truncation
```cpp
#define safe_truncate_u64(value) (assert((value) <= 0xFFFFFFFF), (u32)(value))
```

### Swap macro
```cpp
#define swap(a, b) { auto temp_ = a; a = b; b = temp_; }
```

### Minimum / Maximum
```cpp
#define minimum(a, b) ((a) < (b) ? (a) : (b))
#define maximum(a, b) ((a) > (b) ? (a) : (b))
#define clamp(min_val, value, max_val) maximum(min_val, minimum(value, max_val))
```

### Pointer arithmetic for arrays
```cpp
// Iterate entities via pointer, not index
Entity *entity = state->entities;
for(u32 entity_index = 0; entity_index < state->entity_count; ++entity_index, ++entity) {
    // Use entity-> directly
}
```

### Clearing memory
```cpp
#define zero_struct(instance) zero_size(sizeof(instance), &(instance))
#define zero_array(count, pointer) zero_size((count)*sizeof((pointer)[0]), pointer)

internal void
zero_size(MemoryIndex size, void *ptr) {
    u8 *byte = (u8 *)ptr;
    while(size--) {
        *byte++ = 0;
    }
}
```

### Intrinsics wrappers
```cpp
#include <intrin.h>  // MSVC
// or
#include <x86intrin.h>  // GCC/Clang

inline i32
round_f32_to_s32(f32 value) {
    i32 result = (i32)roundf(value);
    return(result);
}

inline u32
rotate_left(u32 value, i32 amount) {
    u32 result = _rotl(value, amount);
    return(result);
}
```

---

## Build Setup

### Windows (build.bat)
```bat
@echo off
set CommonCompilerFlags=-nologo -Gm- -GR- -EHa- -Od -Oi -WX -W4 -wd4201 -wd4100 -wd4189 -wd4505 -DHANDMADE_INTERNAL=1 -DHANDMADE_SLOW=1 -FC -Zi
set CommonLinkerFlags=-incremental:no -opt:ref user32.lib gdi32.lib winmm.lib

IF NOT EXIST ..\..\build mkdir ..\..\build
pushd ..\..\build

REM 64-bit build
cl %CommonCompilerFlags% ..\handmade\code\win32_handmade.cpp /link %CommonLinkerFlags%

popd
```

Key compiler flags explained:
- `-Gm-` : Disable minimal rebuild (incompatible with modern builds)
- `-GR-` : Disable RTTI (we don't use `dynamic_cast`)
- `-EHa-` : Disable exceptions (we don't use `try`/`catch`)
- `-WX` : Treat warnings as errors
- `-W4` : Warning level 4
- `-wd4201` : Suppress "nameless struct/union" warning (we use anonymous structs in unions)
- `-wd4100` : Suppress "unreferenced formal parameter" (common in platform callbacks)
- `-wd4189` : Suppress "local variable initialized but not referenced"
- `-wd4505` : Suppress "unreferenced local function"
- `-Od` : No optimization (debug)
- `-Oi` : Enable intrinsics
- `-Zi` : Generate debug info
- `-FC` : Full path in diagnostics

### Linux (build.sh)
```bash
#!/bin/bash
mkdir -p build
pushd build
g++ -g -Wall -Wextra -Wno-unused-parameter -Wno-unused-function \
    -DHANDMADE_INTERNAL=1 -DHANDMADE_SLOW=1 \
    ../code/linux_handmade.cpp \
    -o handmade \
    -lX11 -lGL -lasound -lpthread -lm
popd
```

### DLL Hot-Reload Pattern (Windows)
Build the game code as a separate DLL so the platform layer can reload it:

```bat
REM Build game DLL
cl %CommonCompilerFlags% -LD ..\handmade\code\handmade.cpp /link -PDB:handmade_%random%.pdb -EXPORT:game_update_and_render -EXPORT:game_get_sound_samples

REM Build platform EXE
cl %CommonCompilerFlags% ..\handmade\code\win32_handmade.cpp /link %CommonLinkerFlags%
```

The platform layer uses `LoadLibrary` / `GetProcAddress` to load function pointers from the DLL, and
`CopyFile` to copy the DLL before loading (so the compiler can overwrite the original while it's running).
