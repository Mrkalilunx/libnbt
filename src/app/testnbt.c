#include "nbt.h"
#include "snbterror.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static uint64_t fnv1a(const uint8_t* data, size_t len) {
    uint64_t h = 0xCBF29CE484222325ULL;
    for (size_t i = 0; i < len; i++) {
        h ^= data[i];
        h *= 0x100000001B3ULL;
    }
    return h;
}

static uint8_t* read_file(const char* path, size_t* out_len) {
    FILE* f = fopen(path, "rb");
    if (!f) return NULL;
    fseek(f, 0, SEEK_END);
    long len = ftell(f);
    if (len < 0) { fclose(f); return NULL; }
    rewind(f);
    uint8_t* data = (uint8_t*)malloc((size_t)len);
    if (!data) { fclose(f); return NULL; }
    if (fread(data, 1, (size_t)len, f) != (size_t)len) {
        free(data); fclose(f); return NULL;
    }
    fclose(f);
    *out_len = (size_t)len;
    return data;
}

int main(int argc, char** argv) {
    if (argc < 2) {
        fprintf(stderr, "Usage: %s <nbt_file> [little|big]\n", argv[0]);
        return 1;
    }

    nbt_endian endian = NBT_LITTLE_ENDIAN;
    if (argc >= 3 && strcmp(argv[2], "big") == 0)
        endian = NBT_BIG_ENDIAN;

    size_t len;
    uint8_t* data = read_file(argv[1], &len);
    if (!data) {
        fprintf(stderr, "Failed to read %s\n", argv[1]);
        return 1;
    }

    size_t offset = 0;
    if (len >= 4 && data[0] == 0x0A && data[1] == 0x00 && data[2] == 0x00 && data[3] == 0x00) {
        offset = 4;
        printf("Detected level.dat header, skipping %zu bytes\n", offset);
    }

    size_t data_len = len - offset;
    uint64_t h1 = fnv1a(data + offset, data_len);

    struct NBT_tag* root = nbt_load(data + offset, data_len, endian);
    free(data);
    if (!root) {
        fprintf(stderr, "Failed to parse NBT: %s\n", snbt_error_string(snbt_get_error()));
        return 1;
    }

    size_t out_len;
    uint8_t* result = nbt_write2(root, &out_len, endian);
    if (!result) {
        fprintf(stderr, "Failed to serialize NBT\n");
        free_tag(root);
        return 1;
    }

    FILE* f = fopen("out.nbt", "wb");
    if (!f) {
        fprintf(stderr, "Failed to write out.nbt\n");
        free(result); free_tag(root);
        return 1;
    }
    fwrite(result, 1, out_len, f);
    fclose(f);

    uint64_t h2 = fnv1a(result, out_len);
    int pass = (out_len == data_len && h1 == h2);

    printf("  original FNV-1a: %016lX (%zu bytes)\n", h1, data_len);
    printf("  out.nbt  FNV-1a: %016lX (%zu bytes)\n", h2, out_len);
    printf("%s\n", pass ? "PASS" : "FAIL");

    if (!pass && out_len != data_len)
        printf("  Size mismatch: original %zu, got %zu\n", data_len, out_len);

    free(result);
    free_tag(root);
    return pass ? 0 : 1;
}
