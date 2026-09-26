#ifndef KTM_X11_PASTE_H
#define KTM_X11_PASTE_H

#include <stddef.h>

/* Type UTF-8 text into the focused X11 input without generating Return. */
int x11_paste_text(const char *text, char *error, size_t error_size);

#endif
