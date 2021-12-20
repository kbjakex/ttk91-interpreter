#pragma once

#include <vector>
#include <string>

#include "instructions.hpp"
#include "program.hpp"

namespace Compiler {
    bool compile(std::string_view file_name, std::string textual_code, Program &out);
}