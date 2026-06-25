#ifndef NBTBITS_H
#define NBTBITS_H

#include <stdint.h>
#include <string.h>
#include <endian.h>

// 平台字节序检测，定义编译时常量 NBT_IS_LITTLE_ENDIAN
#if defined(__BYTE_ORDER__) && defined(__ORDER_LITTLE_ENDIAN__)
  #define NBT_IS_LITTLE_ENDIAN (__BYTE_ORDER__ == __ORDER_LITTLE_ENDIAN__)
#elif defined(__BYTE_ORDER) && defined(__LITTLE_ENDIAN)
  #define NBT_IS_LITTLE_ENDIAN (__BYTE_ORDER == __LITTLE_ENDIAN)
#elif defined(_WIN32)
  #define NBT_IS_LITTLE_ENDIAN 1
#else
  #error "Unable to determine endianness. Define NBT_IS_LITTLE_ENDIAN manually."
#endif

typedef enum {
    NBT_LITTLE_ENDIAN = 0,  // 基岩版小端
    NBT_BIG_ENDIAN    = 1   // Java版大端
} nbt_endian;

// 8位整数读取，无字节序问题
static inline void ru8(const uint8_t** p, uint8_t* out) {
    *out = **p; (*p)++;
}
static inline void ri8(const uint8_t** p, int8_t* out) {
    *out = (int8_t)**p; (*p)++;
}
static inline void wru8(uint8_t** p, uint8_t val) {
    **p = val; (*p)++;
}
static inline void wi8(uint8_t** p, int8_t val) {
    **p = (uint8_t)val; (*p)++;
}

// 生成整数大端/小端读取函数
#define NBT_RD_INT(bits, utype, stype, beconv, leconv) \
static inline void rbeu##bits(const uint8_t** p, utype* out) { \
    utype v; memcpy(&v, *p, sizeof(v)); \
    *out = NBT_IS_LITTLE_ENDIAN ? beconv(v) : v; \
    *p += (bits/8); \
} \
static inline void rbei##bits(const uint8_t** p, stype* out) { \
    utype v; memcpy(&v, *p, sizeof(v)); \
    *out = (stype)(NBT_IS_LITTLE_ENDIAN ? beconv(v) : v); \
    *p += (bits/8); \
} \
static inline void rleu##bits(const uint8_t** p, utype* out) { \
    utype v; memcpy(&v, *p, sizeof(v)); \
    *out = NBT_IS_LITTLE_ENDIAN ? v : leconv(v); \
    *p += (bits/8); \
} \
static inline void rlei##bits(const uint8_t** p, stype* out) { \
    utype v; memcpy(&v, *p, sizeof(v)); \
    *out = (stype)(NBT_IS_LITTLE_ENDIAN ? v : leconv(v)); \
    *p += (bits/8); \
}

// 生成整数大端/小端写入函数
#define NBT_WR_INT(bits, utype, stype, beconv, leconv) \
static inline void wrbeu##bits(uint8_t** p, utype val) { \
    utype be = NBT_IS_LITTLE_ENDIAN ? beconv(val) : val; \
    memcpy(*p, &be, (bits/8)); *p += (bits/8); \
} \
static inline void wrbei##bits(uint8_t** p, stype val) { \
    utype be = NBT_IS_LITTLE_ENDIAN ? beconv((utype)val) : (utype)val; \
    memcpy(*p, &be, (bits/8)); *p += (bits/8); \
} \
static inline void wrleu##bits(uint8_t** p, utype val) { \
    utype le = NBT_IS_LITTLE_ENDIAN ? val : leconv(val); \
    memcpy(*p, &le, (bits/8)); *p += (bits/8); \
} \
static inline void wrlei##bits(uint8_t** p, stype val) { \
    utype le = NBT_IS_LITTLE_ENDIAN ? (utype)val : leconv((utype)val); \
    memcpy(*p, &le, (bits/8)); *p += (bits/8); \
}

// 生成浮点数大端/小端读取函数
#define NBT_RD_FLOAT(bits, ftype, itype, beconv, leconv) \
static inline void rbef##bits(const uint8_t** p, ftype* out) { \
    itype v; memcpy(&v, *p, sizeof(v)); \
    if (NBT_IS_LITTLE_ENDIAN) v = beconv(v); \
    memcpy(out, &v, sizeof(ftype)); \
    *p += (bits/8); \
} \
static inline void rlef##bits(const uint8_t** p, ftype* out) { \
    itype v; memcpy(&v, *p, sizeof(v)); \
    if (!NBT_IS_LITTLE_ENDIAN) v = leconv(v); \
    memcpy(out, &v, sizeof(ftype)); \
    *p += (bits/8); \
}

