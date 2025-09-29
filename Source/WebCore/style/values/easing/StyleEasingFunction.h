/*
 * Copyright (C) 2024 Samuel Weinig <sam@webkit.org>
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
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#pragma once

#include <WebCore/StyleValueTypes.h>
#include <WebCore/TimingFunction.h>

namespace WebCore {

namespace CSS {
struct EasingFunction;
}

namespace Style {

// <easing-function> = linear | <cubic-bezier-easing-function> | <step-easing-function>
// https://www.w3.org/TR/css-easing-1/#typedef-easing-function
struct EasingFunction {
    Ref<TimingFunction> value;

    bool operator==(const EasingFunction& other) const
    {
        return arePointingToEqualData(value, other.value);
    }
};

// MARK: - Deprecated Conversions

Ref<TimingFunction> createTimingFunctionDeprecated(const CSS::EasingFunction&);
RefPtr<TimingFunction> createTimingFunctionDeprecated(const CSSValue&);

// MARK: - Conversion

template<> struct CSSValueConversion<EasingFunction> { auto operator()(BuilderState&, const CSSValue&) -> EasingFunction; };
template<> struct CSSValueCreation<EasingFunction> { Ref<CSSValue> operator()(CSSValuePool&, const RenderStyle&, const EasingFunction&); };

// MARK: - Serialization

template<> struct Serialize<EasingFunction> { void operator()(StringBuilder&, const CSS::SerializationContext&, const RenderStyle&, const EasingFunction&); };

// MARK: - Logging

TextStream& operator<<(TextStream&, const EasingFunction&);

} // namespace Style
} // namespace WebCore
