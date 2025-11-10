# NeoWall Renderer Module

A modular, high-performance rendering system for GPU-accelerated wallpapers with support for images, live shaders, and transitions.

## 📁 Directory Structure

```
src/renderer/
├── renderer.h          # Main renderer interface (public + internal)
├── renderer.c          # Core renderer implementation
├── textures/           # Texture generation subsystem
│   ├── textures.h      # Texture interface
│   ├── textures.c      # Texture management
│   ├── rgba_noise.c    # RGBA noise generator
│   ├── gray_noise.c    # Grayscale noise generator
│   ├── blue_noise.c    # Blue noise generator
│   ├── wood.c          # Wood grain generator
│   └── abstract.c      # Abstract pattern generator
└── transitions/        # Transition effects subsystem
    ├── transitions.h   # Transition interface
    ├── transitions.c   # Transition registry & dispatch
    ├── fade.c          # Fade transition
    ├── slide.c         # Slide transitions
    ├── glitch.c        # Glitch effect
    └── pixelate.c      # Pixelate effect
```

## 🎯 Design Principles

### Decoupled Architecture
- **Renderer**: Pure rendering primitives (textures, shaders, frames)
- **Output**: Display management (separate module in `/src/output`)
- **Compositor**: Wayland compositor abstraction (separate module)

### Clean Interface
- Single header file (`renderer.h`) with all types and functions
- No hidden internal headers
- Explicit function contracts
- Opaque handles where appropriate

### Modular Subsystems
- **Textures**: Self-contained procedural generation
- **Transitions**: Pluggable effect system
- **Shaders**: Independent shader compilation

## 🚀 Quick Start

### Basic Usage

```c
#include "renderer/renderer.h"

/* 1. Create renderer context */
renderer_config_t config = {
    .gl_version = RENDERER_GLES_AUTO,
    .enable_vsync = true,
    .enable_debug = false,
};
renderer_context_t *ctx = renderer_create(&config);

/* 2. Render a frame */
renderer_begin_frame(ctx, egl_surface, 1920, 1080);

renderer_frame_config_t frame_config = {
    .type = RENDERER_TYPE_IMAGE,
    .image_path = "/path/to/wallpaper.png",
    .mode = RENDERER_MODE_FILL,
};
renderer_render_frame(ctx, &frame_config);

renderer_end_frame(ctx);

/* 3. Cleanup */
renderer_destroy(ctx);
```

### Shader Rendering

```c
/* Compile shader */
GLuint shader = renderer_shader_compile_file(ctx, "shader.glsl", 0);

/* Render shader frame */
renderer_frame_config_t config = {
    .type = RENDERER_TYPE_SHADER,
    .shader_program = shader,
    .shader_time = elapsed_time,
    .shader_speed = 1.0f,
    .show_fps = true,
};

renderer_begin_frame(ctx, egl_surface, 1920, 1080);
renderer_render_frame(ctx, &config);
renderer_end_frame(ctx);
```

### Procedural Textures

```c
/* Create procedural texture */
renderer_texture_t *texture = renderer_texture_procedural(ctx, "rgba_noise", 256);

/* Use texture... */

/* Cleanup */
renderer_texture_destroy(texture);
```

## 📊 Core Components

### Renderer Context (`renderer_context_t`)

The main renderer state managing:
- **EGL/OpenGL ES**: Display and context
- **Capabilities**: GL version detection
- **Global Resources**: Shared shaders and textures
- **Statistics**: Memory usage, frame counts

### Frame Rendering

Three-phase rendering model:

1. **Begin**: `renderer_begin_frame(ctx, surface, width, height)`
   - Makes EGL context current
   - Sets viewport
   - Clears screen

2. **Render**: `renderer_render_frame(ctx, config)`
   - Renders image or shader based on config
   - Handles display modes
   - Applies effects

3. **End**: `renderer_end_frame(ctx)`
   - Swaps buffers
   - Finalizes frame

### Texture System

**File-based textures:**
```c
renderer_texture_t *tex = renderer_texture_from_file(ctx, "image.png");
```

**Memory textures:**
```c
renderer_texture_t *tex = renderer_texture_from_memory(ctx, pixels, w, h);
```

**Procedural textures:**
- `rgba_noise` - Multi-channel noise (most common)
- `gray_noise` - High-quality grayscale
- `blue_noise` - Dithering-friendly
- `wood` - Wood grain pattern
- `abstract` - Colorful Voronoi cells

### Display Modes

- `RENDERER_MODE_CENTER` - Center without scaling
- `RENDERER_MODE_STRETCH` - Stretch to fill (may distort)
- `RENDERER_MODE_FIT` - Fit inside (letterbox)
- `RENDERER_MODE_FILL` - Fill and crop (maintains aspect)
- `RENDERER_MODE_TILE` - Tile pattern

## 🔧 Integration with Other Modules

### Output Module (`/src/output`)

