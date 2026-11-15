#pragma once
#include <string>
#include <string_view>

namespace mizuki {

struct FrontMatter {
    std::string title;
    std::string date;
    bool draft{false};
};

bool parse_front_matter(std::string_view source, FrontMatter& metadata, std::string_view& body,
                        std::string& error);

}  // namespace mizuki
