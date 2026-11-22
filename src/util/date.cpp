#include "util/date.hpp"

#include <array>
#include <stdexcept>

namespace mizuki {

std::chrono::year_month_day parse_date(std::string_view text) {
    auto invalid = [&]() -> std::chrono::year_month_day {
        throw std::runtime_error("Invalid date '" + std::string(text) +
                                 "': expected a valid YYYY-MM-DD date (0001–9999)");
    };
    if (text.size() != 10 || text[4] != '-' || text[7] != '-') {
        return invalid();
    }
    auto number = [&](std::size_t offset, std::size_t count) {
        unsigned value = 0;
        for (char const ch : text.substr(offset, count)) {
            if (ch < '0' || ch > '9') {
                invalid();
            }
            value = value * 10 + static_cast<unsigned>(ch - '0');
        }
        return value;
    };
    auto const year = number(0, 4);
    auto const date = std::chrono::year{static_cast<int>(year)} / std::chrono::month{number(5, 2)} /
                      std::chrono::day{number(8, 2)};
    if (year == 0 || !date.ok()) {
        return invalid();
    }
    return date;
}

std::string rss_date(std::string_view text) {
    auto const date = parse_date(text);
    constexpr std::array<std::string_view, 7> weekdays{"Sun", "Mon", "Tue", "Wed",
                                                       "Thu", "Fri", "Sat"};
    constexpr std::array<std::string_view, 12> months{"Jan", "Feb", "Mar", "Apr", "May", "Jun",
                                                      "Jul", "Aug", "Sep", "Oct", "Nov", "Dec"};
    auto const weekday = std::chrono::weekday{std::chrono::sys_days{date}}.c_encoding();
    return std::string(weekdays[weekday]) + ", " + std::string(text.substr(8, 2)) + " " +
           std::string(months[static_cast<unsigned>(date.month()) - 1]) + " " +
           std::string(text.substr(0, 4)) + " 00:00:00 GMT";
}

} // namespace mizuki
