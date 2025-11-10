#include <stdlib.h>
#include <stdio.h>
#include <math.h>
#include <string.h>
#include <GLES2/gl2.h>
#include "transitions.h"
#include "../renderer.h"

/**
 * Glitch Transition Implementation
 * 
 * Digital glitch effect with RGB channel separation, scan lines, and distortion.
 * Creates a cyberpunk-style transition with color aberration.
 */

/* Vertex shader for glitch transition */
static const char *glitch_vertex_shader_source =
    "#version 100\n"
    "attribute vec2 position;\n"
    "attribute vec2 texcoord;\n"
    "varying vec2 v_texcoord;\n"
    "void main() {\n"
    "    gl_Position = vec4(position, 0.0, 1.0);\n"
    "    v_texcoord = texcoord;\n"
    "}\n";

/* Fragment shader for glitch transition with RGB separation */
static const char *glitch_fragment_shader_source =
    "#version 100\n"
    "precision mediump float;\n"
    "varying vec2 v_texcoord;\n"
    "uniform sampler2D texture0;\n"
    "uniform sampler2D texture1;\n"
    "uniform float progress;\n"
    "uniform float time;\n"
    "uniform vec2 resolution;\n"
    "\n"
    "/* Simple pseudo-random function */\n"
    "float random(vec2 st) {\n"
    "    return fract(sin(dot(st.xy, vec2(12.9898, 78.233))) * 43758.5453);\n"
    "}\n"
    "\n"
    "void main() {\n"
    "    vec2 uv = v_texcoord;\n"
    "    \n"
    "    /* Glitch intensity peaks at mid-transition */\n"
    "    float glitch_intensity = sin(progress * 3.14159) * 0.5;\n"
    "    \n"
    "    /* Horizontal distortion with scan lines */\n"
    "    float line = floor(uv.y * resolution.y / 3.0);\n"
    "    float distort = random(vec2(line, time)) * glitch_intensity * 0.05;\n"
    "    vec2 distorted_uv = vec2(uv.x + distort, uv.y);\n"
    "    \n"
    "    /* RGB channel separation */\n"
    "    float separation = glitch_intensity * 0.02;\n"
    "    vec2 uv_r = distorted_uv + vec2(separation, 0.0);\n"
    "    vec2 uv_g = distorted_uv;\n"
    "    vec2 uv_b = distorted_uv - vec2(separation, 0.0);\n"
    "    \n"
    "    /* Sample both textures with separated channels */\n"
    "    vec4 color0, color1;\n"
    "    color0.r = texture2D(texture0, uv_r).r;\n"
    "    color0.g = texture2D(texture0, uv_g).g;\n"
    "    color0.b = texture2D(texture0, uv_b).b;\n"
    "    color0.a = texture2D(texture0, distorted_uv).a;\n"
    "    \n"
    "    color1.r = texture2D(texture1, uv_r).r;\n"
    "    color1.g = texture2D(texture1, uv_g).g;\n"
    "    color1.b = texture2D(texture1, uv_b).b;\n"
    "    color1.a = texture2D(texture1, distorted_uv).a;\n"
    "    \n"
    "    /* Mix based on progress */\n"
    "    vec4 result = mix(color0, color1, progress);\n"
    "    \n"
    "    /* Add scan line effect */\n"
    "    float scan = sin(uv.y * resolution.y * 2.0) * 0.05 * glitch_intensity;\n"
    "    result.rgb += scan;\n"
    "    \n"
    "    /* Random block corruption */\n"
    "    float block = floor(uv.y * 20.0);\n"
    "    if (random(vec2(block, time)) < glitch_intensity * 0.1) {\n"
    "        result.rgb = vec3(random(uv), random(uv + 0.1), random(uv + 0.2));\n"
    "    }\n"
    "    \n"
    "    gl_FragColor = result;\n"
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
                fprintf(stderr, "[glitch] Shader compilation failed: %s\n", info_log);
                free(info_log);
            }
        }
        glDeleteShader(shader);
        return 0;
    }
    
    return shader;
}

/**
 * Create shader program for glitch transition
 */
bool shader_create_glitch_program(GLuint *program) {
    if (!program) {
        return false;
    }
    
    /* Compile vertex shader */
    GLuint vertex_shader = compile_shader(GL_VERTEX_SHADER, glitch_vertex_shader_source);
    if (vertex_shader == 0) {
        fprintf(stderr, "[glitch] Failed to compile vertex shader\n");
        return false;
    }
    
    /* Compile fragment shader */
    GLuint fragment_shader = compile_shader(GL_FRAGMENT_SHADER, glitch_fragment_shader_source);
    if (fragment_shader == 0) {
        fprintf(stderr, "[glitch] Failed to compile fragment shader\n");
        glDeleteShader(vertex_shader);
        return false;
    }
    
    /* Create program */
    GLuint prog = glCreateProgram();
    if (prog == 0) {
        fprintf(stderr, "[glitch] Failed to create shader program\n");
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
                fprintf(stderr, "[glitch] Program linking failed: %s\n", info_log);
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
 * Glitch Transition Renderer
 * 
 * @param params Transition parameters (textures, dimensions, progress)
 * @return true on success, false on error
 */
bool transition_glitch_render(transition_params_t *params) {
    if (!params) {
        fprintf(stderr, "[glitch] Invalid parameters\n");
        return false;
    }

    /* Ensure we have both textures */
    if (!params->prev_texture || !params->current_texture) {
        fprintf(stderr, "[glitch] Missing textures for transition\n");
        return false;
    }

    /* Get or create shader program */
    static GLuint program = 0;
    if (program == 0) {
        if (!shader_create_glitch_program(&program)) {
            fprintf(stderr, "[glitch] Failed to create shader program\n");
            return false;
        }
    }
    
    /* Initialize transition context */
    transition_context_t ctx;
    if (!transition_begin(&ctx, params, program)) {
        fprintf(stderr, "[glitch] Failed to begin transition\n");
        return false;
    }
    
    /* Use the shader program */
    glUseProgram(program);
    
    /* Get uniform locations */
    GLint texture0_uniform = glGetUniformLocation(program, "texture0");
    GLint texture1_uniform = glGetUniformLocation(program, "texture1");
    GLint progress_uniform = glGetUniformLocation(program, "progress");
    GLint time_uniform = glGetUniformLocation(program, "time");
    GLint resolution_uniform = glGetUniformLocation(program, "resolution");
    
    /* Bind textures */
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, params->prev_texture);
    glUniform1i(texture0_uniform, 0);
    
    glActiveTexture(GL_TEXTURE1);
    glBindTexture(GL_TEXTURE_2D, params->current_texture);
    glUniform1i(texture1_uniform, 1);
    
    /* Set uniforms */
    glUniform1f(progress_uniform, params->progress);
    glUniform1f(time_uniform, (float)(params->frame_count % 1000) * 0.016f); /* Approximate time */
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
    
    glActiveTexture(GL_TEXTURE1);
    glBindTexture(GL_TEXTURE_2D, 0);
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, 0);
    
    transition_end(&ctx);
    
    return true;
}