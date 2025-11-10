#include "output.h"
#include "../renderer/renderer.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/**
 * Output Management Implementation
 * 
 * Handles individual display outputs (monitors) including lifecycle,
 * configuration, and wallpaper management.
 */

/* ========================================================================
 * Helper Functions
 * ======================================================================== */

/**
 * Get output identifier (prefers connector name over model)
 */
const char *output_get_identifier(const struct output_state *output) {
    if (!output) {
        return "unknown";
    }
    
    if (output->connector_name[0] != '\0') {
        return output->connector_name;
    }
    
    if (output->model[0] != '\0') {
        return output->model;
    }
    
    return "unknown";
}

/* ========================================================================
 * Lifecycle Functions
 * ======================================================================== */

struct output_state *output_create(struct neowall_state *state,
                                   struct wl_output *output,
                                   uint32_t name) {
    if (!state || !output) {
        fprintf(stderr, "[output] Invalid parameters for output_create\n");
        return NULL;
    }
    
    /* Allocate output state */
    struct output_state *out = calloc(1, sizeof(struct output_state));
    if (!out) {
        fprintf(stderr, "[output] Failed to allocate output state\n");
        return NULL;
    }
    
    /* Initialize basic fields */
    out->output = output;
    out->name = name;
    out->state = state;
    out->scale = 1;
    out->configured = false;
    out->needs_redraw = true;
    out->last_frame_time = 0;
    out->frames_rendered = 0;
    
    /* Initialize strings */
    out->make[0] = '\0';
    out->model[0] = '\0';
    out->connector_name[0] = '\0';
    
    /* Initialize linked list */
    out->next = NULL;
    
    fprintf(stderr, "[output] Created output state for ID %u\n", name);
    
    return out;
}

void output_destroy(struct output_state *output) {
    if (!output) {
        return;
    }
    
    fprintf(stderr, "[output] Destroying output %s\n", output_get_identifier(output));
    
    /* Clean up compositor surface */
    if (output->compositor_surface) {
        /* Compositor surface cleanup handled by compositor backend */
        output->compositor_surface = NULL;
    }
    
    /* Clean up xdg output */
    if (output->xdg_output) {
        /* xdg_output cleanup */
        output->xdg_output = NULL;
    }
    
    /* Clean up Wayland output */
    if (output->output) {
        wl_output_destroy(output->output);
        output->output = NULL;
    }
    
    /* Clean up configuration */
    if (output->config) {
        /* Config cleanup handled by config module */
        output->config = NULL;
    }
    
    /* Free the output state */
    free(output);
}

/* ========================================================================
 * Configuration Functions
 * ======================================================================== */

bool output_configure_compositor_surface(struct output_state *output) {
    if (!output || !output->state) {
        fprintf(stderr, "[output] Invalid output for configure_compositor_surface\n");
        return false;
    }
    
    /* This will be implemented by compositor abstraction layer */
    /* For now, just mark as configured if we have dimensions */
    if (output->width > 0 && output->height > 0) {
        output->configured = true;
        fprintf(stderr, "[output] Configured %s (%dx%d)\n",
                output_get_identifier(output),
                output->width, output->height);
        return true;
    }
    
    return false;
}

bool output_create_egl_surface(struct output_state *output) {
    if (!output) {
        fprintf(stderr, "[output] Invalid output for create_egl_surface\n");
        return false;
    }
    
    /* EGL surface creation handled by renderer */
    fprintf(stderr, "[output] EGL surface creation requested for %s\n",
            output_get_identifier(output));
    
    return true;
}

bool output_apply_config(struct output_state *output,
                        struct wallpaper_config *config) {
    if (!output || !config) {
        fprintf(stderr, "[output] Invalid parameters for apply_config\n");
        return false;
    }
    
    /* Store config reference */
    output->config = config;
    
    /* Mark for redraw */
    output->needs_redraw = true;
    
    fprintf(stderr, "[output] Applied config to %s\n",
            output_get_identifier(output));
    
    return true;
}

void output_apply_deferred_config(struct output_state *output) {
    if (!output) {
        return;
    }
    
    if (!output->configured) {
        fprintf(stderr, "[output] Cannot apply deferred config - output not configured\n");
        return;
    }
    
    if (output->config) {
        /* Reapply configuration now that output is ready */
        output_apply_config(output, output->config);
    }
}

/* ========================================================================
 * Wallpaper Management
 * ======================================================================== */

void output_set_wallpaper(struct output_state *output, const char *path) {
    if (!output || !path) {
        fprintf(stderr, "[output] Invalid parameters for set_wallpaper\n");
        return;
    }
    
    fprintf(stderr, "[output] Setting wallpaper for %s: %s\n",
            output_get_identifier(output), path);
    
    /* This will be implemented with renderer integration */
    output->needs_redraw = true;
}

void output_set_shader(struct output_state *output, const char *shader_path) {
    if (!output || !shader_path) {
        fprintf(stderr, "[output] Invalid parameters for set_shader\n");
        return;
    }
    
    fprintf(stderr, "[output] Setting shader for %s: %s\n",
            output_get_identifier(output), shader_path);
    
    /* This will be implemented with renderer integration */
    output->needs_redraw = true;
}

void output_cycle_wallpaper(struct output_state *output) {
    if (!output) {
        fprintf(stderr, "[output] Invalid output for cycle_wallpaper\n");
        return;
    }
    
    if (!output->config) {
        fprintf(stderr, "[output] No config for cycling wallpaper\n");
        return;
    }
    
    fprintf(stderr, "[output] Cycling wallpaper for %s\n",
            output_get_identifier(output));
    
    /* This will be implemented with config integration */
    output->needs_redraw = true;
}

bool output_should_cycle(struct output_state *output, uint64_t current_time) {
    if (!output || !output->config) {
        return false;
    }
    
    /* Check if cycling is enabled */
    /* This will be implemented with config integration */
    (void)current_time; /* Unused for now */
    
    return false;
}

void output_preload_next_wallpaper(struct output_state *output) {
    if (!output) {
        return;
    }
    
    fprintf(stderr, "[output] Preloading next wallpaper for %s\n",
            output_get_identifier(output));
    
    /* This will be implemented with renderer integration */
}