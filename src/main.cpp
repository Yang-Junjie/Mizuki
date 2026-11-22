#include "cli/commands.hpp"

#ifdef _WIN32
#include <iostream>
#include <string>
#include <utility>
#include <vector>
#include <windows.h>

// The Windows narrow entry point uses the system code page, which can lose
// Unicode paths and titles before the CLI gets a chance to parse them.
int wmain(int argc, wchar_t** argv) {
    std::vector<std::string> arguments;
    arguments.reserve(static_cast<std::size_t>(argc));
    for (int index = 0; index < argc; ++index) {
        auto const size = WideCharToMultiByte(CP_UTF8, WC_ERR_INVALID_CHARS, argv[index], -1,
                                              nullptr, 0, nullptr, nullptr);
        if (size == 0) {
            std::cerr << "Failed to decode command-line argument as UTF-8\n";
            return 1;
        }
        std::string text(static_cast<std::size_t>(size), '\0');
        if (WideCharToMultiByte(CP_UTF8, WC_ERR_INVALID_CHARS, argv[index], -1, text.data(), size,
                                nullptr, nullptr) != size) {
            std::cerr << "Failed to decode command-line argument as UTF-8\n";
            return 1;
        }
        text.pop_back();
        arguments.push_back(std::move(text));
    }
    std::vector<char*> pointers;
    pointers.reserve(arguments.size());
    for (auto& argument : arguments) {
        pointers.push_back(argument.data());
    }
    return mizuki::run_cli(argc, pointers.data());
}
#else
int main(int argc, char** argv) {
    return mizuki::run_cli(argc, argv);
}
#endif
