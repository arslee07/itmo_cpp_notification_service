#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
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
    static constexpr std::size_t ShardsSize = 16;

    static std::size_t GetShardIdx(std::string_view id)
    {
        return std::hash<std::string_view> {}(id) & (ShardsSize - 1);
    }

    std::set<Notification> pendings_;

    struct Hasher
    {
        using is_transparent = void;

        std::size_t operator()(const std::string& str) const noexcept
        {
            return std::hash<std::string> {}(str);
        }

        std::size_t operator()(std::string_view str) const noexcept
        {
            return std::hash<std::string_view> {}(str);
        }
    };

    struct Shard
    {
        std::unordered_map<std::string,
                           std::ranges::iterator_t<decltype(pendings_)>,
                           Hasher,
                           std::equal_to<>>
            notifications;
        mutable std::shared_mutex mu;
    };
    std::array<Shard, ShardsSize> shards_;

    mutable std::shared_mutex set_mu_;
};

}  // namespace itmo_notification
