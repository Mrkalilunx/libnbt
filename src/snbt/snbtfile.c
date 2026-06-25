// snbtfile.c（更新 fpwsnbt 和 mmwsnbt 使用流式写入）
#include "snbtfile.h"
#include "snbt.h"
#include "snbterror.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// 平台检测
#if defined(__unix__) || defined(__linux__) || defined(__APPLE__)
    #include <sys/mman.h>
    #include <sys/stat.h>
    #include <fcntl.h>
    #include <unistd.h>
    #define HAS_MMAP 1
#else
    #define HAS_MMAP 0
#endif

// 标准文件读取
static char* read_file_text(const char* filename, size_t* out_len) {
    FILE* fp = fopen(filename, "rb");
    if (!fp) return NULL;
    fseek(fp, 0, SEEK_END);
    long file_size = ftell(fp);
    if (file_size < 0) { fclose(fp); return NULL; }
    rewind(fp);
    char* buf = (char*)malloc(file_size + 1);
    if (!buf) { fclose(fp); return NULL; }
    size_t read_bytes = fread(buf, 1, file_size, fp);
    fclose(fp);
    if (read_bytes != (size_t)file_size) {
        free(buf);
        return NULL;
    }
    buf[read_bytes] = '\0';
    if (out_len) *out_len = read_bytes;
    return buf;
}

// 内存映射读取
static char* read_file_mm_text(const char* filename, size_t* out_len) {
#if HAS_MMAP
    int fd = open(filename, O_RDONLY);
    if (fd < 0) return NULL;
    struct stat st;
    if (fstat(fd, &st) < 0) { close(fd); return NULL; }
    size_t fsize = (size_t)st.st_size;
    void* map = mmap(NULL, fsize, PROT_READ, MAP_PRIVATE, fd, 0);
    close(fd);
    if (map == MAP_FAILED) return NULL;
    char* data = (char*)malloc(fsize + 1);
    if (!data) { munmap(map, fsize); return NULL; }
    memcpy(data, map, fsize);
    munmap(map, fsize);
    data[fsize] = '\0';
    if (out_len) *out_len = fsize;
    return data;
#else
    return read_file_text(filename, out_len);
#endif
}

struct NBT_tag* fprsnbt(const char* filename) {
    size_t len;
    char* text = read_file_text(filename, &len);
    if (!text) { snbt_set_error(SNBT_ERR_UNKNOWN); return NULL; }
    struct NBT_tag* root = snbt_load(text);
    free(text);
    return root;
}

int fpwsnbt(const char* filename, const struct NBT_tag* root, int pretty) {
    FILE* fp = fopen(filename, "wb");
    if (!fp) { snbt_set_error(SNBT_ERR_UNKNOWN); return 0; }
    int ret = snbt_write_stream(root, pretty, fp);
    fclose(fp);
    return ret;
}

struct NBT_tag* mmrsnbt(const char* filename) {
    size_t len;
    char* text = read_file_mm_text(filename, &len);
    if (!text) { snbt_set_error(SNBT_ERR_UNKNOWN); return NULL; }
    struct NBT_tag* root = snbt_load(text);
    free(text);
    return root;
}

int mmwsnbt(const char* filename, const struct NBT_tag* root, int pretty) {
    return fpwsnbt(filename, root, pretty);
}