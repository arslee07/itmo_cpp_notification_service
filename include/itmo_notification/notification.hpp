#pragma once

#include <cstdint>
#include <string>

namespace itmo_notification {

enum class NotificationStatus {
    Pending,
    Sent,
    Cancelled,
};

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
    NotificationStatus status{NotificationStatus::Pending};

    bool operator<(const Notification& other) const {
        if (send_at != other.send_at) return send_at < other.send_at;
        if (priority != other.priority) return priority > other.priority;
        if (created_at != other.created_at)
          return created_at < other.created_at;
        return id < other.id;
    }
};

}  // namespace itmo_notification
