#include "event_log_plugin.h"
#include "event_log_manager.h"

#include <flutter/method_channel.h>
#include <flutter/plugin_registrar_windows.h>
#include <flutter/standard_method_codec.h>
#include <flutter/event_stream_handler_functions.h>

#include <memory>
#include <sstream>

namespace event_log
{

    // static
    void EventLogPlugin::RegisterWithRegistrar(
        flutter::PluginRegistrarWindows *registrar)
    {
        auto method_channel =
            std::make_unique<flutter::MethodChannel<flutter::EncodableValue>>(
                registrar->messenger(), "event_log",
                &flutter::StandardMethodCodec::GetInstance());

        auto event_channel =
            std::make_unique<flutter::EventChannel<flutter::EncodableValue>>(
                registrar->messenger(), "event_log/events",
                &flutter::StandardMethodCodec::GetInstance());

        auto plugin = std::make_unique<EventLogPlugin>(
            registrar, std::move(method_channel), std::move(event_channel));

        plugin->method_channel_->SetMethodCallHandler(
            [plugin_pointer = plugin.get()](const auto &call, auto result)
            {
                plugin_pointer->HandleMethodCall(call, std::move(result));
            });

        registrar->AddPlugin(std::move(plugin));
    }

    EventLogPlugin::EventLogPlugin(
        flutter::PluginRegistrarWindows *registrar,
        std::unique_ptr<flutter::MethodChannel<flutter::EncodableValue>> method_channel,
        std::unique_ptr<flutter::EventChannel<flutter::EncodableValue>> event_channel)
        : registrar_(registrar),
          method_channel_(std::move(method_channel)),
          event_channel_(std::move(event_channel)),
          manager_(std::make_unique<EventLogManager>())
    {
        // Create stream handler using functions
        auto handler = std::make_unique<flutter::StreamHandlerFunctions<flutter::EncodableValue>>(
            [this](const flutter::EncodableValue *arguments,
                   std::unique_ptr<flutter::EventSink<flutter::EncodableValue>> &&events)
                -> std::unique_ptr<flutter::StreamHandlerError<flutter::EncodableValue>>
            {
                std::lock_guard<std::mutex> lock(event_sink_mutex_);
                event_sink_ = std::move(events);
                return nullptr;
            },
            [this](const flutter::EncodableValue *arguments)
                -> std::unique_ptr<flutter::StreamHandlerError<flutter::EncodableValue>>
            {
                std::lock_guard<std::mutex> lock(event_sink_mutex_);
                event_sink_ = nullptr;
                return nullptr;
            });

        event_channel_->SetStreamHandler(std::move(handler));
    }

    EventLogPlugin::~EventLogPlugin() {}

    void EventLogPlugin::SendEvent(const flutter::EncodableValue &event)
    {
        std::lock_guard<std::mutex> lock(event_sink_mutex_);
        if (event_sink_)
        {
            event_sink_->Success(event);
        }
    }

