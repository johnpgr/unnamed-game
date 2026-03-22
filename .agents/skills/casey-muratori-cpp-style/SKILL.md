---
name: casey-muratori-cpp-style
description: >
  Write C++ code in Casey Muratori's "Handmade" style — the procedural, performance-aware, compression-oriented
  approach demonstrated in Handmade Hero and his work at RAD Game Tools. Use this skill whenever the user asks for
  C or C++ code in Casey Muratori's style, "handmade style", "compression-oriented programming", procedural C++
  without OOP, data-oriented C/C++, game-engine-style C++, or any variation of "write it like Casey Muratori would".
  Also trigger when the user explicitly mentions Handmade Hero coding conventions, asks for C++ without STL/libraries,
  or wants performance-oriented C++ that avoids classes, inheritance, virtual functions, RAII, exceptions, and
  templates. Even if the user just says "write this in C-style C++" or "no OOP", this skill captures the philosophy.
---

# Casey Muratori's C++ Programming Style

This skill teaches Claude how to write C++ code in the style Casey Muratori uses in Handmade Hero and advocates
in his blog posts on Semantic Compression and his Performance-Aware Programming course. The philosophy can be
summed up as: **write straightforward procedural code, let architecture emerge from usage, and never pay for
abstractions you don't need.**

**NOTE — User Customizations Applied:**
- **Brace style:** K&R (opening brace on the same line), NOT Allman.
- **Naming:** Rust-like — `snake_case` for functions, variables, enum values, and macro-functions.
  `PascalCase` for type names (structs, enums, unions, typedefs). `UPPER_SNAKE_CASE` for macro constants only.

Read `references/philosophy.md` for the deeper reasoning behind each rule before writing any code.
Read `references/code-conventions.md` for the concrete naming, formatting, and structural patterns to follow.

---

## Core Principles (Quick Reference)

1. **C++ as "C plus a few nice things."** Compile as C++ but write procedurally. Use only: operator overloading,
   function overloading, and the fact that `struct` implies a type name without `typedef`. Ignore the rest of
   C++: no classes with access specifiers, no inheritance hierarchies, no virtual functions, no RAII, no exceptions,
   no `std::` anything, no templates (except extremely rare, trivial cases), no `new`/`delete`, no references
   (use pointers), no move semantics.

2. **Compression-Oriented Programming.** Never pre-design architecture. Write the specific thing you need first.
   When you see the same pattern a second time, *then* extract it into a shared function or struct. Architecture
   emerges from repeated compression, not from upfront planning.

3. **Data is just data.** Structs are plain groups of related variables — "bags of data." Functions operate on
   data; data does not "own" behavior. Separate what the data *is* from what you *do* with it.

4. **Performance awareness.** Think about how data flows through the CPU. Prefer flat arrays of structs (or
   struct-of-arrays when it matters). Avoid pointer-chasing, virtual dispatch, heap-per-object allocation, and
   hidden costs. Understand your cache lines.

5. **No external dependencies.** Talk directly to the OS via its native API (Win32, POSIX, etc.). Don't use SDL,
   GLFW, boost, STL containers, or any "convenience" library. Every dependency is a liability.

6. **Platform layer separation.** Isolate OS-specific code into a thin platform layer. The core application logic
   should be pure computation with no OS calls, making it trivially portable.

7. **Custom memory management.** Allocate large blocks up front (memory arenas). Push sub-allocations linearly.
   No per-object malloc/free. Temporary allocations use a scratch arena that gets reset each frame (or scope).

8. **Robustness over elegance.** If the straightforward approach works, use it. Don't introduce an abstraction
   to make code "look clean" at the cost of debuggability, flexibility, or performance. Long functions are fine
   if they're clear.

---

## When Generating Code

Follow this checklist for every piece of code you write under this style:

### Structure
- Use `struct` for all composite types. No `class`. No access specifiers (`public`, `private`, `protected`).
- Use free functions, not member functions (except operator overloads on math types like `V2`, `V3`, `V4`).
- Group related functions by the data they operate on, not by attaching them to a type.
- Use a "unity build" / single-compilation-unit model when showing build setup. One `.cpp` file `#include`s
  all the others.
