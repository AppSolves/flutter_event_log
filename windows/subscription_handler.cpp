#include "subscription_handler.h"
#include "event_renderer.h"

#include <sstream>
#include <chrono>

namespace event_log
{

    SubscriptionHandler::SubscriptionHandler(EventRenderer *renderer)
        : renderer_(renderer), next_subscription_id_(1) {}

    SubscriptionHandler::~SubscriptionHandler()
    {
        CancelAllSubscriptions();
    }

    std::string SubscriptionHandler::CreateSubscription(
        const std::wstring &channel,
        const std::wstring &query,
        std::function<void(flutter::EncodableValue)> callback)
    {

        std::lock_guard<std::mutex> lock(mutex_);

        auto subscription = std::make_unique<Subscription>();
        subscription->id = GenerateSubscriptionId();
        subscription->callback = callback;
        subscription->active = true;
        subscription->stop_event = CreateEvent(nullptr, TRUE, FALSE, nullptr);

        // Subscribe to events using the callback model
        subscription->handle = EvtSubscribe(
            nullptr, // Local session
            subscription->stop_event,
            channel.c_str(),
            query.empty() ? L"*" : query.c_str(),
            nullptr,            // No bookmark
            subscription.get(), // Pass subscription as context
            SubscriptionCallback,
            EvtSubscribeToFutureEvents);

        if (!subscription->handle)
        {
            DWORD error = GetLastError();
            CloseHandle(subscription->stop_event);
            throw std::runtime_error("Failed to create subscription: " + std::to_string(error));
        }

        std::string id = subscription->id;
        subscriptions_[id] = std::move(subscription);

        return id;
    }

    void SubscriptionHandler::CancelSubscription(const std::string &subscription_id)
    {
        std::lock_guard<std::mutex> lock(mutex_);

        auto it = subscriptions_.find(subscription_id);
        if (it != subscriptions_.end())
        {
            auto &subscription = it->second;
            subscription->active = false;

            // Signal the stop event
            SetEvent(subscription->stop_event);

            // Close the subscription handle
            if (subscription->handle)
            {
                EvtClose(subscription->handle);
                subscription->handle = nullptr;
            }

            // Close the stop event
            if (subscription->stop_event)
            {
                CloseHandle(subscription->stop_event);
                subscription->stop_event = nullptr;
            }

            subscriptions_.erase(it);
        }
    }

    void SubscriptionHandler::CancelAllSubscriptions()
    {
        std::lock_guard<std::mutex> lock(mutex_);

        for (auto &pair : subscriptions_)
        {
            auto &subscription = pair.second;
            subscription->active = false;

            if (subscription->stop_event)
            {
                SetEvent(subscription->stop_event);
            }

            if (subscription->handle)
            {
                EvtClose(subscription->handle);
                subscription->handle = nullptr;
            }

            if (subscription->stop_event)
            {
                CloseHandle(subscription->stop_event);
                subscription->stop_event = nullptr;
            }
        }

        subscriptions_.clear();
    }

    // static
    DWORD WINAPI SubscriptionHandler::SubscriptionCallback(
        EVT_SUBSCRIBE_NOTIFY_ACTION action,
        PVOID user_context,
        EVT_HANDLE event_handle)
    {

        if (action == EvtSubscribeActionError)
        {
            // Handle error
            DWORD error_code = reinterpret_cast<DWORD>(event_handle);
            // Could log or report error
            return 0;
        }

        if (action == EvtSubscribeActionDeliver)
        {
            Subscription *subscription = static_cast<Subscription *>(user_context);

            if (subscription && subscription->active && subscription->callback)
            {
                // Get the subscription handler's renderer (we need to store it)
                // For now, create a temporary renderer
                EventRenderer temp_renderer;
                auto event_data = temp_renderer.RenderEvent(event_handle);

                // Call the callback with the event data
                try
                {
                    subscription->callback(event_data);
                }
                catch (...)
                {
                    // Swallow exceptions to prevent crashing the callback thread
                }
            }
        }

        return 0;
    }

    std::string SubscriptionHandler::GenerateSubscriptionId()
    {
        std::ostringstream oss;
        oss << "sub_" << next_subscription_id_++;
        return oss.str();
    }

} // namespace event_log
