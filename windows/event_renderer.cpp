#include "event_renderer.h"

#include <sddl.h> // For ConvertSidToStringSidW
#include <sstream>
#include <iomanip>
#include <vector>
#include <codecvt>
#include <locale>
#include <algorithm>
#include <cctype>

namespace event_log
{
    namespace
    {
        std::string TrimTrailingWhitespace(std::string value)
        {
            value.erase(
                std::find_if(
                    value.rbegin(),
                    value.rend(),
                    [](unsigned char ch)
                    {
                        return !std::isspace(ch);
                    })
                    .base(),
                value.end());
            return value;
        }
    } // namespace

    EventRenderer::EventRenderer()
    {
        // Create render context for system properties
        render_context_ = EvtCreateRenderContext(0, nullptr, EvtRenderContextSystem);
    }

    EventRenderer::~EventRenderer()
    {
        if (render_context_)
        {
            EvtClose(render_context_);
        }
    }

    flutter::EncodableValue EventRenderer::RenderEvent(EVT_HANDLE event_handle)
    {
        auto system_props = GetSystemProperties(event_handle);

        // Optionally get XML and message
        auto xml = RenderEventAsXml(event_handle);
        std::string provider_name;
        auto provider_it = system_props.find(flutter::EncodableValue("providerName"));
        if (provider_it != system_props.end() &&
            std::holds_alternative<std::string>(provider_it->second))
        {
            provider_name = std::get<std::string>(provider_it->second);
        }

        auto message = GetEventMessage(event_handle, provider_name);

        if (!xml.empty())
        {
            system_props[flutter::EncodableValue("xml")] = flutter::EncodableValue(xml);
        }

        if (!message.empty())
        {
            system_props[flutter::EncodableValue("message")] = flutter::EncodableValue(message);
        }

        return flutter::EncodableValue(system_props);
    }

