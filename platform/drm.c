#include "platform.h"
#if defined(__linux__) && USE_DRM_KMS == 1

#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <limits.h>
#include <linux/input-event-codes.h>
#include <linux/kd.h>
#include <linux/vt.h>
#include <math.h>
#include <poll.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <time.h>
#include <unistd.h>

#include <libinput.h>
#include <libudev.h>
#include <xkbcommon/xkbcommon.h>
#include <xf86drm.h>
#include <xf86drmMode.h>

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

struct drm_window {
    int drm_fd;
    char drm_path[PATH_MAX];

    drmModeModeInfo mode;
    uint32_t connector_id;
    uint32_t crtc_id;
    drmModeCrtc *saved_crtc;

    int win_w;
    int win_h;
    int mouse_x;
    int mouse_y;
    int running;
    int visible;

    KEYBOARD_CB on_key;
    MOUSE_CB on_mouse;
    void *callback_data;

    struct udev *udev;
    struct libinput *libinput;

    struct xkb_context *xkb_ctx;
    struct xkb_keymap *xkb_keymap;
    struct xkb_state *xkb_state;
};

static struct drm_window w = { .drm_fd = -1 };

static volatile sig_atomic_t g_stop = 0;
static volatile sig_atomic_t g_vt_release_pending = 0;
static volatile sig_atomic_t g_vt_acquire_pending = 0;

static int g_tty_fd = -1;
static int g_old_kd_mode = KD_TEXT;
static struct vt_mode g_old_vt_mode;
static int g_have_old_vt_mode = 0;
static int g_signal_handlers_installed = 0;
static int g_registered_atexit = 0;
static int g_drm_master_owned = 0;
static int g_vt_active = 1;

int pf_window_width(void)  { return w.win_w; }
int pf_window_height(void) { return w.win_h; }
int pf_window_visible(void) { return w.visible; }

int pf_drm_fd(void) { return w.drm_fd; }
unsigned int pf_drm_connector_id(void) { return w.connector_id; }
unsigned int pf_drm_crtc_id(void) { return w.crtc_id; }
const void *pf_drm_mode(void) { return &w.mode; }

static int open_restricted(const char *path, int flags, void *user_data) {
    (void)user_data;
    int fd = open(path, flags | O_CLOEXEC);
    return fd < 0 ? -errno : fd;
}

static void close_restricted(int fd, void *user_data) {
    (void)user_data;
    close(fd);
}

static const struct libinput_interface libinput_iface = {
    .open_restricted = open_restricted,
    .close_restricted = close_restricted
};

static void clamp_mouse(void) {
    if (w.win_w <= 0 || w.win_h <= 0) return;
    if (w.mouse_x < 0) w.mouse_x = 0;
    if (w.mouse_y < 0) w.mouse_y = 0;
    if (w.mouse_x >= w.win_w) w.mouse_x = w.win_w - 1;
    if (w.mouse_y >= w.win_h) w.mouse_y = w.win_h - 1;
}

static const char *connector_state_str(int state) {
    switch (state) {
        case DRM_MODE_CONNECTED: return "connected";
        case DRM_MODE_DISCONNECTED: return "disconnected";
        case DRM_MODE_UNKNOWNCONNECTION: return "unknown";
        default: return "invalid";
    }
}

static void log_connector_info(int fd, drmModeRes *res, const char *path) {
    printf("DRM probe %s: crtcs=%d encoders=%d connectors=%d\n",
           path, res->count_crtcs, res->count_encoders, res->count_connectors);

    for (int i = 0; i < res->count_connectors; ++i) {
        drmModeConnector *conn = drmModeGetConnector(fd, res->connectors[i]);
        if (!conn) continue;

        printf("  connector id=%u type=%u type_id=%u state=%s modes=%d encoders=%d mm=%ux%u\n",
               conn->connector_id,
               conn->connector_type,
               conn->connector_type_id,
               connector_state_str(conn->connection),
               conn->count_modes,
               conn->count_encoders,
               conn->mmWidth,
               conn->mmHeight);

        for (int m = 0; m < conn->count_modes && m < 8; ++m) {
            const drmModeModeInfo *mode = &conn->modes[m];
            double hz = 0.0;
            if (mode->htotal > 0 && mode->vtotal > 0) {
                hz = (double)mode->clock * 1000.0 /
                     ((double)mode->htotal * (double)mode->vtotal);
            }
            printf("    mode[%d] %s %dx%d %.3fHz flags=0x%x type=0x%x\n",
                   m, mode->name, mode->hdisplay, mode->vdisplay,
                   hz, mode->flags, mode->type);
        }

        drmModeFreeConnector(conn);
    }
}

