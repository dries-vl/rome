#ifdef _WIN32
#include "platform.h"

#include <stdio.h>

#ifndef WINAPI
#  if defined(_M_IX86) || defined(__i386__)
#    define WINAPI __stdcall
#  else
#    define WINAPI
#  endif
#endif

#ifndef CALLBACK
#  define CALLBACK WINAPI
#endif

#ifndef APIENTRY
#  define APIENTRY WINAPI
#endif

#ifndef CONST
#  define CONST const
#endif

#ifndef NULL
#  define NULL ((void*)0)
#endif

#ifndef TRUE
#  define TRUE 1
#  define FALSE 0
#endif

#ifndef PF_WINAPI_IMPORT
#  if defined(_MSC_VER)
#    define PF_WINAPI_IMPORT __declspec(dllimport)
#  else
#    define PF_WINAPI_IMPORT
#  endif
#endif

/* ---------- exact-width-ish ABI types without stdint.h ---------- */

typedef void VOID;
typedef int BOOL;
typedef unsigned char BYTE;
typedef unsigned short WORD;
typedef unsigned int UINT;
typedef short SHORT;
typedef long LONG;
typedef unsigned long DWORD;
typedef long long LONGLONG;
typedef unsigned long long ULONGLONG;

#if defined(_WIN64) || defined(__x86_64__) || defined(__aarch64__)
typedef long long LONG_PTR;
typedef unsigned long long ULONG_PTR;
#else
typedef long LONG_PTR;
typedef unsigned long ULONG_PTR;
#endif

typedef ULONG_PTR DWORD_PTR;
typedef ULONG_PTR UINT_PTR;
typedef LONG_PTR INT_PTR;
typedef UINT_PTR WPARAM;
typedef LONG_PTR LPARAM;
typedef LONG_PTR LRESULT;

typedef WORD ATOM;
typedef void *HANDLE;
typedef HANDLE HWND;
typedef HANDLE HINSTANCE;
typedef HANDLE HMODULE;
typedef HANDLE HICON;
typedef HANDLE HCURSOR;
typedef HANDLE HBRUSH;
typedef HANDLE HMENU;
typedef HANDLE HMONITOR;
typedef const char *LPCSTR;
typedef const unsigned short *LPCWSTR;
typedef unsigned short *LPWSTR;
typedef const char *LPCCH;
typedef void *LPVOID;

/* ---------- structs ---------- */

typedef struct tagPOINT {
    LONG x;
    LONG y;
} POINT;

typedef struct tagRECT {
    LONG left;
    LONG top;
    LONG right;
    LONG bottom;
} RECT;

typedef union _LARGE_INTEGER {
    struct {
        DWORD LowPart;
        LONG HighPart;
    };

    LONGLONG QuadPart;
} LARGE_INTEGER;

typedef struct tagMSG {
    HWND hwnd;
    UINT message;
    WPARAM wParam;
    LPARAM lParam;
    DWORD time;
    POINT pt;
} MSG;

typedef LRESULT (CALLBACK *WNDPROC)(HWND, UINT, WPARAM, LPARAM);

typedef struct tagWNDCLASSW {
    UINT style;
    WNDPROC lpfnWndProc;
    int cbClsExtra;
    int cbWndExtra;
    HINSTANCE hInstance;
    HICON hIcon;
    HCURSOR hCursor;
    HBRUSH hbrBackground;
    LPCWSTR lpszMenuName;
    LPCWSTR lpszClassName;
} WNDCLASSW;

typedef struct tagMONITORINFO {
    DWORD cbSize;
    RECT rcMonitor;
    RECT rcWork;
    DWORD dwFlags;
} MONITORINFO;

/* Must match Win32 layout sufficiently for EnumDisplaySettingsW */
typedef struct _DEVMODEW {
    unsigned short dmDeviceName[32];
    WORD dmSpecVersion;
    WORD dmDriverVersion;
    WORD dmSize;
    WORD dmDriverExtra;
    DWORD dmFields;

    LONG dmPosition_x;
    LONG dmPosition_y;
    DWORD dmDisplayOrientation;
    DWORD dmDisplayFixedOutput;

    SHORT dmColor;
    SHORT dmDuplex;
    SHORT dmYResolution;
    SHORT dmTTOption;
    SHORT dmCollate;
    unsigned short dmFormName[32];
    WORD dmLogPixels;
    DWORD dmBitsPerPel;
    DWORD dmPelsWidth;
    DWORD dmPelsHeight;
    DWORD dmDisplayFlags;
    DWORD dmDisplayFrequency;

    DWORD dmICMMethod;
    DWORD dmICMIntent;
    DWORD dmMediaType;
    DWORD dmDitherType;
    DWORD dmReserved1;
    DWORD dmReserved2;
    DWORD dmPanningWidth;
    DWORD dmPanningHeight;
} DEVMODEW;

