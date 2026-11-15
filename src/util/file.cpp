#include "util/file.hpp"

#include <fstream>
#include <sstream>
#include <stdexcept>

namespace mizuki {

std::string read_file(std::filesystem::path const& path) {
    std::ifstream file(path, std::ios::binary);
    if (!file.is_open()) {
        throw std::runtime_error("Failed to open file: " + path.string());
    }
    std::ostringstream ss;
    ss << file.rdbuf();
    if (file.bad() || ss.bad()) {
        throw std::runtime_error("Failed to read file: " + path.string());
    }
    return ss.str();
}

void write_file(std::filesystem::path const& path, std::string_view content) {
    std::ofstream file(path, std::ios::binary | std::ios::trunc);
    if (!file.is_open()) {
        throw std::runtime_error("Failed to open output file: " + path.string());
    }
    file << content;
    file.close();
    if (!file) {
        throw std::runtime_error("Failed to write file: " + path.string());
    }
}

}  // namespace mizuki
