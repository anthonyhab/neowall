#include <stdlib.h>
#include <math.h>
#include <GLES2/gl2.h>

/**
 * Wood Grain Texture Generator
 * 
 * Generates procedural wood grain pattern with realistic rings.
 * Useful for natural-looking backgrounds.
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

GLuint texture_create_wood(int width, int height) {
    unsigned char *data = malloc(width * height * 4);
    if (!data) {
        return 0;
    }
    
    /* Generate wood grain pattern */
    for (int y = 0; y < height; y++) {
        for (int x = 0; x < width; x++) {
            int idx = (y * width + x) * 4;
            
            float u = (float)x / (float)width;
            float v = (float)y / (float)height;
            
            /* Distance from center for radial rings */
            float dist = sqrtf((u - 0.5f) * (u - 0.5f) + (v - 0.5f) * (v - 0.5f));
            
            /* Create ring pattern with noise variation */
            float rings = sinf(dist * 30.0f + fbm(u * 2.0f, v * 2.0f, 3) * 3.0f);
            
            /* Convert to wood color range (brown tones) */
            float wood = (rings + 1.0f) * 0.5f;
            wood = wood * 0.4f + 0.3f; /* Normalize to brown range */
            
            /* Add subtle grain detail */
            float grain = fbm(u * 10.0f, v * 10.0f, 2) * 0.1f;
            wood += grain;
            
            /* Clamp values */
            wood = fmaxf(0.0f, fminf(1.0f, wood));
            
            /* Brown color palette */
            data[idx + 0] = (unsigned char)(wood * 180.0f + 50.0f);  /* Red */
            data[idx + 1] = (unsigned char)(wood * 120.0f + 30.0f);  /* Green */
            data[idx + 2] = (unsigned char)(wood * 60.0f + 10.0f);   /* Blue */
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