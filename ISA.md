# Oort instruction set

## Cheat sheet

    All instructions      | 0x0* instructions
    0x: (misc) ---------> | 00: null
    1x: test cond         | 01: trace
    2x: mf   reg          | 02: halt
    3x: mt   reg          | 03: ext
    4x: and  reg          | 04: mfsr
    5x: or   reg          | 05: mtsr
    6x: xor  reg          | 06: shl
    7x: add  reg          | 07: shr
    8x: jump cond, offset | 08: jumpa
    9x: call cond, offset | 09: calla
    ax: ld   reg , offset | 0a: ret
    bx: st   reg , offset | 0b: retl
    cx: andi mode, data   | 0c: mflr
    dx: ori  mode, data   | 0d: mtlr
    ex: xori mode, data   | 0e: pc
    fx: addi mode, data   | 0f: nop
    Conditions        | Extended immediates
    bit 0: zero       | bit 0: fill 16-bit chunk
    bit 1: positive   | bit 1: fill 32-bit chunk
    bit 2: minimum    | bit 2: swap 16-bit chunks
    bit 3: negative   | bit 3: swap 32-bit chunks
    Calling convention
        caller-saved: r0-r7, sr, acc
        callee-saved: r8-r15, lr ("preserved")
        arguments:     if possible r0-r3, otherwise *r0
        return values: if possible r4-r7, otherwise *r4

### Mnemonics

Arithmetic instructions are always of the form `0bI1XX` (i.e. instructions 4, 5, 6, 7 for both 1 and 3 byte instructions), where `I` denotes if an immediate is used, and `XX` is the operation. They are always in the order `and`, `or`, `xor`, `and`. The first two are "duals" of each other. The second two are actually both addition, but without carry for `xor` and with carry for `add`.

Load/store instructions are always of the form `0bM01X` (i.e. instructions 2, 3 for both 1 and 3 byte instructions), where `M` denotes if the load/store is from memory, and `X` is the operation. Load always comes first. (`mf` and `mt` are like `ld` and `st` but to/from the register file instead of memory.)

This generalizes somewhat to `mflr`, `mtlr`, `mfsr`, `mtsr`: load (`mf*r`) always comes before store (`mt*r`), and they are always the first two instruction in a four-instruction group.

The four instructions which generate exceptions or could otherwise be slow in executing come first (`null`, `trace`, `halt`, `ext`), with `null` being the null byte (`0x00`). `null` and `trace` are grouped, because in theory, a regular program should not run either, while both `halt` and `ext` are purposely executed. `ext` is last, because it is at the boundary of supervisor-mode and user-mode, as is its opcode.

The four shift-related instructions come next.

The next eight instructions relate to program flow.

Note that `jumpa` and `calla` are `0x08` and `0x09`, while `jump` and `call` are `0x8*` and `0x9*`.

