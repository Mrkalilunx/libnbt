#include "nbt.h"
#include "nbtbuffer.h"
#include <stdlib.h>
#include <string.h>

// 前向声明内部函数
static int parse_payload_buf(uint8_t type_id, Buffer* buf, nbt_Payload* payload, nbt_endian endian, int depth);
static struct NBT_tag* parse_tag_buf(Buffer* buf, nbt_endian endian, int depth);
static void free_payload(uint8_t type_id, nbt_Payload *payload);
static int write_payload(uint8_t type_id, const nbt_Payload* payload, Buffer* buf, nbt_endian endian, int depth);
static int write_tag(const struct NBT_tag* tag, Buffer* buf, nbt_endian endian, int depth);
static size_t payload_ser_size(uint8_t type_id, const nbt_Payload* payload, int depth);
static size_t tag_ser_size(const struct NBT_tag* tag, int depth);

// 安全乘法检查，防止溢出。成功返回1，失败返回0
static inline int safe_mul_size(size_t a, size_t b, size_t* out) {
    if (b != 0 && a > SIZE_MAX / b) return 0;
    *out = a * b;
    return 1;
}

// 返回简单数值类型的字节大小，End 返回 0，复杂类型返回 -1
int type2size(uint8_t type_id) {
    switch (type_id) {
        case NBT_Byte:  return 1;
        case NBT_Short: return 2;
        case NBT_Int:   return 4;
        case NBT_Long:  return 8;
        case NBT_Float: return 4;
        case NBT_Double:return 8;
        case NBT_End:   return 0;
        default:        return -1;
    }
}

// 宏：简化三数组类型的解析（Int_Array / Long_Array / Short_Array）
#define PARSE_ARRAY_CASE(type_enum, arr_type, elem_type, ri_fn) \
    case type_enum: { \
        int32_t len; \
        if (!buf_ri32(buf, &len, endian)) return -1; \
        if (len < 0) return -1; \
        arr_type *arr = (arr_type*)malloc(sizeof(arr_type)); \
        if (!arr) return -1; \
        arr->length = len; \
        if (len > 0) { \
            size_t data_bytes; \
            if (!safe_mul_size((size_t)len, sizeof(elem_type), &data_bytes)) { free(arr); return -1; } \
            arr->data = (elem_type*)malloc(data_bytes); \
            if (!arr->data) { free(arr); return -1; } \
            if (endian == NBT_NATIVE_ENDIAN) { \
                if (!buf_rdbytes(buf, (uint8_t*)arr->data, data_bytes)) { free(arr->data); free(arr); return -1; } \
            } else { \
                for (int32_t i = 0; i < len; ++i) \
                    if (!ri_fn(buf, &arr->data[i], endian)) { free(arr->data); free(arr); return -1; } \
            } \
        } else arr->data = NULL; \
        payload->ptr = arr; \
        return 0; \
    }

// 宏：简化三数组类型的写入
#define WRITE_ARRAY_CASE(type_enum, arr_type, elem_type, wi_fn) \
    case type_enum: { \
        arr_type* arr = (arr_type*)payload->ptr; \
        if (!arr) return 0; \
        if (!buf_wi32(buf, arr->length, endian)) return 0; \
        size_t data_bytes; \
        if (!safe_mul_size((size_t)arr->length, sizeof(elem_type), &data_bytes)) return 0; \
        if (endian == NBT_NATIVE_ENDIAN) { \
            if (!buf_wrbytes(buf, (const uint8_t*)arr->data, data_bytes)) return 0; \
        } else { \
            for (int32_t i = 0; i < arr->length; ++i) \
                if (!wi_fn(buf, arr->data[i], endian)) return 0; \
        } \
        return 1; \
    }

// 宏：简化三数组类型的大小计算
#define SER_SIZE_ARRAY_CASE(type_enum, arr_type, elem_type) \
    case type_enum: { \
        arr_type* arr = (arr_type*)payload->ptr; \
        if (!arr) return 0; \
        size_t mul; \
        if (!safe_mul_size((size_t)arr->length, sizeof(elem_type), &mul)) return 0; \
        return (size_t)4 + mul; \
    }

