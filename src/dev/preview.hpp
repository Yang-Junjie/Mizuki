#pragma once

namespace httplib {
class Server;
}

namespace mizuki {

class LiveReload;

void attach_preview(httplib::Server& server, LiveReload& reload);

} // namespace mizuki
