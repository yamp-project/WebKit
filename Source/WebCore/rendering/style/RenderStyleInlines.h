/**
 * Copyright (C) 2000 Lars Knoll (knoll@kde.org)
 * Copyright (C) 2000 Antti Koivisto (koivisto@kde.org)
 * Copyright (C) 2000 Dirk Mueller (mueller@kde.org)
 * Copyright (C) 2003-2023 Apple Inc. All rights reserved.
 * Copyright (C) 2006 Graham Dennis (graham.dennis@gmail.com)
 * Copyright (C) 2014-2021 Google Inc. All rights reserved.
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
 */

#pragma once

#include <WebCore/AnchorPositionEvaluator.h>
#include <WebCore/AnimationList.h>
#include <WebCore/Element.h>
#include <WebCore/FontCascadeDescription.h>
#include <WebCore/GraphicsTypes.h>
#include <WebCore/HitTestRequest.h>
#include <WebCore/ImageOrientation.h>
#include <WebCore/PositionArea.h>
#include <WebCore/PositionTryOrder.h>
#include <WebCore/RenderStyle.h>
#include <WebCore/ScrollTypes.h>
#include <WebCore/StyleAppearance.h>
#include <WebCore/StyleBackgroundData.h>
#include <WebCore/StyleBoxData.h>
#include <WebCore/StyleDeprecatedFlexibleBoxData.h>
#include <WebCore/StyleFilterData.h>
#include <WebCore/StyleFlexibleBoxData.h>
#include <WebCore/StyleFontData.h>
#include <WebCore/StyleGridData.h>
#include <WebCore/StyleGridItemData.h>
#include <WebCore/StyleGridTrackSizingDirection.h>
#include <WebCore/StyleInheritedData.h>
#include <WebCore/StyleLineBoxContain.h>
#include <WebCore/StyleMarqueeData.h>
#include <WebCore/StyleMiscNonInheritedData.h>
#include <WebCore/StyleMultiColData.h>
#include <WebCore/StyleNonInheritedData.h>
#include <WebCore/StyleRareInheritedData.h>
#include <WebCore/StyleRareNonInheritedData.h>
#include <WebCore/StyleSurroundData.h>
#include <WebCore/StyleTransformData.h>
#include <WebCore/StyleVisitedLinkColorData.h>
#include <WebCore/UnicodeBidi.h>
#include <WebCore/ViewTimeline.h>
#include <WebCore/WebAnimationTypes.h>
#include <WebCore/WillChangeData.h>

#if ENABLE(APPLE_PAY)
#include <WebCore/ApplePayButtonPart.h>
#endif

#if HAVE(CORE_MATERIAL)
#include <WebCore/AppleVisualEffect.h>
#endif

