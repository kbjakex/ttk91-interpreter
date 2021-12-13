#pragma once

#include <vector>

#include "types.hpp"
#include "instructions.hpp"

struct DataConstant {
    i32 address;
    i32 value;
};

struct Program {
    std::vector<Instruction> instructions;
    std::vector<DataConstant> constants;
    std::size_t data_section_bytes;
};
