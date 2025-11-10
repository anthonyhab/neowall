#include "textures.h"
#include "../renderer.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/**
 * Texture Management Implementation
 * 
 * Handles texture creation, loading, and management for the renderer.
 * Supports both file-based textures and procedural generation.
 */

/* ========================================================================
 * Image Loading (stubs for now - will integrate with image loaders)
 * ======================================================================== */

static image_data_t *load_image_file(const char *path) {
    if (!path) {
        return NULL;
    }
    
    /* TODO: Implement actual image loading (PNG, JPEG) */
    fprintf(stderr, "[texture] Loading image: %s (stub)\n", path);
    
    return NULL;
}

static void free_image_data(image_data_t *img) {
    if (!img) {
        return;
    }
    
    if (img->pixels) {
        free(img->pixels);
        img->pixels = NULL;
    }
    
    free(img);
}

/* ========================================================================
 * Texture Creation
 * ======================================================================== */

renderer_texture_t *renderer_texture_from_file(renderer_context_t *ctx,
                                               const char *path) {
    if (!ctx || !path) {
        renderer_set_error("Invalid parameters");
        return NULL;
    }
    
    /* Load image data */
    image_data_t *img = load_image_file(path);
    if (!img) {
        renderer_set_error("Failed to load image: %s", path);
        return NULL;
    }
    
    /* Make context current */
    eglMakeCurrent(ctx->egl_display, EGL_NO_SURFACE, EGL_NO_SURFACE, ctx->egl_context);
    
    /* Create OpenGL texture */
    GLuint texture_id;
    glGenTextures(1, &texture_id);
    glBindTexture(GL_TEXTURE_2D, texture_id);
    
    /* Set texture parameters */
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    
    /* Upload texture data */
    GLenum format = (img->channels == 4) ? GL_RGBA : GL_RGB;
    glTexImage2D(GL_TEXTURE_2D, 0, format, img->width, img->height,
                 0, format, GL_UNSIGNED_BYTE, img->pixels);
    
    glBindTexture(GL_TEXTURE_2D, 0);
    
    /* Check for errors */
    GLenum error = glGetError();
    if (error != GL_NO_ERROR) {
        renderer_set_error("OpenGL error creating texture: 0x%x", error);
        glDeleteTextures(1, &texture_id);
        free_image_data(img);
        return NULL;
    }
    
    /* Create texture handle */
    renderer_texture_t *texture = malloc(sizeof(renderer_texture_t));
    if (!texture) {
        renderer_set_error("Failed to allocate texture handle");
        glDeleteTextures(1, &texture_id);
        free_image_data(img);
        return NULL;
    }
    
    texture->context = ctx;
    texture->texture_id = texture_id;
    texture->width = img->width;
    texture->height = img->height;
    texture->memory_size = img->width * img->height * img->channels;
    
    /* Update memory estimate */
    ctx->gpu_memory_estimate += texture->memory_size;
    
    /* Free image data (pixels are now on GPU) */
    free_image_data(img);
    
    if (ctx->enable_debug) {
        fprintf(stderr, "[texture] Created texture from file: %s (%dx%d, ID=%u)\n",
                path, texture->width, texture->height, texture->texture_id);
    }
    
    return texture;
}

