#include "compiler.hpp"

#include <iostream>
#include <iomanip>
#include <cstring>
#include <charconv>
#include <cstdarg>

#include "tsl/robin_map.h"

#include "types.hpp"
#include "options.hpp"

static void print(std::string_view str) {
    std::cout << std::quoted(str) << "\n";
}

// str.substr() does bounds checks that are redundant here
static std::string_view substring(std::string_view str, std::size_t start, std::size_t end) {
    return std::string_view { str.data() + start, end - start };
}

static std::string_view substring(std::string_view str, std::size_t start) {
    return std::string_view { str.data() + start, str.length() - start };
}

static void skip_spaces(std::string_view &str) {
    while (!str.empty() && std::isspace(str[0])) str = substring(str, 1);
}

static void skip_to_next_line(std::string_view &str) {
    while (!str.empty() && str[0] != '\n') str = substring(str, 1);

    if (!str.empty()) str = substring(str, 1);
}

static bool is_identifier_char(char c) {
    // Allow a-zA-Z0-9
    return std::isalnum(c);
}

static bool is_integer(std::string_view str) {
    if (str.starts_with('-')) str = substring(str, 1);

    for (char c : str) {
        if (!std::isdigit(c)) return false;
    }
    return true;
}

static bool pop_word(std::string_view &str, std::string_view &out) {
    while (true) {
        skip_spaces(str);

        if (str.starts_with(';')) {
            skip_to_next_line(str);
            continue;
        }

        if (str.empty()) return false;
        break;
    }

    for (std::size_t i = 0; i < str.length(); ++i) {
        const char c = str[i];

        if (std::isspace(c)) {
            out = substring(str, 0, i);
            str = substring(str, i + 1);
            return true;
        }

        if (c == ';') {
            out = substring(str, 0, i);
            str = substring(str, i);
            return true;
        }
    }

    if (str.empty()) return false;

    out = str;
    str = {};
    return true;
}

static bool pop_line(std::string_view &str, std::string_view &out) {
    // Skip spaces and empty lines (note std::isspace('\n') == true)
    while (!str.empty()) {
        skip_spaces(str);

        if (str.empty()) return false;

        std::size_t newline = str.find_first_of('\n');
        if (newline == str.npos) newline = str.length();

        std::string_view line = substring(str, 0, newline);
        str = substring(str, newline + 1);

        std::size_t comment = line.find_first_of(';');
        if (comment != str.npos) {
            line = substring(line, 0, comment);
        }

        while (!line.empty() && std::isspace(line.back())) line = substring(line, 0, line.length() - 1);

        if (line.empty()) continue;

        out = line;
        return true;
    }
    return false;
}

enum Extensions : u32 {
    FLOAT_ARITHMETIC    = 1 << 0, // Support for addf, subf, sqrtf et al
    CHAR_CRT            = 1 << 2, // Support for `OUT RX, =CCRT` to print characters
    BIN_HEX_LITERALS    = 1 << 3, // Support for literals in binary and hex (0b110, 0xFF)
    CHAR_KBD            = 1 << 4, // Support for `IN, RX, =CKBD` for character input, and
                                  // `IN, RX, =CKBD_NIO` for non-blocking character input
    FOUR_BYTE_LITERALS  = 1 << 5, // Support for 4-byte integer literals embedded directly in instructions
    FLOAT_LITERALS      = 1 << 6, // Support for `LOAD RX, 3.1415916` (4 bytes even if above ext is not enabled)
};

// Variables must be declared before code, but that is not possible for jumps.
// Because of this, the jump address for jumps to the future need to be
// resolved in a second pass. 
// Instruction index tells which instruction in the "instructions" vector needs resolving.
struct UnresolvedJump {
    std::string_view label_name;
    u32 instruction_idx;
};

// All pseudocommands get a value in the table:
// - Labels get an address (to jump to)
// - DC/DS get an address (to where the data is)
// - EQUs get a value
struct SymbolTable {
    tsl::robin_map<std::string_view, i32> symbols;
    tsl::robin_map<std::string_view, i16> labels;
    std::vector<DataConstant> values;
    i32 total_num_bytes;
};

struct CompilerCtx {
    SymbolTable sym_table;
    std::vector<u32> instructions;
    std::vector<UnresolvedJump> unresolved_jumps;

