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

void NotificationService::add(Notification notification)
{
    auto shard_idx = get_shard_idx(notification.id);
    auto& shard = shards_[shard_idx];
    std::optional<SetItem> stale_item;
    {
        std::unique_lock shard_lock(shard.mu);
        auto it = shard.map.find(notification.id);
        if (it != shard.map.end())
        {
            if (it->second.status == NotificationStatus::Pending)
            {
                return;
            }
            stale_item = SetItem {it->second.id,
                                  it->second.send_at,
                                  it->second.created_at,
                                  it->second.priority};
        }
        notification.status = NotificationStatus::Pending;
        shard.map.insert_or_assign(notification.id, notification);
    }
    {
        std::lock_guard set_lock(set_mu_);
        if (stale_item)
        {
            pendings_.erase(*stale_item);
        }
        pendings_.insert({notification.id,
                          notification.send_at,
                          notification.created_at,
                          notification.priority});
        if (notification.send_at <
            earliest_send_at_.load(std::memory_order_relaxed))
        {
            earliest_send_at_.store(notification.send_at,
                                    std::memory_order_release);
        }
    }
}

bool NotificationService::cancel(std::string_view id)
{
    auto shard_idx = get_shard_idx(id);
    auto& shard = shards_[shard_idx];
    std::unique_lock shard_lock(shard.mu);
    auto it = shard.map.find(id);
    if (it == shard.map.end() ||
        it->second.status != NotificationStatus::Pending)
    {
        return false;
    }
    it->second.status = NotificationStatus::Cancelled;
    return true;
}

bool NotificationService::markSent(std::string_view id)
{
    auto shard_idx = get_shard_idx(id);
    auto& shard = shards_[shard_idx];
    std::unique_lock shard_lock(shard.mu);
    auto it = shard.map.find(id);
    if (it == shard.map.end() ||
        it->second.status != NotificationStatus::Pending)
    {
        return false;
    }
    it->second.status = NotificationStatus::Sent;
    return true;
}

std::optional<Notification>
NotificationService::get(std::string_view id) const
{
    auto shard_idx = get_shard_idx(id);
    auto& shard = shards_[shard_idx];
    std::shared_lock shard_lock(shard.mu);
    auto it = shard.map.find(id);
    if (it == shard.map.end() ||
        it->second.status != NotificationStatus::Pending)
    {
        return std::nullopt;
    }
    return it->second;
}

std::vector<DueNotification>
NotificationService::due(std::int64_t now, std::size_t limit) const
{
    if (limit == 0)
    {
        return {};
    }
    if (earliest_send_at_.load(std::memory_order_acquire) > now)
    {
        return {};
    }
    std::vector<DueNotification> result;
    result.reserve(limit);
    std::lock_guard set_lock(set_mu_);
    for (auto it = pendings_.begin(); it != pendings_.end();)
    {
        if (it->send_at > now)
        {
            break;
        }
        auto shard_idx = get_shard_idx(it->id);
        auto& shard = shards_[shard_idx];
        std::optional<Notification> actual_n;
        {
            std::shared_lock shard_lock(shard.mu);
            auto map_it = shard.map.find(it->id);
            if (map_it != shard.map.end())
            {
                actual_n = map_it->second;
            }
        }
        if (!actual_n || actual_n->status != NotificationStatus::Pending)
        {
            it = pendings_.erase(it);
            continue;
        }
        result.push_back(toDue(*actual_n));
        ++it;
        if (result.size() == limit)
        {
            break;
        }
    }
    if (!pendings_.empty())
    {
        earliest_send_at_.store(pendings_.begin()->send_at,
                                std::memory_order_release);
    } else
    {
        earliest_send_at_.store(std::numeric_limits<std::int64_t>::max(),
                                std::memory_order_release);
    }
    return result;
}

}  // namespace itmo_notification
