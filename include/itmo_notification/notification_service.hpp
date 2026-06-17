#pragma once

#include <array>
#include <atomic>
#include <cstdint>
#include <functional>
#include <limits>
#include <mutex>
#include <optional>
#include <set>
#include <shared_mutex>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

#include "itmo_notification/due_notification.hpp"
#include "itmo_notification/notification.hpp"

namespace itmo_notification
{

// In-memory планировщик уведомлений. API менять нельзя: он соответствует
// HTTP-ручкам.
class NotificationService
{
  public:
    NotificationService();
    ~NotificationService();

    NotificationService(const NotificationService&) = delete;
    NotificationService& operator=(const NotificationService&) = delete;

    // POST /v1/notifications
    void add(Notification notification);

    // DELETE /v1/notifications/{id}
    bool cancel(std::string_view id);

    // POST /v1/notifications/{id}/sent
    bool markSent(std::string_view id);

    // GET /v1/notifications/{id}
    std::optional<Notification> get(std::string_view id) const;

    // GET /v1/due?now=...&limit=...
    std::vector<DueNotification> due(std::int64_t now,
                                     std::size_t limit) const;

  private:
    struct TaskState
    {
        std::atomic<bool> is_active {true};
        Notification n;
    };

    struct Hasher
    {
        using is_transparent = void;

        std::size_t operator()(std::string_view str) const noexcept
        {
            return std::hash<std::string_view> {}(str);
        }

        std::size_t operator()(const std::string& str) const noexcept
        {
            return std::hash<std::string> {}(str);
        }
    };

    struct IdShard
    {
        mutable std::shared_mutex mu;
        std::unordered_map<std::string,
                           std::shared_ptr<TaskState>,
                           Hasher,
                           std::equal_to<>>
            map;
    };

    static constexpr std::size_t SHARDS_COUNT = 16;
    mutable std::array<IdShard, SHARDS_COUNT> shards_;

    std::size_t get_shard_idx(std::string_view id) const noexcept
    {
        return Hasher {}(id) & (SHARDS_COUNT - 1);
    }

    struct SetItem
    {
        std::string id;
        std::shared_ptr<TaskState> task;
        std::int64_t send_at;
        std::int64_t created_at;
        int priority;

        bool operator<(const SetItem& other) const noexcept
        {
            if (send_at != other.send_at)
                return send_at < other.send_at;
            if (priority != other.priority)
                return priority > other.priority;
            if (created_at != other.created_at)
                return created_at < other.created_at;
            if (id != other.id)
                return id < other.id;
            return task.get() < other.task.get();
        }
    };

    mutable std::mutex set_mu_;
    mutable std::set<SetItem> pendings_;
    mutable std::atomic<std::int64_t> earliest_send_at_ {
        std::numeric_limits<std::int64_t>::max()};
};

}  // namespace itmo_notification