static double mode_score(int active, const drmModeModeInfo *m) {
    uint64_t preferred = (m->type & DRM_MODE_TYPE_PREFERRED) ? 1ull : 0ull;
    double area = ((double)m->hdisplay / 1000.0) * ((double)m->vdisplay / 1000.0);
    double refresh_hz = 0.0;

    if (m->htotal > 0 && m->vtotal > 0) {
        refresh_hz = ((double)m->clock * 1000.0) /
                     ((double)m->htotal * (double)m->vtotal);
    }

    double score = (preferred ? 10.0 : 0.0) + (active ? 10.0 : 0.0) + area + refresh_hz;

    printf("display%s%s: %hu, %hu; area %lf, refresh %lf -> score %lf\n",
           preferred ? " PREFERRED" : "",
           active ? " ACTIVE" : "",
           m->hdisplay, m->vdisplay, area, refresh_hz, score);

    return score;
}

static int connector_is_active(int drm_fd, const drmModeConnector *conn) {
    if (!conn || conn->connection != DRM_MODE_CONNECTED || conn->encoder_id == 0) {
        return 0;
    }

    drmModeEncoder *enc = drmModeGetEncoder(drm_fd, conn->encoder_id);
    if (!enc) return 0;

    int active = 0;
    if (enc->crtc_id != 0) {
        drmModeCrtc *crtc = drmModeGetCrtc(drm_fd, enc->crtc_id);
        if (crtc) {
            active = crtc->mode_valid;
            drmModeFreeCrtc(crtc);
        }
    }

    drmModeFreeEncoder(enc);
    return active;
}

static int pick_best_mode(int drmfd, const drmModeConnector *conn,
                          drmModeModeInfo *out_mode, int *out_index) {
    if (!conn || conn->count_modes <= 0) return 0;

    int best_i = -1;
    double best_score = 0.0;
    int active = connector_is_active(drmfd, conn);

    for (int i = 0; i < conn->count_modes; ++i) {
        double score = mode_score(active, &conn->modes[i]);
        if (best_i < 0 || score > best_score) {
            best_score = score;
            best_i = i;
        }
    }

    if (best_i < 0) return 0;
    *out_mode = conn->modes[best_i];
    if (out_index) *out_index = best_i;
    return 1;
}

static uint32_t pick_crtc_for_connector(int drm_fd, drmModeRes *res, drmModeConnector *conn) {
    if (!res || !conn) return 0;

    for (int i = 0; i < conn->count_encoders; ++i) {
        drmModeEncoder *enc = drmModeGetEncoder(drm_fd, conn->encoders[i]);
        if (!enc) continue;

        for (int c = 0; c < res->count_crtcs; ++c) {
            if (enc->possible_crtcs & (1u << c)) {
                uint32_t crtc_id = res->crtcs[c];
                drmModeFreeEncoder(enc);
                return crtc_id;
            }
        }

        drmModeFreeEncoder(enc);
    }

    return 0;
}