typedef struct _SECURITY_ATTRIBUTES {
    DWORD nLength;
    LPVOID lpSecurityDescriptor;
    BOOL bInheritHandle;
} SECURITY_ATTRIBUTES, *PSECURITY_ATTRIBUTES, *LPSECURITY_ATTRIBUTES;

/* ---------- constants ---------- */

#define WS_POPUP                    0x80000000L
#define WS_EX_APPWINDOW             0x00040000L

#define SW_SHOW                     5
#define SW_SHOWMAXIMIZED            3

#define SWP_NOSIZE                  0x0001
#define SWP_NOMOVE                  0x0002
#define SWP_NOZORDER                0x0004
#define SWP_NOOWNERZORDER           0x0200
#define SWP_FRAMECHANGED            0x0020
#define SWP_SHOWWINDOW              0x0040

#define PM_REMOVE                   0x0001

#define GWLP_USERDATA               (-21)
#define CP_UTF8 65001
#define SM_CXSCREEN                 0
#define SM_CYSCREEN                 1

#define MONITOR_DEFAULTTOPRIMARY    0x00000001
#define ENUM_CURRENT_SETTINGS       ((DWORD)-1)

#define WM_SIZE                     0x0005
#define WM_CLOSE                    0x0010
#define WM_QUIT                     0x0012
#define WM_SHOWWINDOW               0x0018
#define WM_KEYDOWN                  0x0100
#define WM_KEYUP                    0x0101
#define WM_SYSKEYDOWN               0x0104
#define WM_SYSKEYUP                 0x0105
#define WM_MOUSEMOVE                0x0200
#define WM_LBUTTONDOWN              0x0201
#define WM_LBUTTONUP                0x0202
#define WM_RBUTTONDOWN              0x0204
#define WM_RBUTTONUP                0x0205
#define WM_MBUTTONDOWN              0x0207
#define WM_MBUTTONUP                0x0208
#define WM_MOUSEWHEEL               0x020A
#define WM_MOUSEHWHEEL              0x020E
#define WM_DESTROY                  0x0002

#define VK_BACK                     0x08
#define VK_TAB                      0x09
#define VK_RETURN                   0x0D
#define VK_SHIFT                    0x10
#define VK_CONTROL                  0x11
#define VK_MENU                     0x12
#define VK_ESCAPE                   0x1B
#define VK_SPACE                    0x20
#define VK_PRIOR                    0x21
#define VK_NEXT                     0x22
#define VK_END                      0x23
#define VK_HOME                     0x24
#define VK_LEFT                     0x25
#define VK_UP                       0x26
#define VK_RIGHT                    0x27
#define VK_DOWN                     0x28
#define VK_INSERT                   0x2D
#define VK_DELETE                   0x2E
#define VK_LWIN                     0x5B
#define VK_RWIN                     0x5C
#define VK_LSHIFT                   0xA0
#define VK_RSHIFT                   0xA1
#define VK_LCONTROL                 0xA2
#define VK_RCONTROL                 0xA3
#define VK_LMENU                    0xA4
#define VK_RMENU                    0xA5
#define VK_F1                       0x70
#define VK_F2                       0x71
#define VK_F3                       0x72
#define VK_F4                       0x73
#define VK_F5                       0x74
#define VK_F6                       0x75
#define VK_F7                       0x76
#define VK_F8                       0x77
#define VK_F9                       0x78
#define VK_F10                      0x79
#define VK_F11                      0x7A
#define VK_F12                      0x7B

#define WHEEL_DELTA                 120

#define IDC_ARROW                   ((LPCWSTR)((ULONG_PTR)32512))
#define HWND_TOP                    ((HWND)0)
#define HWND_TOPMOST                ((HWND)(LONG_PTR)-1)

/* ---------- helper macros ---------- */

