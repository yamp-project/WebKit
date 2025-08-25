/*
    Copyright (C) 2004, 2005, 2007 Nikolas Zimmermann <zimmermann@kde.org>
                  2004, 2005 Rob Buis <buis@kde.org>
    Copyright (C) 2005-2017 Apple Inc. All rights reserved.
    Copyright (C) Research In Motion Limited 2010. All rights reserved.
    Copyright (C) 2014 Adobe Systems Incorporated. All rights reserved.

    This library is free software; you can redistribute it and/or
    modify it under the terms of the GNU Library General Public
    License as published by the Free Software Foundation; either
    version 2 of the License, or (at your option) any later version.

    This library is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
    Library General Public License for more details.

    You should have received a copy of the GNU Library General Public License
    along with this library; see the file COPYING.LIB.  If not, write to
    the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
    Boston, MA 02110-1301, USA.
*/

#pragma once

#include "StyleSVGPaint.h"
#include <WebCore/RenderStyle.h>
#include <WebCore/RenderStyleConstants.h>
#include <WebCore/SVGRenderStyleDefs.h>
#include <WebCore/StyleRareInheritedData.h>
#include <WebCore/StyleURL.h>
#include <WebCore/WindRule.h>

namespace WTF {
class TextStream;
}

namespace WebCore {

using namespace CSS::Literals;

DECLARE_ALLOCATOR_WITH_HEAP_IDENTIFIER(SVGRenderStyle);
class SVGRenderStyle : public RefCounted<SVGRenderStyle> {
    WTF_DEPRECATED_MAKE_FAST_ALLOCATED_WITH_HEAP_IDENTIFIER(SVGRenderStyle, SVGRenderStyle);
public:
    static Ref<SVGRenderStyle> createDefaultStyle();
    static Ref<SVGRenderStyle> create() { return adoptRef(*new SVGRenderStyle); }
    Ref<SVGRenderStyle> copy() const;
    ~SVGRenderStyle();

    bool inheritedEqual(const SVGRenderStyle&) const;
    bool nonInheritedEqual(const SVGRenderStyle&) const;

    void inheritFrom(const SVGRenderStyle&);
    void copyNonInheritedFrom(const SVGRenderStyle&);

    bool changeRequiresRepaint(const SVGRenderStyle& other, bool currentColorDiffers) const;
    bool changeRequiresLayout(const SVGRenderStyle& other) const;

    bool operator==(const SVGRenderStyle&) const;

#if !LOG_DISABLED
    void dumpDifferences(TextStream&, const SVGRenderStyle&) const;
#endif

    // Initial values for all the properties
    static AlignmentBaseline initialAlignmentBaseline() { return AlignmentBaseline::Baseline; }
    static DominantBaseline initialDominantBaseline() { return DominantBaseline::Auto; }
    static VectorEffect initialVectorEffect() { return VectorEffect::None; }
    static BufferedRendering initialBufferedRendering() { return BufferedRendering::Auto; }
    static WindRule initialClipRule() { return WindRule::NonZero; }
    static ColorInterpolation initialColorInterpolation() { return ColorInterpolation::SRGB; }
    static ColorInterpolation initialColorInterpolationFilters() { return ColorInterpolation::LinearRGB; }
    static ColorRendering initialColorRendering() { return ColorRendering::Auto; }
    static WindRule initialFillRule() { return WindRule::NonZero; }
    static ShapeRendering initialShapeRendering() { return ShapeRendering::Auto; }
    static TextAnchor initialTextAnchor() { return TextAnchor::Start; }
    static GlyphOrientation initialGlyphOrientationHorizontal() { return GlyphOrientation::Degrees0; }
    static GlyphOrientation initialGlyphOrientationVertical() { return GlyphOrientation::Auto; }
    static constexpr Style::Opacity initialFillOpacity();
    static Style::SVGPaint initialFill() { return Style::SVGPaint { Style::SVGPaintType::RGBColor, Style::URL::none(), Color::black }; }
    static constexpr Style::Opacity initialStrokeOpacity();
    static Style::SVGPaint initialStroke() { return Style::SVGPaint { Style::SVGPaintType::None, Style::URL::none(), Color { } }; }
    static inline Style::SVGStrokeDasharray initialStrokeDashArray();
    static inline Style::SVGStrokeDashoffset initialStrokeDashOffset();
    static constexpr Style::Opacity initialStopOpacity();
    static Style::Color initialStopColor() { return Color::black; }
    static constexpr Style::Opacity initialFloodOpacity();
    static Style::Color initialFloodColor() { return Color::black; }
    static Style::Color initialLightingColor() { return Color::white; }
    static Style::URL initialMarkerStartResource() { return Style::URL::none(); }
    static Style::URL initialMarkerMidResource() { return Style::URL::none(); }
    static Style::URL initialMarkerEndResource() { return Style::URL::none(); }
    static MaskType initialMaskType() { return MaskType::Luminance; }
    static Style::SVGBaselineShift initialBaselineShift() { return CSS::Keyword::Baseline { }; }