static int try_open_device_path(const char *path,
                                int *out_fd,
                                char *out_path,
                                size_t out_path_size,
                                uint32_t *out_connector_id,
                                uint32_t *out_crtc_id,
                                drmModeModeInfo *out_mode,
                                drmModeCrtc **out_saved_crtc) {
    int fd = open(path, O_RDWR | O_CLOEXEC);
    if (fd < 0) {
        return 0;
    }

    drmModeRes *res = drmModeGetResources(fd);
    if (!res) {
        close(fd);
        return 0;
    }

    log_connector_info(fd, res, path);

    drmModeConnector *best_conn = NULL;
    drmModeModeInfo best_mode;
    uint32_t best_connector_id = 0;
    uint32_t best_crtc_id = 0;
    double best_score = 0.0;

    for (int i = 0; i < res->count_connectors; ++i) {
        drmModeConnector *conn = drmModeGetConnector(fd, res->connectors[i]);
        if (!conn) continue;

        if (conn->connection != DRM_MODE_CONNECTED || conn->count_modes <= 0) {
            drmModeFreeConnector(conn);
            continue;
        }

        drmModeModeInfo chosen_mode;
        int mode_index = -1;
        if (!pick_best_mode(fd, conn, &chosen_mode, &mode_index)) {
            drmModeFreeConnector(conn);
            continue;
        }

        printf("Chosen connector=%u mode[%d]=%s %dx%d flags=0x%x type=0x%x\n",
               conn->connector_id, mode_index, chosen_mode.name,
               chosen_mode.hdisplay, chosen_mode.vdisplay,
               chosen_mode.flags, chosen_mode.type);

        uint32_t crtc_id = pick_crtc_for_connector(fd, res, conn);
        if (!crtc_id) {
            drmModeFreeConnector(conn);
            continue;
        }

        int active = connector_is_active(fd, conn);
        double score = mode_score(active, &chosen_mode);

        if (!best_conn || score > best_score) {
            if (best_conn) drmModeFreeConnector(best_conn);
            best_conn = conn;
            best_mode = chosen_mode;
            best_connector_id = conn->connector_id;
            best_crtc_id = crtc_id;
            best_score = score;
        } else {
            drmModeFreeConnector(conn);
        }
    }

    if (!best_conn) {
        drmModeFreeResources(res);
        close(fd);
        return 0;
    }

    drmModeCrtc *saved = drmModeGetCrtc(fd, best_crtc_id);

    *out_fd = fd;
    snprintf(out_path, out_path_size, "%s", path);
    *out_connector_id = best_connector_id;
    *out_crtc_id = best_crtc_id;
    *out_mode = best_mode;
    *out_saved_crtc = saved;

    printf("Selected DRM device %s connector=%u crtc=%u mode=%s %dx%d\n",
           path, best_connector_id, best_crtc_id,
           best_mode.name, best_mode.hdisplay, best_mode.vdisplay);

    drmModeFreeConnector(best_conn);
    drmModeFreeResources(res);
    return 1;
}

static int open_first_usable_drm_device(int *out_fd,
                                        char *out_path,
                                        size_t out_path_size,
                                        uint32_t *out_connector_id,
                                        uint32_t *out_crtc_id,
                                        drmModeModeInfo *out_mode,
                                        drmModeCrtc **out_saved_crtc) {
    DIR *dir = opendir("/dev/dri");
    if (!dir) {
        perror("opendir /dev/dri");
        return 0;
    }

    struct dirent *ent;
    int found = 0;

    while ((ent = readdir(dir)) != NULL) {
        if (strncmp(ent->d_name, "card", 4) != 0) continue;

        char path[PATH_MAX];
        snprintf(path, sizeof(path), "/dev/dri/%s", ent->d_name);

        if (try_open_device_path(path,
                                 out_fd,
                                 out_path,
                                 out_path_size,
                                 out_connector_id,
                                 out_crtc_id,
                                 out_mode,
                                 out_saved_crtc)) {
            found = 1;
            break;
        }
    }

    closedir(dir);
    return found;
}

