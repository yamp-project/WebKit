/*
 * Copyright (C) 2025 Samuel Weinig <sam@webkit.org>
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

#include "config.h"
#include "StyleFontWidth.h"

#include "CSSPropertyParserConsumer+Font.h"
#include "StyleBuilderChecking.h"
#include "StylePrimitiveNumericTypes+CSSValueConversion.h"

namespace WebCore {
namespace Style {

// MARK: - Conversion

auto CSSValueConversion<FontWidth>::operator()(BuilderState& state, const CSSValue& value) -> FontWidth
{
    RefPtr primitiveValue = requiredDowncast<CSSPrimitiveValue>(state, value);
    if (!primitiveValue)
        return CSS::Keyword::Normal { };

    switch (auto valueID = primitiveValue->valueID(); valueID) {
    case CSSValueInvalid:
        return toStyleFromCSSValue<FontWidth::Percentage>(state, *primitiveValue);
    case CSSValueUltraCondensed:
        return CSS::Keyword::UltraCondensed { };
    case CSSValueExtraCondensed:
        return CSS::Keyword::ExtraCondensed { };
    case CSSValueCondensed:
        return CSS::Keyword::Condensed { };
    case CSSValueSemiCondensed:
        return CSS::Keyword::SemiCondensed { };
    case CSSValueNormal:
        return CSS::Keyword::Normal { };
    case CSSValueSemiExpanded:
        return CSS::Keyword::SemiExpanded { };
    case CSSValueExpanded:
        return CSS::Keyword::Expanded { };
    case CSSValueExtraExpanded:
        return CSS::Keyword::ExtraExpanded { };
    case CSSValueUltraExpanded:
        return CSS::Keyword::UltraExpanded { };
    default:
        if (CSSPropertyParserHelpers::isSystemFontShorthand(valueID))
            return CSS::Keyword::Normal { };

        state.setCurrentPropertyInvalidAtComputedValueTime();
        return CSS::Keyword::Normal { };
    }
}

// MARK: Blending

auto Blending<FontWidth>::blend(const FontWidth& a, const FontWidth& b, const BlendingContext& context) -> FontWidth
{
    return Style::blend(a.percentage(), b.percentage(), context);
}

} // namespace Style
} // namespace WebCore
