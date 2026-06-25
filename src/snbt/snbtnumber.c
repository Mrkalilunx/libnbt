#include "snbtnumber.h"
#include <stdio.h>
#include <stdlib.h>
#include <inttypes.h>
#include <ctype.h>
#include <float.h>
#include <errno.h>
#include <math.h>
#include <stddef.h>

#define VALID_BASE2   (1 << 0)
#define VALID_BASE10  (1 << 1)
#define VALID_BASE16  (1 << 2)

static const unsigned char char_valid_mask[256] = {
    ['_'] = VALID_BASE2 | VALID_BASE10 | VALID_BASE16,
    ['0'] = VALID_BASE2 | VALID_BASE10 | VALID_BASE16,
    ['1'] = VALID_BASE2 | VALID_BASE10 | VALID_BASE16,
    ['2'] = VALID_BASE10 | VALID_BASE16,
    ['3'] = VALID_BASE10 | VALID_BASE16,
    ['4'] = VALID_BASE10 | VALID_BASE16,
    ['5'] = VALID_BASE10 | VALID_BASE16,
    ['6'] = VALID_BASE10 | VALID_BASE16,
    ['7'] = VALID_BASE10 | VALID_BASE16,
    ['8'] = VALID_BASE10 | VALID_BASE16,
    ['9'] = VALID_BASE10 | VALID_BASE16,
    ['a'] = VALID_BASE16,
    ['b'] = VALID_BASE16,
    ['c'] = VALID_BASE16,
    ['d'] = VALID_BASE16,
    ['e'] = VALID_BASE16,
    ['f'] = VALID_BASE16,
    ['A'] = VALID_BASE16,
    ['B'] = VALID_BASE16,
    ['C'] = VALID_BASE16,
    ['D'] = VALID_BASE16,
    ['E'] = VALID_BASE16,
    ['F'] = VALID_BASE16,
};

// -------- 辅助函数 --------

static const char* trim(const char* str, size_t* len) {
    while (*str && isspace((unsigned char)*str)) str++;
    const char* end = str + strlen(str);
    while (end > str && isspace((unsigned char)*(end - 1))) end--;
    *len = (size_t)(end - str);
    return str;
}

static int detnumber(int c, int base) {
    unsigned char idx = (unsigned char)c;
    unsigned char mask = char_valid_mask[idx];
    switch (base) {
        case 2:  return (mask & VALID_BASE2) != 0;
        case 10: return (mask & VALID_BASE10) != 0;
        case 16: return (mask & VALID_BASE16) != 0;
        default: return 0;
    }
}

static char* setunderscore(const char* str, size_t len, int base) {
    if (!str) return NULL;

    char* result = (char*)malloc(len + 1);
    if (!result) return NULL;

    size_t i = 0;
    size_t j = 0;

    while (i < len) {
        const char* p = memchr(str + i, '_', len - i);
        if (!p) {
            size_t remaining = len - i;
            memcpy(result + j, str + i, remaining);
            j += remaining;
            break;
        }

        size_t pos = p - str;

        size_t copy_len = pos - i;
        if (copy_len > 0) {
            memcpy(result + j, str + i, copy_len);
            j += copy_len;
        }

        if (pos == 0 || pos + 1 >= len ||
            !detnumber((unsigned char)str[pos - 1], base) ||
            !detnumber((unsigned char)str[pos + 1], base)) {
            free(result);
            return NULL;
        }

        i = pos + 1;
    }

    result[j] = '\0';
    return result;
}

static int parse_unsigned(const char* str, int base, uint64_t* out) {
    if (!str || !*str) return 0;
    if (base == 10 && str[0] == '0' && str[1] != '\0') return 0;
    char* endptr;
    errno = 0;
    unsigned long long val = strtoull(str, &endptr, base);
    if (errno == ERANGE || endptr == str || *endptr != '\0') return 0;
    *out = (uint64_t)val;
    return 1;
}