static int vt_enter_graphics_and_process_mode(void) {
    if (g_tty_fd >= 0) return 1;

    g_tty_fd = open("/dev/tty", O_RDWR | O_CLOEXEC);
    if (g_tty_fd < 0) {
        perror("open /dev/tty");
        return 0;
    }

    if (ioctl(g_tty_fd, KDGETMODE, &g_old_kd_mode) < 0) {
        perror("KDGETMODE");
        g_old_kd_mode = KD_TEXT;
    }

    if (ioctl(g_tty_fd, VT_GETMODE, &g_old_vt_mode) < 0) {
        perror("VT_GETMODE");
        memset(&g_old_vt_mode, 0, sizeof(g_old_vt_mode));
        g_have_old_vt_mode = 0;
    } else {
        g_have_old_vt_mode = 1;
    }

    struct vt_mode mode;
    memset(&mode, 0, sizeof(mode));
    mode.mode = VT_PROCESS;
    mode.waitv = 0;
    mode.relsig = SIGUSR1;
    mode.acqsig = SIGUSR2;

    if (ioctl(g_tty_fd, VT_SETMODE, &mode) < 0) {
        perror("VT_SETMODE VT_PROCESS");
        close(g_tty_fd);
        g_tty_fd = -1;
        return 0;
    }

    if (ioctl(g_tty_fd, KDSETMODE, KD_GRAPHICS) < 0) {
        perror("KDSETMODE KD_GRAPHICS");
        if (g_have_old_vt_mode) {
            ioctl(g_tty_fd, VT_SETMODE, &g_old_vt_mode);
        }
        close(g_tty_fd);
        g_tty_fd = -1;
        return 0;
    }

    g_vt_active = 1;
    return 1;
}

static void vt_restore_text_best_effort(void) {
    if (g_tty_fd < 0) return;

    if (g_have_old_vt_mode) {
        if (ioctl(g_tty_fd, VT_SETMODE, &g_old_vt_mode) < 0) {
            perror("VT_SETMODE restore");
        }
    }

    if (ioctl(g_tty_fd, KDSETMODE, g_old_kd_mode) < 0) {
        perror("KDSETMODE restore");
    }

    close(g_tty_fd);
    g_tty_fd = -1;
    g_have_old_vt_mode = 0;
    g_vt_active = 0;
}

static int init_input(void) {
    w.udev = udev_new();
    if (!w.udev) {
        fprintf(stderr, "udev_new failed\n");
        return 0;
    }

    w.libinput = libinput_udev_create_context(&libinput_iface, NULL, w.udev);
    if (!w.libinput) {
        fprintf(stderr, "libinput_udev_create_context failed\n");
        return 0;
    }

    if (libinput_udev_assign_seat(w.libinput, "seat0") != 0) {
        fprintf(stderr, "libinput_udev_assign_seat failed\n");
        return 0;
    }

    w.xkb_ctx = xkb_context_new(XKB_CONTEXT_NO_FLAGS);
    if (!w.xkb_ctx) {
        fprintf(stderr, "xkb_context_new failed\n");
        return 0;
    }

    struct xkb_rule_names names = {0};
    w.xkb_keymap = xkb_keymap_new_from_names(
        w.xkb_ctx, &names, XKB_KEYMAP_COMPILE_NO_FLAGS);
    if (!w.xkb_keymap) {
        fprintf(stderr, "xkb_keymap_new_from_names failed\n");
        return 0;
    }

    w.xkb_state = xkb_state_new(w.xkb_keymap);
    if (!w.xkb_state) {
        fprintf(stderr, "xkb_state_new failed\n");
        return 0;
    }

    return 1;
}