    flutter::EncodableMap EventRenderer::GetSystemProperties(EVT_HANDLE event_handle)
    {
        flutter::EncodableMap props;

        DWORD buffer_size = 0;
        DWORD buffer_used = 0;
        DWORD property_count = 0;

        // First call to get buffer size
        EvtRender(render_context_, event_handle, EvtRenderEventValues,
                  buffer_size, nullptr, &buffer_used, &property_count);

        if (GetLastError() != ERROR_INSUFFICIENT_BUFFER)
        {
            return props;
        }

        buffer_size = buffer_used;
        std::vector<BYTE> buffer(buffer_size);

        // Second call to get actual data
        if (!EvtRender(render_context_, event_handle, EvtRenderEventValues,
                       buffer_size, buffer.data(), &buffer_used, &property_count))
        {
            return props;
        }

        EVT_VARIANT *values = reinterpret_cast<EVT_VARIANT *>(buffer.data());

        // Extract system properties
        // EvtSystemProviderName = 0
        if (values[0].Type == EvtVarTypeString && values[0].StringVal)
        {
            props[flutter::EncodableValue("providerName")] =
                flutter::EncodableValue(WideToUtf8(values[0].StringVal));
        }

        // EvtSystemProviderGuid = 1
        if (values[1].Type == EvtVarTypeGuid && values[1].GuidVal)
        {
            props[flutter::EncodableValue("providerGuid")] =
                flutter::EncodableValue(GuidToString(*values[1].GuidVal));
        }

        // EvtSystemEventID = 2
        if (values[2].Type == EvtVarTypeUInt16)
        {
            props[flutter::EncodableValue("eventId")] =
                flutter::EncodableValue(static_cast<int>(values[2].UInt16Val));
        }

        // EvtSystemQualifiers = 3
        if (values[3].Type == EvtVarTypeUInt16)
        {
            props[flutter::EncodableValue("qualifiers")] =
                flutter::EncodableValue(static_cast<int>(values[3].UInt16Val));
        }

        // EvtSystemLevel = 4
        if (values[4].Type == EvtVarTypeByte)
        {
            props[flutter::EncodableValue("level")] =
                flutter::EncodableValue(static_cast<int>(values[4].ByteVal));
        }

        // EvtSystemTask = 5
        if (values[5].Type == EvtVarTypeUInt16)
        {
            props[flutter::EncodableValue("task")] =
                flutter::EncodableValue(static_cast<int>(values[5].UInt16Val));
        }

        // EvtSystemOpcode = 6
        if (values[6].Type == EvtVarTypeByte)
        {
            props[flutter::EncodableValue("opcode")] =
                flutter::EncodableValue(static_cast<int>(values[6].ByteVal));
        }

        // EvtSystemKeywords = 7
        if (values[7].Type == EvtVarTypeInt64 || values[7].Type == EvtVarTypeHexInt64)
        {
            props[flutter::EncodableValue("keywords")] =
                flutter::EncodableValue(values[7].Int64Val);
        }

        // EvtSystemTimeCreated = 8
        if (values[8].Type == EvtVarTypeFileTime)
        {
            // FileTimeVal is a ULONGLONG, cast it to FILETIME
            FILETIME ft;
            ft.dwLowDateTime = static_cast<DWORD>(values[8].FileTimeVal & 0xFFFFFFFF);
            ft.dwHighDateTime = static_cast<DWORD>(values[8].FileTimeVal >> 32);
            props[flutter::EncodableValue("timeCreated")] =
                flutter::EncodableValue(FileTimeToIso8601(ft));
        }

        // EvtSystemEventRecordId = 9
        if (values[9].Type == EvtVarTypeUInt64)
        {
            props[flutter::EncodableValue("eventRecordId")] =
                flutter::EncodableValue(static_cast<int64_t>(values[9].UInt64Val));
        }

        // EvtSystemActivityID = 10
        if (values[10].Type == EvtVarTypeGuid && values[10].GuidVal)
        {
            props[flutter::EncodableValue("activityId")] =
                flutter::EncodableValue(GuidToString(*values[10].GuidVal));
        }

        // EvtSystemRelatedActivityID = 11
        if (values[11].Type == EvtVarTypeGuid && values[11].GuidVal)
        {
            props[flutter::EncodableValue("relatedActivityId")] =
                flutter::EncodableValue(GuidToString(*values[11].GuidVal));
        }

        // EvtSystemProcessID = 12
        if (values[12].Type == EvtVarTypeUInt32)
        {
            props[flutter::EncodableValue("processId")] =
                flutter::EncodableValue(static_cast<int>(values[12].UInt32Val));
        }

        // EvtSystemThreadID = 13
        if (values[13].Type == EvtVarTypeUInt32)
        {
            props[flutter::EncodableValue("threadId")] =
                flutter::EncodableValue(static_cast<int>(values[13].UInt32Val));
        }

        // EvtSystemChannel = 14
        if (values[14].Type == EvtVarTypeString && values[14].StringVal)
        {
            props[flutter::EncodableValue("channel")] =
                flutter::EncodableValue(WideToUtf8(values[14].StringVal));
        }

        // EvtSystemComputer = 15
        if (values[15].Type == EvtVarTypeString && values[15].StringVal)
        {
            props[flutter::EncodableValue("computer")] =
                flutter::EncodableValue(WideToUtf8(values[15].StringVal));
        }

        // EvtSystemUserID = 16
        if (values[16].Type == EvtVarTypeSid && values[16].SidVal)
        {
            props[flutter::EncodableValue("userId")] =
                flutter::EncodableValue(SidToString(values[16].SidVal));
        }

        // EvtSystemVersion = 17
        if (values[17].Type == EvtVarTypeByte)
        {
            props[flutter::EncodableValue("version")] =
                flutter::EncodableValue(static_cast<int>(values[17].ByteVal));
        }

        return props;
    }

    std::string EventRenderer::RenderEventAsXml(EVT_HANDLE event_handle)
    {
        DWORD buffer_size = 0;
        DWORD buffer_used = 0;
        DWORD property_count = 0;

        // First call to get buffer size
        EvtRender(nullptr, event_handle, EvtRenderEventXml,
                  buffer_size, nullptr, &buffer_used, &property_count);

        if (GetLastError() != ERROR_INSUFFICIENT_BUFFER)
        {
            return "";
        }

        buffer_size = buffer_used;
        std::vector<WCHAR> buffer(buffer_size / sizeof(WCHAR));

        // Second call to get actual data
        if (!EvtRender(nullptr, event_handle, EvtRenderEventXml,
                       buffer_size, buffer.data(), &buffer_used, &property_count))
        {
            return "";
        }

        return WideToUtf8(buffer.data());
    }

