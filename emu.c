// Copyright (c) Andrew Li 2018
// https://github.com/andrew0x4c/oort

// Emulator for Oort instruction set

#include <stdlib.h>
#include <stdio.h>
#include <stdint.h>
#include <inttypes.h>
#include <stdbool.h>

typedef struct CPU {
    uint8_t* mem;
    uint64_t gpr[16];
    uint64_t pc;
    uint64_t acc;
    uint64_t lr;
    uint64_t sr;
    bool halt;
} CPU;

CPU* new_CPU(uint8_t* mem) {
    CPU* cpu = malloc(sizeof(CPU));
    cpu->mem = mem;
    for(int r = 0; r < 16; r++) cpu->gpr[r] = 0;
    cpu->pc = 0;
    cpu->acc = 0;
    cpu->lr = 0;
    cpu->sr = 0;
    cpu->halt = 0;
    return cpu;
}

uint64_t get_u64(uint8_t* mem, uint64_t idx) {
    uint64_t base = idx & ~7;
    uint64_t off = idx & 7;
    uint64_t val = 0;
    for(int i = 0; i < 8; i++) {
        val |= ((uint64_t) mem[base + ((off + i) & 7)]) << (i * 8);
    }
    return val;
}
// as much as you want to just cast mem to a uint64_t*,
// not all machines are little-endian.
void set_u64(uint8_t* mem, uint64_t idx, uint64_t val) {
    uint64_t base = idx & ~7;
    uint64_t off = idx & 7;
    for(int i = 0; i < 8; i++) {
        mem[base + ((off + i) & 7)] = (uint8_t) (val >> (i * 8));
    }
}

// these functions will later properly do whatever special actions,
// but for now we use these placeholders
void on_null(CPU* cpu) {
    printf("\n*** executed null ***\n");
    cpu->halt = 1;
}
void on_trace(CPU* cpu) {
    printf("\n*** executed trace ***\n");
    cpu->halt = 1;
}
void on_sys(CPU* cpu) {
    printf("\n*** executed sys ***\n");
    cpu->halt = 1;
}
void on_ext(CPU* cpu) {
    printf("\n*** executed ext ***\n");
    cpu->halt = 1;
}

#define SIGN_BIT (1L << 63)

void step(CPU* cpu) {
    if(cpu->halt) return;
    uint8_t opcode = cpu->mem[cpu->pc];
    uint8_t op = opcode >> 4;
    uint8_t arg = opcode & 0xF;
    uint64_t len = 1;
    uint16_t imm = 0;
    if((op & 0x8) == 0x8) {
        imm = (cpu->mem[cpu->pc + 1]) + (cpu->mem[cpu->pc + 2] << 8);
        len |= 2; // yep; len is always either 0b01 or 0b11.
    }
    uint64_t ximm = 0;
    if((op & 0xC) == 0xC) { // if 0b11??
        ximm = imm;
        if(arg & 0x1) ximm |= 0xFFFF0000;
        if(arg & 0x4) ximm = ((ximm & 0x0000FFFF) << 16)
                           | ((ximm & 0xFFFF0000) >> 16);
        if(arg & 0x2) ximm |= 0xFFFFFFFF00000000;
        if(arg & 0x8) ximm = ((ximm & 0x00000000FFFFFFFF) << 32)
                           | ((ximm & 0xFFFFFFFF00000000) >> 32);
    }
    uint64_t simm = 0;
    if((op & 0xC) == 0x8) { // if 0b10??
        simm = imm | -(imm & 0x8000);
    }
    uint64_t cond = 0;
    if((op & 0x6) == 0x0) { // if 0b?00?
        uint64_t sign = ((cpu->acc) &  SIGN_BIT) != 0;
        uint64_t rest = ((cpu->acc) & ~SIGN_BIT) != 0;
        cond = ((1 << (2 * sign + rest)) & arg) != 0;
    }
    // these if statements may be a little hard to understand as C code,
    // but they might be useful when thinking about an actual circuit.
    uint64_t next_pc = cpu->pc + len;
    switch(op) {
        case 0x0: {
            switch(arg) {
                case 0x0: on_null(cpu); break;
                case 0x1: on_trace(cpu); break;
                case 0x2: on_sys(cpu); break;
                case 0x3: on_ext(cpu); break;
                case 0x4: cpu->acc = cpu->sr; break;
                case 0x5: cpu->sr = cpu->acc; break;
                case 0x6: cpu->acc = cpu->sr << cpu->acc; break;
                case 0x7: cpu->acc = cpu->sr >> cpu->acc; break;
                case 0x8:                    next_pc = cpu->acc; break;
                case 0x9: cpu->lr = next_pc; next_pc = cpu->acc; break;
                case 0xA: next_pc = cpu->lr; break;
                case 0xB: break;
                case 0xC: cpu->acc = cpu->lr; break;
                case 0xD: cpu->lr = cpu->acc; break;
                case 0xE: cpu->acc = next_pc; break;
                case 0xF: cpu->halt = 1; break;
            }
        }; break;
        case 0x1: cpu->acc = -cond; break;
        case 0x2: cpu->acc = cpu->gpr[arg]; break;
        case 0x3: cpu->gpr[arg] = cpu->acc; break;
        case 0x4: cpu->acc &= cpu->gpr[arg]; break;
        case 0x5: cpu->acc |= cpu->gpr[arg]; break;
        case 0x6: cpu->acc ^= cpu->gpr[arg]; break;
        case 0x7: cpu->acc += cpu->gpr[arg]; break;
        case 0x8:                    if(cond) next_pc += simm; break;
        case 0x9: cpu->lr = next_pc; if(cond) next_pc += simm; break;
        case 0xA: cpu->acc = get_u64(cpu->mem, cpu->gpr[arg] + simm); break;
        case 0xB: set_u64(cpu->mem, cpu->gpr[arg] + simm, cpu->acc); break;
        case 0xC: cpu->acc &= ximm; break;
        case 0xD: cpu->acc |= ximm; break;
        case 0xE: cpu->acc ^= ximm; break;
        case 0xF: cpu->acc += ximm; break;
        default:;
    }
    cpu->pc = next_pc;
}

