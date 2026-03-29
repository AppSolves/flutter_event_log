#include "encodable_sanitizer.h"

#include <windows.h>

namespace event_log
{

std::wstring SanitizeWideUtf16(const std::wstring &wide)
{
    std::wstring sanitized;
    sanitized.reserve(wide.size());

    for (size_t index = 0; index < wide.size(); ++index)
    {
        const wchar_t current = wide[index];
        if (current >= 0xD800 && current <= 0xDBFF)
        {
            if (index + 1 < wide.size())
            {
                const wchar_t next = wide[index + 1];
                if (next >= 0xDC00 && next <= 0xDFFF)
                {
                    sanitized.push_back(current);
                    sanitized.push_back(next);
                    ++index;
                    continue;
                }
            }

            sanitized.push_back(static_cast<wchar_t>(0xFFFD));
            continue;
        }

        if (current >= 0xDC00 && current <= 0xDFFF)
        {
            sanitized.push_back(static_cast<wchar_t>(0xFFFD));
            continue;
        }

        sanitized.push_back(current);
    }

    return sanitized;
}

std::string NormalizeUtf8Bytes(const std::string &utf8)
{
    if (utf8.empty())
    {
        return "";
    }

    int wide_size = MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, utf8.c_str(),
                                        static_cast<int>(utf8.size()), nullptr, 0);
    if (wide_size > 0)
    {
        return utf8;
    }

    wide_size =
        MultiByteToWideChar(CP_ACP, 0, utf8.c_str(), static_cast<int>(utf8.size()), nullptr, 0);
    if (wide_size > 0)
    {
        std::wstring ansi_wide(wide_size, 0);
        if (MultiByteToWideChar(CP_ACP, 0, utf8.c_str(), static_cast<int>(utf8.size()),
                                ansi_wide.data(), wide_size) > 0)
        {
            const auto sanitized = SanitizeWideUtf16(ansi_wide);
            const int utf8_size = WideCharToMultiByte(
                CP_UTF8, WC_ERR_INVALID_CHARS, sanitized.c_str(),
                static_cast<int>(sanitized.size()), nullptr, 0, nullptr, nullptr);
            if (utf8_size > 0)
            {
                std::string normalized(utf8_size, 0);
                if (WideCharToMultiByte(CP_UTF8, WC_ERR_INVALID_CHARS, sanitized.c_str(),
                                        static_cast<int>(sanitized.size()), normalized.data(),
                                        utf8_size, nullptr, nullptr) > 0)
                {
                    return normalized;
                }
            }
        }
    }

    std::wstring latin1_wide;
    latin1_wide.reserve(utf8.size());
    for (unsigned char byte : utf8)
    {
        latin1_wide.push_back(static_cast<wchar_t>(byte));
    }

    const auto sanitized = SanitizeWideUtf16(latin1_wide);
    const int utf8_size =
        WideCharToMultiByte(CP_UTF8, WC_ERR_INVALID_CHARS, sanitized.c_str(),
                            static_cast<int>(sanitized.size()), nullptr, 0, nullptr, nullptr);
    if (utf8_size <= 0)
    {
        return "";
    }

    std::string normalized(utf8_size, 0);
    if (WideCharToMultiByte(CP_UTF8, WC_ERR_INVALID_CHARS, sanitized.c_str(),
                            static_cast<int>(sanitized.size()), normalized.data(), utf8_size,
                            nullptr, nullptr) == 0)
    {
        return "";
    }

    return normalized;
}

flutter::EncodableValue SanitizeEncodablePayload(const flutter::EncodableValue &value)
{
    if (std::holds_alternative<std::string>(value))
    {
        return flutter::EncodableValue(NormalizeUtf8Bytes(std::get<std::string>(value)));
    }

    if (std::holds_alternative<flutter::EncodableList>(value))
    {
        flutter::EncodableList sanitized;
        for (const auto &item : std::get<flutter::EncodableList>(value))
        {
            sanitized.push_back(SanitizeEncodablePayload(item));
        }
        return flutter::EncodableValue(sanitized);
    }

    if (std::holds_alternative<flutter::EncodableMap>(value))
    {
        flutter::EncodableMap sanitized;
        for (const auto &[key, map_value] : std::get<flutter::EncodableMap>(value))
        {
            sanitized[SanitizeEncodablePayload(key)] = SanitizeEncodablePayload(map_value);
        }
        return flutter::EncodableValue(sanitized);
    }

    return value;
}

} // namespace event_log