#define LOWORD(l) ((WORD)((DWORD_PTR)(l) & 0xffff))
#define HIWORD(l) ((WORD)(((DWORD_PTR)(l) >> 16) & 0xffff))
#define GET_X_LPARAM(lp) ((int)(short)LOWORD(lp))
#define GET_Y_LPARAM(lp) ((int)(short)HIWORD(lp))
#define GET_WHEEL_DELTA_WPARAM(wp) ((short)HIWORD(wp))

/* ---------- functions ---------- */

PF_WINAPI_IMPORT BOOL WINAPI QueryPerformanceFrequency(LARGE_INTEGER *);

PF_WINAPI_IMPORT BOOL WINAPI QueryPerformanceCounter(LARGE_INTEGER *);

PF_WINAPI_IMPORT HMODULE WINAPI GetModuleHandleW(LPCWSTR);

PF_WINAPI_IMPORT ATOM WINAPI RegisterClassW(const WNDCLASSW *);

PF_WINAPI_IMPORT int WINAPI GetSystemMetrics(int);

PF_WINAPI_IMPORT int WINAPI MultiByteToWideChar(UINT, DWORD, LPCCH, int, LPWSTR, int);

PF_WINAPI_IMPORT HWND WINAPI CreateWindowExW(
    DWORD, LPCWSTR, LPCWSTR, DWORD, int, int, int, int,
    HWND, HMENU, HINSTANCE, LPVOID);

PF_WINAPI_IMPORT LONG_PTR WINAPI SetWindowLongPtrW(HWND, int, LONG_PTR);

PF_WINAPI_IMPORT LONG_PTR WINAPI GetWindowLongPtrW(HWND, int);

PF_WINAPI_IMPORT BOOL WINAPI SetWindowPos(HWND, HWND, int, int, int, int, UINT);

PF_WINAPI_IMPORT BOOL WINAPI ShowWindow(HWND, int);

PF_WINAPI_IMPORT BOOL WINAPI SetForegroundWindow(HWND);

PF_WINAPI_IMPORT HMONITOR WINAPI MonitorFromWindow(HWND, DWORD);

PF_WINAPI_IMPORT BOOL WINAPI GetMonitorInfoW(HMONITOR, MONITORINFO *);

PF_WINAPI_IMPORT HCURSOR WINAPI LoadCursorW(HINSTANCE, LPCWSTR);

PF_WINAPI_IMPORT BOOL WINAPI PeekMessageW(MSG *, HWND, UINT, UINT, UINT);

PF_WINAPI_IMPORT BOOL WINAPI TranslateMessage(const MSG *);

PF_WINAPI_IMPORT LRESULT WINAPI DispatchMessageW(const MSG *);

PF_WINAPI_IMPORT LRESULT WINAPI DefWindowProcW(HWND, UINT, WPARAM, LPARAM);

PF_WINAPI_IMPORT BOOL WINAPI DestroyWindow(HWND);

PF_WINAPI_IMPORT void WINAPI PostQuitMessage(int);

PF_WINAPI_IMPORT BOOL WINAPI EnumDisplaySettingsW(LPCWSTR, DWORD, DEVMODEW *);

PF_WINAPI_IMPORT BOOL WINAPI ScreenToClient(HWND, POINT *);

PF_WINAPI_IMPORT void WINAPI ExitProcess(UINT);

#ifndef WM_MOUSEHWHEEL
#define WM_MOUSEHWHEEL 0x020E
#endif

#ifndef GET_X_LPARAM
#define GET_X_LPARAM(lp) ((int)(short)LOWORD(lp))
#endif
#ifndef GET_Y_LPARAM
#define GET_Y_LPARAM(lp) ((int)(short)HIWORD(lp))
#endif

/* --- high-resolution clock --- */
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

/* --- window state --- */
struct win32_window {
    HWND hwnd;
    HINSTANCE hinst;
    int w, h;
    int mouse_x, mouse_y;
    int visible;

    /* callbacks */
    KEYBOARD_CB on_key;
    MOUSE_CB on_mouse;
    void *cb_ud;

    /* timing (rough; default 60 Hz) */
    long long refresh_ns;
} w;

/* forward decl for wndproc */
static LRESULT CALLBACK wndproc(HWND, UINT, WPARAM, LPARAM);

/* --- contract helpers --- */
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

/* --- event pump (non-blocking) --- */
int pf_poll_events() {
    MSG msg;
    while (PeekMessageW(&msg, NULL, 0, 0, PM_REMOVE)) {
        if (msg.message == WM_QUIT) { w.visible = 0; }
        TranslateMessage(&msg);
        DispatchMessageW(&msg);
    }
    return 1;
}

