#ifdef _WIN32
#include "platform.h"

#include <windows.h>
#include <stdio.h>

#ifndef GET_X_LPARAM
#define GET_X_LPARAM(lp) ((int)(short)LOWORD(lp))
#endif
#ifndef GET_Y_LPARAM
#define GET_Y_LPARAM(lp) ((int)(short)HIWORD(lp))
#endif

// TIMING
static LARGE_INTEGER qpf = {0};
long long pf_ticks_to_ns(long long qpc) {
    if (!qpf.QuadPart) { QueryPerformanceFrequency(&qpf); }
    return (long long) (qpc * 1000000000ULL / (long long) qpf.QuadPart);
}
long long pf_ns_now(void) {
    LARGE_INTEGER qpc;
    QueryPerformanceCounter(&qpc);
    return pf_ticks_to_ns(qpc.QuadPart);
}
static long long T0_ns = 0;
long long pf_ns_start(void) { return T0_ns; }
void pf_time_reset(void) { T0_ns = pf_ns_now(); }
void pf_timestamp(char *msg) {
    long long t = pf_ns_now();
    printf("[+%7.3f ms] %s\n", (double) (long long) (t - T0_ns) / 1e6, msg ? msg : "");
}

void pf_start_logging(void) {
    // redirect logging to a file
    setvbuf(stdout, NULL, _IOFBF, 0);
    setvbuf(stderr, NULL, _IONBF, 0);
    freopen("rome.log", "w", stdout);
    freopen("rome.log", "a", stderr);
    pf_timestamp("Start logging");
}

struct win32_window {
    HWND hwnd;
    HINSTANCE hinst;
    int w, h;
    int mouse_x, mouse_y;
    int visible;
    KEYBOARD_CB on_key;
    MOUSE_CB on_mouse;
    void *cb_ud;
    long long refresh_ns;
} w;

static LRESULT CALLBACK wndproc(HWND, UINT, WPARAM, LPARAM);

int pf_window_width() {
    return w.w;
}
int pf_window_height() {
    return w.h;
}
void *pf_surface_or_hwnd() {
    return w.hwnd;
}
void *pf_display_or_instance() {
    return w.hinst;
}
int pf_window_visible() {
    return w.visible;
}

int pf_poll_events() {
    MSG msg;
    while (PeekMessageW(&msg, NULL, 0, 0, PM_REMOVE)) {
        if (msg.message == WM_QUIT) { w.visible = 0; }
        TranslateMessage(&msg);
        DispatchMessageW(&msg);
    }
    return 1;
}

void pf_sleep(const long long ms) {
    Sleep(ms);
}

