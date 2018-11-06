# Oort instruction set

## Cheat sheet

    All instructions
        0x: (misc)
        1x: test cond
        2x: mf reg
        3x: mt reg
        4x: and reg
        5x: or reg
        6x: xor reg
        7x: add reg
        8x: jump cond, offset
        9x: call cond, offset
        ax: ld reg, offset
        bx: st reg, offset
        cx: andi mode, data
        dx: ori mode, data
        ex: xori mode, data
        fx: addi mode, data
    0x0* instructions
        00: null
        01: trace
        02: sys
        03: ext
        04: mfsr
        05: mtsr
        06: shl
        07: shr
        08: jumpa
        09: calla
        0a: ret
        0b: nop
        0c: mflr
        0d: mtlr
        0e: pc
        0f: halt
    Conditions
        bit 0: zero
        bit 1: positive
        bit 2: minimum
        bit 3: negative
    Extended immediates
        bit 0: fill 16-bit chunk
        bit 1: fill 32-bit chunk
        bit 2: swap 16-bit chunks
        bit 3: swap 32-bit chunks
    Calling convention
        r0-r7, lr: caller-saved
        r8-r15, sr, acc: callee-saved
        arguments: r0-r4, then stack
        return values:
                  b <=  64: acc
             64 < b <= 256: r0-r3
            256 < b       : *r7

### Mnemonics

Arithmetic instructions are always of the form `0bI1XX` (i.e. instructions 4, 5, 6, 7 for both 1 and 3 byte instructions), where `I` denotes if an immediate is used, and `XX` is the operation. They are always in the order `and`, `or`, `xor`, `and`. The first two are "duals" of each other. The second two are actually both addition, but without carry for `xor` and with carry for `add`.

Load/store instructions are always of the form `0bM01X` (i.e. instructions 2, 3 for both 1 and 3 byte instructions), where `M` denotes if the load/store is from memory, and `X` is the operation. Load always comes first. (`mf` and `mt` are like `ld` and `st` but to/from the register file instead of memory.)

This generalizes somewhat to `mflr`, `mtlr`, `mfsr`, `mtsr`: load (`mf*r`) always comes before store (`mt*r`), and they are always the first two instruction in a four-instruction group.

The four instructions which generate exceptions or could otherwise be slow in executing come first (`null`, `trace`, `sys`, `ext`), with `null` being the null byte (`0x00`).

The four shift-related instructions come next.

The next eight instructions relate to program flow.

Note that `jumpa` and `calla` are `0x08` and `0x09`, while `jump` and `call` are `0x8*` and `0x9*`.

