#if defined(__linux__)
#include "platform.h"

#include "vendor/wayland-client.h"
#include "vendor/xdg-shell-client-protocol.h"
#include "vendor/xdg-shell-client-protocol.c"
#include "vendor/presentation-time-client-protocol.h"
#include "vendor/presentation-time-client-protocol.c"
#include "vendor/tearing-control-v1-client-protocol.h"
#include "vendor/tearing-control-v1-client-protocol.c"

#include <string.h>
#include <stdlib.h>
#include <stdio.h>

#include <time.h>
#include <sys/mman.h>
#include <unistd.h>
#include <poll.h>

struct wayland_window {
    struct wl_display* display;
    struct wl_registry* registry;
    struct wl_surface* surface;
    struct wl_compositor* compositor;
    struct xdg_wm_base* xdg_wm_base;
    struct xdg_surface* xdg_surface;
    struct xdg_toplevel* xdg_toplevel;

    struct wp_presentation* presentation;
    struct wp_tearing_control_manager_v1* tearing_mgr;
    struct wp_tearing_control_v1* tearing;

    struct wl_seat* seat;
    struct wl_keyboard* kbd;
    struct wl_pointer* ptr;

    int kbd_repeat_rate;
    int kbd_repeat_delay;

    unsigned long long refresh_ns;
    unsigned long long last_present_ns;
    unsigned long long last_feedback_ns;
    unsigned long long phase_ns;
    double phase_alpha;

    int win_w, win_h;
    int mouse_x, mouse_y;
    int fullscreen_configured;
    int configured_once;
    int visible;
    int activated;
    int fullscreen;
    int tearing_requested;
    int running;

    KEYBOARD_CB on_key;
    MOUSE_CB on_mouse;
    void* callback_data;
};

static void wl_set_surface_opaque(struct wayland_window* w) {
    if (!w || !w->compositor || !w->surface) return;
    if (w->win_w <= 0 || w->win_h <= 0) return;

    struct wl_region* r = wl_compositor_create_region(w->compositor);
    if (!r) return;

    wl_region_add(r, 0, 0, w->win_w, w->win_h);
    wl_surface_set_opaque_region(w->surface, r);
    wl_region_destroy(r);
}

static void wl_request_tearing_async(struct wayland_window* w) {
    if (!w || !w->surface || !w->tearing_mgr || w->tearing) return;

    // w->tearing = wp_tearing_control_manager_v1_get_tearing_control(
    //     w->tearing_mgr, w->surface);
    // if (!w->tearing) return;
    //
    // wp_tearing_control_v1_set_presentation_hint(
    //     w->tearing,
    //     WP_TEARING_CONTROL_V1_PRESENTATION_HINT_ASYNC);

    w->tearing_requested = 1;
    pf_timestamp("requested async/tearing presentation");
}

static void kb_enter(void* data, struct wl_keyboard* k, unsigned int serial,
                     struct wl_surface* surface, struct wl_array* keys) {
    (void)data; (void)k; (void)serial; (void)surface; (void)keys;
}

static void kb_leave(void* data, struct wl_keyboard* k, unsigned int serial,
                     struct wl_surface* surface) {
    (void)data; (void)k; (void)serial; (void)surface;
}

static void kb_keymap(void* data, struct wl_keyboard* k, unsigned int format,
                      int fd, unsigned int size) {
    (void)data;
    (void)k;
    (void)format;
    (void)size;
    close(fd);
}

static void kb_modifiers(void* data, struct wl_keyboard* k, unsigned int serial,
                         unsigned int dep, unsigned int lat,
                         unsigned int lock, unsigned int group) {
    (void)data;
    (void)k;
    (void)serial;
    (void)dep;
    (void)lat;
    (void)lock;
    (void)group;
}

static void kb_repeat_info(void* data, struct wl_keyboard* k, int rate, int delay) {
    (void)k;
    struct wayland_window* w = data;
    if (!w) return;
    w->kbd_repeat_rate = rate;
    w->kbd_repeat_delay = delay;
}

