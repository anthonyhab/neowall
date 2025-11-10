#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <GLES2/gl2.h>
#include "transitions.h"
#include "../renderer.h"

/**
 * Slide Transition Implementation
 * 
 * Implements sliding transitions where images slide in/out horizontally.
 * - Slide Left: New image slides in from right, old slides out to left
 * - Slide Right: New image slides in from left, old slides out to right
 */

/* Vertex shader for slide transition */
static const char *slide_vertex_shader_source =
    "#version 100\n"
    "attribute vec2 position;\n"
    "attribute vec2 texcoord;\n"
    "varying vec2 v_texcoord;\n"
    "void main() {\n"
    "    gl_Position = vec4(position, 0.0, 1.0);\n"
    "    v_texcoord = texcoord;\n"
    "}\n";

/* Fragment shader for slide transition */
static const char *slide_fragment_shader_source =
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
                fprintf(stderr, "[slide] Shader compilation failed: %s\n", info_log);
                free(info_log);
            }
        }
        glDeleteShader(shader);
        return 0;
    }
    
    return shader;
}

/**
 * Create shader program for slide transition
 */
bool shader_create_slide_program(GLuint *program) {
    if (!program) {
        return false;
    }
    
    /* Compile vertex shader */
    GLuint vertex_shader = compile_shader(GL_VERTEX_SHADER, slide_vertex_shader_source);
    if (vertex_shader == 0) {
        fprintf(stderr, "[slide] Failed to compile vertex shader\n");
        return false;
    }
    
    /* Compile fragment shader */
    GLuint fragment_shader = compile_shader(GL_FRAGMENT_SHADER, slide_fragment_shader_source);
    if (fragment_shader == 0) {
        fprintf(stderr, "[slide] Failed to compile fragment shader\n");
        glDeleteShader(vertex_shader);
        return false;
    }
    
    /* Create program */
    GLuint prog = glCreateProgram();
    if (prog == 0) {
        fprintf(stderr, "[slide] Failed to create shader program\n");
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
                fprintf(stderr, "[slide] Program linking failed: %s\n", info_log);
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
 * Create custom vertices for slide effect
 * 
 * @param vertices Output vertex array (16 floats: 4 vertices * (x,y,u,v))
 * @param offset_x Horizontal offset (-1.0 to 1.0)
 */
static void create_slide_vertices(float vertices[16], float offset_x) {
    /* Position (x, y), TexCoord (u, v) */
    float base_vertices[] = {
        /* Bottom-left */
        -1.0f, -1.0f, 0.0f, 0.0f,
        /* Bottom-right */
         1.0f, -1.0f, 1.0f, 0.0f,
        /* Top-left */
        -1.0f,  1.0f, 0.0f, 1.0f,
        /* Top-right */
         1.0f,  1.0f, 1.0f, 1.0f,
    };
    
    /* Copy and apply offset */
    memcpy(vertices, base_vertices, sizeof(base_vertices));
    
    /* Apply horizontal offset to position coordinates */
    for (int i = 0; i < 4; i++) {
        vertices[i * 4 + 0] += offset_x * 2.0f; /* x position */
    }
}

/**
 * Slide Left Transition Renderer
 * 
 * @param params Transition parameters (textures, dimensions, progress)
 * @return true on success, false on error
 */
bool transition_slide_left_render(transition_params_t *params) {
    if (!params) {
        fprintf(stderr, "[slide] Invalid parameters\n");
        return false;
    }
    
    /* Get or create shader program */
    static GLuint program = 0;
    if (program == 0) {
        if (!shader_create_slide_program(&program)) {
            fprintf(stderr, "[slide] Failed to create shader program\n");
            return false;
        }
    }
    
    /* Apply easing */
    float eased_progress = ease_in_out_cubic(params->progress);
    
    /* Initialize transition context */
    transition_context_t ctx;
    if (!transition_begin(&ctx, params, program)) {
        fprintf(stderr, "[slide] Failed to begin transition\n");
        return false;
    }
    
    /* Enable blending for smooth transition */
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    
    /* Get uniform locations */
    GLint alpha_uniform = glGetUniformLocation(program, "alpha");
    GLint texture_uniform = glGetUniformLocation(program, "texture0");
    
    /* Render old image sliding out to the left */
    if (params->prev_texture) {
        float old_offset = -eased_progress; /* Slide left */
        float old_vertices[16];
        create_slide_vertices(old_vertices, old_offset);
        
        glUniform1i(texture_uniform, 0);
        glUniform1f(alpha_uniform, 1.0f);
        
        if (!transition_draw_textured_quad(&ctx, params->prev_texture, 1.0f, old_vertices)) {
            fprintf(stderr, "[slide] Failed to draw old texture\n");
        }
    }
    
    /* Render new image sliding in from the right */
    if (params->current_texture) {
        float new_offset = 1.0f - eased_progress; /* Start from right, move to center */
        float new_vertices[16];
        create_slide_vertices(new_vertices, new_offset);
        
        glUniform1i(texture_uniform, 0);
        glUniform1f(alpha_uniform, 1.0f);
        
        if (!transition_draw_textured_quad(&ctx, params->current_texture, 1.0f, new_vertices)) {
            fprintf(stderr, "[slide] Failed to draw new texture\n");
        }
    }
    
    /* Clean up */
    transition_end(&ctx);
    
    return true;
}

/**
 * Slide Right Transition Renderer
 * 
 * @param params Transition parameters (textures, dimensions, progress)
 * @return true on success, false on error
 */
bool transition_slide_right_render(transition_params_t *params) {
    if (!params) {
        fprintf(stderr, "[slide] Invalid parameters\n");
        return false;
    }
    
    /* Get or create shader program */
    static GLuint program = 0;
    if (program == 0) {
        if (!shader_create_slide_program(&program)) {
            fprintf(stderr, "[slide] Failed to create shader program\n");
            return false;
        }
    }
    
    /* Apply easing */
    float eased_progress = ease_in_out_cubic(params->progress);
    
    /* Initialize transition context */
    transition_context_t ctx;
    if (!transition_begin(&ctx, params, program)) {
        fprintf(stderr, "[slide] Failed to begin transition\n");
        return false;
    }
    
    /* Enable blending for smooth transition */
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    
    /* Get uniform locations */
    GLint alpha_uniform = glGetUniformLocation(program, "alpha");
    GLint texture_uniform = glGetUniformLocation(program, "texture0");
    
    /* Render old image sliding out to the right */
    if (params->prev_texture) {
        float old_offset = eased_progress; /* Slide right */
        float old_vertices[16];
        create_slide_vertices(old_vertices, old_offset);
        
        glUniform1i(texture_uniform, 0);
        glUniform1f(alpha_uniform, 1.0f);
        
        if (!transition_draw_textured_quad(&ctx, params->prev_texture, 1.0f, old_vertices)) {
            fprintf(stderr, "[slide] Failed to draw old texture\n");
        }
    }
    
    /* Render new image sliding in from the left */
    if (params->current_texture) {
        float new_offset = -1.0f + eased_progress; /* Start from left, move to center */
        float new_vertices[16];
        create_slide_vertices(new_vertices, new_offset);
        
        glUniform1i(texture_uniform, 0);
        glUniform1f(alpha_uniform, 1.0f);
        
        if (!transition_draw_textured_quad(&ctx, params->current_texture, 1.0f, new_vertices)) {
            fprintf(stderr, "[slide] Failed to draw new texture\n");
        }
    }
    
    /* Clean up */
    transition_end(&ctx);
    
    return true;
}