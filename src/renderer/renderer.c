#include "renderer.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <time.h>
#include <sys/time.h>

/**
 * Renderer Implementation - Core Lifecycle Functions
 * 
 * This file implements the main renderer API for creating contexts,
 * managing outputs, and coordinating the rendering system.
 */

/* ========================================================================
 * Thread-Local Error Storage
 * ======================================================================== */

static _Thread_local char error_buffer[512] = {0};

void renderer_set_error(const char *format, ...) {
    va_list args;
    va_start(args, format);
    vsnprintf(error_buffer, sizeof(error_buffer), format, args);
    va_end(args);
}

const char *renderer_get_error(void) {
    return error_buffer[0] ? error_buffer : NULL;
}

/* ========================================================================
 * Time Utilities
 * ======================================================================== */

uint64_t renderer_get_time_ms(void) {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (uint64_t)ts.tv_sec * 1000 + (uint64_t)ts.tv_nsec / 1000000;
}

uint64_t renderer_get_time_us(void) {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (uint64_t)ts.tv_sec * 1000000 + (uint64_t)ts.tv_nsec / 1000;
}

/* ========================================================================
 * EGL Utilities
 * ======================================================================== */

static const char *egl_error_string(EGLint error) {
    switch (error) {
        case EGL_SUCCESS: return "Success";
        case EGL_NOT_INITIALIZED: return "Not initialized";
        case EGL_BAD_ACCESS: return "Bad access";
        case EGL_BAD_ALLOC: return "Bad alloc";
        case EGL_BAD_ATTRIBUTE: return "Bad attribute";
        case EGL_BAD_CONFIG: return "Bad config";
        case EGL_BAD_CONTEXT: return "Bad context";
        case EGL_BAD_CURRENT_SURFACE: return "Bad current surface";
        case EGL_BAD_DISPLAY: return "Bad display";
        case EGL_BAD_MATCH: return "Bad match";
        case EGL_BAD_NATIVE_PIXMAP: return "Bad native pixmap";
        case EGL_BAD_NATIVE_WINDOW: return "Bad native window";
        case EGL_BAD_PARAMETER: return "Bad parameter";
        case EGL_BAD_SURFACE: return "Bad surface";
        case EGL_CONTEXT_LOST: return "Context lost";
        default: return "Unknown error";
    }
}

static bool egl_check_error(const char *context) {
    EGLint error = eglGetError();
    if (error != EGL_SUCCESS) {
        renderer_set_error("EGL error in %s: %s (0x%x)", 
                          context ? context : "unknown", 
                          egl_error_string(error), 
                          error);
        return false;
    }
    return true;
}

/* ========================================================================
 * OpenGL ES Detection
 * ======================================================================== */

static gles_capabilities_t detect_gles_capabilities(void) {
    gles_capabilities_t caps = GLES_CAPS_NONE;
    
    const char *version = (const char *)glGetString(GL_VERSION);
    if (!version) {
        return caps;
    }
    
    /* Parse "OpenGL ES X.Y" */
    int major = 0, minor = 0;
    if (sscanf(version, "OpenGL ES %d.%d", &major, &minor) == 2 ||
        sscanf(version, "OpenGL ES-CM %d.%d", &major, &minor) == 2) {
        
        if (major >= 2) caps |= GLES_CAPS_2_0;
        if (major > 3 || (major == 3 && minor >= 0)) caps |= GLES_CAPS_3_0;
        if (major > 3 || (major == 3 && minor >= 1)) caps |= GLES_CAPS_3_1;
        if (major > 3 || (major == 3 && minor >= 2)) caps |= GLES_CAPS_3_2;
    }
    
    return caps;
}

/* ========================================================================
 * Context Creation
 * ======================================================================== */

