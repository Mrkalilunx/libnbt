#include "nbtfile.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define LEVELDAT_HEADER_SIZE 4

// 仅在 POSIX 系统下包含 mmap 相关头文件
#if defined(__unix__) || defined(__linux__) || defined(__APPLE__)
    #include <sys/mman.h>
    #include <sys/stat.h>
    #include <fcntl.h>
    #include <unistd.h>
    #define HAS_MMAP 1
#else
    #define HAS_MMAP 0
#endif

// 使用标准 C 文件 I/O 读取整个文件到堆内存，成功返回缓冲区指针并在 len 中返回大小，失败返回 NULL
static uint8_t* read_file_fp(const char* filename, size_t* len) {
    FILE* fp = fopen(filename, "rb");
    if (!fp) return NULL;
    fseek(fp, 0, SEEK_END);
    long file_size = ftell(fp);
    if (file_size < 0) { fclose(fp); return NULL; }
    *len = (size_t)file_size;
    rewind(fp);
    uint8_t* data = (uint8_t*)malloc(*len);
    if (!data) { fclose(fp); return NULL; }
    if (fread(data, 1, *len, fp) != *len) {
        free(data);
        fclose(fp);
        return NULL;
    }
    fclose(fp);
    return data;
}

// 使用标准 C 文件 I/O 将字节缓冲区写入文件，成功返回 1，失败返回 0
static int write_file_fp(const char* filename, const uint8_t* data, size_t len) {
    FILE* fp = fopen(filename, "wb");
    if (!fp) return 0;
    size_t written = fwrite(data, 1, len, fp);
    fclose(fp);
    return written == len ? 1 : 0;
}

// 直接内存映射文件（POSIX）或回退到标准 I/O
#if HAS_MMAP
static void* mmap_file(const char* filename, size_t* out_len) {
    int fd = open(filename, O_RDONLY);
    if (fd < 0) return NULL;
    struct stat st;
    if (fstat(fd, &st) < 0) { close(fd); return NULL; }
    *out_len = (size_t)st.st_size;
    void* map = mmap(NULL, *out_len, PROT_READ, MAP_PRIVATE, fd, 0);
    close(fd);
    return (map == MAP_FAILED) ? NULL : map;
}
static void unmap_file(void* addr, size_t len) { munmap(addr, len); }
#else
static void* mmap_file(const char* filename, size_t* out_len) {
    return read_file_fp(filename, out_len);
}
static void unmap_file(void* addr, size_t len) { free(addr); }
#endif

static int has_levdat_header(const uint8_t* d, size_t len) {
    return len >= LEVELDAT_HEADER_SIZE
        && d[0] == 0x0A && d[1] == 0x00 && d[2] == 0x00 && d[3] == 0x00;
}

// 读取 level.dat 文件（带头部），按指定字节序解析，成功返回根标签，失败返回 NULL
struct NBT_tag* fprlevdat(const char* filename, nbt_endian endian) {
    size_t len;
    uint8_t* data = read_file_fp(filename, &len);
    if (!data) return NULL;
    if (!has_levdat_header(data, len)) { free(data); return NULL; }
    struct NBT_tag* root = nbt_load(data + LEVELDAT_HEADER_SIZE, len - LEVELDAT_HEADER_SIZE, endian);
    free(data);
    return root;
}

// 写入 level.dat 文件（带头部），按指定字节序序列化，成功返回 1，失败返回 0
int fpwlevdat(const char* filename, const struct NBT_tag* root, nbt_endian endian) {
    if (!root) return 0;
    size_t nbt_len;
    uint8_t* nbt_data = nbt_write2(root, &nbt_len, endian);
    if (!nbt_data) return 0;
    uint8_t header[LEVELDAT_HEADER_SIZE] = {0x0A, 0x00, 0x00, 0x00};
    FILE* fp = fopen(filename, "wb");
    if (!fp) { free(nbt_data); return 0; }
    int ok = fwrite(header, 1, LEVELDAT_HEADER_SIZE, fp) == LEVELDAT_HEADER_SIZE
          && fwrite(nbt_data, 1, nbt_len, fp) == nbt_len;
    fclose(fp);
    free(nbt_data);
    return ok;
}

// 读取纯 NBT 文件（无头部），按指定字节序解析，成功返回根标签，失败返回 NULL
struct NBT_tag* fprnbt(const char* filename, nbt_endian endian) {
    size_t len;
    uint8_t* data = read_file_fp(filename, &len);
    if (!data) return NULL;
    struct NBT_tag* root = nbt_load(data, len, endian);
    free(data);
    return root;
}

// 写入纯 NBT 文件（无头部），按指定字节序序列化，成功返回 1，失败返回 0
int fpwnbt(const char* filename, const struct NBT_tag* root, nbt_endian endian) {
    if (!root) return 0;
    size_t len;
    uint8_t* data = nbt_write2(root, &len, endian);
    if (!data) return 0;
    int result = write_file_fp(filename, data, len);
    free(data);
    return result;
}

// 读取 level.dat 文件（带头部），通过内存映射优化读取，成功返回根标签，失败返回 NULL
struct NBT_tag* mmrlevdat(const char* filename, nbt_endian endian) {
    size_t len;
    uint8_t* data = (uint8_t*)mmap_file(filename, &len);
    if (!data) return NULL;
    if (!has_levdat_header(data, len)) { unmap_file(data, len); return NULL; }
    struct NBT_tag* root = nbt_load(data + LEVELDAT_HEADER_SIZE, len - LEVELDAT_HEADER_SIZE, endian);
    unmap_file(data, len);
    return root;
}

// 写入 level.dat 文件（带头部），实际使用标准文件写入，成功返回 1，失败返回 0
int mmwlevdat(const char* filename, const struct NBT_tag* root, nbt_endian endian) {
    return fpwlevdat(filename, root, endian);
}

// 读取纯 NBT 文件（无头部），通过内存映射优化读取，成功返回根标签，失败返回 NULL
struct NBT_tag* mmrnbt(const char* filename, nbt_endian endian) {
    size_t len;
    uint8_t* data = (uint8_t*)mmap_file(filename, &len);
    if (!data) return NULL;
    struct NBT_tag* root = nbt_load(data, len, endian);
    unmap_file(data, len);
    return root;
}

// 写入纯 NBT 文件（无头部），实际使用标准文件写入，成功返回 1，失败返回 0
int mmwnbt(const char* filename, const struct NBT_tag* root, nbt_endian endian) {
    return fpwnbt(filename, root, endian);
}