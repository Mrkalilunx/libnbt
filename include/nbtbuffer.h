#ifndef NBTBUF_H
#define NBTBUF_H

#include <stdint.h>
#include <stddef.h>
#include "nbtendian.h"

// 动态字节缓冲区，支持写入和读取
typedef struct {
    uint8_t* data;    // 缓冲区数据指针
    size_t   cap;     // 缓冲区容量
    size_t   used;    // 已用字节数
    size_t   pos;     // 当前读取位置
} Buffer;

// 初始化缓冲区，指定初始容量（字节），成功返回1，失败返回0
int buf_init(Buffer* buf, size_t initial_cap);

// 用只读数据包装缓冲区（不拷贝，不分配），仅用于 nbt_load 等读取路径
static inline void buf_wrap(Buffer* buf, const uint8_t* data, size_t len) {
    buf->data = (uint8_t*)data;
    buf->cap = len;
    buf->used = len;
    buf->pos = 0;
}

// 释放缓冲区内部数据，并将结构体归零
void buf_free(Buffer* buf);

// 查询当前容量是否足以再容纳 need 字节（用于写入），是返回1，否返回0（不扩容）
int buf_ensure(const Buffer* buf, size_t need);

// 查询当前读取位置是否足以再读取 need 字节，是返回1，否返回0
int buf_can_read(const Buffer* buf, size_t need);

// 手动扩容至至少 min_cap 字节，成功返回1，失败返回0
int buf_grow(Buffer* buf, size_t min_cap);

// 重置读取位置到开头
void buf_rewind(Buffer* buf);

// 重置缓冲区（清空已用数据和读取位置，不释放内存）
void buf_reset(Buffer* buf);

// 向缓冲区写入1字节（无符号），成功返回1，失败返回0
int buf_wru8(Buffer* buf, uint8_t val);

// 向缓冲区写入1字节（有符号），成功返回1，失败返回0
int buf_wi8(Buffer* buf, int8_t val);

// 向缓冲区以大端写入16位无符号整数，成功返回1，失败返回0
int buf_wrbeu16(Buffer* buf, uint16_t val);

// 向缓冲区以大端写入16位有符号整数，成功返回1，失败返回0
int buf_wrbei16(Buffer* buf, int16_t val);

// 向缓冲区以大端写入32位无符号整数，成功返回1，失败返回0
int buf_wrbeu32(Buffer* buf, uint32_t val);

// 向缓冲区以大端写入32位有符号整数，成功返回1，失败返回0
int buf_wrbei32(Buffer* buf, int32_t val);

// 向缓冲区以大端写入64位无符号整数，成功返回1，失败返回0
int buf_wrbeu64(Buffer* buf, uint64_t val);

// 向缓冲区以大端写入64位有符号整数，成功返回1，失败返回0
int buf_wrbei64(Buffer* buf, int64_t val);

// 向缓冲区以大端写入32位浮点数，成功返回1，失败返回0
int buf_wrbef32(Buffer* buf, float val);

// 向缓冲区以大端写入64位浮点数，成功返回1，失败返回0
int buf_wrbef64(Buffer* buf, double val);

// 向缓冲区以小端写入16位无符号整数，成功返回1，失败返回0
int buf_wrleu16(Buffer* buf, uint16_t val);

// 向缓冲区以小端写入16位有符号整数，成功返回1，失败返回0
int buf_wrlei16(Buffer* buf, int16_t val);

// 向缓冲区以小端写入32位无符号整数，成功返回1，失败返回0
int buf_wrleu32(Buffer* buf, uint32_t val);

// 向缓冲区以小端写入32位有符号整数，成功返回1，失败返回0
int buf_wrlei32(Buffer* buf, int32_t val);

// 向缓冲区以小端写入64位无符号整数，成功返回1，失败返回0
int buf_wrleu64(Buffer* buf, uint64_t val);

// 向缓冲区以小端写入64位有符号整数，成功返回1，失败返回0
int buf_wrlei64(Buffer* buf, int64_t val);

// 向缓冲区以小端写入32位浮点数，成功返回1，失败返回0
int buf_wrlef32(Buffer* buf, float val);

// 向缓冲区以小端写入64位浮点数，成功返回1，失败返回0
int buf_wrlef64(Buffer* buf, double val);

// 按运行时指定端序向缓冲区写入16位无符号整数，成功返回1，失败返回0
int buf_wu16(Buffer* buf, uint16_t val, nbt_endian endian);

// 按运行时指定端序向缓冲区写入16位有符号整数，成功返回1，失败返回0
int buf_wi16(Buffer* buf, int16_t val, nbt_endian endian);

// 按运行时指定端序向缓冲区写入32位无符号整数，成功返回1，失败返回0
int buf_wu32(Buffer* buf, uint32_t val, nbt_endian endian);

// 按运行时指定端序向缓冲区写入32位有符号整数，成功返回1，失败返回0
int buf_wi32(Buffer* buf, int32_t val, nbt_endian endian);

