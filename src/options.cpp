#include "options.hpp"

#include <vector>
#include <iostream>
#include <iomanip>

#include "args.hpp"

static void print_version() {
    std::printf("Running TTK91 compiler-interpreter (ttkic) version 0.0.1 by kbjakex.\n");
}

static void print_option(const char *sform, const char* lform, const char *desc) {
    std::printf("  %-4s  %-18s   %s\n", sform, lform, desc);
}

static void print_help() {
    print_version();
    std::printf("\nBasic usage:\n");
    std::printf("  ttkic <file> [option(s)]\n\n");

    std::printf("Options:\n");
    print_option("-i", "--bench-iterations", "Sets the number of times the program is ran for the benchmark.");
    print_option("-bio", "--bench-io", "Suppresses printing while benchmarking. (default: false)");
    print_option("-d", "--dry", "Compiles the file without executing.");
    print_option("-ss", "--stack-size", "Sets the stack size for the program. (1 MiB by default)");
    print_option("", "--help", "Shows this page.");
    print_option("-v", "--version", "Shows version information.");
}

bool parse_options(int argc, char **argv, Options &out) {
    bool help = false, // --help
        version = false; // -v, --version

    auto result = Args::parser()
        .add_arg("i", "bench-iterations", out.benchmark_iterations)
        .add_arg("bio", "bench-io", out.bench_io)
        .add_arg("d", "dry", out.dry_run)
        .add_arg("ss", "stack-size", out.stack_size)
        .add_arg("help", help)
        .add_arg("v", "version", version)
        .parse(std::size_t(argc), argv);

    if (help) {
        print_help();
        std::exit(0);
    }

    if (version) {
        print_version();
        std::exit(0);
    }

    if (!result.unrecognized_options.empty()) {
        std::printf("Warning: Ignoring unrecognized options (");
        for (const auto &opt : result.unrecognized_options) {
            std::cout << std::quoted(opt) << " "; 
        }
        std::printf("\b)\n\n");
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
    out.filename = result.remaining_args[0].data();

    if (out.benchmark_iterations > 500'000'000) {
        std::printf("Warning: Over 500 million benchmark iterations requested (intentional? ctrl-c to abort)\n\n");
    } else if (out.benchmark_iterations == 0) out.benchmark_iterations = 1;

    return true;
}