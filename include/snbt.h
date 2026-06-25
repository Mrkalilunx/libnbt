// snbt.h
#ifndef SNBT_H
#define SNBT_H

#include "nbt.h"
#include <stdio.h>

// 从 SNBT 字符串加载 NBT 标签树，成功返回根标签指针，失败返回 NULL
struct NBT_tag* snbt_load(const char* str);

// 将 NBT 标签树序列化为 SNBT 字符串，pretty=1 缩进美化，pretty=0 紧凑
// 成功返回动态分配的字符串（调用者需 free），失败返回 NULL
char* snbt_write(const struct NBT_tag* tag, int pretty);

// 将 NBT 标签树直接流式写入文件，pretty=1 美化，pretty=0 紧凑
// 成功返回 1，失败返回 0
int snbt_write_stream(const struct NBT_tag* tag, int pretty, FILE* fp);

#endif // SNBT_H