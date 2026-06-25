// snbtlist.c
#include "snbtlist.h"
#include "snbtstring.h"
#include "snbtarray.h"
#include "snbtbuffer.h"
#include "snbterror.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

#define SNBT_MAX_DEPTH 512

// 引入 nbt.c 中的函数，用于获取基本类型的字节大小
extern int type2size(uint8_t type_id);

// 跳过空白字符，返回下一个非空白字符位置
static const char* skip_ws(const char* s) {
    while (*s && isspace((unsigned char)*s)) s++;
    return s;
}

// 递归释放任意类型的 payload 内存
static void free_payload(uint8_t type, nbt_Payload* p) {
    if (!p) return;
    switch (type) {
        case NBT_Str: {
            NBT_str* s = (NBT_str*)p->ptr;
            free(s->data); free(s); break;
        }
        case NBT_Byte_Array: {
            NBT_bytes* a = (NBT_bytes*)p->ptr;
            free(a->data); free(a); break;
        }
        case NBT_Int_Array: {
            NBT_ints* a = (NBT_ints*)p->ptr;
            free(a->data); free(a); break;
        }
        case NBT_Long_Array: {
            NBT_longs* a = (NBT_longs*)p->ptr;
            free(a->data); free(a); break;
        }
        case NBT_List: {
            NBT_list* l = (NBT_list*)p->ptr;
            if (l && l->elements.units) {
                for (int32_t i = 0; i < l->count; i++)
                    free_payload(l->child_type, &l->elements.units[i]);
                free(l->elements.units);
            }
            free(l); break;
        }
        case NBT_Comp: {
            NBT_comp* c = (NBT_comp*)p->ptr;
            if (c) {
                for (int32_t i = 0; i < c->count; i++) free_tag(c->children[i]);
                free(c->children);
                free(c);
            }
            break;
        }
        default: break;
    }
    p->ptr = NULL;
}

// 前向声明内部列表解析
static int parse_list_internal(const char** p, int depth, nbt_Payload* out);

// 通用 SNBT 值解析器，递归解析任意值，*p 前进到值结束后的位置
int snbt_parse_value(const char** p, int depth, nbt_Payload* out, uint8_t* type) {
    if (!p || !*p || !out || !type) return 0;
    const char* s = skip_ws(*p);
    if (!*s) return 0;
    char first = *s;

    // 引号字符串
    if (first == '"' || first == '\'') {
        int ret = str2string(s, out);
        if (ret != NBT_Str) return 0;
        *type = NBT_Str;
        char quote = first;
        const char* t = s + 1;
        while (*t && *t != quote) {
            if (*t == '\\' && *(t+1)) t++;
            t++;
        }
        if (*t == quote) t++;
        *p = t;
        return NBT_Str;
    }

    // 数组或列表
    if (first == '[') {
        const char* la = s + 1;
        la = skip_ws(la);
        // 检查是否为数组前缀 B; I; L;
        if ((tolower(*la) == 'b' || tolower(*la) == 'i' || tolower(*la) == 'l') &&
            *(skip_ws(la+1)) == ';') {
            int ret = str2array(s, out);
            if (!ret) return 0;
            *type = ret;
            const char* t = s;
            int bracket = 1;
            t++;
            while (*t && bracket > 0) {
                if (*t == '[') bracket++;
                else if (*t == ']') bracket--;
                t++;
            }
            *p = t;
            return ret;
        } else {
            int ret = parse_list_internal(p, depth + 1, out);
            if (!ret) return 0;
            *type = NBT_List;
            return NBT_List;
        }
    }

    // 复合标签
    if (first == '{') {
        int ret = parse_compound_internal(p, depth + 1, out);
        if (!ret) return 0;
        *type = NBT_Comp;
        return NBT_Comp;
    }

    // 提取数值或无引号字符串 token
    const char* start = s;
    if (isalpha((unsigned char)*s) || *s == '_') {
        while (isalnum((unsigned char)*s) || *s == '_' || *s == '.' || *s == '+' || *s == '-') s++;
    } else if (isdigit((unsigned char)*s) || *s == '.' || *s == '+' || *s == '-') {
        while (isalnum((unsigned char)*s) || *s == '_' || *s == '.' || 
               tolower(*s) == 'e' || *s == '+' || *s == '-') s++;
    } else {
        return 0;
    }

    size_t tok_len = s - start;
    char* token = (char*)malloc(tok_len + 1);
    if (!token) return 0;
    memcpy(token, start, tok_len);
    token[tok_len] = '\0';

    nbt_Payload tmp;
    memset(&tmp, 0, sizeof(tmp));

    // 依次尝试整数、浮点、字符串
    snbt_set_error(SNBT_SUCCESS);
    int num_type = str2integer(token, &tmp);
    if (num_type) {
        *type = num_type;
        *out = tmp;  // 整体复制，避免截断
        free(token);
        *p = s;
        return num_type;
    }

    snbt_set_error(SNBT_SUCCESS);
    int float_type = str2float(token, &tmp);
    if (float_type) {
        *type = float_type;
        *out = tmp;
        free(token);
        *p = s;
        return float_type;
    }

    snbt_set_error(SNBT_SUCCESS);
    int str_type = str2string(token, &tmp);
    if (str_type) {
        *type = str_type;
        out->ptr = tmp.ptr;
        free(token);
        *p = s;
        return str_type;
    }

    // 全部失败，记录错误 token
    snbt_set_error(SNBT_ERR_INVALID_PREFIX);
    snbt_set_error_msg(token);
    free(token);
    return 0;
}

