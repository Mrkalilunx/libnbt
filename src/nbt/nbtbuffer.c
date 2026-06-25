#include "nbtbuffer.h"
#include "nbtendian.h"
#include <stdlib.h>
#include <string.h>

int buf_init(Buffer* buf, size_t initial_cap) {
    if (!buf || initial_cap == 0) return 0;
    buf->data = (uint8_t*)malloc(initial_cap);
    if (!buf->data) return 0;
    buf->cap = initial_cap;
    buf->used = 0;
    buf->pos = 0;
    return 1;
}

void buf_free(Buffer* buf) {
    if (!buf) return;
    free(buf->data);
    buf->data = NULL;
    buf->cap = 0;
    buf->used = 0;
    buf->pos = 0;
}

int buf_ensure(const Buffer* buf, size_t need) {
    if (!buf) return 0;
    if (need > buf->cap - buf->used) return 0;
    return 1;
}

int buf_can_read(const Buffer* buf, size_t need) {
    if (!buf) return 0;
    if (need > buf->used - buf->pos) return 0;
    return 1;
}

int buf_grow(Buffer* buf, size_t min_cap) {
    if (!buf) return 0;
    if (min_cap <= buf->cap) return 1;
    size_t new_cap = buf->cap;
    if (new_cap <= SIZE_MAX / 2) new_cap *= 2;
    else new_cap = SIZE_MAX;
    if (new_cap < min_cap) new_cap = min_cap;
    uint8_t* new_data = (uint8_t*)realloc(buf->data, new_cap);
    if (!new_data) return 0;
    buf->data = new_data;
    buf->cap = new_cap;
    return 1;
}

void buf_rewind(Buffer* buf) {
    if (buf) buf->pos = 0;
}

void buf_reset(Buffer* buf) {
    if (buf) {
        buf->used = 0;
        buf->pos = 0;
    }
}

static int buf_require(Buffer* buf, size_t need) {
    if (!buf) return 0;
    if (need > SIZE_MAX - buf->used) return 0;
    size_t required = buf->used + need;
    if (required <= buf->cap) return 1;
    return buf_grow(buf, required);
}

// 生成直接写入函数（无 endian 参数）
#define BUF_WR_FUNC(name, size, type, wfunc) \
int name(Buffer* buf, type val) { \
    if (!buf_require(buf, size)) return 0; \
    uint8_t* ptr = buf->data + buf->used; \
    wfunc(&ptr, val); \
    buf->used = (size_t)(ptr - buf->data); \
    return 1; \
}

// 生成带字节序分发的写入函数
#define BUF_DISP_WR(name, size, type, wfunc) \
int name(Buffer* buf, type val, nbt_endian endian) { \
    if (!buf_require(buf, size)) return 0; \
    uint8_t* ptr = buf->data + buf->used; \
    if (!wfunc(&ptr, val, endian)) return 0; \
    buf->used = (size_t)(ptr - buf->data); \
    return 1; \
}

// 生成直接读取函数（无 endian 参数）
#define BUF_RD_FUNC(name, size, type, rfunc) \
int name(Buffer* buf, type* out) { \
    if (!buf_can_read(buf, size)) return 0; \
    const uint8_t* ptr = buf->data + buf->pos; \
    rfunc(&ptr, out); \
    buf->pos = (size_t)(ptr - buf->data); \
    return 1; \
}

// 生成带字节序分发的读取函数
#define BUF_DISP_RD(name, size, type, rfunc) \
int name(Buffer* buf, type* out, nbt_endian endian) { \
    if (!buf_can_read(buf, size)) return 0; \
    const uint8_t* ptr = buf->data + buf->pos; \
    if (!rfunc(&ptr, out, endian)) return 0; \
    buf->pos = (size_t)(ptr - buf->data); \
    return 1; \
}

// 8 位整数写入
BUF_WR_FUNC(buf_wru8, 1, uint8_t, wru8)
BUF_WR_FUNC(buf_wi8,  1, int8_t,  wi8)

// 大端写入
BUF_WR_FUNC(buf_wrbeu16, 2, uint16_t, wrbeu16)
BUF_WR_FUNC(buf_wrbei16, 2, int16_t,  wrbei16)
BUF_WR_FUNC(buf_wrbeu32, 4, uint32_t, wrbeu32)
BUF_WR_FUNC(buf_wrbei32, 4, int32_t,  wrbei32)
BUF_WR_FUNC(buf_wrbeu64, 8, uint64_t, wrbeu64)
BUF_WR_FUNC(buf_wrbei64, 8, int64_t,  wrbei64)
BUF_WR_FUNC(buf_wrbef32, 4, float,    wrbef32)
BUF_WR_FUNC(buf_wrbef64, 8, double,   wrbef64)