- Prefer `internal` (i.e. `static`) for file-scoped functions and `global_variable` for file-scoped globals,
  defined as macros:
  ```cpp
  #define internal        static
  #define local_persist   static
  #define global_variable static
  ```

### Naming (Rust-like)
- **Types (structs, enums, unions, typedefs):** `PascalCase`
  (e.g., `GameState`, `LoadedBitmap`, `MemoryArena`, `V2`, `V3`, `EntityType`).
- **Functions:** `snake_case` (e.g., `draw_rectangle`, `push_size`, `get_entity`, `move_entity`).
- **Variables (locals, parameters, struct members):** `snake_case`
  (e.g., `entity_count`, `offset_x`, `facing_direction`, `hit_points`).
- **Enum values:** `snake_case` with a type prefix (e.g., `entity_type_hero`, `entity_type_wall`).
- **Macro constants:** `UPPER_SNAKE_CASE` (e.g., `TILES_PER_CHUNK`, `HANDMADE_INTERNAL`).
- **Macro functions:** `snake_case` matching function conventions
  (e.g., `push_struct`, `push_array`, `array_count`).
- **Typedef'd base types:** Short lowercase names:
  ```cpp
  typedef int8_t   i8;
  typedef int16_t  i16;
  typedef int32_t  i32;
  typedef int64_t  i64;
  typedef uint8_t  u8;
  typedef uint16_t u16;
  typedef uint32_t u32;
  typedef uint64_t u64;
  typedef float    f32;
  typedef double   f64;
  // use built-in bool
  ```

### Brace Style (K&R)
- Opening brace on the **same line** as the statement. Closing brace on its own line:
  ```cpp
  internal void
  do_something(GameState *state, f32 dt) {
      if(state->entity_count > 0) {
          for(u32 i = 0; i < state->entity_count; ++i) {
              // ...
          }
      }
  }
  ```
- The **return type still goes on its own line** above the function name (this is a Casey convention
  independent of brace style):
  ```cpp
  internal Entity *
  get_entity(GameState *state, u32 index) {
      // ...
  }
  ```

### Memory
- Define a `MemoryArena` struct and allocation macros:
  ```cpp
  struct MemoryArena {
      u8 *base;
      size_t size;
      size_t used;
  };

  #define push_struct(arena, type) (type *)push_size_(arena, sizeof(type))
  #define push_array(arena, count, type) (type *)push_size_(arena, (count)*sizeof(type))

  internal void *
  push_size_(MemoryArena *arena, size_t size) {
      assert((arena->used + size) <= arena->size);
      void *result = arena->base + arena->used;
      arena->used += size;
      return(result);
  }
  ```
- For temporary allocations, use `begin_temporary_memory` / `end_temporary_memory` pairs that save and
  restore the arena's `used` pointer.
- Never call `malloc`/`free`/`new`/`delete` in hot code. Reserve those for the platform layer's initial
  allocation only.

### Control Flow
- Use plain `if`/`else`, `switch`, and `for` loops. No clever control-flow abstractions.
- Prefer `switch` statements over polymorphism or function-pointer tables for type dispatch.
- Callbacks are a last resort. Plain switch statements are almost always clearer.
- Use `assert()` liberally for assumptions about invariants. Define it to break into the debugger:
  ```cpp
  #if HANDMADE_SLOW
  #define assert(expression) if(!(expression)) {*(volatile int *)0 = 0;}
  #else
  #define assert(expression)
  #endif
  ```

### Math Types
- Define your own `V2`, `V3`, `V4`, `M4x4`, etc. This is the one place operator overloading shines:
  ```cpp
  union V2 {
      struct { f32 x, y; };
      f32 e[2];
  };

  inline V2 operator+(V2 a, V2 b) { return {a.x + b.x, a.y + b.y}; }
  inline V2 operator*(f32 s, V2 a) { return {s * a.x, s * a.y}; }
  // etc.
  ```
- Use `union` with anonymous structs to allow both named and indexed access.

### Build System
- Use a simple batch file or shell script — not CMake, Meson, or any build system generator:
  ```bat
  @echo off
  cl -nologo -Zi -FC -Od  win32_main.cpp -link -subsystem:windows user32.lib gdi32.lib
  ```
