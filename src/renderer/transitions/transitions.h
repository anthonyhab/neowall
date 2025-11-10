#ifndef RENDERER_TRANSITIONS_H
#define RENDERER_TRANSITIONS_H

#include <GLES2/gl2.h>
#include <stdbool.h>
#include <stdint.h>

/**
 * Transition Effects Interface
 * 
 * Provides modular transition effects system for smooth wallpaper changes.
 * Each transition is self-contained and can be easily added/removed.
 * 
 * Transitions are generic rendering functions - they don't know about
 * output or monitor management. The caller provides textures and dimensions.
 */

#ifdef __cplusplus
extern "C" {
#endif

/* ========================================================================
 * Forward Declarations
 * ======================================================================== */

/* ========================================================================
 * Enumerations
 * ======================================================================== */

/**
 * Transition effect types
 */
typedef enum {
    TRANSITION_NONE,
    TRANSITION_FADE,
    TRANSITION_SLIDE_LEFT,
    TRANSITION_SLIDE_RIGHT,
    TRANSITION_GLITCH,
    TRANSITION_PIXELATE,
} transition_type_t;

/* ========================================================================
 * Transition Context
 * ======================================================================== */

/**
 * Transition parameters - generic input for transitions
 * 
 * Contains all information needed to render a transition without
 * knowing about higher-level output management.
 */
typedef struct {
    GLuint prev_texture;             /* Previous wallpaper texture */
    GLuint current_texture;          /* Current wallpaper texture */
    int32_t width;                   /* Viewport width */
    int32_t height;                  /* Viewport height */
    float progress;                  /* Transition progress (0.0-1.0) */
    uint64_t frame_count;            /* Frame counter for animations */
} transition_params_t;

/**
 * Transition context for managing OpenGL state
 * 
 * Abstracts common OpenGL operations needed by transitions.
 * Ensures consistent state management across all effects.
 */
typedef struct {
    transition_params_t *params;     /* Transition parameters */
    GLuint program;                  /* Shader program to use */
    GLint pos_attrib;                /* Position attribute location */
    GLint tex_attrib;                /* Texture coord attribute location */
    float vertices[16];              /* Vertex data buffer */
    bool blend_enabled;              /* Blending state */
    bool error_occurred;             /* Error flag */
} transition_context_t;

/* ========================================================================
 * Transition Functions
 * ======================================================================== */

/**
 * Initialize transitions system
 * 
 * Sets up the transition registry and prepares for use.
 */
void transitions_init(void);

/**
 * Render a transition effect
 * 
 * Dispatches to the appropriate transition renderer based on type.
 * 
 * @param params Transition parameters (textures, dimensions, progress)
 * @param type Type of transition to render
 * @return true on success, false on error
 */
bool transition_render(transition_params_t *params, transition_type_t type);

/* ========================================================================
 * High-Level Transition API
 * ======================================================================== */

/**
 * Begin a transition rendering context
 * 
 * Initializes OpenGL state for transition rendering.
 * Handles viewport, clearing, shader activation, etc.
 * 
 * @param ctx Transition context to initialize
 * @param params Transition parameters
 * @param program Shader program to use
 * @return true on success, false on error
 */
bool transition_begin(transition_context_t *ctx, 
                     transition_params_t *params,
                     GLuint program);

/**
 * Draw a textured quad during transition
 * 
 * Renders a fullscreen quad with the given texture and alpha.
 * 
 * @param ctx Transition context
 * @param texture OpenGL texture ID
 * @param alpha Alpha/opacity value (0.0-1.0)
 * @param custom_vertices Custom vertex data (NULL for fullscreen)
 * @return true on success, false on error
 */
bool transition_draw_textured_quad(transition_context_t *ctx,
                                   GLuint texture,
                                   float alpha,
                                   const float *custom_vertices);

/**
 * End transition rendering context
 * 
 * Cleans up OpenGL state and finalizes the transition frame.
 * 
 * @param ctx Transition context
 */
void transition_end(transition_context_t *ctx);

/* ========================================================================
 * Low-Level Helper Functions
 * ======================================================================== */

/**
 * Setup fullscreen quad vertices
 * 
 * Creates standard fullscreen quad with texture coordinates.
 * 
 * @param vbo VBO to bind and upload to
 * @param vertices Output vertex array (must be 16 floats)
 */
void transition_setup_fullscreen_quad(GLuint vbo, float vertices[16]);

/**
 * Bind texture for transition rendering
 * 
 * Sets up texture with appropriate filtering and wrapping.
 * 
 * @param texture Texture ID to bind
 * @param texture_unit GL_TEXTURE0, GL_TEXTURE1, etc.
 */
void transition_bind_texture_for_transition(GLuint texture, GLenum texture_unit);

/**
 * Setup common vertex attributes
 * 
 * Configures position and texcoord attributes.
 * 
 * @param program Shader program
 * @param vbo VBO containing vertex data
 */
void transition_setup_common_attributes(GLuint program, GLuint vbo);

/* ========================================================================
 * Individual Transition Implementations
 * ======================================================================== */

/**
 * Fade transition
 * Classic crossfade between two images.
 */
bool transition_fade_render(transition_params_t *params);

/**
 * Slide left transition
 * New image slides in from right, old slides out to left.
 */
bool transition_slide_left_render(transition_params_t *params);

/**
 * Slide right transition
 * New image slides in from left, old slides out to right.
 */
bool transition_slide_right_render(transition_params_t *params);

/**
 * Glitch transition
 * Digital glitch effect with RGB channel separation.
 */
bool transition_glitch_render(transition_params_t *params);

/**
 * Pixelate transition
 * Progressive pixelation/depixelation effect.
 */
bool transition_pixelate_render(transition_params_t *params);

/* ========================================================================
 * Shader Program Creation
 * ======================================================================== */

/**
 * Create fade shader program
 */
bool shader_create_fade_program(GLuint *program);

/**
 * Create slide shader program
 */
bool shader_create_slide_program(GLuint *program);

/**
 * Create glitch shader program
 */
bool shader_create_glitch_program(GLuint *program);

/**
 * Create pixelate shader program
 */
bool shader_create_pixelate_program(GLuint *program);

/* ========================================================================
 * Utility Functions
 * ======================================================================== */

/**
 * Get transition name string
 * 
 * @param type Transition type
 * @return Human-readable name
 */
const char *transition_get_name(transition_type_t type);

/**
 * Parse transition from string
 * 
 * @param str Transition name string
 * @return Transition type, or TRANSITION_NONE if unknown
 */
transition_type_t transition_parse(const char *str);

/**
 * Ease-in-out cubic interpolation
 * 
 * Smooth acceleration and deceleration for transitions.
 * 
 * @param t Progress value (0.0-1.0)
 * @return Eased value (0.0-1.0)
 */
float ease_in_out_cubic(float t);

#ifdef __cplusplus
}
#endif

#endif /* RENDERER_TRANSITIONS_H */