Manages individual displays:
```c
#include "output/output.h"
#include "renderer/renderer.h"

struct output_state *output = output_create(...);

/* Render to output */
renderer_begin_frame(ctx, output->egl_surface, output->width, output->height);
/* ... render ... */
renderer_end_frame(ctx);
```

### Compositor Module (`/src/compositor`)

Provides Wayland compositor abstraction:
- Creates EGL surfaces
- Handles layer positioning
- Multi-compositor support

## 🎨 Texture Generation

All procedural textures use **Fractal Brownian Motion (FBM)** for natural-looking patterns.

### RGBA Noise
```c
GLuint tex = texture_create_rgba_noise(256, 256);
```
- 4 independent noise channels
- Tileable
- 4 octaves of detail
- Used by 90% of Shadertoy shaders

### Grayscale Noise
```c
GLuint tex = texture_create_gray_noise(256, 256);
```
- High-quality single-channel
- 6 octaves for detail
- Great for displacement maps

### Blue Noise
```c
GLuint tex = texture_create_blue_noise(256, 256);
```
- Better spatial distribution
- Reduces banding
- Ideal for dithering

### Wood Grain
```c
GLuint tex = texture_create_wood(256, 256);
```
- Radial ring pattern
- Natural brown tones
- Grain detail overlay

### Abstract
```c
GLuint tex = texture_create_abstract(256, 256);
```
- Voronoi-based cells
- Multi-colored patterns
- Artistic backgrounds

## 🎬 Transition System

### Registry Pattern

Transitions are self-contained modules registered at startup:

```c
transition_register(RENDERER_TRANSITION_FADE, "fade", fade_render);
transition_register(RENDERER_TRANSITION_GLITCH, "glitch", glitch_render);
```

### Supported Transitions

- **Fade**: Classic crossfade
- **Slide Left/Right**: Directional slide
- **Glitch**: Digital glitch with RGB separation
- **Pixelate**: Progressive pixelation effect

### Adding New Transitions

1. Create `transitions/my_effect.c`
2. Implement render function
3. Register in `transitions/transitions.c`
4. Add enum to `renderer.h`

## 🔍 Performance Optimizations

### State Caching
- Avoids redundant `glUseProgram()` calls
- Caches uniform locations
- Tracks bound textures

### Memory Management
- Frees CPU pixel data after GPU upload
- Tracks GPU memory usage
- Reuses default textures

### Persistent Resources
- VBOs created once, reused forever
- Shared shader programs
- Global texture cache

## 📈 Statistics & Debugging

### Enable Debug Mode
```c
renderer_config_t config = {
    .enable_debug = true,
};
renderer_context_t *ctx = renderer_create(&config);
```

Outputs:
- OpenGL version info
- Texture creation logs
- Frame rendering details
- Error messages

### Get GL Version
```c
const char *version = renderer_get_gl_version(ctx);
printf("OpenGL ES: %s\n", version);
```

### Error Handling
Thread-local error messages:
```c
if (!renderer_render_frame(ctx, &config)) {
    fprintf(stderr, "Error: %s\n", renderer_get_error());
}
```

## 🧪 Testing

### Texture Generation
```bash
# Test all procedural textures
for type in rgba_noise gray_noise blue_noise wood abstract; do
    # Render texture and verify output
done
```

### Frame Rendering
```bash
# Test display modes
for mode in center stretch fit fill tile; do
    # Render with mode and verify
done
```

## 🔮 Future Enhancements

### Planned Features
- [ ] Image loading (PNG, JPEG)
- [ ] Shader hot-reload
- [ ] Mipmap generation
- [ ] Compressed textures (ASTC, ETC2)
- [ ] Multi-sample anti-aliasing
- [ ] Custom transition plugins

### Performance
- [ ] Texture streaming
- [ ] Shader compilation cache
- [ ] GPU memory pooling
- [ ] Async texture loading

## 📝 API Reference

### Core Functions

| Function | Description |
|----------|-------------|
| `renderer_create()` | Create renderer context |
| `renderer_destroy()` | Destroy renderer |
| `renderer_begin_frame()` | Start frame rendering |
| `renderer_render_frame()` | Render frame content |
| `renderer_end_frame()` | Finalize and swap buffers |

### Texture Functions

| Function | Description |
|----------|-------------|
| `renderer_texture_from_file()` | Load texture from file |
| `renderer_texture_from_memory()` | Create from memory |
| `renderer_texture_procedural()` | Generate procedural |
| `renderer_texture_destroy()` | Free texture |

### Shader Functions

| Function | Description |
|----------|-------------|
| `renderer_shader_compile_file()` | Compile shader |
| `renderer_shader_destroy_program()` | Delete shader |

### Utility Functions

| Function | Description |
|----------|-------------|
| `renderer_display_mode_string()` | Mode to string |
| `renderer_display_mode_parse()` | Parse mode |
| `renderer_transition_string()` | Transition to string |
| `renderer_transition_parse()` | Parse transition |
| `renderer_get_error()` | Get last error |

## 📄 License

Part of NeoWall project - MIT License

---

**Built with ❤️ for beautiful desktop wallpapers**