    // SVG CSS Property setters
    void setAlignmentBaseline(AlignmentBaseline val) { m_nonInheritedFlags.flagBits.alignmentBaseline = static_cast<unsigned>(val); }
    void setDominantBaseline(DominantBaseline val) { m_nonInheritedFlags.flagBits.dominantBaseline = static_cast<unsigned>(val); }
    void setVectorEffect(VectorEffect val) { m_nonInheritedFlags.flagBits.vectorEffect = static_cast<unsigned>(val); }
    void setBufferedRendering(BufferedRendering val) { m_nonInheritedFlags.flagBits.bufferedRendering = static_cast<unsigned>(val); }
    void setClipRule(WindRule val) { m_inheritedFlags.clipRule = static_cast<unsigned>(val); }
    void setColorInterpolation(ColorInterpolation val) { m_inheritedFlags.colorInterpolation = static_cast<unsigned>(val); }
    void setColorInterpolationFilters(ColorInterpolation val) { m_inheritedFlags.colorInterpolationFilters = static_cast<unsigned>(val); }
    void setFillRule(WindRule val) { m_inheritedFlags.fillRule = static_cast<unsigned>(val); }
    void setShapeRendering(ShapeRendering val) { m_inheritedFlags.shapeRendering = static_cast<unsigned>(val); }
    void setTextAnchor(TextAnchor val) { m_inheritedFlags.textAnchor = static_cast<unsigned>(val); }
    void setGlyphOrientationHorizontal(GlyphOrientation val) { m_inheritedFlags.glyphOrientationHorizontal = static_cast<unsigned>(val); }
    void setGlyphOrientationVertical(GlyphOrientation val) { m_inheritedFlags.glyphOrientationVertical = static_cast<unsigned>(val); }
    void setMaskType(MaskType val) { m_nonInheritedFlags.flagBits.maskType = static_cast<unsigned>(val); }
    void setCx(Style::SVGCenterCoordinateComponent&&);
    void setCy(Style::SVGCenterCoordinateComponent&&);
    void setR(Style::SVGRadius&&);
    void setRx(Style::SVGRadiusComponent&&);
    void setRy(Style::SVGRadiusComponent&&);
    void setX(Style::SVGCoordinateComponent&&);
    void setY(Style::SVGCoordinateComponent&&);
    void setD(RefPtr<StylePathData>&&);
    void setFillOpacity(Style::Opacity);
    void setFill(Style::SVGPaint&&);
    void setVisitedLinkFill(Style::SVGPaint&&);
    void setStrokeOpacity(Style::Opacity);
    void setStroke(Style::SVGPaint&&);
    void setVisitedLinkStroke(Style::SVGPaint&&);

    void setStrokeDashArray(Style::SVGStrokeDasharray&&);
    void setStrokeDashOffset(Style::SVGStrokeDashoffset&&);
    void setStopOpacity(Style::Opacity);
    void setStopColor(Style::Color&&);
    void setFloodOpacity(Style::Opacity);
    void setFloodColor(Style::Color&&);
    void setLightingColor(Style::Color&&);
    void setBaselineShift(Style::SVGBaselineShift&&);

    // Setters for inherited resources
    void setMarkerStartResource(Style::URL&&);
    void setMarkerMidResource(Style::URL&&);
    void setMarkerEndResource(Style::URL&&);

