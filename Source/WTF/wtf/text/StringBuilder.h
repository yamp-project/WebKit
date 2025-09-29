/*
 * Copyright (C) 2009-2024 Apple Inc. All rights reserved.
 * Copyright (C) 2012 Google Inc. All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE. 
 */

#pragma once

#include <wtf/OverflowPolicy.h>
#include <wtf/SaturatedArithmetic.h>
#include <wtf/text/StringConcatenateNumbers.h>

namespace WTF {

class StringBuilder {
    // Disallow copying since we don't want to share m_buffer between two builders.
    WTF_MAKE_NONCOPYABLE(StringBuilder);
    WTF_DEPRECATED_MAKE_FAST_ALLOCATED(StringBuilder);

public:
    StringBuilder() = default;
    StringBuilder(StringBuilder&&) = default;
    StringBuilder& operator=(StringBuilder&&) = default;

    explicit StringBuilder(OverflowPolicy);

    void clear();
    void swap(StringBuilder&);

    void didOverflow();
    bool hasOverflowed() const { return m_length > String::MaxLength; }
    bool crashesOnOverflow() const { return m_shouldCrashOnOverflow; }

    template<StringTypeAdaptable... StringTypes> void append(const StringTypes&...);

    // FIXME: We should keep these overloads only if optimizations make them more efficient than the single-argument form of the variadic append above.
    WTF_EXPORT_PRIVATE void append(std::span<const char16_t>);
    WTF_EXPORT_PRIVATE void append(std::span<const Latin1Character>);
    void append(const AtomString& string) { append(string.string()); }
    void append(const String&);
    void append(StringView);
    void append(ASCIILiteral);
    void append(const char*) = delete; // Pass ASCIILiteral or span instead.
    void append(char16_t);
    void append(Latin1Character);
    void append(char character) { append(byteCast<Latin1Character>(character)); }

    template<typename... StringTypeAdapters> void appendFromAdapters(const StringTypeAdapters&...);

    // FIXME: Add a StringTypeAdapter so we can append one string builder to another with variadic append.
    void append(const StringBuilder&);

    void appendSubstring(const String&, unsigned offset, unsigned length = String::MaxLength);
    WTF_EXPORT_PRIVATE void appendQuotedJSONString(const String&);

    const String& toString() LIFETIME_BOUND;
    const String& toStringPreserveCapacity() const LIFETIME_BOUND;
    AtomString toAtomString() const;

#if USE(FOUNDATION) && defined(__OBJC__)
    RetainPtr<NSString> createNSString() const;
#endif

    bool isEmpty() const { return !m_length; }
    unsigned length() const;

    operator StringView() const LIFETIME_BOUND;
    char16_t operator[](unsigned i) const;

    bool is8Bit() const;
    std::span<const Latin1Character> span8() const LIFETIME_BOUND { return span<Latin1Character>(); }
    std::span<const char16_t> span16() const LIFETIME_BOUND { return span<char16_t>(); }
    template<typename CharacterType> std::span<const CharacterType> span() const LIFETIME_BOUND;
    
    unsigned capacity() const;
    WTF_EXPORT_PRIVATE void reserveCapacity(unsigned newCapacity);

    WTF_EXPORT_PRIVATE void shrink(unsigned newLength);
    WTF_EXPORT_PRIVATE bool shouldShrinkToFit() const;
    WTF_EXPORT_PRIVATE void shrinkToFit();

    WTF_EXPORT_PRIVATE bool containsOnlyASCII() const;

private:
    static unsigned expandedCapacity(unsigned capacity, unsigned requiredCapacity);

    template<typename AllocationCharacterType, typename CurrentCharacterType> void allocateBuffer(std::span<const CurrentCharacterType> currentCharacters, unsigned requiredCapacity);
    template<typename CharacterType> void reallocateBuffer(unsigned requiredCapacity);
    void reallocateBuffer(unsigned requiredCapacity);

    template<typename CharacterType> std::span<CharacterType> extendBufferForAppending(unsigned requiredLength);
    template<typename CharacterType> std::span<CharacterType> extendBufferForAppendingSlowCase(unsigned requiredLength);
    WTF_EXPORT_PRIVATE std::span<Latin1Character> extendBufferForAppendingLChar(unsigned requiredLength);
    WTF_EXPORT_PRIVATE std::span<char16_t> extendBufferForAppendingWithUpconvert(unsigned requiredLength);

