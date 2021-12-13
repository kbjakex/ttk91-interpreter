#include "interpreter.hpp"

#include <cstdio>
#include <chrono>
#include <cmath>

__attribute__((noinline))
static void print_timings(u64 exec_time, u64 iterations);

bool execute(Runtime &rt, u64 bench_iterations) {
    auto instructions = rt.instructions.data();
    auto registers = rt.registers.data(); // Copy

    // Compiler extension. Supported by GCC / Clang.
    // Produces FAR better code than a table of function pointers or a switch.
    const void* JUMP_TABLE[] = {
        &&Lop_nop,

        &&Lop_store, &&Lop_load, &&Lop_in, &&Lop_out,

        &&Lop_add, &&Lop_sub, &&Lop_mul, &&Lop_div, &&Lop_mod,

        &&Lop_and, &&Lop_or, &&Lop_xor, &&Lop_shl, &&Lop_shr, &&Lop_not, &&Lop_shra,

        &&Lop_comp,

        &&Lop_jump, &&Lop_jneg, &&Lop_jzer, &&Lop_jpos, &&Lop_jnneg, &&Lop_jnzer, &&Lop_jnpos,

        &&Lop_jles, &&Lop_jequ, &&Lop_jgre, &&Lop_jnles, &&Lop_jnequ, &&Lop_jngre,

        &&Lop_call, &&Lop_exit, &&Lop_push, &&Lop_pop, &&Lop_pushr, &&Lop_popr,

        &&Lop_svc, &&Lop_iret,

        &&Lop_halt
    };

    auto start = std::chrono::steady_clock::now();

    std::printf("Starting execution\n");

    u64 remaining_executions = bench_iterations;
Lstart:
    remaining_executions -= 1;
    registers[PC] = 0;
    while (true) {
        Instruction ir = instructions[registers[PC]];
        //std::printf("PC: %d. ", registers[PC]); ir.debug_print();
        registers[PC] += 1;

        goto *JUMP_TABLE[static_cast<std::size_t>(ir.opcode)];

        Lop_store:

        Lop_load:

        Lop_in:

        Lop_out:

        Lop_add:

        Lop_sub:

        Lop_mul:

        Lop_div:

        Lop_mod:

        Lop_and:

        Lop_or:

        Lop_xor:

        Lop_shl:

        Lop_shr:

        Lop_not:

        Lop_shra:

        Lop_comp:

        Lop_jump: 
        
        Lop_jneg: 
        
        Lop_jzer: 
        
        Lop_jpos: 
        
        Lop_jnneg: 
        
        Lop_jnzer: 
        
        Lop_jnpos:

        Lop_jles: 
        
        Lop_jequ: 
        
        Lop_jgre: 
        
        Lop_jnles: 
        
        Lop_jnequ: 
        
        Lop_jngre:

        Lop_call: 
        
        Lop_exit: 
        
        Lop_push: 
        
        Lop_pop: 
        
        Lop_pushr: 
        
        Lop_popr:

        Lop_svc: 
        
        Lop_iret:

        Lop_nop:
        continue;
    }
Lop_halt:
    if (remaining_executions != 0) goto Lstart;

    auto end = std::chrono::steady_clock::now();

    if (registers[PC] == static_cast<i32>(rt.instructions.size())-1) {
        std::printf("Nag: no terminating instruction found. Perhaps you forgot the `SVC PC, =Halt`?\n");
    }

    auto elapsed = (end - start).count();
    print_timings(elapsed, bench_iterations);

    return true;
}

__attribute__((noinline))
static void print_timings(u64 exec_time, u64 iterations) {
    f64 scaled_time{};
    const char *unit{};

    if (exec_time > 500'000'000) { 
        scaled_time = exec_time / 1'000'000'000.0; unit = "s"; // >500ms
    } else if (exec_time > 500'000) { 
        scaled_time = exec_time / 1'000'000.0; unit = "ms"; // >500us
    } else if (exec_time > 500) { 
        scaled_time = exec_time / 1'000.0; unit = "us"; // >500ns 
    } else { 
        scaled_time = exec_time / 1.0; unit = "ns"; 
    }
        
    std::printf("Execution finished in %.4f%s.\n", scaled_time, unit);

    if (iterations > 1) {
        u64 avg_ns = exec_time / static_cast<u64>(iterations);

        f64 scaled_avg{};
        if (avg_ns > 500'000) {
            scaled_avg = avg_ns / 1'000'000.0; unit = "ms";
        } else if (scaled_avg > 500) {
            scaled_avg = avg_ns / 1'000.0; unit = "us";
        } else {
            scaled_avg = avg_ns / 1.0; unit = "ns";
        }

        std::printf("Benchmark average over %llu iterations: %.2f%s\n\n", iterations, scaled_avg, unit);

        if (exec_time < 1'000'000'000) {
            // Aim for ~10s total execution time based on the current average
            u64 suggested_iter = 10'000'000'000ull / u64(avg_ns);
            if (suggested_iter > 100) {
                // Make it a bit less oddly specific..
                u64 precision = std::pow(10, std::round(std::log10(suggested_iter)));
                suggested_iter = static_cast<u64>(std::round(4*suggested_iter / precision) / 4 * precision);
            }

            std::printf("Warning: Low execution time might result in inaccurate benchmark results.\n");
            std::printf("Try increasing iteration count with --bench-iterations.\n");
            std::printf("Suggestion: --bench-iterations=%llu\n", suggested_iter);
        }
    }
}

bool create_runtime(Program program, Runtime &out) {
    out.memory.clear();
    out.memory.resize(program.data_section_bytes);

    for (const auto &constant : program.constants) {
        out.memory[constant.address] = constant.address;
    }

    out.instructions = program.instructions;

    // Because people will forget their `SVC PC, =HALT`s, understandably,
    // make sure the program actually terminates. And then nag about it :)
    out.instructions.push_back(Instruction {
        .opcode = InstructionType::HALT,
        .reg_info_bits = 0,
        .address = 0
    });

    return true;
}
