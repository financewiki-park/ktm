/* Minimal dynamic Xlib client.  Kindle packages do not ship Xlib headers. */
#define _DEFAULT_SOURCE
#include "x11_paste.h"
#include <dlfcn.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

typedef struct _XDisplay Display;
typedef unsigned long XID;
typedef XID Window;
typedef XID KeySym;
typedef int Bool;
typedef struct {
    int type; unsigned long serial; Bool send_event; Display *display;
    Window window, root, subwindow; unsigned long time;
    int x, y, x_root, y_root; unsigned int state, keycode; Bool same_screen;
} XKeyEvent;
typedef union { int type; XKeyEvent xkey; long pad[24]; } XEvent;
enum { XKeyPress = 2, XKeyRelease = 3, XNone = 0, XTrue = 1 };
enum { XKeyPressMask = 1L << 0, XKeyReleaseMask = 1L << 1 };

typedef struct {
    Display *(*open_display)(const char *);
    int (*close_display)(Display *);
    int (*display_keycodes)(Display *, int *, int *);
    int (*get_input_focus)(Display *, Window *, int *);
    KeySym *(*get_keyboard_mapping)(Display *, int, int, int *);
    int (*change_keyboard_mapping)(Display *, int, int, KeySym *, int);
    int (*send_event)(Display *, Window, Bool, long, XEvent *);
    int (*flush)(Display *);
    int (*free_mem)(void *);
} X11;

static int load_x11(void **lib, X11 *x) {
    *lib = dlopen("libX11.so.6", RTLD_NOW | RTLD_LOCAL);
    if (!*lib) return 0;
#define LOAD(field, symbol) do { *(void **)(&x->field) = dlsym(*lib, symbol); if (!x->field) { dlclose(*lib); return 0; } } while (0)
    LOAD(open_display, "XOpenDisplay"); LOAD(close_display, "XCloseDisplay");
    LOAD(display_keycodes, "XDisplayKeycodes"); LOAD(get_input_focus, "XGetInputFocus"); LOAD(get_keyboard_mapping, "XGetKeyboardMapping");
    LOAD(change_keyboard_mapping, "XChangeKeyboardMapping"); LOAD(send_event, "XSendEvent"); LOAD(flush, "XFlush");
    LOAD(free_mem, "XFree");
#undef LOAD
    return 1;
}

static void send_key(X11 *x, Display *d, Window w, int code) {
    XEvent e; memset(&e, 0, sizeof(e));
    e.xkey.display = d; e.xkey.window = w; e.xkey.keycode = (unsigned int)code; e.xkey.same_screen = XTrue;
    e.type = XKeyPress; e.xkey.type = XKeyPress;
    x->send_event(d, w, XTrue, XKeyPressMask | XKeyReleaseMask, &e);
    e.type = XKeyRelease; e.xkey.type = XKeyRelease;
    x->send_event(d, w, XTrue, XKeyPressMask | XKeyReleaseMask, &e);
}

static int next_utf8(const unsigned char **p, uint32_t *out) {
    const unsigned char *s = *p; uint32_t c;
    if (*s < 0x80) { *out = *s++; *p = s; return 1; }
    int n = (*s & 0xe0) == 0xc0 ? 2 : (*s & 0xf0) == 0xe0 ? 3 : (*s & 0xf8) == 0xf0 ? 4 : 0;
    if (!n) return 0; c = *s & ((1u << (8 - n - 1)) - 1);
    for (int i = 1; i < n; i++) { if ((s[i] & 0xc0) != 0x80) return 0; c = (c << 6) | (s[i] & 0x3f); }
    if ((n == 2 && c < 0x80) || (n == 3 && c < 0x800) || (n == 4 && (c < 0x10000 || c > 0x10ffff)) || (c >= 0xd800 && c <= 0xdfff)) return 0;
    *out = c; *p = s + n; return 1;
}

int x11_paste_text(const char *text, char *error, size_t error_size) {
    void *lib = NULL; X11 x; Display *d; Window w; int revert, min, max;
    const unsigned char *p = (const unsigned char *)text;
    const unsigned char *check = p; KeySym *old_mapping; int keysyms_per_code;
    /* Validate before changing the shared X keyboard mapping. */
    while (*check) {
        uint32_t cp;
        if (!next_utf8(&check, &cp)) { snprintf(error, error_size, "message is not valid UTF-8"); return -1; }
        if (cp < 0x20 || cp == 0x7f) { snprintf(error, error_size, "message contains a newline or control character"); return -1; }
    }
    if (!load_x11(&lib, &x)) { snprintf(error, error_size, "X11 library is unavailable"); return -1; }
    d = x.open_display(NULL);
    if (!d) { dlclose(lib); snprintf(error, error_size, "no Kindle X11 display"); return -1; }
    x.display_keycodes(d, &min, &max);
    if (max <= min) { x.close_display(d); dlclose(lib); snprintf(error, error_size, "X11 keyboard mapping unavailable"); return -1; }
    x.get_input_focus(d, &w, &revert);
    if (w == XNone) { x.close_display(d); dlclose(lib); snprintf(error, error_size, "KTerm has no input focus"); return -1; }
    old_mapping = x.get_keyboard_mapping(d, max, 1, &keysyms_per_code);
    if (!old_mapping || keysyms_per_code < 1) { if (old_mapping) x.free_mem(old_mapping); x.close_display(d); dlclose(lib); snprintf(error, error_size, "X11 keyboard mapping unavailable"); return -1; }
    while (*p) {
        uint32_t cp; KeySym sym;
        (void)next_utf8(&p, &cp);
        sym = cp <= 0xff ? (KeySym)cp : (KeySym)(0x01000000u | cp);
        x.change_keyboard_mapping(d, max, 1, &sym, 1);
        send_key(&x, d, w, max);
        x.flush(d);
        usleep(2500);
    }
    x.change_keyboard_mapping(d, max, keysyms_per_code, old_mapping, 1);
    x.flush(d);
    x.free_mem(old_mapping);
    x.close_display(d); dlclose(lib);
    return 0;
}