// 通过缓冲区读取一个简单数值，写入 dest
static int rtype_buf(Buffer* buf, uint8_t type, void* dest, nbt_endian endian) {
    int size = type2size(type);
    if (size <= 0) return -1;

    if (endian == NBT_NATIVE_ENDIAN) {
        return buf_rdbytes(buf, (uint8_t*)dest, size) ? 0 : -1;
    }

    switch (type) {
        case NBT_Byte: {
            uint8_t v;
            if (!buf_ru8(buf, &v)) return -1;
            *(int8_t*)dest = (int8_t)v;
            return 0;
        }
        case NBT_Short: {
            uint16_t v;
            if (!buf_ru16(buf, &v, endian)) return -1;
            *(int16_t*)dest = (int16_t)v;
            return 0;
        }
        case NBT_Int: {
            int32_t v;
            if (!buf_ri32(buf, &v, endian)) return -1;
            *(int32_t*)dest = v;
            return 0;
        }
        case NBT_Long: {
            int64_t v;
            if (!buf_ri64(buf, &v, endian)) return -1;
            *(int64_t*)dest = v;
            return 0;
        }
        case NBT_Float: {
            float v;
            if (!buf_rf32(buf, &v, endian)) return -1;
            *(float*)dest = v;
            return 0;
        }
        case NBT_Double: {
            double v;
            if (!buf_rf64(buf, &v, endian)) return -1;
            *(double*)dest = v;
            return 0;
        }
        default: return -1;
    }
}

// 释放 nbt_Payload 内部动态资源
static void free_payload(uint8_t type_id, nbt_Payload *payload) {
    if (!payload) return;
    int size = type2size(type_id);
    if (size > 0) return;

    switch (type_id) {
        case NBT_Byte_Array: {
            NBT_bytes *p = (NBT_bytes*)payload->ptr;
            if (p) { free(p->data); free(p); }
            break;
        }
        case NBT_Str: {
            NBT_str *p = (NBT_str*)payload->ptr;
            if (p) { free(p->data); free(p); }
            break;
        }
        case NBT_List: {
            NBT_list *list = (NBT_list*)payload->ptr;
            if (!list) break;
            if (type2size(list->child_type) > 0) {
                free(list->elements.compact);
            } else {
                if (list->elements.units) {
                    for (int32_t i = 0; i < list->count; ++i)
                        free_payload(list->child_type, &list->elements.units[i]);
                    free(list->elements.units);
                }
            }
            free(list);
            break;
        }
        case NBT_Comp: {
            NBT_comp *comp = (NBT_comp*)payload->ptr;
            if (comp) {
                for (int32_t i = 0; i < comp->count; ++i) free_tag(comp->children[i]);
                free(comp->children);
                free(comp);
            }
            break;
        }
        case NBT_Int_Array: {
            NBT_ints *p = (NBT_ints*)payload->ptr;
            if (p) { free(p->data); free(p); }
            break;
        }
        case NBT_Long_Array: {
            NBT_longs *p = (NBT_longs*)payload->ptr;
            if (p) { free(p->data); free(p); }
            break;
        }
        case NBT_Short_Array: {
            NBT_shorts *p = (NBT_shorts*)payload->ptr;
            if (p) { free(p->data); free(p); }
            break;
        }
        default: break;
    }
    payload->ptr = NULL;
}

