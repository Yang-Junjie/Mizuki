#pragma once

#include "build/plan.hpp"

#include <filesystem>
#include <span>

namespace mizuki {

// Owns a staged build. The output path must already be validated and absolute.
class OutputTransaction {
public:
    explicit OutputTransaction(std::filesystem::path output);
    ~OutputTransaction();

    OutputTransaction(OutputTransaction const&) = delete;
    OutputTransaction& operator=(OutputTransaction const&) = delete;

    std::filesystem::path const& stage() const;
    bool matches_published(std::span<PlannedAsset const> pending_copies = {}) const;
    void commit();

private:
    std::filesystem::path output_;
    std::filesystem::path stage_;
};

} // namespace mizuki
