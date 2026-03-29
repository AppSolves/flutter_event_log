#ifndef FLUTTER_PLUGIN_EVENT_RENDERER_H_
#define FLUTTER_PLUGIN_EVENT_RENDERER_H_

#include <windows.h>
#include <winevt.h>

#include <flutter/encodable_value.h>

#include <string>

namespace event_log
{

// Handles rendering of Windows Event Log events to Flutter-compatible data
class EventRenderer
{
  public:
    EventRenderer();
    ~EventRenderer();

    // Disallow copy and assign.
    EventRenderer(const EventRenderer &) = delete;
    EventRenderer &operator=(const EventRenderer &) = delete;

    // Renders an event to an EncodableMap
    flutter::EncodableValue RenderEvent(EVT_HANDLE event_handle);

    // Gets system properties from an event
    flutter::EncodableMap GetSystemProperties(EVT_HANDLE event_handle);

    // Renders event as XML string
    std::string RenderEventAsXml(EVT_HANDLE event_handle);

    // Gets the message text for an event
    std::string GetEventMessage(EVT_HANDLE event_handle, const std::string &provider_name);

    // Converts wide string to UTF-8
    std::string WideToUtf8(const std::wstring &wide);

    // Converts UTF-8 to wide string
    std::wstring Utf8ToWide(const std::string &utf8);

  private:
    // Converts FILETIME to ISO 8601 string
    std::string FileTimeToIso8601(const FILETIME &ft);

    // Gets a string value from variant
    std::string GetStringFromVariant(const EVT_VARIANT &variant);

    // Converts GUID to string
    std::string GuidToString(const GUID &guid);

    // Converts SID to string
    std::string SidToString(PSID sid);

    EVT_HANDLE render_context_;
};

} // namespace event_log

#endif // FLUTTER_PLUGIN_EVENT_RENDERER_H_
