#include "snbtbuffer.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int snbtbuf_printf(SnbtBuf* sb, const char* fmt, ...) {
    va_list ap;
    va_start(ap, fmt);
    int needed = vsnprintf(NULL, 0, fmt, ap);
    va_end(ap);
    if (needed < 0) return 0;

    if (sb->used + (size_t)needed > sb->cap) {
        if (!buf_grow(sb, sb->used + (size_t)needed)) return 0;
    }

    va_start(ap, fmt);
    int written = vsnprintf((char*)sb->data + sb->used, (size_t)needed + 1, fmt, ap);
    va_end(ap);
    if (written < 0 || written != needed) return 0;

    sb->used += (size_t)written;
    return 1;
}