void pf_create_window(char *app_name, void *ud, KEYBOARD_CB key_cb, MOUSE_CB mouse_cb) {
    HINSTANCE hi = GetModuleHandleW(NULL);

    /* estimate refresh (best-effort, default 60 Hz) */
    long long refresh_ns = 16666667ull;
    DEVMODEW dm = {0};
    dm.dmSize = sizeof(dm);
    if (EnumDisplaySettingsW(NULL, ENUM_CURRENT_SETTINGS, &dm) && dm.dmDisplayFrequency >= 30) {
        double hz = (double) dm.dmDisplayFrequency;
        refresh_ns = (long long) (1e9 / (hz > 1.0 ? hz : 60.0));
    }

    /* register class (idempotent ok) */
    WNDCLASSW wc = {0};
    wc.lpfnWndProc = wndproc;
    wc.hInstance = hi;
    wc.lpszClassName = L"pf_win32_cls";
    wc.hCursor = LoadCursorW(NULL, (LPCWSTR) IDC_ARROW);
    RegisterClassW(&wc);

    /* primary monitor size */
    int sw = GetSystemMetrics(SM_CXSCREEN);
    int sh = GetSystemMetrics(SM_CYSCREEN);

    DWORD style = WS_POPUP;
    DWORD exstyle = WS_EX_APPWINDOW;

    wchar_t app_name_16[64]; // convert the name of the window to 16 bit
    MultiByteToWideChar(
        65001, 0,
        app_name, -1,
        app_name_16, 64
    );
    HWND hwnd = CreateWindowExW(
        exstyle, wc.lpszClassName, app_name_16, style,
        0, 0, sw, sh, NULL, NULL, hi, NULL);

    if (!hwnd) {
        printf("CreateWindowEx failed\n");
        ExitProcess(1);
    }

    w.hwnd = hwnd;
    w.hinst = hi;
    w.w = sw;
    w.h = sh;
    w.visible = 0;
    w.on_key = key_cb;
    w.on_mouse = mouse_cb;
    w.cb_ud = ud;
    w.refresh_ns = refresh_ns;

    SetWindowLongPtrW(hwnd, GWLP_USERDATA, (LONG_PTR) &w);

    // fullscreen
    // todo: exclusive fullscreen (?)
    SetWindowPos(hwnd, HWND_TOP, 0, 0, sw, sh, SWP_SHOWWINDOW);
    ShowWindow(hwnd, SW_SHOWMAXIMIZED);
    SetForegroundWindow(hwnd);

    /* hide decorations already absent with WS_POPUP; ensure covers taskbar */
    MONITORINFO mi = {.cbSize = sizeof(mi)};
    GetMonitorInfoW(MonitorFromWindow(hwnd, MONITOR_DEFAULTTOPRIMARY), &mi);
    SetWindowPos(hwnd, HWND_TOPMOST,
                 mi.rcMonitor.left, mi.rcMonitor.top,
                 mi.rcMonitor.right - mi.rcMonitor.left,
                 mi.rcMonitor.bottom - mi.rcMonitor.top,
                 SWP_NOOWNERZORDER | SWP_FRAMECHANGED | SWP_SHOWWINDOW);
    pf_timestamp("win32 window ready");
}

void pf_destroy_window() {
    // todo: proper cleanup
}

static enum BUTTON vk_to_button(WPARAM vk) {
    switch (vk) {
        case VK_ESCAPE: return KEYBOARD_ESCAPE;

        /* main editing / whitespace */
        case VK_RETURN: return KEYBOARD_ENTER; /* numpad Enter is also VK_RETURN with extended flag */
        case VK_BACK: return KEYBOARD_BACKSPACE;
        case VK_TAB: return KEYBOARD_TAB;
        case VK_SPACE: return KEYBOARD_SPACE;

        /* arrows */
        case VK_LEFT: return KEYBOARD_LEFT;
        case VK_RIGHT: return KEYBOARD_RIGHT;
        case VK_UP: return KEYBOARD_UP;
        case VK_DOWN: return KEYBOARD_DOWN;

        /* navigation / editing */
        case VK_HOME: return KEYBOARD_HOME;
        case VK_END: return KEYBOARD_END;
        case VK_PRIOR: return KEYBOARD_PAGEUP; /* Page Up */
        case VK_NEXT: return KEYBOARD_PAGEDOWN; /* Page Down */
        case VK_INSERT: return KEYBOARD_INSERT;
        case VK_DELETE: return KEYBOARD_DELETE;

        /* function row */
        case VK_F1: return KEYBOARD_F1;
        case VK_F2: return KEYBOARD_F2;
        case VK_F3: return KEYBOARD_F3;
        case VK_F4: return KEYBOARD_F4;
        case VK_F5: return KEYBOARD_F5;
        case VK_F6: return KEYBOARD_F6;
        case VK_F7: return KEYBOARD_F7;
        case VK_F8: return KEYBOARD_F8;
        case VK_F9: return KEYBOARD_F9;
        case VK_F10: return KEYBOARD_F10;
        case VK_F11: return KEYBOARD_F11;
        case VK_F12: return KEYBOARD_F12;

        /* modifiers (map both sides to the same logical buttons, like Wayland path) */
        case VK_SHIFT:
        case VK_LSHIFT:
        case VK_RSHIFT: return KEYBOARD_SHIFT;
        case VK_CONTROL:
        case VK_LCONTROL:
        case VK_RCONTROL: return KEYBOARD_CTRL;
        case VK_MENU: /* Alt */
        case VK_LMENU:
        case VK_RMENU: return KEYBOARD_ALT;
        case VK_LWIN:
        case VK_RWIN: return KEYBOARD_SUPER;

        /* digits (top row) */
        case '0': return KEYBOARD_0;
        case '1': return KEYBOARD_1;
        case '2': return KEYBOARD_2;
        case '3': return KEYBOARD_3;
        case '4': return KEYBOARD_4;
        case '5': return KEYBOARD_5;
        case '6': return KEYBOARD_6;
        case '7': return KEYBOARD_7;
        case '8': return KEYBOARD_8;
        case '9': return KEYBOARD_9;

        /* letters */
        case 'A': return KEYBOARD_A;
        case 'B': return KEYBOARD_B;
        case 'C': return KEYBOARD_C;
        case 'D': return KEYBOARD_D;
        case 'E': return KEYBOARD_E;
        case 'F': return KEYBOARD_F;
        case 'G': return KEYBOARD_G;
        case 'H': return KEYBOARD_H;
        case 'I': return KEYBOARD_I;
        case 'J': return KEYBOARD_J;
        case 'K': return KEYBOARD_K;
        case 'L': return KEYBOARD_L;
        case 'M': return KEYBOARD_M;
        case 'N': return KEYBOARD_N;
        case 'O': return KEYBOARD_O;
        case 'P': return KEYBOARD_P;
        case 'Q': return KEYBOARD_Q;
        case 'R': return KEYBOARD_R;
        case 'S': return KEYBOARD_S;
        case 'T': return KEYBOARD_T;
        case 'U': return KEYBOARD_U;
        case 'V': return KEYBOARD_V;
        case 'W': return KEYBOARD_W;
        case 'X': return KEYBOARD_X;
        case 'Y': return KEYBOARD_Y;
        case 'Z': return KEYBOARD_Z;

        default: return BUTTON_UNKNOWN;
    }
}

