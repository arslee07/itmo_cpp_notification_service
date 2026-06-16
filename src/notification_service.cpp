#include "itmo_notification/notification_service.hpp"
#include "itmo_notification/notification.hpp"

#include <algorithm>
#include <queue>

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
        n.created_at,
        n.priority,
    };
}

}  // namespace

NotificationService::NotificationService()  = default;
NotificationService::~NotificationService() = default;

NotificationService::Shard& NotificationService::GetShard(std::string_view id) noexcept {
    size_t idx = IdHash{}(id) & (SHARD_AMOUNT - 1);
    return shards_[idx];
}

const NotificationService::Shard& NotificationService::GetShard(std::string_view id) const noexcept {
    size_t idx = IdHash{}(id) & (SHARD_AMOUNT - 1);
    return shards_[idx];
}

void NotificationService::add(Notification notification) {
    auto& shard = GetShard(notification.id);
    {
        std::shared_lock<std::shared_mutex> lk(shard.mu);
        auto it = shard.notifications.find(notification.id);
        if (it != shard.notifications.end()) {
            return;
        }
    }

    {
        std::unique_lock<std::shared_mutex> lk(shard.mu);
        auto it = shard.notifications.find(notification.id);
        if (it != shard.notifications.end()) {
            return;
        }
        notification.status = NotificationStatus::Pending;
        auto [pending_it, _] = shard.pendings.insert(std::move(notification));
        shard.notifications[pending_it->id] = pending_it;
    }
}

bool NotificationService::cancel(std::string_view id) {
    auto& shard = GetShard(id);
    {
        std::shared_lock<std::shared_mutex> lk(shard.mu);
        auto it = shard.notifications.find(id);
        if (it == shard.notifications.end()) {
            return false;
        }
    }

    {
        std::unique_lock<std::shared_mutex> lk(shard.mu);
        auto it = shard.notifications.find(id);
        if (it == shard.notifications.end()) {
            return false;
        }
        shard.pendings.erase(it->second);
        shard.notifications.erase(it);
    }

    return true;
}

bool NotificationService::markSent(std::string_view id) {
    return cancel(id);
}

std::optional<Notification> NotificationService::get(std::string_view id) const {
    const auto& shard = GetShard(id);
    std::shared_lock<std::shared_mutex> lk(shard.mu);
    auto it = shard.notifications.find(id);
    if (it == shard.notifications.end()) {
        return std::nullopt;
    }
    return *it->second;
}

std::vector<DueNotification> NotificationService::due(std::int64_t now,
                                                      std::size_t  limit) const {
    if (limit == 0) {
        return {};
    }
    std::vector<DueNotification> result;
    result.reserve(limit);

    {
        std::array<std::shared_lock<std::shared_mutex>, SHARD_AMOUNT> lks;
        for (size_t i = 0; i < SHARD_AMOUNT; ++i) {
            lks[i] = std::shared_lock<std::shared_mutex>(shards_[i].mu);
        }
    
        struct HeapItem {
            std::ranges::iterator_t<decltype(std::declval<Shard>().pendings)> it;
            size_t idx;
        
            bool operator>(const HeapItem& other) const noexcept {
                return *other.it < *it;
            }
        };
    
        std::priority_queue<HeapItem, std::vector<HeapItem>, std::greater<HeapItem>> q;
        for (size_t i = 0; i < SHARD_AMOUNT; ++i) {
            auto it = shards_[i].pendings.begin();
            if (it != shards_[i].pendings.end() && it->send_at <= now) {
                q.push(HeapItem{it, i});
            } else {
                lks[i].unlock();
            }
        }
    
        while (result.size() < limit && !q.empty()) {
            auto [it, idx] = q.top();
            q.pop();
            result.push_back(toDue(*it));
            ++it;
            if (it != shards_[idx].pendings.end() && it->send_at <= now) {
                q.push(HeapItem{it, idx});
            } else {
                lks[idx].unlock();
            }
        }
    }

    return result;
}

}  // namespace itmo_notification
