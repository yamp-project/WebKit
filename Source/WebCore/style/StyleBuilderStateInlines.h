/**
 * Copyright (C) 2019-2023 Apple Inc. All rights reserved.
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

#pragma once

#include "RenderStyle.h"
#include "RenderStyleSetters.h"
#include "StyleBuilderState.h"
#include "StyleFontSizeFunctions.h"

namespace WebCore {
namespace Style {

inline const FontCascadeDescription& BuilderState::fontDescription() { return m_style.fontDescription(); }
inline void BuilderState::setFontDescription(FontCascadeDescription&& description) { m_fontDirty |= m_style.setFontDescriptionWithoutUpdate(WTFMove(description)); }

inline const FontCascadeDescription& BuilderState::parentFontDescription() { return parentStyle().fontDescription(); }
inline void BuilderState::setUsedZoom(float zoom) { m_fontDirty |= m_style.setUsedZoom(zoom); }
inline void BuilderState::setTextOrientation(TextOrientation orientation) { m_fontDirty |= m_style.setTextOrientation(orientation); }
inline void BuilderState::setWritingMode(StyleWritingMode mode) { m_fontDirty |= m_style.setWritingMode(mode); }
inline void BuilderState::setZoom(float zoom) { m_fontDirty |= m_style.setZoom(zoom); }

inline void BuilderState::setFontDescriptionKeywordSizeFromIdentifier(CSSValueID identifier)
{
    if (m_style.fontDescription().keywordSizeAsIdentifier() == identifier)
        return;

    m_fontDirty = true;
    m_style.mutableFontDescriptionWithoutUpdate().setKeywordSizeFromIdentifier(identifier);
}

inline void BuilderState::setFontDescriptionIsAbsoluteSize(bool isAbsoluteSize)
{
    if (m_style.fontDescription().isAbsoluteSize() == isAbsoluteSize)
        return;

    m_fontDirty = true;
    m_style.mutableFontDescriptionWithoutUpdate().setIsAbsoluteSize(isAbsoluteSize);
}

inline void BuilderState::setFontDescriptionFontSize(float fontSize)
{
    if (m_style.fontDescription().specifiedSize() != fontSize) {
        m_fontDirty = true;
        m_style.mutableFontDescriptionWithoutUpdate().setSpecifiedSize(fontSize);
    }

    SUPPRESS_UNCOUNTED_ARG auto computedSize = Style::computedFontSizeFromSpecifiedSize(fontSize, m_style.fontDescription().isAbsoluteSize(), useSVGZoomRules(), &style(), document());
    if (m_style.fontDescription().computedSize() != computedSize) {
        m_fontDirty = true;
        m_style.mutableFontDescriptionWithoutUpdate().setComputedSize(computedSize);
    }
}

inline void BuilderState::setFontDescriptionFamilies(RefCountedFixedVector<AtomString>& families)
{
    if (m_style.fontDescription().families() == families)
        return;

    m_fontDirty = true;
    auto& fontCascade = m_style.mutableFontCascadeWithoutUpdate();
    fontCascade.mutableFontDescription().setFamilies(families);
    fontCascade.updateUseBackslashAsYenSymbol();
}

inline void BuilderState::setFontDescriptionFamilies(Vector<AtomString>& families)
{
    if (m_style.fontDescription().families() == families)
        return;

    m_fontDirty = true;
    auto& fontCascade = m_style.mutableFontCascadeWithoutUpdate();
    fontCascade.mutableFontDescription().setFamilies(families);
    fontCascade.updateUseBackslashAsYenSymbol();
}

inline void BuilderState::setFontDescriptionIsSpecifiedFont(bool isSpecifiedFont)
{
    if (m_style.fontDescription().isSpecifiedFont() == isSpecifiedFont)
        return;

    m_fontDirty = true;
    m_style.mutableFontDescriptionWithoutUpdate().setIsSpecifiedFont(isSpecifiedFont);
}

inline void BuilderState::setFontDescriptionFeatureSettings(FontFeatureSettings&& featureSettings)
{
    if (m_style.fontDescription().featureSettings() == featureSettings)
        return;

    m_fontDirty = true;
    auto& fontCascade = m_style.mutableFontCascadeWithoutUpdate();
    fontCascade.mutableFontDescription().setFeatureSettings(WTFMove(featureSettings));
    fontCascade.updateRequiresShaping();
}

inline void BuilderState::setFontDescriptionFontPalette(Style::FontPalette&& fontPalette)
{
    if (m_style.fontDescription().fontPalette() == fontPalette.platform())
        return;

    m_fontDirty = true;
    m_style.mutableFontDescriptionWithoutUpdate().setFontPalette(fontPalette.platform());
}

inline void BuilderState::setFontDescriptionFontSizeAdjust(FontSizeAdjust fontSizeAdjust)
{
    if (m_style.fontDescription().fontSizeAdjust() == fontSizeAdjust)
        return;

    m_fontDirty = true;
    m_style.mutableFontDescriptionWithoutUpdate().setFontSizeAdjust(WTFMove(fontSizeAdjust));
}

inline void BuilderState::setFontDescriptionFontSmoothing(FontSmoothingMode fontSmoothing)
{
    if (m_style.fontDescription().fontSmoothing() == fontSmoothing)
        return;

    m_fontDirty = true;
    m_style.mutableFontDescriptionWithoutUpdate().setFontSmoothing(WTFMove(fontSmoothing));
}

inline void BuilderState::setFontDescriptionFontSynthesisSmallCaps(FontSynthesisLonghandValue fontSynthesisSmallCaps)
{
    if (m_style.fontDescription().fontSynthesisSmallCaps() == fontSynthesisSmallCaps)
        return;

    m_fontDirty = true;
    m_style.mutableFontDescriptionWithoutUpdate().setFontSynthesisSmallCaps(WTFMove(fontSynthesisSmallCaps));
}

inline void BuilderState::setFontDescriptionFontSynthesisStyle(FontSynthesisLonghandValue fontSynthesisStyle)
{
    if (m_style.fontDescription().fontSynthesisStyle() == fontSynthesisStyle)
        return;

    m_fontDirty = true;
    m_style.mutableFontDescriptionWithoutUpdate().setFontSynthesisStyle(fontSynthesisStyle);
}

inline void BuilderState::setFontDescriptionFontSynthesisWeight(FontSynthesisLonghandValue fontSynthesisWeight)
{
    if (m_style.fontDescription().fontSynthesisWeight() == fontSynthesisWeight)
        return;

    m_fontDirty = true;
    m_style.mutableFontDescriptionWithoutUpdate().setFontSynthesisWeight(fontSynthesisWeight);
}

inline void BuilderState::setFontDescriptionKerning(Kerning kerning)
{
    if (m_style.fontDescription().kerning() == kerning)
        return;

    m_fontDirty = true;
    auto& fontCascade = m_style.mutableFontCascadeWithoutUpdate();
    fontCascade.mutableFontDescription().setKerning(kerning);
    fontCascade.updateEnableKerning();
}

inline void BuilderState::setFontDescriptionOpticalSizing(FontOpticalSizing opticalSizing)
{
    if (m_style.fontDescription().opticalSizing() == opticalSizing)
        return;

    m_fontDirty = true;
    m_style.mutableFontDescriptionWithoutUpdate().setOpticalSizing(opticalSizing);
}

inline void BuilderState::setFontDescriptionSpecifiedLocale(const AtomString& specifiedLocale)
{
    if (m_style.fontDescription().specifiedLocale() == specifiedLocale)
        return;

    m_fontDirty = true;
    m_style.mutableFontDescriptionWithoutUpdate().setSpecifiedLocale(specifiedLocale);
}

inline void BuilderState::setFontDescriptionTextAutospace(TextAutospace textAutospace)
{
    if (m_style.fontDescription().textAutospace() == textAutospace)
        return;

    m_fontDirty = true;
    m_style.mutableFontDescriptionWithoutUpdate().setTextAutospace(textAutospace);
}

inline void BuilderState::setFontDescriptionTextRenderingMode(TextRenderingMode textRenderingMode)
{
    if (m_style.fontDescription().textRenderingMode() == textRenderingMode)
        return;

    m_fontDirty = true;
    auto& fontCascade = m_style.mutableFontCascadeWithoutUpdate();
    fontCascade.mutableFontDescription().setTextRenderingMode(textRenderingMode);
    fontCascade.updateEnableKerning();
    fontCascade.updateRequiresShaping();
}

inline void BuilderState::setFontDescriptionTextSpacingTrim(TextSpacingTrim textSpacingTrim)
{
    if (m_style.fontDescription().textSpacingTrim() == textSpacingTrim)
        return;

    m_fontDirty = true;
    m_style.mutableFontDescriptionWithoutUpdate().setTextSpacingTrim(textSpacingTrim);
}

inline void BuilderState::setFontDescriptionVariantCaps(FontVariantCaps variantCaps)
{
    if (m_style.fontDescription().variantCaps() == variantCaps)
        return;

    m_fontDirty = true;
    auto& fontCascade = m_style.mutableFontCascadeWithoutUpdate();
    fontCascade.mutableFontDescription().setVariantCaps(variantCaps);
    fontCascade.updateRequiresShaping();
}

inline void BuilderState::setFontDescriptionVariantEmoji(FontVariantEmoji variantEmoji)
{
    if (m_style.fontDescription().variantEmoji() == variantEmoji)
        return;

    m_fontDirty = true;
    auto& fontCascade = m_style.mutableFontCascadeWithoutUpdate();
    fontCascade.mutableFontDescription().setVariantEmoji(variantEmoji);
    fontCascade.updateRequiresShaping();
}

inline void BuilderState::setFontDescriptionVariantPosition(FontVariantPosition variantPosition)
{
    if (m_style.fontDescription().variantPosition() == variantPosition)
        return;

    m_fontDirty = true;
    auto& fontCascade = m_style.mutableFontCascadeWithoutUpdate();
    fontCascade.mutableFontDescription().setVariantPosition(variantPosition);
    fontCascade.updateRequiresShaping();
}

inline void BuilderState::setFontDescriptionVariationSettings(FontVariationSettings&& variationSettings)
{
    if (m_style.fontDescription().variationSettings() == variationSettings)
        return;

    m_fontDirty = true;
    m_style.mutableFontDescriptionWithoutUpdate().setVariationSettings(WTFMove(variationSettings));
}

inline void BuilderState::setFontDescriptionWeight(FontSelectionValue weight)
{
    if (m_style.fontDescription().weight() == weight)
        return;

    m_fontDirty = true;
    m_style.mutableFontDescriptionWithoutUpdate().setWeight(weight);
}

inline void BuilderState::setFontDescriptionWidth(FontWidth width)
{
    if (m_style.fontDescription().width() == width.platform())
        return;

    m_fontDirty = true;
    m_style.mutableFontDescriptionWithoutUpdate().setWidth(width.platform());
}

inline void BuilderState::setFontDescriptionVariantAlternates(const FontVariantAlternates& variantAlternates)
{
    if (m_style.fontDescription().variantAlternates() == variantAlternates)
        return;

    m_fontDirty = true;
    auto& fontCascade = m_style.mutableFontCascadeWithoutUpdate();
    fontCascade.mutableFontDescription().setVariantAlternates(variantAlternates);
    fontCascade.updateRequiresShaping();
}

inline void BuilderState::setFontDescriptionVariantEastAsianVariant(FontVariantEastAsianVariant variantEastAsianVariant)
{
    if (m_style.fontDescription().variantEastAsianVariant() == variantEastAsianVariant)
        return;

    m_fontDirty = true;
    auto& fontCascade = m_style.mutableFontCascadeWithoutUpdate();
    fontCascade.mutableFontDescription().setVariantEastAsianVariant(variantEastAsianVariant);
    fontCascade.updateRequiresShaping();
}

inline void BuilderState::setFontDescriptionVariantEastAsianWidth(FontVariantEastAsianWidth variantEastAsianWidth)
{
    if (m_style.fontDescription().variantEastAsianWidth() == variantEastAsianWidth)
        return;

    m_fontDirty = true;
    auto& fontCascade = m_style.mutableFontCascadeWithoutUpdate();
    fontCascade.mutableFontDescription().setVariantEastAsianWidth(variantEastAsianWidth);
    fontCascade.updateRequiresShaping();
}

inline void BuilderState::setFontDescriptionVariantEastAsianRuby(FontVariantEastAsianRuby variantEastAsianRuby)
{
    if (m_style.fontDescription().variantEastAsianRuby() == variantEastAsianRuby)
        return;

    m_fontDirty = true;
    auto& fontCascade = m_style.mutableFontCascadeWithoutUpdate();
    fontCascade.mutableFontDescription().setVariantEastAsianRuby(variantEastAsianRuby);
    fontCascade.updateRequiresShaping();
}

inline void BuilderState::setFontDescriptionKeywordSize(unsigned keywordSize)
{
    if (m_style.fontDescription().keywordSize() == keywordSize)
        return;

    m_fontDirty = true;
    m_style.mutableFontDescriptionWithoutUpdate().setKeywordSize(keywordSize);
}

inline void BuilderState::setFontDescriptionVariantCommonLigatures(FontVariantLigatures variantCommonLigatures)
{
    if (m_style.fontDescription().variantCommonLigatures() == variantCommonLigatures)
        return;

    m_fontDirty = true;
    auto& fontCascade = m_style.mutableFontCascadeWithoutUpdate();
    fontCascade.mutableFontDescription().setVariantCommonLigatures(variantCommonLigatures);
    fontCascade.updateRequiresShaping();
}

inline void BuilderState::setFontDescriptionVariantDiscretionaryLigatures(FontVariantLigatures variantDiscretionaryLigatures)
{
    if (m_style.fontDescription().variantDiscretionaryLigatures() == variantDiscretionaryLigatures)
        return;

    m_fontDirty = true;
    auto& fontCascade = m_style.mutableFontCascadeWithoutUpdate();
    fontCascade.mutableFontDescription().setVariantDiscretionaryLigatures(variantDiscretionaryLigatures);
    fontCascade.updateRequiresShaping();
}

inline void BuilderState::setFontDescriptionVariantHistoricalLigatures(FontVariantLigatures variantHistoricalLigatures)
{
    if (m_style.fontDescription().variantHistoricalLigatures() == variantHistoricalLigatures)
        return;

    m_fontDirty = true;
    auto& fontCascade = m_style.mutableFontCascadeWithoutUpdate();
    fontCascade.mutableFontDescription().setVariantHistoricalLigatures(variantHistoricalLigatures);
    fontCascade.updateRequiresShaping();
}

inline void BuilderState::setFontDescriptionVariantContextualAlternates(FontVariantLigatures variantContextualAlternates)
{
    if (m_style.fontDescription().variantContextualAlternates() == variantContextualAlternates)
        return;

    m_fontDirty = true;
    auto& fontCascade = m_style.mutableFontCascadeWithoutUpdate();
    fontCascade.mutableFontDescription().setVariantContextualAlternates(variantContextualAlternates);
    fontCascade.updateRequiresShaping();
}

inline void BuilderState::setFontDescriptionVariantNumericFigure(FontVariantNumericFigure variantNumericFigure)
{
    if (m_style.fontDescription().variantNumericFigure() == variantNumericFigure)
        return;

    m_fontDirty = true;
    auto& fontCascade = m_style.mutableFontCascadeWithoutUpdate();
    fontCascade.mutableFontDescription().setVariantNumericFigure(variantNumericFigure);
    fontCascade.updateRequiresShaping();
}

inline void BuilderState::setFontDescriptionVariantNumericSpacing(FontVariantNumericSpacing variantNumericSpacing)
{
    if (m_style.fontDescription().variantNumericSpacing() == variantNumericSpacing)
        return;

    m_fontDirty = true;
    auto& fontCascade = m_style.mutableFontCascadeWithoutUpdate();
    fontCascade.mutableFontDescription().setVariantNumericSpacing(variantNumericSpacing);
    fontCascade.updateRequiresShaping();
}

inline void BuilderState::setFontDescriptionVariantNumericFraction(FontVariantNumericFraction variantNumericFraction)
{
    if (m_style.fontDescription().variantNumericFraction() == variantNumericFraction)
        return;

    m_fontDirty = true;
    auto& fontCascade = m_style.mutableFontCascadeWithoutUpdate();
    fontCascade.mutableFontDescription().setVariantNumericFraction(variantNumericFraction);
    fontCascade.updateRequiresShaping();
}

inline void BuilderState::setFontDescriptionVariantNumericOrdinal(FontVariantNumericOrdinal variantNumericOrdinal)
{
    if (m_style.fontDescription().variantNumericOrdinal() == variantNumericOrdinal)
        return;

    m_fontDirty = true;
    auto& fontCascade = m_style.mutableFontCascadeWithoutUpdate();
    fontCascade.mutableFontDescription().setVariantNumericOrdinal(variantNumericOrdinal);
    fontCascade.updateRequiresShaping();
}

inline void BuilderState::setFontDescriptionVariantNumericSlashedZero(FontVariantNumericSlashedZero variantNumericSlashedZero)
{
    if (m_style.fontDescription().variantNumericSlashedZero() == variantNumericSlashedZero)
        return;

    m_fontDirty = true;
    auto& fontCascade = m_style.mutableFontCascadeWithoutUpdate();
    fontCascade.mutableFontDescription().setVariantNumericSlashedZero(variantNumericSlashedZero);
    fontCascade.updateRequiresShaping();
}

}
}
