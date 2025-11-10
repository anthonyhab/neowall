#include <stdlib.h>
#include <math.h>
#include <GLES2/gl2.h>

/**
 * Abstract Texture Generator
 * 
 * Generates colorful abstract pattern using Voronoi-based cells.
 * Good for artistic/abstract backgrounds.
 */

/* ========================================================================
 * Noise Functions
 * ======================================================================== */

static float fract(float x) {
    return x - floorf(x);
}

static float hash(float n) {
    return fract(sinf(n) * 43758.5453123f);
}

static float noise(float x, float y) {
    float px = floorf(x);
    float py = floorf(y);
    float fx = fract(x);
    float fy = fract(y);
    
    /* Smoothstep interpolation */
    fx = fx * fx * (3.0f - 2.0f * fx);
    fy = fy * fy * (3.0f - 2.0f * fy);
    
    float n = px + py * 157.0f;
    
    float a = hash(n + 0.0f);
    float b = hash(n + 1.0f);
    float c = hash(n + 157.0f);
    float d = hash(n + 158.0f);
    
    float res = a * (1.0f - fx) * (1.0f - fy) +
                b * fx * (1.0f - fy) +
                c * (1.0f - fx) * fy +
                d * fx * fy;
    
    return res;
}

static float fbm(float x, float y, int octaves) {
    float value = 0.0f;
    float amplitude = 0.5f;
    float frequency = 1.0f;
    
    for (int i = 0; i < octaves; i++) {
        value += amplitude * noise(x * frequency, y * frequency);
        frequency *= 2.0f;
        amplitude *= 0.5f;
    }
    
    return value;
}

/* Simple hash for 2D coordinates */
static float hash2d(float x, float y) {
    return fract(sinf(x * 12.9898f + y * 78.233f) * 43758.5453123f);
}

/* ========================================================================
 * Public API
 * ======================================================================== */

GLuint texture_create_abstract(int width, int height) {
    unsigned char *data = malloc(width * height * 4);
    if (!data) {
        return 0;
    }
    
    /* Generate colorful abstract pattern using layered noise */
    for (int y = 0; y < height; y++) {
        for (int x = 0; x < width; x++) {
            int idx = (y * width + x) * 4;
            
            float u = (float)x / (float)width;
            float v = (float)y / (float)height;
            
            /* Create colorful abstract pattern with independent channels */
            float r = fbm(u * 3.0f, v * 3.0f, 4);
            float g = fbm(u * 4.0f + 5.2f, v * 4.0f + 1.3f, 4);
            float b = fbm(u * 5.0f + 8.7f, v * 5.0f + 4.1f, 4);
            
            /* Add some Voronoi-like cell structure */
            float cell_scale = 8.0f;
            float cell_x = floorf(u * cell_scale);
            float cell_y = floorf(v * cell_scale);
            
            /* Find nearest cell center */
            float min_dist = 10.0f;
            float cell_color = 0.0f;
            
            for (int dy = -1; dy <= 1; dy++) {
                for (int dx = -1; dx <= 1; dx++) {
                    float cx = cell_x + (float)dx;
                    float cy = cell_y + (float)dy;
                    
                    /* Random cell center offset */
                    float offset_x = hash2d(cx, cy);
                    float offset_y = hash2d(cx + 1.0f, cy + 1.0f);
                    
                    float center_x = (cx + offset_x) / cell_scale;
                    float center_y = (cy + offset_y) / cell_scale;
                    
                    float dist = sqrtf((u - center_x) * (u - center_x) + 
                                      (v - center_y) * (v - center_y));
                    
                    if (dist < min_dist) {
                        min_dist = dist;
                        cell_color = hash2d(cx * 7.0f, cy * 3.0f);
                    }
                }
            }
            
            /* Blend cell structure with noise */
            float blend = 0.6f;
            r = r * blend + cell_color * (1.0f - blend);
            g = g * blend + hash2d(cell_color, 1.0f) * (1.0f - blend);
            b = b * blend + hash2d(cell_color, 2.0f) * (1.0f - blend);
            
            /* Enhance colors */
            r = fmaxf(0.0f, fminf(1.0f, r * 1.2f));
            g = fmaxf(0.0f, fminf(1.0f, g * 1.2f));
            b = fmaxf(0.0f, fminf(1.0f, b * 1.2f));
            
            data[idx + 0] = (unsigned char)(r * 255.0f);
            data[idx + 1] = (unsigned char)(g * 255.0f);
            data[idx + 2] = (unsigned char)(b * 255.0f);
            data[idx + 3] = 255;
        }
    }
    
    GLuint texture;
    glGenTextures(1, &texture);
    glBindTexture(GL_TEXTURE_2D, texture);
    
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, width, height, 0, 
                 GL_RGBA, GL_UNSIGNED_BYTE, data);
    
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
    
    free(data);
    
    return texture;
}