renderer_context_t *renderer_create(const renderer_config_t *config) {
    /* Allocate context */
    renderer_context_t *ctx = calloc(1, sizeof(renderer_context_t));
    if (!ctx) {
        renderer_set_error("Failed to allocate renderer context");
        return NULL;
    }
    
    /* Apply configuration */
    if (config) {
        ctx->gl_version_pref = config->gl_version;
        ctx->enable_vsync = config->enable_vsync;
        ctx->enable_debug = config->enable_debug;
        ctx->user_data = config->user_data;
        ctx->egl_display = config->egl_display;
        ctx->egl_config = config->egl_config;
        ctx->egl_context = config->egl_context;
    } else {
        /* Default configuration */
        ctx->gl_version_pref = RENDERER_GLES_AUTO;
        ctx->enable_vsync = true;
        ctx->enable_debug = false;
        ctx->user_data = NULL;
        ctx->egl_display = EGL_NO_DISPLAY;
        ctx->egl_config = NULL;
        ctx->egl_context = EGL_NO_CONTEXT;
    }
    
    /* Initialize mutex */
    if (pthread_mutex_init(&ctx->outputs_mutex, NULL) != 0) {
        renderer_set_error("Failed to initialize mutex");
        free(ctx);
        return NULL;
    }
    
    /* If no EGL context provided, create one */
    if (ctx->egl_context == EGL_NO_CONTEXT) {
        /* Get EGL display if not provided */
        if (ctx->egl_display == EGL_NO_DISPLAY) {
            ctx->egl_display = eglGetDisplay(EGL_DEFAULT_DISPLAY);
            if (ctx->egl_display == EGL_NO_DISPLAY) {
                renderer_set_error("Failed to get EGL display");
                pthread_mutex_destroy(&ctx->outputs_mutex);
                free(ctx);
                return NULL;
            }
        }
        
        /* Initialize EGL */
        EGLint major, minor;
        if (!eglInitialize(ctx->egl_display, &major, &minor)) {
            renderer_set_error("Failed to initialize EGL");
            pthread_mutex_destroy(&ctx->outputs_mutex);
            free(ctx);
            return NULL;
        }
        
        /* Bind OpenGL ES API */
        if (!eglBindAPI(EGL_OPENGL_ES_API)) {
            renderer_set_error("Failed to bind OpenGL ES API");
            eglTerminate(ctx->egl_display);
            pthread_mutex_destroy(&ctx->outputs_mutex);
            free(ctx);
            return NULL;
        }
        
        /* Choose config based on version preference */
        EGLint config_attribs_es3[] = {
            EGL_SURFACE_TYPE, EGL_WINDOW_BIT,
            EGL_RENDERABLE_TYPE, EGL_OPENGL_ES3_BIT,
            EGL_RED_SIZE, 8,
            EGL_GREEN_SIZE, 8,
            EGL_BLUE_SIZE, 8,
            EGL_ALPHA_SIZE, 8,
            EGL_NONE
        };
        
        EGLint config_attribs_es2[] = {
            EGL_SURFACE_TYPE, EGL_WINDOW_BIT,
            EGL_RENDERABLE_TYPE, EGL_OPENGL_ES2_BIT,
            EGL_RED_SIZE, 8,
            EGL_GREEN_SIZE, 8,
            EGL_BLUE_SIZE, 8,
            EGL_ALPHA_SIZE, 8,
            EGL_NONE
        };
        
        EGLint num_configs;
        bool using_es3 = false;
        
        /* Try ES 3.0 first (unless ES 2.0 is forced) */
        if (ctx->gl_version_pref != RENDERER_GLES_2_0) {
            if (eglChooseConfig(ctx->egl_display, config_attribs_es3,
                               &ctx->egl_config, 1, &num_configs) && num_configs > 0) {
                
                EGLint context_attribs_es3[] = {
                    EGL_CONTEXT_MAJOR_VERSION, 3,
                    EGL_CONTEXT_MINOR_VERSION, 0,
                    EGL_NONE
                };
                
                ctx->egl_context = eglCreateContext(ctx->egl_display,
                                                   ctx->egl_config,
                                                   EGL_NO_CONTEXT,
                                                   context_attribs_es3);
                
                if (ctx->egl_context != EGL_NO_CONTEXT) {
                    using_es3 = true;
                    ctx->gl_major = 3;
                    ctx->gl_minor = 0;
                    if (ctx->enable_debug) {
                        fprintf(stderr, "[renderer] Created OpenGL ES 3.0 context\n");
                    }
                }
            }
        }
        
        /* Fallback to ES 2.0 */
        if (!using_es3) {
            if (!eglChooseConfig(ctx->egl_display, config_attribs_es2,
                                &ctx->egl_config, 1, &num_configs) || num_configs == 0) {
                renderer_set_error("No suitable EGL configs found");
                eglTerminate(ctx->egl_display);
                pthread_mutex_destroy(&ctx->outputs_mutex);
                free(ctx);
                return NULL;
            }
            
            EGLint context_attribs_es2[] = {
                EGL_CONTEXT_CLIENT_VERSION, 2,
                EGL_NONE
            };
            
            ctx->egl_context = eglCreateContext(ctx->egl_display,
                                               ctx->egl_config,
                                               EGL_NO_CONTEXT,
                                               context_attribs_es2);
            
            if (ctx->egl_context == EGL_NO_CONTEXT) {
                renderer_set_error("Failed to create EGL context");
                eglTerminate(ctx->egl_display);
                pthread_mutex_destroy(&ctx->outputs_mutex);
                free(ctx);
                return NULL;
            }
            
            ctx->gl_major = 2;
            ctx->gl_minor = 0;
            if (ctx->enable_debug) {
                fprintf(stderr, "[renderer] Created OpenGL ES 2.0 context\n");
            }
        }
        
        ctx->owns_egl_context = true;
    } else {
        ctx->owns_egl_context = false;
    }
    
    /* Make context current temporarily to detect capabilities */
    EGLSurface dummy_surface = EGL_NO_SURFACE;
    if (eglMakeCurrent(ctx->egl_display, dummy_surface, dummy_surface, ctx->egl_context)) {
        ctx->gl_caps = detect_gles_capabilities();
        
        if (ctx->enable_debug) {
            const char *version = (const char *)glGetString(GL_VERSION);
            const char *renderer = (const char *)glGetString(GL_RENDERER);
            const char *vendor = (const char *)glGetString(GL_VENDOR);
            
            fprintf(stderr, "[renderer] OpenGL ES version: %s\n", version ? version : "unknown");
            fprintf(stderr, "[renderer] Renderer: %s\n", renderer ? renderer : "unknown");
            fprintf(stderr, "[renderer] Vendor: %s\n", vendor ? vendor : "unknown");
            fprintf(stderr, "[renderer] Capabilities: 0x%x\n", ctx->gl_caps);
        }
        
        /* Clear context */
        eglMakeCurrent(ctx->egl_display, EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT);
    }
    
    /* Initialize linked list */
    ctx->outputs = NULL;
    ctx->output_count = 0;
    
    /* Initialize global resources */
    ctx->default_channels_initialized = false;
    memset(ctx->default_channel_textures, 0, sizeof(ctx->default_channel_textures));
    ctx->color_program = 0;
    
    if (ctx->enable_debug) {
        fprintf(stderr, "[renderer] Context created successfully\n");
    }
    
    return ctx;
}