    u32 current_line_num;
    u32 line_start_idx; // Index (to source_code) of the first character of the line
    std::string_view current_line; // current (lower case) line
    std::string_view source_code; // Full, original source code for error reporting
    std::string_view file_name;

    u32 extension_bits;
};

class Message {
public:
    static constexpr u32 NO_CARET = -1;

    Message(CompilerCtx &ctx) : ctx(ctx) {}

    Message &for_code(std::string_view code) { this->code = code; return *this; }
    Message &with_caret(i32 pos) { this->caret = pos; return *this; }
    Message &with_hint(std::string_view hint) { this->hint = hint; return *this; }

    Message &printf(const char* format...) {
        u32 line = ctx.current_line_num;
        u32 column = u32(code.data() - ctx.current_line.data());

        std::string_view too_much = ctx.source_code.substr(ctx.line_start_idx);
        std::string_view line_str{};
        pop_line(too_much, line_str); // Extract the current line (without comments)

        std::printf("%s:%d:%d:\n", ctx.file_name.data(), line, column);
        va_list args;
        va_start(args, format);
        std::vprintf(format, args);
        va_end(args);
        std::printf("\n");


        char buf[64];
        for (int i = 0; i < 64; ++i) buf[i] = ' ';
        
        i32 start = i32(code.data() - ctx.current_line.data());
        i32 end = start + i32(code.length());
        if (!code.empty()) {
            for (i32 i = start; i < end; ++i) {
                buf[i] = '~';
            }
        }
        
        if (caret != -1) buf[start + caret] = '^';
        
        buf[63] = '\0';

        std::printf(
            "     |\n"
            "%4d | %.*s\n"
            "     | %.*s ",
            line, (int)line_str.length(), line_str.data(),
            end, buf
        );
        if (!hint.empty()) {
            std::printf("(%.*s)", (int)hint.length(), hint.data());
        }
        std::printf("\n\n");
        return *this;
    }

    Message& extra(const char* format...) {
        va_list args;
        va_start(args, format);
        std::vprintf(format, args);
        va_end(args);
        std::putchar('\n');
        return *this;
    }

private:
    CompilerCtx &ctx;
    std::string_view code;
    std::string_view hint = "";
    i32 caret = NO_CARET;
};

// 'code' is the bit of source code to underline
// Welcome to the ugliest piece of code in the codebase
static void show_message(CompilerCtx &ctx, i32 caret_pos, std::string_view code, std::string_view hint, const char *format...) {
    
}

namespace InstructionParserFns {
    namespace Detail {
        static void add_instruction(CompilerCtx &ctx, InstructionType type, Register dst, Register src, AddressMode addrm, i16 offset) {
            if (type != InstructionType::STORE && src == Register::R0) {
                src = Register::EXT_ZR;
            }
         
            ctx.instructions.push_back(
                encode_opcode(type)
                | encode_dst(dst)
                | encode_src(src)
                | encode_addrm(addrm)
                | encode_value(offset)
            );
        }

        static void add_instruction(CompilerCtx &ctx, InstructionType type, Register reg, i16 value) {
            add_instruction(ctx, type, reg, Register::R0, AddressMode::IMMEDIATE, value);
        }

        static void add_instruction(CompilerCtx &ctx, InstructionType type, Register reg) {
            add_instruction(ctx, type, reg, Register::R0, AddressMode::IMMEDIATE, 0);
        }

        static bool resolve_symbol(std::string_view str, CompilerCtx &ctx, i16 &out) {
            auto &table = ctx.sym_table.symbols;
        
            if (auto it = table.find(str); it != table.end()) {
                out = it->second;
                return true;
            }
            return false;
        }

        static bool read_dst_src_strings(CompilerCtx &ctx, std::string_view line, std::string_view &dst_out, std::string_view &src_out) {
            // Bit of a dirty function, can't rely on pop_lowercase because the second part could be made up of
            // several space-separated parts.
            if (line.empty()) {
                Message{ctx}
                    .for_code(line)
                    .printf("Error: Expected two arguments, found none:");
                return false;
            }

            std::string_view first{};
            if (auto idx = line.find_first_of(','); idx != line.npos) {
                first = substring(line, 0, idx);
                line = substring(line, idx+1);
            } else {
                i32 caret_pos = Message::NO_CARET;
                for (std::size_t i = 0; i < line.length(); ++i) {
                    if (!is_identifier_char(line[i])) {
                        caret_pos = i32(i);
                        break;
                    }
                }

                if (caret_pos == Message::NO_CARET) {
                    Message{ctx}
                        .for_code(line)
                        .printf("Error: Expected two arguments, found one:");
                } else {
                    Message{ctx}
                        .for_code(line)
                        .with_caret(caret_pos)
                        .with_hint("Add a comma here")
                        .printf("Error: No comma (,) found in an instruction that expects multiple parameters.");
                }

                return false;
            }

            skip_spaces(line);

            dst_out = first;
            src_out = line;
            return true;
        }

