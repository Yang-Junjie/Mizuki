#pragma once

#include <chrono>
#include <filesystem>
#include <stdexcept>
#include <string>
#include <string_view>
#include <system_error>

namespace mizuki::test {

namespace fs = std::filesystem;

inline void require(bool condition, std::string_view message) {
    if (!condition) {
        throw std::runtime_error(std::string(message));
    }
}

class TemporaryDirectory {
public:
    TemporaryDirectory() {
        for (int attempt = 0; attempt < 100; ++attempt) {
            path_ = fs::temp_directory_path() /
                    ("mizuki-test-" +
                     std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
            if (fs::create_directory(path_)) {
                return;
            }
        }
        throw std::runtime_error("Could not reserve a temporary test directory");
    }

    TemporaryDirectory(TemporaryDirectory const&) = delete;
    TemporaryDirectory& operator=(TemporaryDirectory const&) = delete;

    ~TemporaryDirectory() {
        std::error_code error;
        fs::remove_all(path_, error);
    }

    fs::path const& path() const { return path_; }

private:
    fs::path path_;
};

} // namespace mizuki::test
