# Makefile for NBT/SNBT Tool

CC       = gcc
CFLAGS   = -Iinclude -Wall -Wextra -O2
LDFLAGS  = -lm
TARGET   = nbt

# 自动搜集所有源文件（包含 main.c）
SRCS = $(wildcard src/nbt/*.c) $(wildcard src/snbt/*.c) src/app/main.c
OBJS     = $(SRCS:.c=.o)

# 默认目标
all: $(TARGET)

# 链接
$(TARGET): $(OBJS)
	$(CC) $^ $(LDFLAGS) -o $@

# 编译规则
%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

# 清理
clean:
	rm -f $(OBJS) $(TARGET)

# 深度清理（包括潜在备份文件）
distclean: clean
	rm -f *~ src/*/*~

# 安装到 /usr/local/bin（可选）
install: $(TARGET)
	install -m 755 $(TARGET) /usr/local/bin

# testnbt — NBT 往返测试工具
testnbt_SRC = src/app/testnbt.c
testnbt_OBJ = src/app/testnbt.o
# 重用除 main.o 以外的所有目标文件
testnbt_DEPS = $(filter-out src/app/main.o, $(OBJS))
testnbt: $(testnbt_DEPS) $(testnbt_OBJ)
	$(CC) $^ $(LDFLAGS) -o $@

.PHONY: all clean distclean install testnbt