    // Read accessors for all the properties
    AlignmentBaseline alignmentBaseline() const { return static_cast<AlignmentBaseline>(m_nonInheritedFlags.flagBits.alignmentBaseline); }
    DominantBaseline dominantBaseline() const { return static_cast<DominantBaseline>(m_nonInheritedFlags.flagBits.dominantBaseline); }
    VectorEffect vectorEffect() const { return static_cast<VectorEffect>(m_nonInheritedFlags.flagBits.vectorEffect); }
    BufferedRendering bufferedRendering() const { return static_cast<BufferedRendering>(m_nonInheritedFlags.flagBits.bufferedRendering); }
    WindRule clipRule() const { return static_cast<WindRule>(m_inheritedFlags.clipRule); }
    ColorInterpolation colorInterpolation() const { return static_cast<ColorInterpolation>(m_inheritedFlags.colorInterpolation); }
    ColorInterpolation colorInterpolationFilters() const { return static_cast<ColorInterpolation>(m_inheritedFlags.colorInterpolationFilters); }
    WindRule fillRule() const { return static_cast<WindRule>(m_inheritedFlags.fillRule); }
    ShapeRendering shapeRendering() const { return static_cast<ShapeRendering>(m_inheritedFlags.shapeRendering); }
    TextAnchor textAnchor() const { return static_cast<TextAnchor>(m_inheritedFlags.textAnchor); }
    GlyphOrientation glyphOrientationHorizontal() const { return static_cast<GlyphOrientation>(m_inheritedFlags.glyphOrientationHorizontal); }
    GlyphOrientation glyphOrientationVertical() const { return static_cast<GlyphOrientation>(m_inheritedFlags.glyphOrientationVertical); }
    const Style::SVGPaint& fill() const { return m_fillData->paint; }
    Style::Opacity fillOpacity() const { return m_fillData->opacity; }
    const Style::SVGPaint& stroke() const { return m_strokeData->paint; }
    Style::Opacity strokeOpacity() const { return m_strokeData->opacity; }
    const Style::SVGStrokeDasharray& strokeDashArray() const { return m_strokeData->dashArray; }
    const Style::SVGStrokeDashoffset& strokeDashOffset() const { return m_strokeData->dashOffset; }
    Style::Opacity stopOpacity() const { return m_stopData->opacity; }
    const Style::Color& stopColor() const { return m_stopData->color; }
    Style::Opacity floodOpacity() const { return m_miscData->floodOpacity; }
    const Style::Color& floodColor() const { return m_miscData->floodColor; }
    const Style::Color& lightingColor() const { return m_miscData->lightingColor; }
    const Style::SVGBaselineShift& baselineShift() const { return m_miscData->baselineShift; }
    const Style::SVGCenterCoordinateComponent& cx() const { return m_layoutData->cx; }
    const Style::SVGCenterCoordinateComponent& cy() const { return m_layoutData->cy; }
    const Style::SVGRadius& r() const { return m_layoutData->r; }
    const Style::SVGRadiusComponent& rx() const { return m_layoutData->rx; }
    const Style::SVGRadiusComponent& ry() const { return m_layoutData->ry; }
    const Style::SVGCoordinateComponent& x() const { return m_layoutData->x; }
    const Style::SVGCoordinateComponent& y() const { return m_layoutData->y; }
    StylePathData* d() const { return m_layoutData->d.get(); }
    const Style::URL& markerStartResource() const { return m_inheritedResourceData->markerStart; }
    const Style::URL& markerMidResource() const { return m_inheritedResourceData->markerMid; }
    const Style::URL& markerEndResource() const { return m_inheritedResourceData->markerEnd; }
    MaskType maskType() const { return static_cast<MaskType>(m_nonInheritedFlags.flagBits.maskType); }
    const Style::SVGPaint& visitedLinkFill() const { return m_fillData->visitedLinkPaint; }
    const Style::SVGPaint& visitedLinkStroke() const { return m_strokeData->visitedLinkPaint; }

    // convenience
    bool hasMarkers() const { return !markerStartResource().isNone() || !markerMidResource().isNone() || !markerEndResource().isNone(); }
    bool hasStroke() const { return stroke().type != Style::SVGPaintType::None; }
    bool hasFill() const { return fill().type != Style::SVGPaintType::None; }

    void conservativelyCollectChangedAnimatableProperties(const SVGRenderStyle&, CSSPropertiesBitSet&) const;

private:
    SVGRenderStyle();
    SVGRenderStyle(const SVGRenderStyle&);

