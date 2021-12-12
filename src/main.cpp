
#include <fstream>
#include <string_view>
#include <iostream>
#include <string>

#include "types.hpp"
#include "compiler.hpp"

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

#include <chrono>

bool compile_file(const char *filename, CompilationResult &out) {
    auto bytes = read_file(filename);
    
    // To lowercase
    for (auto &c : bytes) c = std::tolower(c);
    
    u32 sum{};
    auto start = std::chrono::steady_clock::now();
    for (int i = 0; i < 100'000; ++i) sum += Compiler::compile(bytes, out);
    auto end = std::chrono::steady_clock::now();
    std::printf("Took %.4fus on average\n", (end - start).count() / (100'000.0 * 1'000.0));
    return sum != 0;
}

int main() {
    auto compilation_res = CompilationResult{};
    if (!compile_file("test.k91", compilation_res)) {
        return 1;
    }

    // Optimizer here

    return 0;
}