# Vulkan + Multi-Core by Default: Refactor Plan

## Codebase Assessment

### What Exists

```
src/
├── base/
│   ├── typedef.h          ✅ Base types, internal/global_variable macros
│   ├── core.h             ✅ Compiler/OS detection, assert, overflow checks
│   ├── log.h              ✅ Logging with levels and colors
│   ├── memory.h           ✅ Virtual memory: reserve/commit/release (cross-platform)
│   ├── arena.h            ✅ Arena allocator (virtual memory backed, auto-commit)
│   └── string.h           ✅ Arena-backed string type
├── game_api.h             ✅ Game↔Platform interface (GameMemory, GameInput)
├── game.cpp               ✅ Hot-reloadable game DLL (libgame.so)
├── main.cpp               ✅ Platform layer: GLFW window, input, game loop
└── renderer/
    ├── vulkan.h           ⚠️  Stub (init, cleanup, draw forward decls)
    └── vulkan.cpp         ⚠️  Partial (instance, device, surface, queue — no swapchain,
                               no command buffers, no pipeline, draw() is empty)
```

### What's Good (Keep As-Is)

- **base/ layer** — The arena, types, logging, and memory systems are solid.
  The arena already does virtual reserve + commit-on-demand with alignment,
  temporary memory scopes, etc. This is better than typical Handmade Hero arenas.
- **Hot-reload split** — `game.cpp` builds as a shared library, `main.cpp` loads
  it via `dlopen`/`dlsym`. The `GameMemory`/`GameInput` interface is clean.
- **Naming conventions** — Already Rust-like snake_case with PascalCase types.
- **clang-format** — Already configured with K&R-ish style (return type on own line).

### What's Missing (Everything We Need to Build)

1. **No threading at all** — Single-threaded game loop in `main.cpp`.
2. **No push buffer / render commands** — Game has no way to tell the renderer what to draw.
3. **No swapchain** — Vulkan init stops after device creation. No images to render to.
4. **No command buffers** — No VkCommandPool, no VkCommandBuffer.
5. **No pipeline** — No shaders, no VkPipeline, no VkPipelineLayout.
6. **No depth buffer, no synchronization objects** (semaphores, fences).
7. **No texture system**.

### What Changes

- `main.cpp` — Refactored to launch N threads. GLFW stays on main thread
  (GLFW requires main-thread event polling). Main thread becomes the "platform
  coordinator" that gathers input, signals frame start, waits for frame end.
- `game_api.h` — Expanded with `RenderCommands` in `GameMemory`. The game DLL
  pushes render entries. New lane context passed to the game.
- `game.cpp` — Game logic becomes lane-aware. Entity update + render command
  generation distributed across lanes.
- `renderer/vulkan.cpp` — Massively expanded: swapchain, per-lane command pools,
  dynamic rendering pipeline, command recording (wide), submit+present (narrow).

---

## Target File Structure

```
src/
├── base/
│   ├── typedef.h              (unchanged)
│   ├── core.h                 (unchanged)
│   ├── log.h                  (unchanged)
│   ├── memory.h               (unchanged)
│   ├── arena.h                (unchanged)
│   ├── string.h               (unchanged)
│   └── lane.h                 (NEW — lane primitives)
│
├── game/
│   ├── game_api.h             (EXPANDED — add RenderCommands, LaneContext)
│   ├── game.cpp               (REWRITTEN — lane-aware game frame)
│   ├── game_render_group.h    (NEW — push buffer types + API)
│   └── game_math.h            (NEW — V2, V3, V4, operators)
│
├── renderer/
│   ├── vulkan.h               (EXPANDED — VulkanState with all handles)
│   ├── vulkan_init.cpp        (RENAMED from vulkan.cpp — init + cleanup)
│   ├── vulkan_swapchain.cpp   (NEW — swapchain create/recreate/cleanup)
│   ├── vulkan_pipeline.cpp    (NEW — pipeline + shader module creation)
│   ├── vulkan_commands.cpp    (NEW — per-lane recording + submit+present)
│   └── vulkan_textures.cpp    (NEW — bindless texture array, staging)
│
├── shaders/
│   ├── sprite.vert            (NEW)
│   └── sprite.frag            (NEW)
│
├── main.cpp                   (REWRITTEN — multi-threaded frame loop)
│
└── build/
    ├── linux.sh               (UPDATED — shader compilation, -lpthread)
    └── macos.sh               (UPDATED — same)
```

