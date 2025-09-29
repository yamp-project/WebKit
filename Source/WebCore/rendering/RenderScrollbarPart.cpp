/*
 * Copyright (C) 2008 Apple Inc. All rights reserved.
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
#include "RenderScrollbarPart.h"

#include "PaintInfo.h"
#include "RenderBoxInlines.h"
#include "RenderBoxModelObjectInlines.h"
#include "RenderScrollbar.h"
#include "RenderScrollbarTheme.h"
#include "RenderStyleInlines.h"
#include "RenderView.h"
#include <wtf/StackStats.h>
#include <wtf/TZoneMallocInlines.h>

namespace WebCore {

WTF_MAKE_TZONE_OR_ISO_ALLOCATED_IMPL(RenderScrollbarPart);

RenderScrollbarPart::RenderScrollbarPart(Document& document, RenderStyle&& style, RenderScrollbar* scrollbar, ScrollbarPart part)
    : RenderBlock(Type::ScrollbarPart, document, WTFMove(style), { })
    , m_scrollbar(scrollbar)
    , m_part(part)
{
    ASSERT(isRenderScrollbarPart());
}

RenderScrollbarPart::~RenderScrollbarPart() = default;

void RenderScrollbarPart::layout()
{
    StackStats::LayoutCheckPoint layoutCheckPoint;
    setLocation(LayoutPoint()); // We don't worry about positioning ourselves. We're just determining our minimum width/height.
    if (m_scrollbar->orientation() == ScrollbarOrientation::Horizontal)
        layoutHorizontalPart();
    else
        layoutVerticalPart();

    clearNeedsLayout();
}

void RenderScrollbarPart::layoutHorizontalPart()
{
    if (m_part == ScrollbarBGPart) {
        setWidth(m_scrollbar->width());
        computeScrollbarHeight();
    } else {
        computeScrollbarWidth();
        setHeight(m_scrollbar->height());
    }
}

void RenderScrollbarPart::layoutVerticalPart()
{
    if (m_part == ScrollbarBGPart) {
        computeScrollbarWidth();
        setHeight(m_scrollbar->height());
    } else {
        setWidth(m_scrollbar->width());
        computeScrollbarHeight();
    } 
}

static int calcScrollbarThicknessUsing(const Style::PreferredSize& preferredSize)
{
    if (!preferredSize.isPercentOrCalculated() && !preferredSize.isIntrinsicOrLegacyIntrinsicOrAuto())
        return Style::evaluateMinimum<LayoutUnit>(preferredSize, 0_lu, Style::ZoomNeeded { });
    return ScrollbarTheme::theme().scrollbarThickness();
}

static int calcScrollbarThicknessUsing(const Style::MinimumSize& minimumSize)
{
    if ((!minimumSize.isPercentOrCalculated() && !minimumSize.isIntrinsicOrLegacyIntrinsicOrAuto()) || minimumSize.isAuto())
        return Style::evaluateMinimum<LayoutUnit>(minimumSize, 0_lu, Style::ZoomNeeded { });
    return ScrollbarTheme::theme().scrollbarThickness();
}

static int calcScrollbarThicknessUsing(const Style::MaximumSize& maximumSize)
{
    if (!maximumSize.isPercentOrCalculated() && !maximumSize.isIntrinsic() && !maximumSize.isLegacyIntrinsic())
        return Style::evaluateMinimum<LayoutUnit>(maximumSize, 0_lu, Style::ZoomNeeded { });
    return ScrollbarTheme::theme().scrollbarThickness();
}

void RenderScrollbarPart::computeScrollbarWidth()
{
    if (!m_scrollbar->owningRenderer())
        return;
    auto width = calcScrollbarThicknessUsing(style().width());
    auto minWidth = calcScrollbarThicknessUsing(style().minWidth());
    auto maxWidth = style().maxWidth().isNone() ? width : calcScrollbarThicknessUsing(style().maxWidth());
    setWidth(std::max(minWidth, std::min(maxWidth, width)));
    
    // Buttons and track pieces can all have margins along the axis of the scrollbar. 
    m_marginBox.setLeft(Style::evaluateMinimum<LayoutUnit>(style().marginLeft(), 0_lu, Style::ZoomNeeded { }));
    m_marginBox.setRight(Style::evaluateMinimum<LayoutUnit>(style().marginRight(), 0_lu, Style::ZoomNeeded { }));
}

void RenderScrollbarPart::computeScrollbarHeight()
{
    if (!m_scrollbar->owningRenderer())
        return;
    auto height = calcScrollbarThicknessUsing(style().height());
    auto minHeight = calcScrollbarThicknessUsing(style().minHeight());
    auto maxHeight = style().maxHeight().isNone() ? height : calcScrollbarThicknessUsing(style().maxHeight());
    setHeight(std::max(minHeight, std::min(maxHeight, height)));

    // Buttons and track pieces can all have margins along the axis of the scrollbar. 
    m_marginBox.setTop(Style::evaluateMinimum<LayoutUnit>(style().marginTop(), 0_lu, Style::ZoomNeeded { }));
    m_marginBox.setBottom(Style::evaluateMinimum<LayoutUnit>(style().marginBottom(), 0_lu, Style::ZoomNeeded { }));
}

void RenderScrollbarPart::styleDidChange(StyleDifference diff, const RenderStyle* oldStyle)
{
    RenderBlock::styleDidChange(diff, oldStyle);
    setInline(false);
    clearPositionedState();
    setFloating(false);
    setHasNonVisibleOverflow(false);
    if (oldStyle && m_scrollbar && m_part != NoPart && diff >= StyleDifference::Repaint)
        m_scrollbar->theme().invalidatePart(*m_scrollbar, m_part);
}

void RenderScrollbarPart::imageChanged(WrappedImagePtr image, const IntRect* rect)
{
    if (m_scrollbar && m_part != NoPart)
        m_scrollbar->theme().invalidatePart(*m_scrollbar, m_part);
    else {
        if (view().frameView().isFrameViewScrollCorner(*this)) {
            view().frameView().invalidateScrollCorner(view().frameView().scrollCornerRect());
            return;
        }
        
        RenderBlock::imageChanged(image, rect);
    }
}

void RenderScrollbarPart::paintIntoRect(GraphicsContext& graphicsContext, const LayoutPoint& paintOffset, const LayoutRect& rect)
{
    // Make sure our dimensions match the rect.
    setLocation(rect.location() - toLayoutSize(paintOffset));
    setWidth(rect.width());
    setHeight(rect.height());

    if (graphicsContext.paintingDisabled() || style().opacity().isTransparent())
        return;

    // We don't use RenderLayers for scrollbar parts, so we need to handle opacity here.
    // Opacity for ScrollbarBGPart is handled by RenderScrollbarTheme::willPaintScrollbar().
    bool needsTransparencyLayer = m_part != ScrollbarBGPart && !style().opacity().isOpaque();
    if (needsTransparencyLayer) {
        graphicsContext.save();
        graphicsContext.clip(rect);
        graphicsContext.beginTransparencyLayer(style().opacity().value.value);
    }
    
    // Now do the paint.
    PaintInfo paintInfo(graphicsContext, snappedIntRect(rect), PaintPhase::BlockBackground, PaintBehavior::Normal);
    paint(paintInfo, paintOffset);
    paintInfo.phase = PaintPhase::ChildBlockBackgrounds;
    paint(paintInfo, paintOffset);
    paintInfo.phase = PaintPhase::Float;
    paint(paintInfo, paintOffset);
    paintInfo.phase = PaintPhase::Foreground;
    paint(paintInfo, paintOffset);
    paintInfo.phase = PaintPhase::Outline;
    paint(paintInfo, paintOffset);

    if (needsTransparencyLayer) {
        graphicsContext.endTransparencyLayer();
        graphicsContext.restore();
    }
}

RenderBox* RenderScrollbarPart::rendererOwningScrollbar() const
{
    if (!m_scrollbar)
        return nullptr;
    return m_scrollbar->owningRenderer();
}

}