// 使用缓冲区解析指定类型的负载数据（深度受限）
static int parse_payload_buf(uint8_t type_id, Buffer* buf, nbt_Payload* payload, nbt_endian endian, int depth) {
    if (!buf || !payload) return -1;
    if (depth > NBT_MAX_DEPTH) return -1;

    int size = type2size(type_id);
    if (size > 0) {
        void *dest = NULL;
        switch (type_id) {
            case NBT_Byte:   dest = &payload->b; break;
            case NBT_Short:  dest = &payload->s; break;
            case NBT_Int:    dest = &payload->i; break;
            case NBT_Long:   dest = &payload->l; break;
            case NBT_Float:  dest = &payload->f; break;
            case NBT_Double: dest = &payload->d; break;
            default: return -1;
        }
        return rtype_buf(buf, type_id, dest, endian);
    }

    // 允许 TAG_End 作为空负载（列表中可能出现）
    if (type_id == NBT_End) return 0;

    switch (type_id) {
        case NBT_Byte_Array: {
            int32_t len;
            if (!buf_ri32(buf, &len, endian)) return -1;
            if (len < 0) return -1;
            NBT_bytes *arr = (NBT_bytes*)malloc(sizeof(NBT_bytes));
            if (!arr) return -1;
            arr->length = len;
            if (len > 0) {
                arr->data = (uint8_t*)malloc(len);
                if (!arr->data) { free(arr); return -1; }
                if (!buf_rdbytes(buf, arr->data, len)) { free(arr->data); free(arr); return -1; }
            } else arr->data = NULL;
            payload->ptr = arr;
            return 0;
        }
        case NBT_Str: {
            uint16_t len;
            if (!buf_ru16(buf, &len, endian)) return -1;
            NBT_str *str = (NBT_str*)malloc(sizeof(NBT_str));
            if (!str) return -1;
            str->length = len;
            if (len > 0) {
                str->data = (char*)malloc(len + 1);
                if (!str->data) { free(str); return -1; }
                if (!buf_rdbytes(buf, (uint8_t*)str->data, len)) { free(str->data); free(str); return -1; }
                str->data[len] = '\0';
            } else str->data = NULL;
            payload->ptr = str;
            return 0;
        }
        case NBT_List: {
            uint8_t child_type;
            if (!buf_ru8(buf, &child_type)) return -1;
            // 不检查子类型合法性，完全兼容所有值（包括未知类型和 End）
            int32_t count;
            if (!buf_ri32(buf, &count, endian)) return -1;
            if (count < 0) return -1;
            NBT_list *list = (NBT_list*)malloc(sizeof(NBT_list));
            if (!list) return -1;
            list->child_type = child_type;
            list->count = count;

            int esize = type2size(child_type);
            if (esize > 0) {
                if (count > 0) {
                    size_t total;
                    if (!safe_mul_size((size_t)count, (size_t)esize, &total)) { free(list); return -1; }
                    list->elements.compact = NULL;
                    if (endian == NBT_NATIVE_ENDIAN) {
                        list->elements.compact = malloc(total);
                        if (!list->elements.compact) { free(list); return -1; }
                        if (!buf_rdbytes(buf, list->elements.compact, total)) {
                            free(list->elements.compact); free(list); return -1;
                        }
                    } else {
                        list->elements.compact = malloc(total);
                        if (!list->elements.compact) { free(list); return -1; }
                        uint8_t *dest = (uint8_t*)list->elements.compact;
                        for (int32_t i = 0; i < count; ++i) {
                            if (rtype_buf(buf, child_type, dest, endian) != 0) {
                                free(list->elements.compact); free(list); return -1;
                            }
                            dest += esize;
                        }
                    }
                } else list->elements.compact = NULL;
            } else {
                list->elements.units = NULL;
                if (count > 0) {
                    size_t units_size;
                    if (!safe_mul_size((size_t)count, sizeof(nbt_Payload), &units_size)) { free(list); return -1; }
                    list->elements.units = (nbt_Payload*)malloc(units_size);
                    if (!list->elements.units) { free(list); return -1; }
                    for (int32_t i = 0; i < count; ++i) {
                        if (parse_payload_buf(child_type, buf, &list->elements.units[i], endian, depth + 1) != 0) {
                            // 释放已成功解析的元素
                            for (int32_t j = 0; j < i; ++j) {
                                free_payload(child_type, &list->elements.units[j]);
                            }
                            free(list->elements.units); free(list);
                            return -1;
                        }
                    }
                }
            }
            payload->ptr = list;
            return 0;
        }
        case NBT_Comp: {
            NBT_comp *comp = (NBT_comp*)malloc(sizeof(NBT_comp));
            if (!comp) return -1;
            comp->count = 0;
            comp->children = NULL;
            while (1) {
                if (comp->count >= NBT_MAX_COMPOUND_ELEMENTS) {
                    for (int32_t i = 0; i < comp->count; ++i) free_tag(comp->children[i]);
                    free(comp->children); free(comp);
                    return -1;
                }
                uint8_t next_type;
                if (!buf_ru8(buf, &next_type)) {
                    for (int32_t i = 0; i < comp->count; ++i) free_tag(comp->children[i]);
                    free(comp->children); free(comp);
                    return -1;
                }
                if (next_type == NBT_End) break;
                buf->pos--;
                struct NBT_tag *child = parse_tag_buf(buf, endian, depth + 1);
                if (!child) {
                    for (int32_t i = 0; i < comp->count; ++i) free_tag(comp->children[i]);
                    free(comp->children); free(comp);
                    return -1;
                }
                struct NBT_tag **new_children = (struct NBT_tag**)realloc(comp->children, sizeof(struct NBT_tag*) * (comp->count + 1));
                if (!new_children) {
                    free_tag(child);
                    for (int32_t i = 0; i < comp->count; ++i) free_tag(comp->children[i]);
                    free(comp->children); free(comp);
                    return -1;
                }
                comp->children = new_children;
                comp->children[comp->count++] = child;
            }
            payload->ptr = comp;
            return 0;
        }
        PARSE_ARRAY_CASE(NBT_Int_Array,  NBT_ints,  int32_t, buf_ri32)
        PARSE_ARRAY_CASE(NBT_Long_Array, NBT_longs, int64_t, buf_ri64)
        PARSE_ARRAY_CASE(NBT_Short_Array,NBT_shorts,int16_t, buf_ri16)
        default: return -1;
    }
}