        static bool try_parse_register(CompilerCtx &ctx, std::string_view &word, std::size_t &reg_len_out, Register &out) {
            if (word.length() < 2) return false;

            std::size_t reg_len = 2;

            if (word[0] == 'r' && std::isdigit(word[1]) && word[1] <= '7') {
                out = Register(u32(Register::R0) + (word[1] - '0'));

                if (Warnings::reserved_registers && out > Register::R5) {
                    Message{ctx}
                        .for_code(word)
                        .printf(
                            "Warning: Register R%c is not general-purpose (equivalent to %s)", 
                            word[1], out == Register::R6 ? "SP" : "FP")
                        .extra("Warning: [Suppress with -Wreserved-registers]");
                }
            }
            else if (word.starts_with("fp")) out = Register::FP;
            else if (word.starts_with("sp")) out = Register::SP;
            else return false;

            reg_len_out = reg_len;
            return true;
        }

        static bool parse_register(CompilerCtx &ctx, std::string_view &word, Register &out) {
            std::size_t reg_len{};
            if (!try_parse_register(ctx, word, reg_len, out)) {
                if (word.length() < 2) {
                    Message{ctx}
                        .for_code(word)
                        .printf("Error: EOF while parsing register name");
                } else {
                    std::size_t end{};
                    while (end < word.length() && is_identifier_char(word[end])) end += 1;

                    Message{ctx}
                        .for_code(word.substr(end))
                        .printf("Error: Unknown register '%.*s'\n", (int)end, word.data());
                }
                return false;
            }
            
            word = substring(word, reg_len);
            return true;
        }

        // Should only be called if at least the first character is a digit
        // Also note: not a general-purpose function, this is used specifically to
        // parse an index or an immediate value in the second operand.
        static bool parse_address_or_immediate(std::string_view &str_in_out, i16 &out) {
            // First find the length, and check that there are no unexpected characters.
            // Should end in whitespace or a (.
            std::size_t length = 0;
            while (length < str_in_out.length() && std::isdigit(str_in_out[length])) length += 1;

            if (length < str_in_out.length() && !std::isspace(str_in_out[length]) && str_in_out[length] != '(') {
                std::printf("Error: Unexpected token '%c' while parsing a value/address\n", str_in_out[length]);
                return false;
            }

            auto result = std::from_chars(str_in_out.data(), str_in_out.data() + length, out);
            if (result.ec == std::errc::result_out_of_range) {
                std::printf("Error: Integer %.*s is larger than the maximum allowed value 32767\n", (int)length, str_in_out.data());
                return false;
            }

            if (result.ec == std::errc::invalid_argument) {
                std::printf("Error: Expected integer while parsing value/address, found %.*s\n", (int) length, str_in_out.data());
                return false;
            }

            str_in_out = substring(str_in_out, length);
            return true;
        }

