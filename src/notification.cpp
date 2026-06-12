#include "itmo_notification/notification.hpp"

namespace itmo_notification {

static_assert(sizeof(Notification) > 0);

bool Notification::operator<(const Notification& other) const {
    if (send_at != other.send_at) return send_at < other.send_at;
    if (priority != other.priority) return priority > other.priority;
    if (created_at != other.created_at)
      return created_at < other.created_at;
    return id < other.id;
}

}  // namespace itmo_notification
