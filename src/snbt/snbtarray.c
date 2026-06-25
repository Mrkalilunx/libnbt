#include "snbtarray.h"
#include "snbtbuffer.h"
#include "snbterror.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

// -------- 辅助 --------

// 跳过空白，返回下一个非空白字符，并修改 s 指针
static const char* skip_ws(const char* s) {
    while (*s && isspace((unsigned char)*s)) s++;
    return s;
}

// 解析数组类型前缀，成功返回数组对应的 NBT 类型 ID，失败返回 0
static int parse_array_prefix(const char** p) {
    const char* s = *p;
    s = skip_ws(s);
    if (*s != '[') return 0;
    s++;
    s = skip_ws(s);
    char type = *s;
    if (tolower((unsigned char)type) == 'b') {
        s++;
    } else if (tolower((unsigned char)type) == 'i') {
        s++;
    } else if (tolower((unsigned char)type) == 'l') {
        s++;
    } else {
        return 0;
    }
    s = skip_ws(s);
    if (*s != ';') return 0;
    s++;
    *p = s; // 移动到分号后
    return (type == 'b' || type == 'B') ? NBT_Byte_Array :
           (type == 'i' || type == 'I') ? NBT_Int_Array :
           NBT_Long_Array;
}

// 解析一个数组元素（数字字符串），存储到 out_value，返回元素类型 ID，失败返回 0
static int parse_array_element(const char* s, nbt_Payload* out_value, uint8_t default_elem_type) {
    // 提取逗号或括号之前的子字符串
    const char* start = s;
    while (*s && *s != ',' && *s != ']') s++;
    size_t len = s - start;
    if (len == 0) return 0;
    // 复制一份
    char* token = (char*)malloc(len + 1);
    if (!token) return 0;
    memcpy(token, start, len);
    token[len] = '\0';
    // 使用 defaultstr2int 解析，并传入默认元素类型
    int ret_type = defaultstr2int(token, default_elem_type, out_value);
    free(token);
    return ret_type;
}

// 比较两个整数类型的大小（Byte<Short<Int<Long）
static int type_rank(uint8_t type) {
    switch (type) {
        case NBT_Byte: return 1;
        case NBT_Short: return 2;
        case NBT_Int: return 3;
        case NBT_Long: return 4;
        default: return 0;
    }
}

// 将 payload 值转换为目标类型的 int64_t
static int64_t convert_to_int64(uint8_t src_type, const nbt_Payload* src) {
    switch (src_type) {
        case NBT_Byte:  return (int64_t)src->b;
        case NBT_Short: return (int64_t)src->s;
        case NBT_Int:   return (int64_t)src->i;
        case NBT_Long:  return src->l;
        default: return 0;
    }
}

// 根据数组类型返回其元素的 NBT 类型
static uint8_t array_elem_type(uint8_t array_type) {
    switch (array_type) {
        case NBT_Byte_Array: return NBT_Byte;
        case NBT_Int_Array:  return NBT_Int;
        case NBT_Long_Array: return NBT_Long;
        default: return 0;
    }
}

// -------- 公开接口 --------

