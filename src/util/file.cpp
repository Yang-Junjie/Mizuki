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
    return ss.str();
}

}
