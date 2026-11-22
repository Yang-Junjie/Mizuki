#pragma once

#include <nlohmann/json.hpp>
#include <string>
#include <string_view>
#include <vector>

namespace mizuki {

enum class DocumentKind { Article, Page };

struct FrontMatter {
    std::string title;
    std::string date;
    std::string description;
    std::string layout;
    bool draft{false};
    std::vector<std::string> tags;
    std::vector<std::string> categories;
    nlohmann::json extra = nlohmann::json::object();
};

struct Document {
    FrontMatter metadata;
    std::string body;
};

// Owns the body, so the result does not borrow from the input text.
Document parse_document(std::string_view source, DocumentKind kind);

} // namespace mizuki
