#pragma once

#include "types.hpp"

#include <string_view>

enum class InstructionType : u8 {
    STORE,
    LOAD,

    IN, OUT,

    ADD,
    SUB,
    MUL,
    DIV,
    MOD,

    AND,
    OR,
    XOR,
    NOT,
    SHL,
    SHR,
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
    EXT_IRET, // NOT officially part of the language

    EXT_HALT, // NOT officially part of the language

    NUM_INSTRUCTIONS // not an instruction.
};

enum class Register {
    R0 = 0, R1, R2, R3, R4, R5, R6, R7,
    EXT_ZR, // Zero register, always zero

    SP = R6, // Stack pointer
    FP = R7, // Frame pointer

    NUM_REGISTERS // not a register
};

enum class InDevices {
    KBD = 0,            // Integer input
    EXT_FKBD = 1,       // Opt-in extension for float input
    EXT_CKBD = 2,       // Opt-in extension for character input
    EXT_CKBD_NIO = 3    // Non-blocking variant of EXT_CKBD
};

enum class OutDevices {
    CRT = 0,
    EXT_FCRT = 1, // Opt-in extension to print floats
    EXT_CCRT = 2 // Opt-in extension to print characters
};


// Immediate:
// LOAD R1, =2         value = decode_imm(ins)

// Register:
// LOAD R1, R2
// LOAD R1, =2(R2)     value = memory[-decode_dst(ins)] + decode_imm(ins)

// Direct:
// LOAD R1, 2(R2)      value = memory[memory[-decode_dst(ins)] + decode_imm(ins)]

// Indirect
// LOAD R1, @R2        value = memory[memory[memory[-decode_dst(ins)] + 0]]
// LOAD R1, @2(R2)     value = memory[memory[memory[-decode_dst(ins)] + decode_imm(ins)]]
enum class AddressMode {
    IMMEDIATE = 0,
    REGISTER = 1,
    DIRECT = 2,
    INDIRECT = 3,
};

void debug_print(u32 instruction, const char *end = "\n");

constexpr u32 INSTRUCTION_BITS = 6; // 64 opcodes
constexpr u32 ADDRESS_MODE_BITS = 2;
constexpr u32 DST_REGISTER_BITS = 3;
constexpr u32 SRC_REGISTER_BITS = 4; // To fit EXT_ZR
constexpr u32 VALUE_BITS = 16;

#define OPC_OFFSET 0
#define ADDRM_OFFSET (INSTRUCTION_BITS)
#define DST_OFFSET (ADDRM_OFFSET + ADDRESS_MODE_BITS)
#define SRC_OFFSET (DST_OFFSET + DST_REGISTER_BITS)
#define VALUE_OFFSET (SRC_OFFSET + SRC_REGISTER_BITS)

inline u32 encode_opcode(InstructionType type) { return u32(type) << OPC_OFFSET; }
inline u32 decode_opcode(u32 data) { return (data >> OPC_OFFSET) & ((1 << INSTRUCTION_BITS) - 1); }

inline u32 encode_dst(Register dst) { return u32(dst) << DST_OFFSET; }
inline u32 decode_dst(u32 data) { return (data >> DST_OFFSET) & ((1 << DST_REGISTER_BITS) - 1); }

inline u32 encode_src(Register src) { return u32(src) << SRC_OFFSET; }
inline u32 decode_src(u32 data) { return (data >> SRC_OFFSET) & ((1 << SRC_REGISTER_BITS) - 1); }

inline u32 encode_addrm(AddressMode mode) { return u32(mode) << ADDRM_OFFSET; }
inline u32 decode_addrm(u32 data) { return (data >> ADDRM_OFFSET) & ((1 << ADDRESS_MODE_BITS) - 1); }

inline u32 encode_value(i16 idx) { return u32(idx) << VALUE_OFFSET; }
inline i16 decode_value(u32 data) { return i16((data >> VALUE_OFFSET) & ((1 << VALUE_BITS) - 1)); }

#undef VALUE_OFFSET
#undef SRC_OFFSET
#undef DST_OFFSET
#undef ADDRM_OFFSET
#undef OPC_OFFSET

std::string_view instruction_name(InstructionType type);

std::string_view register_name(Register reg);