        // Parse source register, addressing mode and address all at once,
        // because they are inherently related.
        // NOTE: "str" is expected to contain no newlines. As in, it should be a single line at most.
        static bool parse_src_address_mode(std::string_view &str, CompilerCtx &ctx, Register &src_out, AddressMode &addr_mode_out, i16 &addr_out) {
            AddressMode addr_mode{};
            Register src{};
            i16 address{};

            skip_spaces(str);

            std::printf("Second: '%.*s'\n", (int)str.length(), str.data());

            // 1. Figure out the addressing mode
            if (str[0] == '=') addr_mode = AddressMode::IMMEDIATE;
            else if (str[0] == '@') addr_mode = AddressMode::INDIRECT;
            else addr_mode = AddressMode::DIRECT; // Could also be REGISTER; figured out later

            if (addr_mode != AddressMode::DIRECT) {
                str = substring(str, 1); // Skip = or @
            }

            skip_spaces(str);
            if (str.empty()) {
                std::printf("Error: EOF while parsing register/value/address\n");
                return false;
            }

            // 2. Figure out if there's an index, a register, or a symbol
            bool found_register = false;
            if (std::isdigit(str[0])) {
                if (!parse_address_or_immediate(str, address))
                    return false; // error messages in parse_address_or_immediate
                
                skip_spaces(str);
            } else {
                std::printf("Here\n");
                // Symbol or register
                std::size_t sym_len = 0;
                while (sym_len < str.length() && str[sym_len] != '(' && !std::isspace(str[sym_len])) sym_len += 1;
                auto sym = substring(str, 0, sym_len);

                //std::printf("Sym or reg: '%.*s'\n", (int)sym_len, sym.data());

                if (std::size_t reg_len{}; try_parse_register(ctx, str, reg_len, src) && reg_len == sym_len) {
                    found_register = true;
                    if (addr_mode == AddressMode::IMMEDIATE) {
                        std::printf("Error: `=%.*s` is not a valid construct\n", (int)sym_len, sym.data());
                        return false;
                    }
                    else if (addr_mode == AddressMode::DIRECT) {
                        addr_mode = AddressMode::REGISTER; // `Load R1, R2` <=> `Load R1, =0(R2)`, no mem access
                    } else {
                        addr_mode = AddressMode::DIRECT; // `Load R1, @R2` <-> `Load R1, 0(R2)`, one mem access
                    }
                } else {
                    // Not an address, not a register -> must be a symbol
                    if (!resolve_symbol(sym, ctx, address)) {
                        std::printf("Error: Variable or symbol '%.*s' does not exist\n", (int)sym_len, sym.data());
                        return false;
                    }
                }

                if (str.length() == sym_len) str = {};
                else str = substring(str, sym_len);

                skip_spaces(str);
            }

            if (!found_register && !str.empty() && str[0] == '(') {
                str = substring(str, 1);
                skip_spaces(str);

                if (!parse_register(ctx, str, src)) {
                    return false; // error messages in parse_register
                }
                
                skip_spaces(str);
                if (str.empty() || str[0] != ')') {
                    std::printf("Error: Missing closing ) after register\n");
                    return false;
                }

                str = substring(str, 1); // consume ')'

                if (addr_mode == AddressMode::IMMEDIATE) {
                    addr_mode = AddressMode::REGISTER; // `Load R1, =2(R3)` -> `value = regValue(3) + 2`, no mem access
                }
            }
            else if (!str.empty()) {
                std::printf("Error: Expected EOL or comment after instruction, found '%.*s'\n", (int)str.length(), str.data());
                return false;
            }

            src_out = src;
            addr_mode_out = addr_mode;
            addr_out = address;

            //std::printf("Addressing mode: %d\n", addr_mode_out);

            return true;
        }

        static void make_common_instr(InstructionType type, std::string_view line, CompilerCtx &ctx) {
            std::string_view dst_unparsed{};
            std::string_view src_unparsed{};
            if (!read_dst_src_strings(ctx, line, dst_unparsed, src_unparsed)) {
                return;
            }

            //std::printf("dst/src:\n"); print(dst_unparsed); print(src_unparsed);

            Register dst{};
            if (!parse_register(ctx, dst_unparsed, dst)) {
                return;
            } 
            
            Register src{};
            AddressMode addr_mode{};
            i16 address{};
            if (!parse_src_address_mode(src_unparsed, ctx, src, addr_mode, address)) {
                return;
            }

            if (addr_mode == AddressMode::DIRECT && src == Register::R0 && address > ctx.sym_table.total_num_bytes) {
                std::printf("Warning: Address %d is out of bounds (symbol table size: %d).\n", (int)address, ctx.sym_table.total_num_bytes);
                std::printf("         Prefix with = to make it a literal: `=%d`\n", (int)address);
            }

            add_instruction(ctx, type, dst, src, addr_mode, address);            
        }

        static bool try_resolve_label(std::string_view name, CompilerCtx &ctx, i16 &address_out) {
           auto &label_table = ctx.sym_table.labels;
            if (auto it = label_table.find(name); it != label_table.end()) {
                address_out = it->second;
                return true;
            }
            return false;
        }

