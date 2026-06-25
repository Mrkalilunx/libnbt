#include "snbtcompound.h"
#include "snbtlist.h"
#include "snbtstring.h"
#include "snbtarray.h"
#include "snbtbuffer.h"
#include "snbterror.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

// 解析复合标签，直接调用内部实现
int str2compound(const char* str, nbt_Payload* out) {
    const char* p = str;
    return parse_compound_internal(&p, 0, out);
}

// 将复合标签格式化为 SNBT 字符串
int compound2str(const nbt_Payload* val, char** out, const SnbtNumberStyle* style) {
    if (!val || !out || !style) return 0;
    NBT_comp* comp = (NBT_comp*)val->ptr;
    if (!comp) return 0;

    SnbtBuf sb;
    if (!snbtbuf_init(&sb, 256)) return 0;

    snbtbuf_putc(&sb, '{');

    for (int32_t i = 0; i < comp->count; i++) {
        if (i > 0) snbtbuf_putc(&sb, ',');
        struct NBT_tag* child = comp->children[i];

        nbt_Payload key_payload;
        NBT_str key_str;
        key_str.length = child->name_length;
        key_str.data = child->name;
        key_payload.ptr = &key_str;
        char* key_out = NULL;
        if (!string2str(&key_payload, &key_out)) { snbtbuf_free(&sb); return 0; }
        snbtbuf_append(&sb, key_out, strlen(key_out));
        free(key_out);
        snbtbuf_putc(&sb, ':');

        char* val_str = NULL;
        switch (child->type_id) {
            case NBT_Byte: case NBT_Short: case NBT_Int: case NBT_Long:
                if (!integer2str(child->type_id, &child->payload, &val_str, style)) { snbtbuf_free(&sb); return 0; }
                break;
            case NBT_Float: case NBT_Double:
                if (!float2str(child->type_id, &child->payload, &val_str, style)) { snbtbuf_free(&sb); return 0; }
                break;
            case NBT_Str:
                if (!string2str(&child->payload, &val_str)) { snbtbuf_free(&sb); return 0; }
                break;
            case NBT_Byte_Array: case NBT_Int_Array: case NBT_Long_Array:
                if (!array2str(child->type_id, &child->payload, &val_str, style)) { snbtbuf_free(&sb); return 0; }
                break;
            case NBT_List:
                if (!list2str(&child->payload, &val_str, style)) { snbtbuf_free(&sb); return 0; }
                break;
            case NBT_Comp:
                if (!compound2str(&child->payload, &val_str, style)) { snbtbuf_free(&sb); return 0; }
                break;
            default:
                snbtbuf_free(&sb); return 0;
        }
        snbtbuf_append(&sb, val_str, strlen(val_str));
        free(val_str);
    }

    snbtbuf_putc(&sb, '}');
    snbtbuf_cstr(&sb);
    *out = snbtbuf_steal(&sb);
    return 1;
}