    WTF_EXPORT_PRIVATE void reifyString() const;

    void appendFromAdapters() { /* empty base case */ }
    template<typename StringTypeAdapter, typename... StringTypeAdapters> void appendFromAdaptersSlow(const StringTypeAdapter&, const StringTypeAdapters&...);
    template<typename StringTypeAdapter> void appendFromAdapterSlow(const StringTypeAdapter&);

    mutable String m_string;
    RefPtr<StringImpl> m_buffer;
    unsigned m_length { 0 };
    bool m_shouldCrashOnOverflow { true };
};

template<> struct IntegerToStringConversionTrait<StringBuilder>;

// FIXME: Move this to StringView and make it take a StringView instead of a StringBuilder?
template<typename CharacterType> bool equal(const StringBuilder&, std::span<const CharacterType>);

// Inline function implementations.

inline StringBuilder::StringBuilder(OverflowPolicy policy)
    : m_shouldCrashOnOverflow { policy == OverflowPolicy::CrashOnOverflow }
{
}

inline void StringBuilder::clear()
{
    m_string = { };
    m_buffer = nullptr;
    m_length = 0;
    // We intentionally do not change m_shouldCrashOnOverflow.
}

inline void StringBuilder::swap(StringBuilder& other)
{
    m_string.swap(other.m_string);
    m_buffer.swap(other.m_buffer);
    std::swap(m_length, other.m_length);
    std::swap(m_shouldCrashOnOverflow, other.m_shouldCrashOnOverflow);
}

inline StringBuilder::operator StringView() const
{
    if (is8Bit())
        return span<Latin1Character>();
    return span<char16_t>();
}

inline void StringBuilder::append(char16_t character)
{
    if (m_buffer && m_length < m_buffer->length() && m_string.isNull()) {
        if (!m_buffer->is8Bit()) {
            spanConstCast<char16_t>(m_buffer->span16())[m_length++] = character;
            return;
        }
        if (isLatin1(character)) {
            spanConstCast<Latin1Character>(m_buffer->span8())[m_length++] = static_cast<Latin1Character>(character);
            return;
        }
    }
    append(WTF::span(character));
}

inline void StringBuilder::append(Latin1Character character)
{
    if (m_buffer && m_length < m_buffer->length() && m_string.isNull()) {
        if (m_buffer->is8Bit())
            spanConstCast<Latin1Character>(m_buffer->span8())[m_length++] = character;
        else
            spanConstCast<char16_t>(m_buffer->span16())[m_length++] = character;
        return;
    }
    append(WTF::span(character));
}

inline void StringBuilder::append(const String& string)
{
    // If we're appending to an empty string, and there is not a buffer (reserveCapacity has not been called)
    // then just retain the string.
    if (!m_length && !m_buffer) {
        m_string = string;
        m_length = string.length();
        return;
    }

    append(StringView { string });
}

inline void StringBuilder::append(const StringBuilder& other)
{
    // If we're appending to an empty string, and there is not a buffer (reserveCapacity has not been called)
    // then just retain the string.
    if (!m_length && !m_buffer && !other.m_string.isNull()) {
        // Use the length function here so we crash on overflow without explicit overflow checks.
        m_string = other.m_string;
        m_length = other.length();
        return;
    }

    append(StringView { other });
}

inline void StringBuilder::append(StringView string)
{
    if (string.is8Bit())
        append(string.span8());
    else
        append(string.span16());
}

inline void StringBuilder::append(ASCIILiteral string)
{
    append(string.span8());
}

inline void StringBuilder::appendSubstring(const String& string, unsigned offset, unsigned length)
{
    append(StringView { string }.substring(offset, length));
}

inline const String& StringBuilder::toString()
{
    if (m_string.isNull()) {
        shrinkToFit();
        reifyString();
    }
    return m_string;
}

inline const String& StringBuilder::toStringPreserveCapacity() const
{
    if (m_string.isNull())
        reifyString();
    return m_string;
}

inline AtomString StringBuilder::toAtomString() const
{
    if (isEmpty())
        return emptyAtom();

    // If the buffer is sufficiently over-allocated, make a new AtomString from a copy so its buffer is not so large.
    if (shouldShrinkToFit())
        return StringView { *this }.toAtomString();

    if (!m_string.isNull())
        return AtomString { m_string };

    // Use the length function here so we crash on overflow without explicit overflow checks.
    ASSERT(m_buffer);
    return { m_buffer.get(), 0, length() };
}

#if USE(FOUNDATION) && defined(__OBJC__)
inline RetainPtr<NSString> StringBuilder::createNSString() const
{
    if (isEmpty())
        return @"";

    // If the buffer is sufficiently over-allocated, make a new NSString from a copy so its buffer is not so large.
    if (shouldShrinkToFit())
        return StringView { *this }.createNSString();

    if (!m_string.isNull())
        return m_string.createNSString();

    // Use the length function here so we crash on overflow without explicit overflow checks.
    return StringView { *m_buffer }.left(length()).createNSString();
}
#endif

inline unsigned StringBuilder::length() const
{
    RELEASE_ASSERT(!hasOverflowed());
    return m_length;
}

inline unsigned StringBuilder::capacity() const
{
    return m_buffer ? m_buffer->length() : length();
}

inline char16_t StringBuilder::operator[](unsigned i) const
{
    return is8Bit() ? span8()[i] : span16()[i];
}

inline bool StringBuilder::is8Bit() const
{
    return m_buffer ? m_buffer->is8Bit() : m_string.is8Bit();
}

template<typename CharacterType> inline std::span<const CharacterType> StringBuilder::span() const
{
    if (!m_length || hasOverflowed())
        return { };
    if (!m_string.isNull()) {
        ASSERT(m_string.length() == m_length);
        return m_string.span<CharacterType>();
    }
    return m_buffer->span<CharacterType>().first(m_length);
}

template<typename StringTypeAdapter> constexpr bool stringBuilderSlowPathRequiredForAdapter = requires(const StringTypeAdapter& adapter) {
    { adapter.writeUsing(std::declval<StringBuilder&>) } -> std::same_as<void>;
};
template<typename... StringTypeAdapters> constexpr bool stringBuilderSlowPathRequired = (... || stringBuilderSlowPathRequiredForAdapter<StringTypeAdapters>);

template<typename... StringTypeAdapters> void StringBuilder::appendFromAdapters(const StringTypeAdapters&... adapters)
{
    if constexpr (stringBuilderSlowPathRequired<StringTypeAdapters...>) {
        appendFromAdaptersSlow(adapters...);
    } else {
        auto requiredLength = saturatedSum<uint32_t>(m_length, adapters.length()...);
        if (is8Bit() && are8Bit(adapters...)) {
            auto destination = extendBufferForAppendingLChar(requiredLength);
            if (!destination.data())
                return;
            stringTypeAdapterAccumulator(destination, adapters...);
        } else {
            auto destination = extendBufferForAppendingWithUpconvert(requiredLength);
            if (!destination.data())
                return;
            stringTypeAdapterAccumulator(destination, adapters...);
        }
    }
}

template<typename StringTypeAdapter> void StringBuilder::appendFromAdapterSlow(const StringTypeAdapter& adapter)
{
    if constexpr (stringBuilderSlowPathRequired<StringTypeAdapter>) {
        adapter.writeUsing(*this);
    } else {
        appendFromAdapters(adapter);
    }
}

template<typename StringTypeAdapter, typename... StringTypeAdapters> void StringBuilder::appendFromAdaptersSlow(const StringTypeAdapter& adapter, const StringTypeAdapters&... adapters)
{
    appendFromAdapterSlow(adapter);
    appendFromAdapters(adapters...);
}

template<StringTypeAdaptable... StringTypes> void StringBuilder::append(const StringTypes&... strings)
{
    appendFromAdapters(StringTypeAdapter<StringTypes>(strings)...);
}

template<typename CharacterType> bool equal(const StringBuilder& builder, std::span<const CharacterType> buffer)
{
    return builder == StringView { buffer };
}

template<> struct IntegerToStringConversionTrait<StringBuilder> {
    using ReturnType = void;
    using AdditionalArgumentType = StringBuilder;
    static void flush(std::span<const Latin1Character> characters, StringBuilder* builder) { builder->append(characters); }
};

// Helper functor useful in generic contexts where both makeString() and StringBuilder are being used.
struct SerializeUsingStringBuilder {
    StringBuilder& builder;

    using Result = void;
    template<typename... T> void operator()(T&&... args)
    {
        return builder.append(std::forward<T>(args)...);
    }
};

} // namespace WTF

using WTF::StringBuilder;
using WTF::SerializeUsingStringBuilder;
