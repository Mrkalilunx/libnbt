// snbt.c
#include "snbt.h"
#include "snbterror.h"
#include "snbtbuffer.h"
#include "snbtlist.h"
#include "snbtstring.h"
#include "snbtarray.h"
#include "snbtnumber.h"
#include "snbtcompound.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static const SnbtNumberStyle kDefaultStyle = {
    .base_byte = 10, .base_short = 10, .base_int = 10, .base_long = 10,
    .show_int_suffix = 0,
    .uppercase = 0
};

// 从 SNBT 字符串加载 NBT 标签树
struct NBT_tag* snbt_load(const char* str) {
    if (!str) {
        snbt_set_error(SNBT_ERR_NULL_ARGUMENT);
        return NULL;
    }

    const char* p = str;
    nbt_Payload payload;
    uint8_t type;

    if (!snbt_parse_value(&p, 0, &payload, &type)) {
        return NULL;
    }

    struct NBT_tag* root = (struct NBT_tag*)calloc(1, sizeof(struct NBT_tag));
    if (!root) {
        snbt_set_error(SNBT_ERR_UNKNOWN);
        return NULL;
    }
    root->type_id = type;
    root->name_length = 0;
    root->name = NULL;
    root->payload = payload;

    snbt_set_error(SNBT_SUCCESS);
    return root;
}

// 将紧凑 SNBT 字符串美化（2空格缩进）
static char* snbt_pretty(const char* compact) {
    if (!compact) return NULL;
    SnbtBuf db;
    if (!snbtbuf_init(&db, strlen(compact) * 2 + 256)) return NULL;

    int indent = 0;
    int in_string = 0;
    char quote_char = 0;

    for (const char* p = compact; *p; p++) {
        char c = *p;

        if (!in_string && (c == '"' || c == '\'')) {
            in_string = 1;
            quote_char = c;
            snbtbuf_putc(&db, c);
            continue;
        }
        if (in_string) {
            snbtbuf_putc(&db, c);
            if (c == '\\' && *(p+1)) {
                snbtbuf_putc(&db, *(++p));
            } else if (c == quote_char) {
                in_string = 0;
                quote_char = 0;
            }
            continue;
        }

        switch (c) {
            case '{': case '[':
                snbtbuf_putc(&db, c);
                snbtbuf_putc(&db, '\n');
                indent++;
                for (int i = 0; i < indent * 2; i++) snbtbuf_putc(&db, ' ');
                break;
            case '}': case ']':
                indent--;
                snbtbuf_putc(&db, '\n');
                for (int i = 0; i < indent * 2; i++) snbtbuf_putc(&db, ' ');
                snbtbuf_putc(&db, c);
                break;
            case ',':
                snbtbuf_putc(&db, c);
                snbtbuf_putc(&db, '\n');
                for (int i = 0; i < indent * 2; i++) snbtbuf_putc(&db, ' ');
                break;
            case ':':
                snbtbuf_putc(&db, ':');
                snbtbuf_putc(&db, ' ');
                break;
            case ' ':
                break;
            default:
                snbtbuf_putc(&db, c);
                break;
        }
    }
    return snbtbuf_steal(&db);
}

// 返回字符串版本的序列化（内部使用紧凑生成+可选美化）
char* snbt_write(const struct NBT_tag* tag, int pretty) {
    if (!tag) {
        snbt_set_error(SNBT_ERR_NULL_ARGUMENT);
        return NULL;
    }

    char* out = NULL;
    int ok = 0;

    switch (tag->type_id) {
        case NBT_Byte: case NBT_Short: case NBT_Int: case NBT_Long:
            ok = integer2str(tag->type_id, &tag->payload, &out, &kDefaultStyle);
            break;
        case NBT_Float: case NBT_Double:
            ok = float2str(tag->type_id, &tag->payload, &out, &kDefaultStyle);
            break;
        case NBT_Str:
            ok = string2str(&tag->payload, &out);
            break;
        case NBT_Byte_Array: case NBT_Int_Array: case NBT_Long_Array:
            ok = array2str(tag->type_id, &tag->payload, &out, &kDefaultStyle);
            break;
        case NBT_List:
            ok = list2str(&tag->payload, &out, &kDefaultStyle);
            break;
        case NBT_Comp:
            ok = compound2str(&tag->payload, &out, &kDefaultStyle);
            break;
        default:
            snbt_set_error(SNBT_ERR_UNKNOWN);
            return NULL;
    }

    if (!ok) return NULL;

    if (pretty) {
        char* pretty_out = snbt_pretty(out);
        free(out);
        if (!pretty_out) {
            snbt_set_error(SNBT_ERR_UNKNOWN);
            return NULL;
        }
        snbt_set_error(SNBT_SUCCESS);
        return pretty_out;
    }

    snbt_set_error(SNBT_SUCCESS);
    return out;
}