/* ========================================================================
 * Context Destruction
 * ======================================================================== */

void renderer_destroy(renderer_context_t *ctx) {
    if (!ctx) {
        return;
    }
    
    if (ctx->enable_debug) {
        fprintf(stderr, "[renderer] Destroying context\n");
    }
    
    /* Destroy all outputs */
    pthread_mutex_lock(&ctx->outputs_mutex);
    renderer_output_t *output = ctx->outputs;
    while (output) {
        renderer_output_t *next = output->next;
        renderer_output_destroy(output);
        output = next;
    }
    ctx->outputs = NULL;
    ctx->output_count = 0;
    pthread_mutex_unlock(&ctx->outputs_mutex);
    
    /* Make context current to clean up global resources */
    if (ctx->egl_context != EGL_NO_CONTEXT) {
        eglMakeCurrent(ctx->egl_display, EGL_NO_SURFACE, EGL_NO_SURFACE, ctx->egl_context);
        
        /* Delete global shader programs */
        if (ctx->color_program != 0) {
            glDeleteProgram(ctx->color_program);
            ctx->color_program = 0;
        }
        
        /* Delete default channel textures */
        if (ctx->default_channels_initialized) {
            for (int i = 0; i < 5; i++) {
                if (ctx->default_channel_textures[i] != 0) {
                    glDeleteTextures(1, &ctx->default_channel_textures[i]);
                    ctx->default_channel_textures[i] = 0;
                }
            }
            ctx->default_channels_initialized = false;
        }
        
        /* Clear context */
        eglMakeCurrent(ctx->egl_display, EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT);
    }
    
    /* Destroy EGL context if we own it */
    if (ctx->owns_egl_context && ctx->egl_context != EGL_NO_CONTEXT) {
        eglDestroyContext(ctx->egl_display, ctx->egl_context);
        ctx->egl_context = EGL_NO_CONTEXT;
    }
    
    /* Terminate EGL if we own the display */
    if (ctx->owns_egl_context && ctx->egl_display != EGL_NO_DISPLAY) {
        eglTerminate(ctx->egl_display);
        ctx->egl_display = EGL_NO_DISPLAY;
    }
    
    /* Destroy mutex */
    pthread_mutex_destroy(&ctx->outputs_mutex);
    
    /* Free context */
    free(ctx);
    
    if (ctx->enable_debug) {
        fprintf(stderr, "[renderer] Context destroyed\n");
    }
}

