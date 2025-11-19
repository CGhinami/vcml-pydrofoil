#include "pydrofoilcapi.h"
#include <stdio.h>
#include <stdlib.h>

/*
 * Organised as a linked list of memory blocks
 */

struct block {
  uint64_t block_id;
  uint64_t *mem;
  struct block *next;
};

struct mem {
    struct block *first_block;
};

uint64_t BLOCK_MASK = 0xFFFFFFul;

int write_mem(void* cpu, uint64_t address, int size, uint64_t value, void* payload) {
    struct mem* mem = (struct mem*) payload;
    uint64_t mask = address & ~BLOCK_MASK;
    uint64_t offset = address & BLOCK_MASK;
    uint64_t index = offset >> 3;  // Convert byte offset to uint64_t index

    struct block *current = mem->first_block;

    while (current != NULL) {
      if (current->block_id == mask) {
          current->mem[index] = value;
          return 0;
      } else {
          current = current->next;
      }
    }

    /*
     * If we couldn't find a block matching the mask, allocate a new
     * one, write the byte, and put it at the front of the block list.
     */
    struct block *new_block = (struct block *)malloc(sizeof(struct block));
    new_block->block_id = mask;
    new_block->mem = (uint64_t *)calloc(BLOCK_MASK + 1, sizeof(uint64_t));
    new_block->mem[index] = value;
    new_block->next = mem->first_block;
    mem->first_block = new_block;

    // Register this block as a DMA region
    uint64_t block_size = (BLOCK_MASK + 1) * sizeof(uint64_t);
    pydrofoil_cpu_set_dma_region(cpu, mask, block_size, (uint8_t*)new_block->mem);
    printf("DMA region registered at 0x%llx, size %llu bytes, physical addr %p\n",
           (unsigned long long)mask, (unsigned long long)block_size, (void*)new_block->mem);

    return 0;
}

int read_mem(void* cpu, uint64_t address, int size, uint64_t* destination, void* payload) {
    struct mem* mem = (struct mem*) payload;
    uint64_t mask = address & ~BLOCK_MASK;
    uint64_t offset = address & BLOCK_MASK;
    uint64_t index = offset >> 3;  // Convert byte offset to uint64_t index

    struct block *current = mem->first_block;

    while (current != NULL) {
        if (current->block_id == mask) {
            *destination = current->mem[index];
            return 0;
        } else {
            current = current->next;
        }
    }
    *destination = 0;
    return 0;
}

int main(int argc, char *argv[]) {
    if (argc < 3) {
        // Print usage information if not enough arguments are provided
        printf("Usage: %s <riscv binary> <number of steps to run>\n", argv[0]);
        return -1; // Exit with an error code
    }
    int steps = atoi(argv[2]);
    void* cpu = pydrofoil_allocate_cpu("rv64", argv[1]);
    if (cpu == NULL) {
        printf("Failed to allocate CPU for the provided binary.\n");
        return -1; // Exit with an error code
    }
    struct mem mem;
    mem.first_block = NULL;
    int res = pydrofoil_cpu_set_ram_read_write_callback(cpu, read_mem, write_mem, &mem);
    pydrofoil_cpu_simulate(cpu, steps);
    uint64_t cycles = pydrofoil_cpu_cycles(cpu);
    printf("Simulation completed. Total cycles: %llu\n", (unsigned long long)cycles);
    printf("reset\n");
    pydrofoil_cpu_reset(cpu);
    printf("running quietly\n");
    pydrofoil_cpu_set_verbosity(cpu, 0);
    printf("Reset PC %ld\n", pydrofoil_cpu_pc(cpu));
    pydrofoil_cpu_reset(cpu);
    res = pydrofoil_cpu_set_pc(cpu, 4096);
    if (res != 0) {
        printf("setting pc failed\n");
        return -1;
    }
    pydrofoil_cpu_simulate(cpu, steps);
    cycles = pydrofoil_cpu_cycles(cpu);
    printf("Simulation completed. Total cycles: %llu\n", (unsigned long long)cycles);
    uint64_t pc = pydrofoil_cpu_pc(cpu);
    printf("current pc: %llu\n", (unsigned long long)pc);
    if (pydrofoil_free_cpu(cpu) != 0) {
        printf("Failed to free CPU resources.\n");
        return -1; // Exit with an error code
    }
    printf("freed successfully\n");
    return 0; // Success
}