static int detinteger_prefix(const char* str, size_t len, int* sign, int* base) {
    *sign = 0;
    *base = 10;
    if (!str || len == 0) return 0;
    const char* p = str;
    size_t n = len;
    if (n > 0 && (*p == '+' || *p == '-')) {
        *sign = (*p == '+') ? 1 : -1;
        p++; n--;
    }
    if (n >= 2 && p[0] == '0' && tolower((unsigned char)p[1]) == 'x') {
        *base = 16;
        return 1;
    }
    if (n >= 2 && p[0] == '0' && tolower((unsigned char)p[1]) == 'b') {
        *base = 2;
        return 1;
    }
    return 1;
}

static int detinteger_suffix(const char* str, size_t len, size_t* num_len, uint8_t* type_suffix, int* is_signed) {
    if (!str || !num_len || !type_suffix || !is_signed) return 0;
    *type_suffix = 0;
    *is_signed = -1;
    if (len == 0) { *num_len = 0; return 0; }

    if (len >= 2) {
        char prev = str[len-2];
        char last = str[len-1];
        char lower_prev = tolower((unsigned char)prev);
        char lower_last = tolower((unsigned char)last);
        if ((lower_prev == 's' || lower_prev == 'u') &&
            (lower_last == 'b' || lower_last == 's' || lower_last == 'i' || lower_last == 'l')) {
            *is_signed = (lower_prev == 's') ? 1 : 0;
            switch (lower_last) {
                case 'b': *type_suffix = NBT_Byte;  break;
                case 's': *type_suffix = NBT_Short; break;
                case 'i': *type_suffix = NBT_Int;   break;
                case 'l': *type_suffix = NBT_Long;  break;
            }
            *num_len = len - 2;
            return 1;
        }
    }

    char last = str[len-1];
    char lower_last = tolower((unsigned char)last);
    if (lower_last == 'b' || lower_last == 's' || lower_last == 'i' || lower_last == 'l') {
        switch (lower_last) {
            case 'b': *type_suffix = NBT_Byte;  break;
            case 's': *type_suffix = NBT_Short; break;
            case 'i': *type_suffix = NBT_Int;   break;
            case 'l': *type_suffix = NBT_Long;  break;
        }
        if (len >= 2) {
            char prev = str[len-2];
            char lower_prev = tolower((unsigned char)prev);
            if (lower_prev == 's' || lower_prev == 'u') {
                *is_signed = (lower_prev == 's') ? 1 : 0;
                *num_len = len - 2;
                return 1;
            }
        }
        *num_len = len - 1;
        return 1;
    }

    *num_len = len;
    return 1;
}

// 检测浮点数前缀：浮点数不允许十六进制或二进制前缀，合法时返回 1，否则 0
static int detfloat_prefix(const char* str, size_t len) {
    if (!str) return 0;
    if (len >= 2 && str[0] == '0' && (tolower((unsigned char)str[1]) == 'x' || tolower((unsigned char)str[1]) == 'b'))
        return 0;
    return 1;
}

// 检测浮点数后缀（'f' 或 'd'），返回类型 NBT_Float 或 NBT_Double，0 表示无后缀
static uint8_t detfloat_suffix(const char* str, size_t len, size_t* num_len) {
    if (len >= 1) {
        char last = tolower((unsigned char)str[len-1]);
        if (last == 'f' || last == 'd') {
            *num_len = len - 1;
            return (last == 'f') ? NBT_Float : NBT_Double;
        }
    }
    *num_len = len;
    return 0;
}

#define PAYLOAD_STORE(type, val_expr) do { \
    switch (type) { \
        case NBT_Byte:   out->b = (int8_t)(val_expr); break; \
        case NBT_Short:  out->s = (int16_t)(val_expr); break; \
        case NBT_Int:    out->i = (int32_t)(val_expr); break; \
        case NBT_Long:   out->l = (int64_t)(val_expr); break; \
        default: snbt_set_error(SNBT_ERR_UNKNOWN); return 0; \
    } \
} while(0)

