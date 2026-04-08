#if defined(__linux__)
#include "platform.h"

#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <limits.h>
#include <linux/kd.h>
#include <linux/vt.h>
#include <math.h>
#include <poll.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <termios.h>
#include <string.h>
#include <sys/ioctl.h>
#include <unistd.h>

#include <libinput.h>
#include <libudev.h>
#include <xf86drm.h>
#include <xf86drmMode.h>

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
};

static struct drm_window drm_window = { .drm_fd = -1 };

static volatile sig_atomic_t g_stop = 0;
static volatile sig_atomic_t g_vt_release_pending = 0;
static volatile sig_atomic_t g_vt_acquire_pending = 0;

static int g_tty_fd = -1;
static int g_old_kd_mode = KD_TEXT;
static struct vt_mode g_old_vt_mode;
static int g_old_kb_mode = K_UNICODE;
static int g_have_old_kb_mode = 0;
static int g_have_old_vt_mode = 0;
static int g_signal_handlers_installed = 0;
static int g_registered_atexit = 0;
static int g_drm_master_owned = 0;
static int g_vt_active = 1;

int drm_window_width(void)  { return drm_window.win_w; }
int drm_window_height(void) { return drm_window.win_h; }
int drm_window_visible(void) { return drm_window.visible; }

int drm_fd(void) { return drm_window.drm_fd; }
unsigned int drm_connector_id(void) { return drm_window.connector_id; }
unsigned int drm_crtc_id(void) { return drm_window.crtc_id; }
const void *drm_mode(void) { return &drm_window.mode; }

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
    if (drm_window.win_w <= 0 || drm_window.win_h <= 0) return;
    if (drm_window.mouse_x < 0) drm_window.mouse_x = 0;
    if (drm_window.mouse_y < 0) drm_window.mouse_y = 0;
    if (drm_window.mouse_x >= drm_window.win_w) drm_window.mouse_x = drm_window.win_w - 1;
    if (drm_window.mouse_y >= drm_window.win_h) drm_window.mouse_y = drm_window.win_h - 1;
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

static void flush_tty_input_best_effort(void) {
    if (g_tty_fd >= 0) {
        if (tcflush(g_tty_fd, TCIFLUSH) < 0) {
            perror("tcflush(TCIFLUSH)");
        }
    }
}

static int disable_vt_keyboard(void) {
    if (ioctl(g_tty_fd, KDGKBMODE, &g_old_kb_mode) < 0) {
        perror("KDGKBMODE");
        g_have_old_kb_mode = 0;
    } else {
        g_have_old_kb_mode = 1;
    }

    if (ioctl(g_tty_fd, KDSKBMODE, K_OFF) < 0) {
        perror("KDSKBMODE K_OFF");
        return 0;
    }

    flush_tty_input_best_effort();
    return 1;
}

