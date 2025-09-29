/**
 * Copyright (C) 2003-2023 Apple Inc. All rights reserved.
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

#include <WebCore/PseudoElement.h>
#include <WebCore/RenderBox.h>
#include <WebCore/RenderObjectDocument.h>
#include <WebCore/RenderObjectNode.h>
#include <WebCore/StyleOpacity.h>
#include <WebCore/StyleShapeOutside.h>
#include <WebCore/WillChangeData.h>

namespace WebCore {

inline Overflow RenderElement::effectiveOverflowBlockDirection() const { return writingMode().isHorizontal() ? effectiveOverflowY() : effectiveOverflowX(); }
inline Overflow RenderElement::effectiveOverflowInlineDirection() const { return writingMode().isHorizontal() ? effectiveOverflowX() : effectiveOverflowY(); }
inline bool RenderElement::hasBackdropFilter() const { return style().hasBackdropFilter(); }
inline bool RenderElement::hasBackground() const { return style().hasBackground(); }
inline bool RenderElement::hasBlendMode() const { return style().hasBlendMode(); }
inline bool RenderElement::hasClip() const { return isOutOfFlowPositioned() && style().hasClip(); }
inline bool RenderElement::hasClipOrNonVisibleOverflow() const { return hasClip() || hasNonVisibleOverflow(); }
inline bool RenderElement::hasClipPath() const { return style().hasClipPath(); }
inline bool RenderElement::hasFilter() const { return style().hasFilter(); }
inline bool RenderElement::hasHiddenBackface() const { return style().backfaceVisibility() == BackfaceVisibility::Hidden; }
inline bool RenderElement::hasMask() const { return style().hasMask(); }
inline bool RenderElement::hasOutline() const { return style().hasOutline() || hasOutlineAnnotation(); }
inline bool RenderElement::hasShapeOutside() const { return !style().shapeOutside().isNone(); }
inline bool RenderElement::isTransparent() const { return style().hasOpacity(); }
inline float RenderElement::opacity() const { return style().opacity().value.value; }
inline FloatRect RenderElement::transformReferenceBoxRect() const { return transformReferenceBoxRect(style()); }
inline FloatRect RenderElement::transformReferenceBoxRect(const RenderStyle& style) const { return referenceBoxRect(transformBoxToCSSBoxType(style.transformBox())); }
inline Element* RenderElement::element() const { return downcast<Element>(RenderObject::node()); }
inline RefPtr<Element> RenderElement::protectedElement() const { return element(); }
inline Element* RenderElement::nonPseudoElement() const { return downcast<Element>(RenderObject::nonPseudoNode()); }
inline RefPtr<Element> RenderElement::protectedNonPseudoElement() const { return nonPseudoElement(); }

#if HAVE(CORE_MATERIAL)
inline bool RenderElement::hasAppleVisualEffect() const { return style().hasAppleVisualEffect(); }
inline bool RenderElement::hasAppleVisualEffectRequiringBackdropFilter() const { return style().hasAppleVisualEffectRequiringBackdropFilter(); }
#endif

inline bool RenderElement::isBlockLevelBox() const
{
    // block-level boxes are boxes that participate in a block formatting context.
    auto* renderBox = dynamicDowncast<RenderBox>(*this);
    if (!renderBox)
        return false;

    if (renderBox->isFlexItem() || renderBox->isGridItem() || renderBox->isRenderTableCell())
        return false;
    return style().isDisplayBlockLevel();
}

inline bool RenderElement::isAnonymousBlock() const
{
    return isAnonymous()
        && (style().display() == DisplayType::Block || style().display() == DisplayType::Box)
        && style().pseudoElementType() == PseudoId::None
        && isRenderBlock()
#if ENABLE(MATHML)
        && !isRenderMathMLBlock()
#endif
        && !isRenderListMarker()
        && !isRenderFragmentedFlow()
        && !isRenderMultiColumnSet()
        && !isRenderView()
        && !isViewTransitionContainingBlock();
}

inline bool RenderElement::isBlockContainer() const
{
    auto display = style().display();
    return (display == DisplayType::Block
        || display == DisplayType::InlineBlock
        || display == DisplayType::FlowRoot
        || display == DisplayType::ListItem
        || display == DisplayType::TableCell
        || display == DisplayType::TableCaption) && !isRenderReplaced();
}

inline bool RenderElement::isBlockBox() const
{
    // A block-level box that is also a block container.
    return isBlockLevelBox() && isBlockContainer();
}

inline bool RenderElement::mayContainOutOfFlowPositionedObjects(const RenderStyle* styleToUse) const
{
    auto& style = styleToUse ? *styleToUse : this->style();
    return isRenderView()
        || (canEstablishContainingBlockWithTransform() && (styleToUse ? styleToUse->hasTransformRelatedProperty() : hasTransformRelatedProperty()))
        || (style.hasBackdropFilter() && !isDocumentElementRenderer())
        || (style.hasFilter() && !isDocumentElementRenderer())
#if HAVE(CORE_MATERIAL)
        || (style.hasAppleVisualEffectRequiringBackdropFilter() && !isDocumentElementRenderer())
#endif
        || isRenderOrLegacyRenderSVGForeignObject()
        || shouldApplyLayoutContainment(styleToUse)
        || shouldApplyPaintContainment(styleToUse)
        || isViewTransitionContainingBlock();
}

inline bool RenderElement::canContainAbsolutelyPositionedObjects(const RenderStyle* styleToUse) const
{
    auto& style = styleToUse ? *styleToUse : this->style();
    return mayContainOutOfFlowPositionedObjects(styleToUse) || style.position() != PositionType::Static || (isRenderBlock() && style.willChange() && style.willChange()->createsContainingBlockForAbsolutelyPositioned(isDocumentElementRenderer()));
}

inline bool RenderElement::canContainFixedPositionObjects(const RenderStyle* styleToUse) const
{
    auto& style = styleToUse ? *styleToUse : this->style();
    return mayContainOutOfFlowPositionedObjects(styleToUse) || (isRenderBlock() && style.willChange() && style.willChange()->createsContainingBlockForOutOfFlowPositioned(isDocumentElementRenderer()));
}

inline bool RenderElement::createsGroupForStyle(const RenderStyle& style)
{
    return style.hasOpacity()
        || style.hasMask()
        || style.hasClipPath()
        || style.hasFilter()
        || style.hasBackdropFilter()
#if HAVE(CORE_MATERIAL)
        || style.hasAppleVisualEffect()
#endif
        || style.hasBlendMode();
}

inline bool RenderElement::hasPotentiallyScrollableOverflow() const
{
    // We only need to test one overflow dimension since 'visible' and 'clip' always get accompanied
    // with 'clip' or 'visible' in the other dimension (see Style::Adjuster::adjust).
    return hasNonVisibleOverflow() && style().overflowX() != Overflow::Clip && style().overflowX() != Overflow::Visible;
}

inline bool RenderElement::isBeforeContent() const
{
    // Text nodes don't have their own styles, so ignore the style on a text node.
    // if (isRenderText())
    //     return false;
    if (style().pseudoElementType() != PseudoId::Before)
        return false;
    return true;
}

inline bool RenderElement::isAfterContent() const
{
    // Text nodes don't have their own styles, so ignore the style on a text node.
    // if (isRenderText())
    //     return false;
    if (style().pseudoElementType() != PseudoId::After)
        return false;
    return true;
}

inline bool RenderElement::isBeforeOrAfterContent() const
{
    return isBeforeContent() || isAfterContent();
}

inline bool RenderElement::shouldApplyAnyContainment() const
{
    return shouldApplyLayoutContainment() || shouldApplySizeContainment() || shouldApplyInlineSizeContainment() || shouldApplyStyleContainment() || shouldApplyPaintContainment();
}

inline bool RenderElement::shouldApplySizeOrInlineSizeContainment() const
{
    return shouldApplySizeContainment() || shouldApplyInlineSizeContainment();
}

inline bool RenderElement::shouldApplyLayoutContainment(const RenderStyle* styleToUse) const
{
    return element() && WebCore::shouldApplyLayoutContainment(styleToUse ? *styleToUse : style(), *element());
}

inline bool RenderElement::shouldApplySizeContainment() const
{
    return element() && WebCore::shouldApplySizeContainment(style(), *element());
}

inline bool RenderElement::shouldApplyInlineSizeContainment() const
{
    return element() && WebCore::shouldApplyInlineSizeContainment(style(), *element());
}

inline bool RenderElement::shouldApplyStyleContainment() const
{
    return element() && WebCore::shouldApplyStyleContainment(style(), *element());
}

inline bool RenderElement::shouldApplyPaintContainment(const RenderStyle* styleToUse) const
{
    return element() && WebCore::shouldApplyPaintContainment(styleToUse ? *styleToUse : style(), *element());
}

inline bool RenderElement::visibleToHitTesting(const std::optional<HitTestRequest>& request) const
{
    auto visibility = !request || request->userTriggered() ? style().usedVisibility() : style().visibility();
    return visibility == Visibility::Visible
        && !isSkippedContent()
        && ((request && request->ignoreCSSPointerEventsProperty()) || usedPointerEvents() != PointerEvents::None);
}

inline int adjustForAbsoluteZoom(int value, const RenderElement& renderer)
{
    return adjustForAbsoluteZoom(value, renderer.style());
}

inline LayoutSize adjustLayoutSizeForAbsoluteZoom(LayoutSize size, const RenderElement& renderer)
{
    return adjustLayoutSizeForAbsoluteZoom(size, renderer.style());
}

inline LayoutUnit adjustLayoutUnitForAbsoluteZoom(LayoutUnit value, const RenderElement& renderer)
{
    return adjustLayoutUnitForAbsoluteZoom(value, renderer.style());
}

inline Element* RenderElement::generatingElement() const
{
    return isPseudoElement() ? downcast<PseudoElement>(*element()).hostElement() : element();
}

} // namespace WebCore