        static void make_jump_instr(InstructionType type, std::string_view param, Register opt_reg, CompilerCtx &ctx) {
            i16 address{};
            if (try_resolve_label(param, ctx, address)) {
                return add_instruction(ctx, type, opt_reg, address);
            }

            if (!is_integer(param)) {
                // Delayed resolve
                ctx.unresolved_jumps.push_back(UnresolvedJump {
                    .label_name = param,
                    .instruction_idx = u32(ctx.instructions.size()),
                });
            } else if (!parse_address_or_immediate(param, address)) {
                return; // error messages in parse_address_or_immediate
            }

            if (address < 0) {
                std::printf("Error: Jump address cannot be negative (got '%d')\n", (int)address);
                return;
            }

            add_instruction(ctx, type, opt_reg, address);
        }

        // Category 1: instead of looking at the state register, these jump instructions
        // have a register parameter and act according to the value stored there.
        static void parse_jump1_instr(InstructionType type, std::string_view line, CompilerCtx &ctx) {
            std::string_view reg_str{};
            std::string_view dst_str{}; // destination address/label
            if (!read_dst_src_strings(ctx, line, reg_str, dst_str)) {
                return;
            }

            Register reg{};
            if (!parse_register(ctx, reg_str, reg)) {
                return;
            }

            make_jump_instr(type, dst_str, reg, ctx);
        }

        static void parse_jump2_instr(InstructionType type, std::string_view line, CompilerCtx &ctx) {
            // Category 2: a `comp` instruction is required beforehands, and so there is no
            // register parameter.
            
            // The address/label is a single word, so pop it:
            std::string_view param{};
            if (!pop_word(line, param)) {
                std::printf("Error: Jump instruction missing target address\n");
                return;
            }

            make_jump_instr(type, param, Register::R0, ctx);
        }

        static void parse_exit(InstructionType, std::string_view line, CompilerCtx &ctx) {
            std::string_view reg_str{};
            std::string_view val_str{}; // destination address/label
            if (!read_dst_src_strings(ctx, line, reg_str, val_str)) {
                return;
            }

            Register reg{};
            if (!parse_register(ctx, reg_str, reg)) {
                return;
            }

            Register ignored{};
            AddressMode mode{};
            i16 address{};
            if (!parse_src_address_mode(val_str, ctx, ignored, mode, address)) {
                return;
            }

            if (mode != AddressMode::IMMEDIATE) {
                std::printf("Error: Exit called with non-immediate value\n");
                return;
            }

            add_instruction(ctx, InstructionType::EXIT, reg, address);
        }

        static void parse_svc(InstructionType, std::string_view line, CompilerCtx &ctx) {
            std::string_view reg_str{};
            std::string_view dst_str{}; // destination address/label
            if (!read_dst_src_strings(ctx, line, reg_str, dst_str)) {
                return;
            }

            Register reg{};
            if (!parse_register(ctx, reg_str, reg)) {
                return;
            }

            if (dst_str == "=halt") {
                return add_instruction(ctx, InstructionType::EXT_HALT, reg);
            }

            make_jump_instr(InstructionType::SVC, dst_str, reg, ctx);
        }

        static void parse_nop(InstructionType, std::string_view, CompilerCtx &ctx) {
            add_instruction(ctx, InstructionType::XOR, Register::EXT_ZR, 0);
        }

        static void parse_in(InstructionType, std::string_view line, CompilerCtx &ctx) {
            std::string_view reg_str{};
            std::string_view dst_str{};
            if (!read_dst_src_strings(ctx, line, reg_str, dst_str)) {
                return;
            }

            Register reg{};
            if (!parse_register(ctx, reg_str, reg)) {
                return;
            }

            if (dst_str != "=kbd") { // Change this when more devices are added
                std::printf("Error: Unrecognized device for IN: '%.*s', expected '=KBD'\n", (int)dst_str.length(), dst_str.data());
                return;
            }

            add_instruction(ctx, InstructionType::IN, reg, i16(InDevices::KBD));
        }

        static void parse_out(InstructionType, std::string_view line, CompilerCtx &ctx) {
            std::string_view reg_str{};
            std::string_view dst_str{};
            if (!read_dst_src_strings(ctx, line, reg_str, dst_str)) {
                return;
            }

            Register reg{};
            if (!parse_register(ctx, reg_str, reg)) {
                return;
            }

            if (dst_str != "=crt") { // Change this when more devices are added
                std::printf("Error: Unrecognized device for OUT: '%.*s', expected '=CRT'\n", (int)dst_str.length(), dst_str.data());
                return;
            }

            add_instruction(ctx, InstructionType::OUT, reg, i16(OutDevices::CRT));
        }