static int parse_integer_core(const char* num_str, int base, uint8_t type, char sign_suffix, nbt_Payload* out) {
    int is_hex_or_bin = (base != 10);
    const char* p = num_str;
    int neg = 0;
    if (*p == '-') { neg = 1; p++; }
    else if (*p == '+') { p++; }

    uint64_t abs_val;
    if (!parse_unsigned(p, base, &abs_val)) {
        snbt_set_error(SNBT_ERR_UNKNOWN);
        return 0;
    }

    int is_signed;
    if (sign_suffix == 's') is_signed = 1;
    else if (sign_suffix == 'u') is_signed = 0;
    else is_signed = !is_hex_or_bin;

    if (neg && !is_signed) {
        snbt_set_error(SNBT_ERR_UNSIGNED_NEGATIVE);
        return 0;
    }

    int max_bits = 0;
    switch (type) {
        case NBT_Byte:   max_bits = 8; break;
        case NBT_Short:  max_bits = 16; break;
        case NBT_Int:    max_bits = 32; break;
        case NBT_Long:   max_bits = 64; break;
        default:
            snbt_set_error(SNBT_ERR_UNKNOWN);
            return 0;
    }
    uint64_t max_unsigned = (max_bits == 64) ? UINT64_MAX : ((1ULL << max_bits) - 1);
    int64_t min_signed = (max_bits == 64) ? INT64_MIN : (-(1LL << (max_bits - 1)));
    int64_t max_signed = (max_bits == 64) ? INT64_MAX : ((1LL << (max_bits - 1)) - 1);

    if (is_signed) {
        if (neg) {
            if (max_bits == 64) {
                if (abs_val > (uint64_t)INT64_MAX + 1) { snbt_set_error(SNBT_ERR_OUT_OF_RANGE); return 0; }
            } else {
                if (abs_val > (uint64_t)(-min_signed)) { snbt_set_error(SNBT_ERR_OUT_OF_RANGE); return 0; }
            }
            int64_t val = -(int64_t)abs_val;
            if (val < min_signed) { snbt_set_error(SNBT_ERR_OUT_OF_RANGE); return 0; }
            PAYLOAD_STORE(type, val);
            return 1;
        } else {
            if (abs_val > (uint64_t)max_signed) { snbt_set_error(SNBT_ERR_OUT_OF_RANGE); return 0; }
            PAYLOAD_STORE(type, (int64_t)abs_val);
            return 1;
        }
    } else {
        if (abs_val > max_unsigned) { snbt_set_error(SNBT_ERR_OUT_OF_RANGE); return 0; }
        PAYLOAD_STORE(type, abs_val);
        return 1;
    }
}

// -------- 公开接口 --------

