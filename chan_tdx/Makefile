# chan_tdx Makefile
# 交叉编译为 32 位 Windows DLL

# 编译器
CC = i686-w64-mingw32-gcc
AR = i686-w64-mingw32-ar
WINDRES = i686-w64-mingw32-windres

# 如果没有交叉编译器，回退到本机 gcc（需要 mingw 环境）
CC_FALLBACK = gcc

# 检测编译器
ifeq (, $(shell which $(CC) 2>/dev/null))
  CC = $(CC_FALLBACK)
  ifeq (, $(shell which $(CC) 2>/dev/null))
    $(error "No suitable compiler found. Install mingw-w64 or gcc with mingw support.")
  endif
endif

# 编译选项
CFLAGS = -m32 -O2 -Wall -Wextra -Wno-unused-parameter -std=c99 -D_WIN32 -DDLL_EXPORTS
LDFLAGS = -m32 -shared -Wl,--kill-at

# 输出
TARGET = chan_tdx.dll
DEF = chan_tdx.def

# 源文件和目标文件
SRCS = main.c macd.c kline.c bi.c seg.c zs.c bsp.c
OBJS = $(SRCS:.c=.o)
HEADERS = chan_tdx.h macd.h kline.h bi.h seg.h zs.h bsp.h

# 默认目标
all: $(TARGET)

# 链接
$(TARGET): $(OBJS) $(DEF)
	$(CC) $(LDFLAGS) -o $@ $(OBJS) $(DEF)

# 编译
%.o: %.c $(HEADERS)
	$(CC) $(CFLAGS) -c $< -o $@

# 清理
clean:
	rm -f $(OBJS) $(TARGET)

# 安装到TDX目录（需要设置 TDX_DIR）
install: $(TARGET)
ifdef TDX_DIR
	cp $(TARGET) "$(TDX_DIR)/T0002/dllnew/"
	@echo "Installed to $(TDX_DIR)/T0002/dllnew/"
else
	@echo "Set TDX_DIR to install, e.g.: make install TDX_DIR='C:/new_tdx'"
endif

.PHONY: all clean install
