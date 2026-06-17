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
    auto task = std::make_shared<TaskState>();
    notification.status = NotificationStatus::Pending;
    task->n = notification;
    {
        std::unique_lock shard_lock(shard.mu);
        auto it = shard.map.find(notification.id);
        if (it != shard.map.end())
        {
            return;
        }
        shard.map[notification.id] = task;
    }
    {
        std::lock_guard set_lock(set_mu_);
        pendings_.insert({notification.id,
                          task,
                          notification.send_at,
                          notification.created_at,
                          notification.priority});
        auto current_min =
            earliest_send_at_.load(std::memory_order_relaxed);
        while (notification.send_at < current_min)
        {
            if (earliest_send_at_.compare_exchange_weak(
                    current_min,
                    notification.send_at,
                    std::memory_order_release,
                    std::memory_order_relaxed))
            {
                break;
            }
        }
    }
}

bool NotificationService::cancel(std::string_view id)
{
    auto shard_idx = get_shard_idx(id);
    auto& shard = shards_[shard_idx];
    std::unique_lock shard_lock(shard.mu);
    auto it = shard.map.find(id);
    if (it == shard.map.end())
    {
        return false;
    }
    it->second->is_active.store(false, std::memory_order_relaxed);
    shard.map.erase(it);
    return true;
}

bool NotificationService::markSent(std::string_view id)
{
    auto shard_idx = get_shard_idx(id);
    auto& shard = shards_[shard_idx];
    std::unique_lock shard_lock(shard.mu);
    auto it = shard.map.find(id);
    if (it == shard.map.end())
    {
        return false;
    }
    it->second->is_active.store(false, std::memory_order_relaxed);
    shard.map.erase(it);
    return true;
}

std::optional<Notification>
NotificationService::get(std::string_view id) const
{
    auto shard_idx = get_shard_idx(id);
    auto& shard = shards_[shard_idx];
    std::shared_lock shard_lock(shard.mu);
    auto it = shard.map.find(id);
    if (it == shard.map.end())
    {
        return std::nullopt;
    }
    return it->second->n;
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
        if (!it->task->is_active.load(std::memory_order_relaxed))
        {
            it = pendings_.erase(it);
            continue;
        }
        result.push_back(toDue(it->task->n));
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
