#include "interpreter.hpp"

#include <cstdio>
#include <chrono>
#include <iostream>
#include <cmath>

#define REG(_reg) mem[-u64(Register::_reg)]
#define DST_ADDR(_instruction) -u64(decode_dst(_instruction))
#define SRC_ADDR(_instruction) -u64(decode_src(_instruction))

__attribute__((noinline))
static void print_timings(u64 exec_time, u64 iterations);

__attribute__((noinline))
static void op_print(i32 *__restrict__ mem, u32 ins, i32 value) {
    std::printf("%d\n", mem[DST_ADDR(ins)]);

    (void)value;
}

__attribute__((noinline))
static void op_input(i32 *__restrict__ mem, u32 ins, i32 value) {
    std::printf("(Requesting input)\n> ");
    i32 input;
    std::cin >> input;
    mem[DST_ADDR(ins)] = input;

    (void)value;
}

bool execute(Runtime &rt, Options &opts) {
    const u32 const *instructions = rt.instructions.data();
          u32 const *pc           =   &instructions[0];

    i32 *__restrict__ mem = rt.memory.data() + u64(Register::NUM_REGISTERS);
    i32 *__restrict__ mem_end = rt.memory.data() + rt.memory.size();


    // Cut off a couple indices from both ends to make over/underflow checks easier/faster. 
    // Nobody cares about 4 slots anyways :)
    i32 stack_start_idx = rt.memory.size() - opts.stack_size + 8;
    i32 stack_end_idx = stack_start_idx + opts.stack_size - 16;

    i32 &sp = REG(SP); // stack pointer
    i32 &fp = REG(FP); // frame pointer
    sp = stack_start_idx;
    fp = stack_start_idx;

    i32 comp_result{};
    bool enable_printing = opts.bench_io || opts.benchmark_iterations == 1; // always print if not benchmarking

    // Compiler extension. Supported by GCC / Clang.
    // Produces FAR better code than a table of function pointers or a switch.
    // Kept in same order as the InstructionType enum.
    constexpr void* INS_JUMP_TABLE[] = {
        &&Lop_store,
        &&Lop_load,

        &&Lop_in, 
        &&Lop_out,

        &&Lop_add,  
        &&Lop_sub,  
        &&Lop_mul,  
        &&Lop_div,  
        &&Lop_mod, 

        &&Lop_fadd, 
        &&Lop_fsub, 
        &&Lop_fmul, 
        &&Lop_fdiv, 
        &&Lop_fmod, 

        &&Lop_itof, // int to float
        &&Lop_ftoi, // float to int

        &&Lop_sqrtf,
        &&Lop_rsqrtf, // 1/sqrt(x)
        &&Lop_sinf,
        &&Lop_cosf,
        &&Lop_tanf,
        &&Lop_asinf,
        &&Lop_acosf,
        &&Lop_atanf,
        &&Lop_log2f,
        &&Lop_log10f,
        &&Lop_lnf,
        &&Lop_absf,  

        &&Lop_and,  
        &&Lop_or,   
        &&Lop_xor,
        &&Lop_not,
        &&Lop_shl,  
        &&Lop_shr,  
        &&Lop_shra, 

        &&Lop_comp, 

        &&Lop_jump, 
        &&Lop_jneg, 
        &&Lop_jzer, 
        &&Lop_jpos, 
        &&Lop_jnneg, 
        &&Lop_jnzer, 
        &&Lop_jnpos,

        &&Lop_jles, 
        &&Lop_jequ, 
        &&Lop_jgre, 
        &&Lop_jnles, 
        &&Lop_jnequ, 
        &&Lop_jngre,

        &&Lop_call, 
        &&Lop_exit, 
        &&Lop_push, 
        &&Lop_pop, 
        &&Lop_pushr, 
        &&Lop_popr,

        &&Lop_svc, 
        &&Lop_iret,

        &&Lop_halt,

        // Fill remaining possible opcodes with error handling
        &&Leillegal_instruction, &&Leillegal_instruction,
        &&Leillegal_instruction, &&Leillegal_instruction,
        &&Leillegal_instruction, &&Leillegal_instruction
    };

    constexpr void* VAL_JUMP_TABLE[] = {
        &&Lload_immediate_val,
        &&Lload_register_val,
        &&Lload_direct_val,
        &&Lload_indirect_val,
    };

    auto start = std::chrono::steady_clock::now();

    u64 remaining_executions = opts.benchmark_iterations;
    std::printf("Running %llu iterations\n", remaining_executions);

    // Per cycle values
    i32 value{};
    i32 *value_ptr{};
    u32 opcode{};

Lstart:
    remaining_executions -= 1;
    pc = &instructions[0];

    u32 n_ins = 0;

    while (true) {
        //std::printf("\nFetching instruction\n");
        //std::printf("PC: %d. ", (int)(pc - &instructions[0]));
        n_ins += 1;
        u32 ins = *pc++;
        //debug_print(ins);
        // std::printf("(Opcode %d)\n", (int)decode_opcode(ins));
        opcode = decode_opcode(ins);
        auto op = INS_JUMP_TABLE[opcode];
        
        value = decode_value(ins);

        i32 &__restrict__ src = mem[SRC_ADDR(ins)];
        i32 &__restrict__ dst = mem[DST_ADDR(ins)];

        // std::printf("Decoding value (mode: %d)\n", (int)decode_addrm(ins));
        goto *VAL_JUMP_TABLE[decode_addrm(ins)];

        /*
    1   LOAD R1, =2         value = decode_imm(ins)
    2   LOAD R1, =2(R2)     value = memory[-decode_dst(ins)] + decode_imm(ins)

        (LOAD R1, R2        value = memory[memory[-decode_dst(ins)]]) <=>
    3   LOAD R1, 2(R2)      value = memory[memory[-decode_dst(ins)] + decode_imm(ins)]

    4   (LOAD R1, @R2       value = memory[memory[memory[-decode_dst(ins)]]]) <=>
        LOAD R1, @2(R2)     value = memory[memory[memory[-decode_dst(ins)] + decode_imm(ins)]]
        */

        Lload_immediate_val: // 0 memory accesses :)
        //std::printf("got immediate: %d\n", value);
        goto *op;

        Lload_register_val: // 1 *safe* memory access :I
        value += src;
        //std::printf("got from register: %d\n", value);
        goto *op;

        Lload_direct_val: // 2 accesses, 1 unsafe :(
        value_ptr = mem + u32(src + value);
        if (value_ptr >= mem_end) goto Leout_of_bounds;
        value = *value_ptr;
        //std::printf("got direct %d from address %d\n", value, (int)(value_ptr - mem));
        goto *op;

        Lload_indirect_val: // 3 accesses, 2 unsafe >:(
        value_ptr = mem + u32(src) + value;
        if (value_ptr >= mem_end) goto Leout_of_bounds;

        value_ptr = mem + *value_ptr;
        if (value_ptr >= mem_end) goto Leout_of_bounds;

        value = *value_ptr;
        //std::printf("got indirect: %d\n", value);
        goto *op;

        // OPERATIONS
        // Ordered very approximately from most important to least important

        Lop_load:
        //std::printf("Loaded value %d to address %lld\n", value, DST_ADDR(ins));
        dst = value;
        continue;

        Lop_store:
        //std::printf("Writing %d to address %d\n", dst, value);
        mem[value] = dst;
        continue;

        Lop_add: dst += value; continue;
        Lop_sub: dst -= value; continue;
        Lop_mul: dst *= value; continue;
        
        Lop_div:
        if (value == 0) goto Ledivision_by_zero;
        dst /= value;
        continue;
        
        Lop_mod: 
        if (value == 0) goto Ledivision_by_zero;
        dst %= value;
        continue;

        Lop_or:  dst |= value; continue;
        Lop_and: dst &= value; continue;
        Lop_xor: dst ^= value; continue;
        Lop_not: dst = ~dst; continue;
        Lop_shl: dst <<= value; continue;
        Lop_shr: dst = i32(u32(dst) >> value); continue; // same cost as shra once compiled
        Lop_shra: dst >>= value; continue;

        Lop_fadd: continue;
        Lop_fsub: continue;
        Lop_fmul: continue;
        Lop_fdiv: continue;
        Lop_fmod: continue;

        Lop_comp:
        comp_result = dst - value;
        //std::printf("Comp(%d, %d) result: %d\n", dst, value, comp_result);
        continue;

        Lop_jump:  
        if (value < 0) goto Lenegative_jump_address;
        pc = &instructions[0] + value;
        continue;

        Lop_jneg:
        if (value < 0) goto Lenegative_jump_address;
        if (dst < 0) pc = &instructions[0] + value;
        continue;
        
        Lop_jzer:  
        if (value < 0) goto Lenegative_jump_address;
        if (dst == 0) pc = &instructions[0] + value;
        continue;
        
        Lop_jpos:  
        if (value < 0) goto Lenegative_jump_address;
        //std::printf("JPOS: value %d, reg %d, reg value: %d\n", value, i32(DST_ADDR(ins)), dst);
        if (dst > 0) pc = &instructions[0] + value;
        continue;
        
        Lop_jnneg: 
        if (value < 0) goto Lenegative_jump_address;
        if (dst >= 0) pc = &instructions[0] + value; 
        continue;
        
        Lop_jnzer:  
        if (value < 0) goto Lenegative_jump_address;
        if (dst != 0) pc = &instructions[0] + value;
        continue;
        
        Lop_jnpos: 
        if (value < 0) goto Lenegative_jump_address;
        if (dst <= 0) pc = &instructions[0] + value;
        continue;

        Lop_jles:  
        if (value < 0) goto Lenegative_jump_address;
        if (comp_result < 0) pc = &instructions[0] + value;
        continue;
        
        Lop_jequ:  
        if (value < 0) goto Lenegative_jump_address;
        if (comp_result == 0) pc = &instructions[0] + value;
        continue;
        
        Lop_jgre:  
        if (value < 0) goto Lenegative_jump_address;
        if (comp_result > 0) pc = &instructions[0] + value;
        continue;
        
        Lop_jnles:  
        if (value < 0) goto Lenegative_jump_address;
        if (comp_result >= 0) pc = &instructions[0] + value;
        continue;
        
        Lop_jnequ:  
        if (value < 0) goto Lenegative_jump_address;
        if (comp_result != 0) pc = &instructions[0] + value;
        continue;
        
        Lop_jngre:
        if (value < 0) goto Lenegative_jump_address;
        if (comp_result >= 0) pc = &instructions[0] + value;
        continue;

        Lop_itof:   continue; // int to float 
        Lop_ftoi:   continue; // float to int

        Lop_sqrtf:  continue;
        Lop_rsqrtf: continue;
        Lop_sinf:   continue;
        Lop_cosf:   continue;
        Lop_tanf:   continue;
        Lop_asinf:  continue;
        Lop_acosf:  continue;
        Lop_atanf:  continue;
        Lop_log2f:  continue;
        Lop_log10f: continue;
        Lop_lnf:    continue;
        Lop_absf:   continue;

        Lop_call:
        if (sp >= stack_end_idx) goto Lestack_overflow;
        mem[++sp] = pc - &instructions[0]; // Store old PC
        mem[++sp] = fp;                    // Store old FP
        pc = &instructions[0] + value;
        //std::printf("FP %d -> %d\n", (int)(fp), (int)(sp));
        fp = sp;
        continue;

        Lop_exit: 
        fp = mem[sp--];
        pc = mem[sp--] + &instructions[0];
        //::printf("EXIT: fp: %d, pc: %d, value: %d\n", fp, (int)(pc - &instructions[0]), value);
        sp -= value;
        if (sp < stack_start_idx) goto Lestack_underflow;
        continue;

        Lop_push:  
        //std::printf("Pushing value %d\n", value);
        mem[++sp] = value;
        if (sp >= stack_end_idx) goto Lestack_overflow;
        continue;
        
        Lop_pop:
        if (sp < stack_start_idx) goto Lestack_underflow;
        //std::printf("Popping value %d to register %lld\n", mem[sp], i64(SRC_ADDR(ins)));
        src = mem[sp--];
        continue;

        Lop_pushr:  
        mem[++sp] = REG(R0);
        mem[++sp] = REG(R1);
        mem[++sp] = REG(R2);
        mem[++sp] = REG(R3);
        mem[++sp] = REG(R4);
        mem[++sp] = REG(R5);
        if (sp >= stack_end_idx) goto Lestack_overflow;
        continue;
        
        Lop_popr: 
        REG(R0) = mem[sp--];
        REG(R1) = mem[sp--];
        REG(R2) = mem[sp--];
        REG(R3) = mem[sp--];
        REG(R4) = mem[sp--];
        REG(R5) = mem[sp--];
        if (sp < stack_start_idx) goto Lestack_overflow;
        continue;

        Lop_in: 
        op_input(mem, ins, value); 
        continue;
        
        Lop_out: 
        if (enable_printing) op_print(mem, ins, value);
        continue;

        Lop_svc: continue;
        Lop_iret: continue;
    }

// Start of error handling spaghetti
Lenegative_jump_address:
    std::printf("Execution error: Jump address for instruction #%d was negative (%d)\n", (int)(pc - 1 - &instructions[0]), value);
    goto Lprint_faulty_instruction;

Lestack_underflow:
    std::printf("Execution error: Stack underflowed. Possible reasons: \n");
    std::printf("- Tried to use EXIT to terminate the program (Use `SVC SP, =HALT` instead)\n");
    std::printf("- The number of parameters EXIT was asked to clean up was too big\n");
    goto Lprint_faulty_instruction;

Lestack_overflow:
    std::printf("Execution error: Stack overflowed (recursion too deep?)\n");
    goto Lprint_faulty_instruction;

Leout_of_bounds:
    std::printf("Execution error: Instruction #%d (%s) accessed memory out of bounds\n",
        (int)(pc - 1 - &instructions[0]),
        instruction_name(InstructionType(decode_opcode(*(pc-1)))).data()
    );
    std::printf("Address %d\n", value);
    goto Lprint_faulty_instruction;

Ledivision_by_zero:
    std::printf("Execution error: Division by zero\n");
    goto Lprint_faulty_instruction;

Leillegal_instruction:
    std::printf("Execution error: Illegal instruction (opcode %d)\n", decode_opcode(*(pc-1)));
    goto Lprint_faulty_instruction;

Lprint_faulty_instruction:

    goto Lhalt_no_repeat;
// End of error handling spaghetti

Lop_halt:
    if (remaining_executions != 0) goto Lstart;

Lhalt_no_repeat:
    auto end = std::chrono::steady_clock::now();

    std::printf("Executed %d instructions\n", n_ins);

    // See create_runtime() at the bottom of this file. Execution should never reach that instruction.
    if (pc == rt.instructions.data() + rt.instructions.size()) {
        std::printf("Nag: no terminating instruction found. Perhaps you forgot the `SVC SP, =Halt`?\n");
    }

    auto elapsed = (end - start).count();
    print_timings(elapsed, opts.benchmark_iterations);

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
            std::printf("Suggestion for this program: --bench-iterations=%llu\n", suggested_iter);
        }
    }
}

bool create_runtime(Program &program, Runtime &out, Options &options) {
    // Initialize memory as described in interpreter.hpp
    // Registers have the lowest addresses, then comes program data,
    // and last the stack. Unconventional setup, but fits well here:
    // - No need to move around the addresses of constants
    // - No need for extra care for register access
    // - Stack still grows to higher addresses

    out.memory.clear();
    out.memory.resize(
        std::size_t(Register::NUM_REGISTERS)
        + program.data_section_bytes
        + options.stack_size
    );

    for (const auto &constant : program.constants) {
        out.memory[constant.address + std::size_t(Register::NUM_REGISTERS)] = constant.value;
        //std::printf("Wrote value %d to address %d\n", constant.value, constant.address);
    }

    out.instructions = program.instructions;
    return true;
}