    std::string EventRenderer::GetEventMessage(
        EVT_HANDLE event_handle,
        const std::string &provider_name)
    {
        if (provider_name.empty())
        {
            return "";
        }

        auto provider_wide = Utf8ToWide(provider_name);
        EVT_HANDLE publisher_metadata = EvtOpenPublisherMetadata(
            nullptr,
            provider_wide.c_str(),
            nullptr,
            0,
            0);
        if (!publisher_metadata)
        {
            return "";
        }

        DWORD buffer_used = 0;
        EvtFormatMessage(
            publisher_metadata,
            event_handle,
            0,
            0,
            nullptr,
            EvtFormatMessageEvent,
            0,
            nullptr,
            &buffer_used);

        if (GetLastError() != ERROR_INSUFFICIENT_BUFFER || buffer_used == 0)
        {
            EvtClose(publisher_metadata);
            return "";
        }

        std::vector<WCHAR> buffer(buffer_used);
        if (!EvtFormatMessage(
                publisher_metadata,
                event_handle,
                0,
                0,
                nullptr,
                EvtFormatMessageEvent,
                static_cast<DWORD>(buffer.size()),
                buffer.data(),
                &buffer_used))
        {
            EvtClose(publisher_metadata);
            return "";
        }

        EvtClose(publisher_metadata);
        return TrimTrailingWhitespace(WideToUtf8(buffer.data()));
    }

    std::string EventRenderer::FileTimeToIso8601(const FILETIME &ft)
    {
        SYSTEMTIME st;
        FileTimeToSystemTime(&ft, &st);

        std::ostringstream oss;
        oss << std::setfill('0')
            << std::setw(4) << st.wYear << "-"
            << std::setw(2) << st.wMonth << "-"
            << std::setw(2) << st.wDay << "T"
            << std::setw(2) << st.wHour << ":"
            << std::setw(2) << st.wMinute << ":"
            << std::setw(2) << st.wSecond << "."
            << std::setw(3) << st.wMilliseconds << "Z";

        return oss.str();
    }

    std::string EventRenderer::WideToUtf8(const std::wstring &wide)
    {
        if (wide.empty())
            return "";

        int size_needed = WideCharToMultiByte(CP_UTF8, 0, wide.c_str(),
                                              static_cast<int>(wide.size()),
                                              nullptr, 0, nullptr, nullptr);
        std::string result(size_needed, 0);
        WideCharToMultiByte(CP_UTF8, 0, wide.c_str(),
                            static_cast<int>(wide.size()),
                            &result[0], size_needed, nullptr, nullptr);
        return result;
    }

    std::wstring EventRenderer::Utf8ToWide(const std::string &utf8)
    {
        if (utf8.empty())
            return L"";

        int size_needed = MultiByteToWideChar(CP_UTF8, 0, utf8.c_str(),
                                              static_cast<int>(utf8.size()),
                                              nullptr, 0);
        std::wstring result(size_needed, 0);
        MultiByteToWideChar(CP_UTF8, 0, utf8.c_str(),
                            static_cast<int>(utf8.size()),
                            &result[0], size_needed);
        return result;
    }

    std::string EventRenderer::GetStringFromVariant(const EVT_VARIANT &variant)
    {
        if (variant.Type == EvtVarTypeString && variant.StringVal)
        {
            return WideToUtf8(variant.StringVal);
        }
        return "";
    }

    std::string EventRenderer::GuidToString(const GUID &guid)
    {
        std::ostringstream oss;
        oss << std::hex << std::setfill('0')
            << std::setw(8) << guid.Data1 << "-"
            << std::setw(4) << guid.Data2 << "-"
            << std::setw(4) << guid.Data3 << "-"
            << std::setw(2) << static_cast<int>(guid.Data4[0])
            << std::setw(2) << static_cast<int>(guid.Data4[1]) << "-"
            << std::setw(2) << static_cast<int>(guid.Data4[2])
            << std::setw(2) << static_cast<int>(guid.Data4[3])
            << std::setw(2) << static_cast<int>(guid.Data4[4])
            << std::setw(2) << static_cast<int>(guid.Data4[5])
            << std::setw(2) << static_cast<int>(guid.Data4[6])
            << std::setw(2) << static_cast<int>(guid.Data4[7]);
        return oss.str();
    }

    std::string EventRenderer::SidToString(PSID sid)
    {
        LPWSTR sid_string = nullptr;
        if (ConvertSidToStringSidW(sid, &sid_string))
        {
            std::string result = WideToUtf8(sid_string);
            LocalFree(sid_string);
            return result;
        }
        return "";
    }

} // namespace event_log
