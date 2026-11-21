#pragma once

#include <condition_variable>
#include <cstddef>
#include <filesystem>
#include <mutex>

namespace httplib {
class Server;
}

namespace mizuki {

class LiveReload {
public:
    void attach(httplib::Server& server, std::filesystem::path const& output_dir);
    void notify_build(std::filesystem::path const& output_dir);

private:
    std::mutex mutex_;
    std::condition_variable changed_;
    std::size_t version_{0};
    std::filesystem::path output_dir_;
};

}  // namespace mizuki