All renderer `.cpp` files are `#include`d into `main.cpp` (unity build, as you
already do with `#include "renderer/vulkan.cpp"`). The game DLL remains separate.

---

## Step 1: Lane Primitives (`base/lane.h`)

This is the foundation. Everything else builds on it. Implement first, test with
a trivial parallel sum before touching anything else.

```cpp
// base/lane.h
#pragma once

#include "base/typedef.h"
#include "base/core.h"

#include <cstdatomic>  // or use compiler intrinsics

#define MAX_LANES 64

struct LaneBarrier {
    volatile s64 count;
    volatile s64 generation;
    s64 thread_count;
};

struct LaneContext {
    u32 lane_idx;
    u32 lane_count;
    LaneBarrier *barrier;
    // Shared buffer for lane_sync_u64 broadcasts
    u64 shared_broadcast;
};

// Thread-local storage — set once per thread at startup
#if COMPILER_MSVC
#define thread_local __declspec(thread)
#else
#define thread_local __thread
#endif

global_variable thread_local LaneContext *tl_lane_ctx = nullptr;

internal void
set_lane_context(LaneContext *ctx) {
    tl_lane_ctx = ctx;
}

internal u32
lane_idx(void) {
    return tl_lane_ctx->lane_idx;
}

internal u32
lane_count(void) {
    return tl_lane_ctx->lane_count;
}

internal void
lane_sync(void) {
    LaneBarrier *b = tl_lane_ctx->barrier;
    if(b->thread_count <= 1) return; // single-lane: no-op

    s64 gen = b->generation;
    // Use __sync builtins for C++11 compat
    s64 arrived = __sync_add_and_fetch(&b->count, 1);
    if(arrived == b->thread_count) {
        __sync_lock_test_and_set(&b->count, 0);
        __sync_add_and_fetch(&b->generation, 1);
    } else {
        while(b->generation == gen) {
            // spin — could add _mm_pause() or sched_yield()
#if COMPILER_CLANG || COMPILER_GCC
            __builtin_ia32_pause();
#endif
        }
    }
}

struct LaneRange {
    u64 min;
    u64 max;
};

internal LaneRange
lane_range(u64 count) {
    u64 idx = (u64)lane_idx();
    u64 total = (u64)lane_count();
    u64 per_lane = count / total;
    u64 leftover = count % total;
    b32 has_leftover = (idx < leftover);
    u64 leftovers_before = has_leftover ? idx : leftover;
    u64 first = per_lane * idx + leftovers_before;
    u64 opl = first + per_lane + (has_leftover ? 1 : 0);
    LaneRange result = {first, opl};
    return result;
}

// Broadcast a u64 from source_lane to all other lanes
internal void
lane_sync_u64(u64 *value, u32 source_lane) {
    LaneBarrier *b = tl_lane_ctx->barrier;
    if(b->thread_count <= 1) return;

    // Source writes to shared
    if(lane_idx() == source_lane) {
        tl_lane_ctx->shared_broadcast = *value;
    }
    lane_sync();

    // Others read from shared
    if(lane_idx() != source_lane) {
        *value = tl_lane_ctx->shared_broadcast;
    }
    lane_sync();
}
```

**Note on C++11 compatibility:** Your build uses `-std=c++11`. The `__sync`
builtins work on clang and GCC. If you bump to C++20 later, replace with
`std::atomic`. The `thread_local` keyword is C++11 standard but the `__thread`
fallback is for older compilers.

---

## Step 2: Game Math (`game/game_math.h`)

You'll need this for entity positions and render entry coordinates. Currently
your GameState just has `f32 player_x, player_y` — this replaces that.

```cpp
// game/game_math.h
#pragma once

#include "base/typedef.h"

union V2 {
    struct { f32 x, y; };
    f32 e[2];
};

union V4 {
    struct { f32 x, y, z, w; };
    struct { f32 r, g, b, a; };
    f32 e[4];
};

inline V2 v2(f32 x, f32 y) { V2 r = {x, y}; return r; }
inline V4 v4(f32 x, f32 y, f32 z, f32 w) { V4 r = {x, y, z, w}; return r; }

inline V2 operator+(V2 a, V2 b) { return v2(a.x + b.x, a.y + b.y); }
inline V2 operator-(V2 a, V2 b) { return v2(a.x - b.x, a.y - b.y); }
inline V2 operator*(f32 s, V2 a) { return v2(s * a.x, s * a.y); }
inline V2 operator*(V2 a, f32 s) { return s * a; }
inline V2 &operator+=(V2 &a, V2 b) { a = a + b; return a; }
```

