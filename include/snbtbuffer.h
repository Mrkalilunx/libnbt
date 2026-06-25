#ifndef SNBTBUF_H
#define SNBTBUF_H

#include "nbtbuffer.h"
#include <stddef.h>
#include <stdarg.h>

// 基于 Buffer 的 SNBT 文本缓冲区，自动维护 \0 终止
typedef Buffer SnbtBuf;

// 初始化，初始容量 initial_cap 字节，成功返回 1，失败返回 0
static inline int snbtbuf_init(SnbtBuf* sb, size_t initial_cap) {
    return buf_init(sb, initial_cap);
}

// 释放缓冲区
static inline void snbtbuf_free(SnbtBuf* sb) {
    buf_free(sb);
}

// 追加 len 字节数据，成功返回 1，失败返回 0
static inline int snbtbuf_append(SnbtBuf* sb, const char* src, size_t len) {
    return buf_wrbytes(sb, (const uint8_t*)src, len);
}

// 追加一个字符，成功返回 1，失败返回 0
static inline int snbtbuf_putc(SnbtBuf* sb, char c) {
    return buf_wru8(sb, (uint8_t)c);
}

// 追加 null 终止字符串，成功返回 1，失败返回 0
static inline int snbtbuf_puts(SnbtBuf* sb, const char* s) {
    return buf_wrbytes(sb, (const uint8_t*)s, strlen(s));
}

// 格式化追加，成功返回 1，失败返回 0
int snbtbuf_printf(SnbtBuf* sb, const char* fmt, ...);

// 确保末尾有 \0 并返回 C 字符串指针
static inline char* snbtbuf_cstr(SnbtBuf* sb) {
    if (!sb || !sb->data) return NULL;
    if (sb->used >= sb->cap) {
        if (!buf_grow(sb, sb->used + 1)) return NULL;
    }
    sb->data[sb->used] = '\0';
    return (char*)sb->data;
}

// 返回当前内容长度
static inline size_t snbtbuf_len(const SnbtBuf* sb) {
    return sb ? sb->used : 0;
}

// 重置缓冲区（清空内容，不释放内存）
static inline void snbtbuf_reset(SnbtBuf* sb) {
    buf_reset(sb);
}

// 夺取内部数据指针（调用者负责 free），之后 snbtbuf_free 不再释放数据
static inline char* snbtbuf_steal(SnbtBuf* sb) {
    if (!sb) return NULL;
    char* result = (char*)sb->data;
    sb->data = NULL;
    sb->cap = 0;
    sb->used = 0;
    return result;
}

#endif // SNBTBUF_H
