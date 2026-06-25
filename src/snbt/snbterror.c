// snbterror.c
#include "snbterror.h"
#include <stdio.h>
#include <string.h>

// 内部错误状态
static SnbtError g_last_error = SNBT_SUCCESS;
static char g_error_msg[256] = {0};   // 固定大小缓冲区

// 设置错误码
void snbt_set_error(SnbtError err) {
    g_last_error = err;
}

// 获取错误码
int snbt_get_error(void) {
    return (int)g_last_error;
}

// 设置错误上下文消息
void snbt_set_error_msg(const char* msg) {
    if (msg) {
        strncpy(g_error_msg, msg, sizeof(g_error_msg) - 1);
        g_error_msg[sizeof(g_error_msg) - 1] = '\0';
    } else {
        g_error_msg[0] = '\0';
    }
}

// 获取错误上下文消息
const char* snbt_get_error_msg(void) {
    return g_error_msg;
}

// 将错误码转换为可读字符串
const char* snbt_error_string(int err) {
    switch ((SnbtError)err) {
        case SNBT_SUCCESS:               return "Success";
        case SNBT_ERR_NULL_ARGUMENT:     return "Null argument";
        case SNBT_ERR_EMPTY_STRING:      return "Empty string";
        case SNBT_ERR_UNDERSCORE_AT_ENDS:return "Underscore at start or end";
        case SNBT_ERR_INVALID_UNDERSCORE:return "Invalid underscore placement";
        case SNBT_ERR_INVALID_PREFIX:    return "Invalid number prefix";
        case SNBT_ERR_INVALID_SUFFIX:    return "Invalid number suffix";
        case SNBT_ERR_TYPE_MISMATCH:     return "Suffix type does not match target type";
        case SNBT_ERR_OUT_OF_RANGE:      return "Value out of range for target type";
        case SNBT_ERR_UNSIGNED_NEGATIVE: return "Negative value not allowed for unsigned";
        case SNBT_ERR_FLOAT_HEX_PREFIX:  return "Hexadecimal/binary prefix not allowed for float";
        case SNBT_ERR_FLOAT_PARSE_FAIL:  return "Float parse failed";
        case SNBT_ERR_FLOAT_OVERFLOW:    return "Float overflow";
        case SNBT_ERR_FLOAT_NAN_INF:     return "Float is NaN or infinity";
        case SNBT_ERR_INVALID_ESCAPE:    return "Invalid escape sequence";
        case SNBT_ERR_UNCLOSED_STRING:   return "Unclosed string literal";
        case SNBT_ERR_UNKNOWN:           return "Unknown error";
        case SNBT_WARN_EMPTY_LIST:       return "Empty list: original type information lost (treated as TAG_End)";
        default:                         return "Unrecognized error";
    }
}

// 打印错误信息，包含可选的上下文
void snbt_perror(const char* prefix) {
    const char* msg = snbt_error_string(g_last_error);
    if (prefix) {
        fprintf(stderr, "%s: %s", prefix, msg);
    } else {
        fprintf(stderr, "%s", msg);
    }
    if (g_error_msg[0] != '\0') {
        fprintf(stderr, " (near: \"%s\")", g_error_msg);
    }
    fprintf(stderr, "\n");
}