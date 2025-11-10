# API Mapping: Old Monolithic → New Modular System

## Overview

This document maps the old monolithic API (from `src/output.c` and `src/render.c`) to the new modular API (from `src/output/` and `src/renderer/`).

---

## Output Management Functions

### Output Lifecycle

| Old API (output.c) | New API (src/output/) | Notes |
|-------------------|----------------------|-------|
| `output_create()` | `output_create()` | ✅ Same signature, use new implementation |
| `output_destroy()` | `output_destroy()` | ✅ Same signature, use new implementation |
| `output_create_egl_surface()` | `output_create_egl_surface()` | ✅ Same signature, use new implementation |
| `output_configure_compositor_surface()` | `output_configure_compositor_surface()` | ✅ Same signature, use new implementation |

### Wallpaper Management

| Old API (output.c) | New API (src/output/) | Notes |
|-------------------|----------------------|-------|
| `output_set_wallpaper()` | `output_set_wallpaper()` | ✅ Same signature |
| `output_set_shader()` | `output_set_shader()` | ✅ Same signature |
| `output_cycle_wallpaper()` | `output_cycle_wallpaper()` | ✅ Same signature |
| `output_should_cycle()` | `output_should_cycle()` | ✅ Same signature |
| `output_preload_next_wallpaper()` | `output_preload_next_wallpaper()` | ✅ Same signature |

### Configuration

| Old API (output.c) | New API (src/output/) | Notes |
|-------------------|----------------------|-------|
| `output_apply_config()` | `output_apply_config()` | ✅ Same signature |
| `output_apply_deferred_config()` | `output_apply_deferred_config()` | ✅ Same signature |

### Utility

| Old API (output.c) | New API (src/output/) | Notes |
|-------------------|----------------------|-------|
| `output_get_identifier()` | `output_get_identifier()` | ✅ Same signature |
| `output_find_by_name()` | Use linked list traversal | ⚠️ Need to implement if missing |
| `output_find_by_model()` | Use linked list traversal | ⚠️ Need to implement if missing |
| `output_get_count()` | Use linked list traversal | ⚠️ Need to implement if missing |
| `output_foreach()` | Use linked list traversal | ⚠️ Need to implement if missing |

---

## Rendering Functions

### Renderer Lifecycle

| Old API (render.c) | New API (src/renderer/) | Migration Strategy |
|-------------------|------------------------|-------------------|
| `render_init_output()` | `renderer_create()` | Create renderer context for output |
| `render_cleanup_output()` | `renderer_destroy()` | Destroy renderer context |

**Migration Example:**
```c
// OLD:
bool render_init_output(struct output_state *output);

// NEW:
renderer_config_t config = {
    .egl_display = state->egl_display,
    .egl_context = state->egl_context,
    .gl_version = RENDERER_GLES_2_0,
    .enable_debug = true
};
renderer_context_t *ctx = renderer_create(&config);
```

### Frame Rendering

| Old API (render.c) | New API (src/renderer/) | Migration Strategy |
|-------------------|------------------------|-------------------|
| `render_frame()` | `renderer_begin_frame()` + `renderer_render_frame()` + `renderer_end_frame()` | Three-call pattern |
| `render_frame_shader()` | Use `renderer_render_frame()` with shader config | Set type to `RENDERER_CONTENT_SHADER` |
| `render_frame_transition()` | `transition_render()` | Use new transitions API |

**Migration Example:**
```c
// OLD:
render_frame(output);

// NEW:
renderer_begin_frame(ctx, output->compositor_surface->egl_surface, 
                     output->width, output->height);

renderer_frame_config_t frame_config = {
    .type = RENDERER_CONTENT_IMAGE,
    .image_path = output->config->path,
    .mode = output->config->mode
};
renderer_render_frame(ctx, &frame_config);

renderer_end_frame(ctx);
```

