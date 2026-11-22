#pragma once

#include <chrono>
#include <string>
#include <string_view>

namespace mizuki {

// Dates are Gregorian calendar dates in YYYY-MM-DD form, with years 0001–9999.
std::chrono::year_month_day parse_date(std::string_view text);
std::string rss_date(std::string_view text);
// Used only when creating content; builds remain independent of the local clock.
std::string local_date();

} // namespace mizuki
