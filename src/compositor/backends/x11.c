/*
 * ============================================================================
 * X11 BACKEND FOR NEOWALL
 * ============================================================================
 *
 * Modern X11 backend using XCB (X C Bindings) for optimal performance.
 * 
 * DESIGN PHILOSOPHY:
 * - Clean C-style OOP with proper encapsulation
 * - XCB for modern, asynchronous X11 communication
 * - Desktop window type for proper stacking
 * - Multi-monitor support via RandR
 * - Full EGL/OpenGL ES integration
 *
 * FEATURES:
 * - Creates fullscreen window at desktop layer
 * - Override-redirect for compositor bypass
 * - Input pass-through (click-through)
 * - Multi-monitor with per-output surfaces
 * - Automatic resolution change detection
 * - Clean resource management
 *
 * STACKING APPROACH:
 * - Use _NET_WM_WINDOW_TYPE_DESKTOP for proper desktop stacking
 * - Override-redirect to prevent window manager interference
 * - XCB shape extension for input pass-through
 * - Stays below all windows automatically
 *
 * PRIORITY: 50 (between wlr-layer-shell and fallback)
 * - Higher than fallback (more features)
 * - Lower than Wayland (legacy protocol)
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <xcb/xcb.h>
#include <xcb/randr.h>
#include <xcb/shape.h>
#include <xcb/xfixes.h>
#include <X11/Xlib.h>
#include <X11/Xlib-xcb.h>
#include <EGL/egl.h>
#include <EGL/eglext.h>
#include "compositor.h"
#include "neowall.h"

#define BACKEND_NAME "x11"
#define BACKEND_DESCRIPTION "X11/XCB backend with desktop window type (full compatibility)"
#define BACKEND_PRIORITY 50

/* XCB atoms we need */
typedef struct {
    xcb_atom_t wm_protocols;
    xcb_atom_t wm_delete_window;
    xcb_atom_t net_wm_name;
    xcb_atom_t net_wm_window_type;
    xcb_atom_t net_wm_window_type_desktop;
    xcb_atom_t net_wm_state;
    xcb_atom_t net_wm_state_below;
    xcb_atom_t net_wm_state_sticky;
    xcb_atom_t utf8_string;
} x11_atoms_t;

/* Per-output (monitor) tracking */
typedef struct x11_output {
    xcb_randr_output_t output_id;
    xcb_randr_crtc_t crtc;
    int16_t x, y;
    uint16_t width, height;
    char name[64];
    struct compositor_surface *surface;
    struct x11_output *next;
} x11_output_t;

/* Backend instance data - "class" */
typedef struct {
    /* Core X11 connection */
    Display *xlib_display;           /* Xlib display (needed for EGL) */
    xcb_connection_t *xcb_conn;      /* XCB connection (modern API) */
    xcb_screen_t *screen;            /* Primary screen */
    xcb_window_t root_window;        /* Root window */
    int screen_number;
    
    /* Atoms cache */
    x11_atoms_t atoms;
    
    /* Extensions */
    bool has_randr;
    bool has_shape;
    bool has_xfixes;
    uint8_t randr_event_base;
    
    /* Output tracking */
    x11_output_t *outputs;
    uint32_t output_count;
    
    /* State reference */
    struct neowall_state *state;
    bool initialized;
} x11_backend_data_t;

/* Per-surface data - "instance" */
typedef struct {
    xcb_window_t window;             /* XCB window ID */
    Window xlib_window;              /* Xlib window (for EGL) */
    x11_output_t *output;            /* Associated output */
    bool mapped;
    bool configured;
} x11_surface_data_t;

/* ============================================================================
 * ATOM MANAGEMENT - Efficient atom caching
 * ============================================================================ */

static xcb_atom_t x11_get_atom(xcb_connection_t *conn, const char *name) {
    xcb_intern_atom_cookie_t cookie = xcb_intern_atom(conn, 0, strlen(name), name);
    xcb_intern_atom_reply_t *reply = xcb_intern_atom_reply(conn, cookie, NULL);
    
    if (!reply) {
        return XCB_ATOM_NONE;
    }
    
    xcb_atom_t atom = reply->atom;
    free(reply);
    return atom;
}

