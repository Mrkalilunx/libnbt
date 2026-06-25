#ifndef SNBTLIST_H
#define SNBTLIST_H

#include "nbt.h"
#include "snbtnumber.h"  // SnbtNumberStyle

// 解析 SNBT 列表字符串，成功返回 NBT_List (9)，失败返回 0
int str2list(const char* str, nbt_Payload* out);

// 将 NBT 列表格式化为 SNBT 字符串，style 控制数值元素格式，成功返回 1，失败返回 0
int list2str(const nbt_Payload* val, char** out, const SnbtNumberStyle* style);

// 通用 SNBT 值解析器（递归），用于列表和复合标签内部
// *p 会前进到值结束后的位置，成功返回类型 ID，失败返回 0
int snbt_parse_value(const char** p, int depth, nbt_Payload* out, uint8_t* type);

// 内部复合标签解析，供 snbtcompound.c 使用
int parse_compound_internal(const char** p, int depth, nbt_Payload* out);

#endif