---

## Step 3: Push Buffer + Render Commands (`game/game_render_group.h`)

This is the API-agnostic render command layer. The game DLL pushes entries here.
The Vulkan backend consumes them. Neither knows about the other.

```cpp
// game/game_render_group.h
#pragma once

#include "base/typedef.h"
#include "base/lane.h"
#include "game/game_math.h"

#define MAX_RENDER_ENTRIES_PER_LANE (16 * 1024)
#define PUSH_BUFFER_SIZE_PER_LANE  (4 * MB)

enum RenderEntryType : u32 {
    render_entry_type_clear,
    render_entry_type_rect,
    render_entry_type_bitmap,
};

struct RenderEntryHeader {
    RenderEntryType type;
    u32 size;       // total size including header
    f32 sort_key;   // for z-sorting
};

struct RenderEntryClear {
    RenderEntryHeader header;
    V4 color;
};

struct RenderEntryRect {
    RenderEntryHeader header;
    V2 p;
    f32 width;
    f32 height;
    V4 color;
};

struct RenderEntryBitmap {
    RenderEntryHeader header;
    V2 p;
    f32 width;
    f32 height;
    V4 color;
    u32 texture_handle;
};

struct LanePushBuffer {
    u8 *base;
    u32 used;
    u32 capacity;
    u32 entry_count;
};

struct RenderCommands {
    LanePushBuffer lane_buffers[MAX_LANES];
    u32 active_lane_count;

    // Screen dimensions (set by platform each frame)
    u32 screen_width;
    u32 screen_height;
};

// Push API — automatically routes to the calling lane's buffer
#define push_render_entry(cmds, type_enum, Type) \
    (Type *)push_render_entry_((cmds), sizeof(Type), (type_enum))

internal void *
push_render_entry_(
    RenderCommands *commands,
    u32 size,
    RenderEntryType type
) {
    u32 idx = lane_idx();
    LanePushBuffer *buf = &commands->lane_buffers[idx];
    assert(
        buf->used + size <= buf->capacity,
        "Push buffer overflow!"
    );

    RenderEntryHeader *header = (RenderEntryHeader *)(buf->base + buf->used);
    header->type = type;
    header->size = size;
    header->sort_key = 0.0f;

    buf->used += size;
    buf->entry_count += 1;
    return header;
}

internal void
push_clear(RenderCommands *commands, V4 color) {
    RenderEntryClear *entry = push_render_entry(
        commands, render_entry_type_clear, RenderEntryClear
    );
    entry->color = color;
}

internal void
push_rect(
    RenderCommands *commands,
    V2 p, f32 w, f32 h, V4 color, f32 sort_key
) {
    RenderEntryRect *entry = push_render_entry(
        commands, render_entry_type_rect, RenderEntryRect
    );
    entry->p = p;
    entry->width = w;
    entry->height = h;
    entry->color = color;
    entry->header.sort_key = sort_key;
}

internal void
reset_render_commands(RenderCommands *commands) {
    for(u32 i = 0; i < commands->active_lane_count; ++i) {
        commands->lane_buffers[i].used = 0;
        commands->lane_buffers[i].entry_count = 0;
    }
}
```

---

## Step 4: Expand `game_api.h`

This is the interface between platform and game DLL. It needs to carry
the render commands and lane context.

### Current → Target Diff

```
 struct GameMemory {
     b32 is_initialized;
     u64 permanent_storage_size;
     void *permanent_storage;
     u64 transient_storage_size;
     void *transient_storage;
+    RenderCommands *render_commands;  // platform allocates, game writes
 };

+// Lane info passed to game each frame
+struct GameFrameContext {
+    u32 lane_idx;
+    u32 lane_count;
+    void *lane_barrier;  // opaque — game calls lane_sync() which uses TLS
+};

-typedef void GameUpdateAndRender(GameMemory *memory, GameInput *input);
+typedef void GameUpdateAndRender(
+    GameMemory *memory,
+    GameInput *input,
+    GameFrameContext *lane_ctx
+);

 #define GAME_UPDATE_AND_RENDER(name) \
-    export void name(GameMemory *memory, GameInput *input)
+    export void name( \
+        GameMemory *memory, \
+        GameInput *input, \
+        GameFrameContext *lane_ctx \
+    )
```