- Single compilation unit: `win32_main.cpp` includes everything via `#include`.
- Compile in debug mode (`-Od`, `-Zi`) during development. Optimize (`-O2`) only for release/profiling.

### Platform Layer Pattern
- The platform layer (e.g., `win32_main.cpp`) handles: window creation, input, audio buffer management,
  file I/O, and timing. It fills a struct (e.g., `GameInput`, `GameMemory`) and calls a single entry
  point into the game/application code.
- The application code **never** calls OS functions directly. It only sees its own types and its own memory.
- This makes hot-reloading the application as a DLL trivial — the platform layer loads the DLL, calls
  `game_update_and_render()`, and can reload it on the fly during development.

### What NOT to Do
- Do NOT use `std::vector`, `std::string`, `std::map`, `std::unique_ptr`, or anything from `<algorithm>`.
- Do NOT use class inheritance or virtual functions.
- Do NOT use exceptions or RTTI.
- Do NOT use `auto` (be explicit about types).
- Do NOT use range-based for loops over STL containers.
- Do NOT design "reusable" code before you have at least two concrete uses for it.
- Do NOT create header files full of forward declarations and "interfaces" — if the code is in a
  single compilation unit, you just order your functions so callees come before callers.
- Do NOT use `const` on local variables obsessively. Casey uses `const` sparingly and pragmatically.
- Do NOT use namespaces. Prefix your types/functions if you need disambiguation.

---

## Example: The Compression Workflow

When the user asks you to build something, follow this mental model:

1. **Write it inline first.** Put the code right where it's needed, as the most specific, concrete version of
   what you want to happen. No functions, no structs beyond what exists — just straight-line code.

2. **See a pattern twice? Compress.** When you find yourself writing the same sequence a second time, extract
   it into a function. The function signature comes from the actual code you wrote — not from imagining what
   you might need someday.

3. **Group related state into a struct when it travels together.** If the same three variables keep getting
   passed to the same functions, bundle them. Casey calls this a "shared stack frame."

4. **Keep granularity options open.** Never provide only a high-level function without also keeping the
   lower-level building blocks available. The user should be able to trivially replace any high-level call
   with the few lower-level calls that compose it.

5. **Evaluate by total cost.** The right abstraction is the one that minimizes total human effort over the
   code's lifetime — writing, debugging, modifying, and reading. Not the one that looks "cleanest" or
   satisfies a design principle.

---

## Example Snippet: A Simple Entity System

```cpp
// Types
enum EntityType {
    entity_type_none,
    entity_type_hero,
    entity_type_wall,
    entity_type_familiar,
    entity_type_sword,
};

struct Entity {
    EntityType type;
    V2 p;
    V2 dp;
    f32 width;
    f32 height;
    u32 facing_direction;
    f32 t_bob;
    i32 hit_point_max;
    i32 hit_points;
    bool collides;
};

struct GameState {
    MemoryArena world_arena;
    u32 entity_count;
    Entity entities[4096];
    u32 camera_following_entity_index;
};

// Functions
internal Entity *
get_entity(GameState *state, u32 index) {
    Entity *result = 0;
    if((index > 0) && (index < state->entity_count)) {
        result = state->entities + index;
    }
    return(result);
}

internal u32
add_entity(GameState *state, EntityType type, V2 p) {
    assert(state->entity_count < array_count(state->entities));
    u32 index = state->entity_count++;
    Entity *entity = state->entities + index;
    *entity = {};
    entity->type = type;
    entity->p = p;
    return(index);
}

internal void
move_entity(GameState *state, u32 entity_index, V2 ddp, f32 dt) {
    Entity *entity = get_entity(state, entity_index);
    if(entity) {
        // Acceleration: ddp is acceleration, dp is velocity, p is position
        V2 old_player_p = entity->p;
        entity->dp = entity->dp + dt * ddp;
        entity->p = entity->p + dt * entity->dp;

        // Collision detection would go here — straightforward, inline
    }
}
```

Notice: no inheritance, no virtual methods, no `new`, no getters/setters — just data and functions that
operate on it.

---

For more detailed guidance, consult the reference files:
- `references/philosophy.md` — Casey's reasoning: why OOP fails, why compression works, performance culture
- `references/code-conventions.md` — Detailed formatting, brace style, include structure, debug macros