// 生成浮点数大端/小端写入函数
#define NBT_WR_FLOAT(bits, ftype, itype, beconv, leconv) \
static inline void wrbef##bits(uint8_t** p, ftype val) { \
    itype i; memcpy(&i, &val, sizeof(i)); \
    if (NBT_IS_LITTLE_ENDIAN) i = beconv(i); \
    memcpy(*p, &i, sizeof(i)); *p += (bits/8); \
} \
static inline void wrlef##bits(uint8_t** p, ftype val) { \
    itype i; memcpy(&i, &val, sizeof(i)); \
    if (!NBT_IS_LITTLE_ENDIAN) i = leconv(i); \
    memcpy(*p, &i, sizeof(i)); *p += (bits/8); \
}

// 实例化各宽度函数
NBT_RD_INT(16, uint16_t, int16_t, be16toh, le16toh)
NBT_WR_INT(16, uint16_t, int16_t, htobe16, htole16)

NBT_RD_INT(32, uint32_t, int32_t, be32toh, le32toh)
NBT_WR_INT(32, uint32_t, int32_t, htobe32, htole32)

NBT_RD_INT(64, uint64_t, int64_t, be64toh, le64toh)
NBT_WR_INT(64, uint64_t, int64_t, htobe64, htole64)

NBT_RD_FLOAT(32, float,  uint32_t, be32toh, le32toh)
NBT_WR_FLOAT(32, float,  uint32_t, htobe32, htole32)

NBT_RD_FLOAT(64, double, uint64_t, be64toh, le64toh)
NBT_WR_FLOAT(64, double, uint64_t, htobe64, htole64)

// 根据字节序参数分发读取函数
#define NBT_DISP_RD(name, rbe, rle, type) \
static inline int name(const uint8_t** p, type* out, nbt_endian endian) { \
    switch (endian) { \
        case NBT_BIG_ENDIAN:    rbe(p, out); return 1; \
        case NBT_LITTLE_ENDIAN: rle(p, out); return 1; \
        default:                return 0; \
    } \
}

// 根据字节序参数分发写入函数
#define NBT_DISP_WR(name, wbe, wle, type) \
static inline int name(uint8_t** p, type val, nbt_endian endian) { \
    switch (endian) { \
        case NBT_BIG_ENDIAN:    wbe(p, val); return 1; \
        case NBT_LITTLE_ENDIAN: wle(p, val); return 1; \
        default:                return 0; \
    } \
}

// 读取分发函数实例化
NBT_DISP_RD(ru16, rbeu16, rleu16, uint16_t)
NBT_DISP_RD(ri16, rbei16, rlei16, int16_t)
NBT_DISP_RD(ru32, rbeu32, rleu32, uint32_t)
NBT_DISP_RD(ri32, rbei32, rlei32, int32_t)
NBT_DISP_RD(ru64, rbeu64, rleu64, uint64_t)
NBT_DISP_RD(ri64, rbei64, rlei64, int64_t)
NBT_DISP_RD(rf32, rbef32, rlef32, float)
NBT_DISP_RD(rf64, rbef64, rlef64, double)

// 写入分发函数实例化
NBT_DISP_WR(wu16, wrbeu16, wrleu16, uint16_t)
NBT_DISP_WR(wi16, wrbei16, wrlei16, int16_t)
NBT_DISP_WR(wu32, wrbeu32, wrleu32, uint32_t)
NBT_DISP_WR(wi32, wrbei32, wrlei32, int32_t)
NBT_DISP_WR(wu64, wrbeu64, wrleu64, uint64_t)
NBT_DISP_WR(wi64, wrbei64, wrlei64, int64_t)
NBT_DISP_WR(wf32, wrbef32, wrlef32, float)
NBT_DISP_WR(wf64, wrbef64, wrlef64, double)

// 读取字节块
static inline void rdbytes(const uint8_t** p, uint8_t* buf, size_t len) {
    memcpy(buf, *p, len);
    *p += len;
}

// 写入字节块
static inline void wrbytes(uint8_t** p, const uint8_t* data, size_t len) {
    memcpy(*p, data, len);
    *p += len;
}

#endif /* NBTBITS_H */