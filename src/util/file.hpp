#pragma once
#include <filesystem>
#include <string>

namespace mizuki {
std::string read_file(std::filesystem::path const& path);
}