static void kb_key(void* data, struct wl_keyboard* k, unsigned int serial,
                   unsigned int time, unsigned int key, unsigned int state) {
    (void)k;
    (void)serial;
    (void)time;

    struct wayland_window* w = data;
    if (!w || !w->on_key) return;

    enum BUTTON button = map_evdev_key_to_button(key);
    enum BUTTON_STATE s = (state == WL_KEYBOARD_KEY_STATE_PRESSED) ? PRESSED : RELEASED;
    w->on_key(w->callback_data, button, s);
}

static const struct wl_keyboard_listener* get_kb_listener(void) {
    static const struct wl_keyboard_listener kb_l = {
        .keymap = kb_keymap,
        .enter = kb_enter,
        .leave = kb_leave,
        .key = kb_key,
        .modifiers = kb_modifiers,
        .repeat_info = kb_repeat_info
    };
    return &kb_l;
}

static void ptr_enter(void* d, struct wl_pointer* p, unsigned int serial,
                      struct wl_surface* s, wl_fixed_t sx, wl_fixed_t sy) {
    (void)p; (void)serial; (void)s;
    struct wayland_window* w = d;
    if (!w) return;
    w->mouse_x = wl_fixed_to_int(sx);
    w->mouse_y = wl_fixed_to_int(sy);
    if (w->on_mouse) {
        w->on_mouse(w->callback_data, w->mouse_x, w->mouse_y, MOUSE_MOVED, 0u);
    }
}

static void ptr_leave(void* d, struct wl_pointer* p, unsigned int serial,
                      struct wl_surface* s) {
    (void)d; (void)p; (void)serial; (void)s;
}

static void ptr_motion(void* d, struct wl_pointer* p, unsigned int time,
                       wl_fixed_t sx, wl_fixed_t sy) {
    (void)p; (void)time;
    struct wayland_window* w = d;
    if (!w) return;
    w->mouse_x = wl_fixed_to_int(sx);
    w->mouse_y = wl_fixed_to_int(sy);
    if (w->on_mouse) {
        w->on_mouse(w->callback_data, w->mouse_x, w->mouse_y, MOUSE_MOVED, 0u);
    }
}

static void ptr_button(void* d, struct wl_pointer* p, unsigned int serial,
                       unsigned int time, unsigned int button, unsigned int state) {
    (void)p; (void)serial; (void)time;
    struct wayland_window* w = d;
    if (!w) return;

    unsigned int mb = BUTTON_UNKNOWN;
    if (button == 272) mb = MOUSE_LEFT;
    if (button == 273) mb = MOUSE_RIGHT;
    if (button == 274) mb = MOUSE_MIDDLE;

    if (w->on_mouse) {
        w->on_mouse(w->callback_data, w->mouse_x, w->mouse_y, mb, state);
    }
}

static void ptr_axis(void* d, struct wl_pointer* p, unsigned int time,
                     unsigned int axis, wl_fixed_t value) {
    (void)p; (void)time;
    struct wayland_window* w = d;
    if (!w) return;

    enum BUTTON b = BUTTON_UNKNOWN;
    if (axis == WL_POINTER_AXIS_VERTICAL_SCROLL) b = MOUSE_SCROLL;
    else if (axis == WL_POINTER_AXIS_HORIZONTAL_SCROLL) b = MOUSE_SCROLL_SIDE;

    if (b != BUTTON_UNKNOWN && w->on_mouse) {
        w->on_mouse(w->callback_data, w->mouse_x, w->mouse_y, b, wl_fixed_to_int(value));
    }
}

static void ptr_frame(void* d, struct wl_pointer* p) { (void)d; (void)p; }
static void ptr_axis_source(void* d, struct wl_pointer* p, unsigned int source) { (void)d; (void)p; (void)source; }
static void ptr_axis_stop(void* d, struct wl_pointer* p, unsigned int time, unsigned int axis) { (void)d; (void)p; (void)time; (void)axis; }
static void ptr_axis_discrete(void* d, struct wl_pointer* p, unsigned int axis, int discrete) { (void)d; (void)p; (void)axis; (void)discrete; }
static void ptr_axis_value120(void* d, struct wl_pointer* p, unsigned int axis, int value120) { (void)d; (void)p; (void)axis; (void)value120; }
static void ptr_axis_relative_direction(void* d, struct wl_pointer* p, unsigned int axis, unsigned int direction) { (void)d; (void)p; (void)axis; (void)direction; }