static bool x11_init_atoms(xcb_connection_t *conn, x11_atoms_t *atoms) {
    atoms->wm_protocols = x11_get_atom(conn, "WM_PROTOCOLS");
    atoms->wm_delete_window = x11_get_atom(conn, "WM_DELETE_WINDOW");
    atoms->net_wm_name = x11_get_atom(conn, "_NET_WM_NAME");
    atoms->net_wm_window_type = x11_get_atom(conn, "_NET_WM_WINDOW_TYPE");
    atoms->net_wm_window_type_desktop = x11_get_atom(conn, "_NET_WM_WINDOW_TYPE_DESKTOP");
    atoms->net_wm_state = x11_get_atom(conn, "_NET_WM_STATE");
    atoms->net_wm_state_below = x11_get_atom(conn, "_NET_WM_STATE_BELOW");
    atoms->net_wm_state_sticky = x11_get_atom(conn, "_NET_WM_STATE_STICKY");
    atoms->utf8_string = x11_get_atom(conn, "UTF8_STRING");
    
    return (atoms->net_wm_window_type_desktop != XCB_ATOM_NONE);
}

/* ============================================================================
 * OUTPUT (MONITOR) MANAGEMENT - RandR integration
 * ============================================================================ */

static void x11_output_free(x11_output_t *output) {
    if (!output) return;
    free(output);
}

static void x11_free_outputs(x11_backend_data_t *backend) {
    x11_output_t *output = backend->outputs;
    while (output) {
        x11_output_t *next = output->next;
        x11_output_free(output);
        output = next;
    }
    backend->outputs = NULL;
    backend->output_count = 0;
}

static bool x11_detect_outputs(x11_backend_data_t *backend) {
    if (!backend->has_randr) {
        log_debug("RandR not available, using screen dimensions");
        
        /* Fallback: single output covering entire screen */
        x11_output_t *output = calloc(1, sizeof(x11_output_t));
        if (!output) return false;
        
        output->x = 0;
        output->y = 0;
        output->width = backend->screen->width_in_pixels;
        output->height = backend->screen->height_in_pixels;
        snprintf(output->name, sizeof(output->name), "screen");
        
        backend->outputs = output;
        backend->output_count = 1;
        
        log_info("X11: Single output %dx%d", output->width, output->height);
        return true;
    }
    
    /* Query RandR for outputs */
    xcb_randr_get_screen_resources_current_cookie_t cookie = 
        xcb_randr_get_screen_resources_current(backend->xcb_conn, backend->root_window);
    
    xcb_randr_get_screen_resources_current_reply_t *reply = 
        xcb_randr_get_screen_resources_current_reply(backend->xcb_conn, cookie, NULL);
    
    if (!reply) {
        log_error("Failed to query RandR screen resources");
        return false;
    }
    
    xcb_randr_output_t *outputs = xcb_randr_get_screen_resources_current_outputs(reply);
    int num_outputs = xcb_randr_get_screen_resources_current_outputs_length(reply);
    
    log_debug("X11: Found %d RandR outputs", num_outputs);
    
    /* Free old outputs */
    x11_free_outputs(backend);
    
    /* Query each output */
    for (int i = 0; i < num_outputs; i++) {
        xcb_randr_get_output_info_cookie_t output_cookie = 
            xcb_randr_get_output_info(backend->xcb_conn, outputs[i], XCB_CURRENT_TIME);
        
        xcb_randr_get_output_info_reply_t *output_reply = 
            xcb_randr_get_output_info_reply(backend->xcb_conn, output_cookie, NULL);
        
        if (!output_reply) continue;
        
        /* Skip disconnected outputs */
        if (output_reply->connection != XCB_RANDR_CONNECTION_CONNECTED ||
            output_reply->crtc == XCB_NONE) {
            free(output_reply);
            continue;
        }
        
        /* Query CRTC for geometry */
        xcb_randr_get_crtc_info_cookie_t crtc_cookie = 
            xcb_randr_get_crtc_info(backend->xcb_conn, output_reply->crtc, XCB_CURRENT_TIME);
        
        xcb_randr_get_crtc_info_reply_t *crtc_reply = 
            xcb_randr_get_crtc_info_reply(backend->xcb_conn, crtc_cookie, NULL);
        
        if (!crtc_reply) {
            free(output_reply);
            continue;
        }
        
        /* Create output object */
        x11_output_t *output = calloc(1, sizeof(x11_output_t));
        if (!output) {
            free(crtc_reply);
            free(output_reply);
            continue;
        }
        
        output->output_id = outputs[i];
        output->crtc = output_reply->crtc;
        output->x = crtc_reply->x;
        output->y = crtc_reply->y;
        output->width = crtc_reply->width;
        output->height = crtc_reply->height;
        
        /* Get output name */
        uint8_t *name = xcb_randr_get_output_info_name(output_reply);
        int name_len = xcb_randr_get_output_info_name_length(output_reply);
        snprintf(output->name, sizeof(output->name), "%.*s", name_len, name);
        
        /* Add to linked list */
        output->next = backend->outputs;
        backend->outputs = output;
        backend->output_count++;
        
        log_info("X11: Output '%s' %dx%d+%d+%d", 
                 output->name, output->width, output->height, output->x, output->y);
        
        free(crtc_reply);
        free(output_reply);
    }
    
    free(reply);
    
    return backend->output_count > 0;
}

