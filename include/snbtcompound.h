#ifndef SNBTCOMP_H
#define SNBTCOMP_H

#include "nbt.h"
#include "snbtnumber.h"

// 解析 SNBT 复合标签字符串，成功返回 NBT_Comp (10)，失败返回 0
int str2compound(const char* str, nbt_Payload* out);

// 将 NBT 复合标签格式化为 SNBT 字符串，style 控制数值元素格式，成功返回 1，失败返回 0
int compound2str(const nbt_Payload* val, char** out, const SnbtNumberStyle* style);

#endif