static const struct wl_pointer_listener* get_ptr_listener(void) {
    static const struct wl_pointer_listener ptr_l = {
        .enter = ptr_enter,
        .leave = ptr_leave,
        .motion = ptr_motion,
        .button = ptr_button,
        .axis = ptr_axis,
        .frame = ptr_frame,
        .axis_source = ptr_axis_source,
        .axis_stop = ptr_axis_stop,
        .axis_discrete = ptr_axis_discrete,
        .axis_value120 = ptr_axis_value120,
        .axis_relative_direction = ptr_axis_relative_direction
    };
    return &ptr_l;
}

static void seat_capabilities(void* d, struct wl_seat* s, unsigned int caps) {
    struct wayland_window* w = d;
    if (!w) return;

    if ((caps & WL_SEAT_CAPABILITY_KEYBOARD) && !w->kbd) {
        w->kbd = wl_seat_get_keyboard(s);
        wl_keyboard_add_listener(w->kbd, get_kb_listener(), w);
    }
    if ((caps & WL_SEAT_CAPABILITY_POINTER) && !w->ptr) {
        w->ptr = wl_seat_get_pointer(s);
        wl_pointer_add_listener(w->ptr, get_ptr_listener(), w);
    }
}

static void seat_name(void* d, struct wl_seat* s, const char* name) {
    (void)d; (void)s; (void)name;
}

static const struct wl_seat_listener* get_seat_listener(void) {
    static const struct wl_seat_listener seat_l = {
        .capabilities = seat_capabilities,
        .name = seat_name
    };
    return &seat_l;
}

static void xdg_ping(void* d, struct xdg_wm_base* b, unsigned int s) {
    (void)d;
    xdg_wm_base_pong(b, s);
}

static const struct xdg_wm_base_listener* get_xdg_wm_listener(void) {
    static const struct xdg_wm_base_listener l = { .ping = xdg_ping };
    return &l;
}

static void top_config(void* d, struct xdg_toplevel* t, int w, int h, struct wl_array* st) {
    (void)t;
    struct wayland_window* win = d;
    if (!win) return;

    if (w > 0) win->win_w = w;
    if (h > 0) win->win_h = h;

    int active = 0;
    int fullscreen = 0;

    unsigned int* s;
    wl_array_for_each(s, st) {
        if (*s == XDG_TOPLEVEL_STATE_ACTIVATED) active = 1;
        if (*s == XDG_TOPLEVEL_STATE_FULLSCREEN) fullscreen = 1;
    }

    win->activated = active;
    win->fullscreen = fullscreen;
    win->visible = active && fullscreen;

    if (!win->configured_once && win->win_w > 0 && win->win_h > 0) {
        win->configured_once = 1;
        win->fullscreen_configured = 1;

        wl_set_surface_opaque(win);
        wl_surface_commit(win->surface);

        pf_timestamp("fullscreen configured");
    }

    if (win->visible) pf_timestamp("Window is visible");
    else pf_timestamp("Window is not visible");
}

static void top_close(void* d, struct xdg_toplevel* t) {
    (void)t;
    struct wayland_window* w = d;
    if (w) w->running = 0;
}

static void top_bounds(void* d, struct xdg_toplevel* t, int w, int h) {
    (void)d; (void)t; (void)w; (void)h;
}

static void top_caps(void* d, struct xdg_toplevel* t, struct wl_array* c) {
    (void)d; (void)t; (void)c;
}

static const struct xdg_toplevel_listener* get_top_listener(void) {
    static const struct xdg_toplevel_listener top_l = {
        .configure = top_config,
        .close = top_close,
        .configure_bounds = top_bounds,
        .wm_capabilities = top_caps
    };
    return &top_l;
}

static void xsurf_conf(void* d, struct xdg_surface* s, unsigned int serial) {
    (void)d;
    xdg_surface_ack_configure(s, serial);
}