// 按运行时指定端序向缓冲区写入64位无符号整数，成功返回1，失败返回0
int buf_wu64(Buffer* buf, uint64_t val, nbt_endian endian);

// 按运行时指定端序向缓冲区写入64位有符号整数，成功返回1，失败返回0
int buf_wi64(Buffer* buf, int64_t val, nbt_endian endian);

// 按运行时指定端序向缓冲区写入32位浮点数，成功返回1，失败返回0
int buf_wf32(Buffer* buf, float val, nbt_endian endian);

// 按运行时指定端序向缓冲区写入64位浮点数，成功返回1，失败返回0
int buf_wf64(Buffer* buf, double val, nbt_endian endian);

// 向缓冲区写入原始字节块，成功返回1，失败返回0
int buf_wrbytes(Buffer* buf, const uint8_t* data, size_t len);

// 从缓冲区读取1字节（无符号），成功返回1，失败返回0
int buf_ru8(Buffer* buf, uint8_t* out);

// 从缓冲区读取1字节（有符号），成功返回1，失败返回0
int buf_ri8(Buffer* buf, int8_t* out);

// 从缓冲区以大端读取16位无符号整数，成功返回1，失败返回0
int buf_rbeu16(Buffer* buf, uint16_t* out);

// 从缓冲区以大端读取16位有符号整数，成功返回1，失败返回0
int buf_rbei16(Buffer* buf, int16_t* out);

// 从缓冲区以大端读取32位无符号整数，成功返回1，失败返回0
int buf_rbeu32(Buffer* buf, uint32_t* out);

// 从缓冲区以大端读取32位有符号整数，成功返回1，失败返回0
int buf_rbei32(Buffer* buf, int32_t* out);

// 从缓冲区以大端读取64位无符号整数，成功返回1，失败返回0
int buf_rbeu64(Buffer* buf, uint64_t* out);

// 从缓冲区以大端读取64位有符号整数，成功返回1，失败返回0
int buf_rbei64(Buffer* buf, int64_t* out);

// 从缓冲区以大端读取32位浮点数，成功返回1，失败返回0
int buf_rbef32(Buffer* buf, float* out);

// 从缓冲区以大端读取64位浮点数，成功返回1，失败返回0
int buf_rbef64(Buffer* buf, double* out);

// 从缓冲区以小端读取16位无符号整数，成功返回1，失败返回0
int buf_rleu16(Buffer* buf, uint16_t* out);

// 从缓冲区以小端读取16位有符号整数，成功返回1，失败返回0
int buf_rlei16(Buffer* buf, int16_t* out);

// 从缓冲区以小端读取32位无符号整数，成功返回1，失败返回0
int buf_rleu32(Buffer* buf, uint32_t* out);

// 从缓冲区以小端读取32位有符号整数，成功返回1，失败返回0
int buf_rlei32(Buffer* buf, int32_t* out);

// 从缓冲区以小端读取64位无符号整数，成功返回1，失败返回0
int buf_rleu64(Buffer* buf, uint64_t* out);

// 从缓冲区以小端读取64位有符号整数，成功返回1，失败返回0
int buf_rlei64(Buffer* buf, int64_t* out);

// 从缓冲区以小端读取32位浮点数，成功返回1，失败返回0
int buf_rlef32(Buffer* buf, float* out);

// 从缓冲区以小端读取64位浮点数，成功返回1，失败返回0
int buf_rlef64(Buffer* buf, double* out);

// 按运行时指定端序从缓冲区读取16位无符号整数，成功返回1，失败返回0
int buf_ru16(Buffer* buf, uint16_t* out, nbt_endian endian);

// 按运行时指定端序从缓冲区读取16位有符号整数，成功返回1，失败返回0
int buf_ri16(Buffer* buf, int16_t* out, nbt_endian endian);

// 按运行时指定端序从缓冲区读取32位无符号整数，成功返回1，失败返回0
int buf_ru32(Buffer* buf, uint32_t* out, nbt_endian endian);

// 按运行时指定端序从缓冲区读取32位有符号整数，成功返回1，失败返回0
int buf_ri32(Buffer* buf, int32_t* out, nbt_endian endian);

// 按运行时指定端序从缓冲区读取64位无符号整数，成功返回1，失败返回0
int buf_ru64(Buffer* buf, uint64_t* out, nbt_endian endian);

// 按运行时指定端序从缓冲区读取64位有符号整数，成功返回1，失败返回0
int buf_ri64(Buffer* buf, int64_t* out, nbt_endian endian);

// 按运行时指定端序从缓冲区读取32位浮点数，成功返回1，失败返回0
int buf_rf32(Buffer* buf, float* out, nbt_endian endian);

// 按运行时指定端序从缓冲区读取64位浮点数，成功返回1，失败返回0
int buf_rf64(Buffer* buf, double* out, nbt_endian endian);

// 从缓冲区读取原始字节块，成功返回1，失败返回0
int buf_rdbytes(Buffer* buf, uint8_t* out_buf, size_t len);

#endif // NBTBUF_H