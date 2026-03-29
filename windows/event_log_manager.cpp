#include "event_log_manager.h"
#include "event_log_error.h"
#include "event_renderer.h"
#include "subscription_handler.h"

#include <sstream>
#include <algorithm>
#include <stdexcept>
#include <optional>

namespace event_log
{
    namespace
    {
        std::optional<int64_t> GetIntValue(const flutter::EncodableValue &value)
        {
            if (std::holds_alternative<int32_t>(value))
            {
                return std::get<int32_t>(value);
            }
            if (std::holds_alternative<int64_t>(value))
            {
                return std::get<int64_t>(value);
            }
            return std::nullopt;
        }

        flutter::EncodableMap CreateErrorDetails(
            DWORD error_code,
            flutter::EncodableMap extra = {})
        {
            extra[flutter::EncodableValue("errorCode")] =
                flutter::EncodableValue(static_cast<int64_t>(error_code));
            return extra;
        }

        std::string GetChannelErrorCode(DWORD error_code)
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
                return "EXCEPTION";
            }
        }

        [[noreturn]] void ThrowEventLogError(
            const std::string &code,
            const std::string &message,
            flutter::EncodableMap details = {})
        {
            throw EventLogError(
                code,
                message,
                flutter::EncodableValue(std::move(details)));
        }
    } // namespace

    EventLogManager::EventLogManager()
        : renderer_(std::make_unique<EventRenderer>()),
          subscription_handler_(std::make_unique<SubscriptionHandler>(renderer_.get())) {}

    EventLogManager::~EventLogManager() {}

    std::vector<flutter::EncodableValue> EventLogManager::ListChannels()
    {
        std::lock_guard<std::mutex> lock(mutex_);
        std::vector<flutter::EncodableValue> channels;

        EVT_HANDLE channel_enum = EvtOpenChannelEnum(nullptr, 0);
        if (!channel_enum)
        {
            const DWORD error = GetLastError();
            ThrowEventLogError(
                GetChannelErrorCode(error),
                "Failed to enumerate channels: " + GetErrorMessage(error),
                CreateErrorDetails(error));
        }

        std::vector<WCHAR> channel_name(256);
        DWORD buffer_used = static_cast<DWORD>(channel_name.size());

        while (true)
        {
            if (!EvtNextChannelPath(
                    channel_enum,
                    static_cast<DWORD>(channel_name.size()),
                    channel_name.data(),
                    &buffer_used))
            {
                const DWORD error = GetLastError();
                if (error == ERROR_NO_MORE_ITEMS)
                {
                    break;
                }
                if (error == ERROR_INSUFFICIENT_BUFFER)
                {
                    channel_name.resize(buffer_used);
                    continue;
                }

                EvtClose(channel_enum);
                ThrowEventLogError(
                    "EXCEPTION",
                    "Failed to enumerate channels: " + GetErrorMessage(error),
                    CreateErrorDetails(error));
            }

            flutter::EncodableMap channel_info;
            channel_info[flutter::EncodableValue("enabled")] =
                flutter::EncodableValue(false);

            std::string name = renderer_->WideToUtf8(channel_name.data());
            channel_info[flutter::EncodableValue("name")] = flutter::EncodableValue(name);

            // Get channel configuration
            EVT_HANDLE config = EvtOpenChannelConfig(nullptr, channel_name.data(), 0);
            if (config)
            {
                DWORD buffer_used_config = 0;
                EVT_VARIANT property_value;

                // Get enabled status
                if (EvtGetChannelConfigProperty(config, EvtChannelConfigEnabled,
                                                0, sizeof(property_value), &property_value,
                                                &buffer_used_config))
                {
                    if (property_value.Type == EvtVarTypeBoolean)
                    {
                        channel_info[flutter::EncodableValue("enabled")] =
                            flutter::EncodableValue(property_value.BooleanVal != 0);
                    }
                }

                EvtClose(config);
            }

            channels.push_back(flutter::EncodableValue(channel_info));
            buffer_used = static_cast<DWORD>(channel_name.size());
        }

        EvtClose(channel_enum);
        return channels;
    }

    std::vector<flutter::EncodableValue> EventLogManager::QueryEvents(
        const flutter::EncodableMap &filter)
    {
        std::lock_guard<std::mutex> lock(mutex_);
        std::vector<flutter::EncodableValue> events;

        // Extract filter parameters
        std::wstring channel = L"*";
        auto channel_it = filter.find(flutter::EncodableValue("channel"));
        if (channel_it != filter.end() && std::holds_alternative<std::string>(channel_it->second))
        {
            channel = renderer_->Utf8ToWide(std::get<std::string>(channel_it->second));
        }

        std::wstring query = BuildXPathQuery(filter);

        DWORD flags = EvtQueryChannelPath | EvtQueryReverseDirection;
        auto reverse_it = filter.find(flutter::EncodableValue("reverse"));
        if (reverse_it != filter.end() && std::holds_alternative<bool>(reverse_it->second))
        {
            if (!std::get<bool>(reverse_it->second))
            {
                flags = EvtQueryChannelPath | EvtQueryForwardDirection;
            }
        }

        // Execute query
        EVT_HANDLE query_handle = EvtQuery(nullptr, channel.c_str(), query.c_str(), flags);
        if (!query_handle)
        {
            const DWORD error = GetLastError();
            flutter::EncodableMap details;
            auto channel_utf8 = renderer_->WideToUtf8(channel);
            if (!channel_utf8.empty() && channel_utf8 != "*")
            {
                details[flutter::EncodableValue("channel")] =
                    flutter::EncodableValue(channel_utf8);
            }
            ThrowEventLogError(
                GetChannelErrorCode(error),
                "Failed to query events: " + GetErrorMessage(error),
                CreateErrorDetails(error, std::move(details)));
        }

        // Get max events limit
        int max_events = 1000; // Default
        auto max_it = filter.find(flutter::EncodableValue("maxEvents"));
        if (max_it != filter.end())
        {
            if (auto max_value = GetIntValue(max_it->second))
            {
                max_events = static_cast<int>(*max_value);
            }
        }

        // Retrieve events
        DWORD returned = 0;
        EVT_HANDLE event_handles[100];
        int total_retrieved = 0;

        while (EvtNext(query_handle, 100, event_handles, INFINITE, 0, &returned))
        {
            for (DWORD i = 0; i < returned && total_retrieved < max_events; i++)
            {
                auto event_data = renderer_->RenderEvent(event_handles[i]);
                events.push_back(event_data);
                EvtClose(event_handles[i]);
                total_retrieved++;
            }

            if (total_retrieved >= max_events)
            {
                break;
            }
        }

        EvtClose(query_handle);
        return events;
    }

    flutter::EncodableValue EventLogManager::GetEventById(
        int64_t event_record_id,
        const std::string *channel)
    {
        std::lock_guard<std::mutex> lock(mutex_);

        std::wstring channel_path = L"*";
        if (channel)
        {
            channel_path = renderer_->Utf8ToWide(*channel);
        }

        // Build query for specific event record ID
        std::wostringstream query;
        query << L"*[System[EventRecordID=" << event_record_id << L"]]";

        EVT_HANDLE query_handle = EvtQuery(
            nullptr, channel_path.c_str(), query.str().c_str(),
            EvtQueryChannelPath | EvtQueryForwardDirection);

        if (!query_handle)
        {
            const DWORD error = GetLastError();
            if (error == ERROR_EVT_CHANNEL_NOT_FOUND)
            {
                flutter::EncodableMap details;
                if (channel)
                {
                    details[flutter::EncodableValue("channel")] =
                        flutter::EncodableValue(*channel);
                }
                ThrowEventLogError(
                    "CHANNEL_NOT_FOUND",
                    "Failed to query event by record ID: " + GetErrorMessage(error),
                    CreateErrorDetails(error, std::move(details)));
            }

            return flutter::EncodableValue(); // null
        }

        EVT_HANDLE event_handle;
        DWORD returned = 0;

        if (EvtNext(query_handle, 1, &event_handle, INFINITE, 0, &returned) && returned > 0)
        {
            auto event_data = renderer_->RenderEvent(event_handle);
            EvtClose(event_handle);
            EvtClose(query_handle);
            return event_data;
        }

        EvtClose(query_handle);
        return flutter::EncodableValue(); // null
    }

    std::string EventLogManager::SubscribeToEvents(
        const flutter::EncodableMap &filter,
        std::function<void(flutter::EncodableValue)> callback)
    {

        std::wstring channel = L"*";
        auto channel_it = filter.find(flutter::EncodableValue("channel"));
        if (channel_it != filter.end() && std::holds_alternative<std::string>(channel_it->second))
        {
            channel = renderer_->Utf8ToWide(std::get<std::string>(channel_it->second));
        }

        std::wstring query = BuildXPathQuery(filter);

        return subscription_handler_->CreateSubscription(channel, query, callback);
    }

    void EventLogManager::Unsubscribe(const std::string &subscription_id)
    {
        subscription_handler_->CancelSubscription(subscription_id);
    }

    void EventLogManager::ClearChannel(
        const std::string &channel,
        const std::string *backup_path)
    {
        std::lock_guard<std::mutex> lock(mutex_);

        std::wstring channel_wide = renderer_->Utf8ToWide(channel);
        const WCHAR *backup = nullptr;
        std::wstring backup_wide;

        if (backup_path)
        {
            backup_wide = renderer_->Utf8ToWide(*backup_path);
            backup = backup_wide.c_str();
        }

        if (!EvtClearLog(nullptr, channel_wide.c_str(), backup, 0))
        {
            DWORD error = GetLastError();
            flutter::EncodableMap details;
            details[flutter::EncodableValue("channel")] =
                flutter::EncodableValue(channel);
            ThrowEventLogError(
                GetChannelErrorCode(error),
                "Failed to clear channel: " + GetErrorMessage(error),
                CreateErrorDetails(error, std::move(details)));
        }
    }

    flutter::EncodableValue EventLogManager::GetChannelInfo(const std::string &channel_name)
    {
        std::lock_guard<std::mutex> lock(mutex_);

        std::wstring channel_wide = renderer_->Utf8ToWide(channel_name);
        EVT_HANDLE config = EvtOpenChannelConfig(nullptr, channel_wide.c_str(), 0);

        if (!config)
        {
            return flutter::EncodableValue(); // null
        }

        flutter::EncodableMap info;
        info[flutter::EncodableValue("name")] = flutter::EncodableValue(channel_name);
        info[flutter::EncodableValue("enabled")] = flutter::EncodableValue(false);

        EVT_VARIANT property_value;
        DWORD buffer_used = 0;

        // Get enabled status
        if (EvtGetChannelConfigProperty(config, EvtChannelConfigEnabled,
                                        0, sizeof(property_value), &property_value,
                                        &buffer_used))
        {
            if (property_value.Type == EvtVarTypeBoolean)
            {
                info[flutter::EncodableValue("enabled")] =
                    flutter::EncodableValue(property_value.BooleanVal != 0);
            }
        }

        // Get channel type
        if (EvtGetChannelConfigProperty(config, EvtChannelConfigType,
                                        0, sizeof(property_value), &property_value,
                                        &buffer_used))
        {
            if (property_value.Type == EvtVarTypeUInt32)
            {
                std::string type;
                switch (property_value.UInt32Val)
                {
                case EvtChannelTypeAdmin:
                    type = "Admin";
                    break;
                case EvtChannelTypeOperational:
                    type = "Operational";
                    break;
                case EvtChannelTypeAnalytic:
                    type = "Analytic";
                    break;
                case EvtChannelTypeDebug:
                    type = "Debug";
                    break;
                default:
                    type = "Unknown";
                    break;
                }
                info[flutter::EncodableValue("type")] = flutter::EncodableValue(type);
            }
        }

        // Get log file path
        if (EvtGetChannelConfigProperty(config, EvtChannelLoggingConfigLogFilePath,
                                        0, sizeof(property_value), &property_value,
                                        &buffer_used))
        {
            if (property_value.Type == EvtVarTypeString && property_value.StringVal)
            {
                info[flutter::EncodableValue("logFilePath")] =
                    flutter::EncodableValue(renderer_->WideToUtf8(property_value.StringVal));
            }
        }

        EvtClose(config);
        return flutter::EncodableValue(info);
    }

    std::wstring EventLogManager::BuildXPathQuery(const flutter::EncodableMap &filter)
    {
        // Check for custom XPath query
        auto xpath_it = filter.find(flutter::EncodableValue("xpathQuery"));
        if (xpath_it != filter.end() && std::holds_alternative<std::string>(xpath_it->second))
        {
            return renderer_->Utf8ToWide(std::get<std::string>(xpath_it->second));
        }

        std::vector<std::wstring> conditions;

        // Event IDs
        auto event_ids_it = filter.find(flutter::EncodableValue("eventIds"));
        if (event_ids_it != filter.end() &&
            std::holds_alternative<flutter::EncodableList>(event_ids_it->second))
        {
            auto &ids = std::get<flutter::EncodableList>(event_ids_it->second);
            if (!ids.empty())
            {
                std::wostringstream oss;
                if (ids.size() == 1)
                {
                    if (auto event_id = GetIntValue(ids[0]))
                    {
                        oss << L"EventID=" << *event_id;
                    }
                }
                else
                {
                    oss << L"(";
                    bool first = true;
                    for (size_t i = 0; i < ids.size(); i++)
                    {
                        auto event_id = GetIntValue(ids[i]);
                        if (!event_id)
                        {
                            continue;
                        }
                        if (!first)
                            oss << L" or ";
                        oss << L"EventID=" << *event_id;
                        first = false;
                    }
                    oss << L")";
                }
                if (!oss.str().empty() && oss.str() != L"()")
                {
                    conditions.push_back(oss.str());
                }
            }
        }

        // Levels
        auto levels_it = filter.find(flutter::EncodableValue("levels"));
        if (levels_it != filter.end() &&
            std::holds_alternative<flutter::EncodableList>(levels_it->second))
        {
            auto &levels = std::get<flutter::EncodableList>(levels_it->second);
            if (!levels.empty())
            {
                std::wostringstream oss;
                if (levels.size() == 1)
                {
                    if (auto level = GetIntValue(levels[0]))
                    {
                        oss << L"Level=" << *level;
                    }
                }
                else
                {
                    oss << L"(";
                    bool first = true;
                    for (size_t i = 0; i < levels.size(); i++)
                    {
                        auto level = GetIntValue(levels[i]);
                        if (!level)
                        {
                            continue;
                        }
                        if (!first)
                            oss << L" or ";
                        oss << L"Level=" << *level;
                        first = false;
                    }
                    oss << L")";
                }
                if (!oss.str().empty() && oss.str() != L"()")
                {
                    conditions.push_back(oss.str());
                }
            }
        }

        // Providers
        auto providers_it = filter.find(flutter::EncodableValue("providers"));
        if (providers_it != filter.end() &&
            std::holds_alternative<flutter::EncodableList>(providers_it->second))
        {
            auto &providers = std::get<flutter::EncodableList>(providers_it->second);
            if (!providers.empty())
            {
                std::wostringstream oss;
                if (providers.size() == 1)
                {
                    std::wstring provider = renderer_->Utf8ToWide(std::get<std::string>(providers[0]));
                    oss << L"Provider[@Name='" << provider << L"']";
                }
                else
                {
                    oss << L"(";
                    for (size_t i = 0; i < providers.size(); i++)
                    {
                        if (i > 0)
                            oss << L" or ";
                        std::wstring provider = renderer_->Utf8ToWide(std::get<std::string>(providers[i]));
                        oss << L"Provider[@Name='" << provider << L"']";
                    }
                    oss << L")";
                }
                conditions.push_back(oss.str());
            }
        }

        // Time range
        auto start_time_it = filter.find(flutter::EncodableValue("startTime"));
        auto end_time_it = filter.find(flutter::EncodableValue("endTime"));

        if (start_time_it != filter.end() || end_time_it != filter.end())
        {
            std::wostringstream oss;
            oss << L"TimeCreated[";

            bool has_start = false;
            if (start_time_it != filter.end() &&
                std::holds_alternative<std::string>(start_time_it->second))
            {
                std::string start_str = std::get<std::string>(start_time_it->second);
                std::wstring start_wide = renderer_->Utf8ToWide(start_str);
                oss << L"@SystemTime>='" << start_wide << L"'";
                has_start = true;
            }

            if (end_time_it != filter.end() &&
                std::holds_alternative<std::string>(end_time_it->second))
            {
                if (has_start)
                    oss << L" and ";
                std::string end_str = std::get<std::string>(end_time_it->second);
                std::wstring end_wide = renderer_->Utf8ToWide(end_str);
                oss << L"@SystemTime<='" << end_wide << L"'";
            }

            oss << L"]";
            conditions.push_back(oss.str());
        }

        // Keywords
        auto keywords_it = filter.find(flutter::EncodableValue("keywords"));
        if (keywords_it != filter.end())
        {
            if (auto keywords = GetIntValue(keywords_it->second))
            {
                std::wostringstream oss;
                oss << L"band(Keywords," << *keywords << L")";
                conditions.push_back(oss.str());
            }
        }

        // Build final query
        if (conditions.empty())
        {
            return L"*";
        }

        std::wostringstream query;
        query << L"*[System[";
        for (size_t i = 0; i < conditions.size(); i++)
        {
            if (i > 0)
                query << L" and ";
            query << conditions[i];
        }
        query << L"]]";

        return query.str();
    }

    bool EventLogManager::ParseIsoTime(const std::string &iso_time, FILETIME *file_time)
    {
        // Basic ISO 8601 parsing - implement if needed
        return false;
    }

    std::string EventLogManager::GetErrorMessage(DWORD error_code)
    {
        LPWSTR message_buffer = nullptr;
        size_t size = FormatMessageW(
            FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS,
            nullptr, error_code, MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
            reinterpret_cast<LPWSTR>(&message_buffer), 0, nullptr);

        std::string message;
        if (size > 0)
        {
            message = renderer_->WideToUtf8(message_buffer);
            LocalFree(message_buffer);
        }
        else
        {
            message = "Unknown error: " + std::to_string(error_code);
        }

        return message;
    }

} // namespace event_log
