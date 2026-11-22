#pragma once

#include <filesystem>
#include <string_view>
#include <vector>

namespace mizuki {

// Rolls back only files and directories created by this operation.
class FileCreation {
public:
    FileCreation() = default;
    ~FileCreation();

    FileCreation(FileCreation const&) = delete;
    FileCreation& operator=(FileCreation const&) = delete;

    void create_directory(std::filesystem::path const& path);
    void write(std::filesystem::path const& path, std::string_view content);
    void commit();

private:
    std::vector<std::filesystem::path> created_;
    bool committed_{false};
};

} // namespace mizuki