int str2array(const char* str, nbt_Payload* out) {
    if (!str || !out) { snbt_set_error(SNBT_ERR_NULL_ARGUMENT); return 0; }

    const char* p = str;
    // 去除首空白
    p = skip_ws(p);
    if (!*p) { snbt_set_error(SNBT_ERR_EMPTY_STRING); return 0; }

    int array_type = parse_array_prefix(&p);
    if (!array_type) { snbt_set_error(SNBT_ERR_INVALID_PREFIX); return 0; }

    uint8_t elem_type = array_elem_type(array_type);

    // 用于存储临时 int64_t 值的动态数组
    int64_t* temp = NULL;
    size_t count = 0, capacity = 0;

    // 解析元素直到 ']'
    while (1) {
        p = skip_ws(p);
        if (*p == ']') break;
        if (count > 0) {
            if (*p == ',') p++;
            else {
                free(temp);
                snbt_set_error(SNBT_ERR_INVALID_PREFIX);
                return 0;
            }
            p = skip_ws(p);
        }
        // 解析一个元素
        nbt_Payload elem;
        memset(&elem, 0, sizeof(elem));
        int parsed_type = parse_array_element(p, &elem, elem_type);
        if (!parsed_type) {
            free(temp);
            snbt_set_error(SNBT_ERR_INVALID_PREFIX); // 数字解析失败
            return 0;
        }
        // 移动 p 到元素结束位置（逗号或右括号前）
        while (*p && *p != ',' && *p != ']') p++;
        // 检查类型兼容性：元素类型不能比数组元素类型更大
        if (type_rank(parsed_type) > type_rank(elem_type)) {
            free(temp);
            snbt_set_error(SNBT_ERR_TYPE_MISMATCH);
            return 0;
        }
        // 收集值
        if (count >= capacity) {
            size_t new_cap = capacity ? capacity * 2 : 16;
            int64_t* new_temp = (int64_t*)realloc(temp, new_cap * sizeof(int64_t));
            if (!new_temp) { free(temp); snbt_set_error(SNBT_ERR_UNKNOWN); return 0; }
            temp = new_temp;
            capacity = new_cap;
        }
        temp[count++] = convert_to_int64(parsed_type, &elem);
    }
    p++; // 跳过 ']'

    // 构造 NBT 数组
    if (array_type == NBT_Byte_Array) {
        NBT_bytes* arr = (NBT_bytes*)malloc(sizeof(NBT_bytes));
        if (!arr) { free(temp); snbt_set_error(SNBT_ERR_UNKNOWN); return 0; }
        arr->length = (int32_t)count;
        arr->data = (uint8_t*)malloc(count);
        if (!arr->data) { free(arr); free(temp); snbt_set_error(SNBT_ERR_UNKNOWN); return 0; }
        for (size_t i = 0; i < count; i++) {
            arr->data[i] = (uint8_t)temp[i];
        }
        out->ptr = arr;
    } else if (array_type == NBT_Int_Array) {
        NBT_ints* arr = (NBT_ints*)malloc(sizeof(NBT_ints));
        if (!arr) { free(temp); snbt_set_error(SNBT_ERR_UNKNOWN); return 0; }
        arr->length = (int32_t)count;
        arr->data = (int32_t*)malloc(count * sizeof(int32_t));
        if (!arr->data) { free(arr); free(temp); snbt_set_error(SNBT_ERR_UNKNOWN); return 0; }
        for (size_t i = 0; i < count; i++) {
            arr->data[i] = (int32_t)temp[i];
        }
        out->ptr = arr;
    } else if (array_type == NBT_Long_Array) {
        NBT_longs* arr = (NBT_longs*)malloc(sizeof(NBT_longs));
        if (!arr) { free(temp); snbt_set_error(SNBT_ERR_UNKNOWN); return 0; }
        arr->length = (int32_t)count;
        arr->data = (int64_t*)malloc(count * sizeof(int64_t));
        if (!arr->data) { free(arr); free(temp); snbt_set_error(SNBT_ERR_UNKNOWN); return 0; }
        memcpy(arr->data, temp, count * sizeof(int64_t));
        out->ptr = arr;
    } else {
        free(temp);
        snbt_set_error(SNBT_ERR_UNKNOWN);
        return 0;
    }

    free(temp);
    snbt_set_error(SNBT_SUCCESS);
    return array_type;
}

int array2str(uint8_t type, const nbt_Payload* val, char** out, const SnbtNumberStyle* style) {
    if (!val || !out || !style) return 0;

    uint8_t elem_type = array_elem_type(type);
    if (!elem_type) return 0;

    // 获取长度和数据指针
    int32_t length = 0;
    const void* data = NULL;
    size_t elem_size = 0;
    switch (type) {
        case NBT_Byte_Array: {
            NBT_bytes* arr = (NBT_bytes*)val->ptr;
            if (!arr) return 0;
            length = arr->length;
            data = arr->data;
            elem_size = 1;
            break;
        }
        case NBT_Int_Array: {
            NBT_ints* arr = (NBT_ints*)val->ptr;
            if (!arr) return 0;
            length = arr->length;
            data = arr->data;
            elem_size = 4;
            break;
        }
        case NBT_Long_Array: {
            NBT_longs* arr = (NBT_longs*)val->ptr;
            if (!arr) return 0;
            length = arr->length;
            data = arr->data;
            elem_size = 8;
            break;
        }
        default: return 0;
    }

    SnbtBuf sb;
    if (!snbtbuf_init(&sb, 256)) return 0;

    switch (type) {
        case NBT_Byte_Array: snbtbuf_append(&sb, "[B;", 3); break;
        case NBT_Int_Array:  snbtbuf_append(&sb, "[I;", 3); break;
        case NBT_Long_Array: snbtbuf_append(&sb, "[L;", 3); break;
        default: return 0;
    }

    nbt_Payload elem_payload;
    char* elem_str = NULL;
    for (int32_t i = 0; i < length; i++) {
        if (i > 0) snbtbuf_putc(&sb, ',');
        int64_t val;
        const uint8_t* ptr = (const uint8_t*)data + i * elem_size;
        switch (elem_type) {
            case NBT_Byte:  val = *(const int8_t*)ptr; break;
            case NBT_Int:   val = *(const int32_t*)ptr; break;
            case NBT_Long:  val = *(const int64_t*)ptr; break;
            default: snbtbuf_free(&sb); return 0;
        }
        switch (elem_type) {
            case NBT_Byte: elem_payload.b = (int8_t)val; break;
            case NBT_Int: elem_payload.i = (int32_t)val; break;
            case NBT_Long: memcpy(&elem_payload.l, &val, sizeof(val)); break;
            default: snbtbuf_free(&sb); return 0;
        }
        if (!integer2str(elem_type, &elem_payload, &elem_str, style)) {
            snbtbuf_free(&sb);
            return 0;
        }
        snbtbuf_append(&sb, elem_str, strlen(elem_str));
        free(elem_str);
    }

    snbtbuf_putc(&sb, ']');
    snbtbuf_cstr(&sb);
    *out = snbtbuf_steal(&sb);
    return 1;
}