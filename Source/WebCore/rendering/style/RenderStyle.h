/*
 * Copyright (C) 2000 Lars Knoll (knoll@kde.org)
 *           (C) 2000 Antti Koivisto (koivisto@kde.org)
 *           (C) 2000 Dirk Mueller (mueller@kde.org)
 * Copyright (C) 2003-2023 Apple Inc. All rights reserved.
 * Copyright (C) 2014-2021 Google Inc. All rights reserved.
 * Copyright (C) 2006 Graham Dennis (graham.dennis@gmail.com)
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

#pragma once

#include <WebCore/BoxExtents.h>
#include <WebCore/PseudoElementIdentifier.h>
#include <WebCore/StylePrimitiveNumeric+Forward.h>
#include <WebCore/WritingMode.h>
#include <unicode/utypes.h>
#include <wtf/CheckedRef.h>
#include <wtf/DataRef.h>
#include <wtf/FixedVector.h>
#include <wtf/OptionSet.h>
#include <wtf/Vector.h>

namespace WTF {
class TextStream;
}

namespace WebCore {

class AnimationList;
class AutosizeStatus;
class BorderData;
class BorderValue;
struct CSSPropertiesBitSet;
class Color;
class CursorList;
class Element;
class FillLayer;
class FilterOperations;
class FloatPoint;
class FloatSize;
class FloatPoint3D;
class FloatRect;
class FontCascade;
class FontCascadeDescription;
class FontMetrics;
class FontSelectionValue;
class HitTestRequest;
class IntPoint;
class IntSize;
class LayoutRect;
class LayoutRoundedRect;
class LayoutSize;
class LayoutUnit;
class LengthBox;
class LineClampValue;
class NinePieceImage;
class OutlineValue;
class PathOperation;
class PositionArea;
class PseudoIdSet;
class RenderElement;
class RenderStyle;
class RotateTransformOperation;
class SVGLengthValue;
class SVGRenderStyle;
class ScaleTransformOperation;
class ScrollTimeline;
class ShapeValue;
class StyleContentAlignmentData;
class StyleImage;
class StyleInheritedData;
class StyleNonInheritedData;
class StylePathData;
class StyleRareInheritedData;
class StyleReflection;
class StyleScrollSnapArea;
class StyleSelfAlignmentData;
class TransformOperations;
class TransformationMatrix;
class TranslateTransformOperation;
class ViewTimeline;
class WillChangeData;

enum CSSPropertyID : uint16_t;
enum GridAutoFlow : uint8_t;
enum class PageSizeType : uint8_t;
enum class PaginationMode : uint8_t;

enum class ApplePayButtonStyle : uint8_t;
enum class ApplePayButtonType : uint8_t;
enum class AppleVisualEffect : uint8_t;
enum class BackfaceVisibility : uint8_t;
enum class BlendMode : uint8_t;
enum class FlowDirection : uint8_t;
enum class BlockStepAlign : uint8_t;
enum class BlockStepInsert : uint8_t;
enum class BlockStepRound : uint8_t;
enum class BorderCollapse : bool;
enum class BorderStyle : uint8_t;
enum class BoxAlignment : uint8_t;
enum class BoxAxisFlag : uint8_t;
enum class BoxDecorationBreak : bool;
enum class BoxDirection : bool;
enum class BoxLines : bool;
enum class BoxOrient : bool;
enum class BoxPack : uint8_t;
enum class BoxSizing : bool;
enum class BreakBetween : uint8_t;
enum class BreakInside : uint8_t;
enum class CaptionSide : uint8_t;
enum class Clear : uint8_t;
enum class ColumnAxis : uint8_t;
enum class ColumnFill : bool;
enum class ColumnProgression : bool;
enum class ColumnSpan : bool;
enum class CompositeOperator : uint8_t;
enum class ContainerType : uint8_t;
enum class Containment : uint8_t;
enum class ContentDistribution : uint8_t;
enum class ContentPosition : uint8_t;
enum class ContentVisibility : uint8_t;
enum class CursorType : uint8_t;
enum class CursorVisibility : bool;
enum class DisplayType : uint8_t;
enum class EmptyCell : bool;
enum class EventListenerRegionType : uint32_t;
enum class FieldSizing : bool;
enum class FillAttachment : uint8_t;
enum class FillBox : uint8_t;
enum class FillSizeType : uint8_t;
enum class FlexDirection : uint8_t;
enum class FlexWrap : uint8_t;
enum class Float : uint8_t;
enum class FontOrientation : bool;
enum class FontOpticalSizing : bool;
enum class HangingPunctuation : uint8_t;
enum class Hyphens : uint8_t;
enum class ImageRendering : uint8_t;
enum class InputSecurity : bool;
enum class InsideLink : uint8_t;
enum class Isolation : bool;
enum class ItemPosition : uint8_t;
enum class LengthType : uint8_t;
enum class LineAlign : bool;
enum class LineBreak : uint8_t;
enum class LineCap : uint8_t;
enum class LineJoin : uint8_t;
enum class LineSnap : uint8_t;
enum class ListStylePosition : bool;
enum class MarginTrimType : uint8_t;
enum class MarqueeBehavior : uint8_t;
enum class MarqueeDirection : uint8_t;
enum class MathShift : bool;
enum class MathStyle : bool;
enum class NBSPMode : bool;
enum class NinePieceImageRule : uint8_t;
enum class NonCJKGlyphOrientation : bool;
enum class ObjectFit : uint8_t;
enum class Order : bool;
enum class OutlineStyle : uint8_t;
enum class Overflow : uint8_t;
enum class OverflowAnchor : bool;
enum class OverflowContinue : bool;
enum class OverflowWrap : uint8_t;
enum class OverscrollBehavior : uint8_t;
enum class PaintBehavior : uint32_t;
enum class PaintOrder : uint8_t;
enum class PaintType : uint8_t;
enum class PointerEvents : uint8_t;
enum class PositionType : uint8_t;
enum class PositionVisibility : uint8_t;
enum class PrintColorAdjust : bool;
enum class PseudoId : uint32_t;
enum class Resize : uint8_t;
enum class RubyPosition : uint8_t;
enum class RubyAlign : uint8_t;
enum class RubyOverhang : bool;
enum class ScrollAxis : uint8_t;
enum class ScrollSnapStop : bool;
enum class ScrollbarWidth : uint8_t;
enum class SpeakAs : uint8_t;
enum class StyleAppearance : uint8_t;
enum class StyleColorOptions : uint8_t;
enum class StyleDifference : uint8_t;
enum class StyleDifferenceContextSensitiveProperty : uint8_t;
enum class TableLayoutType : bool;
enum class TextAlignLast : uint8_t;
enum class TextAlignMode : uint8_t;
enum class TextBoxTrim : uint8_t;
enum class TextCombine : bool;
enum class TextDecorationLine : uint8_t;
enum class TextDecorationSkipInk : uint8_t;
enum class TextDecorationStyle : uint8_t;
enum class TextEmphasisPosition : uint8_t;
enum class TextGroupAlign : uint8_t;
enum class TextJustify : uint8_t;
enum class TextOverflow : bool;
enum class TextSecurity : uint8_t;
enum class TextTransform : uint8_t;
enum class TextUnderlinePosition : uint8_t;
enum class TextWrapMode : bool;
enum class TextWrapStyle : uint8_t;
enum class TextZoom : bool;
enum class TouchAction : uint8_t;
enum class TransformBox : uint8_t;
enum class TransformStyle3D : uint8_t;
enum class UnicodeBidi : uint8_t;
enum class UsedClear : uint8_t;
enum class UsedFloat : uint8_t;
enum class UserDrag : uint8_t;
enum class UserModify : uint8_t;
enum class UserSelect : uint8_t;
enum class Visibility : uint8_t;
enum class WhiteSpace : uint8_t;
enum class WhiteSpaceCollapse : uint8_t;
enum class WordBreak : uint8_t;

struct CounterDirectiveMap;
struct FillRepeatXY;
struct FontPalette;
struct FontSizeAdjust;
struct GridTrackList;
struct ImageOrientation;
struct Length;
struct LengthPoint;
struct LengthSize;
struct SingleTimelineRange;

struct ScrollSnapAlign;
struct ScrollSnapType;
struct NameScope;

struct TabSize;
class TextAutospace;
struct TextEdge;
class TextSpacingTrim;
struct TransformOperationData;

template<typename> class FontTaggedSettings;
template<typename> class RectEdges;
template<typename> class RectCorners;
template<typename> struct MinimallySerializingSpaceSeparatedRectEdges;
template<typename> struct MinimallySerializingSpaceSeparatedSize;

using FontVariationSettings = FontTaggedSettings<float>;
using IntOutsets = RectEdges<int>;

namespace Style {
class CustomProperty;
class CustomPropertyData;
class CustomPropertyRegistry;
struct AnchorNames;
struct AspectRatio;
struct BlockEllipsis;
struct BlockStepSize;
struct BorderImage;
struct BorderImageOutset;
struct BorderImageRepeat;
struct BorderImageSlice;
struct BorderImageSource;
struct BorderImageWidth;
struct BorderRadius;
struct BoxShadow;
struct Clip;
struct ClipPath;
struct Color;
struct ColorScheme;
struct ColumnCount;
struct ColumnWidth;
struct ContainIntrinsicSize;
struct ContainerNames;
struct Content;
struct CornerShapeValue;
struct Cursor;
struct DynamicRangeLimit;
struct FlexBasis;
struct GapGutter;
struct GridPosition;
struct GridTemplateAreas;
struct GridTemplateList;
struct GridTrackSizes;
struct HyphenateCharacter;
struct HyphenateLimitEdge;
struct HyphenateLimitLines;
struct InsetEdge;
struct LineWidth;
struct ListStyleType;
struct MarginEdge;
struct MaskBorder;
struct MaskBorderOutset;
struct MaskBorderRepeat;
struct MaskBorderSlice;
struct MaskBorderSource;
struct MaskBorderWidth;
struct MaximumLines;
struct MaximumSize;
struct MinimumSize;
struct OffsetAnchor;
struct OffsetDistance;
struct OffsetPath;
struct OffsetPosition;
struct OffsetRotate;
struct Opacity;
struct Orphans;
struct PaddingEdge;
struct Perspective;
struct Position;
struct PositionX;
struct PositionY;
struct PositionTryFallback;
struct PreferredSize;
struct ProgressTimelineAxes;
struct ProgressTimelineNames;
struct Quotes;
struct Rotate;
struct SVGBaselineShift;
struct SVGCenterCoordinateComponent;
struct SVGCoordinateComponent;
struct SVGPaint;
struct SVGRadius;
struct SVGRadiusComponent;
struct SVGStrokeDasharray;
struct SVGStrokeDashoffset;
struct Scale;
struct ScopedName;
struct ScrollMarginEdge;
struct ScrollPaddingEdge;
struct ScrollTimelines;
struct ScrollbarColor;
struct ScrollbarGutter;
struct ShapeMargin;
struct ShapeOutside;
struct StrokeMiterlimit;
struct StrokeWidth;
struct TextDecorationThickness;
struct TextEmphasisStyle;
struct TextIndent;
struct TextShadow;
struct TextSizeAdjust;
struct TextUnderlineOffset;
struct TransformOrigin;
struct Translate;
struct VerticalAlign;
struct ViewTimelineInsets;
struct ViewTimelines;
struct ViewTransitionClasses;
struct ViewTransitionName;
struct WebkitLineGrid;
struct WebkitTextStrokeWidth;
struct Widows;
struct ZIndex;

enum class Change : uint8_t;
enum class GridTrackSizingDirection : bool;
enum class LineBoxContain : uint8_t;
enum class PositionTryOrder : uint8_t;
enum class ScrollBehavior : bool;
enum class WebkitOverflowScrolling : bool;
enum class WebkitTouchCallout : bool;

template<typename> struct Shadows;

using BorderRadiusValue = MinimallySerializingSpaceSeparatedSize<LengthPercentage<CSS::Nonnegative>>;
using BoxShadows = Shadows<BoxShadow>;
using FlexGrow = Number<CSS::Nonnegative, float>;
using FlexShrink = Number<CSS::Nonnegative, float>;
using InsetBox = MinimallySerializingSpaceSeparatedRectEdges<InsetEdge>;
using LineWidthBox = MinimallySerializingSpaceSeparatedRectEdges<LineWidth>;
using MarginBox = MinimallySerializingSpaceSeparatedRectEdges<MarginEdge>;
using ObjectPosition = Position;
using Order = Integer<>;
using PaddingBox = MinimallySerializingSpaceSeparatedRectEdges<PaddingEdge>;
using PerspectiveOrigin = Position;
using PerspectiveOriginX = PositionX;
using PerspectiveOriginY = PositionY;
using ScrollMarginBox = MinimallySerializingSpaceSeparatedRectEdges<ScrollMarginEdge>;
using ScrollPaddingBox = MinimallySerializingSpaceSeparatedRectEdges<ScrollPaddingEdge>;
using ShapeImageThreshold = Number<CSS::ClosedUnitRangeClampBoth, float>;
using TextShadows = Shadows<TextShadow>;
using TransformOriginX = PositionX;
using TransformOriginXY = Position;
using TransformOriginY = PositionY;
using TransformOriginZ = Length<>;
using WebkitBorderSpacing = Length<CSS::Nonnegative>;
}

constexpr auto PublicPseudoIDBits = 17;
constexpr auto TextDecorationLineBits = 5;
constexpr auto TextTransformBits = 5;
constexpr auto PseudoElementTypeBits = 5;

DECLARE_ALLOCATOR_WITH_HEAP_IDENTIFIER(PseudoStyleCache);
struct PseudoStyleCache {
    WTF_DEPRECATED_MAKE_STRUCT_FAST_ALLOCATED_WITH_HEAP_IDENTIFIER(PseudoStyleCache, PseudoStyleCache);
    HashMap<Style::PseudoElementIdentifier, std::unique_ptr<RenderStyle>> styles;
};

DECLARE_ALLOCATOR_WITH_HEAP_IDENTIFIER(RenderStyle);
class RenderStyle final : public CanMakeCheckedPtr<RenderStyle> {
    WTF_DEPRECATED_MAKE_FAST_ALLOCATED_WITH_HEAP_IDENTIFIER(RenderStyle, RenderStyle);
    WTF_OVERRIDE_DELETE_FOR_CHECKED_PTR(RenderStyle);
private:
    enum CloneTag { Clone };
    enum CreateDefaultStyleTag { CreateDefaultStyle };

public:
    RenderStyle(RenderStyle&&);
    RenderStyle& operator=(RenderStyle&&);
    WEBCORE_EXPORT ~RenderStyle();

    RenderStyle replace(RenderStyle&&) WARN_UNUSED_RETURN;

    explicit RenderStyle(CreateDefaultStyleTag);
    RenderStyle(const RenderStyle&, CloneTag);

    static RenderStyle& defaultStyleSingleton();

    WEBCORE_EXPORT static RenderStyle create();
    static std::unique_ptr<RenderStyle> createPtr();
    static std::unique_ptr<RenderStyle> createPtrWithRegisteredInitialValues(const Style::CustomPropertyRegistry&);

    static RenderStyle clone(const RenderStyle&);
    static RenderStyle cloneIncludingPseudoElements(const RenderStyle&);
    static std::unique_ptr<RenderStyle> clonePtr(const RenderStyle&);

    static RenderStyle createAnonymousStyleWithDisplay(const RenderStyle& parentStyle, DisplayType);
    static RenderStyle createStyleInheritingFromPseudoStyle(const RenderStyle& pseudoStyle);

#if ASSERT_ENABLED || ENABLE(SECURITY_ASSERTIONS)
    bool deletionHasBegun() const { return m_deletionHasBegun; }
#endif

    bool operator==(const RenderStyle&) const;

    void inheritFrom(const RenderStyle&);
    void inheritIgnoringCustomPropertiesFrom(const RenderStyle&);
    void fastPathInheritFrom(const RenderStyle&);
    void copyNonInheritedFrom(const RenderStyle&);
    void copyContentFrom(const RenderStyle&);
    void copyPseudoElementsFrom(const RenderStyle&);
    void copyPseudoElementBitsFrom(const RenderStyle&);

    ContentPosition resolvedJustifyContentPosition(const StyleContentAlignmentData& normalValueBehavior) const;
    ContentDistribution resolvedJustifyContentDistribution(const StyleContentAlignmentData& normalValueBehavior) const;
    ContentPosition resolvedAlignContentPosition(const StyleContentAlignmentData& normalValueBehavior) const;
    ContentDistribution resolvedAlignContentDistribution(const StyleContentAlignmentData& normalValueBehavior) const;
    StyleSelfAlignmentData resolvedAlignItems(ItemPosition normalValueBehavior) const;
    StyleSelfAlignmentData resolvedAlignSelf(const RenderStyle* parentStyle, ItemPosition normalValueBehavior) const;
    StyleContentAlignmentData resolvedAlignContent(const StyleContentAlignmentData& normalValueBehavior) const;
    StyleSelfAlignmentData resolvedJustifyItems(ItemPosition normalValueBehavior) const;
    StyleSelfAlignmentData resolvedJustifySelf(const RenderStyle* parentStyle, ItemPosition normalValueBehavior) const;
    StyleContentAlignmentData resolvedJustifyContent(const StyleContentAlignmentData& normalValueBehavior) const;

    PseudoId pseudoElementType() const { return static_cast<PseudoId>(m_nonInheritedFlags.pseudoElementType); }
    void setPseudoElementType(PseudoId pseudoElementType) { m_nonInheritedFlags.pseudoElementType = static_cast<unsigned>(pseudoElementType); }
    const AtomString& pseudoElementNameArgument() const;
    void setPseudoElementNameArgument(const AtomString&);

    std::optional<Style::PseudoElementIdentifier> pseudoElementIdentifier() const;

    RenderStyle* getCachedPseudoStyle(const Style::PseudoElementIdentifier&) const;
    RenderStyle* addCachedPseudoStyle(std::unique_ptr<RenderStyle>);

    bool hasCachedPseudoStyles() const { return m_cachedPseudoStyles && m_cachedPseudoStyles->styles.size(); }
    const PseudoStyleCache* cachedPseudoStyles() const { return m_cachedPseudoStyles.get(); }

    inline const Style::CustomPropertyData& inheritedCustomProperties() const;
    inline const Style::CustomPropertyData& nonInheritedCustomProperties() const;
    const Style::CustomProperty* customPropertyValue(const AtomString&) const;
    bool customPropertyValueEqual(const RenderStyle&, const AtomString&) const;

    void deduplicateCustomProperties(const RenderStyle&);
    void setCustomPropertyValue(Ref<const Style::CustomProperty>&&, bool isInherited);
    bool customPropertiesEqual(const RenderStyle&) const;

    void setUsesViewportUnits() { m_nonInheritedFlags.usesViewportUnits = true; }
    bool usesViewportUnits() const { return m_nonInheritedFlags.usesViewportUnits; }
    void setUsesContainerUnits() { m_nonInheritedFlags.usesContainerUnits = true; }
    bool usesContainerUnits() const { return m_nonInheritedFlags.usesContainerUnits; }
    void setUsesTreeCountingFunctions() { m_nonInheritedFlags.useTreeCountingFunctions = true; }
    bool useTreeCountingFunctions() const { return m_nonInheritedFlags.useTreeCountingFunctions; }
    void setUsesAnchorFunctions();
    bool usesAnchorFunctions() const;
    void setAnchorFunctionScrollCompensatedAxes(OptionSet<BoxAxisFlag>);
    OptionSet<BoxAxisFlag> anchorFunctionScrollCompensatedAxes() const;

    void setIsPopoverInvoker();
    bool isPopoverInvoker() const;

    inline bool nativeAppearanceDisabled() const;
    inline void setNativeAppearanceDisabled(bool);
    static bool initialNativeAppearanceDisabled() { return false; }

    void setColumnStylesFromPaginationMode(PaginationMode);

    inline bool isFloating() const;
    inline bool hasMargin() const;
    inline bool hasBorder() const;
    inline bool hasBorderImage() const;
    inline bool hasVisibleBorderDecoration() const;
    inline bool hasVisibleBorder() const;
    inline bool hasPadding() const;
    inline bool hasInset() const;

    inline bool hasBackgroundImage() const;
    inline bool hasAnyFixedBackground() const;
    bool hasAnyBackgroundClipText() const;

    bool hasEntirelyFixedBackground() const;
    inline bool hasAnyLocalBackground() const;

    inline bool hasAppearance() const;
    inline bool hasUsedAppearance() const;

    inline bool hasBackground() const;
    
    LayoutBoxExtent imageOutsets(const Style::BorderImage&) const;
    LayoutBoxExtent imageOutsets(const Style::MaskBorder&) const;

    inline bool hasBorderImageOutsets() const;
    inline LayoutBoxExtent borderImageOutsets() const;
    inline LayoutBoxExtent maskBorderOutsets() const;

    inline IntOutsets filterOutsets() const;

    Order rtlOrdering() const { return static_cast<Order>(m_inheritedFlags.rtlOrdering); }
    void setRTLOrdering(Order ordering) { m_inheritedFlags.rtlOrdering = static_cast<unsigned>(ordering); }

    bool isStyleAvailable() const;

    inline bool hasAnyPublicPseudoStyles() const;
    inline bool hasPseudoStyle(PseudoId) const;
    inline void setHasPseudoStyles(PseudoIdSet);

    inline bool hasDisplayAffectedByAnimations() const;
    inline void setHasDisplayAffectedByAnimations();

    // attribute getter methods

    constexpr DisplayType display() const { return static_cast<DisplayType>(m_nonInheritedFlags.effectiveDisplay); }
    constexpr WritingMode writingMode() const { return m_inheritedFlags.writingMode; }
    bool isLeftToRightDirection() const { return writingMode().isBidiLTR(); } // deprecated, because of confusion between physical inline directions and bidi / line-relative directions

    inline const Style::InsetBox& insetBox() const;
    inline const Style::InsetEdge& left() const;
    inline const Style::InsetEdge& right() const;
    inline const Style::InsetEdge& top() const;
    inline const Style::InsetEdge& bottom() const;

    // Accessors for positioned object edges that take into account writing mode.
    inline const Style::InsetEdge& logicalLeft() const;
    inline const Style::InsetEdge& logicalRight() const;
    inline const Style::InsetEdge& logicalTop() const;
    inline const Style::InsetEdge& logicalBottom() const;

    // Whether or not a positioned element requires normal flow x/y to be computed to determine its position.
    inline bool hasStaticInlinePosition(bool horizontal) const;
    inline bool hasStaticBlockPosition(bool horizontal) const;

    PositionType position() const { return static_cast<PositionType>(m_nonInheritedFlags.position); }
    inline bool hasOutOfFlowPosition() const;
    inline bool hasInFlowPosition() const;
    inline bool hasViewportConstrainedPosition() const;
    Float floating() const { return static_cast<Float>(m_nonInheritedFlags.floating); }
    static UsedFloat usedFloat(const RenderElement&); // Returns logical left/right (block-relative).
    Clear clear() const { return static_cast<Clear>(m_nonInheritedFlags.clear); }
    static UsedClear usedClear(const RenderElement&); // Returns logical left/right (block-relative).

    inline const Style::PreferredSize& width() const;
    inline const Style::PreferredSize& height() const;
    inline const Style::MinimumSize& minWidth() const;
    inline const Style::MinimumSize& minHeight() const;
    inline const Style::MaximumSize& maxWidth() const;
    inline const Style::MaximumSize& maxHeight() const;

    inline const Style::PreferredSize& logicalWidth(const WritingMode) const;
    inline const Style::PreferredSize& logicalHeight(const WritingMode) const;
    inline const Style::MinimumSize& logicalMinWidth(const WritingMode) const;
    inline const Style::MinimumSize& logicalMinHeight(const WritingMode) const;
    inline const Style::MaximumSize& logicalMaxWidth(const WritingMode) const;
    inline const Style::MaximumSize& logicalMaxHeight(const WritingMode) const;
    inline const Style::PreferredSize& logicalWidth() const;
    inline const Style::PreferredSize& logicalHeight() const;
    inline const Style::MinimumSize& logicalMinWidth() const;
    inline const Style::MinimumSize& logicalMinHeight() const;
    inline const Style::MaximumSize& logicalMaxWidth() const;
    inline const Style::MaximumSize& logicalMaxHeight() const;

    inline const BorderData& border() const;
    inline const BorderValue& borderLeft() const;
    inline const BorderValue& borderRight() const;
    inline const BorderValue& borderTop() const;
    inline const BorderValue& borderBottom() const;

    const BorderValue& borderBefore(const WritingMode) const;
    const BorderValue& borderAfter(const WritingMode) const;
    const BorderValue& borderStart(const WritingMode) const;
    const BorderValue& borderEnd(const WritingMode) const;
    const BorderValue& borderBefore() const { return borderBefore(writingMode()); }
    const BorderValue& borderAfter() const { return borderAfter(writingMode()); }
    const BorderValue& borderStart() const { return borderStart(writingMode()); }
    const BorderValue& borderEnd() const { return borderEnd(writingMode()); }

    inline const Style::BorderImage& borderImage() const;
    inline const Style::BorderImageSource& borderImageSource() const;
    inline const Style::BorderImageSlice& borderImageSlice() const;
    inline const Style::BorderImageWidth& borderImageWidth() const;
    inline const Style::BorderImageOutset& borderImageOutset() const;
    inline const Style::BorderImageRepeat& borderImageRepeat() const;
    inline NinePieceImageRule borderImageHorizontalRule() const;
    inline NinePieceImageRule borderImageVerticalRule() const;
    static inline Style::BorderImage initialBorderImage();

    inline const Style::BorderRadiusValue& borderTopLeftRadius() const;
    inline const Style::BorderRadiusValue& borderTopRightRadius() const;
    inline const Style::BorderRadiusValue& borderBottomLeftRadius() const;
    inline const Style::BorderRadiusValue& borderBottomRightRadius() const;
    inline const Style::BorderRadius& borderRadii() const;
    inline bool hasBorderRadius() const;
    inline bool hasExplicitlySetBorderBottomLeftRadius() const;
    inline bool hasExplicitlySetBorderBottomRightRadius() const;
    inline bool hasExplicitlySetBorderTopLeftRadius() const;
    inline bool hasExplicitlySetBorderTopRightRadius() const;
    inline bool hasExplicitlySetBorderRadius() const;

    inline Style::LineWidth borderLeftWidth() const;
    inline BorderStyle borderLeftStyle() const;
    inline bool borderLeftIsTransparent() const;
    inline Style::LineWidth borderRightWidth() const;
    inline BorderStyle borderRightStyle() const;
    inline bool borderRightIsTransparent() const;
    inline Style::LineWidth borderTopWidth() const;
    inline BorderStyle borderTopStyle() const;
    inline bool borderTopIsTransparent() const;
    inline Style::LineWidth borderBottomWidth() const;
    inline BorderStyle borderBottomStyle() const;
    inline bool borderBottomIsTransparent() const;
    inline Style::LineWidthBox borderWidth() const;

    Style::LineWidth borderBeforeWidth(const WritingMode) const;
    Style::LineWidth borderAfterWidth(const WritingMode) const;
    Style::LineWidth borderStartWidth(const WritingMode) const;
    Style::LineWidth borderEndWidth(const WritingMode) const;
    inline Style::LineWidth borderBeforeWidth() const;
    inline Style::LineWidth borderAfterWidth() const;
    inline Style::LineWidth borderStartWidth() const;
    inline Style::LineWidth borderEndWidth() const;

    inline bool borderIsEquivalentForPainting(const RenderStyle&) const;

    inline const Style::CornerShapeValue& cornerBottomLeftShape() const;
    inline const Style::CornerShapeValue& cornerBottomRightShape() const;
    inline const Style::CornerShapeValue& cornerTopLeftShape() const;
    inline const Style::CornerShapeValue& cornerTopRightShape() const;

    void setCornerBottomLeftShape(Style::CornerShapeValue&&);
    void setCornerBottomRightShape(Style::CornerShapeValue&&);
    void setCornerTopLeftShape(Style::CornerShapeValue&&);
    void setCornerTopRightShape(Style::CornerShapeValue&&);

    inline const OutlineValue& outline() const;
    float outlineSize() const;
    Style::LineWidth outlineWidth() const;
    Style::Length<> outlineOffset() const;
    inline bool hasOutline() const;
    inline OutlineStyle outlineStyle() const;
    inline bool hasOutlineInVisualOverflow() const;
    
    Overflow overflowX() const { return static_cast<Overflow>(m_nonInheritedFlags.overflowX); }
    Overflow overflowY() const { return static_cast<Overflow>(m_nonInheritedFlags.overflowY); }
    inline bool isOverflowVisible() const;

    inline OverscrollBehavior overscrollBehaviorX() const;
    inline OverscrollBehavior overscrollBehaviorY() const;
    
    Visibility visibility() const { return static_cast<Visibility>(m_inheritedFlags.visibility); }
    inline Visibility usedVisibility() const;

    const Style::VerticalAlign& verticalAlign() const;

    inline const Style::Clip& clip() const;
    inline bool hasClip() const;

    UnicodeBidi unicodeBidi() const { return static_cast<UnicodeBidi>(m_nonInheritedFlags.unicodeBidi); }

    inline FieldSizing fieldSizing() const;

    inline const FontCascade& fontCascade() const;
    CheckedRef<const FontCascade> checkedFontCascade() const;
    WEBCORE_EXPORT const FontMetrics& metricsOfPrimaryFont() const;
    WEBCORE_EXPORT const FontCascadeDescription& fontDescription() const;

    WEBCORE_EXPORT FontCascade& mutableFontCascadeWithoutUpdate();
    WEBCORE_EXPORT FontCascadeDescription& mutableFontDescriptionWithoutUpdate();

    inline bool fontCascadeEqual(const RenderStyle&) const;

    float specifiedFontSize() const;
    float computedFontSize() const;
    std::pair<FontOrientation, NonCJKGlyphOrientation> fontAndGlyphOrientation();

    inline FontOpticalSizing fontOpticalSizing() const;
    inline FontVariationSettings fontVariationSettings() const;
    inline FontSelectionValue fontWeight() const;
    inline FontSelectionValue fontWidth() const;
    inline std::optional<FontSelectionValue> fontItalic() const;
    inline const FontPalette& fontPalette() const;
    inline FontSizeAdjust fontSizeAdjust() const;

    inline const Style::TextIndent& textIndent() const;
    inline TextAlignMode textAlign() const { return static_cast<TextAlignMode>(m_inheritedFlags.textAlign); }
    inline TextAlignLast textAlignLast() const;
    inline TextGroupAlign textGroupAlign() const;
    inline OptionSet<TextTransform> textTransform() const;
    inline OptionSet<TextDecorationLine> textDecorationLineInEffect() const;
    inline OptionSet<TextDecorationLine> textDecorationLine() const;
    inline TextDecorationStyle textDecorationStyle() const;
    inline TextDecorationSkipInk textDecorationSkipInk() const;
    inline OptionSet<TextUnderlinePosition> textUnderlinePosition() const;
    inline const Style::TextUnderlineOffset& textUnderlineOffset() const;
    inline const Style::TextDecorationThickness& textDecorationThickness() const;

    inline TextJustify textJustify() const;

    inline TextBoxTrim textBoxTrim() const;
    TextEdge textBoxEdge() const;
    TextEdge lineFitEdge() const;

    inline OptionSet<MarginTrimType> marginTrim() const;

    const Length& computedLetterSpacing() const;
    const Length& computedWordSpacing() const;
    inline float letterSpacing() const;
    inline float wordSpacing() const;
    TextSpacingTrim textSpacingTrim() const;
    TextAutospace textAutospace() const;

    inline float zoom() const;
    inline float usedZoom() const;
    
    inline TextZoom textZoom() const;

    const Length& specifiedLineHeight() const;
    WEBCORE_EXPORT const Length& lineHeight() const;
    WEBCORE_EXPORT float computedLineHeight() const;
    float computeLineHeight(const Length&) const;

    inline bool autoWrap() const;
    static constexpr bool preserveNewline(WhiteSpaceCollapse);
    inline bool preserveNewline() const;
    static constexpr bool collapseWhiteSpace(WhiteSpaceCollapse);
    inline bool collapseWhiteSpace() const;
    inline bool isCollapsibleWhiteSpace(char16_t) const;
    inline bool breakOnlyAfterWhiteSpace() const;
    inline bool breakWords() const;

    WhiteSpaceCollapse whiteSpaceCollapse() const { return static_cast<WhiteSpaceCollapse>(m_inheritedFlags.whiteSpaceCollapse); }
    TextWrapMode textWrapMode() const { return static_cast<TextWrapMode>(m_inheritedFlags.textWrapMode); }
    TextWrapStyle textWrapStyle() const { return static_cast<TextWrapStyle>(m_inheritedFlags.textWrapStyle); }

    inline FillRepeatXY backgroundRepeat() const;
    inline FillAttachment backgroundAttachment() const;
    inline FillBox backgroundClip() const;
    inline FillBox backgroundOrigin() const;
    inline FillSizeType backgroundSizeType() const;
    inline const LengthSize& backgroundSizeLength() const;
    inline FillLayer& ensureBackgroundLayers();
    inline const FillLayer& backgroundLayers() const; // Defined in RenderStyleInlines.h.
    inline Ref<const FillLayer> protectedBackgroundLayers() const; // Defined in RenderStyleInlines.h.
    inline BlendMode backgroundBlendMode() const;

    inline StyleImage* maskImage() const;
    inline FillRepeatXY maskRepeat() const;
    inline CompositeOperator maskComposite() const;
    inline FillBox maskClip() const;
    inline FillBox maskOrigin() const;
    inline FillSizeType maskSizeType() const;
    inline const LengthSize& maskSizeLength() const;
    inline FillLayer& ensureMaskLayers();
    inline const FillLayer& maskLayers() const; // Defined in RenderStyleInlines.h.
    inline Ref<const FillLayer> protectedMaskLayers() const; // Defined in RenderStyleInlines.h.

    inline const Style::MaskBorder& maskBorder() const;
    inline const Style::MaskBorderSource& maskBorderSource() const;
    inline const Style::MaskBorderSlice& maskBorderSlice() const;
    inline const Style::MaskBorderWidth& maskBorderWidth() const;
    inline const Style::MaskBorderOutset& maskBorderOutset() const;
    inline const Style::MaskBorderRepeat& maskBorderRepeat() const;
    inline NinePieceImageRule maskBorderHorizontalRule() const;
    inline NinePieceImageRule maskBorderVerticalRule() const;
    static inline Style::MaskBorder initialMaskBorder();

    BorderCollapse borderCollapse() const { return static_cast<BorderCollapse>(m_inheritedFlags.borderCollapse); }
    inline Style::WebkitBorderSpacing borderHorizontalSpacing() const;
    inline Style::WebkitBorderSpacing borderVerticalSpacing() const;
    EmptyCell emptyCells() const { return static_cast<EmptyCell>(m_inheritedFlags.emptyCells); }
    CaptionSide captionSide() const { return static_cast<CaptionSide>(m_inheritedFlags.captionSide); }

    inline const Style::ListStyleType& listStyleType() const;
    StyleImage* listStyleImage() const;
    ListStylePosition listStylePosition() const { return static_cast<ListStylePosition>(m_inheritedFlags.listStylePosition); }
    inline bool isFixedTableLayout() const;

    inline const Style::MarginBox& marginBox() const;
    inline const Style::MarginEdge& marginTop() const;
    inline const Style::MarginEdge& marginBottom() const;
    inline const Style::MarginEdge& marginLeft() const;
    inline const Style::MarginEdge& marginRight() const;
    inline const Style::MarginEdge& marginStart(const WritingMode) const;
    inline const Style::MarginEdge& marginEnd(const WritingMode) const;
    inline const Style::MarginEdge& marginBefore(const WritingMode) const;
    inline const Style::MarginEdge& marginAfter(const WritingMode) const;
    inline const Style::MarginEdge& marginBefore() const;
    inline const Style::MarginEdge& marginAfter() const;
    inline const Style::MarginEdge& marginStart() const;
    inline const Style::MarginEdge& marginEnd() const;

    inline const Style::PaddingBox& paddingBox() const;
    inline const Style::PaddingEdge& paddingTop() const;
    inline const Style::PaddingEdge& paddingBottom() const;
    inline const Style::PaddingEdge& paddingLeft() const;
    inline const Style::PaddingEdge& paddingRight() const;
    inline const Style::PaddingEdge& paddingBefore(const WritingMode) const;
    inline const Style::PaddingEdge& paddingAfter(const WritingMode) const;
    inline const Style::PaddingEdge& paddingStart(const WritingMode) const;
    inline const Style::PaddingEdge& paddingEnd(const WritingMode) const;
    inline const Style::PaddingEdge& paddingBefore() const;
    inline const Style::PaddingEdge& paddingAfter() const;
    inline const Style::PaddingEdge& paddingStart() const;
    inline const Style::PaddingEdge& paddingEnd() const;

    inline bool hasExplicitlySetPadding() const;
    inline bool hasExplicitlySetPaddingBottom() const;
    inline bool hasExplicitlySetPaddingLeft() const;
    inline bool hasExplicitlySetPaddingRight() const;
    inline bool hasExplicitlySetPaddingTop() const;

    CursorType cursorType() const { return static_cast<CursorType>(m_inheritedFlags.cursorType); }
    Style::Cursor cursor() const;

#if ENABLE(CURSOR_VISIBILITY)
    CursorVisibility cursorVisibility() const { return static_cast<CursorVisibility>(m_inheritedFlags.cursorVisibility); }
#endif

    InsideLink insideLink() const { return static_cast<InsideLink>(m_inheritedFlags.insideLink); }
    bool isLink() const { return m_nonInheritedFlags.isLink; }

    inline Style::Widows widows() const;
    inline Style::Orphans orphans() const;

    inline BreakInside breakInside() const;
    inline BreakBetween breakBefore() const;
    inline BreakBetween breakAfter() const;

    OptionSet<HangingPunctuation> hangingPunctuation() const;

    inline Style::WebkitTextStrokeWidth textStrokeWidth() const;
    inline Style::Opacity opacity() const;
    inline bool hasOpacity() const;
    inline StyleAppearance appearance() const;
    inline StyleAppearance usedAppearance() const;

    inline const Style::AspectRatio& aspectRatio() const;
    inline Style::Number<CSS::Nonnegative> aspectRatioWidth() const;
    inline Style::Number<CSS::Nonnegative> aspectRatioHeight() const;
    inline Style::Number<CSS::Nonnegative> aspectRatioLogicalWidth() const;
    inline Style::Number<CSS::Nonnegative> aspectRatioLogicalHeight() const;
    inline double logicalAspectRatio() const;
    inline bool hasAspectRatio() const;

    inline OptionSet<Containment> contain() const;
    inline OptionSet<Containment> usedContain() const;
    inline bool containsLayout() const;
    inline bool containsSize() const;
    inline bool containsInlineSize() const;
    inline bool containsSizeOrInlineSize() const;
    inline bool containsStyle() const;
    inline bool containsPaint() const;
    inline bool containsLayoutOrPaint() const;
    inline ContainerType containerType() const;
    inline const Style::ContainerNames& containerNames() const;
    inline bool containerTypeAndNamesEqual(const RenderStyle&) const;

    inline ContentVisibility contentVisibility() const;

    // usedContentVisibility will return ContentVisibility::Hidden in a content-visibility: hidden subtree (overriding
    // content-visibility: auto at all times), ContentVisibility::Auto in a content-visibility: auto subtree (when the
    // content is not user relevant and thus skipped), and ContentVisibility::Visible otherwise.
    inline ContentVisibility usedContentVisibility() const;
    inline bool isSkippedRootOrSkippedContent() const;

    inline const Style::ContainIntrinsicSize& containIntrinsicWidth() const;
    inline const Style::ContainIntrinsicSize& containIntrinsicHeight() const;
    inline const Style::ContainIntrinsicSize& containIntrinsicLogicalWidth() const;
    inline const Style::ContainIntrinsicSize& containIntrinsicLogicalHeight() const;
    inline bool hasAutoLengthContainIntrinsicSize() const;

    inline BoxAlignment boxAlign() const;
    BoxDirection boxDirection() const { return static_cast<BoxDirection>(m_inheritedFlags.boxDirection); }
    inline float boxFlex() const;
    inline unsigned boxFlexGroup() const;
    inline BoxLines boxLines() const;
    inline unsigned boxOrdinalGroup() const;
    inline BoxOrient boxOrient() const;
    inline BoxPack boxPack() const;

    inline Style::Order order() const;
    inline Style::FlexGrow flexGrow() const;
    inline Style::FlexShrink flexShrink() const;
    inline const Style::FlexBasis& flexBasis() const;
    inline const StyleContentAlignmentData& alignContent() const;
    inline const StyleSelfAlignmentData& alignItems() const;
    inline const StyleSelfAlignmentData& alignSelf() const;
    inline FlexDirection flexDirection() const;
    inline bool isRowFlexDirection() const;
    inline bool isColumnFlexDirection() const;
    inline bool isReverseFlexDirection() const;
    inline FlexWrap flexWrap() const;
    inline const StyleContentAlignmentData& justifyContent() const;
    inline const StyleSelfAlignmentData& justifyItems() const;
    inline const StyleSelfAlignmentData& justifySelf() const;

    inline GridAutoFlow gridAutoFlow() const;
    inline bool isGridAutoFlowDirectionRow() const;
    inline bool isGridAutoFlowDirectionColumn() const;
    inline bool isGridAutoFlowAlgorithmSparse() const;
    inline bool isGridAutoFlowAlgorithmDense() const;
    inline const Style::GridTrackSizes& gridAutoColumns() const;
    inline const Style::GridTrackSizes& gridAutoRows() const;
    inline const Style::GridTrackSizes& gridAutoList(Style::GridTrackSizingDirection) const;
    inline const Style::GridTemplateAreas& gridTemplateAreas() const;
    inline const Style::GridTemplateList& gridTemplateColumns() const;
    inline const Style::GridTemplateList& gridTemplateRows() const;
    inline const Style::GridTemplateList& gridTemplateList(Style::GridTrackSizingDirection) const;

    inline const Style::GridPosition& gridItemColumnStart() const;
    inline const Style::GridPosition& gridItemColumnEnd() const;
    inline const Style::GridPosition& gridItemRowStart() const;
    inline const Style::GridPosition& gridItemRowEnd() const;
    inline const Style::GridPosition& gridItemStart(Style::GridTrackSizingDirection) const;
    inline const Style::GridPosition& gridItemEnd(Style::GridTrackSizingDirection) const;

    inline const Style::TextShadows& textShadow() const;
    inline bool hasTextShadow() const;

    inline const Style::BoxShadows& boxShadow() const;
    inline bool hasBoxShadow() const;

    inline BoxDecorationBreak boxDecorationBreak() const;

    inline StyleReflection* boxReflect() const;
    inline BoxSizing boxSizing() const;
    inline BoxSizing boxSizingForAspectRatio() const;
    inline const Length& marqueeIncrement() const;
    inline int marqueeSpeed() const;
    inline int marqueeLoopCount() const;
    inline MarqueeBehavior marqueeBehavior() const;
    inline MarqueeDirection marqueeDirection() const;
    inline UserModify usedUserModify() const;
    inline UserModify userModify() const;
    inline UserDrag userDrag() const;
    WEBCORE_EXPORT UserSelect usedUserSelect() const;
    inline UserSelect userSelect() const;
    inline TextOverflow textOverflow() const;
    inline WordBreak wordBreak() const;
    inline OverflowWrap overflowWrap() const;
    inline NBSPMode nbspMode() const;
    inline LineBreak lineBreak() const;
    inline Hyphens hyphens() const;
    inline Style::HyphenateLimitEdge hyphenateLimitBefore() const;
    inline Style::HyphenateLimitEdge hyphenateLimitAfter() const;
    inline Style::HyphenateLimitLines hyphenateLimitLines() const;
    inline const Style::HyphenateCharacter& hyphenateCharacter() const;
    inline const AtomString& computedLocale() const;
    inline const AtomString& specifiedLocale() const;
    inline Resize resize() const;
    inline ColumnAxis columnAxis() const;
    inline bool hasInlineColumnAxis() const;
    inline ColumnProgression columnProgression() const;
    inline Style::ColumnWidth columnWidth() const;
    inline Style::ColumnCount columnCount() const;
    inline bool specifiesColumns() const;
    inline ColumnFill columnFill() const;
    inline BorderStyle columnRuleStyle() const;
    inline Style::LineWidth columnRuleWidth() const;
    inline bool columnRuleIsTransparent() const;
    inline ColumnSpan columnSpan() const;
    inline bool columnSpanEqual(const RenderStyle&) const;

    inline const Style::GapGutter& columnGap() const;
    inline const Style::GapGutter& rowGap() const;
    inline const Style::GapGutter& gap(Style::GridTrackSizingDirection) const;

    inline const TransformOperations& transform() const;
    inline bool hasTransform() const;
    inline const Style::TransformOrigin& transformOrigin() const;
    inline const Style::TransformOriginX& transformOriginX() const;
    inline const Style::TransformOriginY& transformOriginY() const;
    inline const Style::TransformOriginZ& transformOriginZ() const;

    inline TransformBox transformBox() const;

    inline const Style::Rotate& rotate() const;
    inline bool hasRotate() const;
    inline const Style::Scale& scale() const;
    inline bool hasScale() const;
    inline const Style::Translate& translate() const;
    inline bool hasTranslate() const;

    inline bool affectsTransform() const;

    inline const Style::TextEmphasisStyle& textEmphasisStyle() const;
    inline OptionSet<TextEmphasisPosition> textEmphasisPosition() const;

    inline RubyPosition rubyPosition() const;
    inline bool isInterCharacterRubyPosition() const;
    inline RubyAlign rubyAlign() const;
    inline RubyOverhang rubyOverhang() const;

#if ENABLE(DARK_MODE_CSS)
    inline Style::ColorScheme colorScheme() const;
    inline void setHasExplicitlySetColorScheme();
    inline bool hasExplicitlySetColorScheme() const;
#endif

    inline const Style::DynamicRangeLimit& dynamicRangeLimit() const;

    inline TableLayoutType tableLayout() const;

    inline ObjectFit objectFit() const;
    inline const Style::ObjectPosition& objectPosition() const;

    // Return true if any transform related property (currently transform, translate, scale, rotate, transformStyle3D or perspective)
    // indicates that we are transforming. The usedTransformStyle3D is not used here because in many cases (such as for deciding
    // whether or not to establish a containing block), the computed value is what matters.
    inline bool hasTransformRelatedProperty() const;

    enum class TransformOperationOption : uint8_t {
        TransformOrigin = 1 << 0,
        Translate       = 1 << 1,
        Rotate          = 1 << 2,
        Scale           = 1 << 3,
        Offset          = 1 << 4
    };

    static constexpr OptionSet<TransformOperationOption> allTransformOperations();
    static constexpr OptionSet<TransformOperationOption> individualTransformOperations();

    bool affectedByTransformOrigin() const;

    FloatPoint computePerspectiveOrigin(const FloatRect& boundingBox) const;
    void applyPerspective(TransformationMatrix&, const FloatPoint& originTranslate) const;

    FloatPoint3D computeTransformOrigin(const FloatRect& boundingBox) const;
    void applyTransformOrigin(TransformationMatrix&, const FloatPoint3D& originTranslate) const;
    void unapplyTransformOrigin(TransformationMatrix&, const FloatPoint3D& originTranslate) const;

    // applyTransform calls applyTransformOrigin(), then applyCSSTransform(), followed by unapplyTransformOrigin().
    void applyTransform(TransformationMatrix&, const TransformOperationData& boundingBox) const;
    void applyTransform(TransformationMatrix&, const TransformOperationData& boundingBox, OptionSet<TransformOperationOption>) const;
    void applyCSSTransform(TransformationMatrix&, const TransformOperationData& boundingBox) const;
    void applyCSSTransform(TransformationMatrix&, const TransformOperationData& boundingBox, OptionSet<TransformOperationOption>) const;
    void setPageScaleTransform(float);

    inline bool hasPositionedMask() const;
    inline bool hasMask() const;

    inline TextCombine textCombine() const;
    inline bool hasTextCombine() const;

    inline const TabSize& tabSize() const;

    inline const Style::WebkitLineGrid& lineGrid() const;
    inline LineSnap lineSnap() const;
    inline LineAlign lineAlign() const;

    PointerEvents pointerEvents() const { return static_cast<PointerEvents>(m_inheritedFlags.pointerEvents); }
    inline PointerEvents usedPointerEvents() const;

    inline const Style::ScrollTimelines& scrollTimelines() const;
    inline const Style::ProgressTimelineAxes& scrollTimelineAxes() const;
    inline const Style::ProgressTimelineNames& scrollTimelineNames() const;
    inline bool hasScrollTimelines() const;
    inline void setScrollTimelineAxes(Style::ProgressTimelineAxes&&);
    inline void setScrollTimelineNames(Style::ProgressTimelineNames&&);

    inline const Style::ViewTimelines& viewTimelines() const;
    inline const Style::ViewTimelineInsets& viewTimelineInsets() const;
    inline const Style::ProgressTimelineAxes& viewTimelineAxes() const;
    inline const Style::ProgressTimelineNames& viewTimelineNames() const;
    inline bool hasViewTimelines() const;
    inline void setViewTimelineInsets(Style::ViewTimelineInsets&&);
    inline void setViewTimelineAxes(Style::ProgressTimelineAxes&&);
    inline void setViewTimelineNames(Style::ProgressTimelineNames&&);

    static inline const NameScope initialTimelineScope();
    inline const NameScope& timelineScope() const;
    inline void setTimelineScope(const NameScope&);

    inline const AnimationList* animations() const;
    inline const AnimationList* transitions() const;

    AnimationList* animations();
    AnimationList* transitions();
    
    inline bool hasAnimationsOrTransitions() const;

    AnimationList& ensureAnimations();
    AnimationList& ensureTransitions();

    inline bool hasAnimations() const;
    inline bool hasTransitions() const;

    inline TransformStyle3D transformStyle3D() const;
    inline TransformStyle3D usedTransformStyle3D() const;
    inline bool preserves3D() const;

    inline BackfaceVisibility backfaceVisibility() const;
    inline const Style::Perspective& perspective() const;
    inline float usedPerspective() const;
    inline bool hasPerspective() const;
    inline const Style::PerspectiveOrigin& perspectiveOrigin() const;
    inline const Style::PerspectiveOriginX& perspectiveOriginX() const;
    inline const Style::PerspectiveOriginY& perspectiveOriginY() const;

    inline const LengthSize& pageSize() const;
    inline PageSizeType pageSizeType() const;

    inline OptionSet<Style::LineBoxContain> lineBoxContain() const;
    inline const LineClampValue& lineClamp() const;
    inline const Style::BlockEllipsis& blockEllipsis() const;
    inline Style::MaximumLines maxLines() const;
    inline OverflowContinue overflowContinue() const;
    inline const FloatSize& initialLetter() const;
    inline float initialLetterDrop() const;
    inline float initialLetterHeight() const;

    inline OptionSet<TouchAction> touchActions() const;
    // 'touch-action' behavior depends on values in ancestors. We use an additional inherited property to implement that.
    inline OptionSet<TouchAction> usedTouchActions() const;
    inline OptionSet<EventListenerRegionType> eventListenerRegionTypes() const;

    inline bool effectiveInert() const;

    const Style::ScrollMarginBox& scrollMarginBox() const;
    const Style::ScrollMarginEdge& scrollMarginTop() const;
    const Style::ScrollMarginEdge& scrollMarginBottom() const;
    const Style::ScrollMarginEdge& scrollMarginLeft() const;
    const Style::ScrollMarginEdge& scrollMarginRight() const;

    const Style::ScrollPaddingBox& scrollPaddingBox() const;
    const Style::ScrollPaddingEdge& scrollPaddingTop() const;
    const Style::ScrollPaddingEdge& scrollPaddingBottom() const;
    const Style::ScrollPaddingEdge& scrollPaddingLeft() const;
    const Style::ScrollPaddingEdge& scrollPaddingRight() const;
    inline bool scrollPaddingEqual(const RenderStyle&) const;

    bool hasSnapPosition() const;
    ScrollSnapType scrollSnapType() const;
    const ScrollSnapAlign& scrollSnapAlign() const;
    ScrollSnapStop scrollSnapStop() const;
    bool scrollSnapDataEquivalent(const RenderStyle&) const;

    Color usedScrollbarThumbColor() const;
    Color usedScrollbarTrackColor() const;
    inline const Style::ScrollbarColor& scrollbarColor() const;
    inline const Style::ScrollbarGutter& scrollbarGutter() const;
    inline ScrollbarWidth scrollbarWidth() const;

#if ENABLE(TOUCH_EVENTS)
    inline Style::Color tapHighlightColor() const;
#endif

#if ENABLE(WEBKIT_TOUCH_CALLOUT_CSS_PROPERTY)
    inline Style::WebkitTouchCallout touchCallout() const;
#endif

#if ENABLE(WEBKIT_OVERFLOW_SCROLLING_CSS_PROPERTY)
    inline Style::WebkitOverflowScrolling overflowScrolling() const;
#endif

    inline Style::ScrollBehavior scrollBehavior() const;

#if ENABLE(TEXT_AUTOSIZING)
    inline Style::TextSizeAdjust textSizeAdjust() const;
    AutosizeStatus autosizeStatus() const;
    bool isIdempotentTextAutosizingCandidate() const;
    bool isIdempotentTextAutosizingCandidate(AutosizeStatus overrideStatus) const;
#endif

    inline TextSecurity textSecurity() const;
    inline InputSecurity inputSecurity() const;

    inline ImageOrientation imageOrientation() const;
    inline ImageRendering imageRendering() const;

    inline OptionSet<SpeakAs> speakAs() const;

    inline const FilterOperations& filter() const;
    inline bool hasFilter() const;
    bool hasReferenceFilterOnly() const;

    inline const FilterOperations& appleColorFilter() const;
    inline bool hasAppleColorFilter() const;

    inline const FilterOperations& backdropFilter() const;
    inline bool hasBackdropFilter() const;

    inline void setBlendMode(BlendMode);
    inline bool isInSubtreeWithBlendMode() const;

    inline void setIsForceHidden();
    inline bool isForceHidden() const;

    inline void setIsolation(Isolation);

    inline BlendMode blendMode() const;
    inline bool hasBlendMode() const;

    inline Isolation isolation() const;
    inline bool hasIsolation() const;

    inline void setAutoRevealsWhenFound();
    inline bool autoRevealsWhenFound() const;

    bool shouldPlaceVerticalScrollbarOnLeft() const;

    inline bool usesStandardScrollbarStyle() const;
    inline bool usesLegacyScrollbarStyle() const;

#if ENABLE(APPLE_PAY)
    inline ApplePayButtonStyle applePayButtonStyle() const;
    inline ApplePayButtonType applePayButtonType() const;
#endif

#if HAVE(CORE_MATERIAL)
    inline AppleVisualEffect appleVisualEffect() const;
    inline bool hasAppleVisualEffect() const;
    inline bool hasAppleVisualEffectRequiringBackdropFilter() const;

    inline AppleVisualEffect usedAppleVisualEffectForSubtree() const;
#endif

    inline MathShift mathShift() const;
    inline MathStyle mathStyle() const;

    inline const Style::ViewTransitionClasses& viewTransitionClasses() const;
    inline const Style::ViewTransitionName& viewTransitionName() const;

    void setDisplay(DisplayType value)
    {
        m_nonInheritedFlags.originalDisplay = static_cast<unsigned>(value);
        m_nonInheritedFlags.effectiveDisplay = m_nonInheritedFlags.originalDisplay;
    }
    void setEffectiveDisplay(DisplayType v) { m_nonInheritedFlags.effectiveDisplay = static_cast<unsigned>(v); }
    void setPosition(PositionType v) { m_nonInheritedFlags.position = static_cast<unsigned>(v); }
    void setFloating(Float v) { m_nonInheritedFlags.floating = static_cast<unsigned>(v); }

    inline void setInsetBox(Style::InsetBox&&);
    inline void setLeft(Style::InsetEdge&&);
    inline void setRight(Style::InsetEdge&&);
    inline void setTop(Style::InsetEdge&&);
    inline void setBottom(Style::InsetEdge&&);

    inline void setWidth(Style::PreferredSize&&);
    inline void setHeight(Style::PreferredSize&&);

    inline void setLogicalWidth(Style::PreferredSize&&);
    inline void setLogicalHeight(Style::PreferredSize&&);

    inline void setMinWidth(Style::MinimumSize&&);
    inline void setMinHeight(Style::MinimumSize&&);
    inline void setMaxWidth(Style::MaximumSize&&);
    inline void setMaxHeight(Style::MaximumSize&&);

    inline void setLogicalMinWidth(Style::MinimumSize&&);
    inline void setLogicalMinHeight(Style::MinimumSize&&);
    inline void setLogicalMaxWidth(Style::MaximumSize&&);
    inline void setLogicalMaxHeight(Style::MaximumSize&&);

    inline void resetBorder();
    inline void resetBorderExceptRadius();
    inline void resetBorderTop();
    inline void resetBorderRight();
    inline void resetBorderBottom();
    inline void resetBorderLeft();
    inline void resetBorderImage();
    inline void resetBorderRadius();
    inline void resetBorderTopLeftRadius();
    inline void resetBorderTopRightRadius();
    inline void resetBorderBottomLeftRadius();
    inline void resetBorderBottomRightRadius();

    inline void setBackgroundColor(Style::Color&&);
    inline void setBackgroundAttachment(FillAttachment);
    inline void setBackgroundClip(FillBox);
    inline void setBackgroundOrigin(FillBox);
    inline void setBackgroundRepeat(FillRepeatXY);
    inline void setBackgroundBlendMode(BlendMode);

    inline void setBorderImage(Style::BorderImage&&);
    void setBorderImageSource(Style::BorderImageSource&&);
    void setBorderImageSlice(Style::BorderImageSlice&&);
    void setBorderImageWidth(Style::BorderImageWidth&&);
    void setBorderImageOutset(Style::BorderImageOutset&&);
    void setBorderImageRepeat(Style::BorderImageRepeat&&);

    inline void setBorderTopLeftRadius(Style::BorderRadiusValue&&);
    inline void setBorderTopRightRadius(Style::BorderRadiusValue&&);
    inline void setBorderBottomLeftRadius(Style::BorderRadiusValue&&);
    inline void setBorderBottomRightRadius(Style::BorderRadiusValue&&);
    inline void setBorderRadius(Style::BorderRadiusValue&&);
    inline void setHasExplicitlySetBorderBottomLeftRadius(bool);
    inline void setHasExplicitlySetBorderBottomRightRadius(bool);
    inline void setHasExplicitlySetBorderTopLeftRadius(bool);
    inline void setHasExplicitlySetBorderTopRightRadius(bool);

    inline void setBorderLeftWidth(Style::LineWidth);
    inline void setBorderLeftStyle(BorderStyle);
    inline void setBorderLeftColor(Style::Color&&);
    inline void setBorderRightWidth(Style::LineWidth);
    inline void setBorderRightStyle(BorderStyle);
    inline void setBorderRightColor(Style::Color&&);
    inline void setBorderTopWidth(Style::LineWidth);
    inline void setBorderTopStyle(BorderStyle);
    inline void setBorderTopColor(Style::Color&&);
    inline void setBorderBottomWidth(Style::LineWidth);
    inline void setBorderBottomStyle(BorderStyle);
    inline void setBorderBottomColor(Style::Color&&);

    inline void setOutlineWidth(Style::LineWidth);
    inline void setOutlineStyle(OutlineStyle);
    inline void setOutlineColor(Style::Color&&);

    void setOverflowX(Overflow v) { m_nonInheritedFlags.overflowX =  static_cast<unsigned>(v); }
    void setOverflowY(Overflow v) { m_nonInheritedFlags.overflowY = static_cast<unsigned>(v); }
    inline void setOverscrollBehaviorX(OverscrollBehavior);
    inline void setOverscrollBehaviorY(OverscrollBehavior);
    void setVisibility(Visibility v) { m_inheritedFlags.visibility = static_cast<unsigned>(v); }
    void setVerticalAlign(Style::VerticalAlign&&);

    inline void setClip(Style::Clip&&);

    void setUnicodeBidi(UnicodeBidi v) { m_nonInheritedFlags.unicodeBidi = static_cast<unsigned>(v); }

    void setClear(Clear v) { m_nonInheritedFlags.clear = static_cast<unsigned>(v); }

    inline void setFieldSizing(FieldSizing);

    void setFontCascade(FontCascade&&);
    WEBCORE_EXPORT void setFontDescription(FontCascadeDescription&&);
    bool setFontDescriptionWithoutUpdate(FontCascadeDescription&&);

    // Only used for blending font sizes when animating, for MathML anonymous blocks, and for text autosizing.
    void setFontSize(float);
    void setFontSizeAdjust(FontSizeAdjust);

    void setFontOpticalSizing(FontOpticalSizing);
    void setFontVariationSettings(FontVariationSettings);
    void setFontWeight(FontSelectionValue);
    void setFontWidth(FontSelectionValue);
    void setFontItalic(std::optional<FontSelectionValue>);
    void setFontPalette(const FontPalette&);

    void setColor(Color&&);

    void setTextAlign(TextAlignMode v) { m_inheritedFlags.textAlign = static_cast<unsigned>(v); }
    inline void setTextAlignLast(TextAlignLast);
    inline void setTextGroupAlign(TextGroupAlign);
    inline void addToTextDecorationLineInEffect(OptionSet<TextDecorationLine>);
    inline void setTextDecorationLineInEffect(OptionSet<TextDecorationLine>);
    inline void setTextDecorationLine(OptionSet<TextDecorationLine>);
    inline void setTextDecorationStyle(TextDecorationStyle);
    inline void setTextDecorationSkipInk(TextDecorationSkipInk);
    inline void setTextDecorationThickness(Style::TextDecorationThickness&&);
    inline void setTextIndent(Style::TextIndent&&);
    inline void setTextUnderlinePosition(OptionSet<TextUnderlinePosition>);
    inline void setTextUnderlineOffset(Style::TextUnderlineOffset&&);
    inline void setTextTransform(OptionSet<TextTransform>);
    void setLineHeight(Length&&);
    bool setZoom(float);
    inline bool setUsedZoom(float);
    inline void setTextZoom(TextZoom);

    inline void setTextJustify(TextJustify);

    inline void setTextBoxTrim(TextBoxTrim);
    void setTextBoxEdge(TextEdge);
    void setLineFitEdge(TextEdge);

    inline void setMarginTrim(OptionSet<MarginTrimType>);

#if ENABLE(TEXT_AUTOSIZING)
    void setSpecifiedLineHeight(Length&&);
#endif

    inline void setImageOrientation(ImageOrientation);
    inline void setImageRendering(ImageRendering);

    void setWhiteSpaceCollapse(WhiteSpaceCollapse v) { m_inheritedFlags.whiteSpaceCollapse = static_cast<unsigned>(v); }

    void setTextWrapMode(TextWrapMode v) { m_inheritedFlags.textWrapMode = static_cast<unsigned>(v); }
    void setTextWrapStyle(TextWrapStyle v) { m_inheritedFlags.textWrapStyle = static_cast<unsigned>(v); }

    // If letter-spacing is nonzero, we disable ligatures, which means this property affects font preparation.
    void setLetterSpacing(Length&&);
    void setWordSpacing(Length&&);

    inline void clearBackgroundLayers();
    inline void inheritBackgroundLayers(const FillLayer& parent);

    void adjustBackgroundLayers();

    inline void clearMaskLayers();
    inline void inheritMaskLayers(const FillLayer& parent);

    inline void adjustMaskLayers();

    inline void setMaskImage(RefPtr<StyleImage>&&);

    inline void setMaskBorder(Style::MaskBorder&&);
    void setMaskBorderSource(Style::MaskBorderSource&&);
    void setMaskBorderSlice(Style::MaskBorderSlice&&);
    void setMaskBorderWidth(Style::MaskBorderWidth&&);
    void setMaskBorderOutset(Style::MaskBorderOutset&&);
    void setMaskBorderRepeat(Style::MaskBorderRepeat&&);

    inline void setMaskRepeat(FillRepeatXY);

    void setBorderCollapse(BorderCollapse collapse) { m_inheritedFlags.borderCollapse = static_cast<unsigned>(collapse); }
    inline void setBorderHorizontalSpacing(Style::WebkitBorderSpacing);
    inline void setBorderVerticalSpacing(Style::WebkitBorderSpacing);
    void setEmptyCells(EmptyCell v) { m_inheritedFlags.emptyCells = static_cast<unsigned>(v); }
    void setCaptionSide(CaptionSide v) { m_inheritedFlags.captionSide = static_cast<unsigned>(v); }

    inline void setAspectRatio(Style::AspectRatio&&);

    inline void setContain(OptionSet<Containment>);
    inline void setContainerType(ContainerType);
    inline void setContainerNames(Style::ContainerNames&&);

    inline void containIntrinsicWidthAddAuto();
    inline void containIntrinsicHeightAddAuto();
    inline void setContainIntrinsicWidth(Style::ContainIntrinsicSize&&);
    inline void setContainIntrinsicHeight(Style::ContainIntrinsicSize&&);

    inline void setContentVisibility(ContentVisibility);

    inline void setUsedContentVisibility(ContentVisibility);

    inline void setListStyleType(Style::ListStyleType&&);
    void setListStyleImage(RefPtr<StyleImage>&&);
    void setListStylePosition(ListStylePosition v) { m_inheritedFlags.listStylePosition = static_cast<unsigned>(v); }

    inline void resetMargin();
    inline void setMarginBox(Style::MarginBox&&);
    inline void setMarginTop(Style::MarginEdge&&);
    inline void setMarginBottom(Style::MarginEdge&&);
    inline void setMarginLeft(Style::MarginEdge&&);
    inline void setMarginRight(Style::MarginEdge&&);
    void setMarginStart(Style::MarginEdge&&);
    void setMarginEnd(Style::MarginEdge&&);
    void setMarginBefore(Style::MarginEdge&&);
    void setMarginAfter(Style::MarginEdge&&);

    inline void resetPadding();
    inline void setPaddingBox(Style::PaddingBox&&);
    inline void setPaddingTop(Style::PaddingEdge&&);
    inline void setPaddingBottom(Style::PaddingEdge&&);
    inline void setPaddingLeft(Style::PaddingEdge&&);
    inline void setPaddingRight(Style::PaddingEdge&&);
    void setPaddingStart(Style::PaddingEdge&&);
    void setPaddingEnd(Style::PaddingEdge&&);
    void setPaddingBefore(Style::PaddingEdge&&);
    void setPaddingAfter(Style::PaddingEdge&&);

    inline void setHasExplicitlySetPaddingBottom(bool);
    inline void setHasExplicitlySetPaddingLeft(bool);
    inline void setHasExplicitlySetPaddingRight(bool);
    inline void setHasExplicitlySetPaddingTop(bool);

    inline void setCursor(Style::Cursor&&);

#if ENABLE(CURSOR_VISIBILITY)
    void setCursorVisibility(CursorVisibility c) { m_inheritedFlags.cursorVisibility = static_cast<unsigned>(c); }
#endif

    void setInsideLink(InsideLink insideLink) { m_inheritedFlags.insideLink = static_cast<unsigned>(insideLink); }
    void setIsLink(bool v) { m_nonInheritedFlags.isLink = v; }

    PrintColorAdjust printColorAdjust() const { return static_cast<PrintColorAdjust>(m_inheritedFlags.printColorAdjust); }
    void setPrintColorAdjust(PrintColorAdjust value) { m_inheritedFlags.printColorAdjust = static_cast<unsigned>(value); }

    inline Style::ZIndex specifiedZIndex() const;
    inline void setSpecifiedZIndex(Style::ZIndex);

    inline Style::ZIndex usedZIndex() const;
    inline void setUsedZIndex(Style::ZIndex);

    inline void setWidows(Style::Widows);
    inline void setOrphans(Style::Orphans);

    inline void setOutlineOffset(Style::Length<>);
    inline void setTextShadow(Style::TextShadows&&);
    inline void setTextStrokeColor(Style::Color&&);
    inline void setTextStrokeWidth(Style::WebkitTextStrokeWidth);
    inline void setTextFillColor(Style::Color&&);
    inline void setCaretColor(Style::Color&&);
    inline void setHasAutoCaretColor();
    inline void setAccentColor(Style::Color&&);
    inline void setHasAutoAccentColor();
    inline void setOpacity(Style::Opacity);
    inline void setAppearance(StyleAppearance);
    inline void setUsedAppearance(StyleAppearance);
    inline void setBoxAlign(BoxAlignment);
    void setBoxDirection(BoxDirection d) { m_inheritedFlags.boxDirection = static_cast<unsigned>(d); }
    inline void setBoxFlex(float);
    inline void setBoxFlexGroup(unsigned);
    inline void setBoxLines(BoxLines);
    inline void setBoxOrdinalGroup(unsigned);
    inline void setBoxOrient(BoxOrient);
    inline void setBoxPack(BoxPack);
    inline void setBoxShadow(Style::BoxShadows&&);
    inline void setBoxReflect(RefPtr<StyleReflection>&&);
    inline void setBoxSizing(BoxSizing);
    inline void setFlexGrow(Style::FlexGrow);
    inline void setFlexShrink(Style::FlexShrink);
    inline void setFlexBasis(Style::FlexBasis&&);
    inline void setOrder(Style::Order);
    inline void setAlignContent(const StyleContentAlignmentData&);
    inline void setAlignItems(const StyleSelfAlignmentData&);
    inline void setAlignItemsPosition(ItemPosition);
    inline void setAlignSelf(const StyleSelfAlignmentData&);
    inline void setAlignSelfPosition(ItemPosition);
    inline void setFlexDirection(FlexDirection);
    inline void setFlexWrap(FlexWrap);
    inline void setJustifyContent(const StyleContentAlignmentData&);
    inline void setJustifyContentPosition(ContentPosition);
    inline void setJustifyItems(const StyleSelfAlignmentData&);
    inline void setJustifySelf(const StyleSelfAlignmentData&);
    inline void setJustifySelfPosition(ItemPosition);

    inline void setBoxDecorationBreak(BoxDecorationBreak);

    inline void setGridAutoFlow(GridAutoFlow);
    inline void setGridAutoColumns(Style::GridTrackSizes&&);
    inline void setGridAutoRows(Style::GridTrackSizes&&);
    inline void setGridTemplateAreas(Style::GridTemplateAreas&&);
    inline void setGridTemplateColumns(Style::GridTemplateList&&);
    inline void setGridTemplateRows(Style::GridTemplateList&&);

    inline void setGridItemColumnStart(Style::GridPosition&&);
    inline void setGridItemColumnEnd(Style::GridPosition&&);
    inline void setGridItemRowStart(Style::GridPosition&&);
    inline void setGridItemRowEnd(Style::GridPosition&&);

    inline void setMarqueeIncrement(Length&&);
    inline void setMarqueeSpeed(int);
    inline void setMarqueeDirection(MarqueeDirection);
    inline void setMarqueeBehavior(MarqueeBehavior);
    inline void setMarqueeLoopCount(int);
    inline void setUserModify(UserModify);
    inline void setUserDrag(UserDrag);
    inline void setUserSelect(UserSelect);
    inline void setTextOverflow(TextOverflow);
    inline void setWordBreak(WordBreak);
    inline void setOverflowWrap(OverflowWrap);
    inline void setNBSPMode(NBSPMode);
    inline void setLineBreak(LineBreak);
    inline void setHyphens(Hyphens);
    inline void setHyphenateLimitBefore(Style::HyphenateLimitEdge);
    inline void setHyphenateLimitAfter(Style::HyphenateLimitEdge);
    inline void setHyphenateLimitLines(Style::HyphenateLimitLines);
    inline void setHyphenateCharacter(Style::HyphenateCharacter&&);
    inline void setResize(Resize);
    inline void setColumnAxis(ColumnAxis);
    inline void setColumnProgression(ColumnProgression);
    inline void setColumnWidth(Style::ColumnWidth);
    inline void setColumnCount(Style::ColumnCount);
    inline void setColumnFill(ColumnFill);
    inline void setColumnGap(Style::GapGutter&&);
    inline void setRowGap(Style::GapGutter&&);
    inline void setColumnRuleColor(Style::Color&&);
    inline void setColumnRuleStyle(BorderStyle);
    inline void setColumnRuleWidth(Style::LineWidth);
    inline void resetColumnRule();
    inline void setColumnSpan(ColumnSpan);
    inline void inheritColumnPropertiesFrom(const RenderStyle& parent);

    inline void setTransform(TransformOperations&&);
    inline void setTransformOrigin(Style::TransformOrigin&&);
    inline void setTransformOriginX(Style::TransformOriginX&&);
    inline void setTransformOriginY(Style::TransformOriginY&&);
    inline void setTransformOriginZ(Style::TransformOriginZ&&);
    inline void setTransformBox(TransformBox);

    inline void setRotate(Style::Rotate&&);
    inline void setScale(Style::Scale&&);
    inline void setTranslate(Style::Translate&&);

    inline void setSpeakAs(OptionSet<SpeakAs>);
    inline void setTextCombine(TextCombine);
    inline void setTextDecorationColor(Style::Color&&);
    inline void setTextEmphasisColor(Style::Color&&);
    inline void setTextEmphasisStyle(Style::TextEmphasisStyle&&);
    inline void setTextEmphasisPosition(OptionSet<TextEmphasisPosition>);

    inline void setObjectFit(ObjectFit);
    inline void setObjectPosition(Style::ObjectPosition&&);

    inline void setRubyPosition(RubyPosition);
    inline void setRubyAlign(RubyAlign);
    inline void setRubyOverhang(RubyOverhang);

#if ENABLE(DARK_MODE_CSS)
    inline void setColorScheme(Style::ColorScheme);
#endif

    inline void setDynamicRangeLimit(Style::DynamicRangeLimit&&);

    inline void setTableLayout(TableLayoutType);

    inline void setFilter(FilterOperations&&);
    inline void setAppleColorFilter(FilterOperations&&);

    inline void setBackdropFilter(FilterOperations&&);

    inline void setTabSize(const TabSize&);

    inline void setBreakBefore(BreakBetween);
    inline void setBreakAfter(BreakBetween);
    inline void setBreakInside(BreakInside);
    
    inline void setHangingPunctuation(OptionSet<HangingPunctuation>);

    inline void setLineGrid(Style::WebkitLineGrid&&);
    inline void setLineSnap(LineSnap);
    inline void setLineAlign(LineAlign);

    void setPointerEvents(PointerEvents p) { m_inheritedFlags.pointerEvents = static_cast<unsigned>(p); }

    inline void clearAnimations();
    inline void clearTransitions();

    void adjustAnimations();
    void adjustTransitions();

    void adjustScrollTimelines();
    void adjustViewTimelines();

    inline void setTransformStyle3D(TransformStyle3D);
    inline void setTransformStyleForcedToFlat(bool);
    inline void setBackfaceVisibility(BackfaceVisibility);
    inline void setPerspective(Style::Perspective&&);
    inline void setPerspectiveOrigin(Style::PerspectiveOrigin&&);
    inline void setPerspectiveOriginX(Style::PerspectiveOriginX&&);
    inline void setPerspectiveOriginY(Style::PerspectiveOriginY&&);
    inline void setPageSize(LengthSize);
    inline void setPageSizeType(PageSizeType);
    inline void resetPageSizeType();

    inline void setLineBoxContain(OptionSet<Style::LineBoxContain>);
    inline void setLineClamp(LineClampValue);
    
    inline void setMaxLines(Style::MaximumLines);
    inline void setOverflowContinue(OverflowContinue);
    inline void setBlockEllipsis(Style::BlockEllipsis&&);

    inline void setInitialLetter(const FloatSize&);
    
    inline void setTouchActions(OptionSet<TouchAction>);
    inline void setUsedTouchActions(OptionSet<TouchAction>);
    inline void setEventListenerRegionTypes(OptionSet<EventListenerRegionType>);

    inline void setEffectiveInert(bool);

    void setScrollMarginTop(Style::ScrollMarginEdge&&);
    void setScrollMarginBottom(Style::ScrollMarginEdge&&);
    void setScrollMarginLeft(Style::ScrollMarginEdge&&);
    void setScrollMarginRight(Style::ScrollMarginEdge&&);

    void setScrollPaddingTop(Style::ScrollPaddingEdge&&);
    void setScrollPaddingBottom(Style::ScrollPaddingEdge&&);
    void setScrollPaddingLeft(Style::ScrollPaddingEdge&&);
    void setScrollPaddingRight(Style::ScrollPaddingEdge&&);

    void setScrollSnapType(ScrollSnapType);
    void setScrollSnapAlign(const ScrollSnapAlign&);
    void setScrollSnapStop(ScrollSnapStop);

    inline void setScrollbarColor(Style::ScrollbarColor&&);
    inline void setScrollbarGutter(Style::ScrollbarGutter&&);
    inline void setScrollbarWidth(ScrollbarWidth);

#if ENABLE(TOUCH_EVENTS)
    inline void setTapHighlightColor(Style::Color&&);
#endif

#if ENABLE(WEBKIT_TOUCH_CALLOUT_CSS_PROPERTY)
    inline void setTouchCallout(Style::WebkitTouchCallout);
#endif

#if ENABLE(WEBKIT_OVERFLOW_SCROLLING_CSS_PROPERTY)
    inline void setOverflowScrolling(Style::WebkitOverflowScrolling);
#endif

    inline void setScrollBehavior(Style::ScrollBehavior);

#if ENABLE(TEXT_AUTOSIZING)
    inline void setTextSizeAdjust(Style::TextSizeAdjust);
    void setAutosizeStatus(AutosizeStatus);
#endif

    inline void setTextSecurity(TextSecurity);
    inline void setInputSecurity(InputSecurity);

#if ENABLE(APPLE_PAY)
    inline void setApplePayButtonStyle(ApplePayButtonStyle);
    inline void setApplePayButtonType(ApplePayButtonType);
#endif

#if HAVE(CORE_MATERIAL)
    inline void setAppleVisualEffect(AppleVisualEffect);
    inline void setUsedAppleVisualEffectForSubtree(AppleVisualEffect);
#endif

    void addCustomPaintWatchProperty(const AtomString&);

    // Support for paint-order, stroke-linecap, stroke-linejoin, and stroke-miterlimit from https://drafts.fxtf.org/paint/.
    inline void setPaintOrder(PaintOrder);
    inline PaintOrder paintOrder() const;
    static constexpr PaintOrder initialPaintOrder();
    static std::span<const PaintType, 3> paintTypesForPaintOrder(PaintOrder);
    
    inline void setCapStyle(LineCap);
    inline LineCap capStyle() const;
    static constexpr LineCap initialCapStyle();
    
    inline void setJoinStyle(LineJoin);
    inline LineJoin joinStyle() const;
    static constexpr LineJoin initialJoinStyle();
    
    inline const Style::StrokeWidth& strokeWidth() const;
    inline void setStrokeWidth(Style::StrokeWidth&&);
    inline bool hasVisibleStroke() const;
    static inline Style::StrokeWidth initialStrokeWidth();

    float computedStrokeWidth(const IntSize& viewportSize) const;
    inline void setHasExplicitlySetStrokeWidth(bool);
    inline bool hasExplicitlySetStrokeWidth() const;
    bool hasPositiveStrokeWidth() const;
    
    inline const Style::Color& strokeColor() const;
    inline void setStrokeColor(Style::Color&&);
    inline void setVisitedLinkStrokeColor(Style::Color&&);
    inline const Style::Color& visitedLinkStrokeColor() const;
    inline void setHasExplicitlySetStrokeColor(bool);
    inline bool hasExplicitlySetStrokeColor() const;
    static inline Style::Color initialStrokeColor();
    Color computedStrokeColor() const;
    inline CSSPropertyID usedStrokeColorProperty() const;

    inline Style::StrokeMiterlimit strokeMiterLimit() const;
    inline void setStrokeMiterLimit(Style::StrokeMiterlimit);
    static constexpr Style::StrokeMiterlimit initialStrokeMiterLimit();

    static constexpr Style::Opacity initialFillOpacity();
    static constexpr Style::Opacity initialStrokeOpacity();
    static inline Style::SVGStrokeDasharray initialStrokeDashArray();
    static inline Style::SVGStrokeDashoffset initialStrokeDashOffset();

    const SVGRenderStyle& svgStyle() const { return m_svgStyle; }
    inline SVGRenderStyle& accessSVGStyle();

    inline const Style::SVGPaint& fill() const;
    inline const Style::SVGPaint& visitedLinkFill() const;
    inline void setFill(Style::SVGPaint&&);
    inline void setVisitedLinkFill(Style::SVGPaint&&);
    inline void setHasExplicitlySetColor(bool);
    inline bool hasExplicitlySetColor() const;
    inline Style::Opacity fillOpacity() const;
    inline void setFillOpacity(Style::Opacity);

    inline const Style::SVGPaint& stroke() const;
    inline const Style::SVGPaint& visitedLinkStroke() const;
    inline void setStroke(Style::SVGPaint&&);
    inline void setVisitedLinkStroke(Style::SVGPaint&&);
    inline Style::Opacity strokeOpacity() const;
    inline void setStrokeOpacity(Style::Opacity);
    inline const Style::SVGStrokeDasharray& strokeDashArray() const;
    inline void setStrokeDashArray(Style::SVGStrokeDasharray&&);
    inline const Style::SVGStrokeDashoffset& strokeDashOffset() const;
    inline void setStrokeDashOffset(Style::SVGStrokeDashoffset&&);

    inline const Style::SVGCenterCoordinateComponent& cx() const;
    inline void setCx(Style::SVGCenterCoordinateComponent&&);
    inline const Style::SVGCenterCoordinateComponent& cy() const;
    inline void setCy(Style::SVGCenterCoordinateComponent&&);
    inline const Style::SVGRadius& r() const;
    inline void setR(Style::SVGRadius&&);
    inline const Style::SVGRadiusComponent& rx() const;
    inline void setRx(Style::SVGRadiusComponent&&);
    inline const Style::SVGRadiusComponent& ry() const;
    inline void setRy(Style::SVGRadiusComponent&&);
    inline const Style::SVGCoordinateComponent& x() const;
    inline void setX(Style::SVGCoordinateComponent&&);
    inline const Style::SVGCoordinateComponent& y() const;
    inline void setY(Style::SVGCoordinateComponent&&);

    inline void setD(RefPtr<StylePathData>&&);
    inline StylePathData* d() const;
    static StylePathData* initialD() { return nullptr; }

    inline Style::Opacity floodOpacity() const;
    inline void setFloodOpacity(Style::Opacity);
    static constexpr Style::Opacity initialFloodOpacity();

    inline Style::Opacity stopOpacity() const;
    inline void setStopOpacity(Style::Opacity);
    static constexpr Style::Opacity initialStopOpacity();

    inline void setStopColor(Style::Color&&);
    inline void setFloodColor(Style::Color&&);
    inline void setLightingColor(Style::Color&&);

    inline const Style::SVGBaselineShift& baselineShift() const;
    inline void setBaselineShift(Style::SVGBaselineShift&&);

    inline void setShapeOutside(Style::ShapeOutside&&);
    inline const Style::ShapeOutside& shapeOutside() const;
    static Style::ShapeOutside initialShapeOutside();

    inline const Style::ShapeMargin& shapeMargin() const;
    inline void setShapeMargin(Style::ShapeMargin&&);
    static inline Style::ShapeMargin initialShapeMargin();

    inline Style::ShapeImageThreshold shapeImageThreshold() const;
    void setShapeImageThreshold(Style::ShapeImageThreshold);
    static constexpr Style::ShapeImageThreshold initialShapeImageThreshold();

    inline const Style::ClipPath& clipPath() const;
    inline bool hasClipPath() const;
    inline void setClipPath(Style::ClipPath&&);
    static inline Style::ClipPath initialClipPath();

    inline bool hasUsedContentNone() const;
    inline bool hasContent() const;
    inline const Style::Content& content() const;
    String altFromContent() const;
    inline void setContent(Style::Content&&);

    inline bool hasAttrContent() const;
    inline void setHasAttrContent();

    const CounterDirectiveMap& counterDirectives() const;
    CounterDirectiveMap& accessCounterDirectives();

    inline const Style::Quotes& quotes() const;
    void setQuotes(Style::Quotes&&);

    inline void setViewTransitionClasses(Style::ViewTransitionClasses&&);
    inline void setViewTransitionName(Style::ViewTransitionName&&);

    inline WillChangeData* willChange() const;
    void setWillChange(RefPtr<WillChangeData>&&);

    bool willChangeCreatesStackingContext() const;

    const AtomString& hyphenString() const;

    bool inheritedEqual(const RenderStyle&) const;
    bool nonInheritedEqual(const RenderStyle&) const;
    bool fastPathInheritedEqual(const RenderStyle&) const;
    bool nonFastPathInheritedEqual(const RenderStyle&) const;

    bool descendantAffectingNonInheritedPropertiesEqual(const RenderStyle&) const;

#if ENABLE(TEXT_AUTOSIZING)
    uint32_t hashForTextAutosizing() const;
    bool equalForTextAutosizing(const RenderStyle&) const;
#endif

    StyleDifference diff(const RenderStyle&, OptionSet<StyleDifferenceContextSensitiveProperty>& changedContextSensitiveProperties) const;
    bool diffRequiresLayerRepaint(const RenderStyle&, bool isComposited) const;
    void conservativelyCollectChangedAnimatableProperties(const RenderStyle&, CSSPropertiesBitSet&) const;

#if !LOG_DISABLED
    void dumpDifferences(TextStream&, const RenderStyle&) const;
#endif

    constexpr bool isDisplayInlineType() const;
    constexpr bool isOriginalDisplayInlineType() const;
    constexpr bool isDisplayFlexibleOrGridBox() const;
    constexpr bool isDisplayDeprecatedFlexibleBox() const;
    constexpr bool isDisplayFlexibleBoxIncludingDeprecatedOrGridBox() const;
    constexpr bool isDisplayRegionType() const;
    constexpr bool isDisplayBlockLevel() const;
    constexpr bool doesDisplayGenerateBlockContainer() const;
    constexpr bool isOriginalDisplayBlockType() const;
    constexpr bool isDisplayTableOrTablePart() const;
    constexpr bool isInternalTableBox() const;
    constexpr bool isRubyContainerOrInternalRubyBox() const;
    constexpr bool isOriginalDisplayListItemType() const;

    inline bool setDirection(TextDirection bidiDirection);
    inline bool hasExplicitlySetDirection() const;
    inline void setHasExplicitlySetDirection();

    inline bool setWritingMode(StyleWritingMode);
    inline bool hasExplicitlySetWritingMode() const;
    inline void setHasExplicitlySetWritingMode();
    inline bool setTextOrientation(TextOrientation);

    bool emptyState() const { return m_nonInheritedFlags.emptyState; }
    void setEmptyState(bool v) { m_nonInheritedFlags.emptyState = v; }
    bool firstChildState() const { return m_nonInheritedFlags.firstChildState; }
    void setFirstChildState() { m_nonInheritedFlags.firstChildState = true; }
    bool lastChildState() const { return m_nonInheritedFlags.lastChildState; }
    void setLastChildState() { m_nonInheritedFlags.lastChildState = true; }

    Color colorResolvingCurrentColor(CSSPropertyID colorProperty, bool visitedLink) const;

    // Resolves the currentColor keyword, but must not be used for the "color" property which has a different semantic.
    WEBCORE_EXPORT Color colorResolvingCurrentColor(const Style::Color&, bool visitedLink = false) const;

    WEBCORE_EXPORT Color visitedDependentColor(CSSPropertyID, OptionSet<PaintBehavior> paintBehavior = { }) const;
    WEBCORE_EXPORT Color visitedDependentColorWithColorFilter(CSSPropertyID, OptionSet<PaintBehavior> paintBehavior = { }) const;

    WEBCORE_EXPORT Color colorByApplyingColorFilter(const Color&) const;
    WEBCORE_EXPORT Color colorWithColorFilter(const Style::Color&) const;

    void setHasExplicitlyInheritedProperties() { m_nonInheritedFlags.hasExplicitlyInheritedProperties = true; }
    bool hasExplicitlyInheritedProperties() const { return m_nonInheritedFlags.hasExplicitlyInheritedProperties; }

    bool disallowsFastPathInheritance() const { return m_nonInheritedFlags.disallowsFastPathInheritance; }
    void setDisallowsFastPathInheritance() { m_nonInheritedFlags.disallowsFastPathInheritance = true; }

    inline void setMathShift(const MathShift&);
    inline void setMathStyle(const MathStyle&);

    void setTextSpacingTrim(TextSpacingTrim v);
    void setTextAutospace(TextAutospace v);

    static constexpr Overflow initialOverflowX();
    static constexpr Overflow initialOverflowY();
    static constexpr OverscrollBehavior initialOverscrollBehaviorX();
    static constexpr OverscrollBehavior initialOverscrollBehaviorY();

    static constexpr Clear initialClear();
    static inline Style::Clip initialClip();
    static inline Style::SVGCenterCoordinateComponent initialCx();
    static inline Style::SVGCenterCoordinateComponent initialCy();
    static constexpr DisplayType initialDisplay();
    static constexpr UnicodeBidi initialUnicodeBidi();
    static constexpr PositionType initialPosition();
    static inline Style::VerticalAlign initialVerticalAlign();
    static constexpr Float initialFloating();
    static constexpr BreakBetween initialBreakBetween();
    static constexpr BreakInside initialBreakInside();
    static constexpr OptionSet<HangingPunctuation> initialHangingPunctuation();
    static constexpr TableLayoutType initialTableLayout();
    static constexpr BorderCollapse initialBorderCollapse();
    static constexpr BorderStyle initialBorderStyle();
    static inline Style::BorderRadiusValue initialBorderRadius();
    static constexpr Style::CornerShapeValue initialCornerShapeValue();
    static constexpr CaptionSide initialCaptionSide();
    static constexpr ColumnAxis initialColumnAxis();
    static constexpr ColumnProgression initialColumnProgression();
    static constexpr TextDirection initialDirection();
    static constexpr StyleWritingMode initialWritingMode();
    static constexpr TextCombine initialTextCombine();
    static constexpr TextOrientation initialTextOrientation();
    static constexpr ObjectFit initialObjectFit();
    static inline Style::ObjectPosition initialObjectPosition();
    static constexpr EmptyCell initialEmptyCells();
    static constexpr ListStylePosition initialListStylePosition();
    static inline Style::ListStyleType initialListStyleType();
    static constexpr OptionSet<TextTransform> initialTextTransform();
    static inline Style::ViewTransitionClasses initialViewTransitionClasses();
    static inline Style::ViewTransitionName initialViewTransitionName();
    static constexpr Visibility initialVisibility();
    static constexpr WhiteSpaceCollapse initialWhiteSpaceCollapse();
    static constexpr Style::WebkitBorderSpacing initialBorderHorizontalSpacing();
    static constexpr Style::WebkitBorderSpacing initialBorderVerticalSpacing();
    static inline Style::Cursor initialCursor();
    static inline Color initialColor();
    static inline Style::Color initialTextStrokeColor();
    static inline Style::Color initialTextDecorationColor();
    static StyleImage* initialListStyleImage() { return 0; }
    static constexpr Style::LineWidth initialBorderWidth();
    static constexpr Style::LineWidth initialColumnRuleWidth();
    static constexpr Style::LineWidth initialOutlineWidth();
    static inline Length initialLetterSpacing();
    static inline Length initialWordSpacing();
    static inline Style::PreferredSize initialSize();
    static inline Style::MinimumSize initialMinSize();
    static inline Style::MaximumSize initialMaxSize();
    static inline Style::InsetEdge initialInset();
    static inline Style::SVGRadius initialR();
    static inline Style::SVGRadiusComponent initialRx();
    static inline Style::SVGRadiusComponent initialRy();
    static inline Style::MarginEdge initialMargin();
    static constexpr OptionSet<MarginTrimType> initialMarginTrim();
    static inline Style::PaddingEdge initialPadding();
    static inline Style::TextIndent initialTextIndent();
    static constexpr TextBoxTrim initialTextBoxTrim();
    static TextEdge initialTextBoxEdge();
    static TextEdge initialLineFitEdge();
    static constexpr LengthType zeroLength();
    static constexpr Style::Widows initialWidows();
    static constexpr Style::Orphans initialOrphans();
    // Returning -100% percent here means the line-height is not set.
    static inline Length initialLineHeight();
    static constexpr TextAlignMode initialTextAlign();
    static constexpr TextAlignLast initialTextAlignLast();
    static constexpr TextGroupAlign initialTextGroupAlign();
    static constexpr OptionSet<TextDecorationLine> initialTextDecorationLine();
    static constexpr TextDecorationStyle initialTextDecorationStyle();
    static constexpr TextDecorationSkipInk initialTextDecorationSkipInk();
    static constexpr OptionSet<TextUnderlinePosition> initialTextUnderlinePosition();
    static inline Style::TextUnderlineOffset initialTextUnderlineOffset();
    static inline Style::TextDecorationThickness initialTextDecorationThickness();
    static constexpr Style::ZIndex initialSpecifiedZIndex();
    static constexpr Style::ZIndex initialUsedZIndex();
    static float initialZoom() { return 1.0f; }
    static constexpr TextZoom initialTextZoom();
    static constexpr Style::Length<> initialOutlineOffset();
    static constexpr Style::Opacity initialOpacity();
    static constexpr BoxAlignment initialBoxAlign();
    static constexpr BoxDecorationBreak initialBoxDecorationBreak();
    static constexpr BoxDirection initialBoxDirection();
    static constexpr BoxLines initialBoxLines();
    static constexpr BoxOrient initialBoxOrient();
    static constexpr BoxPack initialBoxPack();
    static float initialBoxFlex() { return 0.0f; }
    static unsigned initialBoxFlexGroup() { return 1; }
    static unsigned initialBoxOrdinalGroup() { return 1; }
    static inline Style::BoxShadows initialBoxShadow();
    static constexpr BoxSizing initialBoxSizing();
    static StyleReflection* initialBoxReflect() { return 0; }
    static constexpr Style::FlexGrow initialFlexGrow();
    static constexpr Style::FlexShrink initialFlexShrink();
    static inline Style::FlexBasis initialFlexBasis();
    static constexpr Style::Order initialOrder();
    static constexpr StyleSelfAlignmentData initialJustifyItems();
    static constexpr StyleSelfAlignmentData initialSelfAlignment();
    static constexpr StyleSelfAlignmentData initialDefaultAlignment();
    static constexpr StyleContentAlignmentData initialContentAlignment();
    static constexpr FlexDirection initialFlexDirection();
    static constexpr FlexWrap initialFlexWrap();
    static int initialMarqueeLoopCount() { return -1; }
    static int initialMarqueeSpeed() { return 85; }
    static inline Length initialMarqueeIncrement();
    static constexpr MarqueeBehavior initialMarqueeBehavior();
    static constexpr MarqueeDirection initialMarqueeDirection();
    static constexpr UserModify initialUserModify();
    static constexpr UserDrag initialUserDrag();
    static constexpr UserSelect initialUserSelect();
    static constexpr TextOverflow initialTextOverflow();
    static inline Style::TextShadows initialTextShadow();
    static constexpr TextWrapMode initialTextWrapMode();
    static constexpr TextWrapStyle initialTextWrapStyle();
    static constexpr WordBreak initialWordBreak();
    static constexpr OutlineStyle initialOutlineStyle();
    static constexpr OverflowWrap initialOverflowWrap();
    static constexpr NBSPMode initialNBSPMode();
    static constexpr LineBreak initialLineBreak();
    static constexpr OptionSet<SpeakAs> initialSpeakAs();
    static constexpr Hyphens initialHyphens();
    static constexpr Style::HyphenateLimitEdge initialHyphenateLimitBefore();
    static constexpr Style::HyphenateLimitEdge initialHyphenateLimitAfter();
    static constexpr Style::HyphenateLimitLines initialHyphenateLimitLines();
    static inline Style::HyphenateCharacter initialHyphenateCharacter();
    static constexpr Resize initialResize();
    static constexpr StyleAppearance initialAppearance();
    static inline Style::AspectRatio initialAspectRatio();
    static constexpr OptionSet<Containment> initialContainment();
    static constexpr OptionSet<Containment> strictContainment();
    static constexpr OptionSet<Containment> contentContainment();
    static constexpr ContainerType initialContainerType();
    static Style::ContainerNames initialContainerNames();
    static inline Style::Content initialContent();
    static constexpr ContentVisibility initialContentVisibility();

    static inline Style::ContainIntrinsicSize initialContainIntrinsicWidth();
    static inline Style::ContainIntrinsicSize initialContainIntrinsicHeight();

    static constexpr Order initialRTLOrdering();
    static constexpr Style::WebkitTextStrokeWidth initialTextStrokeWidth();
    static constexpr Style::ColumnCount initialColumnCount();
    static constexpr ColumnFill initialColumnFill();
    static constexpr ColumnSpan initialColumnSpan();
    static inline Style::GapGutter initialColumnGap();
    static constexpr Style::ColumnWidth initialColumnWidth();
    static inline Style::GapGutter initialRowGap();
    static inline TransformOperations initialTransform();
    static inline Style::TransformOrigin initialTransformOrigin();
    static inline Style::TransformOriginX initialTransformOriginX();
    static inline Style::TransformOriginY initialTransformOriginY();
    static inline Style::TransformOriginZ initialTransformOriginZ();
    static constexpr TransformBox initialTransformBox();
    static inline Style::Rotate initialRotate();
    static inline Style::Scale initialScale();
    static inline Style::Translate initialTranslate();
    static constexpr PointerEvents initialPointerEvents();
    static constexpr TransformStyle3D initialTransformStyle3D();
    static constexpr BackfaceVisibility initialBackfaceVisibility();
    static inline Style::Perspective initialPerspective();
    static inline Style::PerspectiveOrigin initialPerspectiveOrigin();
    static inline Style::PerspectiveOriginX initialPerspectiveOriginX();
    static inline Style::PerspectiveOriginY initialPerspectiveOriginY();
    static inline Style::Color initialBackgroundColor();
    static inline Style::Color initialTextEmphasisColor();
    static inline Style::TextEmphasisStyle initialTextEmphasisStyle();
    static constexpr OptionSet<TextEmphasisPosition> initialTextEmphasisPosition();
    static constexpr RubyPosition initialRubyPosition();
    static constexpr RubyAlign initialRubyAlign();
    static constexpr RubyOverhang initialRubyOverhang();
    static constexpr OptionSet<Style::LineBoxContain> initialLineBoxContain();
    static constexpr ImageOrientation initialImageOrientation();
    static constexpr ImageRendering initialImageRendering();
    static inline Style::BorderImageSource initialBorderImageSource();
    static inline Style::MaskBorderSource initialMaskBorderSource();
    static constexpr PrintColorAdjust initialPrintColorAdjust();
    static inline Style::Quotes initialQuotes();
    static inline Style::SVGCoordinateComponent initialX();
    static inline Style::SVGCoordinateComponent initialY();

#if ENABLE(DARK_MODE_CSS)
    static inline Style::ColorScheme initialColorScheme();
#endif

    static inline Style::DynamicRangeLimit initialDynamicRangeLimit();

    static constexpr TextJustify initialTextJustify();

#if ENABLE(CURSOR_VISIBILITY)
    static constexpr CursorVisibility initialCursorVisibility();
#endif

#if ENABLE(TEXT_AUTOSIZING)
    static inline Length initialSpecifiedLineHeight();
    static constexpr Style::TextSizeAdjust initialTextSizeAdjust();
#endif

    static WillChangeData* initialWillChange() { return nullptr; }

    static constexpr TouchAction initialTouchActions();

    static constexpr FieldSizing initialFieldSizing();

    static inline Style::ScrollMarginEdge initialScrollMargin();
    static inline Style::ScrollPaddingEdge initialScrollPadding();

    static ScrollSnapType initialScrollSnapType();
    static ScrollSnapAlign initialScrollSnapAlign();
    static ScrollSnapStop initialScrollSnapStop();

    static inline Style::ProgressTimelineAxes initialScrollTimelineAxes();
    static inline Style::ProgressTimelineNames initialScrollTimelineNames();

    static inline Style::ProgressTimelineAxes initialViewTimelineAxes();
    static inline Style::ProgressTimelineNames initialViewTimelineNames();
    static inline Style::ViewTimelineInsets initialViewTimelineInsets();

    static inline Style::ScrollbarColor initialScrollbarColor();
    static constexpr Style::ScrollbarGutter initialScrollbarGutter();
    static constexpr ScrollbarWidth initialScrollbarWidth();

#if ENABLE(APPLE_PAY)
    static constexpr ApplePayButtonStyle initialApplePayButtonStyle();
    static constexpr ApplePayButtonType initialApplePayButtonType();
#endif

#if HAVE(CORE_MATERIAL)
    static constexpr AppleVisualEffect initialAppleVisualEffect();
#endif

    static constexpr GridAutoFlow initialGridAutoFlow();
    static inline Style::GridTrackSizes initialGridAutoColumns();
    static inline Style::GridTrackSizes initialGridAutoRows();
    static inline Style::GridTemplateAreas initialGridTemplateAreas();
    static inline Style::GridTemplateList initialGridTemplateColumns();
    static inline Style::GridTemplateList initialGridTemplateRows();

    static inline Style::GridPosition initialGridItemColumnStart();
    static inline Style::GridPosition initialGridItemColumnEnd();
    static inline Style::GridPosition initialGridItemRowStart();
    static inline Style::GridPosition initialGridItemRowEnd();

    static constexpr TabSize initialTabSize();

    static inline Style::WebkitLineGrid initialLineGrid();
    static constexpr LineSnap initialLineSnap();
    static constexpr LineAlign initialLineAlign();

    static constexpr FloatSize initialInitialLetter();
    static constexpr LineClampValue initialLineClamp();
    static inline Style::BlockEllipsis initialBlockEllipsis();
    static OverflowContinue initialOverflowContinue();
    static constexpr Style::MaximumLines initialMaxLines();
    static constexpr TextSecurity initialTextSecurity();
    static constexpr InputSecurity initialInputSecurity();

#if ENABLE(WEBKIT_TOUCH_CALLOUT_CSS_PROPERTY)
    static constexpr Style::WebkitTouchCallout initialTouchCallout();
#endif

#if ENABLE(TOUCH_EVENTS)
    static Style::Color initialTapHighlightColor();
#endif

#if ENABLE(WEBKIT_OVERFLOW_SCROLLING_CSS_PROPERTY)
    static constexpr Style::WebkitOverflowScrolling initialOverflowScrolling();
#endif

    static constexpr Style::ScrollBehavior initialScrollBehavior();

    static inline FilterOperations initialFilter();
    static inline FilterOperations initialAppleColorFilter();

    static inline FilterOperations initialBackdropFilter();

    static constexpr BlendMode initialBlendMode();
    static constexpr Isolation initialIsolation();

    static constexpr MathShift initialMathShift();
    static constexpr MathStyle initialMathStyle();

    void setVisitedLinkColor(Color&&);
    inline void setVisitedLinkBackgroundColor(Style::Color&&);
    inline void setVisitedLinkBorderLeftColor(Style::Color&&);
    inline void setVisitedLinkBorderRightColor(Style::Color&&);
    inline void setVisitedLinkBorderBottomColor(Style::Color&&);
    inline void setVisitedLinkBorderTopColor(Style::Color&&);
    inline void setVisitedLinkOutlineColor(Style::Color&&);
    inline void setVisitedLinkColumnRuleColor(Style::Color&&);
    inline void setVisitedLinkTextDecorationColor(Style::Color&&);
    inline void setVisitedLinkTextEmphasisColor(Style::Color&&);
    inline void setVisitedLinkTextFillColor(Style::Color&&);
    inline void setVisitedLinkTextStrokeColor(Style::Color&&);
    inline void setVisitedLinkCaretColor(Style::Color&&);
    inline void setHasVisitedLinkAutoCaretColor();

    void inheritUnicodeBidiFrom(const RenderStyle* parent) { m_nonInheritedFlags.unicodeBidi = parent->m_nonInheritedFlags.unicodeBidi; }

    inline const Style::Color& borderLeftColor() const;
    inline const Style::Color& borderRightColor() const;
    inline const Style::Color& borderTopColor() const;
    inline const Style::Color& borderBottomColor() const;
    inline const Style::Color& backgroundColor() const;
    WEBCORE_EXPORT const Color& color() const;
    inline const Style::Color& columnRuleColor() const;
    inline const Style::Color& outlineColor() const;
    inline const Style::Color& textEmphasisColor() const;
    inline const Style::Color& textFillColor() const;
    static inline Style::Color initialTextFillColor();
    inline const Style::Color& textStrokeColor() const;
    inline const Style::Color& caretColor() const;
    inline bool hasAutoCaretColor() const;
    const Color& visitedLinkColor() const;
    inline const Style::Color& visitedLinkBackgroundColor() const;
    inline const Style::Color& visitedLinkBorderLeftColor() const;
    inline const Style::Color& visitedLinkBorderRightColor() const;
    inline const Style::Color& visitedLinkBorderBottomColor() const;
    inline const Style::Color& visitedLinkBorderTopColor() const;
    inline const Style::Color& visitedLinkOutlineColor() const;
    inline const Style::Color& visitedLinkColumnRuleColor() const;
    inline const Style::Color& textDecorationColor() const;
    inline const Style::Color& visitedLinkTextDecorationColor() const;
    inline const Style::Color& visitedLinkTextEmphasisColor() const;
    inline const Style::Color& visitedLinkTextFillColor() const;
    inline const Style::Color& visitedLinkTextStrokeColor() const;
    inline const Style::Color& visitedLinkCaretColor() const;
    inline bool hasVisitedLinkAutoCaretColor() const;

    inline const Style::Color& stopColor() const;
    inline const Style::Color& floodColor() const;
    inline const Style::Color& lightingColor() const;

    Color usedAccentColor(OptionSet<StyleColorOptions>) const;
    inline const Style::Color& accentColor() const;
    inline bool hasAutoAccentColor() const;

    inline const Style::OffsetPath& offsetPath() const;
    inline bool hasOffsetPath() const;
    inline void setOffsetPath(Style::OffsetPath&&);
    static inline Style::OffsetPath initialOffsetPath();

    inline const Style::OffsetDistance& offsetDistance() const;
    inline void setOffsetDistance(Style::OffsetDistance&&);
    static inline Style::OffsetDistance initialOffsetDistance();

    inline const Style::OffsetPosition& offsetPosition() const;
    inline void setOffsetPosition(Style::OffsetPosition&&);
    static inline Style::OffsetPosition initialOffsetPosition();

    inline const Style::OffsetAnchor& offsetAnchor() const;
    inline void setOffsetAnchor(Style::OffsetAnchor&&);
    static inline Style::OffsetAnchor initialOffsetAnchor();

    inline const Style::OffsetRotate& offsetRotate() const;
    inline void setOffsetRotate(Style::OffsetRotate&&);
    static constexpr Style::OffsetRotate initialOffsetRotate();

    bool borderAndBackgroundEqual(const RenderStyle&) const;
    
    inline OverflowAnchor overflowAnchor() const;
    inline void setOverflowAnchor(OverflowAnchor);
    static constexpr OverflowAnchor initialOverflowAnchor();

    static inline Style::BlockStepSize initialBlockStepSize();
    inline const Style::BlockStepSize& blockStepSize() const;
    inline void setBlockStepSize(Style::BlockStepSize&&);

    static constexpr BlockStepAlign initialBlockStepAlign();
    inline BlockStepAlign blockStepAlign() const;
    inline void setBlockStepAlign(BlockStepAlign);

    static constexpr BlockStepInsert initialBlockStepInsert();
    inline BlockStepInsert blockStepInsert() const;
    inline void setBlockStepInsert(BlockStepInsert);

    static constexpr BlockStepRound initialBlockStepRound();
    inline BlockStepRound blockStepRound() const;
    inline void setBlockStepRound(BlockStepRound);

    bool scrollAnchoringSuppressionStyleDidChange(const RenderStyle*) const;
    bool outOfFlowPositionStyleDidChange(const RenderStyle*) const;

    static Style::AnchorNames initialAnchorNames();
    inline const Style::AnchorNames& anchorNames() const;
    inline void setAnchorNames(Style::AnchorNames&&);

    static inline NameScope initialAnchorScope();
    inline const NameScope& anchorScope() const;
    inline void setAnchorScope(const NameScope&);

    static inline std::optional<Style::ScopedName> initialPositionAnchor();
    inline const std::optional<Style::ScopedName>& positionAnchor() const;
    inline void setPositionAnchor(const std::optional<Style::ScopedName>&);

    static inline std::optional<PositionArea> initialPositionArea();
    inline std::optional<PositionArea> positionArea() const;
    inline void setPositionArea(std::optional<PositionArea>);

    static constexpr Style::PositionTryOrder initialPositionTryOrder();
    inline Style::PositionTryOrder positionTryOrder() const;
    inline void setPositionTryOrder(Style::PositionTryOrder);

    static FixedVector<Style::PositionTryFallback> initialPositionTryFallbacks();
    const FixedVector<Style::PositionTryFallback>& positionTryFallbacks() const;
    void setPositionTryFallbacks(FixedVector<Style::PositionTryFallback>&&);

    std::optional<size_t> lastSuccessfulPositionTryFallbackIndex() const;
    void setLastSuccessfulPositionTryFallbackIndex(std::optional<size_t>&&);

    static constexpr OptionSet<PositionVisibility> initialPositionVisibility();
    inline OptionSet<PositionVisibility> positionVisibility() const;
    inline void setPositionVisibility(OptionSet<PositionVisibility>);

    inline bool insideDefaultButton() const;
    inline void setInsideDefaultButton(bool);

    inline bool insideSubmitButton() const;
    inline void setInsideSubmitButton(bool);

private:
    struct NonInheritedFlags {
        bool operator==(const NonInheritedFlags&) const = default;

        inline void copyNonInheritedFrom(const NonInheritedFlags&);

        inline bool hasAnyPublicPseudoStyles() const;
        bool hasPseudoStyle(PseudoId) const;
        void setHasPseudoStyles(PseudoIdSet);

#if !LOG_DISABLED
        void dumpDifferences(TextStream&, const NonInheritedFlags&) const;
#endif

        PREFERRED_TYPE(DisplayType) unsigned effectiveDisplay : 5;
        PREFERRED_TYPE(DisplayType) unsigned originalDisplay : 5;
        PREFERRED_TYPE(Overflow) unsigned overflowX : 3;
        PREFERRED_TYPE(Overflow) unsigned overflowY : 3;
        PREFERRED_TYPE(Clear) unsigned clear : 3;
        PREFERRED_TYPE(PositionType) unsigned position : 3;
        PREFERRED_TYPE(UnicodeBidi) unsigned unicodeBidi : 3;
        PREFERRED_TYPE(Float) unsigned floating : 3;

        PREFERRED_TYPE(bool) unsigned usesViewportUnits : 1;
        PREFERRED_TYPE(bool) unsigned usesContainerUnits : 1;
        PREFERRED_TYPE(bool) unsigned useTreeCountingFunctions : 1;
        PREFERRED_TYPE(bool) unsigned hasExplicitlyInheritedProperties : 1; // Explicitly inherits a non-inherited property.
        PREFERRED_TYPE(bool) unsigned disallowsFastPathInheritance : 1;

        // Non-property related state bits.
        PREFERRED_TYPE(bool) unsigned emptyState : 1;
        PREFERRED_TYPE(bool) unsigned firstChildState : 1;
        PREFERRED_TYPE(bool) unsigned lastChildState : 1;
        PREFERRED_TYPE(bool) unsigned isLink : 1;
        PREFERRED_TYPE(PseudoId) unsigned pseudoElementType : PseudoElementTypeBits;
        unsigned pseudoBits : PublicPseudoIDBits;
        PREFERRED_TYPE(OptionSet<TextDecorationLine>) unsigned textDecorationLine : TextDecorationLineBits; // Text decorations defined *only* by this element.

        // If you add more style bits here, you will also need to update RenderStyle::NonInheritedFlags::copyNonInheritedFrom().
    };

    struct InheritedFlags {
        bool operator==(const InheritedFlags&) const = default;

#if !LOG_DISABLED
        void dumpDifferences(TextStream&, const InheritedFlags&) const;
#endif

        // Writing Mode = 8 bits (can be packed into 6 if needed)
        WritingMode writingMode;

        // Text Formatting = 19 bits aligned onto 2 bytes + 4 trailing bits
        PREFERRED_TYPE(WhiteSpaceCollapse) unsigned char whiteSpaceCollapse : 3;
        PREFERRED_TYPE(TextWrapMode) unsigned char textWrapMode : 1;
        PREFERRED_TYPE(TextAlignMode) unsigned char textAlign : 4;
        PREFERRED_TYPE(TextWrapStyle) unsigned char textWrapStyle : 2;
        PREFERRED_TYPE(OptionSet<TextTransform>) unsigned char textTransform : TextTransformBits;
        unsigned char : 1; // byte alignment
        PREFERRED_TYPE(OptionSet<TextDecorationLine>) unsigned char textDecorationLineInEffect : TextDecorationLineBits;

        // Cursors and Visibility = 13 bits aligned onto 4 bits + 1 byte + 1 bit
        PREFERRED_TYPE(PointerEvents) unsigned char pointerEvents : 4;
        PREFERRED_TYPE(Visibility) unsigned char visibility : 2;
        PREFERRED_TYPE(CursorType) unsigned char cursorType : 6;
#if ENABLE(CURSOR_VISIBILITY)
        PREFERRED_TYPE(CursorVisibility) unsigned char cursorVisibility : 1;
#endif

        // Display Type-Specific = 5 bits
        PREFERRED_TYPE(ListStylePosition) unsigned char listStylePosition : 1;
        PREFERRED_TYPE(EmptyCell) unsigned char emptyCells : 1;
        PREFERRED_TYPE(BorderCollapse) unsigned char borderCollapse : 1;
        PREFERRED_TYPE(CaptionSide) unsigned char captionSide : 2;

        // -webkit- Stuff = 2 bits
        PREFERRED_TYPE(BoxDirection) unsigned char boxDirection : 1;
        PREFERRED_TYPE(Order) unsigned char rtlOrdering : 1;

        // Color Stuff = 4 bits
        PREFERRED_TYPE(bool) unsigned char hasExplicitlySetColor : 1;
        PREFERRED_TYPE(PrintColorAdjust) unsigned char printColorAdjust : 1;
        PREFERRED_TYPE(InsideLink) unsigned char insideLink : 2;

#if ENABLE(TEXT_AUTOSIZING)
        unsigned autosizeStatus : 5;
#endif
        // Total = 56 bits (fits in 8 bytes)
    };

    // This constructor is used to implement the replace operation.
    RenderStyle(RenderStyle&, RenderStyle&&);

    constexpr DisplayType originalDisplay() const { return static_cast<DisplayType>(m_nonInheritedFlags.originalDisplay); }

    const Style::Color& unresolvedColorForProperty(CSSPropertyID, bool visitedLink = false) const;

    inline bool hasAutoLeftAndRight() const;
    inline bool hasAutoTopAndBottom() const;

    static constexpr bool isDisplayInlineType(DisplayType);
    static constexpr bool isDisplayBlockType(DisplayType);
    static constexpr bool isDisplayFlexibleBox(DisplayType);
    static constexpr bool isDisplayGridBox(DisplayType);
    static constexpr bool isDisplayFlexibleOrGridBox(DisplayType);
    static constexpr bool isDisplayDeprecatedFlexibleBox(DisplayType);
    static constexpr bool isDisplayListItemType(DisplayType);
    static constexpr bool isDisplayTableOrTablePart(DisplayType);
    static constexpr bool isInternalTableBox(DisplayType);
    static constexpr bool isRubyContainerOrInternalRubyBox(DisplayType);

    bool changeAffectsVisualOverflow(const RenderStyle&) const;
    bool changeRequiresLayout(const RenderStyle&, OptionSet<StyleDifferenceContextSensitiveProperty>& changedContextSensitiveProperties) const;
    bool changeRequiresOutOfFlowMovementLayoutOnly(const RenderStyle&, OptionSet<StyleDifferenceContextSensitiveProperty>& changedContextSensitiveProperties) const;
    bool changeRequiresLayerRepaint(const RenderStyle&, OptionSet<StyleDifferenceContextSensitiveProperty>& changedContextSensitiveProperties) const;
    bool changeRequiresRepaint(const RenderStyle&, OptionSet<StyleDifferenceContextSensitiveProperty>& changedContextSensitiveProperties) const;
    bool changeRequiresRepaintIfText(const RenderStyle&, OptionSet<StyleDifferenceContextSensitiveProperty>& changedContextSensitiveProperties) const;
    bool changeRequiresRecompositeLayer(const RenderStyle&, OptionSet<StyleDifferenceContextSensitiveProperty>& changedContextSensitiveProperties) const;

    // non-inherited attributes
    DataRef<StyleNonInheritedData> m_nonInheritedData;
    NonInheritedFlags m_nonInheritedFlags;

    // inherited attributes
    DataRef<StyleRareInheritedData> m_rareInheritedData;
    DataRef<StyleInheritedData> m_inheritedData;
    InheritedFlags m_inheritedFlags;

    // list of associated pseudo styles
    std::unique_ptr<PseudoStyleCache> m_cachedPseudoStyles;

    DataRef<SVGRenderStyle> m_svgStyle;

#if ASSERT_ENABLED || ENABLE(SECURITY_ASSERTIONS)
    bool m_deletionHasBegun { false };
#endif
};

// Map from computed style values (which take zoom into account) to web-exposed values, which are zoom-independent.
inline int adjustForAbsoluteZoom(int, const RenderStyle&);
inline float adjustFloatForAbsoluteZoom(float, const RenderStyle&);
inline LayoutUnit adjustLayoutUnitForAbsoluteZoom(LayoutUnit, const RenderStyle&);
inline LayoutSize adjustLayoutSizeForAbsoluteZoom(LayoutSize, const RenderStyle&);

// Map from zoom-independent style values to computed style values (which take zoom into account).
inline float applyZoom(float, const RenderStyle&);

constexpr BorderStyle collapsedBorderStyle(BorderStyle);

inline bool pseudoElementRendererIsNeeded(const RenderStyle*);
inline bool generatesBox(const RenderStyle&);
inline bool isNonVisibleOverflow(Overflow);

inline bool isVisibleToHitTesting(const RenderStyle&, const HitTestRequest&);

inline bool shouldApplyLayoutContainment(const RenderStyle&, const Element&);
inline bool shouldApplySizeContainment(const RenderStyle&, const Element&);
inline bool shouldApplyInlineSizeContainment(const RenderStyle&, const Element&);
inline bool shouldApplyStyleContainment(const RenderStyle&, const Element&);
inline bool shouldApplyPaintContainment(const RenderStyle&, const Element&);
inline bool isSkippedContentRoot(const RenderStyle&, const Element&);

} // namespace WebCore