// 内部列表解析，要求元素类型一致，空列表设置警告
static int parse_list_internal(const char** p, int depth, nbt_Payload* out) {
    if (depth > SNBT_MAX_DEPTH) { snbt_set_error(SNBT_ERR_OUT_OF_RANGE); return 0; }
    const char* s = skip_ws(*p);
    if (*s != '[') return 0;
    s++;

    NBT_list* list = (NBT_list*)calloc(1, sizeof(NBT_list));
    if (!list) { snbt_set_error(SNBT_ERR_UNKNOWN); return 0; }
    list->child_type = 0;
    list->count = 0;
    list->elements.units = NULL;

    nbt_Payload* elems = NULL;
    int32_t cap = 0;

    s = skip_ws(s);
    if (*s == ']') {      // 空列表
        s++;
        *p = s;
        out->ptr = list;
        snbt_set_error(SNBT_WARN_EMPTY_LIST);  // 警告：类型信息丢失
        return NBT_List;
    }

    // 解析元素直至 ']'
    while (1) {
        s = skip_ws(s);
        if (*s == ']') break;
        if (list->count > 0) {
            if (*s == ',') {
                s++;
                s = skip_ws(s);
                if (*s == ']') break; // 结尾逗号
            } else {
                for (int32_t i = 0; i < list->count; i++)
                    free_payload(list->child_type, &elems[i]);
                free(elems); free(list);
                snbt_set_error(SNBT_ERR_INVALID_PREFIX);
                snbt_set_error_msg(s);
                return 0;
            }
        }

        nbt_Payload elem;
        uint8_t elem_type;
        if (!snbt_parse_value(&s, depth, &elem, &elem_type)) {
            for (int32_t i = 0; i < list->count; i++)
                free_payload(list->child_type, &elems[i]);
            free(elems); free(list);
            return 0;
        }

        // 类型检查：第一个元素确定子类型，后续必须一致
        if (list->count == 0) {
            list->child_type = elem_type;
        } else if (list->child_type != elem_type) {
            for (int32_t i = 0; i < list->count; i++)
                free_payload(list->child_type, &elems[i]);
            free_payload(elem_type, &elem);
            free(elems); free(list);
            snbt_set_error(SNBT_ERR_TYPE_MISMATCH);
            return 0;
        }

        if (list->count >= cap) {
            cap = cap ? cap * 2 : 4;
            nbt_Payload* new_elems = (nbt_Payload*)realloc(elems, cap * sizeof(nbt_Payload));
            if (!new_elems) {
                for (int32_t i = 0; i < list->count; i++)
                    free_payload(list->child_type, &elems[i]);
                free_payload(elem_type, &elem);
                free(elems); free(list);
                snbt_set_error(SNBT_ERR_UNKNOWN);
                return 0;
            }
            elems = new_elems;
        }
        elems[list->count++] = elem;
    }

    // 根据元素类型选择存储方式：基本类型使用紧凑内存，复杂类型使用 units 数组
    if (list->count > 0) {
        int esize = type2size(list->child_type);
        if (esize > 0) {
            // 紧凑存储
            list->elements.compact = malloc(list->count * esize);
            if (!list->elements.compact) {
                for (int32_t i = 0; i < list->count; i++)
                    free_payload(list->child_type, &elems[i]);
                free(elems); free(list);
                return 0;
            }
            uint8_t* dst = (uint8_t*)list->elements.compact;
            for (int32_t i = 0; i < list->count; i++) {
                memcpy(dst + i * esize, &elems[i], esize);
            }
            free(elems);
        } else {
            // units 数组
            list->elements.units = (nbt_Payload*)malloc(list->count * sizeof(nbt_Payload));
            if (!list->elements.units) {
                for (int32_t i = 0; i < list->count; i++)
                    free_payload(list->child_type, &elems[i]);
                free(elems); free(list);
                return 0;
            }
            memcpy(list->elements.units, elems, list->count * sizeof(nbt_Payload));
            free(elems);
        }
    }

    s++;
    *p = s;
    out->ptr = list;
    return NBT_List;
}