**Transition Example:**
```c
// OLD:
render_frame_transition(output, progress);

// NEW:
transition_params_t params = {
    .prev_texture = output->prev_texture,
    .current_texture = output->current_texture,
    .width = output->width,
    .height = output->height,
    .progress = progress,
    .frame_count = output->frame_count
};
transition_render(&params, TRANSITION_FADE);
```

### Texture Management

| Old API (render.c) | New API (src/renderer/) | Migration Strategy |
|-------------------|------------------------|-------------------|
| `render_create_texture()` | `renderer_texture_from_memory()` | Pass image data |
| `render_create_texture_flipped()` | `renderer_texture_from_memory()` | Handle flipping in image loading |
| `render_destroy_texture()` | `renderer_texture_destroy()` | Pass texture handle |
| `render_load_channel_textures()` | `renderer_texture_from_file()` | Load each channel separately |
| `render_update_channel_texture()` | `renderer_texture_from_file()` | Reload channel texture |

**Migration Example:**
```c
// OLD:
GLuint texture = render_create_texture(image_data);
render_destroy_texture(texture);

// NEW:
renderer_texture_t *tex = renderer_texture_from_memory(
    ctx, image_data->pixels, image_data->width, image_data->height);
renderer_texture_destroy(tex);
```

### Shader Management

| Old API (render.c) | New API (src/renderer/) | Migration Strategy |
|-------------------|------------------------|-------------------|
| `shader_create_program()` | `renderer_shader_compile_file()` | ⚠️ TODO: Implement in renderer |
| `shader_destroy_program()` | `renderer_shader_destroy_program()` | ⚠️ TODO: Implement in renderer |

### Internal Helpers (Not Migrated)

These were internal to render.c and should NOT be used externally:

- `draw_char_at()` - Internal FPS rendering
- `render_fps_watermark()` - Will be moved to renderer
- `cache_program_uniforms()` - Internal GL state caching
- `cache_transition_uniforms()` - Internal GL state caching
- `use_program_cached()` - Internal GL state caching
- `bind_texture_cached()` - Internal GL state caching
- `set_blend_state()` - Internal GL state caching
- `calculate_vertex_coords_for_image()` - Internal vertex calculation
- `calculate_vertex_coords()` - Internal vertex calculation

---

## struct output_state Changes

### Old Structure (include/neowall.h, ~210 lines)

Contains everything: Wayland, EGL, textures, shaders, uniforms, caching, FPS...

### New Structure (src/output/output.h, ~80 lines)

Separated concerns:
- **Output state**: Wayland objects, geometry, config, texture IDs only
- **Renderer context**: Managed separately via `renderer_context_t*`

### Fields Mapping

| Old Field | New Location | Notes |
|-----------|--------------|-------|
| `output`, `xdg_output`, `compositor_surface` | ✅ Stays in `output_state` | Output management |
| `name`, `width`, `height`, `scale`, `transform` | ✅ Stays in `output_state` | Geometry |
| `make`, `model`, `connector_name` | ✅ Stays in `output_state` | Output info |
| `configured`, `needs_redraw` | ✅ Stays in `output_state` | State flags |
| `state` (back-pointer) | ✅ Stays in `output_state` | Global state ref |
| `config`, `config_slots`, `active_slot` | ✅ Stays in `output_state` | Configuration |
| `current_image`, `next_image` | ❌ Removed | Managed by renderer/loader |
| `texture`, `next_texture`, `preload_texture` | ✅ Simplified to `current_texture`, `prev_texture` | Texture IDs only |
| `preload_*` fields | ⚠️ Move to background loader module | Async loading |
| `channel_textures`, `channel_count` | ⚠️ Move to shader manager | iChannel management |
| `program`, `glitch_program`, `pixelate_program`, etc. | ❌ Removed | Managed by renderer internally |
| `vbo` | ❌ Removed | Renderer uses client-side arrays |
| `shader_uniforms`, `program_uniforms`, `transition_uniforms` | ❌ Removed | Internal to renderer |
| `gl_state` cache | ❌ Removed | Internal to renderer |
| `last_frame_time`, `frames_rendered` | ✅ Stays in `output_state` | Frame timing |
| `last_cycle_time` | ✅ Stays in `output_state` | Cycle timing |
| `transition_start_time`, `transition_progress` | ✅ Stays in `output_state` | Transition state |
| `shader_start_time`, `shader_fade_start_time` | ⚠️ Move to shader manager | Shader timing |
| `pending_shader_path`, `shader_load_failed` | ⚠️ Move to shader manager | Shader state |
| `fps_*` fields | ✅ Stays in `output_state` | FPS measurement |
| `next` (linked list) | ✅ Stays in `output_state` | List linkage |

