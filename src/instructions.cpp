#include "instructions.hpp"

#include <string>

#include "tsl/robin_map.h"

static tsl::robin_map<u8, std::string_view> &instr_name_table() {
    static auto table = tsl::robin_map<u8, std::string_view>{
        { u8(InstructionType::NOP), "NOP" },

        { u8(InstructionType::STORE), "STORE" },
        { u8(InstructionType::LOAD), "LOAD" },
        { u8(InstructionType::IN), "IN" },
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
        { u8(InstructionType::NOT), "NOT" },
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
        { u8(InstructionType::IRET), "IRET" }, // Not officially part of the language  

        { u8(InstructionType::HALT), "HALT" }, // Not officially part of the language
    };
    return table;
}

std::string_view instruction_name(InstructionType type) {
    auto &tbl = instr_name_table();
    if (auto it = tbl.find(static_cast<u8>(type)); it != tbl.end()) {
        return it->second;
    }
    return "<unknown>";
}

static void print_register(Register reg) {
    if (R0 <= reg && reg <= R7) std::printf("R%d", (int)reg);
    else std::printf("PC");
}

void Instruction::debug_print(const char *end) const {
    //std::printf("<");

    // Instruction name
    auto &tbl = instr_name_table();
    if (auto it = tbl.find(static_cast<u8>(this->opcode)); it != tbl.end()) {
        std::printf("%s", it->second.data());
    } else {
        std::printf("UNKNOWN(%d)", (int)this->opcode);
    }

    std::putc('\t', stdout);

    // Destination register
    print_register(dst_reg());

    std::printf(", ");

    auto src = src_reg();
    switch (address_mode()) {
        case AddressingMode::INDEXED_IMMEDIATE: std::putc('=', stdout); break;
        case AddressingMode::INDEXED_DIRECT: break;
        case AddressingMode::INDEXED_INDIRECT: std::putc('@', stdout); break;
    }
    std::printf("%d(", address);
    print_register(src);
    std::printf(")%s", end);

    //std::printf(">");
}