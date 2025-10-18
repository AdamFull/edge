#pragma once

#include <variant>
#include <memory>
#include <chrono>
#include <atomic>
#include <mutex>
#include <queue>

#include "foundation/enum_flags.h"

namespace edge {
    // Generic event concepts - works with any flag type
    template<typename FlagsType>
    concept EventFlags = requires(FlagsType flags) {
        // Must support bitwise operations and testing
        { flags.test_any(flags) } -> std::convertible_to<bool>;
        { flags | flags } -> std::convertible_to<FlagsType>;
        { flags& flags } -> std::convertible_to<FlagsType>;
    };

    // Generic event concept - works with any flag type
    template<typename T, typename FlagsType>
    concept Event = requires {
        { T::tag_flags } -> std::convertible_to<FlagsType>;
        requires EventFlags<FlagsType>;
    };

    enum class EventPriority : uint8_t {
        eCritical = 3,
        eHigh = 2,
        eNormal = 1,
        eLow = 0
    };

    template<typename FlagsType, Event<FlagsType>... SupportedEvents>
        requires EventFlags<FlagsType>
    class EventDispatcher {
    public:
        using flags_type = FlagsType;
        using event_variant_t = std::variant<SupportedEvents...>;
        using event_handler_t = void(*)(const event_variant_t&, void* user_data);
        using listener_id_t = uint64_t;

        static constexpr listener_id_t INVALID_LISTENER_ID = 0;

        EventDispatcher() {
            listeners_.reserve(64);
            for (auto& indices : event_listener_indices_) {
                indices.reserve(16);
            }
        }

        // Add a listener with event tag filtering - returns unique listener ID
        template<auto... ListenFlags>
        auto add_listener(event_handler_t handler, void* user_data = nullptr) -> listener_id_t {
            listener_id_t id = next_listener_id_.fetch_add(1, std::memory_order_acq_rel);
            listeners_.emplace_back(id, handler, user_data, make_flags<ListenFlags...>());
            rebuild_listener_indices();
            return id;
        }

        // Remove a listener by ID - returns true if found and removed
        auto remove_listener(listener_id_t id) -> bool {
            if (id == INVALID_LISTENER_ID) {
                return false;
            }

            auto it = std::find_if(listeners_.begin(), listeners_.end(),
                [id](const ListenerInfo& info) {
                    return info.id == id;
                });

            if (it != listeners_.end()) {
                listeners_.erase(it);
                rebuild_listener_indices();
                return true;
            }

            return false;
        }

        // Remove listeners by matching handler and user_data
        auto remove_listeners(event_handler_t handler, void* user_data = nullptr) -> size_t {
            size_t removed_count = 0;

            auto it = listeners_.begin();
            while (it != listeners_.end()) {
                if (it->handler == handler && it->user_data == user_data) {
                    it = listeners_.erase(it);
                    ++removed_count;
                }
                else {
                    ++it;
                }
            }

            if (removed_count > 0) {
                rebuild_listener_indices();
            }

            return removed_count;
        }

        // Remove all listeners that accept specific tag(s)
        template<auto... ListenFlags>
        auto remove_listeners() -> size_t {
            auto tags = make_flags<ListenFlags...>();
            size_t removed_count = 0;

            auto it = listeners_.begin();
            while (it != listeners_.end()) {
                if (it->accepted_tags.test_any(tags)) {
                    it = listeners_.erase(it);
                    ++removed_count;
                }
                else {
                    ++it;
                }
            }

            if (removed_count > 0) {
                rebuild_listener_indices();
            }

            return removed_count;
        }

        // Remove all listeners
        auto remove_all_listeners() -> void {
            listeners_.clear();
            rebuild_listener_indices();
        }

        // Check if a listener ID is valid/exists
        auto has_listener(listener_id_t id) const -> bool {
            if (id == INVALID_LISTENER_ID) {
                return false;
            }

            return std::any_of(listeners_.begin(), listeners_.end(),
                [id](const ListenerInfo& info) {
                    return info.id == id;
                });
        }

        // Emit an event immediately (optimized hot path)
        template<Event<FlagsType> E>
            requires (std::same_as<E, SupportedEvents> || ...)
        auto emit(const E& event) -> void {
            constexpr size_t event_idx = get_event_index<E>();
            event_variant_t event_variant(event);

            // Only call listeners that can handle this event type
            for (size_t listener_idx : event_listener_indices_[event_idx]) {
                const auto& listener = listeners_[listener_idx];
                listener.handler(event_variant, listener.user_data);
            }
        }

        // Defer an event for later processing with priority
        template<Event<FlagsType> E>
            requires (std::same_as<E, SupportedEvents> || ...)
        auto defer(E event, EventPriority priority = EventPriority::eNormal) -> void {
            uint64_t seq = sequence_counter_.fetch_add(1, std::memory_order_acq_rel);
            std::lock_guard<std::mutex> lock(deferred_events_mutex_);
            deferred_events_.emplace(event_variant_t(std::move(event)), priority, seq);
        }

        // Process deferred events (highest priority first)
        void process_events() {
            std::queue<DeferredEvent> events_to_process;

            // Move all events from priority queue to temporary queue
            {
                std::lock_guard<std::mutex> lock(deferred_events_mutex_);
                while (!deferred_events_.empty()) {
                    events_to_process.push(deferred_events_.top());
                    deferred_events_.pop();
                }
            }

            // Process events outside of lock
            while (!events_to_process.empty()) {
                const auto& deferred_event = events_to_process.front();
                std::visit([this](const auto& e) {
                    this->emit(e);
                    }, deferred_event.event);
                events_to_process.pop();
            }
        }

