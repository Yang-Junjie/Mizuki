#pragma once
#include "config/config.hpp"

namespace mizuki {

int serve(ConfigRequest const& request, int port = 3939, bool write_output = false);

} // namespace mizuki
