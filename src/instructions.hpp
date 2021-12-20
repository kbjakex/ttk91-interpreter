#pragma once

#include "types.hpp"

#include <string_view>

// Many instructions are split into three forms:
// _I suffix means the loaded value is immediate, and stored entirely in instruction.
// _M suffix means load from memory, potentially double indirection
// _R means loading from a register.
// As is probably clear, _M is the slow one, _I and _R the fast beasts.
//
// Unfortunately, right now, a crude assumption is made about this enum in the compiler:
// All multi-variant instructions must be in order I, M, R, and must have consecutive integer values.
enum class InstructionType : u8 {
    STORE,
    LOAD,

    IN, OUT,

    // Integer arithmetic
    ADD,
    SUB,
    MUL,
    DIV,
    MOD,

    // Float arithmetic (opt-in extension)
    EXT_FADD,
    EXT_FSUB,
    EXT_FMUL,
    EXT_FDIV,
    EXT_FMOD,

    EXT_ITOF, // int to float
    EXT_FTOI, // float to int
    // TODO add roundf, floorf, ceilf (rdtoi, fltoi, cltoi?), absf

    EXT_SQRTF,  EXT_RSQRTF,             // sqrt(x), 1/sqrt(x)
    EXT_SINF,   EXT_COSF,   EXT_TANF,
    EXT_ASINF,  EXT_ACOSF,  EXT_ATANF,
    EXT_LOG2F,  EXT_LOG10F, EXT_LNF,
    EXT_ABSF,

    // Boolean/bit logic
    AND,
    OR,
    XOR,
    NOT,
    SHL,
    SHR,
    SHRA,

    COMP,


    // State register based jumps
    JUMP,
    JNEG,
    JZER,
    JPOS,
    JNNEG,
    JNZER,
    JNPOS,

    // Register based jumps
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

    /* Stack pointer */ SP = R6, 
    /* Frame pointer */ FP = R7,

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

enum class AddressMode {
    IMMEDIATE = 0,
    REGISTER = 1,
    DIRECT = 2,
    INDIRECT = 3,
};

// In TTK91, the dst register is the left-most one *except* for STORE.
// This exception has been translated away and destination is always in the same position

// Useful information:
// - For IN, device is stored in 'src' register. Target register in 'dst' as usual.
// - For OUT, device is stored in 'dst' and source register in 'src'.
// - For pushr/popr, the register is in 'dst'.

// Format
// CCCCCCCD DDMMSSSS AAAAAAAA AAAAAAAA
// C = opcode bits (7)
// D = destination register bits (3)
// M = addressing mode bits (2)
// S = source reg bits (4)
// A = address bits (16)

// Contains opcode, dst register and bits to figure out how to interpret the instruction data
/* struct InstructionHeader {
    u16 bits;
};

struct InstructionData {
    u32 bits; */

/*     std::size_t opcode() const { return bits >> 25; }
    i64 dst_reg() const { return (bits >> 22) & 0b111; }
    i64 src_reg() const { return (bits >> 16) & 0b1111; }
    u32 address_mode() const { return (bits >> 20) & 0b11; }
    i32 address() const { return i16(bits); }

    InstructionData &set_opcode(InstructionType type) { bits = (bits & ~0xFE000000) | (u32(type) << 24); return *this; }
    InstructionData &set_dst_reg(Register reg) { bits = (bits & ~0x1C00000) | (u32(reg) << 21); return *this; }
    InstructionData &set_src_reg(Register reg) { bits = (bits & ~0xF0000) | (u32(reg) << 16); return *this; }
    InstructionData &set_address_mode(AddressMode mode) { bits = (bits & ~0x300000) | (mode << 19); return *this; }
    InstructionData &set_address(i16 address) { bits = (bits & ~0xFFFF) | u32(address & 0xFFFF); return *this; } */

/*     void debug_print(const char *end = "\n") const;
}; */

void debug_print(u32 instruction, const char *end = "\n");

constexpr u32 INSTRUCTION_BITS = 6;
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