---

## Migration Checklist

### Phase 1: Update output_state Definition
- [ ] Update `include/neowall.h` to use new simplified `output_state`
- [ ] Add `renderer_context_t *renderer_ctx` field to output_state
- [ ] Add transition texture fields: `current_texture`, `prev_texture`

### Phase 2: Update Call Sites - Output Functions
- [ ] `src/compositor/backends/wayland/wayland_core.c` - `output_create()`
- [ ] `src/config.c` - `output_apply_config()`, `render_init_output()`
- [ ] `src/egl/egl_core.c` - `output_create_egl_surface()`, `render_init_output()`
- [ ] All GLES implementations - delegate to new renderer

### Phase 3: Update Call Sites - Render Functions
- [ ] `src/eventloop.c` - `render_frame()` → use new 3-call pattern
- [ ] `src/output.c` - texture loading → use `renderer_texture_*`
- [ ] `src/output.c` - shader loading → use renderer shader API

### Phase 4: Update Call Sites - Transitions
- [ ] `src/eventloop.c` - `render_frame_transition()` → `transition_render()`
- [ ] Use `transition_params_t` instead of passing output_state

### Phase 5: Remove Old Files
- [ ] Delete `src/output.c` (old monolithic version)
- [ ] Delete `src/render.c` (old monolithic version)
- [ ] Update Makefile to remove old files
- [ ] Update Makefile to add new modular files

### Phase 6: Testing
- [ ] Compile and link successfully
- [ ] Test single monitor rendering
- [ ] Test multi-monitor rendering
- [ ] Test all transitions (fade, slide, glitch, pixelate)
- [ ] Test shader wallpapers
- [ ] Test image wallpapers
- [ ] Test wallpaper cycling
- [ ] Test hot-reload
- [ ] Performance benchmarks

---

## Key Architectural Changes

### Before (Monolithic)
```
┌─────────────────────────────────────┐
│   Massive output_state struct       │
│   (210 lines, everything mixed)     │
│                                     │
│   output.c + render.c               │
│   (3000+ lines combined)            │
└─────────────────────────────────────┘
```

### After (Modular)
```
┌──────────────────┐    ┌────────────────────┐
│  output_state    │───→│ renderer_context_t │
│  (80 lines)      │    │  (rendering state) │
│                  │    │                    │
│  - Wayland       │    │  - EGL/GL state    │
│  - Geometry      │    │  - Programs        │
│  - Config        │    │  - Textures        │
│  - Texture IDs   │    │  - Uniforms        │
└──────────────────┘    └────────────────────┘
       │                         │
       ↓                         ↓
  src/output/              src/renderer/
  (output mgmt)            (rendering)
```

### Benefits
- ✅ **Separation of concerns**: Output management ≠ Rendering
- ✅ **Testability**: Renderer can be tested independently
- ✅ **Maintainability**: Smaller, focused modules
- ✅ **Reusability**: Renderer can be used in other contexts
- ✅ **Clear dependencies**: output → renderer (not circular)

---

## Notes

- **Be careful**: This is a major refactor affecting core rendering
- **Test incrementally**: Update one call site at a time
- **Keep backups**: Commit after each successful migration step
- **Measure performance**: Ensure no regressions
- **Visual testing**: Rendering bugs are immediately visible to users