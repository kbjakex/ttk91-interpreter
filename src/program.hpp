#pragma once

#include <vector>

#include "types.hpp"
#include "instructions.hpp"

struct DataConstant {
    i32 address;
    i32 value;
};

struct Program {
    std::vector<u32> instructions;
    std::vector<DataConstant> constants;
    std::size_t data_section_bytes;

    std::string source_code;
    std::vector<std::string_view> source_code_lines;
};
