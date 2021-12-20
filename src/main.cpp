
#include <fstream>
#include <string_view>
#include <iostream>
#include <string>
#include <cstring>

#include "types.hpp"
#include "compiler.hpp"
#include "interpreter.hpp"
#include "options.hpp"

std::string read_file(const char *filename) {
    std::ifstream stream(filename, std::ios::in | std::ios::binary);
    if (stream) {
        auto buf = std::string();
        stream.seekg(0, std::ios::end);
        buf.resize(std::size_t(stream.tellg()) + 1);
        stream.seekg(0, std::ios::beg);
        stream.read(reinterpret_cast<char*>(&buf[0]), buf.size()-1);
        stream.close();

        buf[buf.size()-1] = '\n';
        return buf;
    }
    return {};
}

// FIXME. std::tolower has many problems such as being UB outside ASCII range,
// and doing an incorrect job for anything beyond ASCII. And being slow.
// Pull in ICU to do the transformation correctly.

bool compile_file(const char *filename, Program &out) {
    std::string_view name{ filename, std::strlen(filename) };
    return Compiler::compile(name, read_file(filename), out);
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

    // for (auto &ins : prog.instructions) debug_print(ins);

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