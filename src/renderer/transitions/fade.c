#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <GLES2/gl2.h>
#include "transitions.h"
#include "../renderer.h"

/**
 * Fade Transition Implementation
 * 
 * Classic crossfade effect where the new wallpaper gradually appears
 * over the old wallpaper with smooth alpha blending.
 */

/* Vertex shader for fade transition */
static const char *fade_vertex_shader_source =
    "#version 100\n"
    "attribute vec2 position;\n"
    "attribute vec2 texcoord;\n"
    "varying vec2 v_texcoord;\n"
    "void main() {\n"
    "    gl_Position = vec4(position, 0.0, 1.0);\n"
    "    v_texcoord = texcoord;\n"
    "}\n";

/* Fragment shader for fade transition */
static const char *fade_fragment_shader_source =
    "#version 100\n"
    "precision mediump float;\n"
    "varying vec2 v_texcoord;\n"
    "uniform sampler2D texture0;\n"
    "uniform float alpha;\n"
    "void main() {\n"
    "    vec4 color = texture2D(texture0, v_texcoord);\n"
    "    gl_FragColor = vec4(color.rgb, color.a * alpha);\n"
    "}\n";

/**
 * Compile shader from source
 */
static GLuint compile_shader(GLenum type, const char *source) {
    GLuint shader = glCreateShader(type);
    if (shader == 0) {
        return 0;
    }
    
    glShaderSource(shader, 1, &source, NULL);
    glCompileShader(shader);
    
    GLint compiled;
    glGetShaderiv(shader, GL_COMPILE_STATUS, &compiled);
    if (!compiled) {
        GLint info_len = 0;
        glGetShaderiv(shader, GL_INFO_LOG_LENGTH, &info_len);
        if (info_len > 1) {
            char *info_log = malloc(info_len);
            if (info_log) {
                glGetShaderInfoLog(shader, info_len, NULL, info_log);
                fprintf(stderr, "[fade] Shader compilation failed: %s\n", info_log);
                free(info_log);
            }
        }
        glDeleteShader(shader);
        return 0;
    }
    
    return shader;
}

/**
 * Create shader program for fade transition
 * 
 * @param program Pointer to store the created program ID
 * @return true on success, false on failure
 */
bool shader_create_fade_program(GLuint *program) {
    if (!program) {
        return false;
    }
    
    /* Compile vertex shader */
    GLuint vertex_shader = compile_shader(GL_VERTEX_SHADER, fade_vertex_shader_source);
    if (vertex_shader == 0) {
        fprintf(stderr, "[fade] Failed to compile vertex shader\n");
        return false;
    }
    
    /* Compile fragment shader */
    GLuint fragment_shader = compile_shader(GL_FRAGMENT_SHADER, fade_fragment_shader_source);
    if (fragment_shader == 0) {
        fprintf(stderr, "[fade] Failed to compile fragment shader\n");
        glDeleteShader(vertex_shader);
        return false;
    }
    
    /* Create program */
    GLuint prog = glCreateProgram();
    if (prog == 0) {
        fprintf(stderr, "[fade] Failed to create shader program\n");
        glDeleteShader(vertex_shader);
        glDeleteShader(fragment_shader);
        return false;
    }
    
    /* Attach shaders */
    glAttachShader(prog, vertex_shader);
    glAttachShader(prog, fragment_shader);
    
    /* Link program */
    glLinkProgram(prog);
    
    /* Check link status */
    GLint linked;
    glGetProgramiv(prog, GL_LINK_STATUS, &linked);
    if (!linked) {
        GLint info_len = 0;
        glGetProgramiv(prog, GL_INFO_LOG_LENGTH, &info_len);
        if (info_len > 1) {
            char *info_log = malloc(info_len);
            if (info_log) {
                glGetProgramInfoLog(prog, info_len, NULL, info_log);
                fprintf(stderr, "[fade] Program linking failed: %s\n", info_log);
                free(info_log);
            }
        }
        glDeleteProgram(prog);
        glDeleteShader(vertex_shader);
        glDeleteShader(fragment_shader);
        return false;
    }
    
    /* Clean up shaders (they're now in the program) */
    glDeleteShader(vertex_shader);
    glDeleteShader(fragment_shader);
    
    *program = prog;
    return true;
}

/**
 * Fade Transition Renderer
 * 
 * Implements a classic crossfade between two images. The old image
 * remains at full opacity while the new image fades in from transparent
 * to opaque based on the progress value.
 * 
 * @param params Transition parameters (textures, dimensions, progress)
 * @return true on success, false on error
 */
bool transition_fade_render(transition_params_t *params) {
    if (!params) {
        fprintf(stderr, "[fade] Invalid parameters\n");
        return false;
    }
    
    /* Validate we have both textures for the transition */
    if (params->current_texture == 0 || params->prev_texture == 0) {
        fprintf(stderr, "[fade] Missing textures for transition (current=%u, prev=%u)\n",
                params->current_texture, params->prev_texture);
        return false;
    }
    
    /* Get or create shader program */
    static GLuint program = 0;
    if (program == 0) {
        if (!shader_create_fade_program(&program)) {
            fprintf(stderr, "[fade] Failed to create shader program\n");
            return false;
        }
    }
    
    /* Apply easing to progress for smoother transition */
    float eased_progress = ease_in_out_cubic(params->progress);
    
    /* Initialize transition context */
    transition_context_t ctx;
    if (!transition_begin(&ctx, params, program)) {
        fprintf(stderr, "[fade] Failed to begin transition context\n");
        return false;
    }
    
    /* Draw old image at full opacity (background layer) */
    if (!transition_draw_textured_quad(&ctx, params->prev_texture, 1.0f, NULL)) {
        fprintf(stderr, "[fade] Failed to draw old image\n");
        transition_end(&ctx);
        return false;
    }
    
    /* Draw new image with alpha based on progress (fading in) */
    if (!transition_draw_textured_quad(&ctx, params->current_texture, eased_progress, NULL)) {
        fprintf(stderr, "[fade] Failed to draw new image\n");
        transition_end(&ctx);
        return false;
    }
    
    /* Clean up transition context */
    transition_end(&ctx);
    
    return true;
}