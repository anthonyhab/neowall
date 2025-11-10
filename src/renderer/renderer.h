#ifndef RENDERER_H
#define RENDERER_H

#include <stdint.h>
#include <stdbool.h>
#include <pthread.h>
#include <GLES2/gl2.h>
#include <EGL/egl.h>
#include <wayland-client.h>
#include <wayland-egl.h>

/**
 * NeoWall Renderer
 * 
 * Unified rendering system for wallpapers with GPU acceleration,
 * transitions, and shader support.
 */

#ifdef __cplusplus
extern "C" {
#endif

/* ========================================================================
 * Constants
 * ======================================================================== */

#define RENDERER_MAX_OUTPUTS 16
#define RENDERER_MAX_CHANNELS 8
#define RENDERER_MAX_PATH 4096

/* Timing constants */
#define RENDERER_MS_PER_SECOND 1000
#define RENDERER_US_PER_SECOND 1000000
#define RENDERER_SHADER_FADE_OUT_MS 400
#define RENDERER_SHADER_FADE_IN_MS 400

/* Default values */
#define RENDERER_DEFAULT_FPS 60
#define RENDERER_DEFAULT_TRANSITION_DURATION 0.3f
#define RENDERER_DEFAULT_SHADER_SPEED 1.0f
#define RENDERER_DEFAULT_TEXTURE_SIZE 256

/* ========================================================================
 * Forward Declarations
 * ======================================================================== */

typedef struct renderer_context renderer_context_t;
typedef struct renderer_texture renderer_texture_t;
typedef struct image_data image_data_t;
typedef struct renderer_frame renderer_frame_t;

/* ========================================================================
 * Enumerations
 * ======================================================================== */

/**
 * Wallpaper display modes
 */
typedef enum {
    RENDERER_MODE_CENTER,    /* Center image without scaling */
    RENDERER_MODE_STRETCH,   /* Stretch to fill screen (may distort) */
    RENDERER_MODE_FIT,       /* Scale to fit inside screen (letterbox) */
    RENDERER_MODE_FILL,      /* Scale to fill screen (crop if needed) */
    RENDERER_MODE_TILE,      /* Tile image to fill screen */
} renderer_display_mode_t;

/**
 * Transition effect types
 */
typedef enum {
    RENDERER_TRANSITION_NONE,
    RENDERER_TRANSITION_FADE,
    RENDERER_TRANSITION_SLIDE_LEFT,
    RENDERER_TRANSITION_SLIDE_RIGHT,
    RENDERER_TRANSITION_GLITCH,
    RENDERER_TRANSITION_PIXELATE,
} renderer_transition_t;

/**
 * Wallpaper content types
 */
typedef enum {
    RENDERER_TYPE_IMAGE,     /* Static image (PNG, JPEG) */
    RENDERER_TYPE_SHADER,    /* Live GLSL shader */
} renderer_content_type_t;

/**
 * OpenGL ES version preference
 */
typedef enum {
    RENDERER_GLES_AUTO,      /* Auto-detect (prefer ES 3.0) */
    RENDERER_GLES_2_0,       /* Force OpenGL ES 2.0 */
    RENDERER_GLES_3_0,       /* Force OpenGL ES 3.0 */
} renderer_gles_version_t;

/**
 * OpenGL ES capability levels
 */
typedef enum {
    GLES_CAPS_NONE = 0,
    GLES_CAPS_2_0 = 1 << 0,
    GLES_CAPS_3_0 = 1 << 1,
    GLES_CAPS_3_1 = 1 << 2,
    GLES_CAPS_3_2 = 1 << 3,
} gles_capabilities_t;

/**
 * Image format types
 */
typedef enum {
    IMAGE_FORMAT_PNG,
    IMAGE_FORMAT_JPEG,
    IMAGE_FORMAT_UNKNOWN,
} image_format_t;

/* ========================================================================
 * Configuration Structures
 * ======================================================================== */

/**
 * Renderer initialization configuration
 */
typedef struct {
    EGLDisplay egl_display;          /* Existing EGL display (can be EGL_NO_DISPLAY) */
    EGLConfig egl_config;            /* Existing EGL config (can be NULL) */
    EGLContext egl_context;          /* Existing EGL context (can be EGL_NO_CONTEXT) */
    
    renderer_gles_version_t gl_version; /* OpenGL ES version preference */
    
    bool enable_vsync;               /* Enable vertical sync (default: true) */
    bool enable_debug;               /* Enable debug logging (default: false) */
    
    void *user_data;                 /* User data pointer (passed to callbacks) */
} renderer_config_t;

/**
 * Output (monitor) configuration
 */
typedef struct {
    int32_t width;                   /* Display width in pixels */
    int32_t height;                  /* Display height in pixels */
    int32_t scale;                   /* Display scale factor (1, 2, etc.) */
    
    EGLNativeWindowType native_window; /* Native window handle (wl_egl_window*) */
    
    const char *name;                /* Output name (e.g., "HDMI-A-1") */
    void *user_data;                 /* User data for this output */
} renderer_output_config_t;

/**
 * Wallpaper configuration
 */
typedef struct {
    renderer_content_type_t type;    /* Image or shader */
    
    /* For images */
    const char *image_path;          /* Path to image file */
    renderer_display_mode_t mode;    /* Display mode */
    
    /* For shaders */
    const char *shader_path;         /* Path to GLSL fragment shader */
    float shader_speed;              /* Animation speed multiplier (default: 1.0) */
    const char **channel_paths;      /* iChannel texture paths (NULL-terminated) */
    size_t channel_count;            /* Number of channels (0 = default) */
    
    /* Transitions */
    renderer_transition_t transition; /* Transition effect */
    float transition_duration;       /* Duration in seconds (default: 0.3) */
    
    /* Performance */
    int target_fps;                  /* Target FPS (default: 60, range: 1-240) */
    bool show_fps;                   /* Show FPS counter (default: false) */
} renderer_wallpaper_config_t;

/**
 * Render statistics
 */
typedef struct {
    uint64_t frames_rendered;        /* Total frames rendered */
    float current_fps;               /* Current measured FPS */
    uint64_t frame_time_us;          /* Last frame time in microseconds */
    uint64_t gpu_memory_used;        /* Estimated GPU memory usage (bytes) */
} renderer_stats_t;

/**
 * Frame configuration for rendering
 */
typedef struct {
    EGLSurface egl_surface;          /* Surface to render to */
    int32_t width;                   /* Frame width */
    int32_t height;                  /* Frame height */
    renderer_content_type_t type;    /* Image or shader */
    renderer_display_mode_t mode;    /* Display mode (for images) */
    
    /* Image rendering */
    const char *image_path;          /* Path to image */
    GLuint texture;                  /* Preloaded texture (optional) */
    
    /* Shader rendering */
    const char *shader_path;         /* Path to shader */
    GLuint shader_program;           /* Preloaded shader (optional) */
    float shader_time;               /* Animation time */
    float shader_speed;              /* Animation speed */
    
    /* Performance */
    bool show_fps;                   /* Show FPS overlay */
} renderer_frame_config_t;

/* ========================================================================
 * Internal Structures
 * ======================================================================== */

/**
 * Image data
 */
struct image_data {
    uint8_t *pixels;                 /* RGBA pixel data */
    uint32_t width;
    uint32_t height;
    uint32_t channels;               /* 3 (RGB) or 4 (RGBA) */
    image_format_t format;
    char path[RENDERER_MAX_PATH];
};

/**
 * GL state cache (per-output)
 */
typedef struct {
    GLuint bound_texture;
    GLuint active_program;
    bool blend_enabled;
} gl_state_cache_t;

/**
 * Shader uniform locations cache
 */
typedef struct {
    GLint position;
    GLint texcoord;
    GLint tex_sampler;
    GLint u_resolution;
    GLint u_time;
    GLint u_speed;
    GLint *iChannel;
    size_t iChannel_count;
} shader_uniforms_t;

/**
 * Transition state
 */
typedef struct {
    renderer_transition_t type;
    uint64_t start_time;
    float duration;
    float progress;
    image_data_t *prev_image;
    GLuint prev_texture;
} transition_state_t;

/**
 * Shader fade state
 */
typedef struct {
    uint64_t fade_start_time;
    char pending_shader_path[RENDERER_MAX_PATH];
    bool in_progress;
} shader_fade_state_t;

/**
 * FPS measurement
 */
typedef struct {
    uint64_t last_log_time;
    uint64_t frame_count;
    float current_fps;
} fps_stats_t;

/**
 * Renderer output
 */
struct renderer_output {
    renderer_context_t *context;
    char name[128];
    int32_t width;
    int32_t height;
    int32_t scale;
    void *user_data;
    
    EGLSurface egl_surface;
    struct wl_egl_window *egl_window;
    
    renderer_content_type_t type;
    renderer_display_mode_t mode;
    image_data_t *current_image;
    GLuint current_texture;
    
    GLuint shader_program;
    uint64_t shader_start_time;
    float shader_speed;
    shader_fade_state_t shader_fade;
    bool shader_load_failed;
    
    GLuint *channel_textures;
    size_t channel_count;
    
    GLuint vbo;
    GLuint fade_program;
    GLuint glitch_program;
    GLuint pixelate_program;
    
    gl_state_cache_t gl_cache;
    shader_uniforms_t uniforms;
    transition_state_t transition;
    
    uint64_t last_frame_time;
    uint64_t frames_rendered;
    bool needs_redraw;
    
    int target_fps;
    bool show_fps;
    fps_stats_t fps_stats;
    
};

/**
 * Renderer context
 */
struct renderer_context {
    renderer_gles_version_t gl_version_pref;
    bool enable_vsync;
    bool enable_debug;
    void *user_data;
    
    EGLDisplay egl_display;
    EGLContext egl_context;
    EGLConfig egl_config;
    bool owns_egl_context;
    
    gles_capabilities_t gl_caps;
    int gl_major;
    int gl_minor;
    
    GLuint color_program;
    GLuint default_channel_textures[5];
    bool default_channels_initialized;
    
    uint64_t total_frames_rendered;
    uint64_t gpu_memory_estimate;
};

/**
 * Texture handle
 */
struct renderer_texture {
    renderer_context_t *context;
    GLuint texture_id;
    int32_t width;
    int32_t height;
    size_t memory_size;
};

/* ========================================================================
 * Lifecycle Functions
 * ======================================================================== */

/**
 * Create renderer context
 * 
 * Initializes the rendering system with EGL and OpenGL ES.
 * Can use existing EGL context or create a new one.
 * 
 * @param config Renderer configuration (NULL for defaults)
 * @return Renderer context, or NULL on failure
 * 
 * Example:
 *   renderer_config_t config = {
 *       .egl_display = my_display,
 *       .gl_version = RENDERER_GLES_AUTO,
 *       .enable_vsync = true,
 *   };
 *   renderer_context_t *ctx = renderer_create(&config);
 */
renderer_context_t *renderer_create(const renderer_config_t *config);

/**
 * Destroy renderer context
 * 
 * Cleans up all resources, including outputs and textures.
 * Safe to call with NULL.
 * 
 * @param ctx Renderer context
 */
void renderer_destroy(renderer_context_t *ctx);

/* ========================================================================
 * Frame Rendering (Low-Level)
 * ======================================================================== */

/**
 * Begin frame rendering
 * 
 * Prepares for rendering to a specific EGL surface.
 * Must be called before render_frame.
 * 
 * @param ctx Renderer context
 * @param egl_surface Surface to render to
 * @param width Frame width
 * @param height Frame height
 * @return true on success, false on error
 */
bool renderer_begin_frame(renderer_context_t *ctx,
                         EGLSurface egl_surface,
                         int32_t width,
                         int32_t height);

/**
 * Render a frame
 * 
 * Renders wallpaper content based on configuration.
 * Must call renderer_begin_frame first.
 * 
 * @param ctx Renderer context
 * @param config Frame configuration
 * @return true on success, false on error
 * 
 * Example:
 *   renderer_frame_config_t config = {
 *       .type = RENDERER_TYPE_IMAGE,
 *       .image_path = "/path/to/image.png",
 *       .mode = RENDERER_MODE_FILL,
 *   };
 *   renderer_begin_frame(ctx, egl_surface, 1920, 1080);
 *   renderer_render_frame(ctx, &config);
 *   renderer_end_frame(ctx);
 */
bool renderer_render_frame(renderer_context_t *ctx,
                           const renderer_frame_config_t *config);

/**
 * End frame rendering
 * 
 * Finalizes frame and swaps buffers.
 * 
 * @param ctx Renderer context
 * @return true on success, false on error
 */
bool renderer_end_frame(renderer_context_t *ctx);

/* ========================================================================
 * Texture Management
 * ======================================================================== */

/**
 * Create texture from image file
 * 
 * Loads an image and uploads it to GPU.
 * Supports PNG and JPEG formats.
 * 
 * @param ctx Renderer context
 * @param path Path to image file
 * @return Texture handle, or NULL on failure
 */
renderer_texture_t *renderer_texture_from_file(renderer_context_t *ctx,
                                               const char *path);

/**
 * Create texture from memory
 * 
 * Uploads RGBA pixel data to GPU.
 * 
 * @param ctx Renderer context
 * @param pixels RGBA pixel data (row-major, top-to-bottom)
 * @param width Width in pixels
 * @param height Height in pixels
 * @return Texture handle, or NULL on failure
 */
renderer_texture_t *renderer_texture_from_memory(renderer_context_t *ctx,
                                                 const uint8_t *pixels,
                                                 int32_t width,
                                                 int32_t height);

/**
 * Create procedural texture
 * 
 * Generates a texture using a built-in algorithm.
 * Available types: "rgba_noise", "gray_noise", "blue_noise", "wood", "abstract"
 * 
 * @param ctx Renderer context
 * @param type Texture type name
 * @param size Texture size (width and height in pixels)
 * @return Texture handle, or NULL on failure
 */
renderer_texture_t *renderer_texture_procedural(renderer_context_t *ctx,
                                                const char *type,
                                                int32_t size);

/**
 * Destroy texture
 * 
 * Frees GPU resources. Safe to call with NULL.
 * 
 * @param texture Texture handle
 */
void renderer_texture_destroy(renderer_texture_t *texture);

/* ========================================================================
 * Shader Management
 * ======================================================================== */

/**
 * Compile shader program from file
 * 
 * @param ctx Renderer context
 * @param shader_path Path to fragment shader
 * @param channel_count Number of iChannels (0 = default)
 * @return Shader program ID, or 0 on failure
 */
GLuint renderer_shader_compile_file(renderer_context_t *ctx,
                                    const char *shader_path,
                                    size_t channel_count);

/**
 * Destroy shader program
 * 
 * @param ctx Renderer context
 * @param program Shader program ID
 */
void renderer_shader_destroy_program(renderer_context_t *ctx, GLuint program);

/* ========================================================================
 * Statistics & Debugging
 * ======================================================================== */

/**
 * Get OpenGL ES version string
 * 
 * @param ctx Renderer context
 * @return Version string (e.g., "OpenGL ES 3.0"), or NULL
 */
const char *renderer_get_gl_version(renderer_context_t *ctx);

/**
 * Enable/disable debug logging
 * 
 * @param ctx Renderer context
 * @param enable Enable debug logging
 */
void renderer_set_debug(renderer_context_t *ctx, bool enable);

/* ========================================================================
 * Utility Functions
 * ======================================================================== */

/**
 * Get display mode string
 * 
 * @param mode Display mode
 * @return Human-readable string (e.g., "fill")
 */
const char *renderer_display_mode_string(renderer_display_mode_t mode);

/**
 * Parse display mode from string
 * 
 * @param str Mode string (e.g., "fill", "fit")
 * @return Display mode, or RENDERER_MODE_FILL on parse error
 */
renderer_display_mode_t renderer_display_mode_parse(const char *str);

/**
 * Get transition type string
 * 
 * @param transition Transition type
 * @return Human-readable string (e.g., "fade")
 */
const char *renderer_transition_string(renderer_transition_t transition);

/**
 * Parse transition type from string
 * 
 * @param str Transition string (e.g., "fade", "glitch")
 * @return Transition type, or RENDERER_TRANSITION_FADE on parse error
 */
renderer_transition_t renderer_transition_parse(const char *str);

/* ========================================================================
 * Error Handling
 * ======================================================================== */

/**
 * Get last error message
 * 
 * Thread-local error message for the last failed operation.
 * Valid until next renderer call in same thread.
 * 
 * @return Error message string, or NULL if no error
 * 
 * Example:
 *   if (!renderer_output_render(output)) {
 *       fprintf(stderr, "Render failed: %s\n", renderer_get_error());
 *   }
 */
const char *renderer_get_error(void);

/* ========================================================================
 * Internal Utility Functions
 * ======================================================================== */

void renderer_set_error(const char *format, ...);
uint64_t renderer_get_time_ms(void);
uint64_t renderer_get_time_us(void);
const char *renderer_gl_error_string(GLenum error);
bool renderer_check_gl_error(const char *context);

#ifdef __cplusplus
}
#endif

#endif /* RENDERER_H */