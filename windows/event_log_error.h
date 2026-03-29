#ifndef FLUTTER_PLUGIN_EVENT_LOG_ERROR_H_
#define FLUTTER_PLUGIN_EVENT_LOG_ERROR_H_

#include <flutter/encodable_value.h>

#include <stdexcept>
#include <string>
#include <utility>

namespace event_log
{

    class EventLogError : public std::runtime_error
    {
    public:
        EventLogError(
            std::string code,
            std::string message,
            flutter::EncodableValue details = flutter::EncodableValue())
            : std::runtime_error(std::move(message)),
              code_(std::move(code)),
              details_(std::move(details))
        {
        }

        const std::string &code() const
        {
            return code_;
        }

        const flutter::EncodableValue &details() const
        {
            return details_;
        }

    private:
        std::string code_;
        flutter::EncodableValue details_;
    };

} // namespace event_log

#endif // FLUTTER_PLUGIN_EVENT_LOG_ERROR_H_