`ret` is `0x0a`. This is because `0x0a` is newline, which you get by pressing "return". (Unfortunately I can't take credit for this; I saw a blog post on something similar.)

While the `retl` instruction doesn't have an obvious use and needs to be implemented carefully in an emulator, it fits the pattern of the previous 3 instructions which might reduce the size of the circuit.

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

These registers are used to store data, and are the arguments for arithmetic operations. Often, `gpr[x]` is denoted `rx`; for example, `r4` refers to `gpr[4]`.

## Calling convention

### Reserved registers

The register `r15` is used to point to a table consisting of global symbols such as function pointers. This is because Oort has no absolute memory access instruction, as well as to help write position independent code. Note that `r15` should point to the center of the table, to maximize the amount of accessible memory.

The register `r14` is used as the stack pointer. The stack grows downwards, and the value at `r14` is always the last pushed item. Because there is no specific push instruction, a push is emulated by manually adjusting `r14`. For example, this instruction sequence pushes r8 and r9 to the stack:

    2e .. .. | mf r14
    f3 f0 ff | addi $111x, -16
    3e .. .. | mt r14
    28 .. .. | mf r8
    be 00 00 | st r14, 0
    29 .. .. | mf r9
    be 08 00 | st r14, 8

See section "Reserved registers" in DESIGN.md for additional notes.

### Function calls

A function must preserve the states of the following registers:
- `r8` through `r15`
- The link register `lr`

This means that a function is free to change the values of the following registers:
- `r0` through `r7`
- The shift register `sr`
- The accumulator `acc`

Note that `lr` being callee-saved (preserved) means that when a function returns, the return address must be the same as the address `lr`. The only way a function can return to an arbitrary address without `ret` is through `jumpa`; however, if a function uses `jumpa` to return, it can't control what it returns in `acc`. Thus, it is recommended that functions always return with `ret`, after restoring `lr` if it was pushed onto the stack.

When passing arguments, look at the number (size) of the arguments. If there are at most 4 arguments or they fit in in 256 bits, they are passed in registers `r0` through `r3`. Otherwise, the arguments are stored in memory (with earlier arguments having lower addresses), with `r0` a pointer to the start of the arguments. This includes the case where there are a variable number of arguments. The pointer `r0` must be 8-byte aligned.

If an argument is less than 64 bits (such as when passing an 8-bit value), the full 64 bits are used, and the unused bits should be ignored by the callee. If an argument is more than 64 bits (such as when passing a pair of 64-bit values), it is padded to the nearest multiple of 64-bits. This means that each argument is aligned at a 64-bit boundary, which is a register if the arguments are passed in registers.

Example:
- `void f(uint64_t a)`: `a` stored in `r0`
- `void f(uint8_t a, int32_t b)`: `a` stored in low 8 bits of `r0`, `b` stored in low 32 bits of `r1`
- `void f(uint64_t a, uint64_t b, uint64_t c, uint64_t d)`: `a` stored in `r0`, `b` stored in `r1`, `c` stored in `r2`, `d` stored in `r3`
- `void f(uint64_t a, uint64_t b, uint64_t c, uint64_t d, uint64_t e)`: `a` stored in `*(r0 + 0x00)`, `b` stored in `*(r0 + 0x08)`, `c` stored in `*(r0 + 0x10)`, `c` stored in `*(r0 + 0x18)`, `e` stored in `*(r0 + 0x20)`
- If we have `struct St { uint32_t x; int8_t y; }`; `void f(St a, uint8_t b)`: `a.x` stored in low 32 bits of `r0`, `a.y` stored in next 8 bits of `r0`, `b` stored in low 8 bits of `r1`.
- If we have `struct St { uint64_t x; int64_t y; }`; `void f(St a, uint64_t b)`: `a.x` stored in `r0`, `a.y` stored in `r1`, `b` stored in `r2`.

Return values are analogous to arguments but with registers `r4` through `r7` instead of `r0` through `r3`. Note that in the many-return-value case, the caller passes the pointer in `r4`, not the callee. Also, memory allocated for the return value (`*r4`) should not overlap with memory allocated for the arguments (`*r0`).

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

Certain 3-byte instructions (`cx`, `dx`, `ex`, `fx`) interpret their argument as options for interpreting the immediate IMM. Given an immediate IMM, it is first extended to 32 bits by padding on the left with bit 0 of the argument. Then, if bit 2 is set, the two 16-bit chunks are swapped. This value is then extended to 64 bits by padding on the left with bit 1 of the argument. Finally, if bit 3 is set, the two 32-bit chunks are swapped. This result is XIMM.

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

#### `02: halt`; Halt

    halt()

Halts the processor, until awoken by the next interrupt.

#### `03: ext`; Extension action

Perform extension action given in `sr`. If invalid action is specified, traps with exception (???).

Further extensions to Oort will use `ext` to perform both user-level actions, such as executing additional instructions, and supervisor-level actions, such as controlling virtual memory mappings.

"Basic" implementations of Oort may simply halt on any of `null`, `trace`, `halt`, and `ext`.

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

#### `0b: retl`; Return and link

    old_lr = lr
    lr = next_pc
    next_pc = lr

The address of next instruction is stored in the link register, and execution continues at the address given by the value in the link register (before the store).

One use of `retl` may be some version of coroutines, "bouncing" back and forth between two functions.

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

#### `0f: nop`; No operation

    // no operation

Nothing happens.

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

## Extension actions

Note: this is not yet implemented.

The `ext` instruction is used to add additional functionality to Oort without affecting the instructions already assigned.

### Determining the extension instruction

The extension instruction executed is determined by the value in `sr`.

#### Supervisor

If the highest bit of `sr` is set, the extension is supervisor-level; if the supervisor bit is not set in the configuration register (`asr0`), the extension traps with exception (???). Otherwise, the extension is user-level, and can always be executed.

#### Extension number

When parsing extension instructions, bytes are read "big-endian", starting from the most significant. This is in contrast to regular instructions, which are "little-endian". This makes it easier to sort extension instructions using regular integer comparison.

To account for future additions, extension instructions are encoded with a variable-length encoding.
- If the first byte is not `0x7f` or `0xff`, the extension number is 2 bytes long.
- If the first byte is `0x7f` or `0xff`, the extension number is 4 bytes long.

Extension | Info
--- | ---
`0x_0000_ZZZZ_ZZZZ_ZZZZ` to `0x_7eff_ZZZZ_ZZZZ_ZZZZ` | User, 2-byte
`0x_7f00_0000_ZZZZ_ZZZZ` to `0x_7fff_ffff_ZZZZ_ZZZZ` | User, 4-byte
`0x_8000_ZZZZ_ZZZZ_ZZZZ` to `0x_feff_ZZZZ_ZZZZ_ZZZZ` | Supervisor, 2-byte
`0x_ff00_0000_ZZZZ_ZZZZ` to `0x_ffff_ffff_ZZZZ_ZZZZ` | Supervisor, 4-byte

While there is a natural extension to 6-byte and 8-byte extensions, such instructions cannot fit into the argument of the check extension extension.

#### Arguments

An extension instruction may take arguments, similar to regular instructions. Arguments to an extension instruction are placed after the extension number, from higher byte to lower bytes. However, an extension instruction may include arguments in the number itself, similarly to the `2x` opcodes (`mf`).

Possible arguments include register numbers, flags, and immediate values.

Examples:
- The extension instruction with number `0x1234` and arguments `0xe` and `0xf` is encoded as `0x_1234_ef00_0000_0000`.
- The extension instruction with number `0x7f123456` and argument `0xcdef` is encoded as `0x_7f12_3456_cdef_0000`.

### Extension list

(Tentative)

#### `0x_0010_xx...`: Data fence

(Tentative) Does the same thing as RISC-V's `FENCE` instruction, with the argument bits being (from high to low) PI, PO, PR, PW, SI, SO, SR, SW.

Note that one can easily set all argument bits to 1, to fence all data, using

    10 .. .. | test $never
    dd 20 00 | ori $x100, 0x0020
    03 .. .. | ext

Simple implementations may just fence all operations on any data fence.

Note: I keep this open for further changes since I'm not sure how much atomicity is really needed. For a simple emulator, where all memory accesses and I/O operations are performed immediately, these instructions are entirely unnecessary.

#### `0x_0011...`: Instruction fence

(Tentative) Does the same thing as RISC-V's `FENCE.I` instruction.

See note in "Data fence".

#### `0x_002x_...`: Move from auxiliary user register

Writes the value in auxiliary user register `x` into the accumulator.

Auxiliary user registers are labeled `aur0` through `aur15`.

Reserved for extensions which define additional registers to be accessed in user-mode, such as performance counters. Currently all AURs are reserved.

#### `0x_003x_...`: Move to auxiliary user register

Writes the value in the accumulator into auxiliary user register `x`.

Reserved; see "Move from auxiliary user register".

#### `0x_004x_...`: Atomic bitwise AND

First, keeps a copy of the value in `acc` (call it `k`). Then, loads the value in `gpr[x]` into `acc`, and computes `k` bitwise-and `acc` (i.e. the original value of `acc` bitwise-and the value at `gpr[x]`). Finally, stores the result at `gpr[x]`. (Memory rotation still applies.)

TODO: choose which of "atomic operations" and "LL/SC" to keep. The reservation flag for LL/SC might be a bit confusing to implement in an emulator, especially keeping track of which operations lose the reservation (for instance, interrupts).

#### `0x_005x_...`: Atomic bitwise OR

The same as "Atomic AND", but with the operation being bitwise-or.

#### `0x_006x_...`: Atomic bitwise XOR

The same as "Atomic AND", but with the operation being bitwise-xor.

#### `0x_007x_...`: Atomic add

The same as "Atomic AND", but with the operation being addition.

#### `0x_00ax...`: Load-link

Loads the memory at `gpr[x]` into `acc`, and reserves the memory location. (Memory rotation still applies.)

If another processor writes to that memory location (at any rotation), the reservation is lost.

Note that a reserved memory location is always aligned at 8 bytes.

See note in "Atomic bitwise AND".

#### `0x_00bx...`: Store-conditional

If the reservation on the memory at `gpr[x]` still holds, stores `acc` into the memory at `gpr[x]`, and sets `acc` to 0. Otherwise, does not perform the store, and sets `acc` to -1.

Only one layer of LL/SC is needed. In fact, the reservation can even be lost when any memory operation occurs, or when an interrupt occurs.

#### `0x_7fff_ffff_...`: System call

Traps with exception (???).

This instruction is used to call to supervisor-level routines from user mode.

TODO: it might make sense to pass the system call number as an argument to the extension instruction. Of course, an exception handler can already read the data in `sr`, so this would just be a recommendation.

#### `0x_8010_...`: Hart ID

(Tentative) Writes the ID of the current hardware thread to `acc`.

This is basically the `mhartid` register of RISC-V; however, since it is read only, it doesn't make sense to treat it as a register.

#### `0x_802x_...`: Move from auxiliary supervisor register

Writes the value in auxiliary supervisor register `x` into the accumulator.

Auxiliary supervisor registers are labeled `asr0` through `asr15`.

Reserved for extensions which define additional registers to be accessed in supervisor-mode, such as performance counters. Currently ASRs `asr3` through `asr15` are reserved.

`asr0` is the supervisor flags register (`sfr`).

`asr1` is the interrupt table register (`itr`).

(Tentative) `asr2` is the page fault address register (`pfar`).

#### `0x_803x_...`: Move to auxiliary supervisor register

Writes the value in the accumulator into auxiliary supervisor register `x`.

Reserved; see "Move from auxiliary supervisor register".

#### `0x_8040...`: TLB clear all

Invalidates all entries in the TLB.

#### `0x_805x...`: TLB clear

Invalidates the TLB entry for virtual address `gpr[x]`, if present.

#### `0x_806x...`: TLB get

If the TLB entry for virtual address `gpr[x]` is present, sets `acc` to the corresponding TLB entry; otherwise, sets `acc` to 0. Note that 0 is an invalid value for a TLB entry, so this case can be distinguished from when the entry is present.

#### `0x_807x...`: TLB set

Sets the TLB entry for virtual address `gpr[x]` to `acc`. The eviction algorithm is implementation-defined.

TODO: it may be useful to, instead of using TLB clear, use TLB set with an invalid mapping (such as 0). This saves some instructions, at the cost of increasing the complexity of TLB set.

#### `0x_ffff_ffff_ZZZZ_ZZZZ`: Check extension

If the extension given by the argument is implemented, sets `acc` to -1; otherwise, sets `acc` to 0. Note that only the extension number is considered, so running extension instruction `0x_ffff_ffff_0001_ffff` only checks if extension `0x0001` is implemented, not if the argument `0xffff` is valid.

Note that a check extension instruction can be created with the same length code as the extension instruction itself. For example, to run extension `0x1234`, one can use

    10 .. .. | test $never
    dc 34 12 | ori $x000, 0x1234
    03 .. .. | ext

and to check extension `0x1234`, one can use

    10 .. .. | test $never
    d6 34 12 | ori $11x0, 0x1234
    03 .. .. | ext

## Interrupts

Note: this is not yet implemented.

There are 256 possible interrupts.

When the processor receives an interrupt n, it finds the nth 16-byte entry of the table. The first 8 bytes are the new value of `pc`. The next 8 bytes are the new value of `asr0` (system flags register).

TODO: what is saved?

Each entry in the interrupt table is 16 bytes. This means that the interrupt table is exactly one page. The interrupt table register does not store the lowest 12 bits; when it is read back, those bits are filled with zeros.

TODO: assign interrupt numbers.

## TLB

Note: this is not yet implemented.

The TLB is software-managed.

The TLB is implemented as an n-way set-associative cache. For each virtual address `vaddr`, the tag is bits 63 to 16, and the index is bits 15 to 12. This means that there are 16 sets, each with n lines. Implementations are free to vary n, but it is recommended that n is at least 2. Note that this means the TLB can always map a contiguous 64K block of memory.

    TTTTTTTT TTTTTTTT TTTTTTTT TTTTTTTT TTTTTTTT TTTTTTTT IIII0000 00000000
    33333333 33333333 22222222 22222222 11111111 11111111 00000000 00000000
    fedcba98 76543210 fedcba98 76543210 fedcba98 76543210 fedcba98 76543210

(Tentative) There is no address-space ID field.

TODO: maybe a 1-bit ASID field (for user vs supervisor mappings) would be helpful for virtualization. Of course, a multi-bit ASID field has some practical advantages, but requires some extra bookkeeping on the software side, since PIDs may not have a direct correspondence with ASIDs.

The entry at a particular mapping contains the physical page address, as well as some configuration bits.

    NNNNNNNN NNNNNNNN NNNNNNNN NNNNNNNN NNNNNNNN NNNNNNNN NNNN1??? 0RWX0RWX
    33333333 33333333 22222222 22222222 11111111 11111111 00000000 00000000
    fedcba98 76543210 fedcba98 76543210 fedcba98 76543210 fedcba98 76543210

Bit number | Description
--- | ---
11 | Always 1
10 | Reserved for cache-related flags
9 | Reserved for cache-related flags
8 | Reserved for cache-related flags
7 | Always 0
6 | Supervisor-readable
5 | Supervisor-writable
4 | Supervisor-executable
3 | Always 0
2 | User-readable
1 | User-writable
0 | User-executable

TODO: if the ASID is used, the separate supervisor and user RWX flags might be unnecessary.

If the processor tries to perform an operation disallowed by the flags, interrupt (???) is raised.

## IO

Note: this is not yet implemented.

TODO: IO will likely be memory mapped, without explicit input and output instructions. In particular, IO devices will likely be mapped starting at `0x_ffff_0000_0000_0000`. To avoid even more hardware details, the first few devices will be "magic"; reads and writes will immediately work without setup.