// 使用缓冲区解析一个非 End 标签（深度受限）
static struct NBT_tag* parse_tag_buf(Buffer* buf, nbt_endian endian, int depth) {
    if (depth > NBT_MAX_DEPTH) return NULL;

    uint8_t type_id;
    if (!buf_ru8(buf, &type_id)) return NULL;
    if (type_id == NBT_End) return NULL;

    uint16_t name_len;
    if (!buf_ru16(buf, &name_len, endian)) return NULL;

    struct NBT_tag* tag = (struct NBT_tag*)malloc(sizeof(struct NBT_tag));
    if (!tag) return NULL;
    tag->payload.ptr = NULL;

    tag->type_id = type_id;
    tag->name_length = name_len;
    if (name_len > 0) {
        tag->name = (char*)malloc(name_len + 1);
        if (!tag->name) { free(tag); return NULL; }
        if (!buf_rdbytes(buf, (uint8_t*)tag->name, name_len)) {
            free(tag->name); free(tag);
            return NULL;
        }
        tag->name[name_len] = '\0';
    } else {
        tag->name = NULL;
    }

    if (parse_payload_buf(type_id, buf, &tag->payload, endian, depth + 1) != 0) {
        free_tag(tag);
        return NULL;
    }
    return tag;
}

// 将负载数据写入动态缓冲区（深度受限）
static int write_payload(uint8_t type_id, const nbt_Payload* payload, Buffer* buf, nbt_endian endian, int depth) {
    if (!payload || !buf) return 0;
    if (depth > NBT_MAX_DEPTH) return 0;

    int esize = type2size(type_id);
    if (esize > 0) {
        switch (type_id) {
            case NBT_Byte:   return buf_wru8(buf, (uint8_t)payload->b);
            case NBT_Short:  return buf_wi16(buf, payload->s, endian);
            case NBT_Int:    return buf_wi32(buf, payload->i, endian);
            case NBT_Long:   return buf_wi64(buf, payload->l, endian);
            case NBT_Float:  return buf_wf32(buf, payload->f, endian);
            case NBT_Double: return buf_wf64(buf, payload->d, endian);
            default: return 0;
        }
    }

    switch (type_id) {
        case NBT_Byte_Array: {
            NBT_bytes* arr = (NBT_bytes*)payload->ptr;
            if (!arr) return 0;
            if (!buf_wi32(buf, arr->length, endian)) return 0;
            if (arr->length > 0) if (!buf_wrbytes(buf, arr->data, arr->length)) return 0;
            return 1;
        }
        case NBT_Str: {
            NBT_str* str = (NBT_str*)payload->ptr;
            if (!str) return 0;
            if (!buf_wu16(buf, (uint16_t)str->length, endian)) return 0;
            if (str->length > 0) if (!buf_wrbytes(buf, (const uint8_t*)str->data, str->length)) return 0;
            return 1;
        }
        case NBT_List: {
            NBT_list* list = (NBT_list*)payload->ptr;
            if (!list) return 0;
            if (!buf_wru8(buf, list->child_type)) return 0;
            if (!buf_wi32(buf, list->count, endian)) return 0;
            if (list->count <= 0) return 1;
            int child_esize = type2size(list->child_type);
            if (child_esize > 0) {
                uint8_t* src = (uint8_t*)list->elements.compact;
                if (!src) return 0;
                size_t total;
                if (!safe_mul_size((size_t)list->count, (size_t)child_esize, &total)) return 0;
                if (endian == NBT_NATIVE_ENDIAN) {
                    if (!buf_wrbytes(buf, src, total)) return 0;
                } else {
                    for (int32_t i = 0; i < list->count; ++i) {
                        uint8_t* elem = src + i * child_esize;
                        switch (list->child_type) {
                            case NBT_Byte:   if (!buf_wru8(buf, *elem)) return 0; break;
                            case NBT_Short:  if (!buf_wi16(buf, *(int16_t*)elem, endian)) return 0; break;
                            case NBT_Int:    if (!buf_wi32(buf, *(int32_t*)elem, endian)) return 0; break;
                            case NBT_Long:   if (!buf_wi64(buf, *(int64_t*)elem, endian)) return 0; break;
                            case NBT_Float:  if (!buf_wf32(buf, *(float*)elem, endian)) return 0; break;
                            case NBT_Double: if (!buf_wf64(buf, *(double*)elem, endian)) return 0; break;
                            default: return 0;
                        }
                    }
                }
            } else {
                nbt_Payload* units = list->elements.units;
                if (!units) return 0;
                for (int32_t i = 0; i < list->count; ++i) {
                    if (!write_payload(list->child_type, &units[i], buf, endian, depth + 1)) return 0;
                }
            }
            return 1;
        }
        case NBT_Comp: {
            NBT_comp* comp = (NBT_comp*)payload->ptr;
            if (!comp) return 0;
            for (int32_t i = 0; i < comp->count; ++i) {
                if (!write_tag(comp->children[i], buf, endian, depth + 1)) return 0;
            }
            return buf_wru8(buf, NBT_End);
        }
        WRITE_ARRAY_CASE(NBT_Int_Array,  NBT_ints,  int32_t, buf_wi32)
        WRITE_ARRAY_CASE(NBT_Long_Array, NBT_longs, int64_t, buf_wi64)
        WRITE_ARRAY_CASE(NBT_Short_Array,NBT_shorts,int16_t, buf_wi16)
        default: return 0;
    }
}