// 内部复合标签解析，递归实现，供 snbt_parse_value 调用
int parse_compound_internal(const char** p, int depth, nbt_Payload* out) {
    if (depth > SNBT_MAX_DEPTH) { snbt_set_error(SNBT_ERR_OUT_OF_RANGE); return 0; }
    const char* s = skip_ws(*p);
    if (*s != '{') return 0;
    s++;

    NBT_comp* comp = (NBT_comp*)calloc(1, sizeof(NBT_comp));
    if (!comp) { snbt_set_error(SNBT_ERR_UNKNOWN); return 0; }
    comp->count = 0;
    comp->children = NULL;

    s = skip_ws(s);
    if (*s == '}') {      // 空复合标签
        s++;
        *p = s;
        out->ptr = comp;
        return NBT_Comp;
    }

    while (1) {
        s = skip_ws(s);
        if (*s == '}') break;
        if (comp->count > 0) {
            if (*s == ',') {
                s++;
                s = skip_ws(s);
                if (*s == '}') break; // 结尾逗号
            } else {
                for (int32_t i = 0; i < comp->count; i++) free_tag(comp->children[i]);
                free(comp->children); free(comp);
                snbt_set_error(SNBT_ERR_INVALID_PREFIX);
                snbt_set_error_msg(s);
                return 0;
            }
        }

        // 解析键（必须是字符串）
        nbt_Payload key_payload;
        uint8_t key_type;
        if (!snbt_parse_value(&s, depth, &key_payload, &key_type) || key_type != NBT_Str) {
            for (int32_t i = 0; i < comp->count; i++) free_tag(comp->children[i]);
            free(comp->children); free(comp);
            snbt_set_error(SNBT_ERR_INVALID_PREFIX);
            return 0;
        }
        char* key = ((NBT_str*)key_payload.ptr)->data;
        int32_t key_len = ((NBT_str*)key_payload.ptr)->length;

        s = skip_ws(s);
        if (*s != ':') {
            free(key); free(key_payload.ptr);
            for (int32_t i = 0; i < comp->count; i++) free_tag(comp->children[i]);
            free(comp->children); free(comp);
            snbt_set_error(SNBT_ERR_INVALID_PREFIX);
            snbt_set_error_msg(s);
            return 0;
        }
        s++;

        // 解析值
        nbt_Payload val_payload;
        uint8_t val_type;
        if (!snbt_parse_value(&s, depth, &val_payload, &val_type)) {
            free(key); free(key_payload.ptr);
            for (int32_t i = 0; i < comp->count; i++) free_tag(comp->children[i]);
            free(comp->children); free(comp);
            return 0;
        }

        struct NBT_tag* child = (struct NBT_tag*)calloc(1, sizeof(struct NBT_tag));
        if (!child) {
            free(key); free(key_payload.ptr);
            for (int32_t i = 0; i < comp->count; i++) free_tag(comp->children[i]);
            free(comp->children); free(comp);
            snbt_set_error(SNBT_ERR_UNKNOWN);
            return 0;
        }
        child->type_id = val_type;
        child->name_length = (uint16_t)key_len;
        child->name = key;
        child->payload = val_payload;
        free(key_payload.ptr);  // 释放外壳，数据已转移

        struct NBT_tag** new_children = (struct NBT_tag**)realloc(comp->children, (comp->count + 1) * sizeof(struct NBT_tag*));
        if (!new_children) {
            free_tag(child);
            for (int32_t i = 0; i < comp->count; i++) free_tag(comp->children[i]);
            free(comp->children); free(comp);
            snbt_set_error(SNBT_ERR_UNKNOWN);
            return 0;
        }
        comp->children = new_children;
        comp->children[comp->count++] = child;
    }

    s++;
    *p = s;
    out->ptr = comp;
    return NBT_Comp;
}

