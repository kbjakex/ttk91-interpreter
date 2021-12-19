#pragma once

#include <optional>
#include <vector>
#include <string_view>
#include <cstring> // std::strlen()
#include <unordered_map>
#include <unordered_set>
#include <stdexcept> // std::invalid_argument
#include <iostream>
#include <cstdlib> // std::exit()
#include <charconv> // std::from_chars()
#include <type_traits> // std::is_same_v

#include "types.hpp"

namespace Detail {
    bool equals_lowercase(std::string_view lhs, std::string_view rhs) {
        if (lhs.length() != rhs.length()) return false;

        for (std::size_t i = 0; i < lhs.length(); ++i) {
            if (std::tolower(lhs[i]) != std::tolower(rhs[i])) return false;
        }
        return true;
    }

    template<typename T>
    bool parse_num_to(std::string_view string, T *out) {
        T temp;
        const auto result = std::from_chars(string.data(), string.data() + string.length(), temp);

        if (result.ec == std::errc::invalid_argument || result.ec == std::errc::result_out_of_range) {
            return false;
        }

        *out = temp;
        return true;
    }
}

namespace ArgParsers {
    template<typename T>
    bool parse_to(std::string_view string, T *out);

    // Signed ints
    template<>
    bool parse_to<i8>(std::string_view string, i8 *out) { return Detail::parse_num_to(string, out); }

    template<>
    bool parse_to<i16>(std::string_view string, i16 *out) { return Detail::parse_num_to(string, out); }

    template<>
    bool parse_to<i32>(std::string_view string, i32 *out) { return Detail::parse_num_to(string, out); }

    template<>
    bool parse_to<i64>(std::string_view string, i64 *out) { return Detail::parse_num_to(string, out); }

    // Unsigned ints
    template<>
    bool parse_to<u8>(std::string_view string, u8 *out) { return Detail::parse_num_to(string, out); }

    template<>
    bool parse_to<u16>(std::string_view string, u16 *out) { return Detail::parse_num_to(string, out); }

    template<>
    bool parse_to<u32>(std::string_view string, u32 *out) { return Detail::parse_num_to(string, out); }

    template<>
    bool parse_to<u64>(std::string_view string, u64 *out) { return Detail::parse_num_to(string, out); }

    template<>
    bool parse_to<bool>(std::string_view string, bool *out) {
        using namespace Detail;

        if (string == "0" || equals_lowercase(string, "false")) {
            *out = false;
            return true;
        }

        if (string == "1" || equals_lowercase(string, "true")) {
            *out = true;
            return true;
        }

        return false;
    }

    template<>
    bool parse_to<std::string_view>(std::string_view string, std::string_view *out) {
        *out = string;
        return true;
    }
}

class Args {
    using CallbackFn = void(void*, std::string_view, std::string_view, std::optional<std::string_view>);
    struct Arg {
        void *out_ptr;
        bool is_flag;
        std::optional<std::string_view> error_msg;
        CallbackFn *callback;
    };
public:
    struct ParseResult {
        std::vector<std::string_view> remaining_args;
        std::vector<std::string_view> unrecognized_options;
    };

    static Args parser() {
        return Args();
    }

    template<typename T>
    Args &add_arg(std::string_view short_form, std::string_view long_form, T &out, std::optional<std::string_view> error_msg) {
        if (short_form.empty() && long_form.empty()) {
            throw std::invalid_argument("can't have both short and long forms of an argument empty");
        }

        auto arg = Arg {
            .out_ptr = &out,
            .is_flag = std::is_same_v<T, bool>,
            .error_msg = error_msg,
            .callback = [](void *raw_out_ptr, std::string_view option, std::string_view val, std::optional<std::string_view> error_msg) {
                T *out_ptr = static_cast<T*>(raw_out_ptr);
                if (!ArgParsers::parse_to(val, out_ptr)) {
                    if (error_msg)  std::cout << error_msg.value() << "\n";
                    else std::cout << "Failed to parse value \"" << val << "\" for option \"" << option << "\"\n";

                    std::exit(0);
                }
            }
        };

        if (!short_form.empty()) arg_map.insert({ short_form, arg });
        if (!long_form.empty()) arg_map.insert({ long_form, arg });

        return *this;
    }

