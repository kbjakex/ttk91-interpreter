#pragma once

#include <vector>
#include <array>

#include "types.hpp"
#include "instructions.hpp"
#include "compiler.hpp"

struct Runtime {
    std::vector<Instruction> instructions;
    std::vector<i32> memory;
    std::array<i32, 10> registers;
};

bool create_runtime(Program program, Runtime &out);

bool execute(Runtime &runtime, u64 benchmark_iterations = 1);
