#include "util/file.hpp"

#include "util/path.hpp"

#include <fstream>
#include <sstream>
#include <stdexcept>

namespace mizuki {

std::string read_file(std::filesystem::path const& path) {
    std::ifstream file(path, std::ios::binary);
    if (!file.is_open()) {
        throw std::runtime_error("Failed to open file: " + path_to_utf8(path));
    }
    std::ostringstream ss;
    ss << file.rdbuf();
    if (file.bad() || ss.bad()) {
        throw std::runtime_error("Failed to read file: " + path_to_utf8(path));
    }
    return ss.str();
}

void write_file(std::filesystem::path const& path, std::string_view content) {
    std::ofstream file(path, std::ios::binary | std::ios::trunc);
    if (!file.is_open()) {
        throw std::runtime_error("Failed to open output file: " + path_to_utf8(path));
    }
    file << content;
    file.close();
    if (!file) {
        throw std::runtime_error("Failed to write file: " + path_to_utf8(path));
    }
}

} // namespace mizuki
