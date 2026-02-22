#include "event_log_manager.h"
#include "event_renderer.h"
#include "subscription_handler.h"

#include <sstream>
#include <algorithm>
#include <stdexcept>

namespace event_log
{

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
            throw std::runtime_error("Failed to enumerate channels: " + GetErrorMessage(GetLastError()));
        }

        WCHAR channel_name[256];
        DWORD buffer_used = 0;

        while (EvtNextChannelPath(channel_enum, 256, channel_name, &buffer_used))
        {
            flutter::EncodableMap channel_info;

            std::string name = renderer_->WideToUtf8(channel_name);
            channel_info[flutter::EncodableValue("name")] = flutter::EncodableValue(name);

            // Get channel configuration
            EVT_HANDLE config = EvtOpenChannelConfig(nullptr, channel_name, 0);
            if (config)
            {
                DWORD buffer_size = 0;
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
            throw std::runtime_error("Failed to query events: " + GetErrorMessage(GetLastError()));
        }

        // Get max events limit
        int max_events = 1000; // Default
        auto max_it = filter.find(flutter::EncodableValue("maxEvents"));
        if (max_it != filter.end() && std::holds_alternative<int>(max_it->second))
        {
            max_events = std::get<int>(max_it->second);
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
            throw std::runtime_error("Failed to clear channel: " + GetErrorMessage(error));
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
                    oss << L"EventID=" << std::get<int>(ids[0]);
                }
                else
                {
                    oss << L"(";
                    for (size_t i = 0; i < ids.size(); i++)
                    {
                        if (i > 0)
                            oss << L" or ";
                        oss << L"EventID=" << std::get<int>(ids[i]);
                    }
                    oss << L")";
                }
                conditions.push_back(oss.str());
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
                    oss << L"Level=" << std::get<int>(levels[0]);
                }
                else
                {
                    oss << L"(";
                    for (size_t i = 0; i < levels.size(); i++)
                    {
                        if (i > 0)
                            oss << L" or ";
                        oss << L"Level=" << std::get<int>(levels[i]);
                    }
                    oss << L")";
                }
                conditions.push_back(oss.str());
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