static void emit_key(struct win32_window *w, WPARAM vk, int pressed) {
    if (!w || !w->on_key) return;
    w->on_key(w->cb_ud, vk_to_button(vk), pressed ? PRESSED : RELEASED);
}

static void emit_mouse_button(struct win32_window *w, enum BUTTON b, int pressed) {
    if (!w || !w->on_mouse) return;
    w->on_mouse(w->cb_ud, w->mouse_x, w->mouse_y, b, pressed ? PRESSED : RELEASED);
}

static void emit_mouse_move(struct win32_window *w, int x, int y) {
    if (!w || !w->on_mouse) return;
    w->on_mouse(w->cb_ud, x, y, MOUSE_MOVED, RELEASED);
}

static LRESULT CALLBACK wndproc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    struct win32_window *wnd = (struct win32_window *) GetWindowLongPtrW(hwnd, GWLP_USERDATA);
    switch (msg) {
        case WM_SHOWWINDOW: {
            printf("WM_SHOWWINDOW: Window was %s\n", (int) wParam ? "shown" : "hidden");
            if (wnd && (int) wParam) wnd->visible = 1;
            else if (wnd && (int) wParam == 0) wnd->visible = 0;
        } break;
        case WM_ACTIVATEAPP: {
            printf("WM_ACTIVATEAPP: Window became %s\n", (int) wParam ? "activated" : "deactivated");
            if (wParam) { if (wnd) wnd->visible = 1; }
            else if (wnd) wnd->visible = 0;
        } break;
        case WM_SIZE: {
            if (wParam == SIZE_MINIMIZED) {
                if (wnd) wnd->visible = 0;
                printf("WM_SIZE: Window became minimized\n");
            } else {if (wnd) wnd->visible = 1;}
            if (wnd) wnd->w = LOWORD(lParam);
            if (wnd) wnd->h = HIWORD(lParam);
        } break;
        case WM_WINDOWPOSCHANGED: {
            const WINDOWPOS *wp = (const WINDOWPOS *)lParam;
            if (wp->flags & SWP_SHOWWINDOW && wnd) {
                wnd->visible = 1;
                printf("WM_WINDOWPOSCHANGED: Window shown\n");
            } else if (wp->flags & SWP_HIDEWINDOW && wnd) {
                wnd->visible = 0;
                printf("WM_WINDOWPOSCHANGED: Window hidden\n");
            }
        } break;
        case WM_MOUSEWHEEL:
            if (wnd) {
                // Position in WM_MOUSEWHEEL is in screen coords; convert to client
                POINT pt = {GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam)};
                ScreenToClient(hwnd, &pt);
                wnd->mouse_x = pt.x;
                wnd->mouse_y = pt.y;
                // Convert WHEEL_DELTA units to step count, with remainder for smooth wheels
                static int v_remainder = 0;
                int delta = GET_WHEEL_DELTA_WPARAM(wParam); // multiples of 120
                v_remainder += delta;
                int steps = v_remainder / WHEEL_DELTA; // signed
                v_remainder -= steps * WHEEL_DELTA;
                if (steps != 0 && wnd->on_mouse) {
                    // NOTE: for scroll, we pass 'steps' in the 'pressed' parameter
                    wnd->on_mouse(wnd->cb_ud, wnd->mouse_x, wnd->mouse_y, MOUSE_SCROLL, -steps * 10);
                }
                return 0; // prevent default scrolling (like scrolling inactive window behind)
            }
            break;
        case 0x020e: // WM_MOUSEHWHEEL doesn't exist in tcc headers
            if (wnd) {
                POINT pt = {GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam)};
                ScreenToClient(hwnd, &pt);
                wnd->mouse_x = pt.x;
                wnd->mouse_y = pt.y;
                static int h_remainder = 0;
                int delta = GET_WHEEL_DELTA_WPARAM(wParam);
                h_remainder += delta;
                int steps = h_remainder / WHEEL_DELTA; // signed
                h_remainder -= steps * WHEEL_DELTA;
                if (steps != 0 && wnd->on_mouse) {
                    wnd->on_mouse(wnd->cb_ud, wnd->mouse_x, wnd->mouse_y, MOUSE_SCROLL_SIDE, -steps * 100);
                }
                return 0;
            }
            break;
        case WM_MOUSEMOVE:
            if (wnd) {
                wnd->mouse_x = GET_X_LPARAM(lParam);
                wnd->mouse_y = GET_Y_LPARAM(lParam);
                emit_mouse_move(wnd, wnd->mouse_x, wnd->mouse_y);
            }
            break;
        case WM_LBUTTONDOWN: if (wnd) emit_mouse_button(wnd, MOUSE_LEFT, 1);
            break;
        case WM_LBUTTONUP: if (wnd) emit_mouse_button(wnd, MOUSE_LEFT, 0);
            break;
        case WM_RBUTTONDOWN: if (wnd) emit_mouse_button(wnd, MOUSE_RIGHT, 1);
            break;
        case WM_RBUTTONUP: if (wnd) emit_mouse_button(wnd, MOUSE_RIGHT, 0);
            break;
        case WM_MBUTTONDOWN: if (wnd) emit_mouse_button(wnd, MOUSE_MIDDLE, 1);
            break;
        case WM_MBUTTONUP: if (wnd) emit_mouse_button(wnd, MOUSE_MIDDLE, 0);
            break;
        case WM_KEYDOWN: if (wnd) {
                emit_key(wnd, wParam, 1);
                return 0;
            }
            break;
        case WM_KEYUP: if (wnd) {
                emit_key(wnd, wParam, 0);
                return 0;
            }
            break;
        case WM_SYSKEYDOWN: if (wnd) {
                emit_key(wnd, wParam, 1);
                return 0;
            }
            break;
        case WM_SYSKEYUP: if (wnd) {
                emit_key(wnd, wParam, 0);
                return 0;
            }
            break;
        case WM_CLOSE: DestroyWindow(hwnd);
            return 0;
        case WM_DESTROY: PostQuitMessage(0);
            return 0;
        default: break;
    }
    return DefWindowProcW(hwnd, msg, wParam, lParam);
}
#endif