    template<typename T>
    Args &add_arg(std::string_view short_form, std::string_view long_form, T &out) {
        return add_arg(short_form, long_form, out, std::nullopt);
    }

    template<typename T>
    Args &add_arg(std::string_view long_form, T &out, std::optional<std::string_view> error_msg) {
        return add_arg("", long_form, out, error_msg);
    }

    template<typename T>
    Args &add_arg(std::string_view long_form, T &out) {
        return add_arg(long_form, out, std::nullopt);
    }

    ParseResult parse(std::size_t argc, char **argv) {
        auto result = ParseResult{};
        if (argc == 1) return result;

        std::string_view input = to_str_view(argv[1]);

        // Skip 0 (application name)
        for (std::size_t i = 1; i < argc; ++i) {
            if (input == "--") {
                // rest are positional args and not parsed 
                for (std::size_t j = i + 1; j < argc; ++j) {
                    result.remaining_args.push_back(to_str_view(argv[j]));
                }
                break;
            }

            const auto next = to_str_view(argv[i + 1]);
            auto next_opt = next.empty() ? std::nullopt : std::optional{ next }; // Empty if i + 1 == argc

            parse_input(input, next_opt, result);

            if (!next_opt) {
                // Either 'input' (at i) was the last one, or there was a next one (at i+1) which was consumed by parse_input.
                // Next index of interest is i+2. 
                if (i + 2 >= argc) break;

                input = to_str_view(argv[i + 2]);

                i += 1; // The loop body will increment it again
                continue;
            }

            input = next;
        }

        return result;
    }

private:
    static std::string_view to_str_view(const char *c_str) {
        if (!c_str) return {};
        return std::string_view(c_str, std::strlen(c_str));
    }

    void parse_input(std::string_view input, std::optional<std::string_view> &next, ParseResult &result) {
        if (!input.starts_with("-")) {
            result.remaining_args.push_back(input);
            return;
        }
        input = input.substr(1);

        bool long_form = false;
        if (input.starts_with("-")) {
            long_form = true;
            input = input.substr(1);
        }

        if (long_form) {
            // --flag, --option, --option="true", --option "true", --option=true
            parse_option(input, next, result);
        } else {
            // -v, -O, -fgh, -v="1", -v "1", -v=1
            // Also allows forms such as `-wall
            // Every letter can be a flag, so check all
            parse_short_options(input, next, result);
        }
    }

    void parse_short_options(std::string_view option, std::optional<std::string_view> &next, ParseResult &result) {
        if (const auto idx = option.find_first_of('='); idx != option.npos || next.has_value()) {
            parse_option(option, next, result);
            return; 
        }

        for (std::size_t i = 0; i < option.length(); ++i) {
            parse_option(option.substr(i, 1), std::nullopt, result);
        }
    }

    void parse_option(std::string_view option, std::optional<std::string_view> next, ParseResult &result) {
        auto value = std::string_view{};
        if (const auto idx = option.find_first_of('='); idx != option.npos) {
            value = option.substr(idx + 1);
            option = option.substr(0, idx);
        }

        bool has_value = !value.empty(); // Note consequences: `v=` is considered to have no value.

        if (auto it = arg_map.find(option); it != arg_map.end()) {
            const Arg &mapping = it->second;

            if (!has_value) {
                if (mapping.is_flag) value = "1";
                else if (next.has_value()) { 
                    value = next.value();
                    next.reset();
                }
            }

            (mapping.callback)(mapping.out_ptr, option, value, mapping.error_msg);
            return;    
        }

        result.unrecognized_options.push_back(option);
    }

    std::unordered_map<std::string_view, Arg> arg_map;
};
