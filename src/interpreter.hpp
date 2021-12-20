#pragma once

#include <vector>
#include <array>
#include <span>

#include "types.hpp"
#include "instructions.hpp"
#include "compiler.hpp"
#include "options.hpp"

struct Runtime {
    std::span<u32> instructions;
    std::vector<i32> memory;
};

bool create_runtime(Program &program, Runtime &out, Options &options);

bool execute(Runtime &runtime, Options &options);
