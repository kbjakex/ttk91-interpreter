#pragma once

#include <vector>
#include <string_view>

#include "instructions.hpp"

struct DataConstant {
    i32 address;
    i32 value;
};

struct CompilationResult {
    std::vector<Instruction> instructions;
    std::vector<DataConstant> constants;
    std::size_t reserved_bytes; // bytes needed in total for all DC and DS variables
};

namespace Compiler {
    bool compile(std::string_view textual_code, CompilationResult &out);
}