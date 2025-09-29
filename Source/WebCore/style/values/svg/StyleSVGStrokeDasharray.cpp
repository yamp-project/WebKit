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
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "StyleSVGStrokeDasharray.h"

#include "AnimationUtilities.h"
#include "CSSPrimitiveValue.h"
#include "StyleBuilderChecking.h"
#include "StyleLengthWrapper+Blending.h"
#include "StyleLengthWrapper+CSSValueConversion.h"
#include "StylePrimitiveNumericTypes+Blending.h"
#include "StylePrimitiveNumericTypes+CSSValueConversion.h"

namespace WebCore {
namespace Style {

using namespace CSS::Literals;

// MARK: - Conversion

auto CSSValueConversion<SVGStrokeDasharrayValue>::operator()(BuilderState& state, const CSSValue& value) -> SVGStrokeDasharrayValue
{
    RefPtr primitiveValue = requiredDowncast<CSSPrimitiveValue>(state, value);
    if (!primitiveValue)
        return 0_css_px;

    if (primitiveValue->isNumberOrInteger()) {
        return SVGStrokeDasharrayValueLength { SVGStrokeDasharrayValueLength::Fixed {
            toStyleFromCSSValue<Number<CSS::All, float>>(state, *primitiveValue).value
        } };
    }
    return toStyleFromCSSValue<SVGStrokeDasharrayValueLength>(state, *primitiveValue);
}

// MARK: - Blending

auto Blending<SVGStrokeDasharray>::blend(const SVGStrokeDasharray& a, const SVGStrokeDasharray& b, const BlendingContext& context) -> SVGStrokeDasharray
{
    auto aLength = a.size();
    auto bLength = b.size();
    if (!aLength || !bLength)
        return context.progress < 0.5 ? a : b;

    auto resultLength = aLength;
    if (aLength != bLength) {
        if (!remainder(std::max(aLength, bLength), std::min(aLength, bLength)))
            resultLength = std::max(aLength, bLength);
        else
            resultLength = aLength * bLength;
    }

    return SVGStrokeDasharrayList::createWithSizeFromGenerator(resultLength, [&](auto i) {
        return Style::blend(a[i % aLength], b[i % bLength], context);
    });

}

} // namespace Style
} // namespace WebCore
