#pragma once

#include <filesystem>
#include <string>
#include <string_view>

namespace mizuki {

struct BuildPlan;
enum class LinkKind { Document, Resource, Navigation };

std::string resolve_link(std::string_view url, std::filesystem::path const& source,
                         BuildPlan const& plan, LinkKind kind);

} // namespace mizuki