/* ========================================================================
 * Utility Functions
 * ======================================================================== */

const char *renderer_display_mode_string(renderer_display_mode_t mode) {
    switch (mode) {
        case RENDERER_MODE_CENTER: return "center";
        case RENDERER_MODE_STRETCH: return "stretch";
        case RENDERER_MODE_FIT: return "fit";
        case RENDERER_MODE_FILL: return "fill";
        case RENDERER_MODE_TILE: return "tile";
        default: return "unknown";
    }
}

renderer_display_mode_t renderer_display_mode_parse(const char *str) {
    if (!str) return RENDERER_MODE_FILL;
    
    if (strcmp(str, "center") == 0) return RENDERER_MODE_CENTER;
    if (strcmp(str, "stretch") == 0) return RENDERER_MODE_STRETCH;
    if (strcmp(str, "fit") == 0) return RENDERER_MODE_FIT;
    if (strcmp(str, "fill") == 0) return RENDERER_MODE_FILL;
    if (strcmp(str, "tile") == 0) return RENDERER_MODE_TILE;
    
    return RENDERER_MODE_FILL;
}

const char *renderer_transition_string(renderer_transition_t transition) {
    switch (transition) {
        case RENDERER_TRANSITION_NONE: return "none";
        case RENDERER_TRANSITION_FADE: return "fade";
        case RENDERER_TRANSITION_SLIDE_LEFT: return "slide_left";
        case RENDERER_TRANSITION_SLIDE_RIGHT: return "slide_right";
        case RENDERER_TRANSITION_GLITCH: return "glitch";
        case RENDERER_TRANSITION_PIXELATE: return "pixelate";
        default: return "unknown";
    }
}

renderer_transition_t renderer_transition_parse(const char *str) {
    if (!str) return RENDERER_TRANSITION_FADE;
    
    if (strcmp(str, "none") == 0) return RENDERER_TRANSITION_NONE;
    if (strcmp(str, "fade") == 0) return RENDERER_TRANSITION_FADE;
    if (strcmp(str, "slide_left") == 0) return RENDERER_TRANSITION_SLIDE_LEFT;
    if (strcmp(str, "slide_right") == 0) return RENDERER_TRANSITION_SLIDE_RIGHT;
    if (strcmp(str, "glitch") == 0) return RENDERER_TRANSITION_GLITCH;
    if (strcmp(str, "pixelate") == 0) return RENDERER_TRANSITION_PIXELATE;
    
    return RENDERER_TRANSITION_FADE;
}

const char *renderer_get_gl_version(renderer_context_t *ctx) {
    if (!ctx || ctx->egl_context == EGL_NO_CONTEXT) {
        return NULL;
    }
    
    /* Make context current temporarily */
    eglMakeCurrent(ctx->egl_display, EGL_NO_SURFACE, EGL_NO_SURFACE, ctx->egl_context);
    const char *version = (const char *)glGetString(GL_VERSION);
    eglMakeCurrent(ctx->egl_display, EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT);
    
    return version;
}

void renderer_set_debug(renderer_context_t *ctx, bool enable) {
    if (ctx) {
        ctx->enable_debug = enable;
    }
}

/* ========================================================================
 * OpenGL Utilities
 * ======================================================================== */

const char *renderer_gl_error_string(GLenum error) {
    switch (error) {
        case GL_NO_ERROR: return "No error";
        case GL_INVALID_ENUM: return "Invalid enum";
        case GL_INVALID_VALUE: return "Invalid value";
        case GL_INVALID_OPERATION: return "Invalid operation";
        case GL_OUT_OF_MEMORY: return "Out of memory";
        case GL_INVALID_FRAMEBUFFER_OPERATION: return "Invalid framebuffer operation";
        default: return "Unknown error";
    }
}