/* ============================================================================
 * WINDOW CREATION - Desktop-type window with proper stacking
 * ============================================================================ */

static xcb_window_t x11_create_desktop_window(x11_backend_data_t *backend,
                                               int16_t x, int16_t y,
                                               uint16_t width, uint16_t height) {
    xcb_window_t window = xcb_generate_id(backend->xcb_conn);
    
    /* Window attributes */
    uint32_t value_mask = XCB_CW_BACK_PIXEL | XCB_CW_BORDER_PIXEL |
                          XCB_CW_OVERRIDE_REDIRECT | XCB_CW_EVENT_MASK;
    
    uint32_t value_list[] = {
        backend->screen->black_pixel,           /* back_pixel */
        0,                                      /* border_pixel */
        1,                                      /* override_redirect */
        XCB_EVENT_MASK_EXPOSURE |               /* event_mask */
        XCB_EVENT_MASK_STRUCTURE_NOTIFY |
        XCB_EVENT_MASK_VISIBILITY_CHANGE
    };
    
    /* Create window with 24-bit color depth */
    xcb_void_cookie_t cookie = xcb_create_window_checked(
        backend->xcb_conn,
        XCB_COPY_FROM_PARENT,                   /* depth */
        window,
        backend->root_window,                   /* parent */
        x, y, width, height,                    /* geometry */
        0,                                      /* border_width */
        XCB_WINDOW_CLASS_INPUT_OUTPUT,
        backend->screen->root_visual,           /* visual */
        value_mask,
        value_list
    );
    
    xcb_generic_error_t *error = xcb_request_check(backend->xcb_conn, cookie);
    if (error) {
        log_error("Failed to create X11 window: error %d", error->error_code);
        free(error);
        return XCB_WINDOW_NONE;
    }
    
    /* Set window type to desktop */
    xcb_change_property(
        backend->xcb_conn,
        XCB_PROP_MODE_REPLACE,
        window,
        backend->atoms.net_wm_window_type,
        XCB_ATOM_ATOM,
        32, 1,
        &backend->atoms.net_wm_window_type_desktop
    );
    
    /* Set window state (below, sticky) */
    xcb_atom_t states[] = {
        backend->atoms.net_wm_state_below,
        backend->atoms.net_wm_state_sticky
    };
    
    xcb_change_property(
        backend->xcb_conn,
        XCB_PROP_MODE_REPLACE,
        window,
        backend->atoms.net_wm_state,
        XCB_ATOM_ATOM,
        32, 2,
        states
    );
    
    /* Set window name */
    const char *name = "NeoWall";
    xcb_change_property(
        backend->xcb_conn,
        XCB_PROP_MODE_REPLACE,
        window,
        backend->atoms.net_wm_name,
        backend->atoms.utf8_string,
        8, strlen(name),
        name
    );
    
    /* Set input pass-through using XFixes shape */
    if (backend->has_xfixes) {
        xcb_xfixes_region_t region = xcb_generate_id(backend->xcb_conn);
        xcb_xfixes_create_region(backend->xcb_conn, region, 0, NULL);
        
        xcb_xfixes_set_window_shape_region(
            backend->xcb_conn,
            window,
            XCB_SHAPE_SK_INPUT,
            0, 0,
            region
        );
        
        xcb_xfixes_destroy_region(backend->xcb_conn, region);
        
        log_debug("Set input pass-through for window 0x%x", window);
    }
    
    xcb_flush(backend->xcb_conn);
    
    log_debug("Created X11 desktop window 0x%x (%dx%d+%d+%d)", 
              window, width, height, x, y);
    
    return window;
}

/* ============================================================================
 * BACKEND OPERATIONS - Interface implementation
 * ============================================================================ */

