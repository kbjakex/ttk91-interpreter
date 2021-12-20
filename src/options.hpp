#pragma once

#include "types.hpp"

namespace Warnings {
    extern bool reserved_registers; // using R6/R7 in code
}

// Compiler command line options

struct Options {
    const char* filename;
    u64 benchmark_iterations = 1;
    bool bench_io = false;
    bool dry_run = false; // compilation only
    u64 stack_size = 1 << 20; // 1 MB
};

bool parse_options(int argc, char **argv, Options &out);
