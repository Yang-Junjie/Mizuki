#include "build/output_transaction.hpp"
#include "util/file.hpp"

#include <chrono>
#include <exception>
#include <filesystem>
#include <iostream>
#include <stdexcept>
#include <string>
#include <string_view>
#include <system_error>

namespace fs = std::filesystem;

namespace {

void require(bool condition, std::string_view message) {
    if (!condition) {
        throw std::runtime_error(std::string(message));
    }
}

class TemporaryDirectory {
public:
    TemporaryDirectory() {
        for (int attempt = 0; attempt < 100; ++attempt) {
            path_ = fs::temp_directory_path() /
                    ("mizuki-output-test-" +
                     std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
            if (fs::create_directory(path_)) {
                return;
            }
        }
        throw std::runtime_error("Could not reserve a temporary test directory");
    }

    ~TemporaryDirectory() {
        std::error_code error;
        fs::remove_all(path_, error);
    }

    fs::path const& path() const { return path_; }

private:
    fs::path path_;
};

void assert_no_temporary_directories(fs::path const& parent) {
    for (auto const& entry : fs::directory_iterator(parent)) {
        require(entry.path().filename().string().find(".mizuki-") == std::string::npos,
                "Temporary build directory was not removed");
    }
}

std::string expect_failed_commit(mizuki::OutputTransaction& transaction) {
    try {
        transaction.commit();
    } catch (std::exception const& error) {
        return error.what();
    }
    throw std::runtime_error("Publishing a missing staging directory should fail");
}

void abandoned_build_cleans_partial_output() {
    TemporaryDirectory temporary;
    auto const output = temporary.path() / "dist";
    {
        mizuki::OutputTransaction transaction(output);
        mizuki::write_file(transaction.stage() / "partial.html", "Partial build");
    }
    require(!fs::exists(output), "An abandoned build created the final output");
    assert_no_temporary_directories(temporary.path());
}

void failed_first_publish_leaves_no_output() {
    TemporaryDirectory temporary;
    auto const output = temporary.path() / "dist";
    {
        mizuki::OutputTransaction transaction(output);
        // Force the rename to fail without relying on platform-specific permissions.
        fs::remove_all(transaction.stage());
        auto const error = expect_failed_commit(transaction);
        require(error.find("Failed to publish") != std::string::npos,
                "Publish error did not identify the failed operation");
    }
    require(!fs::exists(output), "A failed first publish left a final output directory");
    assert_no_temporary_directories(temporary.path());
}

void failed_publish_restores_previous_site() {
    TemporaryDirectory temporary;
    auto const output = temporary.path() / "dist";
    fs::create_directories(output / "assets");
    mizuki::write_file(output / "index.html", "Previous successful site");
    mizuki::write_file(output / "assets/style.css", "Previous stylesheet");
    {
        mizuki::OutputTransaction transaction(output);
        mizuki::write_file(transaction.stage() / "index.html", "Unpublished replacement");
        fs::remove_all(transaction.stage());
        expect_failed_commit(transaction);
        require(mizuki::read_file(output / "index.html") == "Previous successful site",
                "Failed publish did not restore the previous HTML");
        require(mizuki::read_file(output / "assets/style.css") == "Previous stylesheet",
                "Failed publish did not restore the previous assets");
    }
    assert_no_temporary_directories(temporary.path());
}

}  // namespace

int main() {
    try {
        abandoned_build_cleans_partial_output();
        failed_first_publish_leaves_no_output();
        failed_publish_restores_previous_site();
    } catch (std::exception const& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
    return 0;
}
