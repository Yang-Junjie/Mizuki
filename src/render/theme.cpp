#include "render/theme.hpp"

#include "config/values.hpp"
#include "util/file.hpp"
#include "util/path.hpp"

#include <toml++/toml.hpp>

#include <algorithm>
#include <inja/inja.hpp>
#include <map>
#include <sstream>
#include <stdexcept>
#include <utility>
#include <vector>

namespace mizuki {
namespace fs = std::filesystem;

struct Theme::Impl {
    fs::path root;
    nlohmann::json extra = nlohmann::json::object();
    inja::Environment environment;
    std::map<fs::path, inja::Template> templates;
    std::vector<fs::path> loading;
    std::ostream* output{nullptr};

    explicit Impl(fs::path const& directory) : root(fs::weakly_canonical(directory / "layouts")) {
        auto const manifest = directory / "theme.toml";
        if (fs::exists(manifest)) {
            try {
                auto const config = toml::parse(read_file(manifest));
                for (auto const& [key, value] : config) {
                    if (key != "name" && key != "engine" && key != "extra") {
                        throw std::runtime_error("Unknown theme configuration key: " +
                                                 std::string(key.str()));
                    }
                }
                for (auto const key : {"name", "engine"}) {
                    if (auto const* value = config.get(key); value && !value->is_string()) {
                        throw std::runtime_error("Theme configuration '" + std::string(key) +
                                                 "' must be a string");
                    }
                }
                if (auto const engine = config["engine"].value<std::string>();
                    engine && *engine != "inja") {
                    throw std::runtime_error("Theme engine must be 'inja'");
                }
                if (auto const* value = config.get("extra")) {
                    if (!value->is_table()) {
                        throw std::runtime_error("Theme configuration 'extra' must be a table");
                    }
                    extra = template_value(*value);
                }
            } catch (std::exception const& error) {
                throw std::runtime_error("Failed to load " + path_to_utf8(manifest) + ": " +
                                         error.what());
            }
        }
        environment.set_html_autoescape(true);
        environment.set_search_included_templates_in_files(false);
        environment.set_include_callback(
            [this](fs::path const&, std::string const& name) { return load(name); });
        // Inja's void callbacks can write to the active render stream. Keeping this an
        // output function preserves automatic escaping for all ordinary expressions.
        environment.add_void_callback("html", 1, [this](inja::Arguments& arguments) {
            if (!arguments.front()->is_string()) {
                throw std::runtime_error("html() requires a string");
            }
            *output << arguments.front()->get_ref<std::string const&>();
        });
    }

    fs::path resolve(std::string_view name) const {
        auto const path = path_from_utf8(name);
        if (name.empty() || name.find_first_of("\\:") != std::string_view::npos ||
            name.find('\0') != std::string_view::npos || path.is_absolute() ||
            path.has_root_name() || std::ranges::find(path, fs::path("..")) != path.end()) {
            throw std::runtime_error("Template path must stay inside theme layouts: " +
                                     std::string(name));
        }
        auto const resolved = fs::weakly_canonical(root / path);
        auto const relative = resolved.lexically_relative(root);
        if (relative.empty() || *relative.begin() == fs::path("..")) {
            throw std::runtime_error("Template path escapes theme layouts: " + std::string(name));
        }
        return resolved;
    }

    inja::Template const& load(std::string_view name) {
        auto const path = resolve(name);
        if (auto const found = templates.find(path); found != templates.end()) {
            return found->second;
        }
        if (std::ranges::find(loading, path) != loading.end() || loading.size() >= 64) {
            std::string chain;
            for (auto const& entry : loading) {
                chain += path_to_utf8(entry.lexically_relative(root)) + " -> ";
            }
            throw std::runtime_error("Template dependency cycle or excessive depth: " + chain +
                                     path_to_utf8(path.lexically_relative(root)));
        }
        std::string source;
        try {
            source = read_file(path);
        } catch (std::exception const& error) {
            throw std::runtime_error("Failed to load template " + path_to_utf8(path) + ": " +
                                     error.what());
        }
        loading.push_back(path);
        try {
            auto parsed = environment.parse(source);
            auto const inserted = templates.emplace(path, std::move(parsed)).first;
            loading.pop_back();
            return inserted->second;
        } catch (std::exception const& error) {
            loading.pop_back();
            throw std::runtime_error("Failed to parse template " + path_to_utf8(path) + ": " +
                                     error.what());
        }
    }
};

Theme::Theme(fs::path directory) : impl_(std::make_unique<Impl>(directory)) {}
Theme::~Theme() = default;

nlohmann::json const& Theme::defaults() const {
    return impl_->extra;
}

std::string Theme::render(std::string_view layout, nlohmann::json const& context) {
    auto const& parsed = impl_->load(layout);
    std::ostringstream result;
    impl_->output = &result;
    try {
        impl_->environment.render_to(result, parsed, context);
        impl_->output = nullptr;
        return result.str();
    } catch (std::exception const& error) {
        impl_->output = nullptr;
        throw std::runtime_error("Failed to render template " +
                                 path_to_utf8(impl_->resolve(layout)) + ": " + error.what());
    }
}

} // namespace mizuki