// 公开的列表解析接口
int str2list(const char* str, nbt_Payload* out) {
    const char* p = str;
    return parse_list_internal(&p, 0, out);
}

// 前向声明 compound2str，定义在 snbtcompound.c
extern int compound2str(const nbt_Payload* val, char** out, const SnbtNumberStyle* style);

// 列表格式化为 SNBT 字符串，支持紧凑存储和 units 两种内部表示
int list2str(const nbt_Payload* val, char** out, const SnbtNumberStyle* style) {
    if (!val || !out || !style) return 0;
    NBT_list* list = (NBT_list*)val->ptr;
    if (!list) return 0;

    SnbtBuf sb;
    if (!snbtbuf_init(&sb, 256)) return 0;

    snbtbuf_putc(&sb, '[');

    int esize = type2size(list->child_type);
    if (esize > 0 && list->elements.compact != NULL) {
        uint8_t* data = (uint8_t*)list->elements.compact;
        nbt_Payload elem;
        for (int32_t i = 0; i < list->count; i++) {
            if (i > 0) snbtbuf_putc(&sb, ',');
            memset(&elem, 0, sizeof(elem));
            memcpy(&elem, data + i * esize, esize);
            char* elem_str = NULL;
            uint8_t type = list->child_type;
            switch (type) {
                case NBT_Byte: case NBT_Short: case NBT_Int: case NBT_Long:
                    if (!integer2str(type, &elem, &elem_str, style)) { snbtbuf_free(&sb); return 0; }
                    break;
                case NBT_Float: case NBT_Double:
                    if (!float2str(type, &elem, &elem_str, style)) { snbtbuf_free(&sb); return 0; }
                    break;
                default: snbtbuf_free(&sb); return 0;
            }
            snbtbuf_append(&sb, elem_str, strlen(elem_str));
            free(elem_str);
        }
    } else {
        for (int32_t i = 0; i < list->count; i++) {
            if (i > 0) snbtbuf_putc(&sb, ',');
            nbt_Payload* elem = &list->elements.units[i];
            char* elem_str = NULL;
            uint8_t type = list->child_type;
            switch (type) {
                case NBT_Byte: case NBT_Short: case NBT_Int: case NBT_Long:
                    if (!integer2str(type, elem, &elem_str, style)) { snbtbuf_free(&sb); return 0; }
                    break;
                case NBT_Float: case NBT_Double:
                    if (!float2str(type, elem, &elem_str, style)) { snbtbuf_free(&sb); return 0; }
                    break;
                case NBT_Str:
                    if (!string2str(elem, &elem_str)) { snbtbuf_free(&sb); return 0; }
                    break;
                case NBT_Byte_Array: case NBT_Int_Array: case NBT_Long_Array:
                    if (!array2str(type, elem, &elem_str, style)) { snbtbuf_free(&sb); return 0; }
                    break;
                case NBT_List:
                    if (!list2str(elem, &elem_str, style)) { snbtbuf_free(&sb); return 0; }
                    break;
                case NBT_Comp:
                    if (!compound2str(elem, &elem_str, style)) { snbtbuf_free(&sb); return 0; }
                    break;
                default:
                    snbtbuf_free(&sb); return 0;
            }
            snbtbuf_append(&sb, elem_str, strlen(elem_str));
            free(elem_str);
        }
    }

    snbtbuf_putc(&sb, ']');
    snbtbuf_cstr(&sb);
    *out = snbtbuf_steal(&sb);
    return 1;
}