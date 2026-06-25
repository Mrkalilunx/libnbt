#include "snbtstring.h"
#include "snbterror.h"
#include "snbtbuffer.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

// UTF-8 编码
static int encode_utf8(uint32_t cp, SnbtBuf* sb) {
    char buf[4];
    int len;
    if (cp <= 0x7F) {
        buf[0] = (char)cp; len = 1;
    } else if (cp <= 0x7FF) {
        buf[0] = (char)(0xC0 | (cp >> 6));
        buf[1] = (char)(0x80 | (cp & 0x3F)); len = 2;
    } else if (cp <= 0xFFFF) {
        buf[0] = (char)(0xE0 | (cp >> 12));
        buf[1] = (char)(0x80 | ((cp >> 6) & 0x3F));
        buf[2] = (char)(0x80 | (cp & 0x3F)); len = 3;
    } else if (cp <= 0x10FFFF) {
        buf[0] = (char)(0xF0 | (cp >> 18));
        buf[1] = (char)(0x80 | ((cp >> 12) & 0x3F));
        buf[2] = (char)(0x80 | ((cp >> 6) & 0x3F));
        buf[3] = (char)(0x80 | (cp & 0x3F)); len = 4;
    } else return 0;
    return snbtbuf_append(sb, buf, len);
}

static int hex_val(char c) {
    if (c >= '0' && c <= '9') return c - '0';
    if (c >= 'a' && c <= 'f') return c - 'a' + 10;
    if (c >= 'A' && c <= 'F') return c - 'A' + 10;
    return -1;
}

// \N{name} 有限映射表
typedef struct { const char* name; uint32_t cp; } NamedChar;
static const NamedChar named_chars[] = {
    {"Snowman", 0x2603},
};

static int resolve_named_char(const char* name, uint32_t* cp) {
    for (size_t i = 0; i < sizeof(named_chars)/sizeof(named_chars[0]); i++) {
        if (strcmp(name, named_chars[i].name) == 0) {
            *cp = named_chars[i].cp;
            return 1;
        }
    }
    return 0;
}

static int valid_name_char(int c) {
    return isalnum(c) || c == ' ' || c == '-';
}

static int parse_escape(const char* src, uint32_t* cp) {
    char c = src[0];
    switch (c) {
        case 'n': *cp = '\n'; return 1;
        case 'f': *cp = '\f'; return 1;
        case 'r': *cp = '\r'; return 1;
        case '\\': *cp = '\\'; return 1;
        case 'b': *cp = '\b'; return 1;
        case 't': *cp = '\t'; return 1;
        case 's': *cp = ' '; return 1;
        case '"': *cp = '"'; return 1;
        case '\'': *cp = '\''; return 1;
        case 'x': {
            int h1 = hex_val(src[1]), h2 = hex_val(src[2]);
            if (h1 < 0 || h2 < 0) return -1;
            *cp = (uint32_t)((h1 << 4) | h2);
            return 3;
        }
        case 'u': {
            uint32_t val = 0;
            for (int i = 0; i < 4; i++) {
                int hv = hex_val(src[1+i]);
                if (hv < 0) return -1;
                val = (val << 4) | hv;
            }
            *cp = val;
            return 5;
        }
        case 'U': {
            uint32_t val = 0;
            for (int i = 0; i < 8; i++) {
                int hv = hex_val(src[1+i]);
                if (hv < 0) return -1;
                val = (val << 4) | hv;
            }
            *cp = val;
            return 9;
        }
        case 'N': {
            if (src[1] != '{') return -1;
            const char* start_name = src + 2;
            const char* end = strchr(start_name, '}');
            if (!end) return -1;
            size_t name_len = (size_t)(end - start_name);
            char* name = (char*)malloc(name_len + 1);
            if (!name) return -1;
            memcpy(name, start_name, name_len);
            name[name_len] = '\0';
            for (size_t i = 0; i < name_len; i++) {
                if (!valid_name_char((unsigned char)name[i])) { free(name); return -1; }
            }
            int ok = resolve_named_char(name, cp);
            free(name);
            if (!ok) return -1;
            return (int)(end - src) + 1;
        }
        default: return -1;
    }
}

// -------- 公开接口 --------

