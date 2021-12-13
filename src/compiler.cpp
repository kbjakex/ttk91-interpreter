#include "compiler.hpp"

#include <iostream>
#include <iomanip>
#include <cstring>
#include <charconv>

#include "tsl/robin_map.h"

#include "types.hpp"

/* static void print(std::string_view str) {
    std::cout << std::quoted(str) << "\n";
} */

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
    
    std::vector<Instruction> instructions;
    std::vector<UnresolvedJump> unresolved_jumps;
};

/* static bool pop_word(CompilerCtx &ctx, std::string_view &out) {
    return pop_word(ctx.line, out);   
} */

namespace InstructionParserFns {
    namespace Detail {
        static Instruction invalid_instruction() {
            // Address mode has two bits, which can store up to 4 values. Only 3 of them are valid,
            // so 0xFF is never a valid configuration.
            return Instruction { .opcode = InstructionType::NOP, .reg_info_bits = 0xFF, .address = 0x0 };
        }

        static Instruction make_instr(InstructionType type, u32 dst_reg, u32 src_reg, u32 address_mode, i16 address) {
            if (dst_reg > 7 || src_reg > 7 || address_mode > 2) {
                std::printf("make_instr: src_reg: %d, dst_reg: %d, address_mode: %d\n", src_reg, dst_reg, address_mode);
                return invalid_instruction();
            }

            return Instruction {
                .opcode = type,
                .reg_info_bits = static_cast<u8>((src_reg << 5) | (address_mode << 3) | dst_reg),
                .address = address
            };
        }

        // Helper fns
        static bool resolve_symbol(std::string_view str, CompilerCtx &ctx, i16 &out) {
            auto &table = ctx.sym_table.symbols;
        
            if (auto it = table.find(str); it != table.end()) {
                out = it->second;
                return true;
            }
            return false;
        }

        static bool read_dst_src_strings(std::string_view line, std::string_view &dst_out, std::string_view &src_out) {
            // Bit of a dirty function, can't rely on pop_lowercase because the second part could be made up of
            // several space-separated parts.

            std::string_view first{};
            if (auto idx = line.find_first_of(','); idx != line.npos) {
                first = substring(line, 0, idx);
                line = substring(line, idx+1);
            } else {
                std::printf("Error: Reached EOF while parsing an instruction (needs more arguments?)\n");
                return false;
            }

            skip_spaces(line);

            dst_out = first;
            src_out = line;
            return true;
        }

        static bool try_parse_register(std::string_view &word, std::size_t &reg_len_out, u32 &out) {
            if (word.length() < 2) return false;

            std::size_t reg_len = 2;

            if (word[0] == 'r' && std::isdigit(word[1]) && word[1] <= '7') out = R0 + (word[1] - '0');
            else if (word.starts_with("pc")) out = Register::PC;
            else if (word.starts_with("fp")) out = Register::FP;
            else if (word.starts_with("sp")) out = Register::SP;
            else return false;

            reg_len_out = reg_len;
            return true;
        }