        // Get total pending event count (thread-safe)
        auto pending_event_count() const -> size_t {
            std::lock_guard<std::mutex> lock(deferred_events_mutex_);
            return deferred_events_.size();
        }

        // Clear all deferred events
        auto clear_events() -> void {
            std::lock_guard<std::mutex> lock(deferred_events_mutex_);
            while (!deferred_events_.empty()) {
                deferred_events_.pop();
            }
        }

        // Clear events of specific priority level
        auto clear_events(EventPriority priority) -> void {
            std::lock_guard<std::mutex> lock(deferred_events_mutex_);

            std::priority_queue<DeferredEvent> temp_queue;
            while (!deferred_events_.empty()) {
                if (deferred_events_.top().priority != priority) {
                    temp_queue.push(deferred_events_.top());
                }
                deferred_events_.pop();
            }

            deferred_events_ = std::move(temp_queue);
        }

        // Get listener count
        auto listener_count() const -> size_t {
            return listeners_.size();
        }

        // Memory usage statistics
        auto memory_usage_bytes() const -> size_t {
            size_t usage = sizeof(*this);
            usage += listeners_.capacity() * sizeof(ListenerInfo);
            for (const auto& indices : event_listener_indices_) {
                usage += indices.capacity() * sizeof(size_t);
            }

            // Estimate priority queue memory usage
            {
                std::lock_guard<std::mutex> lock(deferred_events_mutex_);
                usage += deferred_events_.size() * sizeof(DeferredEvent);
            }
            return usage;
        }
    private:
        struct DeferredEvent {
            event_variant_t event;
            EventPriority priority = EventPriority::eNormal;
            std::chrono::steady_clock::time_point timestamp;
            uint64_t sequence_id; // For stable ordering within same priority

            DeferredEvent() = default;
            DeferredEvent(event_variant_t e, EventPriority p, uint64_t seq)
                : event(std::move(e)), priority(p), timestamp(std::chrono::steady_clock::now()), sequence_id(seq) {}

            // Priority queue orders by highest priority first, then by earliest timestamp
            bool operator<(const DeferredEvent& other) const {
                if (priority != other.priority) {
                    return static_cast<int>(priority) < static_cast<int>(other.priority); // Lower priority value = lower priority
                }
                if (timestamp != other.timestamp) {
                    return timestamp > other.timestamp; // Earlier timestamp = higher priority
                }
                return sequence_id > other.sequence_id; // Earlier sequence = higher priority
            }
        };

        struct ListenerInfo {
            listener_id_t id;
            event_handler_t handler;
            void* user_data;
            FlagsType accepted_tags;

            ListenerInfo(listener_id_t listener_id, event_handler_t h, void* data, FlagsType tags)
                : id(listener_id), handler(h), user_data(data), accepted_tags(tags) {}
        };

        mi::Vector<ListenerInfo> listeners_;
        std::priority_queue<DeferredEvent> deferred_events_;
        mutable std::mutex deferred_events_mutex_; // For thread-safe access to deferred events
        std::atomic<uint64_t> sequence_counter_{ 0 };

        // Pre-computed listener indices for each event type
        std::array<mi::Vector<size_t>, sizeof...(SupportedEvents)> event_listener_indices_;

        // Thread-safe listener ID generation
        std::atomic<listener_id_t> next_listener_id_{ 1 };

        // Get variant index for event type
        template<Event<FlagsType> E>
        static constexpr auto get_event_index() -> size_t {
            size_t index = 0;
            [[maybe_unused]] auto result = ((std::same_as<E, SupportedEvents> || (++index, false)) || ...);
            return index;
        }

        // Rebuild listener indices when listeners change
        auto rebuild_listener_indices() -> void {
            // Clear all indices
            for (auto& indices : event_listener_indices_) {
                indices.clear();
            }

            // For each listener, determine which events it can handle
            for (size_t listener_idx = 0; listener_idx < listeners_.size(); ++listener_idx) {
                const auto& listener = listeners_[listener_idx];

                // Check each supported event type
                size_t event_idx = 0;
                ((check_listener_for_event<SupportedEvents>(listener_idx, listener.accepted_tags, event_idx++)), ...);
            }
        }

        template<Event<FlagsType> E>
        auto check_listener_for_event(size_t listener_idx, const FlagsType& accepted_tags, size_t event_idx) -> void {
            // Check if listener accepts this event type using flag operations
            if (accepted_tags.test_any(E::tag_flags)) {
                event_listener_indices_[event_idx].push_back(listener_idx);
            }
        }

        template<auto... ListenFlags>
        static auto make_flags() -> FlagsType {
            static_assert(sizeof...(ListenFlags) > 0, "At least one flag value required");

            // Ensure all values are of the same enum type
            using enum_type = std::common_type_t<decltype(ListenFlags)...>;
            static_assert((std::same_as<decltype(ListenFlags), enum_type> && ...), "All flag values must be of the same enum type");
            static_assert(foundation::EnumFlag<enum_type>, "Enum type must satisfy EnumFlag concept");

            return (foundation::Flags<enum_type>{} | ... | foundation::Flags<enum_type>(ListenFlags));
        }
    };
}