// 将完整标签写入动态缓冲区（深度受限）
static int write_tag(const struct NBT_tag* tag, Buffer* buf, nbt_endian endian, int depth) {
    if (!tag || !buf) return 0;
    if (depth > NBT_MAX_DEPTH) return 0;

    if (!buf_wru8(buf, tag->type_id)) return 0;
    if (!buf_wu16(buf, tag->name_length, endian)) return 0;
    if (tag->name_length > 0)
        if (!buf_wrbytes(buf, (const uint8_t*)tag->name, tag->name_length)) return 0;
    if (tag->type_id != NBT_End)
        if (!write_payload(tag->type_id, &tag->payload, buf, endian, depth)) return 0;
    return 1;
}


// 计算 payload 序列化后所占字节数（不含 type_id / name 等标签头）
static size_t payload_ser_size(uint8_t type_id, const nbt_Payload* payload, int depth) {
    if (!payload) return 0;
    if (depth > NBT_MAX_DEPTH) return 0;

    int esize = type2size(type_id);
    if (esize > 0) return (size_t)esize;
    if (type_id == NBT_End) return 0;

    switch (type_id) {
        case NBT_Byte_Array: {
            NBT_bytes* arr = (NBT_bytes*)payload->ptr;
            if (!arr) return 0;
            return (size_t)4 + (size_t)arr->length;
        }
        case NBT_Str: {
            NBT_str* str = (NBT_str*)payload->ptr;
            if (!str) return 0;
            return (size_t)2 + (size_t)str->length;
        }
        case NBT_List: {
            NBT_list* list = (NBT_list*)payload->ptr;
            if (!list) return 0;
            size_t total = 1 + 4;
            if (list->count <= 0) return total;
            int child_esize = type2size(list->child_type);
            if (child_esize > 0) {
                size_t mul;
                if (!safe_mul_size((size_t)list->count, (size_t)child_esize, &mul)) return 0;
                total += mul;
            } else {
                nbt_Payload* units = list->elements.units;
                if (!units) return 0;
                for (int32_t i = 0; i < list->count; ++i) {
                    size_t s = payload_ser_size(list->child_type, &units[i], depth + 1);
                    if (s == 0) return 0;
                    total += s;
                }
            }
            return total;
        }
        case NBT_Comp: {
            NBT_comp* comp = (NBT_comp*)payload->ptr;
            if (!comp) return 0;
            size_t total = 0;
            for (int32_t i = 0; i < comp->count; ++i) {
                size_t s = tag_ser_size(comp->children[i], depth + 1);
                if (s == 0) return 0;
                total += s;
            }
            total += 1;
            return total;
        }
        SER_SIZE_ARRAY_CASE(NBT_Int_Array,  NBT_ints,  int32_t)
        SER_SIZE_ARRAY_CASE(NBT_Long_Array, NBT_longs, int64_t)
        SER_SIZE_ARRAY_CASE(NBT_Short_Array,NBT_shorts,int16_t)
        default: return 0;
    }
}

