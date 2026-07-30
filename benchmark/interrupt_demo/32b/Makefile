CC      = riscv32-unknown-elf-gcc
OBJCOPY = riscv32-unknown-elf-objcopy
OBJDUMP = riscv32-unknown-elf-objdump

ARCH    = -march=rv32imac_zicsr -mabi=ilp32

SRC_DIR = ..
LINK_DIR = ..
O ?= $(PWD)/build

CFLAGS  = $(ARCH) -Wall -O0 -g
LDFLAGS = $(ARCH) -nostdlib -nostartfiles -T $(LINK_DIR)/link.ld

SRCS = startup.S $(SRC_DIR)/main.c
OBJS = $(O)/startup.o $(O)/main.o
# This is text substitution
# After those lines OBJS = startup.o and main.o
# OBJS = $(SRCS:.c=.o)
# OBJS := $(OBJS:.S=.o)

all: $(O)/main.elf

$(O):
	mkdir -p $@

$(O)/startup.o: startup.S | $(O)
	$(CC) $(CFLAGS) -c $< -o $@

$(O)/main.o: $(SRC_DIR)/main.c | $(O)
	$(CC) $(CFLAGS) -c $< -o $@

$(O)/main.elf: $(OBJS)
	$(CC) $(OBJS) $(LDFLAGS) -o $@

$(O)/main.dump: $(O)/main.elf
	$(OBJDUMP) -D $< > $@

$(O)/main.bin: $(O)/main.elf
	$(OBJCOPY) -O binary $< $@

clean:
	rm -rf $(O)

.PHONY: all clean