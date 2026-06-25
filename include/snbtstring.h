#ifndef SNBTSTRING_H
#define SNBTSTRING_H

#include "nbt.h"

// 解析 SNBT 字符串（引号或无引号），成功返回 NBT_Str（8），失败返回 0
int str2string(const char* str, nbt_Payload* out);

// 将 NBT 字符串格式化为 SNBT 字面量，成功返回 1，失败返回 0
int string2str(const nbt_Payload* val, char** out);

#endif