void loop(CPU* cpu) {
    while(!cpu->halt) step(cpu);
}

uint64_t str_to_u64(char* ptr) {
    // atol clamps, I like wraparound
    uint64_t acc = 0;
    while(*ptr) {
        acc = acc * 10 + (*ptr - '0');
        ptr++;
    }
    return acc;
}

void info_u64(uint64_t x) {
    printf(" 0x");
    for(int i = 0; i < 4; i++) {
        printf(" %04"PRIx16"", (uint16_t) (x >> (16 * (3 - i))));
        // and they say Lisp has a lot of parentheses
    }
    printf(" (%"PRId64")", x);
    printf("\n");
}

void info_mem(uint8_t* mem, uint64_t ptr) {
    for(int i = 0; i < 16; i++) {
        printf(" %02"PRIx8"", mem[ptr + i]);
    }
    printf("\n");
}

void dump(CPU* cpu) {
    printf("*** begin CPU state ***\n");
    printf("pc  ="); info_u64(cpu->pc);
    printf("acc ="); info_u64(cpu->acc);
    printf("sr  ="); info_u64(cpu->sr);
    printf("lr  ="); info_u64(cpu->lr);
    for(int i = 0; i < 16; i++) {
        printf("r%-2d =", i); info_u64(cpu->gpr[i]);
    }
    printf("mem[pc:pc+16] ="); info_mem(cpu->mem, cpu->pc);
    // this might not be safe if pc doesn't point into memory for some reason
    printf("*** end CPU state ***\n");
}

int main(int argc, char** argv) {
    char* fn = argv[1];
    uint64_t memsize = str_to_u64(argv[1]);
    memsize = ((memsize - 1) | 7) + 1; // round up to multiple of 8
    uint8_t* mem = calloc(memsize, sizeof(uint8_t));
    CPU* cpu = new_CPU(mem);
    // the assembly language isn't yet set in stone,
    // this was just some testing code
    cpu->mem[0] = 0x1F; // test $zpmn
    cpu->mem[1] = 0x35; // mt r5
    cpu->mem[2] = 0x10; // test 0
    cpu->mem[3] = 0xF9; // addi $1x00, 0x6942
    cpu->mem[4] = 0x42;
    cpu->mem[5] = 0x69;
    cpu->mem[6] = 0x10; // test 0
    cpu->mem[7] = 0xE0; // xori $000x, 4096
    cpu->mem[8] = 0x00;
    cpu->mem[9] = 0x10;
    cpu->mem[10] = 0x38; // mt r8
    cpu->mem[11] = 0x29; // mf r9
    cpu->mem[12] = 0xF0; // addi $000x, 0x47
    cpu->mem[13] = 0x47;
    cpu->mem[14] = 0x00;
    cpu->mem[15] = 0x39; // mt r9
    cpu->mem[16] = 0x28; // mf r8
    cpu->mem[17] = 0xF3; // addi $111x, 0xFF
    cpu->mem[18] = 0xFF;
    cpu->mem[19] = 0xFF;
    cpu->mem[20] = 0x82; // jump $p, -13
    cpu->mem[21] = -13;
    cpu->mem[22] = 0xFF;
    cpu->mem[23] = 0x29; // mf r9
    cpu->mem[24] = 0xBF; // st r15, 0x100
    cpu->mem[25] = 0x00;
    cpu->mem[26] = 0x01;
    cpu->mem[27] = 0x10; // test 0
    cpu->mem[28] = 0xD0; // ori $000x, 0x2301
    cpu->mem[29] = 0x01;
    cpu->mem[30] = 0x23;
    cpu->mem[31] = 0xD4; // ori $00x0, 0x6745
    cpu->mem[32] = 0x45;
    cpu->mem[33] = 0x67;
    cpu->mem[34] = 0xD8; // ori $0x00, 0xAB89
    cpu->mem[35] = 0x89;
    cpu->mem[36] = 0xAB;
    cpu->mem[37] = 0xDC; // ori $x000, 0xEFCD
    cpu->mem[38] = 0xCD;
    cpu->mem[39] = 0xEF;
    cpu->mem[40] = 0xBF; // st r15, 0x207
    cpu->mem[41] = 0x07;
    cpu->mem[42] = 0x02;
    cpu->mem[43] = 0x0F; // halt
    dump(cpu);
    loop(cpu);
    dump(cpu);
    printf("mem:"); info_mem(cpu->mem, 0x100);
    printf("mem:"); info_mem(cpu->mem, 0x200);
    printf("mem:"); info_mem(cpu->mem, 0x000);
    printf("mem:"); info_mem(cpu->mem, 0x010);
    printf("mem:"); info_mem(cpu->mem, 0x020);
    printf("mem:"); info_mem(cpu->mem, 0x030);
}
