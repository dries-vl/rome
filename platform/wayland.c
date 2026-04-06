#include "platform.h"
#if defined(__linux__) && USE_DRM_KMS == 0

#include "vendor/wayland-client.h"
#include "vendor/xdg-shell-client-protocol.h"
#include "vendor/xdg-shell-client-protocol.c"
#include "vendor/presentation-time-client-protocol.h"
#include "vendor/presentation-time-client-protocol.c"
#include "vendor/tearing-control-v1-client-protocol.h"
#include "vendor/tearing-control-v1-client-protocol.c"
#include "vendor/xkbcommon.h"

#include <time.h>
#include <sys/mman.h>
#include <unistd.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <poll.h>

static clockid_t clockid;

long long pf_ns_now(void) {
    struct timespec ts;
    clock_gettime(clockid ? clockid : CLOCK_MONOTONIC, &ts);
    return (long long)ts.tv_sec * 1000000000ll + (long long)ts.tv_nsec;
}

long long T0;
long long pf_ns_start(void) { return T0; }
void pf_time_reset(void) { T0 = pf_ns_now(); }

void pf_timestamp(char *msg) {
    unsigned long long _t = pf_ns_now();
    printf("[+%7.3f ms] %s\n", (_t - T0) / 1e6, msg);
}

static enum BUTTON map_keysym_to_button(xkb_keysym_t sym) {
    switch (sym) {
        case XKB_KEY_Escape:     return KEYBOARD_ESCAPE;

        case XKB_KEY_Return:     return KEYBOARD_ENTER;
        case XKB_KEY_KP_Enter:   return KEYBOARD_ENTER;
        case XKB_KEY_BackSpace:  return KEYBOARD_BACKSPACE;
        case XKB_KEY_Tab:        return KEYBOARD_TAB;
        case XKB_KEY_space:      return KEYBOARD_SPACE;

        case XKB_KEY_Left:       return KEYBOARD_LEFT;
        case XKB_KEY_Right:      return KEYBOARD_RIGHT;
        case XKB_KEY_Up:         return KEYBOARD_UP;
        case XKB_KEY_Down:       return KEYBOARD_DOWN;

        case XKB_KEY_Home:       return KEYBOARD_HOME;
        case XKB_KEY_End:        return KEYBOARD_END;
        case XKB_KEY_Page_Up:    return KEYBOARD_PAGEUP;
        case XKB_KEY_Page_Down:  return KEYBOARD_PAGEDOWN;
        case XKB_KEY_Insert:     return KEYBOARD_INSERT;
        case XKB_KEY_Delete:     return KEYBOARD_DELETE;

        case XKB_KEY_F1:  return KEYBOARD_F1;
        case XKB_KEY_F2:  return KEYBOARD_F2;
        case XKB_KEY_F3:  return KEYBOARD_F3;
        case XKB_KEY_F4:  return KEYBOARD_F4;
        case XKB_KEY_F5:  return KEYBOARD_F5;
        case XKB_KEY_F6:  return KEYBOARD_F6;
        case XKB_KEY_F7:  return KEYBOARD_F7;
        case XKB_KEY_F8:  return KEYBOARD_F8;
        case XKB_KEY_F9:  return KEYBOARD_F9;
        case XKB_KEY_F10: return KEYBOARD_F10;
        case XKB_KEY_F11: return KEYBOARD_F11;
        case XKB_KEY_F12: return KEYBOARD_F12;

        case XKB_KEY_Shift_L:
        case XKB_KEY_Shift_R:    return KEYBOARD_SHIFT;
        case XKB_KEY_Control_L:
        case XKB_KEY_Control_R:  return KEYBOARD_CTRL;
        case XKB_KEY_Alt_L:
        case XKB_KEY_Alt_R:      return KEYBOARD_ALT;
        case XKB_KEY_Super_L:
        case XKB_KEY_Super_R:    return KEYBOARD_SUPER;

        case XKB_KEY_0: return KEYBOARD_0;
        case XKB_KEY_1: return KEYBOARD_1;
        case XKB_KEY_2: return KEYBOARD_2;
        case XKB_KEY_3: return KEYBOARD_3;
        case XKB_KEY_4: return KEYBOARD_4;
        case XKB_KEY_5: return KEYBOARD_5;
        case XKB_KEY_6: return KEYBOARD_6;
        case XKB_KEY_7: return KEYBOARD_7;
        case XKB_KEY_8: return KEYBOARD_8;
        case XKB_KEY_9: return KEYBOARD_9;

        case XKB_KEY_a: case XKB_KEY_A: return KEYBOARD_A;
        case XKB_KEY_b: case XKB_KEY_B: return KEYBOARD_B;
        case XKB_KEY_c: case XKB_KEY_C: return KEYBOARD_C;
        case XKB_KEY_d: case XKB_KEY_D: return KEYBOARD_D;
        case XKB_KEY_e: case XKB_KEY_E: return KEYBOARD_E;
        case XKB_KEY_f: case XKB_KEY_F: return KEYBOARD_F;
        case XKB_KEY_g: case XKB_KEY_G: return KEYBOARD_G;
        case XKB_KEY_h: case XKB_KEY_H: return KEYBOARD_H;
        case XKB_KEY_i: case XKB_KEY_I: return KEYBOARD_I;
        case XKB_KEY_j: case XKB_KEY_J: return KEYBOARD_J;
        case XKB_KEY_k: case XKB_KEY_K: return KEYBOARD_K;
        case XKB_KEY_l: case XKB_KEY_L: return KEYBOARD_L;
        case XKB_KEY_m: case XKB_KEY_M: return KEYBOARD_M;
        case XKB_KEY_n: case XKB_KEY_N: return KEYBOARD_N;
        case XKB_KEY_o: case XKB_KEY_O: return KEYBOARD_O;
        case XKB_KEY_p: case XKB_KEY_P: return KEYBOARD_P;
        case XKB_KEY_q: case XKB_KEY_Q: return KEYBOARD_Q;
        case XKB_KEY_r: case XKB_KEY_R: return KEYBOARD_R;
        case XKB_KEY_s: case XKB_KEY_S: return KEYBOARD_S;
        case XKB_KEY_t: case XKB_KEY_T: return KEYBOARD_T;
        case XKB_KEY_u: case XKB_KEY_U: return KEYBOARD_U;
        case XKB_KEY_v: case XKB_KEY_V: return KEYBOARD_V;
        case XKB_KEY_w: case XKB_KEY_W: return KEYBOARD_W;
        case XKB_KEY_x: case XKB_KEY_X: return KEYBOARD_X;
        case XKB_KEY_y: case XKB_KEY_Y: return KEYBOARD_Y;
        case XKB_KEY_z: case XKB_KEY_Z: return KEYBOARD_Z;

        default: return BUTTON_UNKNOWN;
    }
}

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

    struct xkb_context* xkb_ctx;
    struct xkb_keymap*  xkb_keymap;
    struct xkb_state*   xkb_state;
    int                 kbd_repeat_rate;
    int                 kbd_repeat_delay;

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
    (void)k;
    struct wayland_window* w = data;
    if (!w) {
        close(fd);
        return;
    }

    if (format != WL_KEYBOARD_KEYMAP_FORMAT_XKB_V1) {
        close(fd);
        return;
    }

    void* map_shm = mmap(NULL, size, PROT_READ, MAP_PRIVATE, fd, 0);
    if (map_shm == MAP_FAILED) {
        close(fd);
        return;
    }

    if (w->xkb_keymap) {
        xkb_keymap_unref(w->xkb_keymap);
        w->xkb_keymap = NULL;
    }
    if (w->xkb_state) {
        xkb_state_unref(w->xkb_state);
        w->xkb_state = NULL;
    }

    w->xkb_keymap = xkb_keymap_new_from_string(
        w->xkb_ctx,
        (const char*)map_shm,
        XKB_KEYMAP_FORMAT_TEXT_V1,
        XKB_KEYMAP_COMPILE_NO_FLAGS
    );

    munmap(map_shm, size);
    close(fd);

    if (!w->xkb_keymap) return;
    w->xkb_state = xkb_state_new(w->xkb_keymap);
}

