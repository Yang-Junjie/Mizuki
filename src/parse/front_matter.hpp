#pragma once

#include <string>
#include <string_view>

namespace mizuki {

enum class DocumentKind { Article, Page };

struct FrontMatter {
    std::string title;
    std::string date;
    std::string description;
    bool draft{false};
};

struct Document {
    FrontMatter metadata;
    std::string body;
};

// Owns the body, so the result does not borrow from the input text.
Document parse_document(std::string_view source, DocumentKind kind);

} // namespace mizuki
