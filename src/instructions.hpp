#pragma once

#include "types.hpp"

#include <string_view>

// NOTE NOTE NOTE
// The opcodes here do *NOT* have the same 
// ID as in the TTK91 reference!

// This is simply because giving them
// consecutive IDs makes lookups easier & faster,
// as it is now merely an array lookup.

// Even the instructions could be changed (and simply
// translated at compile-time) had there been a need
// (and I'd just have called them uOps ;^) ),
// but for now they are the same as in the reference.

enum class InstructionType : u8 {
    NOP,

    STORE,
    LOAD,
    IN,
    OUT,

    ADD,
    SUB,
    MUL,
    DIV,
    MOD,

    AND,
    OR,
    XOR,
    SHL,
    SHR,
    NOT,
    SHRA,

    COMP,

    JUMP,
    JNEG,
    JZER,
    JPOS,
    JNNEG,
    JNZER,
    JNPOS,

    JLES,
    JEQU,
    JGRE,
    JNLES,
    JNEQU,
    JNGRE,

    CALL,
    EXIT,
    PUSH,
    POP,
    PUSHR,
    POPR,

    SVC,
    IRET, // NOT officially part of the language

    HALT, // NOT officially part of the language

    NUM_INSTRUCTIONS // not an instruction.
};

enum class AddressingMode {
    INDEXED_IMMEDIATE,
    INDEXED_DIRECT,
    INDEXED_INDIRECT
};

// Exposed registers
enum Register {
    R0, R1, R2, R3, R4, R5, R6, R7,

    /* Stack pointer */ SP = R6, 
    /* Frame pointer */ FP = R7,

    PC
};

enum InDevices {
    KBD = 0,
};

enum OutDevices {
    CRT = 0,
};


// In TTK91, the dst register is the left-most one *except* for STORE.
// This exception has been translated away and destination is always in the same position

// Useful information:
// - For IN, device is stored in 'src' register. Target register in 'dst' as usual.
// - For OUT, device is stored in 'dst' and source register in 'src'.
// - For pushr/popr, the register is in 'dst'.

struct Instruction {
    InstructionType opcode;
    u8 reg_info_bits;
    i16 address;

    Register dst_reg() const { return static_cast<Register>((reg_info_bits & 0b00000111) >> 0); }
    Register src_reg() const { return static_cast<Register>((reg_info_bits & 0b11100000) >> 5); }
    AddressingMode address_mode() const { return static_cast<AddressingMode>((reg_info_bits & 0b00011000) >> 3); }


    void debug_print(const char *end = "\n") const;
};

std::string_view instruction_name(InstructionType type);