        static void parse_push(InstructionType type, std::string_view line, CompilerCtx &ctx) {
            std::string_view reg_str{};
            std::string_view dst_str{};
            if (!read_dst_src_strings(ctx, line, reg_str, dst_str)) {
                return;
            }
            
            Register reg{};
            if (!parse_register(ctx, reg_str, reg)) {
                return;
            }

            if (reg != Register::SP) {
                Message{ctx}
                    .for_code(reg_str)
                    .printf("Warning: %s used with register %s, should probably be SP (stack pointer)\n",
                        instruction_name(type).data(),
                        register_name(reg).data());
            }

            Register src{};
            AddressMode mode{};
            i16 address{};
            if (!parse_src_address_mode(dst_str, ctx, src, mode, address)) {
                return;
            }

            add_instruction(ctx, type, reg, src, mode, address);
        }

        static void parse_pop(InstructionType type, std::string_view line, CompilerCtx &ctx) {
            std::string_view reg_str{};
            std::string_view dst_str{};
            if (!read_dst_src_strings(ctx, line, reg_str, dst_str)) {
                return;
            }
            
            Register reg{};
            if (!parse_register(ctx, reg_str, reg)) {
                return;
            }

            if (reg != Register::SP) {
                Message{ctx}
                    .for_code(reg_str)
                    .printf("Warning: %s used with register %s, should probably be SP (stack pointer)\n",
                        instruction_name(type).data(),
                        register_name(reg).data());
            }

            Register dst{};
            if (!parse_register(ctx, dst_str, dst)) {
                return;
            }

            add_instruction(ctx, type, reg, dst, AddressMode::IMMEDIATE, 0);
        }

        static void parse_pushr_popr(InstructionType type, std::string_view line, CompilerCtx &ctx) {
            Register reg{}; // Ignored in execution, but should be valid in source code still
            if (!line.empty() && !parse_register(ctx, line, reg)) {
                return;
            }

            add_instruction(ctx, type, reg);
        }

        static void parse_store(InstructionType, std::string_view line, CompilerCtx& ctx) {
            std::string_view src_unparsed{};
            std::string_view dst_unparsed{};
            if (!read_dst_src_strings(ctx, line, src_unparsed, dst_unparsed)) {
                return;
            }

            Register src{};
            if (!parse_register(ctx, src_unparsed, src)) {
                return;
            } 
            
            Register dst{};
            AddressMode addr_mode{};
            i16 address{};
            if (!parse_src_address_mode(dst_unparsed, ctx, dst, addr_mode, address)) {
                return;
            }

            if (addr_mode == AddressMode::REGISTER || addr_mode == AddressMode::IMMEDIATE) {
                Message{ctx}
                    .for_code(dst_unparsed)
                    .printf("Second operand for STORE cannot be a register or constant");
                return;
            }
            // "Fix up" address mode because STORE is a bit special
            if (addr_mode == AddressMode::DIRECT) addr_mode = AddressMode::REGISTER;
            else if (addr_mode == AddressMode::INDIRECT) addr_mode = AddressMode::DIRECT;

            add_instruction(ctx, InstructionType::STORE, src, dst, addr_mode, address); 
        }

        static void parse_not(InstructionType, std::string_view line, CompilerCtx &ctx) {
            Register reg{};
            if (!parse_register(ctx, line, reg)) {
                return;
            }

            add_instruction(ctx, InstructionType::NOT, reg);
        }
    }

    class Parser {
        using ParserFn = void(InstructionType, std::string_view line, CompilerCtx&);
    public:
        Parser(InstructionType type, ParserFn *fn) : type(type), fn_ptr(fn) {}

        void operator()(std::string_view line, CompilerCtx &ctx) const {
            return (fn_ptr)(type, line, ctx);
        }
    private:
        InstructionType type;
        ParserFn *fn_ptr;
    };

    using ParserTable = tsl::robin_map<std::string_view, Parser>;