int defaultstr2int(const char* str, uint8_t default_type, nbt_Payload* out) {
    if (!str || !out) { snbt_set_error(SNBT_ERR_NULL_ARGUMENT); return 0; }

    size_t len;
    const char* trimmed = trim(str, &len);
    if (len == 0) { snbt_set_error(SNBT_ERR_EMPTY_STRING); return 0; }
    if (trimmed[0] == '_' || trimmed[len-1] == '_') { snbt_set_error(SNBT_ERR_UNDERSCORE_AT_ENDS); return 0; }

    if (len == 4 && (strncasecmp(trimmed, "true", 4) == 0)) {
        out->b = 1;
        snbt_set_error(SNBT_SUCCESS);
        return NBT_Byte;
    }
    if (len == 5 && (strncasecmp(trimmed, "false", 5) == 0)) {
        out->b = 0;
        snbt_set_error(SNBT_SUCCESS);
        return NBT_Byte;
    }

    uint8_t suffix_type = 0;
    int is_signed = -1;
    size_t num_part_len;
    if (!detinteger_suffix(trimmed, len, &num_part_len, &suffix_type, &is_signed)) {
        snbt_set_error(SNBT_ERR_INVALID_SUFFIX);
        return 0;
    }

    char* num_part = (char*)malloc(num_part_len + 1);
    if (!num_part) { snbt_set_error(SNBT_ERR_UNKNOWN); return 0; }
    memcpy(num_part, trimmed, num_part_len);
    num_part[num_part_len] = '\0';

    int sign = 0, base = 10;
    const char* body; size_t body_len;

parse_body:
    detinteger_prefix(num_part, num_part_len, &sign, &base);
    body = num_part;
    body_len = num_part_len;
    if (sign != 0) { body++; body_len--; }
    if (base == 16 || base == 2) { body += 2; body_len -= 2; }

    // 0x...b 歧义：十六进制下末尾 b 无 u/s 前缀时视为数字而非 Byte 后缀
    if (suffix_type == NBT_Byte && is_signed == -1 && base == 16) {
        free(num_part);
        suffix_type = 0;
        num_part_len = len;
        num_part = (char*)malloc(num_part_len + 1);
        if (!num_part) { snbt_set_error(SNBT_ERR_UNKNOWN); return 0; }
        memcpy(num_part, trimmed, num_part_len);
        num_part[num_part_len] = '\0';
        goto parse_body;
    }

    if (body_len == 0) {
        free(num_part);
        snbt_set_error(SNBT_ERR_INVALID_PREFIX);
        return 0;
    }

    for (size_t i = 0; i < body_len; i++) {
        if (!detnumber((unsigned char)body[i], base)) {
            free(num_part);
            snbt_set_error(SNBT_ERR_INVALID_PREFIX);
            return 0;
        }
    }

    // 确定目标类型
    uint8_t target_type;
    if (suffix_type != 0) {
        target_type = suffix_type;
    } else if (default_type >= NBT_Byte && default_type <= NBT_Long) {
        target_type = default_type;
    } else {
        target_type = NBT_Int;
    }

    char* clean_digits = setunderscore(body, body_len, base);
    free(num_part);
    if (!clean_digits) {
        snbt_set_error(SNBT_ERR_INVALID_UNDERSCORE);
        return 0;
    }

    char* final_num = NULL;
    if (sign == -1) {
        size_t dlen = strlen(clean_digits);
        final_num = (char*)malloc(dlen + 2);
        if (!final_num) { free(clean_digits); snbt_set_error(SNBT_ERR_UNKNOWN); return 0; }
        final_num[0] = '-';
        memcpy(final_num + 1, clean_digits, dlen + 1);
        free(clean_digits);
    } else {
        final_num = clean_digits;
    }

    if (base != 10 && strchr(final_num, '.')) {
        free(final_num);
        snbt_set_error(SNBT_ERR_INVALID_PREFIX);
        return 0;
    }

    char sign_suffix = 0;
    if (is_signed == 1) sign_suffix = 's';
    else if (is_signed == 0) sign_suffix = 'u';

    int ret = parse_integer_core(final_num, base, target_type, sign_suffix, out);
    free(final_num);
    if (ret) {
        snbt_set_error(SNBT_SUCCESS);
        return target_type;
    }
    return 0;
}

int str2integer(const char* str, nbt_Payload* out) {
    return defaultstr2int(str, NBT_Int, out);
}

int str2float(const char* str, nbt_Payload* out) {
    if (!str || !out) { snbt_set_error(SNBT_ERR_NULL_ARGUMENT); return 0; }

    size_t len;
    const char* trimmed = trim(str, &len);
    if (len == 0) { snbt_set_error(SNBT_ERR_EMPTY_STRING); return 0; }
    if (trimmed[0] == '_' || trimmed[len-1] == '_') { snbt_set_error(SNBT_ERR_UNDERSCORE_AT_ENDS); return 0; }

    if (!detfloat_prefix(trimmed, len)) {
        snbt_set_error(SNBT_ERR_FLOAT_HEX_PREFIX);
        return 0;
    }

    size_t num_part_len;
    uint8_t suffix_type = detfloat_suffix(trimmed, len, &num_part_len);
    uint8_t target_type = (suffix_type == 0) ? NBT_Double : suffix_type;

    char* num_part = (char*)malloc(num_part_len + 1);
    if (!num_part) { snbt_set_error(SNBT_ERR_UNKNOWN); return 0; }
    memcpy(num_part, trimmed, num_part_len);
    num_part[num_part_len] = '\0';

    char* clean_num = setunderscore(num_part, num_part_len, 10);
    free(num_part);
    if (!clean_num) {
        snbt_set_error(SNBT_ERR_INVALID_UNDERSCORE);
        return 0;
    }

    if (!strchr(clean_num, '.') && !strchr(clean_num, 'e') && !strchr(clean_num, 'E')) {
        free(clean_num);
        snbt_set_error(SNBT_ERR_FLOAT_PARSE_FAIL);
        return 0;
    }

    char* endptr;
    errno = 0;
    double val = strtod(clean_num, &endptr);
    if (errno == ERANGE) {
        if (val == HUGE_VAL || val == -HUGE_VAL) {
            free(clean_num);
            snbt_set_error(SNBT_ERR_FLOAT_OVERFLOW);
            return 0;
        }
        val = 0.0;
    }
    if (endptr == clean_num || *endptr != '\0') {
        free(clean_num);
        snbt_set_error(SNBT_ERR_FLOAT_PARSE_FAIL);
        return 0;
    }
    if (isnan(val) || isinf(val)) {
        free(clean_num);
        snbt_set_error(SNBT_ERR_FLOAT_NAN_INF);
        return 0;
    }

    // 范围检查
    if (target_type == NBT_Float) {
        float f = (float)val;
        if (isinf(f)) {
            free(clean_num);
            snbt_set_error(SNBT_ERR_OUT_OF_RANGE);
            return 0;
        }
        out->f = f;
    } else {
        if (val > DBL_MAX || val < -DBL_MAX) {
            free(clean_num);
            snbt_set_error(SNBT_ERR_OUT_OF_RANGE);
            return 0;
        }
        out->d = val;
    }
    free(clean_num);
    snbt_set_error(SNBT_SUCCESS);
    return target_type;
}

