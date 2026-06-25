#ifndef SNBTIO_H
#define SNBTIO_H

#include "nbt.h"

// 从纯 SNBT 文件读取并解析，成功返回根标签，失败返回 NULL
struct NBT_tag* fprsnbt(const char* filename);

// 将标签树序列化为 SNBT 格式并写入文件，pretty=1 美化，pretty=0 紧凑，成功返回 1，失败返回 0
int fpwsnbt(const char* filename, const struct NBT_tag* root, int pretty);

// 通过内存映射读取纯 SNBT 文件（POSIX 环境）或回退到标准 I/O
struct NBT_tag* mmrsnbt(const char* filename);

// 写入纯 SNBT 文件，pretty 控制美化，内部使用标准文件写入
int mmwsnbt(const char* filename, const struct NBT_tag* root, int pretty);

#endif