renderer_texture_t *renderer_texture_from_memory(renderer_context_t *ctx,
                                                 const uint8_t *pixels,
                                                 int32_t width,
                                                 int32_t height) {
    if (!ctx || !pixels || width <= 0 || height <= 0) {
        renderer_set_error("Invalid parameters");
        return NULL;
    }
    
    /* Make context current */
    eglMakeCurrent(ctx->egl_display, EGL_NO_SURFACE, EGL_NO_SURFACE, ctx->egl_context);
    
    /* Create OpenGL texture */
    GLuint texture_id;
    glGenTextures(1, &texture_id);
    glBindTexture(GL_TEXTURE_2D, texture_id);
    
    /* Set texture parameters */
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    
    /* Upload texture data (assume RGBA) */
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, width, height,
                 0, GL_RGBA, GL_UNSIGNED_BYTE, pixels);
    
    glBindTexture(GL_TEXTURE_2D, 0);
    
    /* Check for errors */
    GLenum error = glGetError();
    if (error != GL_NO_ERROR) {
        renderer_set_error("OpenGL error creating texture: 0x%x", error);
        glDeleteTextures(1, &texture_id);
        return NULL;
    }
    
    /* Create texture handle */
    renderer_texture_t *texture = malloc(sizeof(renderer_texture_t));
    if (!texture) {
        renderer_set_error("Failed to allocate texture handle");
        glDeleteTextures(1, &texture_id);
        return NULL;
    }
    
    texture->context = ctx;
    texture->texture_id = texture_id;
    texture->width = width;
    texture->height = height;
    texture->memory_size = width * height * 4; /* RGBA */
    
    /* Update memory estimate */
    ctx->gpu_memory_estimate += texture->memory_size;
    
    if (ctx->enable_debug) {
        fprintf(stderr, "[texture] Created texture from memory (%dx%d, ID=%u)\n",
                width, height, texture_id);
    }
    
    return texture;
}

renderer_texture_t *renderer_texture_procedural(renderer_context_t *ctx,
                                                const char *type,
                                                int32_t size) {
    if (!ctx || !type || size <= 0) {
        renderer_set_error("Invalid parameters");
        return NULL;
    }
    
    /* Make context current */
    eglMakeCurrent(ctx->egl_display, EGL_NO_SURFACE, EGL_NO_SURFACE, ctx->egl_context);
    
    /* Generate procedural texture based on type */
    GLuint texture_id = 0;
    
    if (strcmp(type, "rgba_noise") == 0 || strcmp(type, "default") == 0) {
        texture_id = texture_create_rgba_noise(size, size);
    } else if (strcmp(type, "gray_noise") == 0) {
        texture_id = texture_create_gray_noise(size, size);
    } else if (strcmp(type, "blue_noise") == 0) {
        texture_id = texture_create_blue_noise(size, size);
    } else if (strcmp(type, "wood") == 0) {
        texture_id = texture_create_wood(size, size);
    } else if (strcmp(type, "abstract") == 0) {
        texture_id = texture_create_abstract(size, size);
    } else {
        renderer_set_error("Unknown procedural texture type: %s", type);
        return NULL;
    }
    
    if (texture_id == 0) {
        renderer_set_error("Failed to generate procedural texture: %s", type);
        return NULL;
    }
    
    /* Create texture handle */
    renderer_texture_t *texture = malloc(sizeof(renderer_texture_t));
    if (!texture) {
        renderer_set_error("Failed to allocate texture handle");
        glDeleteTextures(1, &texture_id);
        return NULL;
    }
    
    texture->context = ctx;
    texture->texture_id = texture_id;
    texture->width = size;
    texture->height = size;
    texture->memory_size = size * size * 4; /* RGBA */
    
    /* Update memory estimate */
    ctx->gpu_memory_estimate += texture->memory_size;
    
    if (ctx->enable_debug) {
        fprintf(stderr, "[texture] Created procedural texture: %s (%dx%d, ID=%u)\n",
                type, size, size, texture_id);
    }
    
    return texture;
}

void renderer_texture_destroy(renderer_texture_t *texture) {
    if (!texture) {
        return;
    }
    
    renderer_context_t *ctx = texture->context;
    
    if (ctx && ctx->enable_debug) {
        fprintf(stderr, "[texture] Destroying texture ID=%u\n", texture->texture_id);
    }
    
    /* Make context current */
    if (ctx && ctx->egl_context != EGL_NO_CONTEXT) {
        eglMakeCurrent(ctx->egl_display, EGL_NO_SURFACE, EGL_NO_SURFACE, ctx->egl_context);
        
        /* Delete OpenGL texture */
        if (texture->texture_id != 0) {
            glDeleteTextures(1, &texture->texture_id);
        }
        
        /* Update memory estimate */
        if (ctx->gpu_memory_estimate >= texture->memory_size) {
            ctx->gpu_memory_estimate -= texture->memory_size;
        }
    }
    
    /* Free texture handle */
    free(texture);
}