    enum CreateDefaultType { CreateDefault };
    SVGRenderStyle(CreateDefaultType); // Used to create the default style.

    void setBitDefaults();

    struct InheritedFlags {
        friend bool operator==(const InheritedFlags&, const InheritedFlags&) = default;

#if !LOG_DISABLED
        void dumpDifferences(TextStream&, const InheritedFlags&) const;
#endif

        unsigned shapeRendering : 2; // ShapeRendering
        unsigned clipRule : 1; // WindRule
        unsigned fillRule : 1; // WindRule
        unsigned textAnchor : 2; // TextAnchor
        unsigned colorInterpolation : 2; // ColorInterpolation
        unsigned colorInterpolationFilters : 2; // ColorInterpolation
        unsigned glyphOrientationHorizontal : 3; // GlyphOrientation
        unsigned glyphOrientationVertical : 3; // GlyphOrientation
    };

    struct NonInheritedFlags {
        // 32 bit non-inherited, don't add to the struct, or the operator will break.
        bool operator==(const NonInheritedFlags& other) const { return flags == other.flags; }

#if !LOG_DISABLED
        void dumpDifferences(TextStream&, const NonInheritedFlags&) const;
#endif

        union {
            struct {
                unsigned alignmentBaseline : 4; // AlignmentBaseline
                unsigned dominantBaseline : 4; // DominantBaseline
                unsigned vectorEffect : 1; // VectorEffect
                unsigned bufferedRendering : 2; // BufferedRendering
                unsigned maskType : 1; // MaskType
                // 20 bits unused
            } flagBits;
            uint32_t flags;
        };
    };

    InheritedFlags m_inheritedFlags;
    NonInheritedFlags m_nonInheritedFlags;

    // inherited attributes
    DataRef<StyleFillData> m_fillData;
    DataRef<StyleStrokeData> m_strokeData;
    DataRef<StyleInheritedResourceData> m_inheritedResourceData;

