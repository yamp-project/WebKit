/*
 * Copyright (C) 2015 Igalia S.L.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS''
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "PlatformPasteboard.h"

#if USE(LIBWPE)

#include "Pasteboard.h"
#include <wpe/wpe.h>
#include <wtf/Assertions.h>
#include <wtf/text/WTFString.h>

namespace WebCore {

PlatformPasteboard::PlatformPasteboard(const String&)
    : m_pasteboard(wpe_pasteboard_get_singleton())
{
    ASSERT(m_pasteboard);
}

PlatformPasteboard::PlatformPasteboard()
    : m_pasteboard(wpe_pasteboard_get_singleton())
{
    ASSERT(m_pasteboard);
}

void PlatformPasteboard::performAsDataOwner(DataOwnerType, NOESCAPE Function<void()>&& actions)
{
    actions();
}

int64_t PlatformPasteboard::changeCount() const
{
    return m_changeCount;
}

void PlatformPasteboard::getTypes(Vector<String>& types) const
{
    struct wpe_pasteboard_string_vector pasteboardTypes = { nullptr, 0 };
    wpe_pasteboard_get_types(m_pasteboard, &pasteboardTypes);
    for (auto& typeString : unsafeMakeSpan(pasteboardTypes.strings, pasteboardTypes.length)) {
        const auto length = std::min(static_cast<size_t>(typeString.length), std::numeric_limits<size_t>::max());
        types.append(String(unsafeMakeSpan(typeString.data, length)));
    }

    wpe_pasteboard_string_vector_free(&pasteboardTypes);
}

String PlatformPasteboard::readString(size_t, const String& type) const
{
    struct wpe_pasteboard_string string = { nullptr, 0 };
    wpe_pasteboard_get_string(m_pasteboard, type.utf8().data(), &string);
    if (!string.length)
        return String();

    const auto length = std::min(static_cast<size_t>(string.length), std::numeric_limits<size_t>::max());
    String returnValue(unsafeMakeSpan(string.data, length));

    wpe_pasteboard_string_free(&string);
    return returnValue;
}

void PlatformPasteboard::write(const PasteboardWebContent& content)
{
    static constexpr auto plainText = "text/plain;charset=utf-8"_s;
    static constexpr auto htmlText = "text/html"_s;

    CString textString = content.text.utf8();
    CString markupString = content.markup.utf8();

    IGNORE_CLANG_WARNINGS_BEGIN("unsafe-buffer-usage-in-libc-call")
    std::array<struct wpe_pasteboard_string_pair, 2> pairs = { {
        { { nullptr, 0 }, { nullptr, 0 } },
        { { nullptr, 0 }, { nullptr, 0 } },
    } };
    wpe_pasteboard_string_initialize(&pairs[0].type, plainText, strlen(plainText));
    wpe_pasteboard_string_initialize(&pairs[0].string, textString.data(), textString.length());
    wpe_pasteboard_string_initialize(&pairs[1].type, htmlText, strlen(htmlText));
    wpe_pasteboard_string_initialize(&pairs[1].string, markupString.data(), markupString.length());
    struct wpe_pasteboard_string_map map = { pairs.data(), pairs.size() };
    IGNORE_CLANG_WARNINGS_END

    wpe_pasteboard_write(m_pasteboard, &map);
    m_changeCount++;

    wpe_pasteboard_string_free(&pairs[0].type);
    wpe_pasteboard_string_free(&pairs[0].string);
    wpe_pasteboard_string_free(&pairs[1].type);
    wpe_pasteboard_string_free(&pairs[1].string);
}

void PlatformPasteboard::write(const String& type, const String& string)
{
    struct wpe_pasteboard_string_pair pairs[] = {
        { { nullptr, 0 }, { nullptr, 0 } },
    };

    auto typeUTF8 = type.utf8();
    auto stringUTF8 = string.utf8();
    wpe_pasteboard_string_initialize(&pairs[0].type, typeUTF8.data(), typeUTF8.length());
    wpe_pasteboard_string_initialize(&pairs[0].string, stringUTF8.data(), stringUTF8.length());
    struct wpe_pasteboard_string_map map = { pairs, 1 };

    wpe_pasteboard_write(m_pasteboard, &map);
    m_changeCount++;

    wpe_pasteboard_string_free(&pairs[0].type);
    wpe_pasteboard_string_free(&pairs[0].string);
}

Vector<String> PlatformPasteboard::typesSafeForDOMToReadAndWrite(const String&) const
{
    return { };
}

int64_t PlatformPasteboard::write(const PasteboardCustomData& customData, PasteboardDataLifetime)
{
    PasteboardWebContent contents;
    customData.forEachPlatformStringOrBuffer([&contents] (auto& type, auto& stringOrBuffer) {
        if (std::holds_alternative<String>(stringOrBuffer)) {
            if (type.startsWith("text/plain"_s))
                contents.text = std::get<String>(stringOrBuffer);
            else if (type == "text/html"_s)
                contents.markup = std::get<String>(stringOrBuffer);
        }
    });
    if (contents.text.isNull() && contents.markup.isNull())
        return m_changeCount;

    write(contents);
    return m_changeCount;
}

int64_t PlatformPasteboard::write(const Vector<PasteboardCustomData>& data, PasteboardDataLifetime)
{
    if (data.isEmpty() || data.size() > 1)
        return m_changeCount;

    return write(data[0]);
}

} // namespace WebCore

#endif // USE(LIBWPE)