// ---------- 流式序列化（直接写文件） ----------

// 递归将单个标签写入流
static void snbt_write_tag_stream(const struct NBT_tag* tag, FILE* fp, int indent, int pretty) {
    if (!tag || !fp) return;

    switch (tag->type_id) {
        case NBT_Byte: case NBT_Short: case NBT_Int: case NBT_Long: {
            char* s = NULL;
            if (integer2str(tag->type_id, &tag->payload, &s, &kDefaultStyle)) {
                fputs(s, fp);
                free(s);
            }
            break;
        }
        case NBT_Float: case NBT_Double: {
            char* s = NULL;
            if (float2str(tag->type_id, &tag->payload, &s, &kDefaultStyle)) {
                fputs(s, fp);
                free(s);
            }
            break;
        }
        case NBT_Str: {
            char* s = NULL;
            if (string2str(&tag->payload, &s)) {
                fputs(s, fp);
                free(s);
            }
            break;
        }
        case NBT_Byte_Array: case NBT_Int_Array: case NBT_Long_Array: {
            char* s = NULL;
            if (array2str(tag->type_id, &tag->payload, &s, &kDefaultStyle)) {
                fputs(s, fp);
                free(s);
            }
            break;
        }
        case NBT_List: {
            NBT_list* list = (NBT_list*)tag->payload.ptr;
            if (!list) break;
            fputc('[', fp);
            int esize = type2size(list->child_type);
            for (int32_t i = 0; i < list->count; i++) {
                if (i > 0) fputc(',', fp);
                if (pretty) {
                    fprintf(fp, "\n%*s", (indent+1)*2, "");
                }

                nbt_Payload elem_payload;
                memset(&elem_payload, 0, sizeof(elem_payload));
                if (esize > 0 && list->elements.compact) {
                    memcpy(&elem_payload, (uint8_t*)list->elements.compact + i*esize, esize);
                } else if (list->elements.units) {
                    elem_payload = list->elements.units[i];
                }

                struct NBT_tag child = {
                    .type_id = list->child_type,
                    .payload = elem_payload,
                    .name = NULL, .name_length = 0
                };
                snbt_write_tag_stream(&child, fp, indent+1, pretty);
            }
            if (pretty && list->count > 0) {
                fprintf(fp, "\n%*s", indent*2, "");
            }
            fputc(']', fp);
            break;
        }
        case NBT_Comp: {
            NBT_comp* comp = (NBT_comp*)tag->payload.ptr;
            if (!comp) break;
            fputc('{', fp);
            for (int32_t i = 0; i < comp->count; i++) {
                if (i > 0) fputc(',', fp);
                if (pretty) {
                    fprintf(fp, "\n%*s", (indent+1)*2, "");
                }
                struct NBT_tag* child = comp->children[i];
                nbt_Payload key_payload;
                NBT_str key_str;
                key_str.length = child->name_length;
                key_str.data = child->name;
                key_payload.ptr = &key_str;
                char* key_out = NULL;
                if (string2str(&key_payload, &key_out)) {
                    fputs(key_out, fp);
                    free(key_out);
                }
                fputs(": ", fp);
                snbt_write_tag_stream(child, fp, indent+1, pretty);
            }
            if (pretty && comp->count > 0) {
                fprintf(fp, "\n%*s", indent*2, "");
            }
            fputc('}', fp);
            break;
        }
        default: break;
    }
}

// 流式写入接口
int snbt_write_stream(const struct NBT_tag* tag, int pretty, FILE* fp) {
    if (!tag || !fp) {
        snbt_set_error(SNBT_ERR_NULL_ARGUMENT);
        return 0;
    }
    snbt_write_tag_stream(tag, fp, 0, pretty);
    if (pretty) fputc('\n', fp);
    snbt_set_error(SNBT_SUCCESS);
    return 1;
}