**Key insight:** The game DLL receives lane info but doesn't need to know about
`LaneBarrier` internals. The platform installs the `LaneContext` into TLS before
calling into the game, so `lane_idx()` / `lane_sync()` just work.

---

## Step 5: Rewrite `game.cpp` (Lane-Aware)

### Current → Target

Current `game.cpp` is 25 lines: a `GameState` with `player_x`/`player_y` and
movement logic. It compiles as a separate DLL. This structure stays — the game
DLL just becomes lane-aware.

```cpp
// game.cpp — hot-reloadable DLL
#include "base/core.h"
#include "base/lane.h"
#include "game/game_math.h"
#include "game/game_render_group.h"
#include "game_api.h"

struct Entity {
    V2 p;
    V2 dp;
    V4 color;
    f32 width;
    f32 height;
};

struct GameState {
    u32 entity_count;
    Entity entities[4096];
    b32 initialized;
};

internal void
init_game_state(GameState *state) {
    state->entity_count = 1;

    // Player entity
    Entity *player = &state->entities[0];
    player->p = v2(400.0f, 300.0f);
    player->dp = v2(0.0f, 0.0f);
    player->color = v4(0.2f, 0.6f, 1.0f, 1.0f);
    player->width = 32.0f;
    player->height = 32.0f;

    state->initialized = true;
}

GAME_UPDATE_AND_RENDER(game_update_and_render) {
    GameState *state = (GameState *)memory->permanent_storage;
    RenderCommands *commands = memory->render_commands;

    // Init (lane 0 only, first frame)
    if(!memory->is_initialized) {
        if(lane_idx() == 0) {
            init_game_state(state);
            memory->is_initialized = true;
        }
        lane_sync();
    }

    //=== Phase 1: Entity simulation (wide) ===
    {
        // Player input — only entity 0 reads input, and only one lane
        // handles it (whichever owns entity 0 in its range)
        LaneRange range = lane_range(state->entity_count);
        for(u64 i = range.min; i < range.max; ++i) {
            Entity *entity = &state->entities[i];

            if(i == 0) {
                // Player movement
                f32 speed = 200.0f;
                V2 accel = v2(0.0f, 0.0f);
                if(input->move_left.ended_down)  accel.x -= 1.0f;
                if(input->move_right.ended_down) accel.x += 1.0f;
                if(input->move_up.ended_down)    accel.y -= 1.0f;
                if(input->move_down.ended_down)  accel.y += 1.0f;
                entity->p += speed * input->dt_for_frame * accel;
            }
        }
    }
    lane_sync();

    //=== Phase 2: Render command generation (wide) ===
    {
        // Lane 0 clears
        if(lane_idx() == 0) {
            push_clear(commands, v4(0.05f, 0.05f, 0.08f, 1.0f));
        }

        // Each lane pushes rects for its slice of entities
        LaneRange range = lane_range(state->entity_count);
        for(u64 i = range.min; i < range.max; ++i) {
            Entity *entity = &state->entities[i];
            push_rect(
                commands,
                entity->p,
                entity->width, entity->height,
                entity->color,
                0.0f  // sort_key
            );
        }
    }
    lane_sync();
}
```

**Observation:** With only 1 entity right now, only one lane does any real work.
That's fine — the architecture is in place, and when you add hundreds of entities,
they automatically distribute across lanes.

---

## Step 6: Vulkan — Complete the Init

Your current `vulkan.cpp` has instance + device + surface + queue. It's missing
swapchain, depth buffer, command pools, sync objects, and pipeline. Here's what
needs to be added to `VulkanState`:

### Expand VulkanState (`renderer/vulkan.h`)