// 计算完整标签序列化后所占字节数
static size_t tag_ser_size(const struct NBT_tag* tag, int depth) {
    if (!tag) return 0;
    if (depth > NBT_MAX_DEPTH) return 0;
    size_t total = 1 + 2 + tag->name_length;
    if (tag->type_id != NBT_End) {
        size_t ps = payload_ser_size(tag->type_id, &tag->payload, depth);
        if (ps == 0) return 0;
        total += ps;
    }
    return total;
}


// 公开接口：从字节流加载 NBT 数据
struct NBT_tag* nbt_load(const uint8_t* data, size_t len, nbt_endian endian) {
    if (!data || len == 0) return NULL;
    Buffer buf;
    buf_wrap(&buf, data, len);
    return parse_tag_buf(&buf, endian, 0);
}

// 公开接口：递归释放标签树内存
void free_tag(struct NBT_tag* tag) {
    if (!tag) return;
    free(tag->name);
    free_payload(tag->type_id, &tag->payload);
    free(tag);
}

// 公开接口：序列化为动态缓冲区
uint8_t* nbt_write(const struct NBT_tag* root, size_t* out_len, nbt_endian endian) {
    if (!root || !out_len) return NULL;
    Buffer buf;
    if (!buf_init(&buf, 256)) return NULL;
    if (!write_tag(root, &buf, endian, 0)) {
        buf_free(&buf);
        return NULL;
    }
    *out_len = buf.used;
    uint8_t* final_data = (uint8_t*)realloc(buf.data, buf.used);
    if (final_data) {
        buf.data = final_data;
        buf.cap = buf.used;
    }
    return buf.data;
}

// 公开接口：序列化为精确大小的缓冲区（预计算大小，一次分配，零 realloc）
uint8_t* nbt_write2(const struct NBT_tag* root, size_t* out_len, nbt_endian endian) {
    if (!root || !out_len) return NULL;
    size_t total = tag_ser_size(root, 0);
    if (total == 0) return NULL;
    Buffer buf;
    if (!buf_init(&buf, total)) return NULL;
    if (!write_tag(root, &buf, endian, 0)) {
        buf_free(&buf);
        return NULL;
    }
    *out_len = buf.used;
    return buf.data;
}