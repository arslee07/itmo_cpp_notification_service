#pragma once

#include <cstdint>
#include <string>

namespace itmo_notification {

struct DueNotification {
    std::string  id;
    std::string  user_id;
    std::string  channel;
    std::string  recipient;
    std::string  template_name;
    std::string  payload;
    std::int64_t send_at{};
    std::int64_t created_at{};
    int          priority{};
    int          attempts{};
};

}  // namespace itmo_notification
