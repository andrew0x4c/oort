# Design decisions

This document describes some remarks on the specific designs of some features in Oort, but which are not necessary for implementation.

## Instruction ordering

As noted in the mnemonics, the instructions are ordered very consistently. For example, the 8 arithmetic instructions (bit `0x40` is 1) can likely be implemented using just few masks and multiplexers:
- The first argument into the ALU is always `acc`.
- The second argument is `gpr[x]` if the bit `0x80` is 0, and XIMM is the bit `0x80` is 1.
- If the instruction is an AND or OR (bit `0x20` is 0), just use an AND gate, and bitwise-xor the inputs and outputs with bit `0x10`.
- If the instruction is an XOR or ADD (bit `0x20` is 1), just use an adder, but bitwise-and each of the carry bits with bit `0x10`.
(Actually that last one may not be that easy when actually building a circuit, but it is easy to remember.)

The `jump` and `call` instructions differ only by the `0x10` bit, so writing `next_pc` into `lr` can almost directly be controlled by that bit.

## Reserved registers

We chose the "high" registers to be reserved for similarity to real architectures. For example, ARM treats its `R13` (from `R0` to `R15`) as the stack pointer `SP`, and MIPS treats its `$29` (from `$0` to `$31`) as the stack pointer `$sp`.

The global pointer corresponds to the TOC in PowerPC. Note that the purpose of pointing to the center of the table is because if the table starts from offset 0, the memory offset overflows after 4096 8-byte entries (only 32K); however, starting from offset -32768, the table can contain 8192 8-byte entries (the full 64K).

Note that a call to a function stored in the global table is not much longer than an offset-based call because of the `calla` instruction:

    9f b1 76 | call $zpmn, 0x76b1

vs

    af c8 a4 | ld r15, 0xa4c8
    09 .. .. | calla

This is also why we don't pass any information in `acc` for function calls.

## Function calls

(Tentative) We leave `r4` through `r7` as temporaries, but don't use them for arguments, in case a function needs temporary storage at the start or directly after a function call. Also, we try to return values in `acc` because the caller may immediately use the result (requiring 0 moves), or move it to a register (requiring 1 move). If we always return in `r0`, which requires a move in the callee, the caller may immediately use the result (requiring 2 moves), or move it to a register (requiring 1 move). However, we do not do this for multiple-word return values, since only one word can be in `acc`, which may not be what the caller wants to use first.

(Tentative) For very large return values, the reason `r7` is used as the return address instead of `r4`, is so we can consistently treat `r4` as the first temporary register, `r5` as the second, and so on. Also, unlike some calling conventions which pass this pointer as an argument, we use a separate register so we can consistently treat `r0` as the first argument register, `r1` as the second, and so on.

TODO: see note in ISA.md on function calls.

## Memory

The memory "rotation" avoids several disadvantages with other implementations of unaligned memory access. Some alternatives include:
- Allow unaligned memory access: this can make it difficult to implement and reason about parts like caching and atomicity. Also, this means accessing data near a word boundary may affect data past the boundary, which may cause issues for memory-mapped devices.
- Trapping on unaligned access: this results in unnecessary complexity for simple implementations.
- Ignore low order bits of address: this requires extra shifting to access unaligned parts of a word.

The only memory access instructions are 64-bit in order to reduce the number of instructions and encodings, which would require additional logic. For example, loads of other data widths may be zero-extended, sign-extended, or just not extended at all. Also, partial word writes can cause complications for caching.

While accessing smaller amounts of data (32-bit, 16-bit, 8-bit) may seem difficult, it does not take too much extra work. For example, to read a 16-bit value pointed to by `r8`, one can directly load the data, and keep only the low 16 bits using an `andi` with the immediate `0xffff`. To write to a 16-bit value is somewhat more complicated; the unmodified data must be loaded into a register, and then masked with the new data, before issuing the store instruction. However, note that the position of the mask does not change with offset relative to a word boundary.

Examples:

Load byte at address `r0` into `r1`

    a0 00 00 | ld r0, 0
    c0 ff 00 | andi $000x, 0x00FF
    31 .. .. | mt r1

Store byte at address `r0` from `r1` (assuming `r1` is between 0 and 255)

    a0 00 00 | ld r0, 0
    c3 00 ff | andi $111x, 0xFF00
    51 .. .. | or r1
    b0 00 00 | st r0, 0

Okay, I guess it goes get kind of long.

## COND

While testing for the minimum possible value may seem strange, this way evaluating COND can be done by just using the sign bit of `acc` and the bitwise-or of the other bits of `acc` to select (multiplex) from the condition argument.

## XIMM

While the process of decoding an extended immediate may seem overcomplicated, it is surprisingly versatile.

First, note that we would like to access each 16-bit chunk of `acc` (such as for bitmasks).

Initially, we looked at using 2 bits for the shift, and sign-extending the rest. However, this means we can't encode values like `0x_0000_ffff_0000_0000`, which might be used in an `or`.

A way to fix this might be to instead control the padding/extension with another bit. This solves the previous problem, but then we can't encode values like `0x_ffff_ffff_0000_0000`, which might be used in an `add`.

We identified 12 possible encodings one might want:

    0x_0000_0000_0000_XXXX
    0x_ffff_ffff_ffff_XXXX
    0x_0000_0000_XXXX_0000
    0x_0000_0000_XXXX_ffff
    0x_ffff_ffff_XXXX_0000
    0x_ffff_ffff_XXXX_ffff
    0x_0000_XXXX_0000_0000
    0x_0000_XXXX_ffff_ffff
    0x_ffff_XXXX_0000_0000
    0x_ffff_XXXX_ffff_ffff
    0x_XXXX_0000_0000_0000
    0x_XXXX_ffff_ffff_ffff

The two middle encodings have a clear pattern: two bits to control the other parts of the value. We decided to generalize this pattern to make it easier to use and remember. This adds the four encodings

    0x_0000_0000_ffff_XXXX
    0x_ffff_ffff_0000_XXXX
    0x_XXXX_ffff_0000_0000
    0x_XXXX_0000_ffff_ffff

which, while strange, are sometimes useful. (For example, the first one might be used to encode the value `0x_0000_0000_ffff_ff00`.)

## Shifting

The result of all shift operations is placed in the accumulator. This is to make it easier to extract multiple bitfields, which can be done by leaving the value in `sr`, and using `shl` and `shr` multiple times.

Using a designated shift register could potentially allow for optimizations by placing `sr` physically closer to the shifter.

Actually, the original reason for using `sr` for shifts was that space for instructions was running low after encoding the other instructions, so we couldn't fit 16 or 32 shift instructions like with the arithmetic instructions. Encoding shift instructions also has the issue that unlike the arithmetic instructions, the shift instructions are not commutative, so one would need to choose if the shift amount is the register or `acc`. For example, a C-style `x << y` might work better with `acc` as the shift amount by moving `x` into a register first, but if `y` was already stored in a register, having the register be the shift amount might work better.

## `test`

The values of 0 and -1 were chosen to make it easier to use the result of a test as a mask. For example, we can conditionally add a value in constant time as follows:

    28 .. .. | mf r8
    11 .. .. | test $z
    49 .. .. | and r9
    7a .. .. | add r10
    3a .. .. | mt r10

If `r8` was 0, we add `r9` to `r10`; else, we don't do anything. Constant time operations are useful in areas like cryptography, where timing can be a side-channel.

Also, as noted in ISA.md, testing the always-false and always-true condition corresponds to unconditionally setting `acc` to 0 or -1.

