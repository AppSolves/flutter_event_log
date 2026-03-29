#ifndef FLUTTER_PLUGIN_EVENT_LOG_PLUGIN_H_
#define FLUTTER_PLUGIN_EVENT_LOG_PLUGIN_H_

#include <flutter/method_channel.h>
#include <flutter/plugin_registrar_windows.h>
#include <flutter/event_channel.h>

#include <memory>
#include <string>
#include <mutex>

namespace event_log
{

class EventLogManager;

class EventLogPlugin : public flutter::Plugin
{
  public:
    static void RegisterWithRegistrar(flutter::PluginRegistrarWindows *registrar);

    EventLogPlugin(flutter::PluginRegistrarWindows *registrar,
                   std::unique_ptr<flutter::MethodChannel<flutter::EncodableValue>> method_channel,
                   std::unique_ptr<flutter::EventChannel<flutter::EncodableValue>> event_channel);

    virtual ~EventLogPlugin();

    // Disallow copy and assign.
    EventLogPlugin(const EventLogPlugin &) = delete;
    EventLogPlugin &operator=(const EventLogPlugin &) = delete;

  private:
    // Handles method calls from Dart.
    void HandleMethodCall(const flutter::MethodCall<flutter::EncodableValue> &method_call,
                          std::unique_ptr<flutter::MethodResult<flutter::EncodableValue>> result);

    // Sends an event to the Dart side via the event channel
    void SendEvent(const flutter::EncodableValue &event);

    flutter::PluginRegistrarWindows *registrar_;
    std::unique_ptr<flutter::MethodChannel<flutter::EncodableValue>> method_channel_;
    std::unique_ptr<flutter::EventChannel<flutter::EncodableValue>> event_channel_;
    std::unique_ptr<EventLogManager> manager_;

    std::mutex event_sink_mutex_;
    std::unique_ptr<flutter::EventSink<flutter::EncodableValue>> event_sink_;
};

} // namespace event_log

#endif // FLUTTER_PLUGIN_EVENT_LOG_PLUGIN_H_
