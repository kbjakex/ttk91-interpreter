#pragma once

#include <vector>
#include <iostream>
#include <iomanip>

#include "types.hpp"
#include "args.hpp"

// Compiler command line options

struct Options {
    const char* filename;
    u64 benchmark_iterations;
};

bool parse_options(int argc, char **argv, Options &out) {
    auto result = Args::parser()
        .add_arg("i", "bench-iterations", out.benchmark_iterations)
        .parse(std::size_t(argc), argv);

    if (!result.unrecognized_options.empty()) {
        std::printf("Warning: Unrecognized options: [");
        for (const auto &opt : result.unrecognized_options) {
            std::cout << std::quoted(opt) << " "; 
        }
        std::printf("\b]");
    }

    if (result.remaining_args.size() > 1) {
        std::printf("Error: More than one filename given (");
        for (std::size_t i = 0; i < result.remaining_args.size()-1; ++i) {
            std::cout << result.remaining_args[i];
            if (i != result.remaining_args.size()-2) std::cout << " ";
            else std::cout << " and ";
        }
        std::cout << result.remaining_args.back() << "). Only one is allowed.\n";
        return false;
    }

    if (result.remaining_args.empty()) {
        std::printf("No input files.\n");
        return false;
    }

    if (out.benchmark_iterations > 500'000'000) {
        std::printf("Warning: Over 500 million benchmark iterations requested (intentional?)\n");
    } else if (out.benchmark_iterations == 0) out.benchmark_iterations = 1;

    return true;
}