    static ParserTable &table() {
        static auto tbl = ParserTable {
            /*Special*/ #define S(_ty, _fn) Parser{ InstructionType::_ty, Detail::_fn }
            /*Common */ #define C(_ty) Parser{ InstructionType::_ty, Detail::make_common_instr }
            /*Jump 1 */ #define J1(_ty) Parser{ InstructionType::_ty, Detail::parse_jump1_instr }
            /*Jump 2 */ #define J2(_ty) Parser{ InstructionType::_ty, Detail::parse_jump2_instr }

            { "nop",    S(XOR, parse_nop) },
            
            { "store",  S(STORE, parse_store)    },
            { "load",   C(LOAD)     },
            { "in",     S(IN, parse_in)   },
            { "out",    S(OUT, parse_out) },
            
            { "add",    C(ADD)      },
            { "sub",    C(SUB)      },
            { "mul",    C(MUL)      },
            { "div",    C(DIV)      },
            { "mod",    C(MOD)      },
            
            { "and",    C(AND)      },
            { "or",     C(OR)       },
            { "xor",    C(XOR)      },
            { "shl",    C(SHL)      },
            { "shr",    C(SHR)      },
            { "not",    S(NOT, parse_not) },
            { "shra",   C(SHRA)     },
            
            { "comp",   C(COMP)     },

            { "jump",   J2(JUMP)    }, // J2!
            { "jneg",   J1(JNEG)    },
            { "jzer",   J1(JZER)    },
            { "jpos",   J1(JPOS)    },
            { "jnneg",  J1(JNNEG)   },
            { "jnzer",  J1(JNZER)   },
            { "jnpos",  J1(JNPOS)   },

            { "jles",   J2(JLES)    },
            { "jequ",   J2(JEQU)    },
            { "jgre",   J2(JGRE)    },
            { "jnles",  J2(JNLES)   },
            { "jnequ",  J2(JNEQU)   },
            { "jngre",  J2(JNGRE)   },

            { "call",   J2(CALL)    },
            { "exit",   S(EXIT, parse_exit)        },
            { "push",   S(PUSH, parse_push)    },
            { "pop",    S(POP, parse_pop)     },
            { "pushr",  S(PUSHR, parse_pushr_popr) },
            { "popr",   S(POPR, parse_pushr_popr)  },

            { "svc",    S(SVC, parse_svc) },
            { "iret",   C(EXT_IRET)       }, // NOT officially part of the language

            #undef S
            #undef C
            #undef J1
            #undef J2
        };

        return tbl;
    } 
}

static bool parse_pseudoinstruction(CompilerCtx &ctx, std::string_view &src) {
    // This function is very careful about modifying CompilerCtx and especially src
    // until the whole pseudoinstruction has been parsed, because it is hard to know in
    // advance whether it's actually a pseudoinstruction or not.

    skip_spaces(src); // this does modify src, but removing excess spaces is fine.

    std::size_t newline = src.find_first_of('\n');
    std::size_t eol = newline;
    if (auto idx = src.find_first_of(';'); idx != src.npos && idx < eol) { eol = idx; }
    if (eol == src.npos) { eol = src.length(); }

    std::string_view line = substring(src, 0, eol);

    if (line.empty()) {
        if (newline != src.npos) {
            src = substring(src, newline);
            return true;
        }
        return false;
    }

    std::string_view name{};
    std::string_view type_str{};
    std::string_view value_str{};
    if (!pop_word(line, name) || !pop_word(line, type_str) || !pop_word(line, value_str)) return false;

    if (type_str != "dc" && type_str != "ds" && type_str != "equ") return false;

    // Value is (or should be) an integer in all cases
    i32 value{};
    auto result = std::from_chars(value_str.data(), value_str.data() + value_str.length(), value);

    if (result.ec == std::errc::invalid_argument) {
        std::printf("Error: Invalid value for a pseudoinstruction: '%.*s'\n", (int)value_str.length(), value_str.data());
        return true; // Just because the value is invalid doesn't mean there are no more pseudoinstructions
    }
    if (result.ec == std::errc::result_out_of_range) {
        std::printf("Error: Pseudoinstruction value out of range: '%.*s'\n", (int)value_str.length(), value_str.data());
        return true;
    }

    if (type_str == "dc") {
        i32 temp = value;
        value = ctx.sym_table.total_num_bytes;
        ctx.sym_table.total_num_bytes += 4;
        ctx.sym_table.values.push_back(DataConstant{.address = value, .value = temp });
    } else if (type_str == "ds") {
        i32 temp = value;
        value = ctx.sym_table.total_num_bytes;
        ctx.sym_table.total_num_bytes += 4 * temp;
    }

    std::printf("'%.*s'\t<- '%d'\n", (int) name.length(), name.data(), value);

    if (!ctx.sym_table.symbols.try_emplace(name, value).second) {
        std::printf("Error: Duplicate symbol '%.*s'\n", (int)name.length(), name.data());
    }

    if (newline != src.npos) {
        src = substring(src, newline);
        return true;
    }
    // Yes, you only get here if the entire file is nothing but pseudoinstructions
    src = {};
    return false;
}

