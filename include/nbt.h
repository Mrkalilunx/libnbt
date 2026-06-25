#ifndef NBT_H
#define NBT_H

#include <stdint.h>
#include <stddef.h>
#include "nbtendian.h"

// 最大递归深度，用于防止恶意嵌套导致栈溢出
#define NBT_MAX_DEPTH 1024

// Compound 内子元素个数上限，防止内存耗尽
#define NBT_MAX_COMPOUND_ELEMENTS (1 << 20)

// 本机字节序，用于优化直接复制
#define NBT_NATIVE_ENDIAN (NBT_IS_LITTLE_ENDIAN ? NBT_LITTLE_ENDIAN : NBT_BIG_ENDIAN)

// 枚举 NBT 标签类型，用于标识 NBT 数据中标签的种类
enum NBT_type {
    NBT_End = 0,        // 无负载，仅用于标记 NBT_Comp 或 NBT_List 的结束
    NBT_Byte = 1,       // 有符号 1 字节整数（-128 ~ 127），常用于布尔值
    NBT_Short = 2,      // 有符号 2 字节整数（-32768 ~ 32767），如物品耐久
    NBT_Int = 3,        // 有符号 4 字节整数（-2³¹ ~ 2³¹-1），最常用
    NBT_Long = 4,       // 有符号 8 字节整数，用于时间戳、世界种子
    NBT_Float = 5,      // 4 字节单精度浮点数，如坐标、速度
    NBT_Double = 6,     // 8 字节双精度浮点数，高精度数值
    NBT_Byte_Array = 7, // 字节数组，用于存储二进制原始数据
    NBT_Str = 8,        // 字符串（Modified UTF‑8 编码）
    NBT_List = 9,       // 列表，元素为同类型的无名标签
    NBT_Comp = 10,      // 复合标签，内部可包含任意多个有名字的子标签
    NBT_Int_Array = 11, // 整数数组，每个元素是 4 字节有符号整数
    NBT_Long_Array = 12, // 长整数数组，每个元素是 8 字节有符号整数
    NBT_Short_Array = 13 // 短整数数组（非标准扩展）
};

// NBT 负载联合体，用于统一存放简单值或复杂类型指针
typedef struct nbt_Payload {
    union {
        int8_t  b;   // TAG_Byte 值
        int16_t s;   // TAG_Short 值
        int32_t i;   // TAG_Int 值
        int64_t l;   // TAG_Long 值
        float   f;   // TAG_Float 值
        double  d;   // TAG_Double 值
        void*   ptr; // 复杂类型指针（TAG_Byte_Array 及以上）
    };
} nbt_Payload;

// NBT 标签结构体，代表一个完整的 NBT 标签节点
struct NBT_tag {
    uint8_t type_id;       // 标签类型（NBT_type 枚举值）
    uint16_t name_length;  // 名称的字节数（不含结尾 \0，与磁盘格式一致）
    char* name;            // 标签名称字符串（UTF‑8 编码，分配时多留 1 字节置零）
    nbt_Payload payload;   // 负载，具体有效成员由 type_id 决定
};

// NBT_Byte_Array 负载结构体
typedef struct NBT_bytes {
    int32_t length;         // 字节数组长度
    uint8_t* data;          // 字节数组数据指针
} NBT_bytes;

// NBT_Str 负载结构体
typedef struct NBT_str {
    int32_t length;         // 字符串长度（字节数）
    char* data;             // 指向 Modified UTF-8 编码的字符串数据
} NBT_str;

// NBT_List 负载结构体
typedef struct NBT_list {
    uint8_t child_type;     // 列表内所有子标签的类型（NBT_type 枚举值）
    int32_t count;          // 子标签数量
    union {
        void*        compact;    // 基本类型列表的紧凑内存块
        nbt_Payload* units;      // 复杂类型列表的元素数组
    } elements;                     // 列表元素存储（根据类型选择紧凑或数组形式）
} NBT_list;

// NBT_Comp 负载结构体
typedef struct NBT_comp {
    int32_t count;          // 子标签个数（不含终止 TAG_End）
    struct NBT_tag** children; // 指向子标签指针数组的指针
} NBT_comp;

// NBT_ints 负载结构体
typedef struct NBT_ints {
    int32_t length;         // 整数数组元素个数
    int32_t* data;          // 整数数组数据指针
} NBT_ints;

// NBT_longs 负载结构体
typedef struct NBT_longs {
    int32_t length;         // 长整数数组元素个数
    int64_t* data;          // 长整数数组数据指针
} NBT_longs;

// NBT_shorts 负载结构体
typedef struct NBT_shorts {
    int32_t length;         // 短整数数组元素个数
    int16_t* data;          // 短整数数组数据指针
} NBT_shorts;

// 从字节流加载 NBT 数据，按指定字节序解析，返回根标签指针，失败返回 NULL
struct NBT_tag* nbt_load(const uint8_t* data, size_t len, nbt_endian endian);

// 递归释放 NBT 标签树所占用的所有内存
void free_tag(struct NBT_tag* tag);

// 将 NBT 标签树序列化为字节流（动态缓冲区），按指定字节序输出，返回动态分配的缓冲区，长度写入 out_len，失败返回 NULL
uint8_t* nbt_write(const struct NBT_tag* root, size_t* out_len, nbt_endian endian);

// 将 NBT 标签树序列化为字节流（预计算大小，一次分配），按指定字节序输出，返回精确大小的缓冲区，长度写入 out_len，失败返回 NULL
uint8_t* nbt_write2(const struct NBT_tag* root, size_t* out_len, nbt_endian endian);

int type2size(uint8_t type_id);
#endif // NBT_H