    // non-inherited attributes
    DataRef<StyleStopData> m_stopData;
    DataRef<StyleMiscData> m_miscData;
    DataRef<StyleLayoutData> m_layoutData;
};

inline SVGRenderStyle& RenderStyle::accessSVGStyle() { return m_svgStyle.access(); }
inline const Style::SVGBaselineShift& RenderStyle::baselineShift() const { return svgStyle().baselineShift(); }
inline const Style::SVGCenterCoordinateComponent& RenderStyle::cx() const { return svgStyle().cx(); }
inline const Style::SVGCenterCoordinateComponent& RenderStyle::cy() const { return svgStyle().cy(); }
inline StylePathData* RenderStyle::d() const { return svgStyle().d(); }
inline Style::Opacity RenderStyle::fillOpacity() const { return svgStyle().fillOpacity(); }
inline const Style::SVGPaint& RenderStyle::fill() const { return svgStyle().fill(); }
inline const Style::SVGPaint& RenderStyle::visitedLinkFill() const { return svgStyle().visitedLinkFill(); }
inline const Style::Color& RenderStyle::floodColor() const { return svgStyle().floodColor(); }
inline Style::Opacity RenderStyle::floodOpacity() const { return svgStyle().floodOpacity(); }
inline bool RenderStyle::hasExplicitlySetStrokeWidth() const { return m_rareInheritedData->hasSetStrokeWidth; }
inline bool RenderStyle::hasVisibleStroke() const { return svgStyle().hasStroke() && !strokeWidth().isZero(); }
inline const Style::Color& RenderStyle::lightingColor() const { return svgStyle().lightingColor(); }
inline const Style::SVGRadius& RenderStyle::r() const { return svgStyle().r(); }
inline const Style::SVGRadiusComponent& RenderStyle::rx() const { return svgStyle().rx(); }
inline const Style::SVGRadiusComponent& RenderStyle::ry() const { return svgStyle().ry(); }
inline void RenderStyle::setBaselineShift(Style::SVGBaselineShift&& baselineShift) { accessSVGStyle().setBaselineShift(WTFMove(baselineShift)); }
inline void RenderStyle::setCx(Style::SVGCenterCoordinateComponent&& cx) { accessSVGStyle().setCx(WTFMove(cx)); }
inline void RenderStyle::setCy(Style::SVGCenterCoordinateComponent&& cy) { accessSVGStyle().setCy(WTFMove(cy)); }
inline void RenderStyle::setD(RefPtr<StylePathData>&& d) { accessSVGStyle().setD(WTFMove(d)); }
inline void RenderStyle::setFillOpacity(Style::Opacity opacity) { accessSVGStyle().setFillOpacity(opacity); }
inline void RenderStyle::setFill(Style::SVGPaint&& paint) { accessSVGStyle().setFill(WTFMove(paint)); }
inline void RenderStyle::setVisitedLinkFill(Style::SVGPaint&& paint) { accessSVGStyle().setVisitedLinkFill(WTFMove(paint)); }
inline void RenderStyle::setFloodColor(Style::Color&& c) { accessSVGStyle().setFloodColor(WTFMove(c)); }
inline void RenderStyle::setFloodOpacity(Style::Opacity opacity) { accessSVGStyle().setFloodOpacity(opacity); }
inline void RenderStyle::setLightingColor(Style::Color&& c) { accessSVGStyle().setLightingColor(WTFMove(c)); }
inline void RenderStyle::setR(Style::SVGRadius&& r) { accessSVGStyle().setR(WTFMove(r)); }
inline void RenderStyle::setRx(Style::SVGRadiusComponent&& rx) { accessSVGStyle().setRx(WTFMove(rx)); }
inline void RenderStyle::setRy(Style::SVGRadiusComponent&& ry) { accessSVGStyle().setRy(WTFMove(ry)); }
inline void RenderStyle::setStopColor(Style::Color&& c) { accessSVGStyle().setStopColor(WTFMove(c)); }
inline void RenderStyle::setStopOpacity(Style::Opacity opacity) { accessSVGStyle().setStopOpacity(opacity); }
inline void RenderStyle::setStrokeDashArray(Style::SVGStrokeDasharray&& array) { accessSVGStyle().setStrokeDashArray(WTFMove(array)); }
inline void RenderStyle::setStrokeDashOffset(Style::SVGStrokeDashoffset&& offset) { accessSVGStyle().setStrokeDashOffset(WTFMove(offset)); }
inline void RenderStyle::setStrokeOpacity(Style::Opacity opacity) { accessSVGStyle().setStrokeOpacity(opacity); }
inline void RenderStyle::setStroke(Style::SVGPaint&& paint) { accessSVGStyle().setStroke(WTFMove(paint)); }
inline void RenderStyle::setVisitedLinkStroke(Style::SVGPaint&& paint) { accessSVGStyle().setVisitedLinkStroke(WTFMove(paint)); }
inline void RenderStyle::setX(Style::SVGCoordinateComponent&& x) { accessSVGStyle().setX(WTFMove(x)); }
inline void RenderStyle::setY(Style::SVGCoordinateComponent&& y) { accessSVGStyle().setY(WTFMove(y)); }
inline const Style::Color& RenderStyle::stopColor() const { return svgStyle().stopColor(); }
inline Style::Opacity RenderStyle::stopOpacity() const { return svgStyle().stopOpacity(); }
inline const Style::SVGStrokeDasharray& RenderStyle::strokeDashArray() const { return svgStyle().strokeDashArray(); }
inline const Style::SVGStrokeDashoffset& RenderStyle::strokeDashOffset() const { return svgStyle().strokeDashOffset(); }
inline Style::Opacity RenderStyle::strokeOpacity() const { return svgStyle().strokeOpacity(); }
inline const Style::SVGPaint& RenderStyle::stroke() const { return svgStyle().stroke(); }
inline const Style::SVGPaint& RenderStyle::visitedLinkStroke() const { return svgStyle().visitedLinkStroke(); }
inline const Style::StrokeWidth& RenderStyle::strokeWidth() const { return m_rareInheritedData->strokeWidth; }
inline const Style::SVGCoordinateComponent& RenderStyle::x() const { return svgStyle().x(); }
inline const Style::SVGCoordinateComponent& RenderStyle::y() const { return svgStyle().y(); }

inline Style::SVGCenterCoordinateComponent RenderStyle::initialCx() { return 0_css_px; }
inline Style::SVGCenterCoordinateComponent RenderStyle::initialCy() { return 0_css_px; }
inline Style::SVGRadius RenderStyle::initialR() { return 0_css_px; }
inline Style::SVGRadiusComponent RenderStyle::initialRx() { return CSS::Keyword::Auto { }; }
inline Style::SVGRadiusComponent RenderStyle::initialRy() { return CSS::Keyword::Auto { }; }
inline Style::SVGCoordinateComponent RenderStyle::initialX() { return 0_css_px; }
inline Style::SVGCoordinateComponent RenderStyle::initialY() { return 0_css_px; }
inline Style::SVGStrokeDasharray RenderStyle::initialStrokeDashArray() { return CSS::Keyword::None { }; }
inline Style::SVGStrokeDashoffset RenderStyle::initialStrokeDashOffset() { return 0_css_px; }
constexpr Style::Opacity RenderStyle::initialFillOpacity() { return 1_css_number; }
constexpr Style::Opacity RenderStyle::initialStrokeOpacity() { return 1_css_number; }
constexpr Style::Opacity RenderStyle::initialStopOpacity() { return 1_css_number; }
constexpr Style::Opacity RenderStyle::initialFloodOpacity() { return 1_css_number; }

inline Style::SVGStrokeDasharray SVGRenderStyle::initialStrokeDashArray() { return RenderStyle::initialStrokeDashArray(); }
inline Style::SVGStrokeDashoffset SVGRenderStyle::initialStrokeDashOffset() { return RenderStyle::initialStrokeDashOffset(); }
constexpr Style::Opacity SVGRenderStyle::initialFillOpacity() { return RenderStyle::initialFillOpacity(); }
constexpr Style::Opacity SVGRenderStyle::initialStrokeOpacity() { return RenderStyle::initialStrokeOpacity(); }
constexpr Style::Opacity SVGRenderStyle::initialStopOpacity() { return RenderStyle::initialStopOpacity(); }
constexpr Style::Opacity SVGRenderStyle::initialFloodOpacity() { return RenderStyle::initialFloodOpacity(); }

inline void SVGRenderStyle::setCx(Style::SVGCenterCoordinateComponent&& length)
{
    if (!(m_layoutData->cx == length))
        m_layoutData.access().cx = WTFMove(length);
}

inline void SVGRenderStyle::setCy(Style::SVGCenterCoordinateComponent&& length)
{
    if (!(m_layoutData->cy == length))
        m_layoutData.access().cy = WTFMove(length);
}

inline void SVGRenderStyle::setR(Style::SVGRadius&& length)
{
    if (!(m_layoutData->r == length))
        m_layoutData.access().r = WTFMove(length);
}

inline void SVGRenderStyle::setRx(Style::SVGRadiusComponent&& length)
{
    if (!(m_layoutData->rx == length))
        m_layoutData.access().rx = WTFMove(length);
}

inline void SVGRenderStyle::setRy(Style::SVGRadiusComponent&& length)
{
    if (!(m_layoutData->ry == length))
        m_layoutData.access().ry = WTFMove(length);
}

inline void SVGRenderStyle::setX(Style::SVGCoordinateComponent&& length)
{
    if (!(m_layoutData->x == length))
        m_layoutData.access().x = WTFMove(length);
}

inline void SVGRenderStyle::setY(Style::SVGCoordinateComponent&& length)
{
    if (!(m_layoutData->y == length))
        m_layoutData.access().y = WTFMove(length);
}

inline void SVGRenderStyle::setD(RefPtr<StylePathData>&& d)
{
    if (!(m_layoutData->d == d))
        m_layoutData.access().d = WTFMove(d);
}

inline void SVGRenderStyle::setFillOpacity(Style::Opacity opacity)
{
    if (!(m_fillData->opacity == opacity))
        m_fillData.access().opacity = opacity;
}

inline void SVGRenderStyle::setFill(Style::SVGPaint&& paint)
{
    if (m_fillData->paint != paint)
        m_fillData.access().paint = WTFMove(paint);
}

inline void SVGRenderStyle::setVisitedLinkFill(Style::SVGPaint&& paint)
{
    if (m_fillData->visitedLinkPaint != paint)
        m_fillData.access().visitedLinkPaint = WTFMove(paint);
}

inline void SVGRenderStyle::setStrokeOpacity(Style::Opacity opacity)
{
    if (!(m_strokeData->opacity == opacity))
        m_strokeData.access().opacity = opacity;
}

inline void SVGRenderStyle::setStroke(Style::SVGPaint&& paint)
{
    if (m_strokeData->paint != paint)
        m_strokeData.access().paint = WTFMove(paint);
}

inline void SVGRenderStyle::setVisitedLinkStroke(Style::SVGPaint&& paint)
{
    if (m_strokeData->visitedLinkPaint != paint)
        m_strokeData.access().visitedLinkPaint = WTFMove(paint);
}

inline void SVGRenderStyle::setStrokeDashArray(Style::SVGStrokeDasharray&& array)
{
    if (!(m_strokeData->dashArray == array))
        m_strokeData.access().dashArray = WTFMove(array);
}

inline void SVGRenderStyle::setStrokeDashOffset(Style::SVGStrokeDashoffset&& offset)
{
    if (!(m_strokeData->dashOffset == offset))
        m_strokeData.access().dashOffset = WTFMove(offset);
}

inline void SVGRenderStyle::setStopOpacity(Style::Opacity opacity)
{
    if (!(m_stopData->opacity == opacity))
        m_stopData.access().opacity = opacity;
}

inline void SVGRenderStyle::setStopColor(Style::Color&& color)
{
    if (!(m_stopData->color == color))
        m_stopData.access().color = WTFMove(color);
}

inline void SVGRenderStyle::setFloodOpacity(Style::Opacity opacity)
{
    if (!(m_miscData->floodOpacity == opacity))
        m_miscData.access().floodOpacity = opacity;
}

inline void SVGRenderStyle::setFloodColor(Style::Color&& color)
{
    if (!(m_miscData->floodColor == color))
        m_miscData.access().floodColor = WTFMove(color);
}

inline void SVGRenderStyle::setLightingColor(Style::Color&& color)
{
    if (!(m_miscData->lightingColor == color))
        m_miscData.access().lightingColor = WTFMove(color);
}

inline void SVGRenderStyle::setBaselineShift(Style::SVGBaselineShift&& baselineShift)
{
    if (!(m_miscData->baselineShift == baselineShift))
        m_miscData.access().baselineShift = WTFMove(baselineShift);
}

inline void SVGRenderStyle::setMarkerStartResource(Style::URL&& resource)
{
    if (!(m_inheritedResourceData->markerStart == resource))
        m_inheritedResourceData.access().markerStart = WTFMove(resource);
}

inline void SVGRenderStyle::setMarkerMidResource(Style::URL&& resource)
{
    if (!(m_inheritedResourceData->markerMid == resource))
        m_inheritedResourceData.access().markerMid = WTFMove(resource);
}

inline void SVGRenderStyle::setMarkerEndResource(Style::URL&& resource)
{
    if (!(m_inheritedResourceData->markerEnd == resource))
        m_inheritedResourceData.access().markerEnd = WTFMove(resource);
}

inline void SVGRenderStyle::setBitDefaults()
{
    m_inheritedFlags.clipRule = static_cast<unsigned>(initialClipRule());
    m_inheritedFlags.fillRule = static_cast<unsigned>(initialFillRule());
    m_inheritedFlags.shapeRendering = static_cast<unsigned>(initialShapeRendering());
    m_inheritedFlags.textAnchor = static_cast<unsigned>(initialTextAnchor());
    m_inheritedFlags.colorInterpolation = static_cast<unsigned>(initialColorInterpolation());
    m_inheritedFlags.colorInterpolationFilters = static_cast<unsigned>(initialColorInterpolationFilters());
    m_inheritedFlags.glyphOrientationHorizontal = static_cast<unsigned>(initialGlyphOrientationHorizontal());
    m_inheritedFlags.glyphOrientationVertical = static_cast<unsigned>(initialGlyphOrientationVertical());

    m_nonInheritedFlags.flags = 0;
    m_nonInheritedFlags.flagBits.alignmentBaseline = static_cast<unsigned>(initialAlignmentBaseline());
    m_nonInheritedFlags.flagBits.dominantBaseline = static_cast<unsigned>(initialDominantBaseline());
    m_nonInheritedFlags.flagBits.vectorEffect = static_cast<unsigned>(initialVectorEffect());
    m_nonInheritedFlags.flagBits.bufferedRendering = static_cast<unsigned>(initialBufferedRendering());
    m_nonInheritedFlags.flagBits.maskType = static_cast<unsigned>(initialMaskType());
}

} // namespace WebCore
