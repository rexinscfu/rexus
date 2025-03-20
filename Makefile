TARGET = rexus.bin
CC = gcc
CXX = g++
AS = nasm
LD = ld

KERNEL_ADDR = 0x100000

CFLAGS = -m32 -std=c11 -ffreestanding -O2 -Wall -Wextra -Werror -fno-exceptions -fno-stack-protector \
         -fno-pie -mno-mmx -mno-sse -mno-sse2
CXXFLAGS = -m32 -std=c++17 -ffreestanding -O2 -Wall -Wextra -Werror -fno-exceptions -fno-rtti \
           -fno-stack-protector -fno-pie -mno-mmx -mno-sse -mno-sse2 -fno-use-cxa-atexit
ASFLAGS = -f elf32
LDFLAGS = -m elf_i386 -T link.ld -nostdlib --no-undefined

OBJ_DIR = build
SRC_DIRS = arch core drivers fs mem proc net

C_SRCS = $(shell find $(SRC_DIRS) -name '*.c')
CXX_SRCS = $(shell find $(SRC_DIRS) -name '*.cpp')
ASM_SRCS = $(shell find $(SRC_DIRS) -name '*.asm')

C_OBJS = $(patsubst %.c, $(OBJ_DIR)/%.o, $(C_SRCS))
CXX_OBJS = $(patsubst %.cpp, $(OBJ_DIR)/%.o, $(CXX_SRCS))
ASM_OBJS = $(patsubst %.asm, $(OBJ_DIR)/%.o, $(ASM_SRCS))

OBJS = $(C_OBJS) $(CXX_OBJS) $(ASM_OBJS)

.PHONY: all clean dirs

all: dirs $(TARGET)

$(TARGET): $(OBJS)
	$(LD) $(LDFLAGS) -o $@ $^

$(OBJ_DIR)/%.o: %.c
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -c $< -o $@

$(OBJ_DIR)/%.o: %.cpp
	@mkdir -p $(dir $@)
	$(CXX) $(CXXFLAGS) -c $< -o $@

$(OBJ_DIR)/%.o: %.asm
	@mkdir -p $(dir $@)
	$(AS) $(ASFLAGS) $< -o $@

dirs:
	@mkdir -p $(OBJ_DIR)
	@for dir in $(SRC_DIRS); do \
		mkdir -p $(OBJ_DIR)/$$dir; \
	done

clean:
	rm -rf $(OBJ_DIR) $(TARGET)

dump: $(TARGET)
	objdump -D $< > $(TARGET).dump 