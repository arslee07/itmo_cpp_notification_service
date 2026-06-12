#pragma once

#include <cstdint>
#include <string>

namespace itmo_notification {

struct Notification {
    std::string  id;
    std::string  user_id;
    std::string  channel;
    std::string  recipient;
    std::string  template_name;
    std::string  payload;
    std::int64_t send_at{};
    int          priority{};
    std::int64_t created_at{};
};

struct NotificationCompare {
    using is_transparent = void;

    bool operator()(const Notification& lhs, const Notification& rhs) const {
        if (lhs.send_at != rhs.send_at) return lhs.send_at < rhs.send_at;
        if (lhs.priority != rhs.priority) return lhs.priority > rhs.priority;
        if (lhs.created_at != rhs.created_at) return lhs.created_at < rhs.created_at;
        return lhs.id < rhs.id;
    }

    bool operator()(const Notification& lhs, std::int64_t rhs_send_at) const {
        return lhs.send_at < rhs_send_at;
    }

    bool operator()(std::int64_t lhs_send_at, const Notification& rhs) const {
        return lhs_send_at < rhs.send_at;
    }
};

}  // namespace itmo_notification