namespace WebCore {

using namespace CSS::Literals;

inline const Style::Color& RenderStyle::accentColor() const { return m_rareInheritedData->accentColor; }
inline bool RenderStyle::affectsTransform() const { return hasTransform() || hasOffsetPath() || hasRotate() || hasScale() || hasTranslate(); }
inline const StyleContentAlignmentData& RenderStyle::alignContent() const { return m_nonInheritedData->miscData->alignContent; }
inline const StyleSelfAlignmentData& RenderStyle::alignItems() const { return m_nonInheritedData->miscData->alignItems; }
inline const StyleSelfAlignmentData& RenderStyle::alignSelf() const { return m_nonInheritedData->miscData->alignSelf; }
constexpr auto RenderStyle::allTransformOperations() -> OptionSet<TransformOperationOption> { return { TransformOperationOption::TransformOrigin, TransformOperationOption::Translate, TransformOperationOption::Rotate, TransformOperationOption::Scale, TransformOperationOption::Offset }; }
inline const AnimationList* RenderStyle::animations() const { return m_nonInheritedData->miscData->animations.get(); }
inline AnimationList* RenderStyle::animations() { return m_nonInheritedData->miscData->animations.get(); }
inline const Style::AnchorNames& RenderStyle::anchorNames() const { return m_nonInheritedData->rareData->anchorNames; }
inline const NameScope& RenderStyle::anchorScope() const { return m_nonInheritedData->rareData->anchorScope; }
inline StyleAppearance RenderStyle::appearance() const { return static_cast<StyleAppearance>(m_nonInheritedData->miscData->appearance); }
inline const FilterOperations& RenderStyle::appleColorFilter() const { return m_rareInheritedData->appleColorFilter->operations; }
#if HAVE(CORE_MATERIAL)
inline AppleVisualEffect RenderStyle::appleVisualEffect() const { return static_cast<AppleVisualEffect>(m_nonInheritedData->rareData->appleVisualEffect); }
#endif
inline const Style::AspectRatio& RenderStyle::aspectRatio() const { return m_nonInheritedData->miscData->aspectRatio; }
inline Style::Number<CSS::Nonnegative> RenderStyle::aspectRatioHeight() const { return aspectRatio().height(); }
inline Style::Number<CSS::Nonnegative> RenderStyle::aspectRatioLogicalHeight() const { return writingMode().isHorizontal() ? aspectRatioHeight() : aspectRatioWidth(); }
inline Style::Number<CSS::Nonnegative> RenderStyle::aspectRatioLogicalWidth() const { return writingMode().isHorizontal() ? aspectRatioWidth() : aspectRatioHeight(); }
inline Style::Number<CSS::Nonnegative> RenderStyle::aspectRatioWidth() const { return aspectRatio().width(); }
inline bool RenderStyle::autoWrap() const { return textWrapMode() != TextWrapMode::NoWrap; }
inline BackfaceVisibility RenderStyle::backfaceVisibility() const { return static_cast<BackfaceVisibility>(m_nonInheritedData->rareData->backfaceVisibility); }
inline FillAttachment RenderStyle::backgroundAttachment() const { return backgroundLayers().attachment(); }
inline BlendMode RenderStyle::backgroundBlendMode() const { return backgroundLayers().blendMode(); }
inline FillBox RenderStyle::backgroundClip() const { return backgroundLayers().clip(); }
inline const Style::Color& RenderStyle::backgroundColor() const { return m_nonInheritedData->backgroundData->color; }
inline const FillLayer& RenderStyle::backgroundLayers() const { return m_nonInheritedData->backgroundData->background; }
inline Ref<const FillLayer> RenderStyle::protectedBackgroundLayers() const { return backgroundLayers(); }
inline FillBox RenderStyle::backgroundOrigin() const { return backgroundLayers().origin(); }
inline FillRepeatXY RenderStyle::backgroundRepeat() const { return backgroundLayers().repeat(); }
inline const LengthSize& RenderStyle::backgroundSizeLength() const { return backgroundLayers().sizeLength(); }
inline FillSizeType RenderStyle::backgroundSizeType() const { return backgroundLayers().sizeType(); }
inline const Style::BlockEllipsis& RenderStyle::blockEllipsis() const { return m_rareInheritedData->blockEllipsis; }
inline BlockStepAlign RenderStyle::blockStepAlign() const { return static_cast<BlockStepAlign>(m_nonInheritedData->rareData->blockStepAlign); }
inline BlockStepInsert RenderStyle::blockStepInsert() const { return static_cast<BlockStepInsert>(m_nonInheritedData->rareData->blockStepInsert); }
inline BlockStepRound RenderStyle::blockStepRound() const { return static_cast<BlockStepRound>(m_nonInheritedData->rareData->blockStepRound); }
inline const Style::BlockStepSize& RenderStyle::blockStepSize() const { return m_nonInheritedData->rareData->blockStepSize; }
inline const BorderData& RenderStyle::border() const { return m_nonInheritedData->surroundData->border; }
inline Style::LineWidth RenderStyle::borderAfterWidth() const { return borderAfterWidth(writingMode()); }
inline Style::LineWidth RenderStyle::borderBeforeWidth() const { return borderBeforeWidth(writingMode()); }
inline const BorderValue& RenderStyle::borderBottom() const { return border().bottom(); }
inline const Style::Color& RenderStyle::borderBottomColor() const { return border().bottom().color(); }
inline bool RenderStyle::borderBottomIsTransparent() const { return border().bottom().isTransparent(); }
inline const Style::BorderRadiusValue& RenderStyle::borderBottomLeftRadius() const { return border().bottomLeftRadius(); }
inline const Style::BorderRadiusValue& RenderStyle::borderBottomRightRadius() const { return border().bottomRightRadius(); }
inline BorderStyle RenderStyle::borderBottomStyle() const { return border().bottom().style(); }
inline Style::LineWidth RenderStyle::borderBottomWidth() const { return border().borderBottomWidth(); }
inline Style::LineWidth RenderStyle::borderEndWidth() const { return borderEndWidth(writingMode()); }
inline const Style::BorderImage& RenderStyle::borderImage() const { return border().image(); }
inline NinePieceImageRule RenderStyle::borderImageHorizontalRule() const { return borderImageRepeat().horizontalRule(); }
inline const Style::BorderImageOutset& RenderStyle::borderImageOutset() const { return borderImage().outset(); }
inline LayoutBoxExtent RenderStyle::borderImageOutsets() const { return imageOutsets(borderImage()); }
inline const Style::BorderImageRepeat& RenderStyle::borderImageRepeat() const { return borderImage().repeat(); }
inline const Style::BorderImageSlice& RenderStyle::borderImageSlice() const { return borderImage().slice(); }
inline const Style::BorderImageSource& RenderStyle::borderImageSource() const { return borderImage().source(); }
inline NinePieceImageRule RenderStyle::borderImageVerticalRule() const { return borderImageRepeat().verticalRule(); }
inline const Style::BorderImageWidth& RenderStyle::borderImageWidth() const { return borderImage().width(); }
inline const BorderValue& RenderStyle::borderLeft() const { return border().left(); }
inline const Style::Color& RenderStyle::borderLeftColor() const { return border().left().color(); }
inline bool RenderStyle::borderLeftIsTransparent() const { return border().left().isTransparent(); }
inline BorderStyle RenderStyle::borderLeftStyle() const { return border().left().style(); }
inline Style::LineWidth RenderStyle::borderLeftWidth() const { return border().borderLeftWidth(); }
inline const Style::BorderRadius& RenderStyle::borderRadii() const { return border().radii(); }
inline const BorderValue& RenderStyle::borderRight() const { return border().right(); }
inline const Style::Color& RenderStyle::borderRightColor() const { return border().right().color(); }
inline bool RenderStyle::borderRightIsTransparent() const { return border().right().isTransparent(); }
inline BorderStyle RenderStyle::borderRightStyle() const { return border().right().style(); }
inline Style::LineWidth RenderStyle::borderRightWidth() const { return border().borderRightWidth(); }
inline Style::LineWidth RenderStyle::borderStartWidth() const { return borderStartWidth(writingMode()); }
inline const BorderValue& RenderStyle::borderTop() const { return border().top(); }
inline const Style::Color& RenderStyle::borderTopColor() const { return border().top().color(); }
inline bool RenderStyle::borderTopIsTransparent() const { return border().top().isTransparent(); }
inline const Style::BorderRadiusValue& RenderStyle::borderTopLeftRadius() const { return border().topLeftRadius(); }
inline const Style::BorderRadiusValue& RenderStyle::borderTopRightRadius() const { return border().topRightRadius(); }
inline BorderStyle RenderStyle::borderTopStyle() const { return border().top().style(); }
inline Style::LineWidth RenderStyle::borderTopWidth() const { return border().borderTopWidth(); }
inline Style::LineWidthBox RenderStyle::borderWidth() const { return border().borderWidth(); }
inline Style::WebkitBorderSpacing RenderStyle::borderHorizontalSpacing() const { return m_inheritedData->borderHorizontalSpacing; }
inline Style::WebkitBorderSpacing RenderStyle::borderVerticalSpacing() const { return m_inheritedData->borderVerticalSpacing; }
inline const Style::InsetEdge& RenderStyle::bottom() const { return m_nonInheritedData->surroundData->inset.bottom(); }
inline BoxAlignment RenderStyle::boxAlign() const { return static_cast<BoxAlignment>(m_nonInheritedData->miscData->deprecatedFlexibleBox->align); }
inline float RenderStyle::boxFlex() const { return m_nonInheritedData->miscData->deprecatedFlexibleBox->flex; }
inline unsigned RenderStyle::boxFlexGroup() const { return m_nonInheritedData->miscData->deprecatedFlexibleBox->flexGroup; }
inline BoxLines RenderStyle::boxLines() const { return static_cast<BoxLines>(m_nonInheritedData->miscData->deprecatedFlexibleBox->lines); }
inline unsigned RenderStyle::boxOrdinalGroup() const { return m_nonInheritedData->miscData->deprecatedFlexibleBox->ordinalGroup; }
inline BoxOrient RenderStyle::boxOrient() const { return static_cast<BoxOrient>(m_nonInheritedData->miscData->deprecatedFlexibleBox->orient); }
inline BoxPack RenderStyle::boxPack() const { return static_cast<BoxPack>(m_nonInheritedData->miscData->deprecatedFlexibleBox->pack); }
inline StyleReflection* RenderStyle::boxReflect() const { return m_nonInheritedData->rareData->boxReflect.get(); }
inline const Style::BoxShadows& RenderStyle::boxShadow() const { return m_nonInheritedData->miscData->boxShadow; }
inline bool RenderStyle::hasBoxShadow() const { return !boxShadow().isNone(); }
inline BoxSizing RenderStyle::boxSizing() const { return m_nonInheritedData->boxData->boxSizing(); }
inline BoxSizing RenderStyle::boxSizingForAspectRatio() const { return aspectRatio().isAutoAndRatio() ? BoxSizing::ContentBox : boxSizing(); }
inline BreakBetween RenderStyle::breakAfter() const { return static_cast<BreakBetween>(m_nonInheritedData->rareData->breakAfter); }
inline BreakBetween RenderStyle::breakBefore() const { return static_cast<BreakBetween>(m_nonInheritedData->rareData->breakBefore); }
inline BreakInside RenderStyle::breakInside() const { return static_cast<BreakInside>(m_nonInheritedData->rareData->breakInside); }
inline LineCap RenderStyle::capStyle() const { return static_cast<LineCap>(m_rareInheritedData->capStyle); }
inline const Style::Color& RenderStyle::caretColor() const { return m_rareInheritedData->caretColor; }
inline const Style::Clip& RenderStyle::clip() const { return m_nonInheritedData->rareData->clip; }
inline const Style::ClipPath& RenderStyle::clipPath() const { return m_nonInheritedData->rareData->clipPath; }
inline bool RenderStyle::collapseWhiteSpace() const { return collapseWhiteSpace(whiteSpaceCollapse()); }
inline ColumnAxis RenderStyle::columnAxis() const { return static_cast<ColumnAxis>(m_nonInheritedData->miscData->multiCol->axis); }
inline Style::ColumnCount RenderStyle::columnCount() const { return m_nonInheritedData->miscData->multiCol->count; }
inline ColumnFill RenderStyle::columnFill() const { return static_cast<ColumnFill>(m_nonInheritedData->miscData->multiCol->fill); }
inline const Style::GapGutter& RenderStyle::columnGap() const { return m_nonInheritedData->rareData->columnGap; }
inline ColumnProgression RenderStyle::columnProgression() const { return static_cast<ColumnProgression>(m_nonInheritedData->miscData->multiCol->progression); }
inline const Style::Color& RenderStyle::columnRuleColor() const { return m_nonInheritedData->miscData->multiCol->rule.color(); }
inline bool RenderStyle::columnRuleIsTransparent() const { return m_nonInheritedData->miscData->multiCol->rule.isTransparent(); }
inline BorderStyle RenderStyle::columnRuleStyle() const { return m_nonInheritedData->miscData->multiCol->rule.style(); }
inline Style::LineWidth RenderStyle::columnRuleWidth() const { return m_nonInheritedData->miscData->multiCol->ruleWidth(); }
inline ColumnSpan RenderStyle::columnSpan() const { return static_cast<ColumnSpan>(m_nonInheritedData->miscData->multiCol->columnSpan); }
inline Style::ColumnWidth RenderStyle::columnWidth() const { return m_nonInheritedData->miscData->multiCol->width; }
inline const AtomString& RenderStyle::computedLocale() const { return fontDescription().computedLocale(); }
inline OptionSet<Containment> RenderStyle::contain() const { return m_nonInheritedData->rareData->contain; }
inline const Style::ContainIntrinsicSize& RenderStyle::containIntrinsicLogicalHeight() const { return writingMode().isHorizontal() ? containIntrinsicHeight() : containIntrinsicWidth(); }
inline const Style::ContainIntrinsicSize& RenderStyle::containIntrinsicLogicalWidth() const { return writingMode().isHorizontal() ? containIntrinsicWidth() : containIntrinsicHeight(); }
inline const Style::ContainIntrinsicSize& RenderStyle::containIntrinsicHeight() const { return m_nonInheritedData->rareData->containIntrinsicHeight; }
inline const Style::ContainIntrinsicSize& RenderStyle::containIntrinsicWidth() const { return m_nonInheritedData->rareData->containIntrinsicWidth; }
inline const Style::ContainerNames& RenderStyle::containerNames() const { return m_nonInheritedData->rareData->containerNames; }
inline ContainerType RenderStyle::containerType() const { return static_cast<ContainerType>(m_nonInheritedData->rareData->containerType); }
inline bool RenderStyle::containsInlineSize() const { return usedContain().contains(Containment::InlineSize); }
inline bool RenderStyle::containsLayout() const { return usedContain().contains(Containment::Layout); }
inline bool RenderStyle::containsLayoutOrPaint() const { return usedContain().containsAny({ Containment::Layout, Containment::Paint }); }
inline bool RenderStyle::containsPaint() const { return usedContain().contains(Containment::Paint); }
inline bool RenderStyle::containsSize() const { return usedContain().contains(Containment::Size); }
inline bool RenderStyle::containsSizeOrInlineSize() const { return usedContain().containsAny({ Containment::Size, Containment::InlineSize }); }
inline bool RenderStyle::containsStyle() const { return usedContain().contains(Containment::Style); }
constexpr OptionSet<Containment> RenderStyle::contentContainment() { return { Containment::Layout, Containment::Paint, Containment::Style }; }
inline const Style::Content& RenderStyle::content() const { return m_nonInheritedData->miscData->content; }
inline ContentVisibility RenderStyle::contentVisibility() const { return static_cast<ContentVisibility>(m_nonInheritedData->rareData->contentVisibility); }
inline Style::Cursor RenderStyle::cursor() const { return { m_rareInheritedData->cursorImages, cursorType() }; }
inline StyleAppearance RenderStyle::usedAppearance() const { return static_cast<StyleAppearance>(m_nonInheritedData->miscData->usedAppearance); }
#if HAVE(CORE_MATERIAL)
inline AppleVisualEffect RenderStyle::usedAppleVisualEffectForSubtree() const { return static_cast<AppleVisualEffect>(m_rareInheritedData->usedAppleVisualEffectForSubtree); }
#endif
inline OptionSet<Containment> RenderStyle::usedContain() const { return m_nonInheritedData->rareData->usedContain(); }
inline bool RenderStyle::effectiveInert() const { return m_rareInheritedData->effectiveInert; }
inline PointerEvents RenderStyle::usedPointerEvents() const { return effectiveInert() ? PointerEvents::None : pointerEvents(); }
inline CSSPropertyID RenderStyle::usedStrokeColorProperty() const { return hasExplicitlySetStrokeColor() ? CSSPropertyStrokeColor : CSSPropertyWebkitTextStrokeColor; }
inline OptionSet<TouchAction> RenderStyle::usedTouchActions() const { return m_rareInheritedData->usedTouchActions; }
inline UserModify RenderStyle::usedUserModify() const { return effectiveInert() ? UserModify::ReadOnly : userModify(); }
inline float RenderStyle::usedZoom() const { return m_rareInheritedData->usedZoom; }
inline OptionSet<EventListenerRegionType> RenderStyle::eventListenerRegionTypes() const { return m_rareInheritedData->eventListenerRegionTypes; }
inline FieldSizing RenderStyle::fieldSizing() const { return static_cast<FieldSizing>(m_nonInheritedData->rareData->fieldSizing); }
inline const FilterOperations& RenderStyle::filter() const { return m_nonInheritedData->miscData->filter->operations; }
inline IntOutsets RenderStyle::filterOutsets() const { return hasFilter() ? filter().outsets() : IntOutsets(); }
inline const Style::FlexBasis& RenderStyle::flexBasis() const { return m_nonInheritedData->miscData->flexibleBox->flexBasis; }
inline FlexDirection RenderStyle::flexDirection() const { return static_cast<FlexDirection>(m_nonInheritedData->miscData->flexibleBox->flexDirection); }
inline Style::FlexGrow RenderStyle::flexGrow() const { return m_nonInheritedData->miscData->flexibleBox->flexGrow; }
inline Style::FlexShrink RenderStyle::flexShrink() const { return m_nonInheritedData->miscData->flexibleBox->flexShrink; }
inline FlexWrap RenderStyle::flexWrap() const { return static_cast<FlexWrap>(m_nonInheritedData->miscData->flexibleBox->flexWrap); }
inline std::optional<FontSelectionValue> RenderStyle::fontItalic() const { return fontDescription().italic(); }
inline const FontPalette& RenderStyle::fontPalette() const { return fontDescription().fontPalette(); }
inline FontSizeAdjust RenderStyle::fontSizeAdjust() const { return fontDescription().fontSizeAdjust(); }
inline FontSelectionValue RenderStyle::fontWidth() const { return fontDescription().width(); }
inline FontOpticalSizing RenderStyle::fontOpticalSizing() const { return fontDescription().opticalSizing(); }
inline FontVariationSettings RenderStyle::fontVariationSettings() const { return fontDescription().variationSettings(); }
inline FontSelectionValue RenderStyle::fontWeight() const { return fontDescription().weight(); }
inline const Style::GapGutter& RenderStyle::gap(Style::GridTrackSizingDirection direction) const { return direction == Style::GridTrackSizingDirection::Columns ? columnGap() : rowGap(); }
inline const Style::GridTrackSizes& RenderStyle::gridAutoColumns() const { return m_nonInheritedData->rareData->grid->m_gridAutoColumns; }
inline GridAutoFlow RenderStyle::gridAutoFlow() const { return static_cast<GridAutoFlow>(m_nonInheritedData->rareData->grid->m_gridAutoFlow); }
inline const Style::GridTrackSizes& RenderStyle::gridAutoRows() const { return m_nonInheritedData->rareData->grid->m_gridAutoRows; }
inline const Style::GridTrackSizes& RenderStyle::gridAutoList(Style::GridTrackSizingDirection direction) const { return direction == Style::GridTrackSizingDirection::Columns ? gridAutoColumns() : gridAutoRows(); }
inline const Style::GridTemplateList& RenderStyle::gridTemplateColumns() const { return m_nonInheritedData->rareData->grid->m_gridTemplateColumns; }
inline const Style::GridPosition& RenderStyle::gridItemColumnEnd() const { return m_nonInheritedData->rareData->gridItem->gridColumnEnd; }
inline const Style::GridPosition& RenderStyle::gridItemColumnStart() const { return m_nonInheritedData->rareData->gridItem->gridColumnStart; }
inline const Style::GridPosition& RenderStyle::gridItemEnd(Style::GridTrackSizingDirection direction) const { return direction == Style::GridTrackSizingDirection::Columns ? gridItemColumnEnd() : gridItemRowEnd(); }
inline const Style::GridPosition& RenderStyle::gridItemRowEnd() const { return m_nonInheritedData->rareData->gridItem->gridRowEnd; }
inline const Style::GridPosition& RenderStyle::gridItemRowStart() const { return m_nonInheritedData->rareData->gridItem->gridRowStart; }
inline const Style::GridPosition& RenderStyle::gridItemStart(Style::GridTrackSizingDirection direction) const { return direction == Style::GridTrackSizingDirection::Columns ? gridItemColumnStart() : gridItemRowStart(); }
inline const Style::GridTemplateList& RenderStyle::gridTemplateRows() const { return m_nonInheritedData->rareData->grid->m_gridTemplateRows; }
inline const Style::GridTemplateList& RenderStyle::gridTemplateList(Style::GridTrackSizingDirection direction) const { return direction == Style::GridTrackSizingDirection::Columns ? gridTemplateColumns() : gridTemplateRows(); }
inline const Style::GridTemplateAreas& RenderStyle::gridTemplateAreas() const { return m_nonInheritedData->rareData->grid->m_gridTemplateAreas; }
inline OptionSet<HangingPunctuation> RenderStyle::hangingPunctuation() const { return OptionSet<HangingPunctuation>::fromRaw(m_rareInheritedData->hangingPunctuation); }
inline bool RenderStyle::hasAnimations() const { return animations() && animations()->size(); }
inline bool RenderStyle::hasAnimationsOrTransitions() const { return hasAnimations() || hasTransitions(); }
inline bool RenderStyle::hasAnyFixedBackground() const { return backgroundLayers().hasImageWithAttachment(FillAttachment::FixedBackground); }
inline bool RenderStyle::hasAnyLocalBackground() const { return backgroundLayers().hasImageWithAttachment(FillAttachment::LocalBackground); }
inline bool RenderStyle::hasAnyPublicPseudoStyles() const { return m_nonInheritedFlags.hasAnyPublicPseudoStyles(); }
// FIXME: Rename this function.
inline bool RenderStyle::hasAppearance() const { return appearance() != StyleAppearance::None && appearance() != StyleAppearance::Base; }
inline bool RenderStyle::hasAppleColorFilter() const { return !appleColorFilter().isEmpty(); }
#if HAVE(CORE_MATERIAL)
inline bool RenderStyle::hasAppleVisualEffect() const { return appleVisualEffect() != AppleVisualEffect::None; }
inline bool RenderStyle::hasAppleVisualEffectRequiringBackdropFilter() const { return appleVisualEffectNeedsBackdrop(appleVisualEffect()); }
#endif
inline bool RenderStyle::hasAspectRatio() const { return aspectRatio().hasRatio(); }
inline bool RenderStyle::hasAttrContent() const { return m_nonInheritedData->miscData->hasAttrContent; }
inline bool RenderStyle::hasAutoAccentColor() const { return m_rareInheritedData->hasAutoAccentColor; }
inline bool RenderStyle::hasAutoCaretColor() const { return m_rareInheritedData->hasAutoCaretColor; }
inline bool RenderStyle::hasAutoLeftAndRight() const { return left().isAuto() && right().isAuto(); }
inline bool RenderStyle::hasAutoLengthContainIntrinsicSize() const { return containIntrinsicWidth().hasAuto() || containIntrinsicHeight().hasAuto(); }
inline bool RenderStyle::hasAutoTopAndBottom() const { return top().isAuto() && bottom().isAuto(); }
inline bool RenderStyle::hasBackground() const { return visitedDependentColor(CSSPropertyBackgroundColor).isVisible() || hasBackgroundImage(); }
inline bool RenderStyle::hasBackgroundImage() const { return backgroundLayers().hasImage(); }
inline bool RenderStyle::hasBlendMode() const { return blendMode() != BlendMode::Normal; }
inline bool RenderStyle::hasBorder() const { return border().hasBorder(); }
inline bool RenderStyle::hasBorderImage() const { return border().hasBorderImage(); }
inline bool RenderStyle::hasBorderImageOutsets() const { return borderImage().hasSource() && !borderImage().outset().isZero(); }
inline bool RenderStyle::hasBorderRadius() const { return border().hasBorderRadius(); }
inline bool RenderStyle::hasClip() const { return !clip().isAuto(); }
inline bool RenderStyle::hasClipPath() const { return !clipPath().isNone(); }
inline bool RenderStyle::hasContent() const { return content().isData(); }
inline bool RenderStyle::hasDisplayAffectedByAnimations() const { return m_nonInheritedData->miscData->hasDisplayAffectedByAnimations; }
// FIXME: Rename this function.
inline bool RenderStyle::hasUsedAppearance() const { return usedAppearance() != StyleAppearance::None && usedAppearance() != StyleAppearance::Base; }
inline bool RenderStyle::hasUsedContentNone() const { return content().isNone() || (content().isNormal() && (pseudoElementType() == PseudoId::Before || pseudoElementType() == PseudoId::After)); }
inline bool RenderStyle::hasExplicitlySetBorderBottomLeftRadius() const { return m_nonInheritedData->surroundData->hasExplicitlySetBorderBottomLeftRadius; }
inline bool RenderStyle::hasExplicitlySetBorderBottomRightRadius() const { return m_nonInheritedData->surroundData->hasExplicitlySetBorderBottomRightRadius; }
inline bool RenderStyle::hasExplicitlySetBorderRadius() const { return hasExplicitlySetBorderBottomLeftRadius() || hasExplicitlySetBorderBottomRightRadius() || hasExplicitlySetBorderTopLeftRadius() || hasExplicitlySetBorderTopRightRadius(); }
inline bool RenderStyle::hasExplicitlySetBorderTopLeftRadius() const { return m_nonInheritedData->surroundData->hasExplicitlySetBorderTopLeftRadius; }
inline bool RenderStyle::hasExplicitlySetBorderTopRightRadius() const { return m_nonInheritedData->surroundData->hasExplicitlySetBorderTopRightRadius; }
inline bool RenderStyle::hasExplicitlySetPadding() const { return hasExplicitlySetPaddingBottom() || hasExplicitlySetPaddingLeft() || hasExplicitlySetPaddingRight() || hasExplicitlySetPaddingTop(); }
inline bool RenderStyle::hasExplicitlySetPaddingBottom() const { return m_nonInheritedData->surroundData->hasExplicitlySetPaddingBottom; }
inline bool RenderStyle::hasExplicitlySetPaddingLeft() const { return m_nonInheritedData->surroundData->hasExplicitlySetPaddingLeft; }
inline bool RenderStyle::hasExplicitlySetPaddingRight() const { return m_nonInheritedData->surroundData->hasExplicitlySetPaddingRight; }
inline bool RenderStyle::hasExplicitlySetPaddingTop() const { return m_nonInheritedData->surroundData->hasExplicitlySetPaddingTop; }
inline bool RenderStyle::hasExplicitlySetStrokeColor() const { return m_rareInheritedData->hasSetStrokeColor; }
inline bool RenderStyle::hasFilter() const { return !filter().isEmpty(); }
inline bool RenderStyle::hasInFlowPosition() const { return position() == PositionType::Relative || position() == PositionType::Sticky; }
inline bool RenderStyle::hasIsolation() const { return isolation() != Isolation::Auto; }
inline bool RenderStyle::hasMargin() const { return !Style::isZero(marginBox()); }
inline bool RenderStyle::hasMask() const { return maskLayers().hasImage() || maskBorder().hasSource(); }
inline bool RenderStyle::hasInset() const { return !Style::isZero(insetBox()); }
inline bool RenderStyle::hasOffsetPath() const { return !WTF::holdsAlternative<CSS::Keyword::None>(m_nonInheritedData->rareData->offsetPath); }
inline bool RenderStyle::hasOpacity() const { return !opacity().isOpaque(); }
inline bool RenderStyle::hasOutOfFlowPosition() const { return position() == PositionType::Absolute || position() == PositionType::Fixed; }
inline bool RenderStyle::hasOutline() const { return outlineStyle() != OutlineStyle::None && outlineWidth().isPositive(); }
inline bool RenderStyle::hasOutlineInVisualOverflow() const { return hasOutline() && outlineSize() > 0; }
inline bool RenderStyle::hasPadding() const { return !Style::isZero(paddingBox()); }
inline bool RenderStyle::hasPerspective() const { return !perspective().isNone(); }
inline bool RenderStyle::hasPositionedMask() const { return maskLayers().hasImage(); }
inline bool RenderStyle::hasPseudoStyle(PseudoId pseudo) const { return m_nonInheritedFlags.hasPseudoStyle(pseudo); }
inline bool RenderStyle::hasRotate() const { return !rotate().isNone(); }
inline bool RenderStyle::hasScale() const { return !scale().isNone(); }
inline bool RenderStyle::hasStaticBlockPosition(bool horizontal) const { return horizontal ? hasAutoTopAndBottom() : hasAutoLeftAndRight(); }
inline bool RenderStyle::hasStaticInlinePosition(bool horizontal) const { return horizontal ? hasAutoLeftAndRight() : hasAutoTopAndBottom(); }
inline bool RenderStyle::hasTextCombine() const { return textCombine() != TextCombine::None; }
inline bool RenderStyle::hasTransform() const { return !transform().isEmpty() || hasOffsetPath(); }
inline bool RenderStyle::hasTransformRelatedProperty() const { return hasTransform() || hasRotate() || hasScale() || hasTranslate() || transformStyle3D() == TransformStyle3D::Preserve3D || hasPerspective(); }
inline bool RenderStyle::hasTranslate() const { return !translate().isNone(); }
inline bool RenderStyle::hasTransitions() const { return transitions() && transitions()->size(); }
inline bool RenderStyle::hasViewportConstrainedPosition() const { return position() == PositionType::Fixed || position() == PositionType::Sticky; }
inline bool RenderStyle::hasVisibleBorder() const { return border().hasVisibleBorder(); }
inline bool RenderStyle::hasVisibleBorderDecoration() const { return hasVisibleBorder() || hasBorderImage(); }
inline bool RenderStyle::hasVisitedLinkAutoCaretColor() const { return m_rareInheritedData->hasVisitedLinkAutoCaretColor; }
inline const Style::PreferredSize& RenderStyle::height() const { return m_nonInheritedData->boxData->height(); }
inline Style::HyphenateLimitEdge RenderStyle::hyphenateLimitAfter() const { return m_rareInheritedData->hyphenateLimitAfter; }
inline Style::HyphenateLimitEdge RenderStyle::hyphenateLimitBefore() const { return m_rareInheritedData->hyphenateLimitBefore; }
inline Style::HyphenateLimitLines RenderStyle::hyphenateLimitLines() const { return m_rareInheritedData->hyphenateLimitLines; }
inline const Style::HyphenateCharacter& RenderStyle::hyphenateCharacter() const { return m_rareInheritedData->hyphenateCharacter; }
inline Hyphens RenderStyle::hyphens() const { return static_cast<Hyphens>(m_rareInheritedData->hyphens); }
inline ImageOrientation RenderStyle::imageOrientation() const { return static_cast<ImageOrientation::Orientation>(m_rareInheritedData->imageOrientation); }
inline ImageRendering RenderStyle::imageRendering() const { return static_cast<ImageRendering>(m_rareInheritedData->imageRendering); }
constexpr auto RenderStyle::individualTransformOperations() -> OptionSet<TransformOperationOption> { return { TransformOperationOption::Translate, TransformOperationOption::Rotate, TransformOperationOption::Scale, TransformOperationOption::Offset }; }
inline const Style::CustomPropertyData& RenderStyle::inheritedCustomProperties() const { return m_rareInheritedData->customProperties.get(); }
inline Style::AnchorNames RenderStyle::initialAnchorNames() { return CSS::Keyword::None { }; }
inline NameScope RenderStyle::initialAnchorScope() { return { }; }
constexpr StyleAppearance RenderStyle::initialAppearance() { return StyleAppearance::None; }
#if HAVE(CORE_MATERIAL)
constexpr AppleVisualEffect RenderStyle::initialAppleVisualEffect() { return AppleVisualEffect::None; }
#endif
inline FilterOperations RenderStyle::initialAppleColorFilter() { return { }; }
inline Style::AspectRatio RenderStyle::initialAspectRatio() { return CSS::Keyword::Auto { }; }
constexpr BackfaceVisibility RenderStyle::initialBackfaceVisibility() { return BackfaceVisibility::Visible; }
inline Style::Color RenderStyle::initialBackgroundColor() { return Color::transparentBlack; }
inline Style::BlockEllipsis RenderStyle::initialBlockEllipsis() { return CSS::Keyword::None { }; }
constexpr BlockStepAlign RenderStyle::initialBlockStepAlign() { return BlockStepAlign::Auto; }
constexpr BlockStepInsert RenderStyle::initialBlockStepInsert() { return BlockStepInsert::MarginBox; }
constexpr BlockStepRound RenderStyle::initialBlockStepRound() { return BlockStepRound::Up; }
inline Style::BlockStepSize RenderStyle::initialBlockStepSize() { return CSS::Keyword::None { }; }
constexpr BorderCollapse RenderStyle::initialBorderCollapse() { return BorderCollapse::Separate; }
constexpr Style::Length<CSS::Nonnegative> RenderStyle::initialBorderHorizontalSpacing() { return 0_css_px; }
inline Style::BorderImage RenderStyle::initialBorderImage() { return Style::BorderImage { }; }
inline Style::BorderImageSource RenderStyle::initialBorderImageSource() { return CSS::Keyword::None { }; }
inline Style::BorderRadiusValue RenderStyle::initialBorderRadius() { return { 0_css_px, 0_css_px }; }
constexpr BorderStyle RenderStyle::initialBorderStyle() { return BorderStyle::None; }
constexpr Style::Length<CSS::Nonnegative> RenderStyle::initialBorderVerticalSpacing() { return 0_css_px; }
constexpr Style::LineWidth RenderStyle::initialBorderWidth() { return CSS::Keyword::Medium { }; }
constexpr BoxAlignment RenderStyle::initialBoxAlign() { return BoxAlignment::Stretch; }
constexpr BoxDecorationBreak RenderStyle::initialBoxDecorationBreak() { return BoxDecorationBreak::Slice; }
constexpr BoxDirection RenderStyle::initialBoxDirection() { return BoxDirection::Normal; }
constexpr BoxLines RenderStyle::initialBoxLines() { return BoxLines::Single; }
constexpr BoxOrient RenderStyle::initialBoxOrient() { return BoxOrient::Horizontal; }
constexpr BoxPack RenderStyle::initialBoxPack() { return BoxPack::Start; }
inline Style::BoxShadows RenderStyle::initialBoxShadow() { return CSS::Keyword::None { }; }
constexpr BoxSizing RenderStyle::initialBoxSizing() { return BoxSizing::ContentBox; }
constexpr BreakBetween RenderStyle::initialBreakBetween() { return BreakBetween::Auto; }
constexpr BreakInside RenderStyle::initialBreakInside() { return BreakInside::Auto; }
constexpr LineCap RenderStyle::initialCapStyle() { return LineCap::Butt; }
constexpr CaptionSide RenderStyle::initialCaptionSide() { return CaptionSide::Top; }
constexpr Clear RenderStyle::initialClear() { return Clear::None; }
inline Style::Clip RenderStyle::initialClip() { return CSS::Keyword::Auto { }; }
inline Style::ClipPath RenderStyle::initialClipPath() { return CSS::Keyword::None { }; }
inline Color RenderStyle::initialColor() { return Color::black; }
constexpr ColumnAxis RenderStyle::initialColumnAxis() { return ColumnAxis::Auto; }
constexpr Style::ColumnCount RenderStyle::initialColumnCount() { return CSS::Keyword::Auto { }; }
constexpr ColumnFill RenderStyle::initialColumnFill() { return ColumnFill::Balance; }
inline Style::GapGutter RenderStyle::initialColumnGap() { return CSS::Keyword::Normal { }; }
constexpr ColumnProgression RenderStyle::initialColumnProgression() { return ColumnProgression::Normal; }
constexpr Style::LineWidth RenderStyle::initialColumnRuleWidth() { return CSS::Keyword::Medium { }; }
constexpr ColumnSpan RenderStyle::initialColumnSpan() { return ColumnSpan::None; }
constexpr Style::ColumnWidth RenderStyle::initialColumnWidth() { return CSS::Keyword::Auto { }; }
inline Style::ContainIntrinsicSize RenderStyle::initialContainIntrinsicHeight() { return CSS::Keyword::None { }; }
inline Style::ContainIntrinsicSize RenderStyle::initialContainIntrinsicWidth() { return CSS::Keyword::None { }; }
inline Style::ContainerNames RenderStyle::initialContainerNames() { return CSS::Keyword::None { }; }
constexpr ContainerType RenderStyle::initialContainerType() { return ContainerType::Normal; }
constexpr OptionSet<Containment> RenderStyle::initialContainment() { return { }; }
inline Style::Content RenderStyle::initialContent() { return CSS::Keyword::Normal { }; }
constexpr StyleContentAlignmentData RenderStyle::initialContentAlignment() { return { }; }
constexpr ContentVisibility RenderStyle::initialContentVisibility() { return ContentVisibility::Visible; }
constexpr Style::CornerShapeValue RenderStyle::initialCornerShapeValue() { return Style::CornerShapeValue::round(); }
inline Style::Cursor RenderStyle::initialCursor() { return CSS::Keyword::Auto { }; }
constexpr StyleSelfAlignmentData RenderStyle::initialDefaultAlignment() { return { ItemPosition::Normal, OverflowAlignment::Default }; }
constexpr TextDirection RenderStyle::initialDirection() { return TextDirection::LTR; }
constexpr DisplayType RenderStyle::initialDisplay() { return DisplayType::Inline; }
constexpr EmptyCell RenderStyle::initialEmptyCells() { return EmptyCell::Show; }
constexpr FieldSizing RenderStyle::initialFieldSizing() { return FieldSizing::Fixed; }
inline FilterOperations RenderStyle::initialFilter() { return { }; }
inline Style::FlexBasis RenderStyle::initialFlexBasis() { return CSS::Keyword::Auto { }; }
constexpr FlexDirection RenderStyle::initialFlexDirection() { return FlexDirection::Row; }
constexpr Style::FlexGrow RenderStyle::initialFlexGrow() { return 0_css_number; }
constexpr Style::FlexShrink RenderStyle::initialFlexShrink() { return 1_css_number; }
constexpr FlexWrap RenderStyle::initialFlexWrap() { return FlexWrap::NoWrap; }
constexpr Float RenderStyle::initialFloating() { return Float::None; }
inline Style::GridTrackSizes RenderStyle::initialGridAutoColumns() { return CSS::Keyword::Auto { }; }
constexpr GridAutoFlow RenderStyle::initialGridAutoFlow() { return AutoFlowRow; }
inline Style::GridTrackSizes RenderStyle::initialGridAutoRows() { return CSS::Keyword::Auto { }; }
inline Style::GridPosition RenderStyle::initialGridItemColumnEnd() { return CSS::Keyword::Auto { }; }
inline Style::GridPosition RenderStyle::initialGridItemColumnStart() { return CSS::Keyword::Auto { }; }
inline Style::GridPosition RenderStyle::initialGridItemRowEnd() { return CSS::Keyword::Auto { }; }
inline Style::GridPosition RenderStyle::initialGridItemRowStart() { return CSS::Keyword::Auto { }; }
inline Style::GridTemplateList RenderStyle::initialGridTemplateColumns() { return CSS::Keyword::None { }; }
inline Style::GridTemplateList RenderStyle::initialGridTemplateRows() { return CSS::Keyword::None { }; }
inline Style::GridTemplateAreas RenderStyle::initialGridTemplateAreas() { return CSS::Keyword::None { }; }
constexpr OptionSet<HangingPunctuation> RenderStyle::initialHangingPunctuation() { return { }; }
constexpr Style::HyphenateLimitEdge RenderStyle::initialHyphenateLimitAfter() { return CSS::Keyword::Auto { }; }
constexpr Style::HyphenateLimitEdge RenderStyle::initialHyphenateLimitBefore() { return CSS::Keyword::Auto { }; }
constexpr Style::HyphenateLimitLines RenderStyle::initialHyphenateLimitLines() { return CSS::Keyword::NoLimit { }; }
inline Style::HyphenateCharacter RenderStyle::initialHyphenateCharacter() { return CSS::Keyword::Auto { }; }
constexpr Hyphens RenderStyle::initialHyphens() { return Hyphens::Manual; }
constexpr ImageOrientation RenderStyle::initialImageOrientation() { return ImageOrientation::Orientation::FromImage; }
constexpr ImageRendering RenderStyle::initialImageRendering() { return ImageRendering::Auto; }
inline Style::InsetEdge RenderStyle::initialInset() { return CSS::Keyword::Auto { }; }
constexpr FloatSize RenderStyle::initialInitialLetter() { return { }; }
constexpr InputSecurity RenderStyle::initialInputSecurity() { return InputSecurity::Auto; }
constexpr LineJoin RenderStyle::initialJoinStyle() { return LineJoin::Miter; }
constexpr StyleSelfAlignmentData RenderStyle::initialJustifyItems() { return { ItemPosition::Legacy }; }
inline const Style::InsetBox& RenderStyle::insetBox() const { return m_nonInheritedData->surroundData->inset; }
inline const FloatSize& RenderStyle::initialLetter() const { return m_nonInheritedData->rareData->initialLetter; }
inline float RenderStyle::initialLetterDrop() const { return initialLetter().width(); }
inline float RenderStyle::initialLetterHeight() const { return initialLetter().height(); }
constexpr LineAlign RenderStyle::initialLineAlign() { return LineAlign::None; }
constexpr OptionSet<Style::LineBoxContain> RenderStyle::initialLineBoxContain() { return { Style::LineBoxContain::Block, Style::LineBoxContain::Inline, Style::LineBoxContain::Replaced }; }
constexpr LineBreak RenderStyle::initialLineBreak() { return LineBreak::Auto; }
constexpr LineClampValue RenderStyle::initialLineClamp() { return { }; }
inline Style::WebkitLineGrid RenderStyle::initialLineGrid() { return CSS::Keyword::None { }; }
constexpr LineSnap RenderStyle::initialLineSnap() { return LineSnap::None; }
constexpr ListStylePosition RenderStyle::initialListStylePosition() { return ListStylePosition::Outside; }
inline Style::ListStyleType RenderStyle::initialListStyleType() { return CSS::Keyword::Disc { }; }
inline Style::MarginEdge RenderStyle::initialMargin() { return 0_css_px; }
constexpr OptionSet<MarginTrimType> RenderStyle::initialMarginTrim() { return { }; }
constexpr MarqueeBehavior RenderStyle::initialMarqueeBehavior() { return MarqueeBehavior::Scroll; }
constexpr MarqueeDirection RenderStyle::initialMarqueeDirection() { return MarqueeDirection::Auto; }
inline Length RenderStyle::initialMarqueeIncrement() { return { 6, LengthType::Fixed }; }
inline Style::MaskBorder RenderStyle::initialMaskBorder() { return Style::MaskBorder { }; }
inline Style::MaskBorderSource RenderStyle::initialMaskBorderSource() { return CSS::Keyword::None { }; }
constexpr MathShift RenderStyle::initialMathShift() { return MathShift::Normal; }
constexpr MathStyle RenderStyle::initialMathStyle() { return MathStyle::Normal; }
constexpr Style::MaximumLines RenderStyle::initialMaxLines() { return CSS::Keyword::None { }; }
inline Style::MaximumSize RenderStyle::initialMaxSize() { return CSS::Keyword::None { }; }
inline Style::MinimumSize RenderStyle::initialMinSize() { return CSS::Keyword::Auto { }; }
constexpr NBSPMode RenderStyle::initialNBSPMode() { return NBSPMode::Normal; }
constexpr ObjectFit RenderStyle::initialObjectFit() { return ObjectFit::Fill; }
inline Style::ObjectPosition RenderStyle::initialObjectPosition() { return { 50_css_percentage, 50_css_percentage }; }
inline Style::OffsetAnchor RenderStyle::initialOffsetAnchor() { return CSS::Keyword::Auto { }; }
inline Style::OffsetDistance RenderStyle::initialOffsetDistance() { return 0_css_px; }
inline Style::OffsetPath RenderStyle::initialOffsetPath() { return CSS::Keyword::None { }; }
inline Style::OffsetPosition RenderStyle::initialOffsetPosition() { return CSS::Keyword::Normal { }; }
constexpr Style::OffsetRotate RenderStyle::initialOffsetRotate() { return CSS::Keyword::Auto { }; }
constexpr Style::Opacity RenderStyle::initialOpacity() { return 1_css_number; }
constexpr Style::Order RenderStyle::initialOrder() { return 0_css_integer; }
constexpr Style::Orphans RenderStyle::initialOrphans() { return CSS::Keyword::Auto { }; }
constexpr OverflowAnchor RenderStyle::initialOverflowAnchor() { return OverflowAnchor::Auto; }
inline OverflowContinue RenderStyle::initialOverflowContinue() { return OverflowContinue::Auto; }
constexpr Style::Length<> RenderStyle::initialOutlineOffset() { return 0_css_px; }
constexpr OutlineStyle RenderStyle::initialOutlineStyle() { return OutlineStyle::None; }
constexpr Style::LineWidth RenderStyle::initialOutlineWidth() { return CSS::Keyword::Medium { }; }
constexpr OverflowWrap RenderStyle::initialOverflowWrap() { return OverflowWrap::Normal; }
constexpr Overflow RenderStyle::initialOverflowX() { return Overflow::Visible; }
constexpr Overflow RenderStyle::initialOverflowY() { return Overflow::Visible; }
constexpr OverscrollBehavior RenderStyle::initialOverscrollBehaviorX() { return OverscrollBehavior::Auto; }
constexpr OverscrollBehavior RenderStyle::initialOverscrollBehaviorY() { return OverscrollBehavior::Auto; }
inline Style::PaddingEdge RenderStyle::initialPadding() { return 0_css_px; }
constexpr PaintOrder RenderStyle::initialPaintOrder() { return PaintOrder::Normal; }
inline Style::Perspective RenderStyle::initialPerspective() { return CSS::Keyword::None { }; }
inline Style::PerspectiveOrigin RenderStyle::initialPerspectiveOrigin() { return { initialPerspectiveOriginX(), initialPerspectiveOriginY() }; }
inline Style::PerspectiveOriginX RenderStyle::initialPerspectiveOriginX() { return 50_css_percentage; }
inline Style::PerspectiveOriginY RenderStyle::initialPerspectiveOriginY() { return 50_css_percentage; }
constexpr PointerEvents RenderStyle::initialPointerEvents() { return PointerEvents::Auto; }
constexpr PositionType RenderStyle::initialPosition() { return PositionType::Static; }
inline std::optional<Style::ScopedName> RenderStyle::initialPositionAnchor() { return { }; }
inline std::optional<PositionArea> RenderStyle::initialPositionArea() { return { }; }
inline FixedVector<Style::PositionTryFallback> RenderStyle::initialPositionTryFallbacks() { return { }; }
constexpr Style::PositionTryOrder RenderStyle::initialPositionTryOrder() { return Style::PositionTryOrder::Normal; }
constexpr OptionSet<PositionVisibility> RenderStyle::initialPositionVisibility() { return PositionVisibility::AnchorsVisible; }
constexpr PrintColorAdjust RenderStyle::initialPrintColorAdjust() { return PrintColorAdjust::Economy; }
inline Style::Quotes RenderStyle::initialQuotes() { return CSS::Keyword::Auto { }; }
constexpr Order RenderStyle::initialRTLOrdering() { return Order::Logical; }
constexpr Resize RenderStyle::initialResize() { return Resize::None; }
inline Style::GapGutter RenderStyle::initialRowGap() { return CSS::Keyword::Normal { }; }
constexpr RubyPosition RenderStyle::initialRubyPosition() { return RubyPosition::Over; }
constexpr RubyAlign RenderStyle::initialRubyAlign() { return RubyAlign::SpaceAround; }
constexpr RubyOverhang RenderStyle::initialRubyOverhang() { return RubyOverhang::Auto; }
constexpr Style::ScrollBehavior RenderStyle::initialScrollBehavior() { return Style::ScrollBehavior::Auto; }
inline Style::ScrollMarginEdge RenderStyle::initialScrollMargin() { return 0_css_px; }
inline Style::ScrollPaddingEdge RenderStyle::initialScrollPadding() { return CSS::Keyword::Auto { }; }
inline Style::ProgressTimelineAxes RenderStyle::initialScrollTimelineAxes() { return CSS::Keyword::Block { }; }
inline Style::ProgressTimelineNames RenderStyle::initialScrollTimelineNames() { return CSS::Keyword::None { }; }
inline Style::ScrollbarColor RenderStyle::initialScrollbarColor() { return CSS::Keyword::Auto { }; }
constexpr Style::ScrollbarGutter RenderStyle::initialScrollbarGutter() { return CSS::Keyword::Auto { }; }
constexpr ScrollbarWidth RenderStyle::initialScrollbarWidth() { return ScrollbarWidth::Auto; }
constexpr StyleSelfAlignmentData RenderStyle::initialSelfAlignment() { return { ItemPosition::Auto, OverflowAlignment::Default }; }
constexpr Style::ShapeImageThreshold RenderStyle::initialShapeImageThreshold() { return 0_css_number; }
inline Style::ShapeMargin RenderStyle::initialShapeMargin() { return 0_css_px; }
inline Style::ShapeOutside RenderStyle::initialShapeOutside() { return CSS::Keyword::None { }; }
inline Style::PreferredSize RenderStyle::initialSize() { return CSS::Keyword::Auto { }; }
constexpr OptionSet<SpeakAs> RenderStyle::initialSpeakAs() { return { }; }
constexpr Style::ZIndex RenderStyle::initialSpecifiedZIndex() { return CSS::Keyword::Auto { }; }
inline Style::Color RenderStyle::initialStrokeColor() { return { Color::transparentBlack }; }
constexpr Style::StrokeMiterlimit RenderStyle::initialStrokeMiterLimit() { return 4_css_number; }
inline Style::StrokeWidth RenderStyle::initialStrokeWidth() { return 1_css_px; }
constexpr TabSize RenderStyle::initialTabSize() { return 8; }
constexpr TableLayoutType RenderStyle::initialTableLayout() { return TableLayoutType::Auto; }
constexpr TextAlignMode RenderStyle::initialTextAlign() { return TextAlignMode::Start; }
constexpr TextAlignLast RenderStyle::initialTextAlignLast() { return TextAlignLast::Auto; }
constexpr TextBoxTrim RenderStyle::initialTextBoxTrim() { return TextBoxTrim::None; }
constexpr TextCombine RenderStyle::initialTextCombine() { return TextCombine::None; }
inline Style::Color RenderStyle::initialTextDecorationColor() { return Style::Color::currentColor(); }
constexpr OptionSet<TextDecorationLine> RenderStyle::initialTextDecorationLine() { return { }; }
constexpr TextDecorationSkipInk RenderStyle::initialTextDecorationSkipInk() { return TextDecorationSkipInk::Auto; }
constexpr TextDecorationStyle RenderStyle::initialTextDecorationStyle() { return TextDecorationStyle::Solid; }
inline Style::TextDecorationThickness RenderStyle::initialTextDecorationThickness() { return CSS::Keyword::Auto { }; }
inline Style::Color RenderStyle::initialTextEmphasisColor() { return Style::Color::currentColor(); }
inline Style::TextEmphasisStyle RenderStyle::initialTextEmphasisStyle() { return CSS::Keyword::None { }; }
constexpr OptionSet<TextEmphasisPosition> RenderStyle::initialTextEmphasisPosition() { return { TextEmphasisPosition::Over, TextEmphasisPosition::Right }; }
inline Style::Color RenderStyle::initialTextFillColor() { return Style::Color::currentColor(); }
inline bool RenderStyle::hasExplicitlySetColor() const { return m_inheritedFlags.hasExplicitlySetColor; }
constexpr TextGroupAlign RenderStyle::initialTextGroupAlign() { return TextGroupAlign::None; }
inline Style::TextIndent RenderStyle::initialTextIndent() { return 0_css_px; }
constexpr TextJustify RenderStyle::initialTextJustify() { return TextJustify::Auto; }
constexpr TextOrientation RenderStyle::initialTextOrientation() { return TextOrientation::Mixed; }
constexpr TextOverflow RenderStyle::initialTextOverflow() { return TextOverflow::Clip; }
constexpr TextSecurity RenderStyle::initialTextSecurity() { return TextSecurity::None; }
inline Style::TextShadows RenderStyle::initialTextShadow() { return CSS::Keyword::None { }; }
inline Style::Color RenderStyle::initialTextStrokeColor() { return Style::Color::currentColor(); }
constexpr Style::WebkitTextStrokeWidth RenderStyle::initialTextStrokeWidth() { return 0_css_px; }
constexpr OptionSet<TextTransform> RenderStyle::initialTextTransform() { return { }; }
inline Style::TextUnderlineOffset RenderStyle::initialTextUnderlineOffset() { return CSS::Keyword::Auto { }; }
constexpr OptionSet<TextUnderlinePosition> RenderStyle::initialTextUnderlinePosition() { return { }; }
constexpr TextWrapMode RenderStyle::initialTextWrapMode() { return TextWrapMode::Wrap; }
constexpr TextWrapStyle RenderStyle::initialTextWrapStyle() { return TextWrapStyle::Auto; }
constexpr TextZoom RenderStyle::initialTextZoom() { return TextZoom::Normal; }
constexpr TouchAction RenderStyle::initialTouchActions() { return TouchAction::Auto; }
inline TransformOperations RenderStyle::initialTransform() { return { }; }
constexpr TransformBox RenderStyle::initialTransformBox() { return TransformBox::ViewBox; }
inline Style::Rotate RenderStyle::initialRotate() { return CSS::Keyword::None { }; }
inline Style::Scale RenderStyle::initialScale() { return CSS::Keyword::None { }; }
inline Style::Translate RenderStyle::initialTranslate() { return CSS::Keyword::None { }; }
inline Style::TransformOrigin RenderStyle::initialTransformOrigin() { return { initialTransformOriginX(), initialTransformOriginY(), initialTransformOriginZ() }; }
inline Style::TransformOriginX RenderStyle::initialTransformOriginX() { return 50_css_percentage; }
inline Style::TransformOriginY RenderStyle::initialTransformOriginY() { return 50_css_percentage; }
inline Style::TransformOriginZ RenderStyle::initialTransformOriginZ() { return 0_css_px; }
constexpr TransformStyle3D RenderStyle::initialTransformStyle3D() { return TransformStyle3D::Flat; }
constexpr UnicodeBidi RenderStyle::initialUnicodeBidi() { return UnicodeBidi::Normal; }
constexpr Style::ZIndex RenderStyle::initialUsedZIndex() { return CSS::Keyword::Auto { }; }
constexpr UserDrag RenderStyle::initialUserDrag() { return UserDrag::Auto; }
constexpr UserModify RenderStyle::initialUserModify() { return UserModify::ReadOnly; }
constexpr UserSelect RenderStyle::initialUserSelect() { return UserSelect::Text; }
inline Style::VerticalAlign RenderStyle::initialVerticalAlign() { return CSS::Keyword::Baseline { }; }
inline Style::ProgressTimelineAxes RenderStyle::initialViewTimelineAxes() { return CSS::Keyword::Block { };}
inline Style::ViewTimelineInsets RenderStyle::initialViewTimelineInsets() { return CSS::Keyword::Auto { };}
inline Style::ProgressTimelineNames RenderStyle::initialViewTimelineNames() { return CSS::Keyword::None { }; }
inline Style::ViewTransitionClasses RenderStyle::initialViewTransitionClasses() { return CSS::Keyword::None { }; }
inline Style::ViewTransitionName RenderStyle::initialViewTransitionName() { return CSS::Keyword::None { }; }
constexpr Visibility RenderStyle::initialVisibility() { return Visibility::Visible; }
inline const NameScope RenderStyle::initialTimelineScope() { return { }; }
constexpr WhiteSpaceCollapse RenderStyle::initialWhiteSpaceCollapse() { return WhiteSpaceCollapse::Collapse; }
constexpr Style::Widows RenderStyle::initialWidows() { return CSS::Keyword::Auto { }; }
constexpr WordBreak RenderStyle::initialWordBreak() { return WordBreak::Normal; }
inline Length RenderStyle::initialLetterSpacing() { return zeroLength(); }
inline Length RenderStyle::initialWordSpacing() { return zeroLength(); }
constexpr StyleWritingMode RenderStyle::initialWritingMode() { return StyleWritingMode::HorizontalTb; }
inline InputSecurity RenderStyle::inputSecurity() const { return static_cast<InputSecurity>(m_nonInheritedData->rareData->inputSecurity); }
inline bool RenderStyle::isColumnFlexDirection() const { return flexDirection() == FlexDirection::Column || flexDirection() == FlexDirection::ColumnReverse; }
inline bool RenderStyle::isRowFlexDirection() const { return flexDirection() == FlexDirection::Row || flexDirection() == FlexDirection::RowReverse; }
constexpr bool RenderStyle::isDisplayBlockLevel() const { return isDisplayBlockType(display()); }
constexpr bool RenderStyle::isDisplayDeprecatedFlexibleBox(DisplayType display) { return display == DisplayType::Box || display == DisplayType::InlineBox; }
constexpr bool RenderStyle::isDisplayFlexibleBox(DisplayType display) { return display == DisplayType::Flex || display == DisplayType::InlineFlex; }
constexpr bool RenderStyle::isDisplayDeprecatedFlexibleBox() const { return isDisplayDeprecatedFlexibleBox(display()); }
constexpr bool RenderStyle::isDisplayFlexibleBoxIncludingDeprecatedOrGridBox() const { return isDisplayFlexibleOrGridBox() || isDisplayDeprecatedFlexibleBox(); }
constexpr bool RenderStyle::isDisplayFlexibleOrGridBox() const { return isDisplayFlexibleOrGridBox(display()); }
constexpr bool RenderStyle::isDisplayFlexibleOrGridBox(DisplayType display) { return isDisplayFlexibleBox(display) || isDisplayGridBox(display); }
constexpr bool RenderStyle::isDisplayGridBox(DisplayType display) { return display == DisplayType::Grid || display == DisplayType::InlineGrid; }
constexpr bool RenderStyle::isDisplayInlineType() const { return isDisplayInlineType(display()); }
constexpr bool RenderStyle::isDisplayListItemType(DisplayType display) { return display == DisplayType::ListItem; }
constexpr bool RenderStyle::isDisplayTableOrTablePart() const { return isDisplayTableOrTablePart(display()); }
constexpr bool RenderStyle::isInternalTableBox() const { return isInternalTableBox(display()); }
constexpr bool RenderStyle::isRubyContainerOrInternalRubyBox() const { return isRubyContainerOrInternalRubyBox(display()); }
inline bool RenderStyle::isFixedTableLayout() const { return tableLayout() == TableLayoutType::Fixed && (logicalWidth().isSpecified() || logicalWidth().isFitContent() || logicalWidth().isFillAvailable() || logicalWidth().isMinContent()); }
inline bool RenderStyle::isFloating() const { return floating() != Float::None; }
inline bool RenderStyle::isGridAutoFlowAlgorithmDense() const { return m_nonInheritedData->rareData->grid->m_gridAutoFlow & InternalAutoFlowAlgorithmDense; }
inline bool RenderStyle::isGridAutoFlowAlgorithmSparse() const { return m_nonInheritedData->rareData->grid->m_gridAutoFlow & InternalAutoFlowAlgorithmSparse; }
inline bool RenderStyle::isGridAutoFlowDirectionColumn() const { return m_nonInheritedData->rareData->grid->m_gridAutoFlow & InternalAutoFlowDirectionColumn; }
inline bool RenderStyle::isGridAutoFlowDirectionRow() const { return m_nonInheritedData->rareData->grid->m_gridAutoFlow & InternalAutoFlowDirectionRow; }
constexpr bool RenderStyle::isOriginalDisplayBlockType() const { return isDisplayBlockType(originalDisplay()); }
constexpr bool RenderStyle::isOriginalDisplayInlineType() const { return isDisplayInlineType(originalDisplay()); }
constexpr bool RenderStyle::isOriginalDisplayListItemType() const { return isDisplayListItemType(originalDisplay()); }
inline bool RenderStyle::isOverflowVisible() const { return overflowX() == Overflow::Visible || overflowY() == Overflow::Visible; }
inline bool RenderStyle::isReverseFlexDirection() const { return flexDirection() == FlexDirection::RowReverse || flexDirection() == FlexDirection::ColumnReverse; }
inline LineJoin RenderStyle::joinStyle() const { return static_cast<LineJoin>(m_rareInheritedData->joinStyle); }
inline const StyleContentAlignmentData& RenderStyle::justifyContent() const { return m_nonInheritedData->miscData->justifyContent; }
inline const StyleSelfAlignmentData& RenderStyle::justifyItems() const { return m_nonInheritedData->miscData->justifyItems; }
inline const StyleSelfAlignmentData& RenderStyle::justifySelf() const { return m_nonInheritedData->miscData->justifySelf; }
inline const Style::InsetEdge& RenderStyle::left() const { return m_nonInheritedData->surroundData->inset.left(); }
inline float RenderStyle::letterSpacing() const { return m_inheritedData->fontData->fontCascade.letterSpacing(); }
inline const FontCascade& RenderStyle::fontCascade() const { return m_inheritedData->fontData->fontCascade; }
inline LineAlign RenderStyle::lineAlign() const { return static_cast<LineAlign>(m_rareInheritedData->lineAlign); }
inline OptionSet<Style::LineBoxContain> RenderStyle::lineBoxContain() const { return OptionSet<Style::LineBoxContain>::fromRaw(m_rareInheritedData->lineBoxContain); }
inline LineBreak RenderStyle::lineBreak() const { return static_cast<LineBreak>(m_rareInheritedData->lineBreak); }
inline const LineClampValue& RenderStyle::lineClamp() const { return m_nonInheritedData->rareData->lineClamp; }
inline const Style::WebkitLineGrid& RenderStyle::lineGrid() const { return m_rareInheritedData->lineGrid; }
inline LineSnap RenderStyle::lineSnap() const { return static_cast<LineSnap>(m_rareInheritedData->lineSnap); }
inline const Style::ListStyleType& RenderStyle::listStyleType() const { return m_rareInheritedData->listStyleType; }
inline const Style::InsetEdge& RenderStyle::logicalBottom() const { return m_nonInheritedData->surroundData->inset.after(writingMode()); }
inline const Style::PreferredSize& RenderStyle::logicalHeight() const { return logicalHeight(writingMode()); }
inline const Style::PreferredSize& RenderStyle::logicalHeight(const WritingMode writingMode) const { return writingMode.isHorizontal() ? height() : width(); }
inline const Style::InsetEdge& RenderStyle::logicalLeft() const { return m_nonInheritedData->surroundData->inset.logicalLeft(writingMode()); }
inline const Style::MaximumSize& RenderStyle::logicalMaxHeight() const { return logicalMaxHeight(writingMode()); }
inline const Style::MaximumSize& RenderStyle::logicalMaxHeight(const WritingMode writingMode) const { return writingMode.isHorizontal() ? maxHeight() : maxWidth(); }
inline const Style::MaximumSize& RenderStyle::logicalMaxWidth() const { return logicalMaxWidth(writingMode()); }
inline const Style::MaximumSize& RenderStyle::logicalMaxWidth(const WritingMode writingMode) const { return writingMode.isHorizontal() ? maxWidth() : maxHeight(); }
inline const Style::MinimumSize& RenderStyle::logicalMinHeight() const { return logicalMinHeight(writingMode()); }
inline const Style::MinimumSize& RenderStyle::logicalMinHeight(const WritingMode writingMode) const { return writingMode.isHorizontal() ? minHeight() : minWidth(); }
inline const Style::MinimumSize& RenderStyle::logicalMinWidth() const { return logicalMinWidth(writingMode()); }
inline const Style::MinimumSize& RenderStyle::logicalMinWidth(const WritingMode writingMode) const { return writingMode.isHorizontal() ? minWidth() : minHeight(); }
inline const Style::InsetEdge& RenderStyle::logicalRight() const { return m_nonInheritedData->surroundData->inset.logicalRight(writingMode()); }
inline const Style::InsetEdge& RenderStyle::logicalTop() const { return m_nonInheritedData->surroundData->inset.before(writingMode()); }
inline const Style::PreferredSize& RenderStyle::logicalWidth() const { return logicalWidth(writingMode()); }
inline const Style::PreferredSize& RenderStyle::logicalWidth(const WritingMode writingMode) const { return writingMode.isHorizontal() ? width() : height(); }
inline const Style::MarginBox& RenderStyle::marginBox() const { return m_nonInheritedData->surroundData->margin; }
inline const Style::MarginEdge& RenderStyle::marginAfter() const { return marginAfter(writingMode()); }
inline const Style::MarginEdge& RenderStyle::marginAfter(const WritingMode writingMode) const { return m_nonInheritedData->surroundData->margin.after(writingMode); }
inline const Style::MarginEdge& RenderStyle::marginBefore() const { return marginBefore(writingMode()); }
inline const Style::MarginEdge& RenderStyle::marginBefore(const WritingMode writingMode) const { return m_nonInheritedData->surroundData->margin.before(writingMode); }
inline const Style::MarginEdge& RenderStyle::marginBottom() const { return m_nonInheritedData->surroundData->margin.bottom(); }
inline const Style::MarginEdge& RenderStyle::marginEnd() const { return marginEnd(writingMode()); }
inline const Style::MarginEdge& RenderStyle::marginEnd(const WritingMode writingMode) const { return m_nonInheritedData->surroundData->margin.end(writingMode); }
inline const Style::MarginEdge& RenderStyle::marginLeft() const { return m_nonInheritedData->surroundData->margin.left(); }
inline const Style::MarginEdge& RenderStyle::marginRight() const { return m_nonInheritedData->surroundData->margin.right(); }
inline const Style::MarginEdge& RenderStyle::marginStart() const { return marginStart(writingMode()); }
inline const Style::MarginEdge& RenderStyle::marginStart(const WritingMode writingMode) const { return m_nonInheritedData->surroundData->margin.start(writingMode); }
inline const Style::MarginEdge& RenderStyle::marginTop() const { return m_nonInheritedData->surroundData->margin.top(); }
inline OptionSet<MarginTrimType> RenderStyle::marginTrim() const { return m_nonInheritedData->rareData->marginTrim; }
inline MarqueeBehavior RenderStyle::marqueeBehavior() const { return static_cast<MarqueeBehavior>(m_nonInheritedData->rareData->marquee->behavior); }
inline MarqueeDirection RenderStyle::marqueeDirection() const { return static_cast<MarqueeDirection>(m_nonInheritedData->rareData->marquee->direction); }
inline const Length& RenderStyle::marqueeIncrement() const { return m_nonInheritedData->rareData->marquee->increment; }
inline int RenderStyle::marqueeLoopCount() const { return m_nonInheritedData->rareData->marquee->loops; }
inline int RenderStyle::marqueeSpeed() const { return m_nonInheritedData->rareData->marquee->speed; }
inline const Style::MaskBorder& RenderStyle::maskBorder() const { return m_nonInheritedData->rareData->maskBorder; }
inline NinePieceImageRule RenderStyle::maskBorderHorizontalRule() const { return maskBorderRepeat().horizontalRule(); }
inline const Style::MaskBorderOutset& RenderStyle::maskBorderOutset() const { return maskBorder().outset(); }
inline LayoutBoxExtent RenderStyle::maskBorderOutsets() const { return imageOutsets(maskBorder()); }
inline const Style::MaskBorderRepeat& RenderStyle::maskBorderRepeat() const { return maskBorder().repeat(); }
inline const Style::MaskBorderSlice& RenderStyle::maskBorderSlice() const { return maskBorder().slice(); }
inline const Style::MaskBorderSource& RenderStyle::maskBorderSource() const { return maskBorder().source(); }
inline NinePieceImageRule RenderStyle::maskBorderVerticalRule() const { return maskBorderRepeat().verticalRule(); }
inline const Style::MaskBorderWidth& RenderStyle::maskBorderWidth() const { return maskBorder().width(); }
inline FillBox RenderStyle::maskClip() const { return maskLayers().clip(); }
inline CompositeOperator RenderStyle::maskComposite() const { return maskLayers().composite(); }
inline StyleImage* RenderStyle::maskImage() const { return maskLayers().image(); }
inline const FillLayer& RenderStyle::maskLayers() const { return m_nonInheritedData->miscData->mask; }
inline Ref<const FillLayer> RenderStyle::protectedMaskLayers() const { return maskLayers(); }
inline FillBox RenderStyle::maskOrigin() const { return maskLayers().origin(); }
inline FillRepeatXY RenderStyle::maskRepeat() const { return maskLayers().repeat(); }
inline const LengthSize& RenderStyle::maskSizeLength() const { return maskLayers().sizeLength(); }
inline FillSizeType RenderStyle::maskSizeType() const { return maskLayers().sizeType(); }
inline MathShift RenderStyle::mathShift() const { return static_cast<MathShift>(m_rareInheritedData->mathShift); }
inline MathStyle RenderStyle::mathStyle() const { return static_cast<MathStyle>(m_rareInheritedData->mathStyle); }
inline const Style::MaximumSize& RenderStyle::maxHeight() const { return m_nonInheritedData->boxData->maxHeight(); }
inline Style::MaximumLines RenderStyle::maxLines() const { return m_nonInheritedData->rareData->maxLines; }
inline const Style::MaximumSize& RenderStyle::maxWidth() const { return m_nonInheritedData->boxData->maxWidth(); }
inline const Style::MinimumSize& RenderStyle::minHeight() const { return m_nonInheritedData->boxData->minHeight(); }
inline const Style::MinimumSize& RenderStyle::minWidth() const { return m_nonInheritedData->boxData->minWidth(); }
inline NBSPMode RenderStyle::nbspMode() const { return static_cast<NBSPMode>(m_rareInheritedData->nbspMode); }
inline const Style::CustomPropertyData& RenderStyle::nonInheritedCustomProperties() const { return m_nonInheritedData->rareData->customProperties.get(); }
inline ObjectFit RenderStyle::objectFit() const { return static_cast<ObjectFit>(m_nonInheritedData->miscData->objectFit); }
inline const Style::ObjectPosition& RenderStyle::objectPosition() const { return m_nonInheritedData->miscData->objectPosition; }
inline const Style::OffsetAnchor& RenderStyle::offsetAnchor() const { return m_nonInheritedData->rareData->offsetAnchor; }
inline const Style::OffsetDistance& RenderStyle::offsetDistance() const { return m_nonInheritedData->rareData->offsetDistance; }
inline const Style::OffsetPath& RenderStyle::offsetPath() const { return m_nonInheritedData->rareData->offsetPath; }
inline const Style::OffsetPosition& RenderStyle::offsetPosition() const { return m_nonInheritedData->rareData->offsetPosition; }
inline const Style::OffsetRotate& RenderStyle::offsetRotate() const { return m_nonInheritedData->rareData->offsetRotate; }
inline Style::Opacity RenderStyle::opacity() const { return m_nonInheritedData->miscData->opacity; }
inline Style::Order RenderStyle::order() const { return m_nonInheritedData->miscData->order; }
inline Style::Orphans RenderStyle::orphans() const { return m_rareInheritedData->orphans; }
inline const OutlineValue& RenderStyle::outline() const { return m_nonInheritedData->backgroundData->outline; }
inline const Style::Color& RenderStyle::outlineColor() const { return outline().color(); }
inline OutlineStyle RenderStyle::outlineStyle() const { return outline().style(); }
inline OverflowAnchor RenderStyle::overflowAnchor() const { return static_cast<OverflowAnchor>(m_nonInheritedData->rareData->overflowAnchor); }
inline OverflowContinue RenderStyle::overflowContinue() const { return m_nonInheritedData->rareData->overflowContinue; }
inline OverflowWrap RenderStyle::overflowWrap() const { return static_cast<OverflowWrap>(m_rareInheritedData->overflowWrap); }
inline OverscrollBehavior RenderStyle::overscrollBehaviorX() const { return static_cast<OverscrollBehavior>(m_nonInheritedData->rareData->overscrollBehaviorX); }
inline OverscrollBehavior RenderStyle::overscrollBehaviorY() const { return static_cast<OverscrollBehavior>(m_nonInheritedData->rareData->overscrollBehaviorY); }
inline const Style::PaddingEdge& RenderStyle::paddingAfter() const { return paddingAfter(writingMode()); }
inline const Style::PaddingEdge& RenderStyle::paddingAfter(const WritingMode writingMode) const { return paddingBox().after(writingMode); }
inline const Style::PaddingEdge& RenderStyle::paddingBefore() const { return paddingBefore(writingMode()); }
inline const Style::PaddingEdge& RenderStyle::paddingBefore(const WritingMode writingMode) const { return paddingBox().before(writingMode); }
inline const Style::PaddingEdge& RenderStyle::paddingBottom() const { return paddingBox().bottom(); }
inline const Style::PaddingBox& RenderStyle::paddingBox() const { return m_nonInheritedData->surroundData->padding; }
inline const Style::PaddingEdge& RenderStyle::paddingEnd() const { return paddingEnd(writingMode()); }
inline const Style::PaddingEdge& RenderStyle::paddingEnd(const WritingMode writingMode) const { return paddingBox().end(writingMode); }
inline const Style::PaddingEdge& RenderStyle::paddingLeft() const { return paddingBox().left(); }
inline const Style::PaddingEdge& RenderStyle::paddingRight() const { return paddingBox().right(); }
inline const Style::PaddingEdge& RenderStyle::paddingStart() const { return paddingStart(writingMode()); }
inline const Style::PaddingEdge& RenderStyle::paddingStart(const WritingMode writingMode) const { return paddingBox().start(writingMode); }
inline const Style::PaddingEdge& RenderStyle::paddingTop() const { return paddingBox().top(); }
inline const LengthSize& RenderStyle::pageSize() const { return m_nonInheritedData->rareData->pageSize; }
inline PageSizeType RenderStyle::pageSizeType() const { return static_cast<PageSizeType>(m_nonInheritedData->rareData->pageSizeType); }
inline PaintOrder RenderStyle::paintOrder() const { return static_cast<PaintOrder>(m_rareInheritedData->paintOrder); }
inline const Style::Perspective& RenderStyle::perspective() const { return m_nonInheritedData->rareData->perspective; }
inline const Style::PerspectiveOrigin& RenderStyle::perspectiveOrigin() const { return m_nonInheritedData->rareData->perspectiveOrigin; }
inline const Style::PerspectiveOriginX& RenderStyle::perspectiveOriginX() const { return m_nonInheritedData->rareData->perspectiveOrigin.x; }
inline const Style::PerspectiveOriginY& RenderStyle::perspectiveOriginY() const { return m_nonInheritedData->rareData->perspectiveOrigin.y; }
inline const std::optional<Style::ScopedName>& RenderStyle::positionAnchor() const { return m_nonInheritedData->rareData->positionAnchor; }
inline std::optional<PositionArea> RenderStyle::positionArea() const { return m_nonInheritedData->rareData->positionArea; }
inline Style::PositionTryOrder RenderStyle::positionTryOrder() const { return static_cast<Style::PositionTryOrder>(m_nonInheritedData->rareData->positionTryOrder); }
inline OptionSet<PositionVisibility> RenderStyle::positionVisibility() const { return OptionSet<PositionVisibility>::fromRaw(m_nonInheritedData->rareData->positionVisibility); }
inline bool RenderStyle::preserveNewline() const { return preserveNewline(whiteSpaceCollapse()); }
inline bool RenderStyle::preserves3D() const { return usedTransformStyle3D() == TransformStyle3D::Preserve3D; }
inline const Style::Quotes& RenderStyle::quotes() const { return m_rareInheritedData->quotes; }
inline Resize RenderStyle::resize() const { return static_cast<Resize>(m_nonInheritedData->miscData->resize); }
inline const Style::InsetEdge& RenderStyle::right() const { return m_nonInheritedData->surroundData->inset.right(); }
inline const Style::Rotate& RenderStyle::rotate() const { return m_nonInheritedData->rareData->rotate; }
inline const Style::GapGutter& RenderStyle::rowGap() const { return m_nonInheritedData->rareData->rowGap; }
inline RubyPosition RenderStyle::rubyPosition() const { return static_cast<RubyPosition>(m_rareInheritedData->rubyPosition); }
inline RubyAlign RenderStyle::rubyAlign() const { return static_cast<RubyAlign>(m_rareInheritedData->rubyAlign); }
inline RubyOverhang RenderStyle::rubyOverhang() const { return static_cast<RubyOverhang>(m_rareInheritedData->rubyOverhang); }
inline const Style::Scale& RenderStyle::scale() const { return m_nonInheritedData->rareData->scale; }
inline const Style::ScrollTimelines& RenderStyle::scrollTimelines() const { return m_nonInheritedData->rareData->scrollTimelines; }
inline const Style::ProgressTimelineAxes& RenderStyle::scrollTimelineAxes() const { return m_nonInheritedData->rareData->scrollTimelineAxes; }
inline const Style::ProgressTimelineNames& RenderStyle::scrollTimelineNames() const { return m_nonInheritedData->rareData->scrollTimelineNames; }
inline bool RenderStyle::hasScrollTimelines() const { return m_nonInheritedData->rareData->hasScrollTimelines(); }
inline const Style::ViewTimelines& RenderStyle::viewTimelines() const { return m_nonInheritedData->rareData->viewTimelines; }
inline const Style::ProgressTimelineAxes& RenderStyle::viewTimelineAxes() const { return m_nonInheritedData->rareData->viewTimelineAxes; }
inline const Style::ViewTimelineInsets& RenderStyle::viewTimelineInsets() const { return m_nonInheritedData->rareData->viewTimelineInsets; }
inline const Style::ProgressTimelineNames& RenderStyle::viewTimelineNames() const { return m_nonInheritedData->rareData->viewTimelineNames; }
inline bool RenderStyle::hasViewTimelines() const { return m_nonInheritedData->rareData->hasViewTimelines(); }
inline const NameScope& RenderStyle::timelineScope() const { return m_nonInheritedData->rareData->timelineScope; }
inline const Style::ScrollbarColor& RenderStyle::scrollbarColor() const { return m_rareInheritedData->scrollbarColor; }
inline const Style::ScrollbarGutter& RenderStyle::scrollbarGutter() const { return m_nonInheritedData->rareData->scrollbarGutter; }
inline ScrollbarWidth RenderStyle::scrollbarWidth() const { return static_cast<ScrollbarWidth>(m_nonInheritedData->rareData->scrollbarWidth); }
inline Style::ShapeImageThreshold RenderStyle::shapeImageThreshold() const { return m_nonInheritedData->rareData->shapeImageThreshold; }
inline const Style::ShapeMargin& RenderStyle::shapeMargin() const { return m_nonInheritedData->rareData->shapeMargin; }
inline const Style::ShapeOutside& RenderStyle::shapeOutside() const { return m_nonInheritedData->rareData->shapeOutside; }
inline ContentVisibility RenderStyle::usedContentVisibility() const { return static_cast<ContentVisibility>(m_rareInheritedData->usedContentVisibility); }
inline bool RenderStyle::isSkippedRootOrSkippedContent() const { return usedContentVisibility() != ContentVisibility::Visible; }
inline OptionSet<SpeakAs> RenderStyle::speakAs() const { return OptionSet<SpeakAs>::fromRaw(m_rareInheritedData->speakAs); }
inline const AtomString& RenderStyle::specifiedLocale() const { return fontDescription().specifiedLocale(); }
inline Style::ZIndex RenderStyle::specifiedZIndex() const { return m_nonInheritedData->boxData->specifiedZIndex(); }
inline bool RenderStyle::specifiesColumns() const { return !columnCount().isAuto() || !columnWidth().isAuto() || !hasInlineColumnAxis(); }
constexpr OptionSet<Containment> RenderStyle::strictContainment() { return { Containment::Size, Containment::Layout, Containment::Paint, Containment::Style }; }
inline const Style::Color& RenderStyle::strokeColor() const { return m_rareInheritedData->strokeColor; }
inline Style::StrokeMiterlimit RenderStyle::strokeMiterLimit() const { return m_rareInheritedData->miterLimit; }
inline const AtomString& RenderStyle::pseudoElementNameArgument() const { return m_nonInheritedData->rareData->pseudoElementNameArgument; }
inline const TabSize& RenderStyle::tabSize() const { return m_rareInheritedData->tabSize; }
inline TableLayoutType RenderStyle::tableLayout() const { return static_cast<TableLayoutType>(m_nonInheritedData->miscData->tableLayout); }
inline TextAlignLast RenderStyle::textAlignLast() const { return static_cast<TextAlignLast>(m_rareInheritedData->textAlignLast); }
inline TextBoxTrim RenderStyle::textBoxTrim() const { return static_cast<TextBoxTrim>(m_nonInheritedData->rareData->textBoxTrim); }
inline TextCombine RenderStyle::textCombine() const { return static_cast<TextCombine>(m_rareInheritedData->textCombine); }
inline const Style::Color& RenderStyle::textDecorationColor() const { return m_nonInheritedData->rareData->textDecorationColor; }
inline OptionSet<TextDecorationLine> RenderStyle::textDecorationLine() const { return OptionSet<TextDecorationLine>::fromRaw(m_nonInheritedFlags.textDecorationLine); }
inline TextDecorationSkipInk RenderStyle::textDecorationSkipInk() const { return static_cast<TextDecorationSkipInk>(m_rareInheritedData->textDecorationSkipInk); }
inline TextDecorationStyle RenderStyle::textDecorationStyle() const { return static_cast<TextDecorationStyle>(m_nonInheritedData->rareData->textDecorationStyle); }
inline const Style::TextDecorationThickness& RenderStyle::textDecorationThickness() const { return m_nonInheritedData->rareData->textDecorationThickness; }
inline OptionSet<TextDecorationLine> RenderStyle::textDecorationLineInEffect() const { return OptionSet<TextDecorationLine>::fromRaw(m_inheritedFlags.textDecorationLineInEffect); }
inline const Style::Color& RenderStyle::textEmphasisColor() const { return m_rareInheritedData->textEmphasisColor; }
inline const Style::TextEmphasisStyle& RenderStyle::textEmphasisStyle() const { return m_rareInheritedData->textEmphasisStyle; }
inline OptionSet<TextEmphasisPosition> RenderStyle::textEmphasisPosition() const { return OptionSet<TextEmphasisPosition>::fromRaw(m_rareInheritedData->textEmphasisPosition); }
inline const Style::Color& RenderStyle::textFillColor() const { return m_rareInheritedData->textFillColor; }
inline TextGroupAlign RenderStyle::textGroupAlign() const { return static_cast<TextGroupAlign>(m_nonInheritedData->rareData->textGroupAlign); }
inline const Style::TextIndent& RenderStyle::textIndent() const { return m_rareInheritedData->textIndent; }
inline TextJustify RenderStyle::textJustify() const { return static_cast<TextJustify>(m_rareInheritedData->textJustify); }
inline TextOverflow RenderStyle::textOverflow() const { return static_cast<TextOverflow>(m_nonInheritedData->miscData->textOverflow); }
inline TextSecurity RenderStyle::textSecurity() const { return static_cast<TextSecurity>(m_rareInheritedData->textSecurity); }
inline const Style::TextShadows& RenderStyle::textShadow() const { return m_rareInheritedData->textShadow; }
inline bool RenderStyle::hasTextShadow() const { return !textShadow().isNone(); }
inline const Style::Color& RenderStyle::textStrokeColor() const { return m_rareInheritedData->textStrokeColor; }
inline Style::WebkitTextStrokeWidth RenderStyle::textStrokeWidth() const { return m_rareInheritedData->textStrokeWidth; }
inline OptionSet<TextTransform> RenderStyle::textTransform() const { return OptionSet<TextTransform>::fromRaw(m_inheritedFlags.textTransform); }
inline const Style::TextUnderlineOffset& RenderStyle::textUnderlineOffset() const { return m_rareInheritedData->textUnderlineOffset; }
inline OptionSet<TextUnderlinePosition> RenderStyle::textUnderlinePosition() const { return OptionSet<TextUnderlinePosition>::fromRaw(m_rareInheritedData->textUnderlinePosition); }
inline TextZoom RenderStyle::textZoom() const { return static_cast<TextZoom>(m_rareInheritedData->textZoom); }
inline const Style::InsetEdge& RenderStyle::top() const { return m_nonInheritedData->surroundData->inset.top(); }
inline OptionSet<TouchAction> RenderStyle::touchActions() const { return m_nonInheritedData->rareData->touchActions; }
inline const TransformOperations& RenderStyle::transform() const { return m_nonInheritedData->miscData->transform->operations; }
inline TransformBox RenderStyle::transformBox() const { return m_nonInheritedData->miscData->transform->transformBox; }
inline const Style::TransformOrigin& RenderStyle::transformOrigin() const { return m_nonInheritedData->miscData->transform->origin; }
inline const Style::TransformOriginX& RenderStyle::transformOriginX() const { return transformOrigin().x; }
inline const Style::TransformOriginY& RenderStyle::transformOriginY() const { return transformOrigin().y; }
inline const Style::TransformOriginZ& RenderStyle::transformOriginZ() const { return transformOrigin().z; }
inline TransformStyle3D RenderStyle::transformStyle3D() const { return static_cast<TransformStyle3D>(m_nonInheritedData->rareData->transformStyle3D); }
inline const AnimationList* RenderStyle::transitions() const { return m_nonInheritedData->miscData->transitions.get(); }
inline AnimationList* RenderStyle::transitions() { return m_nonInheritedData->miscData->transitions.get(); }
inline const Style::Translate& RenderStyle::translate() const { return m_nonInheritedData->rareData->translate; }
inline Style::ScrollBehavior RenderStyle::scrollBehavior() const { return static_cast<Style::ScrollBehavior>(m_nonInheritedData->rareData->scrollBehavior); }
inline float RenderStyle::usedPerspective() const { return perspective().usedPerspective(); }
inline TransformStyle3D RenderStyle::usedTransformStyle3D() const { return static_cast<bool>(m_nonInheritedData->rareData->transformStyleForcedToFlat) ? TransformStyle3D::Flat : transformStyle3D(); }
inline Style::ZIndex RenderStyle::usedZIndex() const { return m_nonInheritedData->boxData->usedZIndex(); }
inline UserDrag RenderStyle::userDrag() const { return static_cast<UserDrag>(m_nonInheritedData->miscData->userDrag); }
inline UserModify RenderStyle::userModify() const { return static_cast<UserModify>(m_rareInheritedData->userModify); }
inline UserSelect RenderStyle::userSelect() const { return static_cast<UserSelect>(m_rareInheritedData->userSelect); }
inline const Style::VerticalAlign& RenderStyle::verticalAlign() const { return m_nonInheritedData->boxData->verticalAlign(); }
inline const Style::ViewTransitionClasses& RenderStyle::viewTransitionClasses() const { return m_nonInheritedData->rareData->viewTransitionClasses; }
inline const Style::ViewTransitionName& RenderStyle::viewTransitionName() const { return m_nonInheritedData->rareData->viewTransitionName; }
inline const Style::Color& RenderStyle::visitedLinkBackgroundColor() const { return m_nonInheritedData->miscData->visitedLinkColor->background; }
inline const Style::Color& RenderStyle::visitedLinkBorderBottomColor() const { return m_nonInheritedData->miscData->visitedLinkColor->borderBottom; }
inline const Style::Color& RenderStyle::visitedLinkBorderLeftColor() const { return m_nonInheritedData->miscData->visitedLinkColor->borderLeft; }
inline const Style::Color& RenderStyle::visitedLinkBorderRightColor() const { return m_nonInheritedData->miscData->visitedLinkColor->borderRight; }
inline const Style::Color& RenderStyle::visitedLinkBorderTopColor() const { return m_nonInheritedData->miscData->visitedLinkColor->borderTop; }
inline const Style::Color& RenderStyle::visitedLinkCaretColor() const { return m_rareInheritedData->visitedLinkCaretColor; }
inline const Style::Color& RenderStyle::visitedLinkColumnRuleColor() const { return m_nonInheritedData->miscData->multiCol->visitedLinkColumnRuleColor; }
inline const Style::Color& RenderStyle::visitedLinkOutlineColor() const { return m_nonInheritedData->miscData->visitedLinkColor->outline; }
inline const Style::Color& RenderStyle::visitedLinkStrokeColor() const { return m_rareInheritedData->visitedLinkStrokeColor; }
inline const Style::Color& RenderStyle::visitedLinkTextDecorationColor() const { return m_nonInheritedData->miscData->visitedLinkColor->textDecoration; }
inline const Style::Color& RenderStyle::visitedLinkTextEmphasisColor() const { return m_rareInheritedData->visitedLinkTextEmphasisColor; }
inline const Style::Color& RenderStyle::visitedLinkTextFillColor() const { return m_rareInheritedData->visitedLinkTextFillColor; }
inline const Style::Color& RenderStyle::visitedLinkTextStrokeColor() const { return m_rareInheritedData->visitedLinkTextStrokeColor; }
inline Style::Widows RenderStyle::widows() const { return m_rareInheritedData->widows; }
inline const Style::PreferredSize& RenderStyle::width() const { return m_nonInheritedData->boxData->width(); }
inline WillChangeData* RenderStyle::willChange() const { return m_nonInheritedData->rareData->willChange.get(); }
inline bool RenderStyle::willChangeCreatesStackingContext() const { return willChange() && willChange()->canCreateStackingContext(); }
inline WordBreak RenderStyle::wordBreak() const { return static_cast<WordBreak>(m_rareInheritedData->wordBreak); }
inline float RenderStyle::wordSpacing() const { return m_inheritedData->fontData->fontCascade.wordSpacing(); }
constexpr LengthType RenderStyle::zeroLength() { return LengthType::Fixed; }
inline float RenderStyle::zoom() const { return m_nonInheritedData->rareData->zoom; }

inline bool RenderStyle::nativeAppearanceDisabled() const { return m_nonInheritedData->rareData->nativeAppearanceDisabled; }

inline const Style::CornerShapeValue& RenderStyle::cornerBottomLeftShape() const { return border().bottomLeftCornerShape(); }
inline const Style::CornerShapeValue& RenderStyle::cornerBottomRightShape() const { return border().bottomRightCornerShape(); }
inline const Style::CornerShapeValue& RenderStyle::cornerTopLeftShape() const { return border().topLeftCornerShape(); }
inline const Style::CornerShapeValue& RenderStyle::cornerTopRightShape() const { return border().topRightCornerShape(); }

// ignore non-standard ::-webkit-scrollbar when standard properties are in use
inline bool RenderStyle::usesStandardScrollbarStyle() const { return scrollbarWidth() != ScrollbarWidth::Auto || !scrollbarColor().isAuto(); }
inline bool RenderStyle::usesLegacyScrollbarStyle() const { return hasPseudoStyle(PseudoId::WebKitScrollbar) && !usesStandardScrollbarStyle(); }

#if ENABLE(APPLE_PAY)
inline ApplePayButtonStyle RenderStyle::applePayButtonStyle() const { return static_cast<ApplePayButtonStyle>(m_nonInheritedData->rareData->applePayButtonStyle); }
inline ApplePayButtonType RenderStyle::applePayButtonType() const { return static_cast<ApplePayButtonType>(m_nonInheritedData->rareData->applePayButtonType); }
constexpr ApplePayButtonStyle RenderStyle::initialApplePayButtonStyle() { return ApplePayButtonStyle::Black; }
constexpr ApplePayButtonType RenderStyle::initialApplePayButtonType() { return ApplePayButtonType::Plain; }
#endif

inline BoxDecorationBreak RenderStyle::boxDecorationBreak() const { return m_nonInheritedData->boxData->boxDecorationBreak(); }

inline BlendMode RenderStyle::blendMode() const { return static_cast<BlendMode>(m_nonInheritedData->rareData->effectiveBlendMode); }
constexpr BlendMode RenderStyle::initialBlendMode() { return BlendMode::Normal; }
constexpr Isolation RenderStyle::initialIsolation() { return Isolation::Auto; }
inline bool RenderStyle::isInSubtreeWithBlendMode() const { return m_rareInheritedData->isInSubtreeWithBlendMode; }
inline bool RenderStyle::isForceHidden() const { return m_rareInheritedData->isForceHidden; }
inline Isolation RenderStyle::isolation() const { return static_cast<Isolation>(m_nonInheritedData->rareData->isolation); }
inline bool RenderStyle::usesAnchorFunctions() const { return m_nonInheritedData->rareData->usesAnchorFunctions; }
inline OptionSet<BoxAxisFlag> RenderStyle::anchorFunctionScrollCompensatedAxes() const { return OptionSet<BoxAxisFlag>::fromRaw(m_nonInheritedData->rareData->anchorFunctionScrollCompensatedAxes); }

inline bool RenderStyle::isPopoverInvoker() const { return m_nonInheritedData->rareData->isPopoverInvoker; }

inline Visibility RenderStyle::usedVisibility() const
{
    if (isForceHidden()) [[unlikely]]
        return Visibility::Hidden;
    return static_cast<Visibility>(m_inheritedFlags.visibility);
}

inline bool RenderStyle::autoRevealsWhenFound() const { return m_rareInheritedData->autoRevealsWhenFound; }

#if ENABLE(CURSOR_VISIBILITY)
constexpr CursorVisibility RenderStyle::initialCursorVisibility() { return CursorVisibility::Auto; }
#endif

#if ENABLE(DARK_MODE_CSS)
inline Style::ColorScheme RenderStyle::colorScheme() const { return m_rareInheritedData->colorScheme; }
inline Style::ColorScheme RenderStyle::initialColorScheme() { return Style::ColorScheme { .schemes = { }, .only = { } }; }
inline bool RenderStyle::hasExplicitlySetColorScheme() const { return m_nonInheritedData->miscData->hasExplicitlySetColorScheme; }
#endif

inline const FilterOperations& RenderStyle::backdropFilter() const { return m_nonInheritedData->rareData->backdropFilter->operations; }
inline bool RenderStyle::hasBackdropFilter() const { return !backdropFilter().isEmpty(); }
inline FilterOperations RenderStyle::initialBackdropFilter() { return { }; }

inline bool RenderStyle::hasExplicitlySetDirection() const { return m_nonInheritedData->miscData->hasExplicitlySetDirection; }
inline bool RenderStyle::hasExplicitlySetWritingMode() const { return m_nonInheritedData->miscData->hasExplicitlySetWritingMode; }

inline const Style::DynamicRangeLimit& RenderStyle::dynamicRangeLimit() const { return m_rareInheritedData->dynamicRangeLimit; }
inline Style::DynamicRangeLimit RenderStyle::initialDynamicRangeLimit() { return CSS::Keyword::NoLimit { }; }

#if ENABLE(WEBKIT_OVERFLOW_SCROLLING_CSS_PROPERTY)
inline Style::WebkitOverflowScrolling RenderStyle::overflowScrolling() const { return static_cast<Style::WebkitOverflowScrolling>(m_rareInheritedData->webkitOverflowScrolling); }
constexpr Style::WebkitOverflowScrolling RenderStyle::initialOverflowScrolling() { return Style::WebkitOverflowScrolling::Auto; }
#endif

#if ENABLE(WEBKIT_TOUCH_CALLOUT_CSS_PROPERTY)
inline Style::WebkitTouchCallout RenderStyle::touchCallout() const { return static_cast<Style::WebkitTouchCallout>(m_rareInheritedData->webkitTouchCallout); }
constexpr Style::WebkitTouchCallout RenderStyle::initialTouchCallout() { return Style::WebkitTouchCallout::Default; }
#endif

#if ENABLE(TEXT_AUTOSIZING)
inline Length RenderStyle::initialSpecifiedLineHeight() { return LengthType::Normal; }
constexpr Style::TextSizeAdjust RenderStyle::initialTextSizeAdjust() { return CSS::Keyword::Auto { }; }
inline Style::TextSizeAdjust RenderStyle::textSizeAdjust() const { return m_rareInheritedData->textSizeAdjust; }
#endif

#if ENABLE(TOUCH_EVENTS)
inline Style::Color RenderStyle::tapHighlightColor() const { return m_rareInheritedData->tapHighlightColor; }
#endif

inline bool RenderStyle::insideDefaultButton() const { return m_rareInheritedData->insideDefaultButton; }

inline bool RenderStyle::insideSubmitButton() const { return m_rareInheritedData->insideSubmitButton; }

inline bool RenderStyle::NonInheritedFlags::hasPseudoStyle(PseudoId pseudo) const
{
    ASSERT(pseudo > PseudoId::None);
    ASSERT(pseudo < PseudoId::FirstInternalPseudoId);
    return pseudoBits & (1 << (static_cast<unsigned>(pseudo) - 1 /* PseudoId::None */));
}

inline bool RenderStyle::NonInheritedFlags::hasAnyPublicPseudoStyles() const
{
    return PublicPseudoIdMask & pseudoBits;
}

inline bool RenderStyle::breakOnlyAfterWhiteSpace() const
{
    return whiteSpaceCollapse() == WhiteSpaceCollapse::Preserve || whiteSpaceCollapse() == WhiteSpaceCollapse::PreserveBreaks || whiteSpaceCollapse() == WhiteSpaceCollapse::BreakSpaces || lineBreak() == LineBreak::AfterWhiteSpace;
}

inline bool RenderStyle::breakWords() const
{
    return wordBreak() == WordBreak::BreakWord || overflowWrap() == OverflowWrap::BreakWord || overflowWrap() == OverflowWrap::Anywhere;
}

constexpr bool RenderStyle::collapseWhiteSpace(WhiteSpaceCollapse mode)
{
    return mode == WhiteSpaceCollapse::Collapse || mode == WhiteSpaceCollapse::PreserveBreaks;
}

inline bool RenderStyle::hasInlineColumnAxis() const
{
    auto axis = columnAxis();
    return axis == ColumnAxis::Auto || writingMode().isHorizontal() == (axis == ColumnAxis::Horizontal);
}

inline Length RenderStyle::initialLineHeight()
{
    return LengthType::Normal;
}

inline bool RenderStyle::isCollapsibleWhiteSpace(char16_t character) const
{
    switch (character) {
    case ' ':
    case '\t':
        return collapseWhiteSpace();
    case '\n':
        return !preserveNewline();
    default:
        return false;
    }
}

constexpr bool RenderStyle::isDisplayBlockType(DisplayType display)
{
    return display == DisplayType::Block
        || display == DisplayType::Box
        || display == DisplayType::Flex
        || display == DisplayType::FlowRoot
        || display == DisplayType::Grid
        || display == DisplayType::ListItem
        || display == DisplayType::Table
        || display == DisplayType::RubyBlock;
}

constexpr bool RenderStyle::isDisplayInlineType(DisplayType display)
{
    return display == DisplayType::Inline
        || display == DisplayType::InlineBlock
        || display == DisplayType::InlineBox
        || display == DisplayType::InlineFlex
        || display == DisplayType::InlineGrid
        || display == DisplayType::InlineTable
        || display == DisplayType::Ruby
        || display == DisplayType::RubyBase
        || display == DisplayType::RubyAnnotation;
}

constexpr bool RenderStyle::isDisplayRegionType() const
{
    return display() == DisplayType::Block
        || display() == DisplayType::InlineBlock
        || display() == DisplayType::TableCell
        || display() == DisplayType::TableCaption
        || display() == DisplayType::ListItem;
}

constexpr bool RenderStyle::isDisplayTableOrTablePart(DisplayType display)
{
    return display == DisplayType::Table
        || display == DisplayType::InlineTable
        || display == DisplayType::TableCell
        || display == DisplayType::TableCaption
        || display == DisplayType::TableRowGroup
        || display == DisplayType::TableHeaderGroup
        || display == DisplayType::TableFooterGroup
        || display == DisplayType::TableRow
        || display == DisplayType::TableColumnGroup
        || display == DisplayType::TableColumn;
}

constexpr bool RenderStyle::isInternalTableBox(DisplayType display)
{
    // https://drafts.csswg.org/css-display-3/#layout-specific-display
    return display == DisplayType::TableCell
        || display == DisplayType::TableRowGroup
        || display == DisplayType::TableHeaderGroup
        || display == DisplayType::TableFooterGroup
        || display == DisplayType::TableRow
        || display == DisplayType::TableColumnGroup
        || display == DisplayType::TableColumn;
}

constexpr bool RenderStyle::isRubyContainerOrInternalRubyBox(DisplayType display)
{
    return display == DisplayType::Ruby
        || display == DisplayType::RubyAnnotation
        || display == DisplayType::RubyBase;
}

constexpr bool RenderStyle::doesDisplayGenerateBlockContainer() const
{
    auto display = this->display();
    return (display == DisplayType::Block
        || display == DisplayType::InlineBlock
        || display == DisplayType::FlowRoot
        || display == DisplayType::ListItem
        || display == DisplayType::TableCell
        || display == DisplayType::TableCaption);
}

inline double RenderStyle::logicalAspectRatio() const
{
    auto ratio = this->aspectRatio().tryRatio();
    ASSERT(ratio);

    if (writingMode().isHorizontal())
        return ratio->numerator.value / ratio->denominator.value;
    return ratio->denominator.value / ratio->numerator.value;
}

constexpr bool RenderStyle::preserveNewline(WhiteSpaceCollapse mode)
{
    return mode == WhiteSpaceCollapse::Preserve || mode == WhiteSpaceCollapse::PreserveBreaks || mode == WhiteSpaceCollapse::BreakSpaces;
}

inline float adjustFloatForAbsoluteZoom(float value, const RenderStyle& style)
{
    return value / style.usedZoom();
}

inline int adjustForAbsoluteZoom(int value, const RenderStyle& style)
{
    double zoomFactor = style.usedZoom();
    if (zoomFactor == 1)
        return value;
    // Needed because resolveAsLength<int> truncates (rather than rounds) when scaling up.
    if (zoomFactor > 1) {
        if (value < 0)
            value--;
        else
            value++;
    }

    return roundForImpreciseConversion<int>(value / zoomFactor);
}

inline LayoutSize adjustLayoutSizeForAbsoluteZoom(LayoutSize size, const RenderStyle& style)
{
    auto zoom = style.usedZoom();
    return { size.width() / zoom, size.height() / zoom };
}

inline LayoutUnit adjustLayoutUnitForAbsoluteZoom(LayoutUnit value, const RenderStyle& style)
{
    return LayoutUnit(value / style.usedZoom());
}

inline float applyZoom(float value, const RenderStyle& style)
{
    return value * style.usedZoom();
}

constexpr BorderStyle collapsedBorderStyle(BorderStyle style)
{
    if (style == BorderStyle::Outset)
        return BorderStyle::Groove;
    if (style == BorderStyle::Inset)
        return BorderStyle::Ridge;
    return style;
}

inline bool RenderStyle::isInterCharacterRubyPosition() const
{
    auto rubyPosition = this->rubyPosition();
    return rubyPosition == RubyPosition::InterCharacter || rubyPosition == RubyPosition::LegacyInterCharacter;
}

inline bool RenderStyle::columnSpanEqual(const RenderStyle& other) const
{
    if (m_nonInheritedData.ptr() == other.m_nonInheritedData.ptr()
        || m_nonInheritedData->miscData.ptr() == other.m_nonInheritedData->miscData.ptr()
        || m_nonInheritedData->miscData->multiCol.ptr() == other.m_nonInheritedData->miscData->multiCol.ptr())
        return true;

    return m_nonInheritedData->miscData->multiCol->columnSpan == other.m_nonInheritedData->miscData->multiCol->columnSpan;
}

inline bool RenderStyle::borderIsEquivalentForPainting(const RenderStyle& other) const
{
    bool colorDiffers = color() != other.color();

    if (!colorDiffers
        && (m_nonInheritedData.ptr() == other.m_nonInheritedData.ptr()
        || m_nonInheritedData->surroundData.ptr() == other.m_nonInheritedData->surroundData.ptr()
        || m_nonInheritedData->surroundData->border == other.m_nonInheritedData->surroundData->border))
        return true;

    return border().isEquivalentForPainting(other.border(), colorDiffers);
}

inline bool RenderStyle::containerTypeAndNamesEqual(const RenderStyle& other) const
{
    if (m_nonInheritedData.ptr() == other.m_nonInheritedData.ptr()
        || m_nonInheritedData->rareData.ptr() == other.m_nonInheritedData->rareData.ptr())
        return true;

    return containerType() == other.containerType() && containerNames() == other.containerNames();
}

inline bool RenderStyle::scrollPaddingEqual(const RenderStyle& other) const
{
    if (m_nonInheritedData.ptr() == other.m_nonInheritedData.ptr()
        || m_nonInheritedData->rareData.ptr() == other.m_nonInheritedData->rareData.ptr())
        return true;

    return m_nonInheritedData->rareData->scrollPadding == other.m_nonInheritedData->rareData->scrollPadding;
}

inline bool generatesBox(const RenderStyle& style)
{
    return style.display() != DisplayType::None && style.display() != DisplayType::Contents;
}

inline bool isNonVisibleOverflow(Overflow overflow)
{
    return overflow == Overflow::Hidden || overflow == Overflow::Scroll || overflow == Overflow::Clip;
}

inline bool pseudoElementRendererIsNeeded(const RenderStyle* style)
{
    return style && style->display() != DisplayType::None && style->content().isData();
}

inline bool isVisibleToHitTesting(const RenderStyle& style, const HitTestRequest& request)
{
    return (request.userTriggered() ? style.usedVisibility() : style.visibility()) == Visibility::Visible;
}

inline bool shouldApplyLayoutContainment(const RenderStyle& style, const Element& element)
{
    // content-visibility hidden and auto turns on layout containment.
    auto hasContainment = style.containsLayout() || style.contentVisibility() == ContentVisibility::Hidden || style.contentVisibility() == ContentVisibility::Auto;
    if (!hasContainment)
        return false;
    // Giving an element layout containment has no effect if any of the following are true:
    //   if the element does not generate a principal box (as is the case with display: contents or display: none)
    //   if its principal box is an internal table box other than table-cell
    //   if its principal box is an internal ruby box or a non-atomic inline-level box
    if (style.display() == DisplayType::None || style.display() == DisplayType::Contents)
        return false;
    if (style.isInternalTableBox() && style.display() != DisplayType::TableCell)
        return false;
    if (style.isRubyContainerOrInternalRubyBox() || (style.display() == DisplayType::Inline && !element.isReplaced(style)))
        return false;
    return true;
}

inline bool shouldApplySizeContainment(const RenderStyle& style, const Element& element)
{
    auto hasContainment = style.containsSize() || style.contentVisibility() == ContentVisibility::Hidden || (style.contentVisibility() == ContentVisibility::Auto && !element.isRelevantToUser());
    if (!hasContainment)
        return false;
    // Giving an element size containment has no effect if any of the following are true:
    //   if the element does not generate a principal box (as is the case with display: contents or display: none)
    //   if its inner display type is table
    //   if its principal box is an internal table box
    //   if its principal box is an internal ruby box or a non-atomic inline-level box
    if (style.display() == DisplayType::None || style.display() == DisplayType::Contents)
        return false;
    if (style.display() == DisplayType::Table || style.display() == DisplayType::InlineTable)
        return false;
    if (style.isInternalTableBox())
        return false;
    if (style.isRubyContainerOrInternalRubyBox() || (style.display() == DisplayType::Inline && !element.isReplaced(style)))
        return false;
    return true;
}

inline bool shouldApplyInlineSizeContainment(const RenderStyle& style, const Element& element)
{
    if (!style.containsInlineSize())
        return false;
    // Giving an element inline-size containment has no effect if any of the following are true:
    //   if the element does not generate a principal box (as is the case with display: contents or display: none)
    //   if its inner display type is table
    //   if its principal box is an internal table box
    //   if its principal box is an internal ruby box or a non-atomic inline-level box
    if (style.display() == DisplayType::None || style.display() == DisplayType::Contents)
        return false;
    if (style.display() == DisplayType::Table || style.display() == DisplayType::InlineTable)
        return false;
    if (style.isInternalTableBox())
        return false;
    if (style.isRubyContainerOrInternalRubyBox() || (style.display() == DisplayType::Inline && !element.isReplaced(style)))
        return false;
    return true;
}

inline bool shouldApplyStyleContainment(const RenderStyle& style, const Element&)
{
    // content-visibility hidden and auto turns on style containment.
    return style.containsStyle() || style.contentVisibility() == ContentVisibility::Hidden || style.contentVisibility() == ContentVisibility::Auto;
}

inline bool shouldApplyPaintContainment(const RenderStyle& style, const Element& element)
{
    // content-visibility hidden and auto turns on paint containment.
    auto hasContainment = style.containsPaint() || style.contentVisibility() == ContentVisibility::Hidden || style.contentVisibility() == ContentVisibility::Auto;
    if (!hasContainment)
        return false;
    // Giving an element paint containment has no effect if any of the following are true:
    //   if the element does not generate a principal box (as is the case with display: contents or display: none)
    //   if its principal box is an internal table box other than table-cell
    //   if its principal box is an internal ruby box or a non-atomic inline-level box
    if (style.display() == DisplayType::None || style.display() == DisplayType::Contents)
        return false;
    if (style.isInternalTableBox() && style.display() != DisplayType::TableCell)
        return false;
    if (style.isRubyContainerOrInternalRubyBox() || (style.display() == DisplayType::Inline && !element.isReplaced(style)))
        return false;
    return true;
}

inline bool isSkippedContentRoot(const RenderStyle& style, const Element& element)
{
    if (!shouldApplySizeContainment(style, element))
        return false;

    switch (style.contentVisibility()) {
    case ContentVisibility::Visible:
        return false;
    case ContentVisibility::Hidden:
        return true;
    case ContentVisibility::Auto:
        return !element.isRelevantToUser();
    default:
        ASSERT_NOT_REACHED();
        return false;
    }
}

inline bool RenderStyle::fontCascadeEqual(const RenderStyle& other) const
{
    return m_inheritedData.ptr() == other.m_inheritedData.ptr()
        || m_inheritedData->fontData.ptr() == other.m_inheritedData->fontData.ptr()
        || m_inheritedData->fontData->fontCascade == other.m_inheritedData->fontData->fontCascade;
}

} // namespace WebCore
