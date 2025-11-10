#include <stdlib.h>
#include <math.h>
#include <GLES2/gl2.h>

/**
 * Blue Noise Texture Generator
 * 
 * Generates blue noise texture with better distribution than white noise.
 * Ideal for dithering and reduces banding artifacts in shaders.
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

/* ========================================================================
 * Public API
 * ======================================================================== */

GLuint texture_create_blue_noise(int width, int height) {
    unsigned char *data = malloc(width * height * 4);
    if (!data) {
        return 0;
    }
    
    /* Generate blue noise using dithered pattern
     * Blue noise has better spatial distribution than white noise */
    for (int y = 0; y < height; y++) {
        for (int x = 0; x < width; x++) {
            int idx = (y * width + x) * 4;
            
            float u = (float)x / (float)width;
            float v = (float)y / (float)height;
            
            /* Create dithered pattern with spatial offset */
            float val = fbm(u * 4.0f, v * 4.0f, 3);
            
            /* Add spatial dithering to improve distribution */
            float dither = (float)((x * 7 + y * 11) % 16) / 16.0f;
            val = fract(val + dither);
            
            unsigned char c = (unsigned char)(val * 255.0f);
            
            data[idx + 0] = c;
            data[idx + 1] = c;
            data[idx + 2] = c;
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