static const struct xdg_surface_listener* get_xsurf_listener(void) {
    static const struct xdg_surface_listener xsurf_l = { .configure = xsurf_conf };
    return &xsurf_l;
}

static void surf_enter(void* d, struct wl_surface* s, struct wl_output* o) {
    (void)d; (void)s; (void)o;
}

static void surf_leave(void* d, struct wl_surface* s, struct wl_output* o) {
    (void)d; (void)s; (void)o;
}

static const struct wl_surface_listener* get_surface_listener(void) {
    static const struct wl_surface_listener L = {
        .enter = surf_enter,
        .leave = surf_leave
    };
    return &L;
}

static void reg_add(void* d, struct wl_registry* r, unsigned int name,
                    const char* iface, unsigned int ver) {
    struct wayland_window* w = d;
    if (!w) return;

    if (!strcmp(iface, wl_compositor_interface.name)) {
        unsigned int v = ver < 4 ? ver : 4;
        w->compositor = wl_registry_bind(r, name, &wl_compositor_interface, v);
    } else if (!strcmp(iface, xdg_wm_base_interface.name)) {
        unsigned int v = ver < 6 ? ver : 6;
        w->xdg_wm_base = wl_registry_bind(r, name, &xdg_wm_base_interface, v);
        xdg_wm_base_add_listener(w->xdg_wm_base, get_xdg_wm_listener(), w);
    } else if (!strcmp(iface, wl_seat_interface.name)) {
        unsigned int v = ver < 5 ? ver : 5;
        w->seat = wl_registry_bind(r, name, &wl_seat_interface, v);
        wl_seat_add_listener(w->seat, get_seat_listener(), w);
    } else if (!strcmp(iface, wp_tearing_control_manager_v1_interface.name)) {
        w->tearing_mgr = wl_registry_bind(r, name, &wp_tearing_control_manager_v1_interface, 1);
    }
}

static void reg_rem(void* d, struct wl_registry* r, unsigned int name) {
    (void)d; (void)r; (void)name;
}

static const struct wl_registry_listener* get_registry_listener(void) {
    static const struct wl_registry_listener reg_l = {
        .global = reg_add,
        .global_remove = reg_rem
    };
    return &reg_l;
}

/* APPLICATION CODE */
static struct wayland_window wl_window;

int wl_window_width(void) { return wl_window.win_w; }
int wl_window_height(void) { return wl_window.win_h; }
void* wl_surface_or_hwnd(void) { return wl_window.surface; }
void* wl_display_or_instance(void) { return wl_window.display; }
int wl_window_visible(void) { return wl_window.visible; }

int wl_poll_events(void) {
    if (!wl_window.display || !wl_window.running) return 0;

    int fd = wl_display_get_fd(wl_window.display);
    struct pollfd pfd = {
        .fd = fd,
        .events = POLLIN,
        .revents = 0
    };

    if (wl_display_prepare_read(wl_window.display) == 0) {
        int r = poll(&pfd, 1, 0);  // non-blocking
        if (r > 0 && (pfd.revents & POLLIN)) {
            wl_display_read_events(wl_window.display);
            wl_display_dispatch_pending(wl_window.display);
        } else {
            wl_display_cancel_read(wl_window.display);
        }
    } else {
        wl_display_dispatch_pending(wl_window.display);
    }

    return wl_window.running;
}

