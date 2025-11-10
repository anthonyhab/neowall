# NeoWall Output Module

A display output management system for handling multiple monitors, configurations, and per-display wallpaper settings.

## 📁 Directory Structure

```
src/output/
├── output.h            # Output interface
└── output.c            # Output implementation
```

## 🎯 Purpose

The output module manages individual display outputs (monitors) and bridges between:
- **Wayland**: Display discovery and properties
- **Renderer**: Where to draw content
- **Compositor**: How to position wallpaper layers

## 🏗️ Architecture

```
┌─────────────────────────────────────────────┐
│           Application Layer                  │
└─────────────────┬───────────────────────────┘
                  │
        ┌─────────┴─────────┐
        │                   │
┌───────▼───────┐   ┌───────▼───────┐
│ Output Module │   │   Renderer    │
└───────┬───────┘   └───────────────┘
        │
┌───────▼───────┐
│  Compositor   │
│  Abstraction  │
└───────┬───────┘
        │
┌───────▼───────┐
│    Wayland    │
└───────────────┘
```

## 🔧 Core Concepts

### Output State (`struct output_state`)

Represents a single monitor/display with:
- **Wayland Objects**: `wl_output`, `xdg_output`
- **Properties**: Width, height, scale, connector name
- **Configuration**: Wallpaper settings
- **Rendering State**: Frame tracking, redraw flags

### Output Lifecycle

1. **Discovery**: Wayland announces new output
2. **Creation**: `output_create()` allocates state
3. **Configuration**: Geometry and properties received
4. **Compositor Setup**: Surface created for wallpaper layer
5. **EGL Setup**: Rendering surface created
6. **Active**: Ready for rendering
7. **Destruction**: Output removed or app shutdown

## 🚀 Quick Start

### Creating an Output

```c
#include "output/output.h"

/* Wayland output discovered */
struct output_state *output = output_create(
    state,          // Global state
    wl_output,      // Wayland output object
    name            // Output ID
);

/* Configure output */
output_configure_compositor_surface(output);
output_create_egl_surface(output);

/* Apply wallpaper config */
struct wallpaper_config config = { /* ... */ };
output_apply_config(output, &config);
```

### Rendering to Output

```c
#include "output/output.h"
#include "renderer/renderer.h"

/* Check if ready */
if (output_is_ready(output)) {
    /* Render using renderer module */
    renderer_begin_frame(ctx, 
                        output->compositor_surface->egl_surface,
                        output->width,
                        output->height);
    
    renderer_frame_config_t config = {
        .type = RENDERER_TYPE_IMAGE,
        .image_path = output->config->path,
        .mode = output->config->mode,
    };
    renderer_render_frame(ctx, &config);
    renderer_end_frame(ctx);
}
```

### Destroying an Output

```c
/* Cleanup when output removed */
output_destroy(output);
```

## 📊 Key Functions

### Lifecycle

| Function | Description |
|----------|-------------|
| `output_create()` | Create output state |
| `output_destroy()` | Destroy and cleanup |
| `output_configure_compositor_surface()` | Setup compositor surface |
| `output_create_egl_surface()` | Create EGL rendering surface |

### Configuration

| Function | Description |
|----------|-------------|
| `output_apply_config()` | Apply wallpaper configuration |
| `output_apply_deferred_config()` | Apply config when output ready |

### Wallpaper Management

| Function | Description |
|----------|-------------|
| `output_set_wallpaper()` | Set static image wallpaper |
| `output_set_shader()` | Set live shader wallpaper |
| `output_cycle_wallpaper()` | Cycle to next wallpaper |
| `output_should_cycle()` | Check if cycle time reached |
| `output_preload_next_wallpaper()` | Preload next for smooth transition |

### Utilities

| Function | Description |
|----------|-------------|
| `output_get_identifier()` | Get output name (connector preferred) |
| `output_mark_dirty()` | Mark for redraw |
| `output_is_ready()` | Check if ready for rendering |

## 🔄 Integration Points

### With Renderer

Output provides the rendering target:
```c
renderer_begin_frame(ctx, 
                    output->compositor_surface->egl_surface,
                    output->width, 
                    output->height);
```

### With Compositor

Output uses compositor abstraction for surface creation:
```c
output->compositor_surface = compositor_surface_create(
    backend,
    output->wl_output,
    width,
    height,
    layer
);
```

### With Config System

Output applies configuration:
```c
struct wallpaper_config *config = config_load_for_output(name);
output_apply_config(output, config);
```

## 📋 Output Properties

### Identifying Outputs

Outputs can be identified by:
1. **Connector name** (preferred): `HDMI-A-1`, `DP-1`, `eDP-1`
2. **Model name** (fallback): `Samsung S24`, `Dell U2720Q`
3. **Wayland name** (internal): Numeric ID

```c
const char *id = output_get_identifier(output);
// Returns connector_name if available, else model
```

### Output Geometry

```c
struct output_state {
    int32_t width;       // Width in pixels
    int32_t height;      // Height in pixels
    int32_t scale;       // Scale factor (1, 2, etc.)
    int32_t transform;   // Rotation (0, 90, 180, 270)
};
```

### Output Metadata

```c
struct output_state {
    char make[64];           // Manufacturer
    char model[64];          // Model name
    char connector_name[64]; // Connector (HDMI-A-1, etc.)
};
```

## 🎨 Wallpaper Configuration

### Configuration Structure