static void handle_libinput_events(void) {
    if (!w.libinput || !g_vt_active) return;

    libinput_dispatch(w.libinput);

    struct libinput_event *ev;
    while ((ev = libinput_get_event(w.libinput)) != NULL) {
        enum libinput_event_type type = libinput_event_get_type(ev);

        switch (type) {
            case LIBINPUT_EVENT_KEYBOARD_KEY: {
                struct libinput_event_keyboard *k =
                    libinput_event_get_keyboard_event(ev);
                uint32_t key = libinput_event_keyboard_get_key(k);
                enum libinput_key_state state =
                    libinput_event_keyboard_get_key_state(k);

                xkb_keycode_t kc = (xkb_keycode_t)(key + 8);
                xkb_state_update_key(
                    w.xkb_state,
                    kc,
                    state == LIBINPUT_KEY_STATE_PRESSED ? XKB_KEY_DOWN : XKB_KEY_UP
                );

                if (w.on_key) {
                    xkb_keysym_t sym = xkb_state_key_get_one_sym(w.xkb_state, kc);
                    enum BUTTON b = map_keysym_to_button(sym);
                    w.on_key(w.callback_data, b,
                             state == LIBINPUT_KEY_STATE_PRESSED ? PRESSED : RELEASED);
                }
            } break;

            case LIBINPUT_EVENT_POINTER_MOTION: {
                struct libinput_event_pointer *p =
                    libinput_event_get_pointer_event(ev);
                w.mouse_x += (int)lrint(libinput_event_pointer_get_dx(p));
                w.mouse_y += (int)lrint(libinput_event_pointer_get_dy(p));
                clamp_mouse();

                if (w.on_mouse) {
                    w.on_mouse(w.callback_data, w.mouse_x, w.mouse_y, MOUSE_MOVED, 0);
                }
            } break;

            case LIBINPUT_EVENT_POINTER_MOTION_ABSOLUTE: {
                struct libinput_event_pointer *p =
                    libinput_event_get_pointer_event(ev);
                w.mouse_x = (int)lrint(
                    libinput_event_pointer_get_absolute_x_transformed(p, w.win_w));
                w.mouse_y = (int)lrint(
                    libinput_event_pointer_get_absolute_y_transformed(p, w.win_h));
                clamp_mouse();

                if (w.on_mouse) {
                    w.on_mouse(w.callback_data, w.mouse_x, w.mouse_y, MOUSE_MOVED, 0);
                }
            } break;

            case LIBINPUT_EVENT_POINTER_BUTTON: {
                struct libinput_event_pointer *p =
                    libinput_event_get_pointer_event(ev);
                uint32_t button = libinput_event_pointer_get_button(p);
                enum libinput_button_state state =
                    libinput_event_pointer_get_button_state(p);

                enum BUTTON b = BUTTON_UNKNOWN;
                if (button == BTN_LEFT)   b = MOUSE_LEFT;
                if (button == BTN_RIGHT)  b = MOUSE_RIGHT;
                if (button == BTN_MIDDLE) b = MOUSE_MIDDLE;

                if (w.on_mouse) {
                    w.on_mouse(w.callback_data, w.mouse_x, w.mouse_y, b,
                               state == LIBINPUT_BUTTON_STATE_PRESSED ? PRESSED : RELEASED);
                }
            } break;

            case LIBINPUT_EVENT_POINTER_SCROLL_WHEEL: {
                struct libinput_event_pointer *p =
                    libinput_event_get_pointer_event(ev);

                if (w.on_mouse) {
                    double vy = libinput_event_pointer_get_scroll_value_v120(
                        p, LIBINPUT_POINTER_AXIS_SCROLL_VERTICAL);
                    double hx = libinput_event_pointer_get_scroll_value_v120(
                        p, LIBINPUT_POINTER_AXIS_SCROLL_HORIZONTAL);

                    if (vy != 0.0) {
                        w.on_mouse(w.callback_data, w.mouse_x, w.mouse_y,
                                   MOUSE_SCROLL, (int)vy);
                    }
                    if (hx != 0.0) {
                        w.on_mouse(w.callback_data, w.mouse_x, w.mouse_y,
                                   MOUSE_SCROLL_SIDE, (int)hx);
                    }
                }
            } break;

            default:
                break;
        }

        libinput_event_destroy(ev);
        libinput_dispatch(w.libinput);
    }
}

static void drm_page_flip_handler(int fd,
                                  unsigned int frame,
                                  unsigned int sec,
                                  unsigned int usec,
                                  void *data) {
    (void)fd;
    (void)frame;
    (void)sec;
    (void)usec;
    (void)data;
}

static void handle_drm_events(void) {
    if (w.drm_fd < 0 || !g_vt_active) return;

    drmEventContext ev;
    memset(&ev, 0, sizeof(ev));
    ev.version = DRM_EVENT_CONTEXT_VERSION;
    ev.page_flip_handler = drm_page_flip_handler;
    drmHandleEvent(w.drm_fd, &ev);
}

