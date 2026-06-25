#ifndef SNBTARRAY_H
#define SNBTARRAY_H

#include "nbt.h"
#include "snbtnumber.h"   // 需要 SnbtNumberStyle 以及 integer2str/str2integer

// 解析 SNBT 数组字符串，成功返回对应 NBT 类型 ID（NBT_Byte_Array, NBT_Int_Array, NBT_Long_Array），失败返回 0
int str2array(const char* str, nbt_Payload* out);

// 将 NBT 数组格式化为 SNBT 字符串，输出符合 [B;...] / [I;...] / [L;...] 格式，
// style 控制元素输出进制和后缀等（与 integer2str 相同），成功返回 1，失败返回 0
int array2str(uint8_t type, const nbt_Payload* val, char** out, const SnbtNumberStyle* style);

#endif // SNBTARRAY_H