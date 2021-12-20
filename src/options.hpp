#pragma once

#include "types.hpp"

// Compiler command line options

struct Options {
    u64 benchmark_iterations = 1;
    u64 stack_size = 1 << 20; // 1 MB
    const char* filename;
    bool bench_io = false;
    bool dry_run = false; // compilation only
};

bool parse_options(int argc, char **argv, Options &out);