// 小端写入
BUF_WR_FUNC(buf_wrleu16, 2, uint16_t, wrleu16)
BUF_WR_FUNC(buf_wrlei16, 2, int16_t,  wrlei16)
BUF_WR_FUNC(buf_wrleu32, 4, uint32_t, wrleu32)
BUF_WR_FUNC(buf_wrlei32, 4, int32_t,  wrlei32)
BUF_WR_FUNC(buf_wrleu64, 8, uint64_t, wrleu64)
BUF_WR_FUNC(buf_wrlei64, 8, int64_t,  wrlei64)
BUF_WR_FUNC(buf_wrlef32, 4, float,    wrlef32)
BUF_WR_FUNC(buf_wrlef64, 8, double,   wrlef64)

// 带字节序分发的写入
BUF_DISP_WR(buf_wu16, 2, uint16_t, wu16)
BUF_DISP_WR(buf_wi16, 2, int16_t,  wi16)
BUF_DISP_WR(buf_wu32, 4, uint32_t, wu32)
BUF_DISP_WR(buf_wi32, 4, int32_t,  wi32)
BUF_DISP_WR(buf_wu64, 8, uint64_t, wu64)
BUF_DISP_WR(buf_wi64, 8, int64_t,  wi64)
BUF_DISP_WR(buf_wf32, 4, float,    wf32)
BUF_DISP_WR(buf_wf64, 8, double,   wf64)

// 8 位整数读取
BUF_RD_FUNC(buf_ru8, 1, uint8_t, ru8)
BUF_RD_FUNC(buf_ri8, 1, int8_t,  ri8)

// 大端读取
BUF_RD_FUNC(buf_rbeu16, 2, uint16_t, rbeu16)
BUF_RD_FUNC(buf_rbei16, 2, int16_t,  rbei16)
BUF_RD_FUNC(buf_rbeu32, 4, uint32_t, rbeu32)
BUF_RD_FUNC(buf_rbei32, 4, int32_t,  rbei32)
BUF_RD_FUNC(buf_rbeu64, 8, uint64_t, rbeu64)
BUF_RD_FUNC(buf_rbei64, 8, int64_t,  rbei64)
BUF_RD_FUNC(buf_rbef32, 4, float,    rbef32)
BUF_RD_FUNC(buf_rbef64, 8, double,   rbef64)

// 小端读取
BUF_RD_FUNC(buf_rleu16, 2, uint16_t, rleu16)
BUF_RD_FUNC(buf_rlei16, 2, int16_t,  rlei16)
BUF_RD_FUNC(buf_rleu32, 4, uint32_t, rleu32)
BUF_RD_FUNC(buf_rlei32, 4, int32_t,  rlei32)
BUF_RD_FUNC(buf_rleu64, 8, uint64_t, rleu64)
BUF_RD_FUNC(buf_rlei64, 8, int64_t,  rlei64)
BUF_RD_FUNC(buf_rlef32, 4, float,    rlef32)
BUF_RD_FUNC(buf_rlef64, 8, double,   rlef64)

// 带字节序分发的读取
BUF_DISP_RD(buf_ru16, 2, uint16_t, ru16)
BUF_DISP_RD(buf_ri16, 2, int16_t,  ri16)
BUF_DISP_RD(buf_ru32, 4, uint32_t, ru32)
BUF_DISP_RD(buf_ri32, 4, int32_t,  ri32)
BUF_DISP_RD(buf_ru64, 8, uint64_t, ru64)
BUF_DISP_RD(buf_ri64, 8, int64_t,  ri64)
BUF_DISP_RD(buf_rf32, 4, float,    rf32)
BUF_DISP_RD(buf_rf64, 8, double,   rf64)

// 字节块读写
int buf_wrbytes(Buffer* buf, const uint8_t* data, size_t len) {
    if (!buf_require(buf, len)) return 0;
    uint8_t* ptr = buf->data + buf->used;
    wrbytes(&ptr, data, len);
    buf->used = (size_t)(ptr - buf->data);
    return 1;
}

int buf_rdbytes(Buffer* buf, uint8_t* out_buf, size_t len) {
    if (!buf_can_read(buf, len)) return 0;
    const uint8_t* ptr = buf->data + buf->pos;
    rdbytes(&ptr, out_buf, len);
    buf->pos = (size_t)(ptr - buf->data);
    return 1;
}