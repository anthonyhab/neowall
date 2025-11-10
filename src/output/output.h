#ifndef OUTPUT_H
#define OUTPUT_H

#include <stdint.h>
#include <stdbool.h>
#include <EGL/egl.h>
#include <wayland-client.h>

/**
 * NeoWall Output System
 * 
 * Manages individual display outputs (monitors) including:
 * - Output detection and configuration
 * - Multi-monitor coordination
 * - Per-output wallpaper settings
 * - Output lifecycle management
 */

#ifdef __cplusplus
extern "C" {
#endif

/* ========================================================================
 * Forward Declarations
 * ======================================================================== */

struct neowall_state;
struct output_state;
struct wallpaper_config;
struct compositor_surface;

/* ========================================================================
 * Types & Structures
 * ======================================================================== */

/**
 * Output state - represents a single monitor/display
 */
struct output_state {
    /* Wayland objects */
    struct wl_output *output;              /* Wayland output object */
    struct zxdg_output_v1 *xdg_output;     /* Extended output info */
    struct compositor_surface *compositor_surface; /* Compositor surface */
    
    /* Output properties */
    uint32_t name;                         /* Wayland output name/ID */
    int32_t width;                         /* Width in pixels */
    int32_t height;                        /* Height in pixels */
    int32_t scale;                         /* Scale factor */
    int32_t transform;                     /* Rotation/transform */
    
    char make[64];                         /* Manufacturer */
    char model[64];                        /* Model name */
    char connector_name[64];               /* Connector (e.g., HDMI-A-1) */
    
    /* State flags */
    bool configured;                       /* Output fully configured */
    bool needs_redraw;                     /* Redraw requested */
    
    /* Parent state */
    struct neowall_state *state;           /* Back-pointer to global state */
    
    /* Configuration */
    struct wallpaper_config *config;       /* Current wallpaper config */
    
    /* Rendering state */
    uint64_t last_frame_time;              /* Last frame timestamp */
    uint64_t frames_rendered;              /* Total frames rendered */
    uint64_t frame_count;                  /* Frame counter for animations */
    
    /* Texture state for transitions */
    uint32_t current_texture;              /* Current wallpaper texture (OpenGL) */
    uint32_t prev_texture;                 /* Previous wallpaper texture (for transitions) */
    
    /* Transition state */
    bool in_transition;                    /* Currently transitioning */
    float transition_progress;             /* Transition progress (0.0-1.0) */
    uint64_t transition_start_time;        /* Transition start timestamp */
    uint32_t transition_duration_ms;       /* Transition duration in milliseconds */
    
    /* Linked list */
    struct output_state *next;             /* Next output in list */
};

/* ========================================================================
 * Lifecycle Functions
 * ======================================================================== */

/**
 * Create a new output state
 * 
 * @param state Global state
 * @param output Wayland output object
 * @param name Output name/ID
 * @return Output state, or NULL on failure
 */
struct output_state *output_create(struct neowall_state *state,
                                   struct wl_output *output,
                                   uint32_t name);

/**
 * Destroy output state
 * 
 * Cleans up all resources associated with this output.
 * Safe to call with NULL.
 * 
 * @param output Output state
 */
void output_destroy(struct output_state *output);

/* ========================================================================
 * Configuration Functions
 * ======================================================================== */

/**
 * Configure compositor surface for output
 * 
 * Sets up the compositor-specific surface (layer shell, plasma shell, etc.)
 * based on the detected compositor.
 * 
 * @param output Output state
 * @return true on success, false on failure
 */
bool output_configure_compositor_surface(struct output_state *output);

/**
 * Create EGL surface for output
 * 
 * Creates the EGL rendering surface for this output.
 * 
 * @param output Output state
 * @return true on success, false on failure
 */
bool output_create_egl_surface(struct output_state *output);

/**
 * Apply wallpaper configuration to output
 * 
 * Updates the output with new wallpaper settings.
 * 
 * @param output Output state
 * @param config Wallpaper configuration
 * @return true on success, false on failure
 */
bool output_apply_config(struct output_state *output,
                        struct wallpaper_config *config);

/**
 * Apply deferred configuration
 * 
 * Applies configuration that was deferred due to output not being ready.
 * 
 * @param output Output state
 */
void output_apply_deferred_config(struct output_state *output);

/* ========================================================================
 * Wallpaper Management
 * ======================================================================== */

/**
 * Set static image wallpaper
 * 
 * @param output Output state
 * @param path Path to image file
 */
void output_set_wallpaper(struct output_state *output, const char *path);

/**
 * Set live shader wallpaper
 * 
 * @param output Output state
 * @param shader_path Path to GLSL shader file
 */
void output_set_shader(struct output_state *output, const char *shader_path);

/**
 * Cycle to next wallpaper
 * 
 * Switches to the next wallpaper in the cycle list.
 * 
 * @param output Output state
 */
void output_cycle_wallpaper(struct output_state *output);

/**
 * Check if output should cycle
 * 
 * Determines if enough time has passed for automatic cycling.
 * 
 * @param output Output state
 * @param current_time Current time in milliseconds
 * @return true if should cycle, false otherwise
 */
bool output_should_cycle(struct output_state *output, uint64_t current_time);

/**
 * Preload next wallpaper
 * 
 * Loads the next wallpaper in background for smooth transitions.
 * 
 * @param output Output state
 */
void output_preload_next_wallpaper(struct output_state *output);

/* ========================================================================
 * Utility Functions
 * ======================================================================== */

/**
 * Get output identifier
 * 
 * Returns the best identifier for this output (connector name preferred).
 * 
 * @param output Output state
 * @return Identifier string
 */
const char *output_get_identifier(const struct output_state *output);

/**
 * Mark output for redraw
 * 
 * @param output Output state
 */
static inline void output_mark_dirty(struct output_state *output) {
    if (output) {
        output->needs_redraw = true;
    }
}

/**
 * Check if output is ready for rendering
 * 
 * @param output Output state
 * @return true if ready, false otherwise
 */
static inline bool output_is_ready(const struct output_state *output) {
    return output && output->configured && output->compositor_surface;
}

#ifdef __cplusplus
}
#endif

#endif /* OUTPUT_H */