int str2string(const char* str, nbt_Payload* out) {
    if (!str || !out) { snbt_set_error(SNBT_ERR_NULL_ARGUMENT); return 0; }

    // 跳过首尾空白
    size_t len;
    const char* p = str;
    while (*p && isspace((unsigned char)*p)) p++;
    if (!*p) { snbt_set_error(SNBT_ERR_EMPTY_STRING); return 0; }
    len = strlen(p);
    while (len > 0 && isspace((unsigned char)p[len-1])) len--;
    if (len == 0) { snbt_set_error(SNBT_ERR_EMPTY_STRING); return 0; }

    SnbtBuf sb;
    if (!snbtbuf_init(&sb, len + 16)) { snbt_set_error(SNBT_ERR_UNKNOWN); return 0; }

    char quote = 0;
    const char* end = p + len;

    if (*p == '"' || *p == '\'') {
        quote = *p;
        p++; len--;
        while (p < end && *p != quote) {
            if (*p == '\\') {
                if (p + 1 >= end) {
                    snbtbuf_putc(&sb, '\\');
                    p++; len--;
                    continue;
                }
                uint32_t cp;
                int consumed = parse_escape(p + 1, &cp);
                if (consumed < 0) {
                    snbtbuf_free(&sb);
                    snbt_set_error(SNBT_ERR_INVALID_ESCAPE);
                    return 0;
                }
                if (!encode_utf8(cp, &sb)) { snbtbuf_free(&sb); snbt_set_error(SNBT_ERR_UNKNOWN); return 0; }
                p += 1 + consumed;
                len -= (size_t)(1 + consumed);
            } else {
                if (!snbtbuf_putc(&sb, *p)) { snbtbuf_free(&sb); snbt_set_error(SNBT_ERR_UNKNOWN); return 0; }
                p++;
                len--;
            }
        }
        if (p >= end) {
            snbtbuf_free(&sb);
            snbt_set_error(SNBT_ERR_UNCLOSED_STRING);
            return 0;
        }
        p++; len--;
    } else {
        // 无引号字符串
        if (isdigit((unsigned char)*p) || *p == '.' || *p == '+' || *p == '-') {
            snbtbuf_free(&sb);
            snbt_set_error(SNBT_ERR_INVALID_PREFIX);
            return 0;
        }
        while (p < end && (isalnum((unsigned char)*p) || *p == '.' || *p == '_' || *p == '+' || *p == '-')) {
            if (!snbtbuf_putc(&sb, *p)) { snbtbuf_free(&sb); snbt_set_error(SNBT_ERR_UNKNOWN); return 0; }
            p++;
            len--;
        }
        if (snbtbuf_len(&sb) == 0) { snbtbuf_free(&sb); snbt_set_error(SNBT_ERR_EMPTY_STRING); return 0; }
        if ((snbtbuf_len(&sb) == 4 && strncasecmp((const char*)sb.data, "true", 4) == 0) ||
            (snbtbuf_len(&sb) == 5 && strncasecmp((const char*)sb.data, "false", 5) == 0)) {
            snbtbuf_free(&sb);
            snbt_set_error(SNBT_ERR_TYPE_MISMATCH);
            return 0;
        }
    }

    NBT_str* nbt_str = (NBT_str*)malloc(sizeof(NBT_str));
    if (!nbt_str) { snbtbuf_free(&sb); snbt_set_error(SNBT_ERR_UNKNOWN); return 0; }
    nbt_str->length = (int32_t)snbtbuf_len(&sb);
    nbt_str->data = snbtbuf_len(&sb) > 0 ? snbtbuf_steal(&sb) : NULL;
    if (snbtbuf_len(&sb) == 0) snbtbuf_free(&sb);
    out->ptr = nbt_str;
    snbt_set_error(SNBT_SUCCESS);
    return NBT_Str;
}

int string2str(const nbt_Payload* val, char** out) {
    if (!val || !out) return 0;
    NBT_str* s = (NBT_str*)val->ptr;
    if (!s) return 0;

    SnbtBuf sb;

    // 空字符串处理（length == 0 或 data 为 NULL）
    if (s->length == 0 || !s->data) {
        if (!snbtbuf_init(&sb, 16)) return 0;
        if (!snbtbuf_putc(&sb, '"')) { snbtbuf_free(&sb); return 0; }
        if (!snbtbuf_putc(&sb, '"')) { snbtbuf_free(&sb); return 0; }
        *out = snbtbuf_steal(&sb);
        return 1;
    }

    // 尝试无引号表示
    if (isdigit((unsigned char)s->data[0]) || s->data[0] == '.' || s->data[0] == '+' || s->data[0] == '-')
        goto quoted;
    if ((s->length == 4 && strncasecmp(s->data, "true", 4) == 0) ||
        (s->length == 5 && strncasecmp(s->data, "false", 5) == 0))
        goto quoted;
    for (int32_t i = 0; i < s->length; i++) {
        if (!(isalnum((unsigned char)s->data[i]) || s->data[i] == '.' || s->data[i] == '_' || s->data[i] == '+' || s->data[i] == '-'))
            goto quoted;
    }

    // 无引号输出
    *out = (char*)malloc(s->length + 1);
    if (!*out) return 0;
    memcpy(*out, s->data, s->length);
    (*out)[s->length] = '\0';
    return 1;

quoted:
    if (!snbtbuf_init(&sb, s->length + 16)) return 0;
    if (!snbtbuf_putc(&sb, '"')) { snbtbuf_free(&sb); return 0; }
    for (int32_t i = 0; i < s->length; i++) {
        unsigned char c = (unsigned char)s->data[i];
        switch (c) {
            case '\n': snbtbuf_append(&sb, "\\n", 2); break;
            case '\f': snbtbuf_append(&sb, "\\f", 2); break;
            case '\r': snbtbuf_append(&sb, "\\r", 2); break;
            case '\\': snbtbuf_append(&sb, "\\\\", 2); break;
            case '\b': snbtbuf_append(&sb, "\\b", 2); break;
            case '\t': snbtbuf_append(&sb, "\\t", 2); break;
            case '"':  snbtbuf_append(&sb, "\\\"", 2); break;
            default:
                if (c < 0x20 || c == 0x7F) {
                    char buf[5];
                    snprintf(buf, sizeof(buf), "\\x%02X", c);
                    snbtbuf_append(&sb, buf, strlen(buf));
                } else {
                    snbtbuf_putc(&sb, c);
                }
                break;
        }
    }
    if (!snbtbuf_putc(&sb, '"')) { snbtbuf_free(&sb); return 0; }
    *out = snbtbuf_steal(&sb);
    return 1;
}
