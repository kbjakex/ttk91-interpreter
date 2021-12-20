
#include <fstream>
#include <string_view>
#include <iostream>
#include <string>
#include <cstring>

#include "types.hpp"
#include "compiler.hpp"
#include "interpreter.hpp"
#include "options.hpp"

bool read_file(const char *filename, std::string &out) {
    std::ifstream stream(filename, std::ios::in | std::ios::binary);
    if (stream) {
        stream.seekg(0, std::ios::end);
        out.clear();
        out.resize(std::size_t(stream.tellg()) + 1);
        stream.seekg(0, std::ios::beg);
        stream.read(reinterpret_cast<char*>(&out[0]), out.size()-1);
        stream.close();

        out[out.size()-1] = '\n';
        return true;
    }
    return false;
}

// Something for the future:
// std::tolower has many problems such as being UB outside ASCII range,
// and doing an incorrect job for anything beyond ASCII. And being slow.
// Pull in ICU to do the transformation correctly.

bool compile_file(const char *filename, Program &out) {
    std::string bytes{};
    if (!read_file(filename, bytes)) {
        std::printf("Error: File \"%s\" does not exist\n", filename);
        return false;
    }

    std::string_view name{ filename, std::strlen(filename) };
    return Compiler::compile(name, std::move(bytes), out);
}

int main(int argc, char **argv) {
    auto opts = Options{};
    if (!parse_options(argc, argv, opts)) {
        return 1;
    }

    auto prog = Program{};
    if (!compile_file(opts.filename, prog)) {
        return 1;
    }

    if (opts.dry_run) {
        std::printf("Dry run finished\n");
        return 0;
    }

    // Optimizer here

    auto runtime = Runtime{};
    if (!create_runtime(prog, runtime, opts)) {
        return 1;
    }

    if (!execute(runtime, opts)) {
        return 1;
    }

    return 0;
}