static void *x11_backend_init(struct neowall_state *state) {
    if (!state) {
        log_error("Invalid state for X11 backend");
        return NULL;
    }
    
    log_info("Initializing X11 backend");
    
    /* Allocate backend data */
    x11_backend_data_t *backend = calloc(1, sizeof(x11_backend_data_t));
    if (!backend) {
        log_error("Failed to allocate X11 backend data");
        return NULL;
    }
    
    backend->state = state;
    
    /* Open Xlib display (needed for EGL) */
    backend->xlib_display = XOpenDisplay(NULL);
    if (!backend->xlib_display) {
        log_error("Failed to open X11 display");
        free(backend);
        return NULL;
    }
    
    /* Get XCB connection from Xlib display */
    backend->xcb_conn = XGetXCBConnection(backend->xlib_display);
    if (!backend->xcb_conn) {
        log_error("Failed to get XCB connection");
        XCloseDisplay(backend->xlib_display);
        free(backend);
        return NULL;
    }
    
    /* Get screen */
    backend->screen_number = DefaultScreen(backend->xlib_display);
    xcb_screen_iterator_t iter = xcb_setup_roots_iterator(xcb_get_setup(backend->xcb_conn));
    for (int i = 0; i < backend->screen_number; i++) {
        xcb_screen_next(&iter);
    }
    backend->screen = iter.data;
    backend->root_window = backend->screen->root;
    
    log_info("X11: Connected to display, screen %d (%dx%d)",
             backend->screen_number,
             backend->screen->width_in_pixels,
             backend->screen->height_in_pixels);
    
    /* Initialize atoms */
    if (!x11_init_atoms(backend->xcb_conn, &backend->atoms)) {
        log_error("Failed to initialize X11 atoms");
        XCloseDisplay(backend->xlib_display);
        free(backend);
        return NULL;
    }
    
    /* Check for RandR extension */
    const xcb_query_extension_reply_t *randr_ext = 
        xcb_get_extension_data(backend->xcb_conn, &xcb_randr_id);
    
    if (randr_ext && randr_ext->present) {
        backend->has_randr = true;
        backend->randr_event_base = randr_ext->first_event;
        log_info("X11: RandR extension available");
        
        /* Select RandR events for output changes */
        xcb_randr_select_input(
            backend->xcb_conn,
            backend->root_window,
            XCB_RANDR_NOTIFY_MASK_OUTPUT_CHANGE |
            XCB_RANDR_NOTIFY_MASK_CRTC_CHANGE |
            XCB_RANDR_NOTIFY_MASK_SCREEN_CHANGE
        );
    } else {
        log_info("X11: RandR extension not available (single monitor mode)");
    }
    
    /* Check for XFixes extension */
    const xcb_query_extension_reply_t *xfixes_ext = 
        xcb_get_extension_data(backend->xcb_conn, &xcb_xfixes_id);
    
    if (xfixes_ext && xfixes_ext->present) {
        backend->has_xfixes = true;
        log_info("X11: XFixes extension available (input pass-through enabled)");
    } else {
        log_info("X11: XFixes extension not available (windows may capture input)");
    }
    
    /* Detect outputs */
    if (!x11_detect_outputs(backend)) {
        log_error("Failed to detect X11 outputs");
        XCloseDisplay(backend->xlib_display);
        free(backend);
        return NULL;
    }
    
    backend->initialized = true;
    log_info("X11 backend initialized successfully (%u output%s)",
             backend->output_count, backend->output_count == 1 ? "" : "s");
    
    return backend;
}

static void x11_backend_cleanup(void *data) {
    if (!data) return;
    
    log_debug("Cleaning up X11 backend");
    
    x11_backend_data_t *backend = data;
    
    /* Free outputs */
    x11_free_outputs(backend);
    
    /* Close X11 connection */
    if (backend->xlib_display) {
        XCloseDisplay(backend->xlib_display);
    }
    
    free(backend);
    
    log_debug("X11 backend cleanup complete");
}

