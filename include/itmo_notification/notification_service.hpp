#pragma once

#include <cstddef>
#include <cstdint>
#include <shared_mutex>
#include <optional>
#include <set>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>
#include <array>

#include "itmo_notification/due_notification.hpp"
#include "itmo_notification/notification.hpp"

namespace itmo_notification {

// In-memory планировщик уведомлений. API менять нельзя: он соответствует HTTP-ручкам.
class NotificationService {
public:
    NotificationService();
    ~NotificationService();

    NotificationService(const NotificationService&)            = delete;
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
    std::vector<DueNotification> due(std::int64_t now, std::size_t limit) const;

private:
    struct IdHash {
        using is_transparent = void;

        size_t operator()(const std::string& id) const noexcept {
            return std::hash<std::string>{}(id);
        }

        size_t operator()(std::string_view id) const noexcept {
            return std::hash<std::string_view>{}(id);
        }
    };

    static constexpr size_t SHARD_AMOUNT = 16;
    struct alignas(64) Shard {
        std::set<Notification> pendings;
        std::unordered_map<
            std::string,
            std::ranges::iterator_t<decltype(pendings)>,
            IdHash, std::equal_to<>
        > notifications;
        mutable std::shared_mutex mu;
    };

    Shard& GetShard(std::string_view id) noexcept;

    const Shard& GetShard(std::string_view id) const noexcept;

    std::array<Shard, 16> shards_;
};

}  // namespace itmo_notification
