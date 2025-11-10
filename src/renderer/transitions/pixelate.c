#include <stdlib.h>
#include <stdio.h>
#include <math.h>
#include <string.h>
#include <GLES2/gl2.h>
#include "transitions.h"
#include "../renderer.h"

/**
 * Pixelate Transition Implementation
 * 
 * Progressive pixelation/depixelation effect during transition.
 * The image pixelates to maximum size at mid-transition, then depixelates.
 */

/* Vertex shader for pixelate transition */
static const char *pixelate_vertex_shader_source =
    "#version 100\n"
    "attribute vec2 position;\n"
    "attribute vec2 texcoord;\n"
    "varying vec2 v_texcoord;\n"
    "void main() {\n"
    "    gl_Position = vec4(position, 0.0, 1.0);\n"
    "    v_texcoord = texcoord;\n"
    "}\n";

/* Fragment shader for pixelate transition */
static const char *pixelate_fragment_shader_source =
    "#version 100\n"
    "precision mediump float;\n"
    "varying vec2 v_texcoord;\n"
    "uniform sampler2D texture0;\n"
    "uniform sampler2D texture1;\n"
    "uniform float progress;\n"
    "uniform vec2 resolution;\n"
    "\n"
    "void main() {\n"
    "    /* Pixelation peaks at mid-transition (progress = 0.5) */\n"
    "    float pixelation_factor = sin(progress * 3.14159);\n"
    "    \n"
    "    /* Pixel size ranges from 1.0 (no pixelation) to 64.0 (max pixelation) */\n"
    "    float pixel_size = mix(1.0, 64.0, pixelation_factor);\n"
    "    \n"
    "    /* Calculate pixelated UV coordinates */\n"
    "    vec2 pixel_count = resolution / pixel_size;\n"
    "    vec2 pixelated_uv = floor(v_texcoord * pixel_count) / pixel_count;\n"
    "    \n"
    "    /* Add half-pixel offset to sample from center of pixel block */\n"
    "    pixelated_uv += 0.5 / pixel_count;\n"
    "    \n"
    "    /* Sample both textures */\n"
    "    vec4 color0 = texture2D(texture0, pixelated_uv);\n"
    "    vec4 color1 = texture2D(texture1, pixelated_uv);\n"
    "    \n"
    "    /* Crossfade between textures */\n"
    "    gl_FragColor = mix(color0, color1, progress);\n"
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
                fprintf(stderr, "[pixelate] Shader compilation failed: %s\n", info_log);
                free(info_log);
            }
        }
        glDeleteShader(shader);
        return 0;
    }
    
    return shader;
}

/**
 * Create shader program for pixelate transition
 */
bool shader_create_pixelate_program(GLuint *program) {
    if (!program) {
        return false;
    }
    
    /* Compile vertex shader */
    GLuint vertex_shader = compile_shader(GL_VERTEX_SHADER, pixelate_vertex_shader_source);
    if (vertex_shader == 0) {
        fprintf(stderr, "[pixelate] Failed to compile vertex shader\n");
        return false;
    }
    
    /* Compile fragment shader */
    GLuint fragment_shader = compile_shader(GL_FRAGMENT_SHADER, pixelate_fragment_shader_source);
    if (fragment_shader == 0) {
        fprintf(stderr, "[pixelate] Failed to compile fragment shader\n");
        glDeleteShader(vertex_shader);
        return false;
    }
    
    /* Create program */
    GLuint prog = glCreateProgram();
    if (prog == 0) {
        fprintf(stderr, "[pixelate] Failed to create shader program\n");
        glDeleteShader(vertex_shader);
        glDeleteShader(fragment_shader);
        return false;
    }
    
    /* Attach and link */
    glAttachShader(prog, vertex_shader);
    glAttachShader(prog, fragment_shader);
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
                fprintf(stderr, "[pixelate] Program linking failed: %s\n", info_log);
                free(info_log);
            }
        }
        glDeleteProgram(prog);
        glDeleteShader(vertex_shader);
        glDeleteShader(fragment_shader);
        return false;
    }
    
    /* Clean up shaders */
    glDeleteShader(vertex_shader);
    glDeleteShader(fragment_shader);
    
    *program = prog;
    return true;
}

/**
 * Pixelate Transition Renderer
 * 
 * @param params Transition parameters (textures, dimensions, progress)
 * @return true on success, false on error
 */
bool transition_pixelate_render(transition_params_t *params) {
    if (!params) {
        fprintf(stderr, "[pixelate] Invalid parameters\n");
        return false;
    }

    /* Ensure we have both textures */
    if (!params->prev_texture || !params->current_texture) {
        fprintf(stderr, "[pixelate] Missing textures for transition\n");
        return false;
    }

    /* Get or create shader program */
    static GLuint program = 0;
    if (program == 0) {
        if (!shader_create_pixelate_program(&program)) {
            fprintf(stderr, "[pixelate] Failed to create shader program\n");
            return false;
        }
    }
    
    /* Initialize transition context */
    transition_context_t ctx;
    if (!transition_begin(&ctx, params, program)) {
        fprintf(stderr, "[pixelate] Failed to begin transition\n");
        return false;
    }
    
    /* Use the shader program */
    glUseProgram(program);
    
    /* Get uniform locations */
    GLint texture0_uniform = glGetUniformLocation(program, "texture0");
    GLint texture1_uniform = glGetUniformLocation(program, "texture1");
    GLint progress_uniform = glGetUniformLocation(program, "progress");
    GLint resolution_uniform = glGetUniformLocation(program, "resolution");
    
    /* Bind textures */
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, params->prev_texture);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glUniform1i(texture0_uniform, 0);
    
    glActiveTexture(GL_TEXTURE1);
    glBindTexture(GL_TEXTURE_2D, params->current_texture);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glUniform1i(texture1_uniform, 1);
    
    /* Set uniforms */
    glUniform1f(progress_uniform, params->progress);
    glUniform2f(resolution_uniform, (float)params->width, (float)params->height);
    
    /* Setup fullscreen quad */
    float vertices[16];
    transition_setup_fullscreen_quad(0, vertices);
    
    /* Get attribute locations */
    GLint pos_attrib = glGetAttribLocation(program, "position");
    GLint tex_attrib = glGetAttribLocation(program, "texcoord");
    
    /* Setup vertex attributes */
    glEnableVertexAttribArray(pos_attrib);
    glEnableVertexAttribArray(tex_attrib);
    
    glVertexAttribPointer(pos_attrib, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), vertices);
    glVertexAttribPointer(tex_attrib, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), vertices + 2);
    
    /* Draw quad */
    glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
    
    /* Cleanup */
    glDisableVertexAttribArray(pos_attrib);
    glDisableVertexAttribArray(tex_attrib);
    
    /* Reset texture filtering to default */
    glActiveTexture(GL_TEXTURE1);
    glBindTexture(GL_TEXTURE_2D, params->current_texture);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glBindTexture(GL_TEXTURE_2D, 0);
    
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, params->prev_texture);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glBindTexture(GL_TEXTURE_2D, 0);
    
    transition_end(&ctx);
    
    return true;
}