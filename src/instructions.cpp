#include "instructions.hpp"

#include <string>

#include "tsl/robin_map.h"

static tsl::robin_map<u8, std::string_view> &instr_name_table() {
    static auto table = tsl::robin_map<u8, std::string_view>{
        { u8(InstructionType::STORE), "STORE" },
        { u8(InstructionType::LOAD), "LOAD" },

        { u8(InstructionType::IN),  "IN" },
        { u8(InstructionType::OUT), "OUT" },
        
        { u8(InstructionType::ADD), "ADD" },
        { u8(InstructionType::SUB), "SUB" },
        { u8(InstructionType::MUL), "MUL" },
        { u8(InstructionType::DIV), "DIV" },
        { u8(InstructionType::MOD), "MOD" },

        { u8(InstructionType::AND), "AND" },
        { u8(InstructionType::OR), "OR" },
        { u8(InstructionType::XOR), "XOR" },
        { u8(InstructionType::SHL), "SHL" },
        { u8(InstructionType::SHR), "SHR" },
        { u8(InstructionType::SHRA), "SHRA" },

        { u8(InstructionType::COMP), "COMP" },

        { u8(InstructionType::JUMP), "JUMP" },
        { u8(InstructionType::JNEG), "JNEG" },
        { u8(InstructionType::JZER), "JZER" },
        { u8(InstructionType::JPOS), "JPOS" },
        { u8(InstructionType::JNNEG), "JNNEG" },
        { u8(InstructionType::JNZER), "JNZER" },
        { u8(InstructionType::JNPOS), "JNPOS" },

        { u8(InstructionType::JLES), "JLES" },
        { u8(InstructionType::JEQU), "JEQU" },
        { u8(InstructionType::JGRE), "JGRE" },
        { u8(InstructionType::JNLES), "JNLES" },
        { u8(InstructionType::JNEQU), "JNEQU" },
        { u8(InstructionType::JNGRE), "JNGRE" },

        { u8(InstructionType::CALL), "CALL" },
        { u8(InstructionType::EXIT), "EXIT" },
        { u8(InstructionType::PUSH), "PUSH" },
        { u8(InstructionType::POP), "POP" },
        { u8(InstructionType::PUSHR), "PUSHR" },
        { u8(InstructionType::POPR), "POPR" },

        { u8(InstructionType::SVC), "SVC" },
        { u8(InstructionType::EXT_IRET), "EXTRET" }, // Not officially part of the language  

        { u8(InstructionType::EXT_HALT), "EXT_HALT" }, // Not officially part of the language
    };
    return table;
}

std::string_view instruction_name(InstructionType type) {
    auto &tbl = instr_name_table();
    if (auto it = tbl.find(static_cast<u8>(type)); it != tbl.end()) {
        return it->second;
    }
    return "{unknown}";
}

std::string_view register_name(Register reg) {
    switch(reg) {
        case Register::R0: return "R0";
        case Register::R1: return "R1";
        case Register::R2: return "R2";
        case Register::R3: return "R3";
        case Register::R4: return "R4";
        case Register::R5: return "R5";
        case Register::R6: return "SP (R6)";
        case Register::R7: return "FP (R7)";
        case Register::EXT_ZR: return "ZR";
        default: return "{??}";
    }
}

void debug_print(u32 ins, const char *end) {
    // Instruction name
    auto &tbl = instr_name_table();
    auto opcode = decode_opcode(ins);
    if (auto it = tbl.find(u8(opcode)); it != tbl.end()) {
        std::printf("%s", it->second.data());
    } else {
        std::printf("{??: %d)", (int)opcode);
    }

    // Destination register
    std::printf("\t%s, ", register_name(Register(decode_dst(ins))).data());

    auto src = decode_src(ins);
    switch (AddressMode(decode_addrm(ins))) {
        case AddressMode::IMMEDIATE: std::putc('=', stdout); break;
        case AddressMode::REGISTER: break;
        case AddressMode::DIRECT: break;
        case AddressMode::INDIRECT: std::putc('@', stdout); break;
    }
    std::printf("%d(%s)%s", 
        decode_value(ins),
        register_name(Register(src)).data(),
        end
    );
}