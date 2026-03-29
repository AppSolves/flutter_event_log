#ifndef FLUTTER_PLUGIN_ENCODABLE_SANITIZER_H_
#define FLUTTER_PLUGIN_ENCODABLE_SANITIZER_H_

#include <flutter/encodable_value.h>

#include <string>

namespace event_log
{

std::wstring SanitizeWideUtf16(const std::wstring &wide);

std::string NormalizeUtf8Bytes(const std::string &utf8);

flutter::EncodableValue SanitizeEncodablePayload(const flutter::EncodableValue &value);

} // namespace event_log

#endif // FLUTTER_PLUGIN_ENCODABLE_SANITIZER_H_
