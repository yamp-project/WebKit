/*
 * Copyright (C) 1999 Antti Koivisto (koivisto@kde.org)
 * Copyright (C) 2004-2023 Apple Inc. All rights reserved.
 * Copyright (C) 2025 Samuel Weinig <sam@webkit.org>
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
 *
 */

#include "config.h"
#include "StyleRareInheritedData.h"

#include "RenderStyleInlines.h"
#include "RenderStyleConstants.h"
#include "RenderStyleDifference.h"
#include "StyleFilterData.h"
#include "StyleImage.h"
#include "StylePrimitiveNumericTypes+Logging.h"
#include <wtf/PointerComparison.h>

namespace WebCore {

struct GreaterThanOrSameSizeAsStyleRareInheritedData : public RefCounted<GreaterThanOrSameSizeAsStyleRareInheritedData> {
    float firstFloat;
    void* styleImage;
    Style::Color firstColor;
    Style::Color colors[10];
    Style::ScrollbarColor scrollbarColor;
    Style::DynamicRangeLimit dynamicRangeLimit;
    void* ownPtrs[1];
    AtomString atomStrings[5];
    void* refPtrs[3];
    float secondFloat;
    Style::TextEmphasisStyle textEmphasisStyle;
    Style::TextIndent textIndent;
    Style::TextUnderlineOffset offset;
    TextEdge textEdges[2];
    Length length;
    void* customPropertyDataRefs[1];
    unsigned bitfields[7];
    short pagedMediaShorts[2];
    TabSize tabSize;
    short hyphenationShorts[3];

#if ENABLE(TEXT_AUTOSIZING)
    Style::TextSizeAdjust textSizeAdjust;
#endif

#if ENABLE(TOUCH_EVENTS)
    Style::Color tapHighlightColor;
#endif

#if ENABLE(DARK_MODE_CSS)
    Style::ColorScheme colorScheme;
#endif
    Style::ListStyleType listStyleType;
    Style::BlockEllipsis blockEllipsis;
};

static_assert(sizeof(StyleRareInheritedData) <= sizeof(GreaterThanOrSameSizeAsStyleRareInheritedData), "StyleRareInheritedData should bit pack");

DEFINE_ALLOCATOR_WITH_HEAP_IDENTIFIER(StyleRareInheritedData);

StyleRareInheritedData::StyleRareInheritedData()
    : usedZoom(RenderStyle::initialZoom())
    , listStyleImage(RenderStyle::initialListStyleImage())
    , textStrokeWidth(RenderStyle::initialTextStrokeWidth())
    , textStrokeColor(RenderStyle::initialTextStrokeColor())
    , textFillColor(RenderStyle::initialTextFillColor())
    , textEmphasisColor(RenderStyle::initialTextEmphasisColor())
    , visitedLinkTextStrokeColor(RenderStyle::initialTextStrokeColor())
    , visitedLinkTextFillColor(RenderStyle::initialTextFillColor())
    , visitedLinkTextEmphasisColor(RenderStyle::initialTextEmphasisColor())
    , caretColor(Style::Color::currentColor())
    , visitedLinkCaretColor(Style::Color::currentColor())
    , accentColor(Style::Color::currentColor())
    , scrollbarColor(RenderStyle::initialScrollbarColor())
    , dynamicRangeLimit(RenderStyle::initialDynamicRangeLimit())
    , textShadow(RenderStyle::initialTextShadow())
    , cursorImages(RenderStyle::initialCursor().images)
    , textEmphasisStyle(RenderStyle::initialTextEmphasisStyle())
    , textIndent(RenderStyle::initialTextIndent())
    , textUnderlineOffset(RenderStyle::initialTextUnderlineOffset())
    , textBoxEdge(RenderStyle::initialTextBoxEdge())
    , lineFitEdge(RenderStyle::initialLineFitEdge())
    , miterLimit(RenderStyle::initialStrokeMiterLimit())
    , customProperties(Style::CustomPropertyData::create())
    , widows(RenderStyle::initialWidows())
    , orphans(RenderStyle::initialOrphans())
    , textSecurity(static_cast<unsigned>(RenderStyle::initialTextSecurity()))
    , userModify(static_cast<unsigned>(UserModify::ReadOnly))
    , wordBreak(static_cast<unsigned>(RenderStyle::initialWordBreak()))
    , overflowWrap(static_cast<unsigned>(RenderStyle::initialOverflowWrap()))
    , nbspMode(static_cast<unsigned>(NBSPMode::Normal))
    , lineBreak(static_cast<unsigned>(LineBreak::Auto))
    , userSelect(static_cast<unsigned>(RenderStyle::initialUserSelect()))
    , hyphens(static_cast<unsigned>(Hyphens::Manual))
    , textCombine(static_cast<unsigned>(RenderStyle::initialTextCombine()))
    , textEmphasisPosition(static_cast<unsigned>(RenderStyle::initialTextEmphasisPosition().toRaw()))
    , textUnderlinePosition(static_cast<unsigned>(RenderStyle::initialTextUnderlinePosition().toRaw()))
    , lineBoxContain(static_cast<unsigned>(RenderStyle::initialLineBoxContain().toRaw()))
    , imageOrientation(RenderStyle::initialImageOrientation())
    , imageRendering(static_cast<unsigned>(RenderStyle::initialImageRendering()))
    , lineSnap(static_cast<unsigned>(RenderStyle::initialLineSnap()))
    , lineAlign(static_cast<unsigned>(RenderStyle::initialLineAlign()))
#if ENABLE(WEBKIT_OVERFLOW_SCROLLING_CSS_PROPERTY)
    , webkitOverflowScrolling(static_cast<unsigned>(RenderStyle::initialOverflowScrolling()))
#endif
    , textAlignLast(static_cast<unsigned>(RenderStyle::initialTextAlignLast()))
    , textJustify(static_cast<unsigned>(RenderStyle::initialTextJustify()))
    , textDecorationSkipInk(static_cast<unsigned>(RenderStyle::initialTextDecorationSkipInk()))
    , mathShift(static_cast<unsigned>(RenderStyle::initialMathShift()))
    , mathStyle(static_cast<unsigned>(RenderStyle::initialMathStyle()))
    , rubyPosition(static_cast<unsigned>(RenderStyle::initialRubyPosition()))
    , rubyAlign(static_cast<unsigned>(RenderStyle::initialRubyAlign()))
    , rubyOverhang(static_cast<unsigned>(RenderStyle::initialRubyOverhang()))
    , textZoom(static_cast<unsigned>(RenderStyle::initialTextZoom()))
#if ENABLE(WEBKIT_TOUCH_CALLOUT_CSS_PROPERTY)
    , webkitTouchCallout(static_cast<unsigned>(RenderStyle::initialTouchCallout()))
#endif
    , hangingPunctuation(RenderStyle::initialHangingPunctuation().toRaw())
    , paintOrder(static_cast<unsigned>(RenderStyle::initialPaintOrder()))
    , capStyle(static_cast<unsigned>(RenderStyle::initialCapStyle()))
    , joinStyle(static_cast<unsigned>(RenderStyle::initialJoinStyle()))
    , hasSetStrokeWidth(false)
    , hasSetStrokeColor(false)
    , hasAutoCaretColor(true)
    , hasVisitedLinkAutoCaretColor(true)
    , hasAutoAccentColor(true)
    , effectiveInert(false)
    , isInSubtreeWithBlendMode(false)
    , isForceHidden(false)
    , usedContentVisibility(static_cast<unsigned>(ContentVisibility::Visible))
    , autoRevealsWhenFound(false)
    , insideDefaultButton(false)
    , insideSubmitButton(false)
#if HAVE(CORE_MATERIAL)
    , usedAppleVisualEffectForSubtree(static_cast<unsigned>(AppleVisualEffect::None))
#endif
    , usedTouchActions(RenderStyle::initialTouchActions())
    , strokeWidth(RenderStyle::initialStrokeWidth())
    , strokeColor(RenderStyle::initialStrokeColor())
    , hyphenateCharacter(RenderStyle::initialHyphenateCharacter())
    , hyphenateLimitBefore(RenderStyle::initialHyphenateLimitBefore())
    , hyphenateLimitAfter(RenderStyle::initialHyphenateLimitAfter())
    , hyphenateLimitLines(RenderStyle::initialHyphenateLimitLines())
#if ENABLE(DARK_MODE_CSS)
    , colorScheme(RenderStyle::initialColorScheme())
#endif
    , quotes(RenderStyle::initialQuotes())
    , appleColorFilter(StyleFilterData::create())
    , lineGrid(RenderStyle::initialLineGrid())
    , tabSize(RenderStyle::initialTabSize())
#if ENABLE(TEXT_AUTOSIZING)
    , textSizeAdjust(RenderStyle::initialTextSizeAdjust())
#endif
#if ENABLE(TOUCH_EVENTS)
    , tapHighlightColor(RenderStyle::initialTapHighlightColor())
#endif
    , listStyleType(RenderStyle::initialListStyleType())
    , blockEllipsis(RenderStyle::initialBlockEllipsis())
{
}

inline StyleRareInheritedData::StyleRareInheritedData(const StyleRareInheritedData& o)
    : RefCounted<StyleRareInheritedData>()
    , usedZoom(o.usedZoom)
    , listStyleImage(o.listStyleImage)
    , textStrokeWidth(o.textStrokeWidth)
    , textStrokeColor(o.textStrokeColor)
    , textFillColor(o.textFillColor)
    , textEmphasisColor(o.textEmphasisColor)
    , visitedLinkTextStrokeColor(o.visitedLinkTextStrokeColor)
    , visitedLinkTextFillColor(o.visitedLinkTextFillColor)
    , visitedLinkTextEmphasisColor(o.visitedLinkTextEmphasisColor)
    , caretColor(o.caretColor)
    , visitedLinkCaretColor(o.visitedLinkCaretColor)
    , accentColor(o.accentColor)
    , scrollbarColor(o.scrollbarColor)
    , dynamicRangeLimit(o.dynamicRangeLimit)
    , textShadow(o.textShadow)
    , cursorImages(o.cursorImages)
    , textEmphasisStyle(o.textEmphasisStyle)
    , textIndent(o.textIndent)
    , textUnderlineOffset(o.textUnderlineOffset)
    , textBoxEdge(o.textBoxEdge)
    , lineFitEdge(o.lineFitEdge)
    , miterLimit(o.miterLimit)
    , customProperties(o.customProperties)
    , widows(o.widows)
    , orphans(o.orphans)
    , textSecurity(o.textSecurity)
    , userModify(o.userModify)
    , wordBreak(o.wordBreak)
    , overflowWrap(o.overflowWrap)
    , nbspMode(o.nbspMode)
    , lineBreak(o.lineBreak)
    , userSelect(o.userSelect)
    , speakAs(o.speakAs)
    , hyphens(o.hyphens)
    , textCombine(o.textCombine)
    , textEmphasisPosition(o.textEmphasisPosition)
    , textUnderlinePosition(o.textUnderlinePosition)
    , lineBoxContain(o.lineBoxContain)
    , imageOrientation(o.imageOrientation)
    , imageRendering(o.imageRendering)
    , lineSnap(o.lineSnap)
    , lineAlign(o.lineAlign)
#if ENABLE(WEBKIT_OVERFLOW_SCROLLING_CSS_PROPERTY)
    , webkitOverflowScrolling(o.webkitOverflowScrolling)
#endif
    , textAlignLast(o.textAlignLast)
    , textJustify(o.textJustify)
    , textDecorationSkipInk(o.textDecorationSkipInk)
    , mathShift(o.mathShift)
    , mathStyle(o.mathStyle)
    , rubyPosition(o.rubyPosition)
    , rubyAlign(o.rubyAlign)
    , rubyOverhang(o.rubyOverhang)
    , textZoom(o.textZoom)
#if ENABLE(WEBKIT_TOUCH_CALLOUT_CSS_PROPERTY)
    , webkitTouchCallout(o.webkitTouchCallout)
#endif
    , hangingPunctuation(o.hangingPunctuation)
    , paintOrder(o.paintOrder)
    , capStyle(o.capStyle)
    , joinStyle(o.joinStyle)
    , hasSetStrokeWidth(o.hasSetStrokeWidth)
    , hasSetStrokeColor(o.hasSetStrokeColor)
    , hasAutoCaretColor(o.hasAutoCaretColor)
    , hasVisitedLinkAutoCaretColor(o.hasVisitedLinkAutoCaretColor)
    , hasAutoAccentColor(o.hasAutoAccentColor)
    , effectiveInert(o.effectiveInert)
    , isInSubtreeWithBlendMode(o.isInSubtreeWithBlendMode)
    , isForceHidden(o.isForceHidden)
    , usedContentVisibility(o.usedContentVisibility)
    , autoRevealsWhenFound(o.autoRevealsWhenFound)
    , insideDefaultButton(o.insideDefaultButton)
    , insideSubmitButton(o.insideSubmitButton)
#if HAVE(CORE_MATERIAL)
    , usedAppleVisualEffectForSubtree(o.usedAppleVisualEffectForSubtree)
#endif
    , usedTouchActions(o.usedTouchActions)
    , eventListenerRegionTypes(o.eventListenerRegionTypes)
    , strokeWidth(o.strokeWidth)
    , strokeColor(o.strokeColor)
    , visitedLinkStrokeColor(o.visitedLinkStrokeColor)
    , hyphenateCharacter(o.hyphenateCharacter)
    , hyphenateLimitBefore(o.hyphenateLimitBefore)
    , hyphenateLimitAfter(o.hyphenateLimitAfter)
    , hyphenateLimitLines(o.hyphenateLimitLines)
#if ENABLE(DARK_MODE_CSS)
    , colorScheme(o.colorScheme)
#endif
    , quotes(o.quotes)
    , appleColorFilter(o.appleColorFilter)
    , lineGrid(o.lineGrid)
    , tabSize(o.tabSize)
#if ENABLE(TEXT_AUTOSIZING)
    , textSizeAdjust(o.textSizeAdjust)
#endif
#if ENABLE(TOUCH_EVENTS)
    , tapHighlightColor(o.tapHighlightColor)
#endif
    , listStyleType(o.listStyleType)
    , blockEllipsis(o.blockEllipsis)
{
    ASSERT(o == *this, "StyleRareInheritedData should be properly copied.");
}

Ref<StyleRareInheritedData> StyleRareInheritedData::copy() const
{
    return adoptRef(*new StyleRareInheritedData(*this));
}

StyleRareInheritedData::~StyleRareInheritedData() = default;

bool StyleRareInheritedData::operator==(const StyleRareInheritedData& o) const
{
    return usedZoom == o.usedZoom
        && textStrokeWidth == o.textStrokeWidth
        && textStrokeColor == o.textStrokeColor
        && textFillColor == o.textFillColor
        && textEmphasisColor == o.textEmphasisColor
        && visitedLinkTextStrokeColor == o.visitedLinkTextStrokeColor
        && visitedLinkTextFillColor == o.visitedLinkTextFillColor
        && visitedLinkTextEmphasisColor == o.visitedLinkTextEmphasisColor
        && caretColor == o.caretColor
        && visitedLinkCaretColor == o.visitedLinkCaretColor
        && accentColor == o.accentColor
        && scrollbarColor == o.scrollbarColor
        && dynamicRangeLimit == o.dynamicRangeLimit
#if ENABLE(TOUCH_EVENTS)
        && tapHighlightColor == o.tapHighlightColor
#endif
        && textShadow == o.textShadow
        && cursorImages == o.cursorImages
        && textEmphasisStyle == o.textEmphasisStyle
        && textIndent == o.textIndent
        && textUnderlineOffset == o.textUnderlineOffset
        && textBoxEdge == o.textBoxEdge
        && lineFitEdge == o.lineFitEdge
        && wordSpacing == o.wordSpacing
        && miterLimit == o.miterLimit
        && widows == o.widows
        && orphans == o.orphans
        && textSecurity == o.textSecurity
        && userModify == o.userModify
        && wordBreak == o.wordBreak
        && overflowWrap == o.overflowWrap
        && nbspMode == o.nbspMode
        && lineBreak == o.lineBreak
#if ENABLE(WEBKIT_OVERFLOW_SCROLLING_CSS_PROPERTY)
        && webkitOverflowScrolling == o.webkitOverflowScrolling
#endif
#if ENABLE(TEXT_AUTOSIZING)
        && textSizeAdjust == o.textSizeAdjust
#endif
        && userSelect == o.userSelect
        && speakAs == o.speakAs
        && hyphens == o.hyphens
        && hyphenateLimitBefore == o.hyphenateLimitBefore
        && hyphenateLimitAfter == o.hyphenateLimitAfter
        && hyphenateLimitLines == o.hyphenateLimitLines
#if ENABLE(DARK_MODE_CSS)
        && colorScheme == o.colorScheme
#endif
        && textCombine == o.textCombine
        && textEmphasisPosition == o.textEmphasisPosition
        && lineBoxContain == o.lineBoxContain
#if ENABLE(WEBKIT_TOUCH_CALLOUT_CSS_PROPERTY)
        && webkitTouchCallout == o.webkitTouchCallout
#endif
        && hyphenateCharacter == o.hyphenateCharacter
        && quotes == o.quotes
        && appleColorFilter == o.appleColorFilter
        && tabSize == o.tabSize
        && lineGrid == o.lineGrid
        && imageOrientation == o.imageOrientation
        && imageRendering == o.imageRendering
        && textAlignLast == o.textAlignLast
        && textJustify == o.textJustify
        && textDecorationSkipInk == o.textDecorationSkipInk
        && textUnderlinePosition == o.textUnderlinePosition
        && rubyPosition == o.rubyPosition
        && rubyAlign == o.rubyAlign
        && rubyOverhang == o.rubyOverhang
        && textZoom == o.textZoom
        && lineSnap == o.lineSnap
        && lineAlign == o.lineAlign
        && hangingPunctuation == o.hangingPunctuation
        && paintOrder == o.paintOrder
        && capStyle == o.capStyle
        && joinStyle == o.joinStyle
        && hasSetStrokeWidth == o.hasSetStrokeWidth
        && hasSetStrokeColor == o.hasSetStrokeColor
        && mathShift == o.mathShift
        && mathStyle == o.mathStyle
        && hasAutoCaretColor == o.hasAutoCaretColor
        && hasVisitedLinkAutoCaretColor == o.hasVisitedLinkAutoCaretColor
        && hasAutoAccentColor == o.hasAutoAccentColor
        && isInSubtreeWithBlendMode == o.isInSubtreeWithBlendMode
        && isForceHidden == o.isForceHidden
        && autoRevealsWhenFound == o.autoRevealsWhenFound
        && usedTouchActions == o.usedTouchActions
        && eventListenerRegionTypes == o.eventListenerRegionTypes
        && effectiveInert == o.effectiveInert
        && usedContentVisibility == o.usedContentVisibility
        && insideDefaultButton == o.insideDefaultButton
        && insideSubmitButton == o.insideSubmitButton
#if HAVE(CORE_MATERIAL)
        && usedAppleVisualEffectForSubtree == o.usedAppleVisualEffectForSubtree
#endif
        && strokeWidth == o.strokeWidth
        && strokeColor == o.strokeColor
        && visitedLinkStrokeColor == o.visitedLinkStrokeColor
        && customProperties == o.customProperties
        && arePointingToEqualData(listStyleImage, o.listStyleImage)
        && listStyleType == o.listStyleType
        && blockEllipsis == o.blockEllipsis;
}

bool StyleRareInheritedData::hasColorFilters() const
{
    return !appleColorFilter->operations.isEmpty();
}

#if !LOG_DISABLED
void StyleRareInheritedData::dumpDifferences(TextStream& ts, const StyleRareInheritedData& other) const
{
    customProperties->dumpDifferences(ts, other.customProperties);

    LOG_IF_DIFFERENT(usedZoom);

    LOG_IF_DIFFERENT(listStyleImage);

    LOG_IF_DIFFERENT(textStrokeWidth);

    LOG_IF_DIFFERENT(textStrokeColor);
    LOG_IF_DIFFERENT(textFillColor);
    LOG_IF_DIFFERENT(textEmphasisColor);

    LOG_IF_DIFFERENT(visitedLinkTextStrokeColor);
    LOG_IF_DIFFERENT(visitedLinkTextFillColor);
    LOG_IF_DIFFERENT(visitedLinkTextEmphasisColor);

    LOG_IF_DIFFERENT(caretColor);
    LOG_IF_DIFFERENT(visitedLinkCaretColor);

    LOG_IF_DIFFERENT(accentColor);

    LOG_IF_DIFFERENT(scrollbarColor);

    LOG_IF_DIFFERENT(dynamicRangeLimit);

    LOG_IF_DIFFERENT(textShadow);

    LOG_IF_DIFFERENT(cursorImages);

    LOG_IF_DIFFERENT(textEmphasisStyle);
    LOG_IF_DIFFERENT(textIndent);
    LOG_IF_DIFFERENT(textUnderlineOffset);

    LOG_IF_DIFFERENT(textBoxEdge);
    LOG_IF_DIFFERENT(lineFitEdge);

    LOG_IF_DIFFERENT(wordSpacing);
    LOG_IF_DIFFERENT(miterLimit);

    LOG_IF_DIFFERENT(widows);
    LOG_IF_DIFFERENT(orphans);

    LOG_IF_DIFFERENT_WITH_CAST(TextSecurity, textSecurity);
    LOG_IF_DIFFERENT_WITH_CAST(UserModify, userModify);

    LOG_IF_DIFFERENT_WITH_CAST(WordBreak, wordBreak);
    LOG_IF_DIFFERENT_WITH_CAST(OverflowWrap, overflowWrap);
    LOG_IF_DIFFERENT_WITH_CAST(NBSPMode, nbspMode);
    LOG_IF_DIFFERENT_WITH_CAST(LineBreak, lineBreak);
    LOG_IF_DIFFERENT_WITH_CAST(UserSelect, userSelect);
    LOG_IF_DIFFERENT_WITH_CAST(ColorSpace, colorSpace);

    LOG_RAW_OPTIONSET_IF_DIFFERENT(SpeakAs, speakAs);

    LOG_IF_DIFFERENT_WITH_CAST(Hyphens, hyphens);
    LOG_IF_DIFFERENT_WITH_CAST(TextCombine, textCombine);
    LOG_IF_DIFFERENT_WITH_CAST(TextEmphasisPosition, textEmphasisPosition);
    LOG_IF_DIFFERENT_WITH_CAST(TextUnderlinePosition, textUnderlinePosition);

    LOG_RAW_OPTIONSET_IF_DIFFERENT(Style::LineBoxContain, lineBoxContain);

    LOG_IF_DIFFERENT_WITH_CAST(ImageOrientation, imageOrientation);
    LOG_IF_DIFFERENT_WITH_CAST(ImageRendering, imageRendering);
    LOG_IF_DIFFERENT_WITH_CAST(LineSnap, lineSnap);
    LOG_IF_DIFFERENT_WITH_CAST(LineAlign, lineAlign);

#if ENABLE(WEBKIT_OVERFLOW_SCROLLING_CSS_PROPERTY)
    LOG_IF_DIFFERENT_WITH_CAST(Style::WebkitOverflowScrolling, webkitOverflowScrolling);
#endif

    LOG_IF_DIFFERENT_WITH_CAST(TextAlignLast, textAlignLast);
    LOG_IF_DIFFERENT_WITH_CAST(TextJustify, textJustify);
    LOG_IF_DIFFERENT_WITH_CAST(TextDecorationSkipInk, textDecorationSkipInk);

    LOG_IF_DIFFERENT_WITH_CAST(RubyPosition, rubyPosition);
    LOG_IF_DIFFERENT_WITH_CAST(RubyAlign, rubyAlign);
    LOG_IF_DIFFERENT_WITH_CAST(RubyOverhang, rubyOverhang);

    LOG_IF_DIFFERENT_WITH_CAST(TextZoom, textZoom);

#if ENABLE(WEBKIT_TOUCH_CALLOUT_CSS_PROPERTY)
    LOG_IF_DIFFERENT_WITH_CAST(Style::WebkitTouchCallout, webkitTouchCallout);
#endif

    LOG_RAW_OPTIONSET_IF_DIFFERENT(HangingPunctuation, hangingPunctuation);

    LOG_IF_DIFFERENT_WITH_CAST(PaintOrder, paintOrder);
    LOG_IF_DIFFERENT_WITH_CAST(LineCap, capStyle);
    LOG_IF_DIFFERENT_WITH_CAST(LineJoin, joinStyle);

    LOG_IF_DIFFERENT_WITH_CAST(bool, hasSetStrokeWidth);
    LOG_IF_DIFFERENT_WITH_CAST(bool, hasSetStrokeColor);

    LOG_IF_DIFFERENT_WITH_CAST(MathShift, mathShift);
    LOG_IF_DIFFERENT_WITH_CAST(MathStyle, mathStyle);

    LOG_IF_DIFFERENT_WITH_CAST(bool, hasAutoCaretColor);
    LOG_IF_DIFFERENT_WITH_CAST(bool, hasVisitedLinkAutoCaretColor);
    LOG_IF_DIFFERENT_WITH_CAST(bool, hasAutoAccentColor);
    LOG_IF_DIFFERENT_WITH_CAST(bool, effectiveInert);
    LOG_IF_DIFFERENT_WITH_CAST(bool, isInSubtreeWithBlendMode);
    LOG_IF_DIFFERENT_WITH_CAST(bool, isForceHidden);
    LOG_IF_DIFFERENT_WITH_CAST(bool, autoRevealsWhenFound);

    LOG_IF_DIFFERENT_WITH_CAST(ContentVisibility, usedContentVisibility);

    LOG_IF_DIFFERENT_WITH_CAST(bool, insideDefaultButton);
    LOG_IF_DIFFERENT_WITH_CAST(bool, insideSubmitButton);

#if HAVE(CORE_MATERIAL)
    LOG_IF_DIFFERENT_WITH_CAST(AppleVisualEffect, usedAppleVisualEffectForSubtree);
#endif

    LOG_IF_DIFFERENT(usedTouchActions);
    LOG_IF_DIFFERENT(eventListenerRegionTypes);

    LOG_IF_DIFFERENT(strokeWidth);
    LOG_IF_DIFFERENT(strokeColor);
    LOG_IF_DIFFERENT(visitedLinkStrokeColor);

    LOG_IF_DIFFERENT(hyphenateCharacter);
    LOG_IF_DIFFERENT(hyphenateLimitBefore);
    LOG_IF_DIFFERENT(hyphenateLimitAfter);
    LOG_IF_DIFFERENT(hyphenateLimitLines);

#if ENABLE(DARK_MODE_CSS)
    LOG_IF_DIFFERENT(colorScheme);
#endif

    LOG_IF_DIFFERENT(quotes);

    appleColorFilter->dumpDifferences(ts, other.appleColorFilter);

    LOG_IF_DIFFERENT(lineGrid);
    LOG_IF_DIFFERENT(tabSize);

#if ENABLE(TEXT_AUTOSIZING)
    LOG_IF_DIFFERENT(textSizeAdjust);
#endif
#if ENABLE(TOUCH_EVENTS)
    LOG_IF_DIFFERENT(tapHighlightColor);
#endif

    LOG_IF_DIFFERENT(listStyleType);
    LOG_IF_DIFFERENT(blockEllipsis);
}
#endif

} // namespace WebCore
