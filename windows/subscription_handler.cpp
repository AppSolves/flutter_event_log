#include "subscription_handler.h"
#include "event_renderer.h"
#include "event_log_error.h"

#include <sstream>
#include <chrono>

namespace event_log
{
namespace
{
flutter::EncodableMap CreateErrorDetails(const std::string &code, const std::string &message,
                                         DWORD error_code, flutter::EncodableMap extra = {})
{
    extra[flutter::EncodableValue("code")] = flutter::EncodableValue(code);
    extra[flutter::EncodableValue("message")] = flutter::EncodableValue(message);
    extra[flutter::EncodableValue("errorCode")] =
        flutter::EncodableValue(static_cast<int64_t>(error_code));
    return extra;
}

std::string GetSubscriptionErrorCode(DWORD error_code)
{
    switch (error_code)
    {
    case ERROR_ACCESS_DENIED:
        return "ACCESS_DENIED";
    case ERROR_EVT_CHANNEL_NOT_FOUND:
        return "CHANNEL_NOT_FOUND";
    case ERROR_EVT_INVALID_QUERY:
        return "INVALID_QUERY";
    default:
        return "SUBSCRIPTION_ERROR";
    }
}

std::string GetSubscriptionErrorMessage(DWORD error_code)
{
    switch (error_code)
    {
    case ERROR_ACCESS_DENIED:
        return "Access denied while creating the subscription.";
    case ERROR_NOT_SUPPORTED:
        return "This channel does not support live event subscriptions.";
    case ERROR_INVALID_PARAMETER:
        return "Windows rejected the live subscription parameters.";
    case ERROR_EVT_CHANNEL_NOT_FOUND:
        return "The specified event log channel was not found.";
    case ERROR_EVT_INVALID_QUERY:
        return "The event log query is invalid for this channel.";
    default:
        return "Failed to create subscription: " + std::to_string(error_code);
    }
}
} // namespace

SubscriptionHandler::SubscriptionHandler(EventRenderer *renderer)
    : renderer_(renderer), next_subscription_id_(1)
{
}

SubscriptionHandler::~SubscriptionHandler()
{
    CancelAllSubscriptions();
}

std::string SubscriptionHandler::CreateSubscription(
    const std::wstring &channel, const std::wstring &query,
    std::function<void(flutter::EncodableValue)> callback)
{

    std::lock_guard<std::mutex> lock(mutex_);

    auto subscription = std::make_unique<Subscription>();
    subscription->id = GenerateSubscriptionId();
    subscription->channel_name = renderer_->WideToUtf8(channel);
    subscription->callback = callback;
    subscription->renderer = renderer_;
    subscription->active = true;
    subscription->stop_event = CreateEvent(nullptr, TRUE, FALSE, nullptr);

    // Subscribe to events using the callback model
    subscription->handle = EvtSubscribe(nullptr, // Local session
                                        subscription->stop_event, channel.c_str(),
                                        query.empty() ? L"*" : query.c_str(),
                                        nullptr,            // No bookmark
                                        subscription.get(), // Pass subscription as context
                                        SubscriptionCallback, EvtSubscribeToFutureEvents);

    if (!subscription->handle)
    {
        DWORD error = GetLastError();
        CloseHandle(subscription->stop_event);
        flutter::EncodableMap details;
        if (!subscription->channel_name.empty() && subscription->channel_name != "*")
        {
            details[flutter::EncodableValue("channel")] =
                flutter::EncodableValue(subscription->channel_name);
            details[flutter::EncodableValue("operation")] = flutter::EncodableValue("subscribe");
        }
        const auto message = GetSubscriptionErrorMessage(error);
        const auto code = (error == ERROR_NOT_SUPPORTED) ? std::string("UNSUPPORTED_CHANNEL")
                                                         : GetSubscriptionErrorCode(error);
        throw EventLogError(
            code, message,
            flutter::EncodableValue(CreateErrorDetails(code, message, error, std::move(details))));
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
DWORD WINAPI SubscriptionHandler::SubscriptionCallback(EVT_SUBSCRIBE_NOTIFY_ACTION action,
                                                       PVOID user_context, EVT_HANDLE event_handle)
{

    if (action == EvtSubscribeActionError)
    {
        Subscription *subscription = static_cast<Subscription *>(user_context);
        if (!subscription || !subscription->active || !subscription->callback)
        {
            return 0;
        }

        DWORD error_code = static_cast<DWORD>(reinterpret_cast<uintptr_t>(event_handle));
        const auto code = (error_code == ERROR_NOT_SUPPORTED)
                              ? std::string("UNSUPPORTED_CHANNEL")
                              : GetSubscriptionErrorCode(error_code);
        const auto message = GetSubscriptionErrorMessage(error_code);
        flutter::EncodableMap details;
        if (!subscription->channel_name.empty() && subscription->channel_name != "*")
        {
            details[flutter::EncodableValue("channel")] =
                flutter::EncodableValue(subscription->channel_name);
            details[flutter::EncodableValue("operation")] = flutter::EncodableValue("subscribe");
        }

        flutter::EncodableMap payload;
        payload[flutter::EncodableValue("subscriptionId")] =
            flutter::EncodableValue(subscription->id);
        payload[flutter::EncodableValue("error")] = flutter::EncodableValue(
            CreateErrorDetails(code, message, error_code, std::move(details)));

        try
        {
            subscription->callback(flutter::EncodableValue(payload));
        }
        catch (...)
        {
        }
        return 0;
    }

    if (action == EvtSubscribeActionDeliver)
    {
        Subscription *subscription = static_cast<Subscription *>(user_context);

        if (subscription && subscription->active && subscription->callback &&
            subscription->renderer)
        {
            flutter::EncodableMap payload;
            payload[flutter::EncodableValue("subscriptionId")] =
                flutter::EncodableValue(subscription->id);
            payload[flutter::EncodableValue("event")] =
                subscription->renderer->RenderEvent(event_handle);

            // Call the callback with the event data
            try
            {
                subscription->callback(flutter::EncodableValue(payload));
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
