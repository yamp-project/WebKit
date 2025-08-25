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

#pragma once

#include <WebCore/StyleColor.h>
#include <WebCore/StyleURL.h>

namespace WebCore {
namespace Style {

enum class ForVisitedLink : bool;

enum class SVGPaintType : uint8_t {
    RGBColor,
    None,
    CurrentColor,
    URINone,
    URICurrentColor,
    URIRGBColor,
    URI
};

// <paint> = none | <color> | <url> [none | <color>]? | context-fill | context-stroke
// NOTE: `context-fill` and `context-stroke` are not implemented.
// https://svgwg.org/svg2-draft/painting.html#SpecifyingPaint
struct SVGPaint {
    // Models Variant<CSS::Keyword::None, Color, URL, SpaceSeparatedTuple<URL, CSS::Keyword::None>, SpaceSeparatedTuple<URL, Color>>;

    SVGPaintType type;
    URL url { URL::none() };
    Color color { Color::currentColor() };

    template<typename F> decltype(auto) switchOn(F&& functor) const
    {
        switch (type) {
        case SVGPaintType::None:
            return functor(CSS::Keyword::None { });
        case SVGPaintType::RGBColor:
        case SVGPaintType::CurrentColor:
            return functor(color);
        case SVGPaintType::URINone:
            return functor(SpaceSeparatedTuple { url, CSS::Keyword::None { } });
        case SVGPaintType::URICurrentColor:
        case SVGPaintType::URIRGBColor:
            return functor(SpaceSeparatedTuple { url, color });
        case SVGPaintType::URI:
            return functor(url);
        }
        RELEASE_ASSERT_NOT_REACHED();
    }

    bool operator==(const SVGPaint&) const = default;
};

// MARK: - Conversion

template<> struct CSSValueConversion<SVGPaint> { auto operator()(BuilderState&, const CSSValue&, ForVisitedLink) -> SVGPaint; };

// MARK: - Blending

template<> struct Blending<SVGPaint> {
    auto equals(const SVGPaint&, const SVGPaint&, const RenderStyle&, const RenderStyle&) -> bool;
    auto canBlend(const SVGPaint&, const SVGPaint&) -> bool;
    auto blend(const SVGPaint&, const SVGPaint&, const RenderStyle&, const RenderStyle&, const BlendingContext&) -> SVGPaint;
};

// MARK: - Logging

WTF::TextStream& operator<<(WTF::TextStream&, SVGPaintType);

} // namespace Style
} // namespace WebCore

DEFINE_VARIANT_LIKE_CONFORMANCE(WebCore::Style::SVGPaint)
