/*
 * Copyright (C) Research In Motion Limited 2010-2012. All rights reserved.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public License
 * along with this library; see the file COPYING.LIB.  If not, write to
 * the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 */

#include "config.h"
#include "SVGTextMetricsBuilder.h"

#include "ComplexTextController.h"
#include "FontCascadeCache.h"
#include "RenderChildIterator.h"
#include "RenderSVGInline.h"
#include "RenderSVGInlineText.h"
#include "RenderSVGText.h"
#include "WidthIterator.h"
#include <wtf/WeakPtr.h>

namespace WebCore {

SVGTextMetricsBuilder::SVGTextMetricsBuilder()
    : m_run(StringView())
    , m_textPosition(0)
    , m_isComplexText(false)
    , m_totalWidth(0)
{
}

inline bool SVGTextMetricsBuilder::currentCharacterStartsSurrogatePair() const
{
    return U16_IS_LEAD(m_run[m_textPosition]) && (m_textPosition + 1) < m_run.length() && U16_IS_TRAIL(m_run[m_textPosition + 1]);
}

template<typename Iterator>
bool SVGTextMetricsBuilder::advance(Iterator& iterator)
{
    m_textPosition += m_currentMetrics.length();
    if (m_textPosition >= m_run.length())
        return false;

    advanceIterator(iterator);
    return m_currentMetrics.length() > 0;
}

void SVGTextMetricsBuilder::advanceIterator(WidthIterator& simpleWidthIterator)
{
    GlyphBuffer glyphBuffer;
    auto before = simpleWidthIterator.currentCharacterIndex();
    simpleWidthIterator.advance(m_textPosition + 1, glyphBuffer);
    auto after = simpleWidthIterator.currentCharacterIndex();
    if (before == after) {
        m_currentMetrics = SVGTextMetrics();
        return;
    }

    float currentWidth = simpleWidthIterator.runWidthSoFar() - m_totalWidth;
    m_totalWidth = simpleWidthIterator.runWidthSoFar();

    m_currentMetrics = SVGTextMetrics(*m_text, after - before, currentWidth);
}

void SVGTextMetricsBuilder::advanceIterator(ComplexTextController& complexTextController)
{
    unsigned metricsLength = currentCharacterStartsSurrogatePair() ? 2 : 1;
    float beforeWidth = 0;
    float afterWidth = 0;

    complexTextController.advance(m_textPosition, nullptr);
    beforeWidth = complexTextController.runWidthSoFar();

    complexTextController.advance(m_textPosition + metricsLength, nullptr);
    afterWidth = complexTextController.runWidthSoFar();

    m_currentMetrics = SVGTextMetrics(*m_text, metricsLength, afterWidth - beforeWidth);
    m_complexStartToCurrentMetrics = SVGTextMetrics(*m_text, m_textPosition + metricsLength, afterWidth);

    ASSERT(m_currentMetrics.length() == metricsLength);

    // Frequent case for Arabic text: when measuring a single character the arabic isolated form is taken
    // when rendering the glyph "in context" (with it's surrounding characters) it changes due to shaping.
    // So whenever currentWidth != currentMetrics.width(), we are processing a text run whose length is
    // not equal to the sum of the individual lengths of the glyphs, when measuring them isolated.
    float currentWidth = m_complexStartToCurrentMetrics.width() - m_totalWidth;
    if (currentWidth != m_currentMetrics.width())
        m_currentMetrics.setWidth(currentWidth);

    m_totalWidth = m_complexStartToCurrentMetrics.width();
}

static inline bool shouldUseComplexTextController(FontCascade::CodePath codePathToUse, const FontCascade& scaledFont)
{
#if PLATFORM(GTK) || PLATFORM(WPE)
    if (codePathToUse != FontCascade::CodePath::Complex && scaledFont.shouldUseComplexTextControllerForSimpleText())
        return true;
#else
    UNUSED_PARAM(scaledFont);
#endif
    return codePathToUse == FontCascade::CodePath::Complex;
}

void SVGTextMetricsBuilder::initializeMeasurementWithTextRenderer(RenderSVGInlineText& text)
{
    m_text = text;
    m_textPosition = 0;
    m_currentMetrics = SVGTextMetrics();
    m_complexStartToCurrentMetrics = SVGTextMetrics();
    m_totalWidth = 0;

    const FontCascade& scaledFont = text.scaledFont();
    m_run = SVGTextMetrics::constructTextRun(text);
    m_isComplexText = shouldUseComplexTextController(scaledFont.codePath(m_run), scaledFont);

    if (m_isComplexText)
        FontCascadeCache::forCurrentThread().invalidate();

    m_canUseSimplifiedTextMeasuring = false;
    if (!m_isComplexText) {
        if (auto cachedValue = text.canUseSimplifiedTextMeasuring())
            m_canUseSimplifiedTextMeasuring = cachedValue.value();
        else {
            // Currently SVG implementation does not support first-line, so we always pass nullptr for firstLineStyle.
            // When supporting first-line, we also need to update firstLineStyle's FontCascade to be aligned with scaledFont in RenderSVGInlineText.
            m_canUseSimplifiedTextMeasuring = Layout::TextUtil::canUseSimplifiedTextMeasuring(m_run.text(), scaledFont, text.style().collapseWhiteSpace(), nullptr);
            text.setCanUseSimplifiedTextMeasuring(m_canUseSimplifiedTextMeasuring);
        }
    }
}

struct MeasureTextData {
    MeasureTextData(SVGCharacterDataMap* characterDataMap)
        : allCharactersMap(characterDataMap)
    {
    }