```cpp
#pragma once

#include "base/typedef.h"
#include "base/lane.h"
#include <vulkan/vulkan.h>

struct Arena;
struct GLFWwindow;

#define MAX_SWAPCHAIN_IMAGES 4

struct VulkanState {
    // --- EXISTING (keep) ---
    VkInstance instance;
    VkApplicationInfo app_info;
    VkPhysicalDevice physical_device;
    VkDevice device;
    VkQueue graphics_queue;
    VkSurfaceKHR surface;
    u32 graphics_queue_family_index;
    VkDebugUtilsMessengerEXT debug_messenger;
    b32 dynamic_rendering_supported;
    b32 initialized;

    // --- NEW: Swapchain ---
    VkSwapchainKHR swapchain;
    VkFormat swapchain_format;
    VkExtent2D swapchain_extent;
    u32 swapchain_image_count;
    VkImage swapchain_images[MAX_SWAPCHAIN_IMAGES];
    VkImageView swapchain_views[MAX_SWAPCHAIN_IMAGES];

    // --- NEW: Depth buffer ---
    VkImage depth_image;
    VkDeviceMemory depth_memory;
    VkImageView depth_view;

    // --- NEW: Per-lane command recording ---
    VkCommandPool lane_pools[MAX_LANES];
    VkCommandBuffer lane_cmds[MAX_LANES];  // secondary
    u32 active_lane_count;

    // --- NEW: Primary command buffer (lane 0) ---
    VkCommandPool primary_pool;
    VkCommandBuffer primary_cmd;

    // --- NEW: Frame sync ---
    VkSemaphore image_available_semaphore;
    VkSemaphore render_finished_semaphore;
    VkFence frame_fence;

    // --- NEW: Pipeline ---
    VkPipelineLayout pipeline_layout;
    VkPipeline sprite_pipeline;

    // --- NEW: Descriptor (for future bindless textures) ---
    VkDescriptorSetLayout descriptor_layout;
    VkDescriptorPool descriptor_pool;
    VkDescriptorSet global_descriptor_set;
    VkSampler default_sampler;
};

b32 init_vulkan(Arena *arena, GLFWwindow *window, u32 lane_count);
void cleanup_vulkan(void);
void vulkan_record_commands(VulkanState *vk, struct RenderCommands *commands);
void vulkan_submit_and_present(VulkanState *vk);
b32 vulkan_recreate_swapchain(Arena *arena, GLFWwindow *window);
```

### New Files to Create

| File | Contents |
|------|----------|
| `renderer/vulkan_swapchain.cpp` | `create_swapchain()`, `cleanup_swapchain()`, choose format/present mode/extent, create image views, depth buffer |
| `renderer/vulkan_pipeline.cpp` | Load SPIR-V, create shader modules, `VkPipelineLayout` with push constants, `VkPipeline` with dynamic rendering (no render pass) |
| `renderer/vulkan_commands.cpp` | `create_per_lane_command_pools()`, `vulkan_record_commands()` (wide), `vulkan_submit_and_present()` (narrow lane 0) |
| `renderer/vulkan_textures.cpp` | Placeholder for now. Bindless descriptor array + staging buffer. Implement when you need sprites. |

### Critical: Per-Lane Command Pools

This is the Vulkan multi-threading win. In `create_per_lane_command_pools()`:

```cpp
internal b32
create_per_lane_command_pools(VulkanState *vk, u32 lane_count) {
    vk->active_lane_count = lane_count;

    for(u32 i = 0; i < lane_count; ++i) {
        VkCommandPoolCreateInfo pool_info = {};
        pool_info.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
        pool_info.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
        pool_info.queueFamilyIndex = vk->graphics_queue_family_index;

        if(vkCreateCommandPool(vk->device, &pool_info, nullptr, &vk->lane_pools[i])
           != VK_SUCCESS) {
            return false;
        }

        VkCommandBufferAllocateInfo alloc_info = {};
        alloc_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
        alloc_info.commandPool = vk->lane_pools[i];
        alloc_info.level = VK_COMMAND_BUFFER_LEVEL_SECONDARY;
        alloc_info.commandBufferCount = 1;

        if(vkAllocateCommandBuffers(vk->device, &alloc_info, &vk->lane_cmds[i])
           != VK_SUCCESS) {
            return false;
        }
    }

    return true;
}
```

Each lane resets and records into its own pool — zero contention.

---

## Step 7: Rewrite `main.cpp` (Multi-Threaded Frame Loop)

This is the biggest change. Current `main.cpp` is a single-threaded loop:
`glfwPollEvents → update_input → game_update_and_render → draw()`.

The new structure:

```
Main thread (thread 0):
  - GLFW event polling (GLFW requires main thread)
  - Input gathering
  - Signal frame_start to all game threads
  - Wait for frame_end
  - (Main thread also participates as lane 0)

Game threads (threads 1..N-1):
  - Wait for frame_start
  - Run game_update_and_render (wide)
  - Record Vulkan commands (wide — each lane uses its own VkCommandPool)
  - Lane 0: submit + present (narrow)
  - Signal frame_end
```

### Key Architecture Decision

GLFW requires `glfwPollEvents()` on the main thread. Two options:

**(A)** Main thread is the coordinator only, game threads are separate.
**(B)** Main thread participates as lane 0, also does GLFW.

**Choice: (B)**. The main thread does GLFW event polling, then joins the other
lanes as lane 0 for the game frame. This avoids wasting a core on just event polling.

```
Frame timeline:
  Main thread: [PollEvents + Input] → [barrier] → [Game frame as lane 0] → [Submit+Present] → [barrier]
  Thread 1:    [...waiting........] → [barrier] → [Game frame as lane 1] → [..............] → [barrier]
  Thread 2:    [...waiting........] → [barrier] → [Game frame as lane 2] → [..............] → [barrier]
```

### Thread Launch Structure

```cpp
struct ThreadParams {
    LaneContext lane_ctx;
    GameMemory *game_memory;
    GameInput *input;               // pointer to shared input (written by main before barrier)
    LaneBarrier *frame_start;       // main signals this after input is ready
    LaneBarrier *frame_end;         // all lanes signal this after frame is done
    VulkanState *vk;
    b32 *running;                   // set to false to exit
};

// Worker thread entry (lanes 1..N-1)
internal void *
game_worker_thread(void *raw_params) {
    ThreadParams *p = (ThreadParams *)raw_params;
    set_lane_context(&p->lane_ctx);

    while(*p->running) {
        // Wait for main thread to finish input gathering
        barrier_sync(p->frame_start);

        // Reset this lane's push buffer
        RenderCommands *cmds = p->game_memory->render_commands;
        cmds->lane_buffers[lane_idx()].used = 0;
        cmds->lane_buffers[lane_idx()].entry_count = 0;

        // Run game frame (wide)
        p->game_code->update_and_render(p->game_memory, p->input, ...);

        // Record Vulkan commands (wide)
        vulkan_record_commands(p->vk, cmds);

        // Signal frame complete
        barrier_sync(p->frame_end);
    }

    return nullptr;
}
```

Main thread does the same work but also handles GLFW and lane-0-only duties
(submit+present) between the two barriers.

---

## Step 8: Build Script Changes

### `build/linux.sh` — Changes Needed

```diff
 COMMON_FLAGS=(
-  -std=c++11
+  -std=c++11    # keep — lane primitives use __sync builtins, not C++20 atomics
   -Wall
   -Wextra
   -Werror
   -Wno-unused-function
+  -Wno-missing-field-initializers   # Vulkan designated init warnings
   -I"$ROOT_DIR/src"
 )

+# Shader compilation
+SHADER_DIR="$ROOT_DIR/src/shaders"
+SHADER_OUT="$BIN_DIR/shaders"
+mkdir -p "$SHADER_OUT"
+glslangValidator -V "$SHADER_DIR/sprite.vert" -o "$SHADER_OUT/sprite.vert.spv"
+glslangValidator -V "$SHADER_DIR/sprite.frag" -o "$SHADER_OUT/sprite.frag.spv"

 # Platform executable now links pthread
 "$CXX" \
   "${COMMON_FLAGS[@]}" \
   "${MODE_FLAGS[@]}" \
   "${GLFW_CFLAGS[@]}" \
   "${VULKAN_CFLAGS[@]}" \
   "$ROOT_DIR/src/main.cpp" \
   "${GLFW_LIBS[@]}" \
   "${VULKAN_LIBS[@]}" \
-  -ldl \
+  -ldl -lpthread \
   -o "$BIN_DIR/main"

-# Game DLL stays the same (no Vulkan, no GLFW, no threads)
+# Game DLL — now includes lane.h and render_group.h but still no Vulkan/GLFW
 "$CXX" \
   "${COMMON_FLAGS[@]}" \
   "${MODE_FLAGS[@]}" \
   -shared \
   -fPIC \
-  "$ROOT_DIR/src/game.cpp" \
+  "$ROOT_DIR/src/game/game.cpp" \
   -o "$BIN_DIR/libgame.so"
```