static void restore_saved_crtc_best_effort(void) {
    if (w.saved_crtc && w.drm_fd >= 0) {
        drmModeSetCrtc(w.drm_fd,
                       w.saved_crtc->crtc_id,
                       w.saved_crtc->buffer_id,
                       w.saved_crtc->x,
                       w.saved_crtc->y,
                       &w.connector_id,
                       1,
                       &w.saved_crtc->mode);
    }
}

static void on_term(int sig) {
    (void)sig;
    g_stop = 1;
}

static void on_vt_release(int sig) {
    (void)sig;
    g_vt_release_pending = 1;
}

static void on_vt_acquire(int sig) {
    (void)sig;
    g_vt_acquire_pending = 1;
}

static void on_crash(int sig) {
    const char msg[] = "fatal signal, attempting VT restore\n";
    write(STDERR_FILENO, msg, sizeof(msg) - 1);

    if (g_tty_fd >= 0) {
        if (g_have_old_vt_mode) {
            ioctl(g_tty_fd, VT_SETMODE, &g_old_vt_mode);
        }
        ioctl(g_tty_fd, KDSETMODE, KD_TEXT);
    }

    _exit(128 + sig);
}

static void install_signal_handlers(void) {
    if (g_signal_handlers_installed) return;

    struct sigaction sa;
    memset(&sa, 0, sizeof(sa));
    sigemptyset(&sa.sa_mask);

    sa.sa_handler = on_term;
    sa.sa_flags = 0;
    sigaction(SIGINT,  &sa, NULL);
    sigaction(SIGTERM, &sa, NULL);
    sigaction(SIGHUP,  &sa, NULL);
    sigaction(SIGQUIT, &sa, NULL);

    sa.sa_handler = on_vt_release;
    sa.sa_flags = 0;
    sigaction(SIGUSR1, &sa, NULL);

    sa.sa_handler = on_vt_acquire;
    sa.sa_flags = 0;
    sigaction(SIGUSR2, &sa, NULL);

    sa.sa_handler = on_crash;
    sa.sa_flags = SA_RESETHAND;
    sigaction(SIGSEGV, &sa, NULL);
    sigaction(SIGABRT, &sa, NULL);
    sigaction(SIGBUS,  &sa, NULL);
    sigaction(SIGILL,  &sa, NULL);

    g_signal_handlers_installed = 1;
}

static void cleanup_on_exit(void) {
    pf_destroy_window();
}

static void process_vt_requests(void) {
    if (g_vt_release_pending) {
        g_vt_release_pending = 0;

        w.visible = 0;
        g_vt_active = 0;

        if (w.drm_fd >= 0 && g_drm_master_owned) {
            if (drmDropMaster(w.drm_fd) != 0) {
                perror("drmDropMaster");
            } else {
                g_drm_master_owned = 0;
            }
        }

        if (g_tty_fd >= 0) {
            if (ioctl(g_tty_fd, VT_RELDISP, 1) < 0) {
                perror("VT_RELDISP release");
            }
        }
    }

    if (g_vt_acquire_pending) {
        g_vt_acquire_pending = 0;

        if (g_tty_fd >= 0) {
            if (ioctl(g_tty_fd, VT_RELDISP, VT_ACKACQ) < 0) {
                perror("VT_RELDISP ack acquire");
            }
            if (ioctl(g_tty_fd, KDSETMODE, KD_GRAPHICS) < 0) {
                perror("KDSETMODE KD_GRAPHICS reacquire");
            }
        }

        if (w.drm_fd >= 0 && !g_drm_master_owned) {
            if (drmSetMaster(w.drm_fd) != 0) {
                perror("drmSetMaster");
            } else {
                g_drm_master_owned = 1;
            }
        }

        restore_saved_crtc_best_effort();

        g_vt_active = 1;
        w.visible = 1;
    }
}