bool renderer_check_gl_error(const char *context) {
    GLenum error = glGetError();
    if (error != GL_NO_ERROR) {
        renderer_set_error("OpenGL error in %s: %s (0x%x)",
                          context ? context : "unknown",
                          renderer_gl_error_string(error),
                          error);
        return false;
    }
    return true;
}

/* ========================================================================
 * Frame Rendering
 * ======================================================================== */

static EGLSurface current_surface = EGL_NO_SURFACE;
static int32_t current_width = 0;
static int32_t current_height = 0;

bool renderer_begin_frame(renderer_context_t *ctx,
                         EGLSurface egl_surface,
                         int32_t width,
                         int32_t height) {
    if (!ctx) {
        renderer_set_error("Invalid context");
        return false;
    }
    
    if (egl_surface == EGL_NO_SURFACE) {
        renderer_set_error("Invalid EGL surface");
        return false;
    }
    
    /* Make context current */
    if (!eglMakeCurrent(ctx->egl_display, egl_surface, egl_surface, ctx->egl_context)) {
        egl_check_error("eglMakeCurrent");
        return false;
    }
    
    /* Store current surface for end_frame */
    current_surface = egl_surface;
    current_width = width;
    current_height = height;
    
    /* Set viewport */
    glViewport(0, 0, width, height);
    
    /* Clear screen */
    glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT);
    
    return true;
}

bool renderer_render_frame(renderer_context_t *ctx,
                           const renderer_frame_config_t *config) {
    if (!ctx || !config) {
        renderer_set_error("Invalid parameters");
        return false;
    }
    
    if (current_surface == EGL_NO_SURFACE) {
        renderer_set_error("Must call renderer_begin_frame first");
        return false;
    }
    
    /* Render based on content type */
    if (config->type == RENDERER_TYPE_IMAGE) {
        /* Image rendering stub */
        if (ctx->enable_debug) {
            fprintf(stderr, "[renderer] Rendering image: %s\n", 
                    config->image_path ? config->image_path : "none");
        }
        /* TODO: Implement image rendering */
    } else if (config->type == RENDERER_TYPE_SHADER) {
        /* Shader rendering stub */
        if (ctx->enable_debug) {
            fprintf(stderr, "[renderer] Rendering shader: %s\n",
                    config->shader_path ? config->shader_path : "none");
        }
        /* TODO: Implement shader rendering */
    }
    
    return true;
}

bool renderer_end_frame(renderer_context_t *ctx) {
    if (!ctx) {
        renderer_set_error("Invalid context");
        return false;
    }
    
    if (current_surface == EGL_NO_SURFACE) {
        renderer_set_error("No active frame");
        return false;
    }
    
    /* Swap buffers */
    if (!eglSwapBuffers(ctx->egl_display, current_surface)) {
        egl_check_error("eglSwapBuffers");
        return false;
    }
    
    /* Clear current surface */
    current_surface = EGL_NO_SURFACE;
    current_width = 0;
    current_height = 0;
    
    return true;
}

/* ========================================================================
 * Shader Management
 * ======================================================================== */

GLuint renderer_shader_compile_file(renderer_context_t *ctx,
                                    const char *shader_path,
                                    size_t channel_count) {
    if (!ctx || !shader_path) {
        renderer_set_error("Invalid parameters");
        return 0;
    }
    
    if (ctx->enable_debug) {
        fprintf(stderr, "[renderer] Compiling shader: %s\n", shader_path);
    }
    
    /* TODO: Implement shader compilation */
    (void)channel_count; /* Unused for now */
    
    return 0;
}

void renderer_shader_destroy_program(renderer_context_t *ctx, GLuint program) {
    if (!ctx || program == 0) {
        return;
    }
    
    /* Make context current */
    eglMakeCurrent(ctx->egl_display, EGL_NO_SURFACE, EGL_NO_SURFACE, ctx->egl_context);
    
    /* Delete program */
    glDeleteProgram(program);
    
    if (ctx->enable_debug) {
        fprintf(stderr, "[renderer] Destroyed shader program %u\n", program);
    }
}