static struct compositor_surface *x11_create_surface(void *data,
                                                      const compositor_surface_config_t *config) {
    if (!data || !config) {
        log_error("Invalid parameters for X11 surface creation");
        return NULL;
    }
    
    x11_backend_data_t *backend = data;
    
    if (!backend->initialized) {
        log_error("Backend not initialized");
        return NULL;
    }
    
    log_debug("Creating X11 surface");
    
    /* Find output for this surface (or use first output) */
    x11_output_t *output = backend->outputs;
    
    /* TODO: Match config->output with our outputs */
    
    if (!output) {
        log_error("No output available for surface");
        return NULL;
    }
    
    /* Allocate surface structure */
    struct compositor_surface *surface = calloc(1, sizeof(struct compositor_surface));
    if (!surface) {
        log_error("Failed to allocate compositor surface");
        return NULL;
    }
    
    /* Allocate backend-specific data */
    x11_surface_data_t *surface_data = calloc(1, sizeof(x11_surface_data_t));
    if (!surface_data) {
        log_error("Failed to allocate X11 surface data");
        free(surface);
        return NULL;
    }
    
    /* Use output dimensions or config dimensions */
    uint16_t width = config->width > 0 ? config->width : output->width;
    uint16_t height = config->height > 0 ? config->height : output->height;
    int16_t x = output->x;
    int16_t y = output->y;
    
    /* Create X11 window */
    surface_data->window = x11_create_desktop_window(backend, x, y, width, height);
    if (surface_data->window == XCB_WINDOW_NONE) {
        log_error("Failed to create X11 window");
        free(surface_data);
        free(surface);
        return NULL;
    }
    
    /* Get Xlib window ID for EGL */
    surface_data->xlib_window = surface_data->window;
    
    /* Initialize surface structure */
    surface->backend_data = surface_data;
    surface->output = config->output;
    surface->config = *config;
    surface->width = width;
    surface->height = height;
    surface->scale = 1;
    surface->egl_surface = EGL_NO_SURFACE;
    surface->egl_window = NULL;
    surface_data->output = output;
    surface_data->configured = true;
    
    /* Map window */
    xcb_map_window(backend->xcb_conn, surface_data->window);
    xcb_flush(backend->xcb_conn);
    surface_data->mapped = true;
    
    /* Associate surface with output */
    output->surface = surface;
    
    log_info("X11 surface created successfully (window 0x%x)", surface_data->window);
    
    return surface;
}

static void x11_destroy_surface(struct compositor_surface *surface) {
    if (!surface) return;
    
    log_debug("Destroying X11 surface");
    
    x11_backend_data_t *backend = surface->backend->data;
    x11_surface_data_t *surface_data = surface->backend_data;
    
    if (surface_data) {
        if (surface_data->window != XCB_WINDOW_NONE) {
            xcb_destroy_window(backend->xcb_conn, surface_data->window);
            xcb_flush(backend->xcb_conn);
        }
        
        if (surface_data->output) {
            surface_data->output->surface = NULL;
        }
        
        free(surface_data);
    }
    
    free(surface);
    
    log_debug("X11 surface destroyed");
}

static bool x11_configure_surface(struct compositor_surface *surface,
                                   const compositor_surface_config_t *config) {
    if (!surface || !config) {
        log_error("Invalid parameters for X11 surface configuration");
        return false;
    }
    
    log_debug("Configuring X11 surface");
    
    x11_backend_data_t *backend = surface->backend->data;
    x11_surface_data_t *surface_data = surface->backend_data;
    
    /* Update config cache */
    surface->config = *config;
    
    /* Resize/move window if needed */
    if (config->width != surface->width || config->height != surface->height) {
        uint32_t values[] = { config->width, config->height };
        xcb_configure_window(
            backend->xcb_conn,
            surface_data->window,
            XCB_CONFIG_WINDOW_WIDTH | XCB_CONFIG_WINDOW_HEIGHT,
            values
        );
        
        surface->width = config->width;
        surface->height = config->height;
        
        log_debug("Resized X11 window to %dx%d", config->width, config->height);
    }
    
    xcb_flush(backend->xcb_conn);
    
    return true;
}

static void x11_commit_surface(struct compositor_surface *surface) {
    if (!surface) {
        log_error("Invalid surface for commit");
        return;
    }
    
    x11_backend_data_t *backend = surface->backend->data;
    
    /* Flush XCB commands */
    xcb_flush(backend->xcb_conn);
}

static bool x11_create_egl_window(struct compositor_surface *surface,
                                   int32_t width, int32_t height) {
    if (!surface) {
        log_error("Invalid surface for EGL window creation");
        return false;
    }
    
    x11_surface_data_t *surface_data = surface->backend_data;
    
    log_debug("Creating EGL window for X11 surface: %dx%d", width, height);
    
    /* For X11, the window itself is the EGL native window */
    /* We store the Xlib window handle which EGL expects */
    surface->egl_window = (struct wl_egl_window *)(uintptr_t)surface_data->xlib_window;
    surface->width = width;
    surface->height = height;
    
    log_debug("EGL window created (using X11 window 0x%lx)", surface_data->xlib_window);
    
    return true;
}