using ParserTable = InstructionParserFns::ParserTable;

// Returns false when error
static bool parse_line(std::string_view line, CompilerCtx &ctx, ParserTable &parsers) {
    std::printf("\nParsing line \"%.*s\"\n", (int)line.length(), line.data());

    std::string_view word{};
    if (!pop_word(line, word)) {
        return true; // A line cannot be empty, so if no word was found, there was just a comment
    }

    auto it = parsers.find(word);

    // Check for label
    if (it == parsers.end()) { // if not found in the table, it must be a label
        if (!ctx.sym_table.labels.try_emplace(word, i16(ctx.instructions.size())).second) {
            std::printf("Error: Duplicate label '%.*s'\n", (int)word.length(), word.data());
            return false;
        }

        if (!pop_word(line, word)) {
            std::printf("Error: Trailing label: '%.*s'\n", (int)word.length(), word.data());
            return false;
        }
        //print(word);
        it = parsers.find(word);
    }

    if (it != parsers.end()) {
        // See InstructionParserFns::table() for which function is executed
        (it->second)(line, ctx);
    } else {
        std::printf("Error: Unknown instruction '%.*s'\n", (int)word.length(), word.data());
        return false;
    }
    return true;
}

static u32 resolve_jumps(CompilerCtx &ctx) {
    u32 num_errors{};
    auto &labels = ctx.sym_table.labels;
    for (auto entry : ctx.unresolved_jumps) {
        u32 &instruction = ctx.instructions[entry.instruction_idx];
        
        if (auto it = labels.find(entry.label_name); it != labels.end()) {
            instruction &= ~encode_value(i16((1 << VALUE_BITS) - 1));
            instruction |= encode_value(it->second);
        } else {
            auto &label = entry.label_name;
            std::printf("Error: Label '%.*s' not found\n", (int) label.length(), label.data());
            num_errors += 1;
        }
    }
    ctx.unresolved_jumps.clear();
    return num_errors;
}

bool Compiler::compile(std::string_view file_name, std::string original_src, Program &out) {
    out.source_code = std::move(original_src);

    std::string lowercase = out.source_code; // Copy
    for (char &c : lowercase) c = std::tolower(c);

    auto src = std::string_view{ lowercase };
    auto ctx = CompilerCtx {};
    auto &parsers = InstructionParserFns::table();

    // Address 0 is unfortunately reserved for R0 with the system in place,
    // so this here is a nasty hack: initializing this to 1 shifts all variables
    // such that they start from address 1, leaving address 0 for R0.
    ctx.sym_table.total_num_bytes = 1;

    ctx.source_code = out.source_code; // view
    ctx.file_name = file_name; // view

    // Pseudoinstructions must be at the top, parse them first
    while (parse_pseudoinstruction(ctx, src)) {}

    u32 num_errors{};

    std::string_view line{};
    while (pop_line(src, line)) {
        print(line);

        ctx.current_line_num += 1;
        ctx.line_start_idx = u32(line.data() - lowercase.data());
        ctx.current_line = line;

        out.source_code_lines.push_back(out.source_code.substr(u64(ctx.line_start_idx), line.length()));

        if (!parse_line(line, ctx, parsers)) {
            num_errors += 1;
        }
    }

    num_errors += resolve_jumps(ctx);

    if (num_errors > 0) {
        std::printf("Found %d error%s, aborting\n", num_errors, num_errors == 1 ? "" : "s");
        return false;
    }

    // Because people will forget their `SVC SP, =HALT`s, understandably,
    // make sure the program actually terminates. And then nag about it :)
    InstructionParserFns::Detail::add_instruction(ctx, InstructionType::EXT_HALT, Register::SP);

    out.instructions = ctx.instructions;
    out.constants = ctx.sym_table.values;
    out.data_section_bytes = ctx.sym_table.total_num_bytes;

    return true;
}
