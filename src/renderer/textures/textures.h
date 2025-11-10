#ifndef RENDERER_TEXTURES_H
#define RENDERER_TEXTURES_H

#include <stdint.h>
#include <GLES2/gl2.h>

/**
 * Texture Generation Interface
 * 
 * Provides both image-based texture loading and procedural texture generation
 * for use in the renderer.
 */

#ifdef __cplusplus
extern "C" {
#endif

/* ========================================================================
 * Procedural Texture Generators
 * ======================================================================== */

/**
 * Create RGBA noise texture
 * 
 * Generates a tileable RGBA noise texture using Fractal Brownian Motion.
 * Most common texture type used in Shadertoy shaders.
 * Each channel has independent noise patterns.
 * 
 * @param width Texture width in pixels
 * @param height Texture height in pixels
 * @return OpenGL texture ID, or 0 on failure
 */
GLuint texture_create_rgba_noise(int width, int height);

/**
 * Create grayscale noise texture
 * 
 * Generates high-quality multi-octave grayscale noise.
 * Useful for displacement maps, heightfields, and effects.
 * 
 * @param width Texture width in pixels
 * @param height Texture height in pixels
 * @return OpenGL texture ID, or 0 on failure
 */
GLuint texture_create_gray_noise(int width, int height);

/**
 * Create blue noise texture
 * 
 * Generates blue noise with better spatial distribution than white noise.
 * Ideal for dithering and reduces banding artifacts in shaders.
 * 
 * @param width Texture width in pixels
 * @param height Texture height in pixels
 * @return OpenGL texture ID, or 0 on failure
 */
GLuint texture_create_blue_noise(int width, int height);

/**
 * Create wood grain texture
 * 
 * Generates procedural wood grain pattern with realistic rings.
 * Useful for natural-looking backgrounds.
 * 
 * @param width Texture width in pixels
 * @param height Texture height in pixels
 * @return OpenGL texture ID, or 0 on failure
 */
GLuint texture_create_wood(int width, int height);

/**
 * Create abstract texture
 * 
 * Generates colorful abstract pattern using Voronoi-based cells.
 * Good for artistic/abstract backgrounds.
 * 
 * @param width Texture width in pixels
 * @param height Texture height in pixels
 * @return OpenGL texture ID, or 0 on failure
 */
GLuint texture_create_abstract(int width, int height);

/* ========================================================================
 * Texture Type Constants
 * ======================================================================== */

#define TEXTURE_TYPE_RGBA_NOISE "rgba_noise"
#define TEXTURE_TYPE_GRAY_NOISE "gray_noise"
#define TEXTURE_TYPE_BLUE_NOISE "blue_noise"
#define TEXTURE_TYPE_WOOD "wood"
#define TEXTURE_TYPE_ABSTRACT "abstract"
#define TEXTURE_TYPE_DEFAULT "default"

/* Default texture size for procedural generation */
#define TEXTURE_DEFAULT_SIZE 256

#ifdef __cplusplus
}
#endif

#endif /* RENDERER_TEXTURES_H */