**The game DLL still has NO Vulkan dependency.** It only includes base/ types,
lane primitives, math types, and the render group header. Platform separation
is maintained.

---

## Step 9: Shaders

### `src/shaders/sprite.vert`

Generates a fullscreen quad from push constants. No vertex buffer needed.

```glsl
#version 450

layout(push_constant) uniform PushConstants {
    vec2 position;     // screen-space position
    vec2 size;         // width, height in pixels
    vec4 color;
    uint texture_index;
} pc;

layout(location = 0) out vec2 out_uv;
layout(location = 1) out vec4 out_color;

void main() {
    // Generate quad vertices from gl_VertexIndex (0-5, two triangles)
    vec2 uv = vec2((gl_VertexIndex << 1) & 2, gl_VertexIndex & 2);
    vec2 pos = pc.position + uv * pc.size;

    // Convert from pixel coords to clip space (-1..1)
    // Assumes screen dimensions are passed via a UBO or specialization constant
    // For now, hardcode or pass via push constant
    out_uv = uv;
    out_color = pc.color;
    gl_Position = vec4(pos, 0.0, 1.0);
}
```

### `src/shaders/sprite.frag`

```glsl
#version 450

layout(location = 0) in vec2 in_uv;
layout(location = 1) in vec4 in_color;

layout(location = 0) out vec4 out_color;

void main() {
    // Flat color for now — texture sampling added in Step 10
    out_color = in_color;
}
```

---

## Implementation Order

| Step | What | Depends On | Tests |
|------|------|------------|-------|
| **1** | `base/lane.h` | Nothing | Parallel sum with lane_count=1 and lane_count=N produce same result |
| **2** | `game/game_math.h` | Nothing | V2 operators: `v2(1,2) + v2(3,4) == v2(4,6)` |
| **3** | `game/game_render_group.h` | lane.h, math | Push 100 rects from 4 lanes, verify all present in buffers |
| **4** | Expand `game_api.h` | render_group.h | Compiles clean |
| **5** | Rewrite `game/game.cpp` | Steps 1-4 | Game DLL loads, single-lane mode produces same movement as before |
| **6** | Vulkan swapchain + depth + sync | Existing vulkan init | Triangle on screen (hardcoded, no push buffer yet) |
| **7** | Vulkan pipeline + shaders | Step 6 | Colored rect renders |
| **8** | Per-lane command pools | Step 6 | N pools created, N secondaries allocated |
| **9** | `vulkan_commands.cpp` (wide record + narrow submit) | Steps 6-8 | Push buffer rect renders through Vulkan |
| **10** | Multi-threaded `main.cpp` | All above | Same frame renders with 1 lane and N lanes |
| **11** | Texture system (bindless) | Steps 6-9 | Load a sprite, render with texture_index |
| **12** | Profile + tune | Everything | Measure barrier wait times, look for imbalance |

### What to Build First to See Pixels

If you want to see something on screen quickly: do **Steps 6-7** next (swapchain +
pipeline). You can test with a hardcoded triangle/rect in `draw()` before wiring
up the push buffer. Then wire push buffer + multi-threading incrementally.

If you want to build the "correct" architecture first and see pixels later: follow
the table order (1 → 2 → 3 → ... → 10).

---

## Notes

### Hot Reload Still Works

The game DLL boundary is unchanged. The platform installs lane context into TLS
before calling `game_update_and_render`, and the game DLL's `lane_idx()` reads
from TLS. When the DLL is reloaded, the new code gets the same TLS values.
Vulkan handles live in the platform layer, not the DLL. Nothing breaks.

### GLFW Stays

Your use of GLFW is fine. It handles window creation and Vulkan surface creation
cross-platform. The only constraint is `glfwPollEvents()` must run on the main
thread. Our architecture handles this — main thread polls, then joins as lane 0.

### VK_KHR_swapchain Extension Missing

Your current `create_device()` doesn't enable `VK_KHR_swapchain`. You'll need to
add it to the device extensions list. This is required for any rendering.

### Single-Lane Fallback

Everything works with `lane_count = 1`. Barriers become no-ops, lane_range gives
the full range, the single secondary command buffer gets all entries. You can
develop the entire Vulkan pipeline single-threaded and flip the switch later.
