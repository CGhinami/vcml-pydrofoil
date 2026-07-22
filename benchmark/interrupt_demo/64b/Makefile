CC      = riscv64-unknown-elf-gcc
OBJCOPY = riscv64-unknown-elf-objcopy
OBJDUMP = riscv64-unknown-elf-objdump

ARCH    = -march=rv64imac -mabi=lp64 -mcmodel=medany

SRC_DIR = ..
LINK_DIR = ..
O ?= $(PWD)/build

CFLAGS  = $(ARCH) -Wall -O0 -g
LDFLAGS = $(ARCH) -nostdlib -nostartfiles -T $(LINK_DIR)/link.ld

OBJS = $(O)/startup.o $(O)/main.o

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