`ret` is `0x0A`. This is because `0x0A` is newline, which you get by pressing "return". (Unfortunately I can't take credit for this; I saw a blog post on something similar.)

`halt` is `0x0F`. `halt` is the last thing your program does, and `0x0F` is the last misc instruction.

## Registers

Oort has 20 architecturally visible registers.

### `pc`: Program counter (64-bit)

This register points to the instruction that will be executed next.

### `acc`: Accumulator (64-bit)

This register is used as the target or condition for most instructions, including arithmetic operations, memory accesses, and control flow operations.

### `sr`: Shift register (64-bit)

This register is used for shift instructions.

### `lr`: Link register (64-bit)

After a function call, this register points to the instruction directly after the call instruction. It is used to resume execution after a function call.

### `gpr[16]`: General purpose registers (16, 64-bit)

These registers are used to store data, and are the arguments for arithmetic operations. Often, `gpr[x]` is denoted `rx`; for example, `r4`.

## Calling convention

### Reserved registers

The register `r15` is used to point to a table consisting of global symbols such as function pointers. This is because Oort has no absolute memory access instruction, as well as to help write position independent code. Note that `r15` should point to the center of the table, to maximize the amount of accessible memory.

The register `r14` is used as the stack pointer. The stack grows downwards, and the value at `r14` is always the last pushed item. Because there is no specific push instruction, a push is emulated by manually adjusting `r14`. For example, this instruction sequence pushes r8 and r9 to the stack:

    2e .. .. | mf r14
    f3 f0 ff | addi -16
    3e .. .. | mt r14
    28 .. .. | mf r8
    be 00 00 | st r14, 0
    29 .. .. | mf r9
    be 08 00 | st r14, 8

See section "Reserved registers" for additional notes.

### Function calls

A function must preserve the states of the following registers:
- `r8` through `r15`
- The link register `lr`
This means that a function is free to change the values of the following registers:
- `r0` through `r7`
- The shift register `sr`
- The accumulator `acc`

When passing arguments, the first four arguments are stored in registers `r0` through `r3`. Subsequent arguments are pushed onto the stack in reverse order (so the stack pointer points to the fifth argument).

If the return value fits inside 64 bits, it should be placed in `acc`. If the return value doesn't fit inside 64 bits, but fits inside 256 bits, it should be placed in registers `r0` through `r3`. Otherwise, the caller function should place in `r7` a pointer representing the memory where the return value should be placed, and the callee function should write the return values there.

The global pointer and stack pointer must always be aligned at 8 bytes on a function call. A function can assume that this is always true.

See section "Function calls" in DESIGN.md for additional notes.

## Memory

All memory accesses are 64-bit. Multibyte values are stored little-endian.

Whenever an address is unaligned, the data is rotated according to the low bits of the address, so the low byte in the accumulator always corresponds to byte point to by the address pointer, and the actual memory affected is always aligned.

For example, if the memory at addresses 0 to 7 contained the bytes `01 23 45 67 89 ab cd ef`, and a load operation was performed at address 3, the accumulator would then contain `0x_4523_01ef_cdab_8967`. If the accumulator contained the value `0x_fedc_ba98_7654_3210`, and a store operation was performed at address 3, the memory at addresses 0 to 7 would contain the bytes `ba dc fe 10 32 54 76 98`.

See section "Memory" in DESIGN.md for additional notes.

## Common variables

### COND: Condition

Certain instructions (`1x`, `8x`, `9x`) interpret their argument field as a condition. The four bits (bit 3 to bit 0) correspond to certain possible values of the accumulator.

Bit number | Accumulator range | Description
--- | --- | ---
bit 0 (`0x1`) | `0x_0000_0000_0000_0000` | zero
bit 1 (`0x2`) | `0x_0000_0000_0000_0001` to `0x_7fff_ffff_ffff_ffff` | positive
bit 2 (`0x4`) | `0x_8000_0000_0000_0000` | minimum (smallest possible negative)
bit 3 (`0x8`) | `0x_8000_0000_0000_0001` to `0x_ffff_ffff_ffff_ffff` | negative, but not the smallest value

Note that the first (low) bit of the option is equal to the OR of the low 63 bits of the accumulator, and the second (high) bit is equal to the highest (sign) bit of the accumulator.

To evaluate the condition `x`, look at which set the accumulator falls in (0, 1, 2, or 3). If this bit is set in `x`, COND is true. Otherwise, COND is false.

Example:
- The opcode `11` (condition `0001`) tests if the accumulator is zero (it falls in set 0).
- The opcode `1c` (condition `1100`) tests if the accumulator is negative (it falls in set 2 or 3).
- The opcode `15` (condition `0101`) tests if the accumulator is invariant under negation (it falls in set 0 or 2).

See section "COND" in DESIGN.md for additional notes.

### IMM: immediate

If an instruction has the high bit set (`0x80`), a 16-bit immediate value IMM follows. The immediate value is encoded little-endian.

### SIMM: Signed immediate

Certain 3-byte instructions interpret IMM as a signed 64-bit value, SIMM. In other words, SIMM is IMM, sign extended from 16-bit to 64-bit.

### XIMM: Extended immediate

Certain 3-byte instructions (`cx`, `dx`, `ex`, `fx`) interpret their argument as options for interpreting the immediate IMM. Given an immediate IMM, it is first extended to 32 bits by padding with bit 0 of the argument. Then, if bit 2 is set, the two 16-bit chunks are swapped. This value is then extended to 64 bits by padding with bit 1 of the argument. Finally, if bit 3 is set, the two 32-bit chunks are swapped. This result is XIMM.

See section "XIMM" in DESIGN.md for additional notes.

## Arithmetic

All arithmetic is performed in 2's complement representation. Integer overflow is ignored.

Whenever a shift operation is performed, the result is padded with zeros. That is, all shifts are logical, and not arithmetic.

## Instructions

See section "Instruction ordering" in DESIGN.md for additional notes.

### `0X: (misc)`

This encodes all of the miscellaneous opcodes which don't really take any arguments, such as return. If implementing an emulator, it might be useful to make this another switch-case.

#### `00: null`; Null instruction

Always traps with exception (???). When running regular programs, `null` should not be executed.

This behavior, as opposed to something like NOP, is to help catch when the program counter runs into unused or zeroed memory. This is useful regardless if due to a bug (not returning or jumping to an incorrect location), or maliciously (trying to use `0x00` instructions as a NOP sled).

#### `01: trace`; Trace instruction

Always traps with exception (???). Used for debugging.

This instruction is similar to the INT 3 (`0xCC`) instruction in x86. It is one byte, because if a debugger wants to modify an instruction stream, a one-byte instruction can replace an opcode anywhere, while a multi-byte instruction may overwrite parts of subsequent instructions if the target instruction was shorter than the `trace` instruction.

#### `02: sys`; System action

Perform system action. If invalid action is specified, traps with exception (???).

Further extensions to Oort will use `sys` to perform supervisor-level actions, such as writing to devices or controlling virtual memory mappings.

#### `03: ext`; Extension action

Perform extension action. If invalid action is specified, traps with exception (???).

Further extensions to Oort will use `ext` to perform user-level actions, such as executing additional instructions.

#### `04: mfsr`; Move from shift register

    acc = sr

Writes the value in the shift register into the accumulator.

#### `05: mtsr`; Move to shift register

    sr = acc

Writes the value in the accumulator into the shift register.

#### `06: shl`; Shift left

    acc = sr << (acc & 0x3F)

Computes the value in the shift register logically shifted left by the lower 6 bits of the accumulator, and writes the value in the accumulator.

See section "Shifting" in DESIGN.md for additional notes.

#### `07: shr`; Shift right

    acc = sr >> (acc & 0x3F)

Computes the value in the shift register logically shifted right by the lower 6 bits of the accumulator, and writes the value in the accumulator.

See section "Shifting" in DESIGN.md for additional notes.

#### `08: jumpa`; Jump to accumulator

    next_pc = acc

Execution continues at the address given by the value in the accumulator.

#### `09: calla`; Call to accumulator

    lr = next_pc
    next_pc = acc

The address of next instruction is stored in the link register, and execution continues at the address given by the value in the accumulator.

#### `0a: ret`; Return

    next_pc = lr

Execution continues at the address given by the value in the link register.

#### `0b: nop`; No operation

    // no operation

Nothing happens.

#### `0c: mflr`; Move from link register

    acc = lr

Writes the value in the link register into the accumulator.

#### `0d: mtlr`; Move to link register

    lr = acc

Writes the value in the accumulator into the link register.

#### `0e: pc`; Get program counter

    acc = next_pc

Writes the address of the next instruction into the accumulator.

This instruction is useful for performing PC-relative addressing, without using the call, copy return address, and update stack / link register trick, as this trick may negatively impact branch prediction.

#### `0f: halt`; Halt

    halt()

Halts the processor.

### `1x: test cond`; Test for condition

    if COND:
        acc = -1
    else:
        acc = 0

If COND is true, writes -1 into the accumulator. Otherwise, writes 0 into the accumulator.

Note that `10` has the effect of always setting the accumulator to 0, and `1f` has the effect of always setting the accumulator to -1.

See section "`test`" in DESIGN.md for additional notes.

### `2x: mf reg`; Move from register

    acc = gpr[x]

Writes the value in register `reg` into the accumulator.

### `3x: mt reg`; Move to register

    gpr[x] = acc

Writes the value in the accumulator into register `reg`.

### `4x: and reg`; Bitwise AND register

    acc &= gpr[x]

Computes the value in the accumulator bitwise-and the value in register `reg`, and writes the result into the accumulator.

### `5x: or reg`; Bitwise OR register

    acc |= gpr[x]

Computes the value in the accumulator bitwise-or the value in register `reg`, and writes the result into the accumulator.

### `6x: xor reg`; Bitwise XOR register

    acc ^= gpr[x]

Computes the value in the accumulator bitwise-xor the value in register `reg`, and writes the result into the accumulator.

### `7x: add reg`; Add register

    acc += gpr[x]

Computes the value in the accumulator plus the value in register `reg`, and writes the result into the accumulator.

### `8x: jump cond, imm`; Jump conditional

    if COND:
        next_pc += SIMM

If COND is true, execution continues at the address of the next instruction, plus SIMM.

### `9x: call cond, imm`; Call conditional

    if COND:
        lr = next_pc
        next_pc += SIMM

If COND is true, the address of next instruction is stored in the link register, and execution continues at the address of the next instruction, plus SIMM.

### `ax: ld reg, imm`; Load

    acc = mem[gpr[x] + SIMM]

Writes the 64 bits at the address given by ADDR into the accumulator, where ADDR denotes the value in register x, plus SIMM.

See note on unaligned accesses in section "Memory".

### `bx: st reg, imm`; Store

    mem[gpr[x] + SIMM] = acc

Writes the value in the accumulator into the 64 bits at the address given by ADDR, where ADDR denotes the value in register x, plus SIMM.

See note on unaligned accesses in section "Memory".

### `cx: andi mode, imm`; Bitwise AND immediate

    acc &= XIMM

Computes the value in the accumulator bitwise-and XIMM, and writes the result into the accumulator.

### `dx: ori mode, imm`; Bitwise OR immediate

    acc |= XIMM

Computes the value in the accumulator bitwise-or XIMM, and writes the result into the accumulator.

### `ex: xori mode, imm`; Bitwise XOR immediate

    acc ^= XIMM

Computes the value in the accumulator bitwise-xor XIMM, and writes the result into the accumulator.

### `fx: addi mode, imm`; Add immediate

    acc += XIMM

Computes the value in the accumulator plus XIMM, and writes the result into the accumulator.

