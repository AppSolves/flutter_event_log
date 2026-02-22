#ifndef FLUTTER_PLUGIN_SUBSCRIPTION_HANDLER_H_
#define FLUTTER_PLUGIN_SUBSCRIPTION_HANDLER_H_

#include <windows.h>
#include <winevt.h>

#include <flutter/encodable_value.h>

#include <memory>
#include <string>
#include <map>
#include <mutex>
#include <functional>
#include <thread>

namespace event_log
{

    class EventRenderer;

    // Handles active event subscriptions
    class SubscriptionHandler
    {
    public:
        SubscriptionHandler(EventRenderer *renderer);
        ~SubscriptionHandler();

        // Disallow copy and assign.
        SubscriptionHandler(const SubscriptionHandler &) = delete;
        SubscriptionHandler &operator=(const SubscriptionHandler &) = delete;

        // Creates a new subscription
        std::string CreateSubscription(
            const std::wstring &channel,
            const std::wstring &query,
            std::function<void(flutter::EncodableValue)> callback);

        // Cancels a subscription
        void CancelSubscription(const std::string &subscription_id);

        // Cancels all subscriptions
        void CancelAllSubscriptions();

    private:
        struct Subscription
        {
            std::string id;
            EVT_HANDLE handle;
            std::function<void(flutter::EncodableValue)> callback;
            std::thread thread;
            bool active;
            HANDLE stop_event;
        };

        // Subscription callback (static for Windows API)
        static DWORD WINAPI SubscriptionCallback(
            EVT_SUBSCRIBE_NOTIFY_ACTION action,
            PVOID user_context,
            EVT_HANDLE event_handle);

        // Generates a unique subscription ID
        std::string GenerateSubscriptionId();

        EventRenderer *renderer_;
        std::map<std::string, std::unique_ptr<Subscription>> subscriptions_;
        std::mutex mutex_;
        int next_subscription_id_;
    };

} // namespace event_log

#endif // FLUTTER_PLUGIN_SUBSCRIPTION_HANDLER_H_