void wl_create_window(char* app_name, void* ud, KEYBOARD_CB key_cb, MOUSE_CB mouse_cb) {
    wl_window.win_w = 0;
    wl_window.win_h = 0;
    wl_window.fullscreen_configured = 0;
    wl_window.configured_once = 0;
    wl_window.visible = 0;
    wl_window.activated = 0;
    wl_window.fullscreen = 0;
    wl_window.tearing_requested = 0;
    wl_window.running = 1;

    wl_window.on_key = key_cb;
    wl_window.on_mouse = mouse_cb;
    wl_window.callback_data = ud;

    wl_window.refresh_ns = 16666667ull;
    wl_window.last_present_ns = 0;
    wl_window.last_feedback_ns = 0;
    wl_window.phase_ns = 0;
    wl_window.phase_alpha = 0.10;

    wl_window.kbd_repeat_rate = 0;
    wl_window.kbd_repeat_delay = 600;

    wl_window.display = wl_display_connect(NULL);
    if (!wl_window.display) {
        printf("wl_display_connect failed\n");
        exit(1);
    }

    wl_window.registry = wl_display_get_registry(wl_window.display);
    wl_registry_add_listener(wl_window.registry, get_registry_listener(), &wl_window);

    wl_display_roundtrip(wl_window.display);
    pf_time_reset();
    pf_timestamp("wl globals ready");

    if (!wl_window.compositor || !wl_window.xdg_wm_base) {
        printf("missing compositor/xdg_wm_base\n");
        exit(1);
    }

    if (wl_window.tearing_mgr) pf_timestamp("tearing-control protocol available");
    else pf_timestamp("tearing-control protocol NOT available");

    wl_window.surface = wl_compositor_create_surface(wl_window.compositor);
    wl_surface_add_listener(wl_window.surface, get_surface_listener(), &wl_window);

    /* Ask for tearing-friendly async presentation if compositor supports it. */
    wl_request_tearing_async(&wl_window);

    wl_window.xdg_surface = xdg_wm_base_get_xdg_surface(wl_window.xdg_wm_base, wl_window.surface);
    xdg_surface_add_listener(wl_window.xdg_surface, get_xsurf_listener(), &wl_window);

    wl_window.xdg_toplevel = xdg_surface_get_toplevel(wl_window.xdg_surface);
    xdg_toplevel_add_listener(wl_window.xdg_toplevel, get_top_listener(), &wl_window);

    xdg_toplevel_set_title(wl_window.xdg_toplevel, app_name);
    xdg_toplevel_set_app_id(wl_window.xdg_toplevel, app_name);
    xdg_toplevel_set_fullscreen(wl_window.xdg_toplevel, NULL);

    /* Initial commit to create/map the role-bearing surface state. */
    wl_surface_commit(wl_window.surface);

    while (!wl_window.fullscreen_configured && wl_window.running) {
        wl_display_flush(wl_window.display);
        wl_display_dispatch(wl_window.display);
    }

    pf_timestamp("set up wayland");
}

void wl_destroy_window(void) {
    if (wl_window.tearing) {
        wp_tearing_control_v1_destroy(wl_window.tearing);
        wl_window.tearing = NULL;
    }
    if (wl_window.presentation) {
        wp_presentation_destroy(wl_window.presentation);
        wl_window.presentation = NULL;
    }

    if (wl_window.ptr) {
        wl_pointer_destroy(wl_window.ptr);
        wl_window.ptr = NULL;
    }
    if (wl_window.kbd) {
        wl_keyboard_destroy(wl_window.kbd);
        wl_window.kbd = NULL;
    }
    if (wl_window.seat) {
        wl_seat_destroy(wl_window.seat);
        wl_window.seat = NULL;
    }

    if (wl_window.xdg_toplevel) {
        xdg_toplevel_destroy(wl_window.xdg_toplevel);
        wl_window.xdg_toplevel = NULL;
    }
    if (wl_window.xdg_surface) {
        xdg_surface_destroy(wl_window.xdg_surface);
        wl_window.xdg_surface = NULL;
    }
    if (wl_window.surface) {
        wl_surface_destroy(wl_window.surface);
        wl_window.surface = NULL;
    }

    if (wl_window.xdg_wm_base) {
        xdg_wm_base_destroy(wl_window.xdg_wm_base);
        wl_window.xdg_wm_base = NULL;
    }
    if (wl_window.compositor) {
        wl_compositor_destroy(wl_window.compositor);
        wl_window.compositor = NULL;
    }
    if (wl_window.registry) {
        wl_registry_destroy(wl_window.registry);
        wl_window.registry = NULL;
    }

    if (wl_window.display) {
        wl_display_disconnect(wl_window.display);
        wl_window.display = NULL;
    }
}
#endif