void pf_create_window(char *app_name, void *ud, KEYBOARD_CB key_cb, MOUSE_CB mouse_cb) {
    (void)app_name;

    memset(&w, 0, sizeof(w));
    w.drm_fd = -1;
    w.running = 1;
    w.visible = 1;
    w.on_key = key_cb;
    w.on_mouse = mouse_cb;
    w.callback_data = ud;

    g_stop = 0;
    g_vt_release_pending = 0;
    g_vt_acquire_pending = 0;
    g_vt_active = 1;
    g_drm_master_owned = 0;

    install_signal_handlers();
    if (!g_registered_atexit) {
        atexit(cleanup_on_exit);
        g_registered_atexit = 1;
    }

    if (!vt_enter_graphics_and_process_mode()) {
        fprintf(stderr, "Failed to enter VT graphics/process mode.\n");
        goto fail;
    }

    if (!open_first_usable_drm_device(&w.drm_fd,
                                      w.drm_path,
                                      sizeof(w.drm_path),
                                      &w.connector_id,
                                      &w.crtc_id,
                                      &w.mode,
                                      &w.saved_crtc)) {
        fprintf(stderr,
                "No usable DRM/KMS device+connector found.\n"
                "Run from a real VT, ensure a display is connected to the selected GPU,\n"
                "and make sure another compositor/session is not owning the display path.\n");
        goto fail;
    }

    g_drm_master_owned = 1;

    w.win_w = w.mode.hdisplay;
    w.win_h = w.mode.vdisplay;
    w.mouse_x = w.win_w / 2;
    w.mouse_y = w.win_h / 2;

    if (!init_input()) {
        goto fail;
    }

    pf_time_reset();
    pf_timestamp("DRM/KMS + libinput ready");
    return;

fail:
    pf_destroy_window();
    exit(1);
}

int pf_poll_events(void) {
    process_vt_requests();

    if (!w.running || g_stop) {
        return 0;
    }

    struct pollfd fds[2];
    nfds_t nfds = 0;

    if (w.drm_fd >= 0) {
        fds[nfds].fd = w.drm_fd;
        fds[nfds].events = POLLIN;
        fds[nfds].revents = 0;
        ++nfds;
    }

    if (w.libinput) {
        int li_fd = libinput_get_fd(w.libinput);
        fds[nfds].fd = li_fd;
        fds[nfds].events = POLLIN;
        fds[nfds].revents = 0;
        ++nfds;
    }

    int r = poll(fds, nfds, 0);
    if (r < 0) {
        if (errno == EINTR) {
            process_vt_requests();
            return w.running && !g_stop;
        }
        perror("poll");
        return w.running && !g_stop;
    }

    int idx = 0;
    if (w.drm_fd >= 0) {
        if (fds[idx].revents & POLLIN) {
            handle_drm_events();
        }
        ++idx;
    }

    if (w.libinput) {
        if (fds[idx].revents & POLLIN) {
            handle_libinput_events();
        }
    }

    process_vt_requests();
    return w.running && !g_stop;
}

void pf_destroy_window(void) {
    static int in_destroy = 0;
    if (in_destroy) return;
    in_destroy = 1;

    w.running = 0;
    w.visible = 0;

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

    if (w.libinput) {
        libinput_unref(w.libinput);
        w.libinput = NULL;
    }
    if (w.udev) {
        udev_unref(w.udev);
        w.udev = NULL;
    }

    if (w.saved_crtc && w.drm_fd >= 0) {
        drmModeSetCrtc(w.drm_fd,
                       w.saved_crtc->crtc_id,
                       w.saved_crtc->buffer_id,
                       w.saved_crtc->x,
                       w.saved_crtc->y,
                       &w.connector_id,
                       1,
                       &w.saved_crtc->mode);
        drmModeFreeCrtc(w.saved_crtc);
        w.saved_crtc = NULL;
    }

    if (w.drm_fd >= 0) {
        if (g_drm_master_owned) {
            if (drmDropMaster(w.drm_fd) == 0) {
                g_drm_master_owned = 0;
            }
        }
        close(w.drm_fd);
        w.drm_fd = -1;
    }

    vt_restore_text_best_effort();

    memset(&w, 0, sizeof(w));
    w.drm_fd = -1;

    g_stop = 0;
    g_vt_release_pending = 0;
    g_vt_acquire_pending = 0;
    g_drm_master_owned = 0;
    g_vt_active = 0;

    in_destroy = 0;
}

#endif
