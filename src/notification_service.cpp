#include <algorithm>
#include <mutex>

#include "itmo_notification/notification.hpp"
#include "itmo_notification/notification_service.hpp"

namespace itmo_notification
{

namespace
{

DueNotification toDue(const Notification& n)
{
    return {
        n.id,
        n.user_id,
        n.channel,
        n.recipient,
        n.template_name,
        n.payload,
        n.send_at,
        n.created_at,
        n.priority,
    };
}

}  // namespace

NotificationService::NotificationService() = default;
NotificationService::~NotificationService() = default;

namespace
{}  // namespace

void NotificationService::add(Notification notification)
{
    std::size_t idx = GetShardIdx(notification.id);
    auto& shard = shards_[idx];
    {
        std::shared_lock<std::shared_mutex> shard_lock(shard.mu);
        auto& map = shard.notifications;
        auto it = map.find(notification.id);
        if (it != map.end())
        {
            return;
        }
    }
    {
        std::unique_lock<std::shared_mutex> set_lock(set_mu_);
        std::unique_lock<std::shared_mutex> shard_lock(shard.mu);
        auto& map = shard.notifications;
        auto it = map.find(notification.id);
        if (it != map.end())
        {
            return;
        }
        notification.status = NotificationStatus::Pending;
        auto [pending_it, _] = pendings_.insert(std::move(notification));
        map[pending_it->id] = pending_it;
    }
}

bool NotificationService::cancel(std::string_view id)
{
    std::size_t idx = GetShardIdx(id);
    auto& shard = shards_[idx];
    {
        std::shared_lock<std::shared_mutex> shard_lock(shard.mu);
        auto& map = shard.notifications;
        auto it = map.find(id);
        if (it == map.end())
        {
            return false;
        }
    }
    {
        std::unique_lock<std::shared_mutex> set_lock(set_mu_);
        std::unique_lock<std::shared_mutex> shard_lock(shard.mu);
        auto& map = shard.notifications;
        auto it = map.find(id);
        if (it == map.end())
        {
            return false;
        }
        auto pending_it = it->second;
        map.erase(it);
        pendings_.erase(pending_it);
    }
    return true;
}

bool NotificationService::markSent(std::string_view id)
{
    return cancel(id);
}

std::optional<Notification>
NotificationService::get(std::string_view id) const
{
    std::size_t idx = GetShardIdx(id);
    auto& shard = shards_[idx];
    std::shared_lock<std::shared_mutex> shard_lock(shard.mu);
    auto& map = shard.notifications;
    auto it = map.find(id);
    if (it == map.end())
    {
        return std::nullopt;
    }
    return *it->second;
}

std::vector<DueNotification>
NotificationService::due(std::int64_t now, std::size_t limit) const
{
    std::vector<DueNotification> result;
    if (limit == 0)
    {
        return result;
    }
    result.reserve(std::min(limit, pendings_.size()));
    std::shared_lock<std::shared_mutex> set_lock(set_mu_);
    for (const auto& notification : pendings_)
    {
        if (notification.send_at > now)
        {
            break;
        }
        result.push_back(toDue(notification));
        if (result.size() == limit)
        {
            break;
        }
    }
    return result;
}

}  // namespace itmo_notification
