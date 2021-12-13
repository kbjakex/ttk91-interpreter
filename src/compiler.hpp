#pragma once

#include <vector>
#include <string_view>

#include "instructions.hpp"
#include "program.hpp"

namespace Compiler {
    bool compile(std::string_view textual_code, Program &out);
}