        static bool parse_register(std::string_view &word, u32 &out) {
            std::size_t reg_len{};
            if (!try_parse_register(word, reg_len, out)) {
                if (word.length() < 2) {
                    std::printf("Error: EOF while parsing register name\n");
                } else {
                    std::size_t end{};
                    while (end < word.length() && is_identifier_char(word[end])) end += 1;

                    std::printf("Error: Unknown register '%.*s'\n", (int)end, word.data());
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
        static bool parse_src_address_mode(std::string_view &str, CompilerCtx &ctx, u32 &src_out, u32 &addr_mode_out, i16 &addr_out) {
            AddressingMode addr_mode{};
            u32 src{};
            i16 address{};

            skip_spaces(str);

            // 1. Figure out the addressing mode
            if (str[0] == '=') addr_mode = AddressingMode::INDEXED_IMMEDIATE;
            else if (str[0] == '@') addr_mode = AddressingMode::INDEXED_INDIRECT;
            else addr_mode = AddressingMode::INDEXED_DIRECT;

            if (addr_mode != AddressingMode::INDEXED_DIRECT) {
                str = substring(str, 1); // Skip = or @
            }

            skip_spaces(str);
            if (str.empty()) {
                std::printf("Error: EOF while parsing register/value/address\n");
                return false;
            }

            // 2. Figure out if there's an index, a register, or a symbol
            bool found_register = false;
            bool found_symbol = false;
            if (std::isdigit(str[0])) {
                if (!parse_address_or_immediate(str, address))
                    return false; // error messages in parse_address_or_immediate
                
                skip_spaces(str);
            } else {
                // Symbol or register
                std::size_t sym_len = 0;
                while (sym_len < str.length() && str[sym_len] != '(' && !std::isspace(str[sym_len])) sym_len += 1;
                auto sym = substring(str, 0, sym_len);

                //std::printf("Sym or reg: '%.*s'\n", (int)sym_len, sym.data());

                if (std::size_t reg_len{}; try_parse_register(str, reg_len, src) && reg_len == sym_len) {
                    found_register = true;
                } else {
                    // Not an address, not a register -> must be a symbol
                    found_symbol = true;
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

                if (!parse_register(str, src)) {
                    return false; // error messages in parse_register
                }
                
                skip_spaces(str);
                if (str.empty() || str[0] != ')') {
                    std::printf("Error: Missing closing ) after register\n");
                    return false;
                }

                str = substring(str, 1); // consume ')'
            }
            else if (!str.empty()) {
                std::printf("Error: Expected EOL or comment after instruction, found '%.*s'\n", (int)str.length(), str.data());
                return false;
            }

            src_out = src;
            addr_mode_out = static_cast<u32>(addr_mode);
            addr_out = address;

            //std::printf("Addressing mode: %d\n", addr_mode_out);

            return true;
        }

        static Instruction make_common_instr(InstructionType type, std::string_view line, CompilerCtx &ctx) {
            std::string_view dst_unparsed{};
            std::string_view src_unparsed{};
            if (!read_dst_src_strings(line, dst_unparsed, src_unparsed)) {
                return invalid_instruction();
            }

            u32 dst_reg{};
            if (!parse_register(dst_unparsed, dst_reg)) {
                return invalid_instruction();
            } 
            
            u32 src_reg{};
            u32 addr_mode{};
            i16 address{};
            if (!parse_src_address_mode(src_unparsed, ctx, src_reg, addr_mode, address)) {
                return invalid_instruction();
            }

            return make_instr(type, dst_reg, src_reg, addr_mode, address);            
        }

        static bool try_resolve_label(std::string_view name, CompilerCtx &ctx, i16 &address_out) {
           auto &label_table = ctx.sym_table.labels;
            if (auto it = label_table.find(name); it != label_table.end()) {
                address_out = it->second;
                return true;
            }
            return false;
        }

        static Instruction make_jump_instr(InstructionType type, std::string_view param, u32 opt_reg, CompilerCtx &ctx) {
            i16 address{};
            if (try_resolve_label(param, ctx, address)) {
                return make_instr(type, opt_reg, 0, 0, address);
            }

            if (!is_integer(param)) {
                // Delayed resolve
                ctx.unresolved_jumps.push_back(UnresolvedJump {
                    .label_name = param,
                    .instruction_idx = u32(ctx.instructions.size()),
                });
            } else if (!parse_address_or_immediate(param, address)) {
                return invalid_instruction(); // error messages in parse_address_or_immediate
            }

            if (address < 0) {
                std::printf("Error: Jump address cannot be negative (got '%d')\n", (int)address);
                return invalid_instruction();
            }

            return make_instr(type, opt_reg, 0, 0, address);
        }

        // Category 1: instead of looking at the state register, these jump instructions
        // have a register parameter and act according to the value stored there.
        static Instruction parse_jump1_instr(InstructionType type, std::string_view line, CompilerCtx &ctx) {
            std::string_view reg_str{};
            std::string_view dst_str{}; // destination address/label
            if (!read_dst_src_strings(line, reg_str, dst_str)) {
                return invalid_instruction();
            }

            u32 reg{};
            if (!parse_register(reg_str, reg)) {
                return invalid_instruction();
            }

            return make_jump_instr(type, dst_str, reg, ctx);
        }

        static Instruction parse_jump2_instr(InstructionType type, std::string_view line, CompilerCtx &ctx) {
            // Category 2: a `comp` instruction is required beforehands, and so there is no
            // register parameter.
            
            // The address/label is a single word, so pop it:
            std::string_view param{};
            if (!pop_word(line, param)) {
                std::printf("Error: Jump instruction missing target address\n");
                return invalid_instruction();
            }

            return make_jump_instr(type, param, 0, ctx);
        }

        static Instruction parse_svc(InstructionType, std::string_view line, CompilerCtx &ctx) {
            std::string_view reg_str{};
            std::string_view dst_str{}; // destination address/label
            if (!read_dst_src_strings(line, reg_str, dst_str)) {
                return invalid_instruction();
            }

            u32 reg{};
            if (!parse_register(reg_str, reg)) {
                return invalid_instruction();
            }

            if (dst_str == "=halt") {
                return make_instr(InstructionType::HALT, reg, 0, 0, 0);
            }

            return make_jump_instr(InstructionType::SVC, dst_str, reg, ctx);
        }

        static Instruction parse_nop(InstructionType, std::string_view, CompilerCtx&) {
            return make_instr(InstructionType::NOP, 0, 0, 0, 0);
        }

        static Instruction parse_in(InstructionType, std::string_view line, CompilerCtx&) {
            std::string_view reg_str{};
            std::string_view dst_str{};
            if (!read_dst_src_strings(line, reg_str, dst_str)) {
                return invalid_instruction();
            }

            u32 reg{};
            if (!parse_register(reg_str, reg)) {
                return invalid_instruction();
            }

            if (dst_str != "=kbd") { // Change this when more devices are added
                std::printf("Error: Unrecognized device for IN: '%.*s', expected '=KBD'\n", (int)dst_str.length(), dst_str.data());
                return invalid_instruction();
            }

            return make_instr(InstructionType::IN, KBD, reg, 0, 0);
        }

        static Instruction parse_out(InstructionType, std::string_view line, CompilerCtx&) {
            std::string_view reg_str{};
            std::string_view dst_str{};
            if (!read_dst_src_strings(line, reg_str, dst_str)) {
                return invalid_instruction();
            }

            u32 reg{};
            if (!parse_register(reg_str, reg)) {
                return invalid_instruction();
            }

            if (dst_str != "=crt") { // Change this when more devices are added
                std::printf("Error: Unrecognized device for OUT: '%.*s', expected '=CRT'\n", (int)dst_str.length(), dst_str.data());
                return invalid_instruction();
            }

            return make_instr(InstructionType::OUT, CRT, reg, 0, 0);
        }

        static Instruction parse_pushr_popr(InstructionType type, std::string_view line, CompilerCtx&) {
            // Single register parameter
            u32 reg{};
            if (!parse_register(line, reg)) {
                return invalid_instruction();
            }

            return make_instr(type, reg, 0, 0, 0);
        }
    }

    class Parser {
        using ParserFn = Instruction(InstructionType, std::string_view line, CompilerCtx&);
    public:
        Parser(InstructionType type, ParserFn *fn) : type(type), fn_ptr(fn) {}

        Instruction operator()(std::string_view line, CompilerCtx &ctx) const {
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

            { "nop",    S(NOP, parse_nop) },
            
            { "store",  C(STORE)    },
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
            { "not",    C(NOT)      },
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

            { "call",   J1(CALL)    },
            { "exit",   C(EXIT)     },
            { "push",   C(PUSH)     },
            { "pop",    C(POP)      },
            { "pushr",  S(PUSHR, parse_pushr_popr) },
            { "popr",   S(POPR, parse_pushr_popr)  },

            { "svc",    S(SVC, parse_svc) },
            { "iret",   C(IRET)     }, // NOT officially part of the language

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

    //std::printf("'%.*s'\t<- '%d'\n", (int) name.length(), name.data(), value);

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
    //std::printf("\nParsing line \"%.*s\"\n", (int)line.length(), line.data());

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
        Instruction instruction = (it->second)(line, ctx);

        // Test for and skip invalid instructions
        if (instruction.reg_info_bits == 0xFF) {
            std::printf("Invalid instruction!\n");
            return false;
        }

        ctx.instructions.push_back(instruction);
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
        Instruction &instruction = ctx.instructions[entry.instruction_idx];
        
        if (auto it = labels.find(entry.label_name); it != labels.end()) {
            instruction.address = it->second;
        } else {
            auto &label = entry.label_name;
            std::printf("Error: Label '%.*s' not found\n", (int) label.length(), label.data());
            num_errors += 1;
        }
    }
    ctx.unresolved_jumps.clear();
    return num_errors;
}

bool Compiler::compile(std::string_view src, Program &out) {
    auto ctx = CompilerCtx {};
    auto &parsers = InstructionParserFns::table();

    // Pseudoinstructions must be at the top, parse them first
    while (parse_pseudoinstruction(ctx, src)) {}

    u32 num_errors{};

    std::string_view line{};
    while (pop_line(src, line)) {
        if (!parse_line(line, ctx, parsers)) {
            num_errors += 1;
        }
    }

    num_errors += resolve_jumps(ctx);

    if (num_errors > 0) {
        std::printf("Found %d error%s, aborting\n", num_errors, num_errors == 1 ? "" : "s");
        return false;
    }

    out.instructions = ctx.instructions;
    out.constants = ctx.sym_table.values;
    out.data_section_bytes = ctx.sym_table.total_num_bytes;

    return true;
}
