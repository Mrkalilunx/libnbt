#ifndef NBTIO_H
#define NBTIO_H

#include "nbt.h"

// 读取 level.dat 文件（带头部），按指定字节序解析，成功返回根标签，失败返回 NULL
struct NBT_tag* fprlevdat(const char* filename, nbt_endian endian);

// 写入 level.dat 文件（带头部），按指定字节序序列化，成功返回 1，失败返回 0
int fpwlevdat(const char* filename, const struct NBT_tag* root, nbt_endian endian);

// 读取纯 NBT 文件（无头部），按指定字节序解析，成功返回根标签，失败返回 NULL
struct NBT_tag* fprnbt(const char* filename, nbt_endian endian);

// 写入纯 NBT 文件（无头部），按指定字节序序列化，成功返回 1，失败返回 0
int fpwnbt(const char* filename, const struct NBT_tag* root, nbt_endian endian);

// 读取 level.dat 文件（带头部），通过内存映射优化读取，成功返回根标签，失败返回 NULL
struct NBT_tag* mmrlevdat(const char* filename, nbt_endian endian);

// 写入 level.dat 文件（带头部），实际使用标准文件写入，成功返回 1，失败返回 0
int mmwlevdat(const char* filename, const struct NBT_tag* root, nbt_endian endian);

// 读取纯 NBT 文件（无头部），通过内存映射优化读取，成功返回根标签，失败返回 NULL
struct NBT_tag* mmrnbt(const char* filename, nbt_endian endian);

// 写入纯 NBT 文件（无头部），实际使用标准文件写入，成功返回 1，失败返回 0
int mmwnbt(const char* filename, const struct NBT_tag* root, nbt_endian endian);

#endif // NBTIO_H