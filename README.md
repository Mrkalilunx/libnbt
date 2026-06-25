# libnbt

Minecraft NBT / SNBT 的 C 语言解析与序列化库。

## 简介

libnbt 提供完整的 NBT（Named Binary Tag）和 SNBT（Stringified NBT）处理能力，支持 Java Edition（大端）和 Bedrock Edition（小端）格式。核心是一个递归标签树（`struct NBT_tag`），包含读写、解析、序列化的完整 API。

## 核心 API

### NBT 读写

```c
// 从字节流加载 NBT，endian 控制字节序
struct NBT_tag* nbt_load(const uint8_t* data, size_t len, nbt_endian endian);

// 序列化为字节流（动态缓冲）
uint8_t* nbt_write(const struct NBT_tag* root, size_t* out_len, nbt_endian endian);

// 序列化为字节流（预计算大小，一次分配，更高效）
uint8_t* nbt_write2(const struct NBT_tag* root, size_t* out_len, nbt_endian endian);

// 递归释放标签树
void free_tag(struct NBT_tag* tag);
```

### SNBT 解析与序列化

```c
// 从 SNBT 字符串解析
struct NBT_tag* snbt_load(const char* str);

// 序列化为 SNBT 字符串（pretty=1 美化输出）
char* snbt_write(const struct NBT_tag* tag, int pretty);

// 流式写入文件
int snbt_write_stream(const struct NBT_tag* tag, int pretty, FILE* fp);
```

### 文件 I/O

```c
// 纯 NBT 文件
struct NBT_tag* fprnbt(const char* filename, nbt_endian endian);
int fpwnbt(const char* filename, const struct NBT_tag* root, nbt_endian endian);

// level.dat（自动处理 4 字节头部）
struct NBT_tag* fprlevdat(const char* filename, nbt_endian endian);
int fpwlevdat(const char* filename, const struct NBT_tag* root, nbt_endian endian);

// SNBT 文件
struct NBT_tag* fprsnbt(const char* filename);
int fpwsnbt(const char* filename, const struct NBT_tag* root, int pretty);
```

所有文件 API 均有 `mm*`（mmap）变体用于大文件高效处理。

## 构建

```sh
make            # 编译库 + CLI 工具 ./nbt
make clean
```

生成的 `nbt` 是附带的命令行工具，库通过直接链接 `.o` 目标文件使用。

## 项目结构

```
include/        — 全部头文件
  nbt.h           NBT 标签树定义与核心 API
  nbtbuffer.h     序列化缓冲区
  nbtendian.h     字节序工具
  nbtfile.h       NBT 文件读写
  snbt.h          SNBT 序列化与解析
  snbtfile.h      SNBT 文件读写
  snbterror.h     错误处理
  snbtarray.h     TAG_*_Array 处理
  snbtlist.h      TAG_List 处理
  snbtcompound.h  TAG_Compound 处理
  snbtstring.h    字符串转义
  snbtnumber.h    数字格式
src/nbt/        — NBT 核心实现
src/snbt/       — SNBT 核心实现
src/app/        — CLI 示例入口（main.c）和测试工具（testnbt.c）
```

## 示例

```c
#include "nbtfile.h"
#include "snbt.h"
#include <stdio.h>

int main() {
    // 读取 level.dat
    struct NBT_tag* root = fprlevdat("level.dat", NBT_BIG_ENDIAN);
    if (!root) return 1;

    // 转 SNBT 并打印
    char* snbt = snbt_write(root, 1);
    if (snbt) {
        printf("%s\n", snbt);
        free(snbt);
    }

    // 写回 NBT
    fpwlevdat("output.dat", root, NBT_BIG_ENDIAN);

    free_tag(root);
    return 0;
}
```

## 特性

- 支持全部 13 种 NBT 标签类型（含 Short_Array 扩展）
- 大端/小端自适应，匹配时批量 `memcpy` 零开销
- `nbt_write2` 预计算大小，零重分配
- mmap 支持大文件零拷贝解析
- `level.dat` 头部自动处理
- 递归深度上限（1024）防止栈溢出
- 安全算术防止整数溢出

## 许可

AGPL-3.0