    void EventLogPlugin::HandleMethodCall(
        const flutter::MethodCall<flutter::EncodableValue> &method_call,
        std::unique_ptr<flutter::MethodResult<flutter::EncodableValue>> result)
    {
        const auto &method_name = method_call.method_name();

        try
        {
            if (method_name == "listChannels")
            {
                auto channels = manager_->ListChannels();
                result->Success(flutter::EncodableValue(channels));
            }
            else if (method_name == "queryEvents")
            {
                const auto *args = std::get_if<flutter::EncodableMap>(method_call.arguments());
                if (!args)
                {
                    result->Error("INVALID_ARGUMENT", "Expected map arguments");
                    return;
                }
                auto events = manager_->QueryEvents(*args);
                result->Success(flutter::EncodableValue(events));
            }
            else if (method_name == "getEventById")
            {
                const auto *args = std::get_if<flutter::EncodableMap>(method_call.arguments());
                if (!args)
                {
                    result->Error("INVALID_ARGUMENT", "Expected map arguments");
                    return;
                }

                auto it = args->find(flutter::EncodableValue("eventRecordId"));
                if (it == args->end())
                {
                    result->Error("INVALID_ARGUMENT", "Missing eventRecordId");
                    return;
                }

                int64_t record_id = std::get<int64_t>(it->second);

                const std::string *channel = nullptr;
                auto channel_it = args->find(flutter::EncodableValue("channel"));
                std::string channel_str;
                if (channel_it != args->end() && std::holds_alternative<std::string>(channel_it->second))
                {
                    channel_str = std::get<std::string>(channel_it->second);
                    channel = &channel_str;
                }

                auto event = manager_->GetEventById(record_id, channel);
                result->Success(event);
            }
            else if (method_name == "subscribeToEvents")
            {
                const auto *args = std::get_if<flutter::EncodableMap>(method_call.arguments());
                if (!args)
                {
                    result->Error("INVALID_ARGUMENT", "Expected map arguments");
                    return;
                }

                // Capture this pointer to send events safely
                auto subscription_id = manager_->SubscribeToEvents(
                    *args,
                    [this](flutter::EncodableValue event)
                    {
                        this->SendEvent(event);
                    });

                result->Success(flutter::EncodableValue(subscription_id));
            }
            else if (method_name == "unsubscribe")
            {
                const auto *args = std::get_if<flutter::EncodableMap>(method_call.arguments());
                if (!args)
                {
                    result->Error("INVALID_ARGUMENT", "Expected map arguments");
                    return;
                }

                auto it = args->find(flutter::EncodableValue("subscriptionId"));
                if (it == args->end())
                {
                    result->Error("INVALID_ARGUMENT", "Missing subscriptionId");
                    return;
                }

                std::string subscription_id = std::get<std::string>(it->second);
                manager_->Unsubscribe(subscription_id);
                result->Success();
            }
            else if (method_name == "clearChannel")
            {
                const auto *args = std::get_if<flutter::EncodableMap>(method_call.arguments());
                if (!args)
                {
                    result->Error("INVALID_ARGUMENT", "Expected map arguments");
                    return;
                }

                auto it = args->find(flutter::EncodableValue("channel"));
                if (it == args->end())
                {
                    result->Error("INVALID_ARGUMENT", "Missing channel");
                    return;
                }

                std::string channel = std::get<std::string>(it->second);

                const std::string *backup_path = nullptr;
                auto backup_it = args->find(flutter::EncodableValue("backupPath"));
                std::string backup_str;
                if (backup_it != args->end() && std::holds_alternative<std::string>(backup_it->second))
                {
                    backup_str = std::get<std::string>(backup_it->second);
                    backup_path = &backup_str;
                }

                manager_->ClearChannel(channel, backup_path);
                result->Success();
            }
            else if (method_name == "getChannelInfo")
            {
                const auto *args = std::get_if<flutter::EncodableMap>(method_call.arguments());
                if (!args)
                {
                    result->Error("INVALID_ARGUMENT", "Expected map arguments");
                    return;
                }

                auto it = args->find(flutter::EncodableValue("channelName"));
                if (it == args->end())
                {
                    result->Error("INVALID_ARGUMENT", "Missing channelName");
                    return;
                }

                std::string channel_name = std::get<std::string>(it->second);
                auto info = manager_->GetChannelInfo(channel_name);
                result->Success(info);
            }
            else
            {
                result->NotImplemented();
            }
        }
        catch (const std::exception &e)
        {
            result->Error("EXCEPTION", e.what());
        }
    }

} // namespace event_log

// C API for Flutter plugin registration
extern "C"
{
    __declspec(dllexport) void EventLogPluginRegisterWithRegistrar(
        FlutterDesktopPluginRegistrarRef registrar)
    {
        event_log::EventLogPlugin::RegisterWithRegistrar(
            flutter::PluginRegistrarManager::GetInstance()
                ->GetRegistrar<flutter::PluginRegistrarWindows>(registrar));
    }
}
