#ifndef SNBTNUMBER_H
#define SNBTNUMBER_H

#include "nbt.h"
#include "snbterror.h"

// 数值类型输出样式集合（控制整数和浮点数的格式化输出）
typedef struct {
    int base_byte;      // Byte 类型的输出进制：2/10/16，默认 10
    int base_short;     // Short 类型的输出进制，默认 10
    int base_int;       // Int 类型的输出进制，默认 10
    int base_long;      // Long 类型的输出进制，默认 10
    int show_int_suffix;// Int 类型是否显示后缀（1 显示，0 不显示），Byte/Short/Long 必须显示不受此控制，默认 0
    int uppercase;      // 后缀字母大小写，作用于所有类型（包括浮点），1 大写，0 小写，默认 0
} SnbtNumberStyle;

// 将字符串按 SNBT 规范解析为整数，当无后缀时默认视为 Int
// 成功返回 NBT 类型 ID（NBT_Byte 等），失败返回 0
int str2integer(const char* str, nbt_Payload* out);

// 将字符串按 SNBT 规范解析为整数，当无后缀时使用 default_type 作为目标类型
// default_type 应为 NBT_Byte, NBT_Short, NBT_Int, NBT_Long 之一，若传入非法值则退化为 Int
int defaultstr2int(const char* str, uint8_t default_type, nbt_Payload* out);

// 将字符串按 SNBT 规范解析为浮点数，成功返回 NBT 类型 ID（NBT_Float/NBT_Double），失败返回 0
int str2float(const char* str, nbt_Payload* out);

// 将整数类型数值格式化为带后缀的字符串，样式由 style 控制
int integer2str(uint8_t type, const nbt_Payload* val, char** out, const SnbtNumberStyle* style);

// 将浮点类型数值格式化为带后缀的字符串，样式由 style 控制
int float2str(uint8_t type, const nbt_Payload* val, char** out, const SnbtNumberStyle* style);

#endif // SNBTNUMBER_H