static void kb_modifiers(void* data, struct wl_keyboard* k, unsigned int serial,
                         unsigned int dep, unsigned int lat,
                         unsigned int lock, unsigned int group) {
    (void)k; (void)serial;
    struct wayland_window* w = data;
    if (!w || !w->xkb_state) return;

    xkb_state_update_mask(w->xkb_state, dep, lat, lock, 0, 0, group);
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
    (void)k; (void)serial; (void)time;
    struct wayland_window* w = data;
    if (!w || !w->on_key) return;

    xkb_keycode_t kc = (xkb_keycode_t)(key + 8);

    enum BUTTON button = BUTTON_UNKNOWN;
    if (w->xkb_state) {
        xkb_keysym_t sym = xkb_state_key_get_one_sym(w->xkb_state, kc);
        button = map_keysym_to_button(sym);
    } else if (key == 1) {
        button = KEYBOARD_ESCAPE;
    }

    w->on_key(w->callback_data, button, state);
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

static void pres_clock_id(void* d, struct wp_presentation* p, unsigned int c) {
    (void)p;
    struct wayland_window* w = d;
    clockid = (clockid_t)c;
    if (w) pf_timestamp("presentation clock ready");
}

static const struct wp_presentation_listener* get_pres_listener(void) {
    static const struct wp_presentation_listener L = { .clock_id = pres_clock_id };
    return &L;
}

struct FBUserData {
    struct wayland_window* w;
    unsigned long long id;
    unsigned long long queued_ns;
};

static void fb_sync_output(void* data, struct wp_presentation_feedback* fb, struct wl_output* out) {
    (void)data; (void)fb; (void)out;
}

static void fb_presented(void* data, struct wp_presentation_feedback* fb,
    unsigned int tv_sec_hi, unsigned int tv_sec_lo, unsigned int tv_nsec,
    unsigned int refresh_ns, unsigned int seq_hi, unsigned int seq_lo, unsigned int flags) {
    (void)seq_hi; (void)seq_lo; (void)flags;
    struct FBUserData* ud = data;
    struct wayland_window* w = ud ? ud->w : NULL;

    unsigned long long sec = ((unsigned long long)tv_sec_hi << 32) | tv_sec_lo;
    unsigned long long t_present = sec * 1000000000ull + (unsigned long long)tv_nsec;

    if (w) {
        if (refresh_ns) w->refresh_ns = (unsigned long long)refresh_ns;
        w->last_present_ns = t_present;
        w->last_feedback_ns = (unsigned long long)pf_ns_now();
    }

    double queued_ms = (double)((long long)(ud->queued_ns - T0)) / 1e6;
    double presented_ms = (double)((long long)(t_present - T0)) / 1e6;
    double dt_ms = (double)((long long)(t_present - ud->queued_ns)) / 1e6;
    double next_ms = (double)((long long)(t_present + (unsigned long long)refresh_ns - T0)) / 1e6;

    printf("Frame %llu sent at %.3f ms and presented at %.3f ms (%.3f ms later) (next expected at %.3f ms)\n",
           (unsigned long long)ud->id, queued_ms, presented_ms, dt_ms, next_ms);

    wp_presentation_feedback_destroy(fb);
    free(ud);
}

static void fb_discarded(void* data, struct wp_presentation_feedback* fb) {
    struct FBUserData* ud = data;
    if (ud) {
        printf("[present fb id=%llu] DISCARDED\n", (unsigned long long)ud->id);
    }
    wp_presentation_feedback_destroy(fb);
    free(ud);
}

static const struct wp_presentation_feedback_listener* get_fb_listener(void) {
    static const struct wp_presentation_feedback_listener L = {
        .sync_output = fb_sync_output,
        .presented   = fb_presented,
        .discarded   = fb_discarded
    };
    return &L;
}

void pf_request_present_feedback(void* win, unsigned long long frame_id) {
    struct wayland_window* w = win;
    if (!w || !w->presentation || !w->surface) return;

    struct FBUserData* ud = calloc(1, sizeof *ud);
    if (!ud) return;

    ud->w = w;
    ud->id = frame_id;
    ud->queued_ns = (unsigned long long)pf_ns_now();

    struct wp_presentation_feedback* fb =
        wp_presentation_feedback(w->presentation, w->surface);
    if (!fb) {
        free(ud);
        return;
    }

    wp_presentation_feedback_add_listener(fb, get_fb_listener(), ud);
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
    } else if (!strcmp(iface, wp_presentation_interface.name)) {
        w->presentation = wl_registry_bind(r, name, &wp_presentation_interface, 1);
        wp_presentation_add_listener(w->presentation, get_pres_listener(), w);
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
static struct wayland_window w;

int pf_window_width(void) { return w.win_w; }
int pf_window_height(void) { return w.win_h; }
void* pf_surface_or_hwnd(void) { return w.surface; }
void* pf_display_or_instance(void) { return w.display; }
int pf_window_visible(void) { return w.visible; }

int pf_poll_events(void) {
    if (!w.display || !w.running) return 0;

    int fd = wl_display_get_fd(w.display);
    struct pollfd pfd = {
        .fd = fd,
        .events = POLLIN,
        .revents = 0
    };

    if (wl_display_prepare_read(w.display) == 0) {
        int r = poll(&pfd, 1, 0);  // non-blocking
        if (r > 0 && (pfd.revents & POLLIN)) {
            wl_display_read_events(w.display);
            wl_display_dispatch_pending(w.display);
        } else {
            wl_display_cancel_read(w.display);
        }
    } else {
        wl_display_dispatch_pending(w.display);
    }

    return w.running;
}

void pf_create_window(char* app_name, void* ud, KEYBOARD_CB key_cb, MOUSE_CB mouse_cb) {
    memset(&w, 0, sizeof(w));

    w.win_w = 0;
    w.win_h = 0;
    w.fullscreen_configured = 0;
    w.configured_once = 0;
    w.visible = 0;
    w.activated = 0;
    w.fullscreen = 0;
    w.tearing_requested = 0;
    w.running = 1;

    w.on_key = key_cb;
    w.on_mouse = mouse_cb;
    w.callback_data = ud;

    w.refresh_ns = 16666667ull;
    w.last_present_ns = 0;
    w.last_feedback_ns = 0;
    w.phase_ns = 0;
    w.phase_alpha = 0.10;

    w.xkb_ctx = xkb_context_new(XKB_CONTEXT_NO_FLAGS);
    w.kbd_repeat_rate = 0;
    w.kbd_repeat_delay = 600;

    w.display = wl_display_connect(NULL);
    if (!w.display) {
        printf("wl_display_connect failed\n");
        exit(1);
    }

    w.registry = wl_display_get_registry(w.display);
    wl_registry_add_listener(w.registry, get_registry_listener(), &w);

    wl_display_roundtrip(w.display);
    pf_time_reset();
    pf_timestamp("wl globals ready");

    if (!w.compositor || !w.xdg_wm_base) {
        printf("missing compositor/xdg_wm_base\n");
        exit(1);
    }

    if (w.tearing_mgr) pf_timestamp("tearing-control protocol available");
    else pf_timestamp("tearing-control protocol NOT available");

    w.surface = wl_compositor_create_surface(w.compositor);
    wl_surface_add_listener(w.surface, get_surface_listener(), &w);

    /* Ask for tearing-friendly async presentation if compositor supports it. */
    wl_request_tearing_async(&w);

    w.xdg_surface = xdg_wm_base_get_xdg_surface(w.xdg_wm_base, w.surface);
    xdg_surface_add_listener(w.xdg_surface, get_xsurf_listener(), &w);

    w.xdg_toplevel = xdg_surface_get_toplevel(w.xdg_surface);
    xdg_toplevel_add_listener(w.xdg_toplevel, get_top_listener(), &w);

    xdg_toplevel_set_title(w.xdg_toplevel, app_name);
    xdg_toplevel_set_app_id(w.xdg_toplevel, app_name);
    xdg_toplevel_set_fullscreen(w.xdg_toplevel, NULL);

    /* Initial commit to create/map the role-bearing surface state. */
    wl_surface_commit(w.surface);

    while (!w.fullscreen_configured && w.running) {
        wl_display_flush(w.display);
        wl_display_dispatch(w.display);
    }

    pf_timestamp("set up wayland");
}

void pf_destroy_window(void) {
    if (w.tearing) {
        wp_tearing_control_v1_destroy(w.tearing);
        w.tearing = NULL;
    }
    if (w.presentation) {
        wp_presentation_destroy(w.presentation);
        w.presentation = NULL;
    }

    if (w.ptr) {
        wl_pointer_destroy(w.ptr);
        w.ptr = NULL;
    }
    if (w.kbd) {
        wl_keyboard_destroy(w.kbd);
        w.kbd = NULL;
    }
    if (w.seat) {
        wl_seat_destroy(w.seat);
        w.seat = NULL;
    }

    if (w.xdg_toplevel) {
        xdg_toplevel_destroy(w.xdg_toplevel);
        w.xdg_toplevel = NULL;
    }
    if (w.xdg_surface) {
        xdg_surface_destroy(w.xdg_surface);
        w.xdg_surface = NULL;
    }
    if (w.surface) {
        wl_surface_destroy(w.surface);
        w.surface = NULL;
    }

    if (w.xdg_wm_base) {
        xdg_wm_base_destroy(w.xdg_wm_base);
        w.xdg_wm_base = NULL;
    }
    if (w.compositor) {
        wl_compositor_destroy(w.compositor);
        w.compositor = NULL;
    }
    if (w.registry) {
        wl_registry_destroy(w.registry);
        w.registry = NULL;
    }

    if (w.xkb_state) {
        xkb_state_unref(w.xkb_state);
        w.xkb_state = NULL;
    }
    if (w.xkb_keymap) {
        xkb_keymap_unref(w.xkb_keymap);
        w.xkb_keymap = NULL;
    }
    if (w.xkb_ctx) {
        xkb_context_unref(w.xkb_ctx);
        w.xkb_ctx = NULL;
    }

    if (w.display) {
        wl_display_disconnect(w.display);
        w.display = NULL;
    }
}

#endif