    SVGCharacterDataMap* allCharactersMap;
    bool processRenderer { false };
};

std::tuple<unsigned, char16_t> SVGTextMetricsBuilder::measureTextRenderer(RenderSVGInlineText& text, const MeasureTextData& data, std::tuple<unsigned, char16_t> state)
{
    SVGTextLayoutAttributes* attributes = text.layoutAttributes();
    ASSERT(attributes);
    Vector<SVGTextMetrics>& textMetricsValues = attributes->textMetricsValues();
    if (data.processRenderer) {
        if (data.allCharactersMap)
            attributes->clear();
        else
            textMetricsValues.shrink(0);
    }

    initializeMeasurementWithTextRenderer(text);

    auto& scaledFont = text.scaledFont();
    if (m_canUseSimplifiedTextMeasuring && data.processRenderer) {
        // If we are not specifying specific configuration for characters, data.allCharactersMap has only 1 entry for default case.
        // This is extremely common, and that's why we crafted a fast path here.
        // FIXME: For any cases, we are handling one character by one character in SVGTextMetrics. But many texts do not have
        // characterDataMap. We should handle multiple characters in one SVGTextMetrics. This also makes RTL work.
        // FIXME: This function is called even though width information is not changed at all. RenderSVGText / RenderSVGInlineText
        // should track the potential changes to width etc. and invoke this function only when it is actually changed.
        if (data.allCharactersMap && m_run.direction() == TextDirection::LTR && data.allCharactersMap->size() == 1) {
            constexpr unsigned defaultPosition = 1;
            ASSERT(data.allCharactersMap->contains(defaultPosition)); // "1" is the default value and always exists.
            auto characterData = data.allCharactersMap->get(defaultPosition);

            auto [valueListPosition, lastCharacter] = state;
            bool preserveWhiteSpace = text.style().whiteSpaceCollapse() == WhiteSpaceCollapse::Preserve;
            auto view = m_run.text();
            unsigned length = view.length();
            unsigned skippedCharacters = 0;
            float scalingFactor = text.scalingFactor();
            ASSERT(scalingFactor);
            float scaledHeight = scaledFont.metricsOfPrimaryFont().height() / scalingFactor;

            // m_canUseSimplifiedTextMeasuring ensures that this does not include surrogate pairs. So we do not need to consider about them.
            for (unsigned i = 0; i < length; ++i) {
                char16_t currentCharacter = view.characterAt(i);
                ASSERT(!U16_IS_LEAD(currentCharacter));
                if (currentCharacter == space && !preserveWhiteSpace && (!lastCharacter || lastCharacter == space)) {
                    if (data.processRenderer)
                        textMetricsValues.append(SVGTextMetrics(SVGTextMetrics::SkippedSpaceMetrics));
                    ++skippedCharacters;
                    continue;
                }

                if ((valueListPosition + i - skippedCharacters + 1) == defaultPosition)
                    attributes->characterDataMap().set(i + 1, characterData);

                float width = scaledFont.widthForTextUsingSimplifiedMeasuring(view.substring(i, 1), TextDirection::LTR);
                float scaledWidth = width / scalingFactor;
                textMetricsValues.append(SVGTextMetrics(1, scaledWidth, scaledHeight));
                lastCharacter = currentCharacter;
            }

            return std::tuple { valueListPosition + length - skippedCharacters, lastCharacter };
        }
    }

    if (m_isComplexText) {
        ComplexTextController iterator(scaledFont, m_run, true);
        return measureTextRendererWithIterator(iterator, text, data, state);
    }

    WidthIterator iterator(scaledFont, m_run);
    return measureTextRendererWithIterator(iterator, text, data, state);
}

template<typename Iterator>
std::tuple<unsigned, char16_t> SVGTextMetricsBuilder::measureTextRendererWithIterator(Iterator& iterator, RenderSVGInlineText& text, const MeasureTextData& data, std::tuple<unsigned, char16_t> state)
{
    auto [valueListPosition, lastCharacter] = state;
    bool preserveWhiteSpace = text.style().whiteSpaceCollapse() == WhiteSpaceCollapse::Preserve;
    auto* attributes = text.layoutAttributes();
    auto& textMetricsValues = attributes->textMetricsValues();
    int surrogatePairCharacters = 0;
    unsigned skippedCharacters = 0;
    while (advance(iterator)) {
        char16_t currentCharacter = m_run[m_textPosition];
        if (currentCharacter == space && !preserveWhiteSpace && (!lastCharacter || lastCharacter == space)) {
            if (data.processRenderer)
                textMetricsValues.append(SVGTextMetrics(SVGTextMetrics::SkippedSpaceMetrics));
            skippedCharacters += m_currentMetrics.length();
            continue;
        }

        if (data.processRenderer) {
            if (data.allCharactersMap) {
                auto it = data.allCharactersMap->find(valueListPosition + m_textPosition - skippedCharacters - surrogatePairCharacters + 1);
                if (it != data.allCharactersMap->end())
                    attributes->characterDataMap().set(m_textPosition + 1, it->value);
            }
            textMetricsValues.append(m_currentMetrics);
        }

        if (data.allCharactersMap && currentCharacterStartsSurrogatePair())
            surrogatePairCharacters++;

        lastCharacter = currentCharacter;
    }

    return std::tuple { valueListPosition + m_textPosition - skippedCharacters, lastCharacter };
}

void SVGTextMetricsBuilder::walkTree(RenderElement& start, RenderSVGInlineText* stopAtLeaf, MeasureTextData& data)
{
    unsigned valueListPosition = 0;
    char16_t lastCharacter = 0;
    CheckedPtr child = start.firstChild();
    while (child) {
        if (auto* text = dynamicDowncast<RenderSVGInlineText>(*child)) {
            data.processRenderer = !stopAtLeaf || stopAtLeaf == text;
            std::tie(valueListPosition, lastCharacter) = measureTextRenderer(*text, data, std::tuple { valueListPosition, lastCharacter });
            if (stopAtLeaf && stopAtLeaf == text)
                return;
        } else if (auto* renderer = dynamicDowncast<RenderSVGInline>(*child)) {
            // Visit children of text content elements.
            if (auto* inlineChild = renderer->firstChild()) {
                child = inlineChild;
                continue;
            }
        }
        child = child->nextInPreOrderAfterChildren(&start);
    }
}

void SVGTextMetricsBuilder::measureTextRenderer(RenderSVGText& textRoot, RenderSVGInlineText* stopAtLeaf)
{
    MeasureTextData data(nullptr);
    walkTree(textRoot, stopAtLeaf, data);
}

void SVGTextMetricsBuilder::buildMetricsAndLayoutAttributes(RenderSVGText& textRoot, RenderSVGInlineText* stopAtLeaf, SVGCharacterDataMap& allCharactersMap)
{
    MeasureTextData data(&allCharactersMap);
    walkTree(textRoot, stopAtLeaf, data);
}

}
