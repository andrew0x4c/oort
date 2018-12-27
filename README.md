# Oort
A simple variable-length instruction set

## Overview

Oort is a simple 64-bit load-store variable-length instruction set, intended for educational use.

It was designed to be simple enough to memorize all of the instructions and their encodings, while still being powerful enough to write complex programs. Specifically, this should be without having very long instruction sequences (especially for common practical operations like bitwise or and bitshifts; looking at you, SUBLEQ/SBN/RSSB!). In theory, one should be able to write an entire OS in Oort machine code without looking at the documentation. (In practice this is probably impossible.)

Further goals included realism and extensibility, to be able to succinctly demonstrate systems-related concepts like virtual memory.

## Running

To build the emulator, just run

    make

To run the emulator, use

    ./emu --infile file [--memsize size]

The command `xxd -r -p` to convert from raw hex to binary may be useful; for example:

    echo f010320f | xxd -r -p | ./emu --infile /dev/stdin

Currently, the emulator just dumps the state of the processor before and after running.

## Roadmap

Not necessarily in order.

- [x] Write spec for core features
  - [x] Registers
  - [x] Calling convention
  - [x] Instructions
- [ ] Implement emulator
  - [x] Get something running
  - [ ] Load/store from files nicely
  - [ ] Interact with the program
  - [ ] Manually poke around in memory
- [x] Standardize `sys` and `ext` instruction formats
- [ ] Write spec for I/O (terminal-only for now)
- [ ] Implement assembler
  - [ ] Standardize assembly
    - [ ] Conditions
    - [ ] Extended immediates
  - [ ] Basic version (instructions, labels, a few directives)
  - [ ] Arithmetic expressions?
- [ ] Implement disassembler
- [ ] Write some nice example programs?
  - [ ] Common low-level idioms
    - [ ] Jump table
    - [ ] Loop unrolling
    - [ ] Duff's device
    - [ ] Bitwise tricks
    - [ ] ???
  - [ ] [Rosetta Code](https://rosettacode.org/wiki/Rosetta_Code)
  - [ ] [Esolang wiki](https://esolangs.org/wiki/Popular_problem)
  - [ ] [PPCG.SE](https://codegolf.stackexchange.com)
- [ ] Implement compiler to Oort assembly?
  - [ ] Lexer
  - [ ] Parser
  - [ ] Code generation
  - [ ] Optimization?
- [ ] Rewrite all that in Oort
  - [ ] Emulator
  - [ ] Assembler
  - [ ] Disassembler
  - [ ] Compiler?
- [ ] Add traps/exceptions
  - [ ] Write spec for exceptions
  - [ ] Implement it
- [ ] Standardize virtual memory
  - [ ] Instructions
  - [ ] TLB
  - [ ] ???
- [ ] Other supervisor mode stuff
  - [ ] ???
- [ ] Other OS-y devices one might need
  - [ ] Disk
  - [ ] Timer
  - [ ] Screen?
  - [ ] Keyboard?
  - [ ] Mouse?
  - [ ] Audio?
  - [ ] ???
- [ ] Write an OS for Oort
  - [ ] Wow that sounds like a lot of work
  - [ ] I'm definitely not going to fill this in for now
- [ ] ???

## Name

The name Oort comes from the instruction encoding. Each Oort instruction is either **o**ne **or** **t**hree bytes long.

(It took me way too long to find this name; more than a year after I came up with the instruction set itself!)

(Also you can probably make an Oort cloud joke out of this.)

## Instruction set

See [ISA.md](ISA.md) for details. Also, see [DESIGN.md](DESIGN.md) for some remarks on the design.

(Maybe I should make a fancy PDF document like the real architectures.)

## Inspirations

Oort draws inspiration from a variety of other computer architectures.
- The accumulator-based design comes from early computer architectures which did most or all of their arithmetic on an accumulator register.
- All memory accesses are load-store, as in RISC architectures such as ARM and MIPS.
- The variable-length instruction encoding (including the encoding for immediate values) was inspired somewhat by x86, but the encoding is much simpler.
- The rotation of unaligned accesses comes from some RISC architectures (ARM?); this makes it faster to access smaller aligned blocks of memory, as opposed to ignoring the least significant bits of the address (which would necessitate a rotate after loading and before storing).
- The use of the global pointer / TOC comes from PowerPC; we use a TOC because each memory access and jump can only directly access a 64K region around a pointer or the PC, which may not be enough for large programs.
- The 16 registers, as opposed to 8, comes from RISC architectures. It's 16 and not 32 because 16 fits nicely into one hexadecimal digit.
- The idea of using a software managed TLB comes from MIPS; although TLB miss handling is slower than with a hardware managed TLB, a software managed TLB makes it easier to write an emulator ;)
  - (Yes, I know I haven't started memory management, but it'll probably be a software TLB.)

## History

The idea behind something like Oort started as early as December 2016, back when I just wanted a toy 64-bit accumulator-based instruction set. Back then, it was very messy (with some instructions taking 8-byte immediates and multiple extension prefixes), but over time the instruction set simplified down to what it was now. In May 2017 the idea of 1 and 3 byte instructions was solidified, though there were some interesting peculiarities such as multiple register files. I worked on this only intermittently, so there are often gaps between changes.

See [HISTORY.md](HISTORY.md) for details on the changes. (TODO: actually write HISTORY.md.)

## License

TODO
