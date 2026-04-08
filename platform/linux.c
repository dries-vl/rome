#if defined(__linux__)
#include "platform.h"

#include <stdio.h>
#include <time.h>
static long long T0;
long long pf_ns_now(void) {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (long long)ts.tv_sec * 1000000000ll + (long long)ts.tv_nsec;
}
long long pf_ns_start(void) { return T0; }
void pf_time_reset(void) { T0 = pf_ns_now(); }
void pf_timestamp(char *msg) {
    unsigned long long t = (unsigned long long)pf_ns_now();
    printf("[+%7.3f ms] %s\n", (t - (unsigned long long)T0) / 1e6, msg);
}

#include <linux/input-event-codes.h>
static enum BUTTON map_evdev_key_to_button(unsigned int key) {
    switch (key) {
        case KEY_ESC:         return KEYBOARD_ESCAPE;

        case KEY_ENTER:       return KEYBOARD_ENTER;
        case KEY_KPENTER:     return KEYBOARD_ENTER;
        case KEY_BACKSPACE:   return KEYBOARD_BACKSPACE;
        case KEY_TAB:         return KEYBOARD_TAB;
        case KEY_SPACE:       return KEYBOARD_SPACE;

        case KEY_LEFT:        return KEYBOARD_LEFT;
        case KEY_RIGHT:       return KEYBOARD_RIGHT;
        case KEY_UP:          return KEYBOARD_UP;
        case KEY_DOWN:        return KEYBOARD_DOWN;

        case KEY_HOME:        return KEYBOARD_HOME;
        case KEY_END:         return KEYBOARD_END;
        case KEY_PAGEUP:      return KEYBOARD_PAGEUP;
        case KEY_PAGEDOWN:    return KEYBOARD_PAGEDOWN;
        case KEY_INSERT:      return KEYBOARD_INSERT;
        case KEY_DELETE:      return KEYBOARD_DELETE;

        case KEY_F1:          return KEYBOARD_F1;
        case KEY_F2:          return KEYBOARD_F2;
        case KEY_F3:          return KEYBOARD_F3;
        case KEY_F4:          return KEYBOARD_F4;
        case KEY_F5:          return KEYBOARD_F5;
        case KEY_F6:          return KEYBOARD_F6;
        case KEY_F7:          return KEYBOARD_F7;
        case KEY_F8:          return KEYBOARD_F8;
        case KEY_F9:          return KEYBOARD_F9;
        case KEY_F10:         return KEYBOARD_F10;
        case KEY_F11:         return KEYBOARD_F11;
        case KEY_F12:         return KEYBOARD_F12;

        case KEY_LEFTSHIFT:
        case KEY_RIGHTSHIFT:  return KEYBOARD_SHIFT;
        case KEY_LEFTCTRL:
        case KEY_RIGHTCTRL:   return KEYBOARD_CTRL;
        case KEY_LEFTALT:
        case KEY_RIGHTALT:    return KEYBOARD_ALT;
        case KEY_LEFTMETA:
        case KEY_RIGHTMETA:   return KEYBOARD_SUPER;

        case KEY_0:           return KEYBOARD_0;
        case KEY_1:           return KEYBOARD_1;
        case KEY_2:           return KEYBOARD_2;
        case KEY_3:           return KEYBOARD_3;
        case KEY_4:           return KEYBOARD_4;
        case KEY_5:           return KEYBOARD_5;
        case KEY_6:           return KEYBOARD_6;
        case KEY_7:           return KEYBOARD_7;
        case KEY_8:           return KEYBOARD_8;
        case KEY_9:           return KEYBOARD_9;

        case KEY_A:           return KEYBOARD_A;
        case KEY_B:           return KEYBOARD_B;
        case KEY_C:           return KEYBOARD_C;
        case KEY_D:           return KEYBOARD_D;
        case KEY_E:           return KEYBOARD_E;
        case KEY_F:           return KEYBOARD_F;
        case KEY_G:           return KEYBOARD_G;
        case KEY_H:           return KEYBOARD_H;
        case KEY_I:           return KEYBOARD_I;
        case KEY_J:           return KEYBOARD_J;
        case KEY_K:           return KEYBOARD_K;
        case KEY_L:           return KEYBOARD_L;
        case KEY_M:           return KEYBOARD_M;
        case KEY_N:           return KEYBOARD_N;
        case KEY_O:           return KEYBOARD_O;
        case KEY_P:           return KEYBOARD_P;
        case KEY_Q:           return KEYBOARD_Q;
        case KEY_R:           return KEYBOARD_R;
        case KEY_S:           return KEYBOARD_S;
        case KEY_T:           return KEYBOARD_T;
        case KEY_U:           return KEYBOARD_U;
        case KEY_V:           return KEYBOARD_V;
        case KEY_W:           return KEYBOARD_W;
        case KEY_X:           return KEYBOARD_X;
        case KEY_Y:           return KEYBOARD_Y;
        case KEY_Z:           return KEYBOARD_Z;

        default:              return BUTTON_UNKNOWN;
    }
}

#include "wayland.inc"
#include "drm.inc"
int DRM_KMS = 0;

int pf_window_width() {
    return DRM_KMS ? drm_window_width() : wl_window_width();
}

int pf_window_height() {
    return DRM_KMS ? drm_window_height() : wl_window_height();
}

void *pf_surface_or_hwnd() {
    return DRM_KMS ? NULL : wl_surface_or_hwnd();
}

void *pf_display_or_instance() {
    return DRM_KMS ? NULL : wl_display_or_instance();
}

int pf_window_visible() {
    return DRM_KMS ? drm_window_visible() : wl_window_visible();
}

int pf_poll_events() {
    return DRM_KMS ? drm_poll_events() : wl_poll_events();
}

void pf_create_window(char* name, void* ud, KEYBOARD_CB kcb, MOUSE_CB mcb) {
    // pick between wayland and drm
    extern char* getenv(const char*);
    const char* session = getenv("XDG_SESSION_TYPE");
    const char* wayland_display = getenv("WAYLAND_DISPLAY");
    const char* x_display = getenv("DISPLAY");
    char* tty_path = ttyname(STDIN_FILENO); // try only stdin -> stdout and stderr were redirected already above
    int tty_num = -1;
    int is_tty = sscanf(tty_path, "/dev/tty%d", &tty_num) == 1;
    printf("Env values: %s, %s, %s, %s\n", session, wayland_display, x_display, tty_path);
    int use_wayland = (session && strcmp(session, "wayland") == 0) || (wayland_display && wayland_display[0] != '\0');
    int use_x11 = (x_display && x_display[0] != '\0');
    int use_drm = !use_x11 && !use_wayland && tty_path && is_tty;
    if (use_wayland) {
        wl_create_window(name, ud, kcb, mcb);
    } else if (use_drm) {
        DRM_KMS = 1;
        drm_create_window(ud, kcb, mcb);
    } else if (use_x11) {
        pf_timestamp("Running in X11, not supported, exiting");
        exit(0);
    } else {
        pf_timestamp("Couldn't detect the linux windowing context, exiting");
        exit(0);
    }
}
void pf_destroy_window() {
    if (DRM_KMS) drm_destroy_window();
    else wl_destroy_window();
}
#endif