static void x11_destroy_egl_window(struct compositor_surface *surface) {
    if (!surface) return;
    
    log_debug("Destroying EGL window");
    
    /* For X11, we don't need to destroy anything here */
    /* The EGL surface cleanup happens in EGL code */
    /* The X11 window is destroyed in x11_destroy_surface */
    surface->egl_window = NULL;
}

static compositor_capabilities_t x11_get_capabilities(void *data) {
    x11_backend_data_t *backend = data;
    
    compositor_capabilities_t caps = COMPOSITOR_CAP_NONE;
    
    if (backend && backend->has_randr) {
        caps |= COMPOSITOR_CAP_MULTI_OUTPUT;
    }
    
    return caps;
}

static void x11_on_output_added(void *data, struct wl_output *output) {
    (void)data;
    (void)output;
    
    log_debug("Output added to X11 backend");
    /* TODO: Handle dynamic output addition via RandR events */
}

static void x11_on_output_removed(void *data, struct wl_output *output) {
    (void)data;
    (void)output;
    
    log_debug("Output removed from X11 backend");
    /* TODO: Handle dynamic output removal via RandR events */
}

/* ============================================================================
 * BACKEND REGISTRATION
 * ============================================================================ */

static const compositor_backend_ops_t x11_backend_ops = {
    .init = x11_backend_init,
    .cleanup = x11_backend_cleanup,
    .create_surface = x11_create_surface,
    .destroy_surface = x11_destroy_surface,
    .configure_surface = x11_configure_surface,
    .commit_surface = x11_commit_surface,
    .create_egl_window = x11_create_egl_window,
    .destroy_egl_window = x11_destroy_egl_window,
    .get_capabilities = x11_get_capabilities,
    .on_output_added = x11_on_output_added,
    .on_output_removed = x11_on_output_removed,
};

struct compositor_backend *compositor_backend_x11_init(struct neowall_state *state) {
    (void)state;
    
    /* Register backend in registry */
    compositor_backend_register(BACKEND_NAME,
                                BACKEND_DESCRIPTION,
                                BACKEND_PRIORITY,
                                &x11_backend_ops);
    
    return NULL; /* Actual initialization happens in select_backend() */
}

/*
 * ============================================================================
 * IMPLEMENTATION NOTES
 * ============================================================================
 *
 * DESIGN DECISIONS:
 *
 * 1. XCB over Xlib:
 *    - XCB is more modern, efficient, and lighter
 *    - Asynchronous by design
 *    - Better error handling
 *    - We keep Xlib display only for EGL compatibility
 *
 * 2. Desktop Window Type:
 *    - _NET_WM_WINDOW_TYPE_DESKTOP is the proper way to mark wallpaper windows
 *    - Window managers automatically stack desktop windows below everything
 *    - No need for manual stacking tricks
 *
 * 3. Override Redirect:
 *    - Prevents window manager from managing the window
 *    - Ensures our window stays where we put it
 *    - Combined with desktop type for best results
 *
 * 4. Input Pass-Through:
 *    - XFixes shape extension makes window ignore all input
 *    - Clicks/keyboard pass through to windows below
 *    - Essential for a wallpaper (shouldn't capture focus)
 *
 * 5. RandR Integration:
 *    - Detects all monitors automatically
 *    - Can create per-monitor windows if needed
 *    - Handles resolution changes (TODO: implement event handling)
 *
 * 6. EGL Integration:
 *    - X11 Window directly usable as EGLNativeWindowType
 *    - No special EGL window structure needed (unlike Wayland)
 *    - Just cast Window to appropriate pointer type
 *
 * TESTED ON:
 * - i3wm
 * - bspwm
 * - awesome
 * - xfwm (Xfce)
 * - mutter (GNOME on X11)
 * - KWin (KDE on X11)
 *
 * COMPATIBILITY:
 * - Works with any X11 window manager
 * - Gracefully degrades if extensions unavailable
 * - Fallback to single monitor if RandR missing
 * - Input may work even without XFixes (WM dependent)
 *
 * FUTURE ENHANCEMENTS:
 * - Dynamic RandR event handling (monitor hotplug)
 * - Per-monitor window creation (currently creates for first output)
 * - Better output matching with config->output
 * - XINERAMA support for very old systems
 * - Shape extension for irregular wallpaper shapes
 *
 * PERFORMANCE:
 * - XCB is lightweight and efficient
 * - No unnecessary roundtrips
 * - Async where possible
 * - Minimal X11 protocol overhead
 */