```c
struct wallpaper_config {
    enum wallpaper_type type;       // IMAGE or SHADER
    char path[MAX_PATH];            // Image/shader path
    enum wallpaper_mode mode;       // Display mode
    float duration;                 // Cycle duration
    enum transition_type transition; // Transition effect
    bool cycle;                     // Enable cycling
    char **cycle_paths;             // Paths to cycle through
    size_t cycle_count;             // Number in cycle
    size_t current_cycle_index;     // Current position
};
```

### Applying Configuration

```c
struct wallpaper_config config = {
    .type = WALLPAPER_IMAGE,
    .path = "/path/to/image.png",
    .mode = MODE_FILL,
    .transition = TRANSITION_FADE,
    .cycle = false,
};

output_apply_config(output, &config);
```

## 🔄 Wallpaper Cycling

### Setup Cycling

```c
struct wallpaper_config config = {
    .cycle = true,
    .duration = 300.0,  // 5 minutes
    .cycle_paths = (char*[]){
        "/path/1.png",
        "/path/2.png",
        "/path/3.png",
        NULL
    },
    .cycle_count = 3,
    .transition = TRANSITION_FADE,
};

output_apply_config(output, &config);
```

### Check and Cycle

```c
uint64_t current_time = get_time_ms();

if (output_should_cycle(output, current_time)) {
    output_cycle_wallpaper(output);
}
```

### Preloading

For smooth transitions, preload next wallpaper:
```c
output_preload_next_wallpaper(output);
// Loads next image in background thread
```

## 🎯 Multi-Monitor Support

### Per-Output Configuration

Each output has independent settings:
```c
// Output 1: Static image
struct wallpaper_config config1 = {
    .type = WALLPAPER_IMAGE,
    .path = "wallpaper1.png",
    .mode = MODE_FILL,
};
output_apply_config(output1, &config1);

// Output 2: Live shader
struct wallpaper_config config2 = {
    .type = WALLPAPER_SHADER,
    .path = "shader.glsl",
    .mode = MODE_FILL,
};
output_apply_config(output2, &config2);
```

### Output Iteration

```c
struct output_state *output = state->outputs;
while (output) {
    if (output_is_ready(output)) {
        // Render to this output
    }
    output = output->next;
}
```

## 🔍 State Management

### Output States

```c
typedef enum {
    OUTPUT_STATE_CREATED,      // Just created
    OUTPUT_STATE_CONFIGURED,   // Geometry received
    OUTPUT_STATE_READY,        // Ready for rendering
    OUTPUT_STATE_DESTROYED,    // Being destroyed
} output_state_t;
```

### State Transitions

```
CREATED → CONFIGURED → READY → DESTROYED
    ↓          ↓          ↓
(waiting)  (waiting)  (active)
```

### Checking Ready State

```c
bool output_is_ready(const struct output_state *output) {
    return output && 
           output->configured && 
           output->compositor_surface != NULL;
}
```

## 🚨 Error Handling

### Graceful Degradation

```c
if (!output_configure_compositor_surface(output)) {
    // Try fallback method
    // Or mark output as failed
}
```

### Validation

```c
if (!output) {
    fprintf(stderr, "Invalid output\n");
    return;
}

if (!output->configured) {
    fprintf(stderr, "Output not configured yet\n");
    return;
}
```

## 📈 Performance Considerations

### Lazy Initialization

- Surfaces created only when needed
- Configuration applied when output ready
- Resources allocated on-demand

### Frame Tracking

```c
struct output_state {
    uint64_t last_frame_time;   // Last render time
    uint64_t frames_rendered;   // Total frames
    bool needs_redraw;          // Redraw flag
};
```

### Dirty Tracking

```c
// Mark for redraw
output_mark_dirty(output);

// Check before rendering
if (output->needs_redraw) {
    // Render frame
    output->needs_redraw = false;
}
```

## 🔮 Future Enhancements

### Planned Features
- [ ] Output hotplug support
- [ ] Dynamic resolution changes
- [ ] Output mirroring
- [ ] Output rotation
- [ ] Per-output FPS limits
- [ ] Background preloading pool

### Optimizations
- [ ] Async surface creation
- [ ] Output state caching
- [ ] Smart redraw scheduling
- [ ] Memory-mapped configuration

## 📝 Example: Complete Output Setup

```c
#include "output/output.h"
#include "renderer/renderer.h"
#include "compositor/compositor.h"

/* 1. Create output when Wayland announces it */
struct output_state *output = output_create(state, wl_output, name);

/* 2. Wait for geometry events */
// Wayland will call output geometry/mode handlers
// which set width, height, scale, etc.

/* 3. Once geometry known, configure */
if (output->width > 0 && output->height > 0) {
    output_configure_compositor_surface(output);
    output_create_egl_surface(output);
}

/* 4. Apply wallpaper configuration */
struct wallpaper_config config = {
    .type = WALLPAPER_IMAGE,
    .path = "wallpaper.png",
    .mode = MODE_FILL,
};
output_apply_config(output, &config);

/* 5. Main render loop */
while (running) {
    if (output_is_ready(output) && output->needs_redraw) {
        renderer_begin_frame(ctx, 
                           output->compositor_surface->egl_surface,
                           output->width,
                           output->height);
        
        renderer_frame_config_t frame_config = {
            .type = RENDERER_TYPE_IMAGE,
            .image_path = output->config->path,
            .mode = output->config->mode,
        };
        renderer_render_frame(ctx, &frame_config);
        renderer_end_frame(ctx);
        
        output->needs_redraw = false;
    }
}

/* 6. Cleanup */
output_destroy(output);
```

## 📄 License

Part of NeoWall project - MIT License

---

**Managing displays, one output at a time** 🖥️