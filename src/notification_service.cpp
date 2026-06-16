#include "itmo_notification/notification_service.hpp"

#include <algorithm>

#include "itmo_notification/notification.hpp"

namespace itmo_notification {

namespace {

DueNotification toDue(const Notification& n) {
  return {
      n.id,      n.user_id, n.channel,    n.recipient, n.template_name,
      n.payload, n.send_at, n.created_at, n.priority,
  };
}

}  // namespace

NotificationService::NotificationService() = default;
NotificationService::~NotificationService() = default;

void NotificationService::add(Notification notification) {
  std::unique_lock lk(mu_);

  if (notifications_.contains(notification.id)) {
    return;
  }
  notification.status = NotificationStatus::Pending;
  auto [pending_it, inserted] = pendings_.insert(std::move(notification));
  if (!inserted) {
    return;
  }

  notifications_.emplace(pending_it->id, pending_it);
}

bool NotificationService::cancel(std::string_view id) {
  std::unique_lock<std::shared_mutex> lk(mu_);
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

bool NotificationService::markSent(std::string_view id) { return cancel(id); }

std::optional<Notification> NotificationService::get(
    std::string_view id) const {
  std::shared_lock<std::shared_mutex> lk(mu_);
  auto it = notifications_.find(std::string(id));
  if (it == notifications_.end()) {
    return std::nullopt;
  }
  return *it->second;
}

std::vector<DueNotification> NotificationService::due(std::int64_t now,
                                                      std::size_t limit) const {
  std::vector<DueNotification> result;
  if (limit == 0) {
    return result;
  }

  std::shared_lock<std::shared_mutex> lk(mu_);
  result.reserve(std::min(limit, pendings_.size()));

  for (const auto& notification : pendings_) {
    if (notification.send_at > now || result.size() == limit) {
      break;
    }

    result.push_back(toDue(notification));
  }

  return result;
}

}  // namespace itmo_notification