static void restore_vt_keyboard_best_effort(void) {
    if (g_tty_fd >= 0 && g_have_old_kb_mode) {
        if (ioctl(g_tty_fd, KDSKBMODE, g_old_kb_mode) < 0) {
            perror("KDSKBMODE restore");
        }
    }
    g_have_old_kb_mode = 0;
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

    if (!disable_vt_keyboard()) {
        if (ioctl(g_tty_fd, KDSETMODE, g_old_kd_mode) < 0) {
            perror("KDSETMODE restore after keyboard disable failure");
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
        restore_vt_keyboard_best_effort();
        if (ioctl(g_tty_fd, VT_SETMODE, &g_old_vt_mode) < 0) {
            perror("VT_SETMODE restore");
        }
    }

    if (ioctl(g_tty_fd, KDSETMODE, g_old_kd_mode) < 0) {
        perror("KDSETMODE restore");
    }

    flush_tty_input_best_effort();

    close(g_tty_fd);
    g_tty_fd = -1;
    g_have_old_vt_mode = 0;
    g_vt_active = 0;
}

static int init_input(void) {
    drm_window.udev = udev_new();
    if (!drm_window.udev) {
        fprintf(stderr, "udev_new failed\n");
        return 0;
    }

    drm_window.libinput = libinput_udev_create_context(&libinput_iface, NULL, drm_window.udev);
    if (!drm_window.libinput) {
        fprintf(stderr, "libinput_udev_create_context failed\n");
        return 0;
    }

    if (libinput_udev_assign_seat(drm_window.libinput, "seat0") != 0) {
        fprintf(stderr, "libinput_udev_assign_seat failed\n");
        return 0;
    }

    return 1;
}

static void handle_libinput_events(void) {
    if (!drm_window.libinput || !g_vt_active) return;

    libinput_dispatch(drm_window.libinput);

    struct libinput_event *ev;
    while ((ev = libinput_get_event(drm_window.libinput)) != NULL) {
        enum libinput_event_type type = libinput_event_get_type(ev);

        switch (type) {
            case LIBINPUT_EVENT_KEYBOARD_KEY: {
                struct libinput_event_keyboard *k =
                    libinput_event_get_keyboard_event(ev);
                uint32_t key = libinput_event_keyboard_get_key(k);
                enum libinput_key_state state =
                    libinput_event_keyboard_get_key_state(k);

                    if (drm_window.on_key) {
                        enum BUTTON b = map_evdev_key_to_button(key);
                        drm_window.on_key(drm_window.callback_data, b,
                                 state == LIBINPUT_KEY_STATE_PRESSED ? PRESSED : RELEASED);
                    }
            } break;

            case LIBINPUT_EVENT_POINTER_MOTION: {
                struct libinput_event_pointer *p =
                    libinput_event_get_pointer_event(ev);
                drm_window.mouse_x += (int)lrint(libinput_event_pointer_get_dx(p));
                drm_window.mouse_y += (int)lrint(libinput_event_pointer_get_dy(p));
                clamp_mouse();

                if (drm_window.on_mouse) {
                    drm_window.on_mouse(drm_window.callback_data, drm_window.mouse_x, drm_window.mouse_y, MOUSE_MOVED, 0);
                }
            } break;

            case LIBINPUT_EVENT_POINTER_MOTION_ABSOLUTE: {
                struct libinput_event_pointer *p =
                    libinput_event_get_pointer_event(ev);
                drm_window.mouse_x = (int)lrint(
                    libinput_event_pointer_get_absolute_x_transformed(p, drm_window.win_w));
                drm_window.mouse_y = (int)lrint(
                    libinput_event_pointer_get_absolute_y_transformed(p, drm_window.win_h));
                clamp_mouse();

                if (drm_window.on_mouse) {
                    drm_window.on_mouse(drm_window.callback_data, drm_window.mouse_x, drm_window.mouse_y, MOUSE_MOVED, 0);
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

                if (drm_window.on_mouse) {
                    drm_window.on_mouse(drm_window.callback_data, drm_window.mouse_x, drm_window.mouse_y, b,
                               state == LIBINPUT_BUTTON_STATE_PRESSED ? PRESSED : RELEASED);
                }
            } break;

            case LIBINPUT_EVENT_POINTER_SCROLL_WHEEL: {
                struct libinput_event_pointer *p =
                    libinput_event_get_pointer_event(ev);

                if (drm_window.on_mouse) {
                    double vy = libinput_event_pointer_get_scroll_value_v120(
                        p, LIBINPUT_POINTER_AXIS_SCROLL_VERTICAL) / 10.0;
                    double hx = libinput_event_pointer_get_scroll_value_v120(
                        p, LIBINPUT_POINTER_AXIS_SCROLL_HORIZONTAL) / 10.0;

                    if (vy != 0.0) {
                        drm_window.on_mouse(drm_window.callback_data, drm_window.mouse_x, drm_window.mouse_y,
                                   MOUSE_SCROLL, (int)vy);
                    }
                    if (hx != 0.0) {
                        drm_window.on_mouse(drm_window.callback_data, drm_window.mouse_x, drm_window.mouse_y,
                                   MOUSE_SCROLL_SIDE, (int)hx);
                    }
                }
            } break;

            default:
                break;
        }

        libinput_event_destroy(ev);
        libinput_dispatch(drm_window.libinput);
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
    if (drm_window.drm_fd < 0 || !g_vt_active) return;

    drmEventContext ev;
    memset(&ev, 0, sizeof(ev));
    ev.version = DRM_EVENT_CONTEXT_VERSION;
    ev.page_flip_handler = drm_page_flip_handler;
    drmHandleEvent(drm_window.drm_fd, &ev);
}

static void restore_saved_crtc_best_effort(void) {
    if (drm_window.saved_crtc && drm_window.drm_fd >= 0) {
        drmModeSetCrtc(drm_window.drm_fd,
                       drm_window.saved_crtc->crtc_id,
                       drm_window.saved_crtc->buffer_id,
                       drm_window.saved_crtc->x,
                       drm_window.saved_crtc->y,
                       &drm_window.connector_id,
                       1,
                       &drm_window.saved_crtc->mode);
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
        if (g_have_old_kb_mode) {
            ioctl(g_tty_fd, KDSKBMODE, g_old_kb_mode);
        } else {
            ioctl(g_tty_fd, KDSKBMODE, K_UNICODE);
        }
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

void drm_destroy_window(void) {
    static int in_destroy = 0;
    if (in_destroy) return;
    in_destroy = 1;

    drm_window.running = 0;
    drm_window.visible = 0;

    if (drm_window.libinput) {
        libinput_unref(drm_window.libinput);
        drm_window.libinput = NULL;
    }
    if (drm_window.udev) {
        udev_unref(drm_window.udev);
        drm_window.udev = NULL;
    }

    if (drm_window.saved_crtc && drm_window.drm_fd >= 0) {
        drmModeSetCrtc(drm_window.drm_fd,
                       drm_window.saved_crtc->crtc_id,
                       drm_window.saved_crtc->buffer_id,
                       drm_window.saved_crtc->x,
                       drm_window.saved_crtc->y,
                       &drm_window.connector_id,
                       1,
                       &drm_window.saved_crtc->mode);
        drmModeFreeCrtc(drm_window.saved_crtc);
        drm_window.saved_crtc = NULL;
    }

    if (drm_window.drm_fd >= 0) {
        if (g_drm_master_owned) {
            if (drmDropMaster(drm_window.drm_fd) == 0) {
                g_drm_master_owned = 0;
            }
        }
        close(drm_window.drm_fd);
        drm_window.drm_fd = -1;
    }

    vt_restore_text_best_effort();

    memset(&drm_window, 0, sizeof(drm_window));
    drm_window.drm_fd = -1;

    g_stop = 0;
    g_vt_release_pending = 0;
    g_vt_acquire_pending = 0;
    g_drm_master_owned = 0;
    g_have_old_kb_mode = 0;
    g_old_kb_mode = K_UNICODE;
    g_vt_active = 0;

    in_destroy = 0;
}

static void cleanup_on_exit(void) {
    drm_destroy_window();
}

static void process_vt_requests(void) {
    if (g_vt_release_pending) {
        g_vt_release_pending = 0;

        drm_window.visible = 0;
        g_vt_active = 0;

        if (drm_window.libinput) {
            if (libinput_resume(drm_window.libinput) != 0) {
                fprintf(stderr, "libinput_resume failed\n");
            } else {
                libinput_dispatch(drm_window.libinput);
                struct libinput_event *ev;
                while ((ev = libinput_get_event(drm_window.libinput)) != NULL) {
                    libinput_event_destroy(ev);
                }
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

        if (drm_window.libinput) {
            if (libinput_resume(drm_window.libinput) != 0) {
                fprintf(stderr, "libinput_resume failed\n");
            } else {
                libinput_dispatch(drm_window.libinput);
                while (libinput_get_event(drm_window.libinput) != NULL) {
                    /* discard stale events after resume */
                }
            }
        }

        if (drm_window.drm_fd >= 0 && !g_drm_master_owned) {
            if (drmSetMaster(drm_window.drm_fd) != 0) {
                perror("drmSetMaster");
            } else {
                g_drm_master_owned = 1;
            }
        }

        restore_saved_crtc_best_effort();

        g_vt_active = 1;
        drm_window.visible = 1;
    }
}

void drm_create_window(void *ud, KEYBOARD_CB key_cb, MOUSE_CB mouse_cb) {
    memset(&drm_window, 0, sizeof(drm_window));
    drm_window.drm_fd = -1;
    drm_window.running = 1;
    drm_window.visible = 1;
    drm_window.on_key = key_cb;
    drm_window.on_mouse = mouse_cb;
    drm_window.callback_data = ud;

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

    if (!open_first_usable_drm_device(&drm_window.drm_fd,
                                      drm_window.drm_path,
                                      sizeof(drm_window.drm_path),
                                      &drm_window.connector_id,
                                      &drm_window.crtc_id,
                                      &drm_window.mode,
                                      &drm_window.saved_crtc)) {
        fprintf(stderr,
                "No usable DRM/KMS device+connector found.\n"
                "Run from a real VT, ensure a display is connected to the selected GPU,\n"
                "and make sure another compositor/session is not owning the display path.\n");
        goto fail;
    }

    g_drm_master_owned = 1;

    drm_window.win_w = drm_window.mode.hdisplay;
    drm_window.win_h = drm_window.mode.vdisplay;
    drm_window.mouse_x = drm_window.win_w / 2;
    drm_window.mouse_y = drm_window.win_h / 2;

    if (!init_input()) {
        goto fail;
    }

    pf_time_reset();
    pf_timestamp("DRM/KMS + libinput ready");
    return;

fail:
    drm_destroy_window();
    exit(1);
}

int drm_poll_events(void) {
    process_vt_requests();

    if (!drm_window.running || g_stop) {
        return 0;
    }

    struct pollfd fds[2];
    nfds_t nfds = 0;

    if (drm_window.drm_fd >= 0) {
        fds[nfds].fd = drm_window.drm_fd;
        fds[nfds].events = POLLIN;
        fds[nfds].revents = 0;
        ++nfds;
    }

    if (drm_window.libinput) {
        int li_fd = libinput_get_fd(drm_window.libinput);
        fds[nfds].fd = li_fd;
        fds[nfds].events = POLLIN;
        fds[nfds].revents = 0;
        ++nfds;
    }

    int r = poll(fds, nfds, 0);
    if (r < 0) {
        if (errno == EINTR) {
            process_vt_requests();
            return drm_window.running && !g_stop;
        }
        perror("poll");
        return drm_window.running && !g_stop;
    }

    int idx = 0;
    if (drm_window.drm_fd >= 0) {
        if (fds[idx].revents & POLLIN) {
            handle_drm_events();
        }
        ++idx;
    }

    if (drm_window.libinput) {
        if (fds[idx].revents & POLLIN) {
            handle_libinput_events();
        }
    }

    process_vt_requests();
    return drm_window.running && !g_stop;
}

#endif
