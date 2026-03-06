CC      = riscv64-unknown-elf-gcc
OBJCOPY = riscv64-unknown-elf-objcopy
OBJDUMP = riscv64-unknown-elf-objdump

ARCH    = -march=rv64imac -mabi=lp64 -mcmodel=medany

CFLAGS  = $(ARCH) -Wall -O2
LDFLAGS = $(ARCH) -nostdlib -nostartfiles -T link.ld

SRCS = startup.S main.c
# This is text substitution
# After those lines OBJS = startup.o and main.o
OBJS = $(SRCS:.c=.o)
OBJS := $(OBJS:.S=.o)

# This is the first rule that make builds, we know from
# line main.elf: $(OBJS) that the elf depends on the object files
# For the object files, it takes the pattern rules %.o: %.c and %.o: %.S
all: main.elf

# Each rule has the form 
# target: dependencies
# 	command
# This mean: if the dependencies are newer than the target, 
# or the target does not exist, run command
# Since there's %, the rule becomes a pattern rule: 
# For every .o file, if a .c exists with the same name, run command
# command in this case expands to $(CC) $(CFLAGS) -c main.c -o main.o
%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

# Pattern rule for assembly file
%.o: %.S
	$(CC) $(CFLAGS) -c $< -o $@

main.elf: $(OBJS)
	$(CC) $(OBJS) $(LDFLAGS) -o $@

# Optional: generate disassembly
main.dump: main.elf
	$(OBJDUMP) -D main.elf > main.dump

# Optional: generate binary
main.bin: main.elf
	$(OBJCOPY) -O binary main.elf main.bin

# Clean
clean:
	rm -f *.o *.elf *.bin *.dump

.PHONY: all clean