/* --- present feedback stub (no-op on pure Win32; keep parity) --- */
void pf_request_present_feedback(long long frame_id) { (void) frame_id; }

/* --- create fullscreen borderless window --- */
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
        CP_UTF8, 0,
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

    /* make it topmost fullscreen */
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

    pf_time_reset();
    pf_timestamp("win32 window ready");
}

/* --- input helpers --- */
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

/* --- WndProc --- */
static LRESULT CALLBACK wndproc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    struct win32_window *w = (struct win32_window *) GetWindowLongPtrW(hwnd, GWLP_USERDATA);
    switch (msg) {
        case WM_SHOWWINDOW: if (w) w->visible = (int) wParam;
            break;
        case WM_SIZE:
            if (w) {
                w->w = LOWORD(lParam);
                w->h = HIWORD(lParam);
            }
            break;

        /* mouse */
        /* scrolling (vertical + horizontal) */
        case WM_MOUSEWHEEL:
            if (w) {
                // Position in WM_MOUSEWHEEL is in screen coords; convert to client
                POINT pt = {GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam)};
                ScreenToClient(hwnd, &pt);
                w->mouse_x = pt.x;
                w->mouse_y = pt.y;

                // Convert WHEEL_DELTA units to step count, with remainder for smooth wheels
                static int v_remainder = 0;
                int delta = GET_WHEEL_DELTA_WPARAM(wParam); // multiples of 120
                v_remainder += delta;
                int steps = v_remainder / WHEEL_DELTA; // signed
                v_remainder -= steps * WHEEL_DELTA;

                if (steps != 0 && w->on_mouse) {
                    // NOTE: for scroll, we pass 'steps' in the 'pressed' parameter
                    w->on_mouse(w->cb_ud, w->mouse_x, w->mouse_y, MOUSE_SCROLL, -steps * 10);
                }
                return 0; // prevent default scrolling (like scrolling inactive window behind)
            }
            break;

        case WM_MOUSEHWHEEL:
            if (w) {
                POINT pt = {GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam)};
                ScreenToClient(hwnd, &pt);
                w->mouse_x = pt.x;
                w->mouse_y = pt.y;

                static int h_remainder = 0;
                int delta = GET_WHEEL_DELTA_WPARAM(wParam);
                h_remainder += delta;
                int steps = h_remainder / WHEEL_DELTA; // signed
                h_remainder -= steps * WHEEL_DELTA;

                if (steps != 0 && w->on_mouse) {
                    w->on_mouse(w->cb_ud, w->mouse_x, w->mouse_y, MOUSE_SCROLL_SIDE, -steps * 100);
                }
                return 0;
            }
            break;

        case WM_MOUSEMOVE:
            if (w) {
                w->mouse_x = GET_X_LPARAM(lParam);
                w->mouse_y = GET_Y_LPARAM(lParam);
                emit_mouse_move(w, w->mouse_x, w->mouse_y);
            }
            break;
        case WM_LBUTTONDOWN: if (w) emit_mouse_button(w, MOUSE_LEFT, 1);
            break;
        case WM_LBUTTONUP: if (w) emit_mouse_button(w, MOUSE_LEFT, 0);
            break;
        case WM_RBUTTONDOWN: if (w) emit_mouse_button(w, MOUSE_RIGHT, 1);
            break;
        case WM_RBUTTONUP: if (w) emit_mouse_button(w, MOUSE_RIGHT, 0);
            break;
        case WM_MBUTTONDOWN: if (w) emit_mouse_button(w, MOUSE_MIDDLE, 1);
            break;
        case WM_MBUTTONUP: if (w) emit_mouse_button(w, MOUSE_MIDDLE, 0);
            break;

        /* keyboard – include SYSKEY to catch Alt/F10 and avoid system menu */
        case WM_KEYDOWN: if (w) {
                emit_key(w, wParam, 1);
                return 0;
            }
            break;
        case WM_KEYUP: if (w) {
                emit_key(w, wParam, 0);
                return 0;
            }
            break;
        case WM_SYSKEYDOWN: if (w) {
                emit_key(w, wParam, 1);
                return 0;
            }
            break;
        case WM_SYSKEYUP: if (w) {
                emit_key(w, wParam, 0);
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