int integer2str(uint8_t type, const nbt_Payload* val, char** out, const SnbtNumberStyle* style) {
    if (!val || !out || !style) return 0;

    int64_t v;
    switch (type) {
        case NBT_Byte:  v = (int64_t)val->b; break;
        case NBT_Short: v = (int64_t)val->s; break;
        case NBT_Int:   v = (int64_t)val->i; break;
        case NBT_Long:  v = val->l;          break;
        default: return 0;
    }

    char suffix = 0;
    switch (type) {
        case NBT_Byte:  suffix = 'b'; break;
        case NBT_Short: suffix = 's'; break;
        case NBT_Long:  suffix = 'l'; break;
        default: break;
    }
    if (type == NBT_Int && !style->show_int_suffix) suffix = 0;
    if (suffix && style->uppercase) suffix = (char)toupper((unsigned char)suffix);

    int need;
    if (suffix)
        need = snprintf(NULL, 0, "%" PRId64 "%c", v, suffix);
    else
        need = snprintf(NULL, 0, "%" PRId64, v);
    if (need < 0) return 0;

    char* buf = (char*)malloc((size_t)need + 1);
    if (!buf) return 0;

    int written;
    if (suffix)
        written = snprintf(buf, (size_t)need + 1, "%" PRId64 "%c", v, suffix);
    else
        written = snprintf(buf, (size_t)need + 1, "%" PRId64, v);
    if (written < 0 || written != need) { free(buf); return 0; }

    *out = buf;
    return 1;
}

int float2str(uint8_t type, const nbt_Payload* val, char** out, const SnbtNumberStyle* style) {
    if (!val || !out || !style) return 0;

    double v;
    if (type == NBT_Float) v = (double)val->f;
    else if (type == NBT_Double) v = val->d;
    else return 0;

    char suffix = (type == NBT_Float) ? 'f' : 0;
    if (suffix && style->uppercase) suffix = (char)toupper((unsigned char)suffix);

    int precision = (type == NBT_Float) ? FLT_DECIMAL_DIG : DBL_DECIMAL_DIG;
    int need = snprintf(NULL, 0, "%.*g", precision, v);
    if (need < 0) return 0;

    char* num_str = (char*)malloc((size_t)need + 1);
    if (!num_str) return 0;
    snprintf(num_str, (size_t)need + 1, "%.*g", precision, v);

    int has_dot_or_e = 0;
    for (char* p = num_str; *p; p++) {
        if (*p == '.' || tolower((unsigned char)*p) == 'e') { has_dot_or_e = 1; break; }
    }

    char* formatted;
    if (!has_dot_or_e) {
        formatted = (char*)malloc((size_t)need + 2 + 1);
        if (!formatted) { free(num_str); return 0; }
        sprintf(formatted, "%s.0", num_str);
        free(num_str);
    } else {
        formatted = num_str;
    }

    if (suffix) {
        size_t flen = strlen(formatted);
        char* with_suffix = (char*)malloc(flen + 2);
        if (!with_suffix) { free(formatted); return 0; }
        sprintf(with_suffix, "%s%c", formatted, suffix);
        free(formatted);
        *out = with_suffix;
    } else {
        *out = formatted;
    }
    return 1;
}