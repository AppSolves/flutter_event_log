#ifndef FLUTTER_PLUGIN_EVENT_LOG_MANAGER_H_
#define FLUTTER_PLUGIN_EVENT_LOG_MANAGER_H_

#include <windows.h>
#include <winevt.h>

#include <flutter/encodable_value.h>
#include <flutter/event_sink.h>

#include <memory>
#include <string>
#include <vector>
#include <map>
#include <mutex>
#include <functional>

namespace event_log
{

class SubscriptionHandler;
class EventRenderer;

// Main manager class for Windows Event Log operations
class EventLogManager
{
  public:
    EventLogManager();
    ~EventLogManager();

    // Disallow copy and assign.
    EventLogManager(const EventLogManager &) = delete;
    EventLogManager &operator=(const EventLogManager &) = delete;

    // Lists all available event log channels
    std::vector<flutter::EncodableValue> ListChannels();

    // Queries historical events based on filter criteria
    std::vector<flutter::EncodableValue> QueryEvents(const flutter::EncodableMap &filter);

    // Gets a single event by its record ID
    flutter::EncodableValue GetEventById(int64_t event_record_id, const std::string *channel);

    // Subscribes to real-time events
    std::string SubscribeToEvents(const flutter::EncodableMap &filter,
                                  std::function<void(flutter::EncodableValue)> callback);

    // Unsubscribes from a subscription
    void Unsubscribe(const std::string &subscription_id);

    // Clears a channel's events
    void ClearChannel(const std::string &channel, const std::string *backup_path);

    // Gets information about a specific channel
    flutter::EncodableValue GetChannelInfo(const std::string &channel_name);

  private:
    // Builds an XPath query from the filter
    std::wstring BuildXPathQuery(const flutter::EncodableMap &filter);

    // Converts time string to FILETIME
    bool ParseIsoTime(const std::string &iso_time, FILETIME *file_time);

    // Gets error message from Windows error code
    std::string GetErrorMessage(DWORD error_code);

    std::unique_ptr<EventRenderer> renderer_;
    std::unique_ptr<SubscriptionHandler> subscription_handler_;
    std::mutex mutex_;
};

} // namespace event_log

#endif // FLUTTER_PLUGIN_EVENT_LOG_MANAGER_H_
