#pragma once

#include <vector>
#include <array>
#include <span>

#include "types.hpp"
#include "instructions.hpp"
#include "compiler.hpp"
#include "options.hpp"
#include "program.hpp"

struct Runtime {
    std::span<u32> instructions;
    std::vector<i32> memory;

    Program *program_ref;
};

bool create_runtime(Program &program, Runtime &out, Options &options);

bool execute(Runtime &runtime, Options &options);
