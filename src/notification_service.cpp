#include "itmo_notification/notification_service.hpp"
#include "itmo_notification/notification.hpp"

#include <algorithm>

namespace itmo_notification {

namespace {

DueNotification toDue(const Notification& n) {
    return {
        n.id,
        n.user_id,
        n.channel,
        n.recipient,
        n.template_name,
        n.payload,
        n.send_at,
        n.priority,
        n.created_at,
    };
}

}  // namespace

NotificationService::NotificationService()  = default;
NotificationService::~NotificationService() = default;

void NotificationService::add(Notification notification) {
    std::lock_guard<std::mutex> lk(mu_);
    auto it = notifications_.find(notification.id);
    if (it != notifications_.end()) {
        return;
    }
    notification.status = NotificationStatus::Pending;
    auto [pending_it, _] = pendings_.insert(std::move(notification));
    notifications_[pending_it->id] = pending_it;
}

bool NotificationService::cancel(std::string_view id) {
    std::lock_guard<std::mutex> lk(mu_);
    const auto key = std::string(id);
    auto it = notifications_.find(key);
    if (it == notifications_.end()) {
        return false;
    }
    auto pending_it = it->second;
    notifications_.erase(it);
    pendings_.erase(pending_it);
    return true;
}

bool NotificationService::markSent(std::string_view id) {
    return cancel(id);
}

std::optional<Notification> NotificationService::get(std::string_view id) const {
    std::lock_guard<std::mutex> lk(mu_);
    auto it = notifications_.find(std::string(id));
    if (it == notifications_.end()) {
        return std::nullopt;
    }
    return *it->second;
}

std::vector<DueNotification> NotificationService::due(std::int64_t now,
                                                      std::size_t  limit) const {
    std::vector<DueNotification> result;
    if (limit == 0) {
        return result;
    }

    std::lock_guard<std::mutex> lk(mu_);

    result.reserve(std::min(limit, pendings_.size()));

    for (const auto& notification : pendings_) {
        if (notification.send_at > now) {
            break;
        }

        result.push_back(toDue(notification));

        if (result.size() == limit) {
            break;
        }
    }

    return result;
}

}  // namespace itmo_notification
