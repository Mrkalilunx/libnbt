// snbterror.h
#ifndef SNBTERR_H
#define SNBTERR_H

// SNBT 错误码枚举，包含成功、各类错误以及空列表警告
typedef enum {
    SNBT_SUCCESS = 0,                    // 操作成功
    SNBT_ERR_NULL_ARGUMENT,              // 传入参数为 NULL
    SNBT_ERR_EMPTY_STRING,               // 输入字符串为空
    SNBT_ERR_UNDERSCORE_AT_ENDS,         // 下划线出现在数字首尾
    SNBT_ERR_INVALID_UNDERSCORE,         // 下划线位置不合法（左右非有效数字）
    SNBT_ERR_INVALID_PREFIX,             // 非法进制前缀
    SNBT_ERR_INVALID_SUFFIX,             // 非法类型/符号后缀
    SNBT_ERR_TYPE_MISMATCH,              // 后缀类型与目标类型不匹配
    SNBT_ERR_OUT_OF_RANGE,               // 数值超出目标类型的表示范围
    SNBT_ERR_UNSIGNED_NEGATIVE,          // 无符号类型指定了负数
    SNBT_ERR_FLOAT_HEX_PREFIX,           // 浮点数不允许十六进制/二进制前缀
    SNBT_ERR_FLOAT_PARSE_FAIL,           // 浮点数解析失败
    SNBT_ERR_FLOAT_OVERFLOW,             // 浮点数溢出
    SNBT_ERR_FLOAT_NAN_INF,              // 浮点数为 NaN 或无穷大
    SNBT_ERR_INVALID_ESCAPE,             // 非法的转义序列
    SNBT_ERR_UNCLOSED_STRING,            // 字符串引号未闭合
    SNBT_ERR_UNKNOWN,                    // 未知错误
    SNBT_WARN_EMPTY_LIST                 // 空列表类型丢失警告（非致命）
} SnbtError;

// 获取最后一次错误码
int snbt_get_error(void);

// 将错误码转换为描述字符串
const char* snbt_error_string(int err);

// 类似 perror 打印错误信息到 stderr（包含错误上下文）
void snbt_perror(const char* prefix);

// 内部使用：设置错误码
void snbt_set_error(SnbtError err);

// 设置错误上下文（解析失败时的字符串片段，会显示在 snbt_perror 中）
void snbt_set_error_msg(const char* msg);

// 获取错误上下文
const char* snbt_get_error_msg(void);

#endif // SNBTERR_H