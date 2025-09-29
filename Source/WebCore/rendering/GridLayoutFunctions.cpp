/*
 * Copyright (C) 2017 Igalia S.L.
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
#include "GridLayoutFunctions.h"

#include "AncestorSubgridIterator.h"
#include "RenderBoxInlines.h"
#include "RenderBoxModelObjectInlines.h"
#include "RenderChildIterator.h"
#include "RenderGrid.h"
#include "RenderStyleConstants.h"
#include "RenderStyleInlines.h"
#include "StyleGridTrackSizingDirection.h"

namespace WebCore {

namespace GridLayoutFunctions {

static inline bool marginStartIsAuto(const RenderBox& gridItem, Style::GridTrackSizingDirection direction)
{
    return direction == Style::GridTrackSizingDirection::Columns ? gridItem.style().marginStart().isAuto() : gridItem.style().marginBefore().isAuto();
}

static inline bool marginEndIsAuto(const RenderBox& gridItem, Style::GridTrackSizingDirection direction)
{
    return direction == Style::GridTrackSizingDirection::Columns ? gridItem.style().marginEnd().isAuto() : gridItem.style().marginAfter().isAuto();
}

static bool gridItemHasMargin(const RenderBox& gridItem, Style::GridTrackSizingDirection direction)
{
    // Length::IsZero returns true for 'auto' margins, which is aligned with the purpose of this function.
    if (direction == Style::GridTrackSizingDirection::Columns)
        return !gridItem.style().marginStart().isZero() || !gridItem.style().marginEnd().isZero();
    return !gridItem.style().marginBefore().isZero() || !gridItem.style().marginAfter().isZero();
}

LayoutUnit computeMarginLogicalSizeForGridItem(const RenderGrid& grid, Style::GridTrackSizingDirection direction, const RenderBox& gridItem)
{
    auto flowAwareDirection = flowAwareDirectionForGridItem(grid, gridItem, direction);
    if (!gridItemHasMargin(gridItem, flowAwareDirection))
        return 0;

    LayoutUnit marginStart;
    LayoutUnit marginEnd;
    if (direction == Style::GridTrackSizingDirection::Columns)
        gridItem.computeInlineDirectionMargins(grid, gridItem.containingBlockLogicalWidthForContent(), { }, gridItem.logicalWidth(), marginStart, marginEnd);
    else
        gridItem.computeBlockDirectionMargins(grid, marginStart, marginEnd);
    return marginStartIsAuto(gridItem, flowAwareDirection) ? marginEnd : marginEndIsAuto(gridItem, flowAwareDirection) ? marginStart : marginStart + marginEnd;
}

bool hasRelativeOrIntrinsicSizeForGridItem(const RenderBox& gridItem, Style::GridTrackSizingDirection direction)
{
    if (direction == Style::GridTrackSizingDirection::Columns)
        return gridItem.hasRelativeLogicalWidth() || gridItem.style().logicalWidth().isIntrinsicOrLegacyIntrinsicOrAuto();
    return gridItem.hasRelativeLogicalHeight() || gridItem.style().logicalHeight().isIntrinsicOrLegacyIntrinsicOrAuto();
}

static ExtraMarginsFromSubgrids extraMarginForSubgrid(const RenderGrid& parent, unsigned startLine, unsigned endLine, Style::GridTrackSizingDirection direction)
{
    unsigned numTracks = parent.numTracks(direction);
    if (!numTracks || !parent.isSubgrid(direction))
        return { };

    std::optional<LayoutUnit> availableSpace;
    if (!hasRelativeOrIntrinsicSizeForGridItem(parent, direction))
        availableSpace = parent.availableSpaceForGutters(direction);

    RenderGrid& grandParent = downcast<RenderGrid>(*parent.parent());
    ExtraMarginsFromSubgrids extraMargins;
    if (!startLine)
        extraMargins.addTrackStartMargin((direction == Style::GridTrackSizingDirection::Columns) ? parent.marginAndBorderAndPaddingStart() : parent.marginAndBorderAndPaddingBefore());
    else
        extraMargins.addTrackStartMargin((parent.gridGap(direction, availableSpace) - grandParent.gridGap(direction)) / 2);

    if (endLine == numTracks)
        extraMargins.addTrackEndMargin((direction == Style::GridTrackSizingDirection::Columns) ? parent.marginAndBorderAndPaddingEnd() : parent.marginAndBorderAndPaddingAfter());
    else
        extraMargins.addTrackEndMargin((parent.gridGap(direction, availableSpace) - grandParent.gridGap(direction)) / 2);

    return extraMargins;
}

ExtraMarginsFromSubgrids extraMarginForSubgridAncestors(Style::GridTrackSizingDirection direction, const RenderBox& gridItem)
{
    ExtraMarginsFromSubgrids extraMargins;
    for (auto& currentAncestorSubgrid : ancestorSubgridsOfGridItem(gridItem, direction)) {
        GridSpan span = currentAncestorSubgrid.gridSpanForGridItem(gridItem, direction);
        extraMargins += extraMarginForSubgrid(currentAncestorSubgrid, span.startLine(), span.endLine(), direction);
    }
    return extraMargins;
}

LayoutUnit marginLogicalSizeForGridItem(const RenderGrid& grid, Style::GridTrackSizingDirection direction, const RenderBox& gridItem)
{
    auto margin = computeMarginLogicalSizeForGridItem(grid, direction, gridItem);

    if (&grid != gridItem.parent()) {
        auto subgridDirection = flowAwareDirectionForGridItem(grid, *downcast<RenderGrid>(gridItem.parent()), direction);
        margin += extraMarginForSubgridAncestors(subgridDirection, gridItem).extraTotalMargin();
    }

    return margin;
}

bool isOrthogonalGridItem(const RenderGrid& grid, const RenderBox& gridItem)
{
    return gridItem.isHorizontalWritingMode() != grid.isHorizontalWritingMode();
}

bool isOrthogonalParent(const RenderGrid& grid, const RenderElement& parent)
{
    return parent.isHorizontalWritingMode() != grid.isHorizontalWritingMode();
}

bool isAspectRatioBlockSizeDependentGridItem(const RenderBox& gridItem)
{
    return (gridItem.style().hasAspectRatio() || gridItem.hasIntrinsicAspectRatio()) && (gridItem.hasRelativeLogicalHeight() || gridItem.hasStretchedLogicalHeight());
}

bool isGridItemInlineSizeDependentOnBlockConstraints(const RenderBox& gridItem, const RenderGrid& parentGrid, ItemPosition gridItemAlignSelf)
{
    ASSERT(gridItem.parent() == &parentGrid);

    if (isOrthogonalGridItem(parentGrid, gridItem))
        return true;

    auto& gridItemStyle = gridItem.style();
    auto gridItemFlexWrap = gridItemStyle.flexWrap();
    if (gridItem.isRenderFlexibleBox() && gridItem.style().isColumnFlexDirection() && (gridItemFlexWrap == FlexWrap::Wrap || gridItemFlexWrap == FlexWrap::Reverse))
        return true;

    if (gridItem.isRenderMultiColumnFlow())
        return true;

    if (isAspectRatioBlockSizeDependentGridItem(gridItem))
        return true;

    // Stretch alignment allows the grid item content to resolve against the stretched size.
    if (gridItemAlignSelf != ItemPosition::Stretch)
        return false;

    auto hasAspectRatioAndInlineSizeDependsOnBlockSize = [](auto& renderer) {
        auto& rendererStyle = renderer.style();
        bool rendererHasAspectRatio = renderer.hasIntrinsicAspectRatio() || rendererStyle.hasAspectRatio();

        return rendererHasAspectRatio && rendererStyle.logicalWidth().isAuto() && !rendererStyle.logicalHeight().isIntrinsicOrLegacyIntrinsicOrAuto();
    };

    for (auto& gridItemChild : childrenOfType<RenderBox>(gridItem)) {
        if (hasAspectRatioAndInlineSizeDependsOnBlockSize(gridItemChild))
            return true;
    }

    return false;
}

Style::GridTrackSizingDirection flowAwareDirectionForGridItem(const RenderGrid& grid, const RenderBox& gridItem, Style::GridTrackSizingDirection direction)
{
    return !isOrthogonalGridItem(grid, gridItem) ? direction : orthogonalDirection(direction);
}

Style::GridTrackSizingDirection flowAwareDirectionForParent(const RenderGrid& grid, const RenderElement& parent, Style::GridTrackSizingDirection direction)
{
    return isOrthogonalParent(grid, parent) ? orthogonalDirection(direction) : direction;
}

std::optional<RenderBox::GridAreaSize> overridingContainingBlockContentSizeForGridItem(const RenderBox& gridItem, Style::GridTrackSizingDirection direction)
{
    return direction == Style::GridTrackSizingDirection::Columns ? gridItem.gridAreaContentLogicalWidth() : gridItem.gridAreaContentLogicalHeight();
}

bool isFlippedDirection(const RenderGrid& grid, Style::GridTrackSizingDirection direction)
{
    if (direction == Style::GridTrackSizingDirection::Columns)
        return grid.writingMode().isBidiRTL();
    return grid.writingMode().isBlockFlipped();
}

bool isSubgridReversedDirection(const RenderGrid& grid, Style::GridTrackSizingDirection outerDirection, const RenderGrid& subgrid)
{
    auto subgridDirection = flowAwareDirectionForGridItem(grid, subgrid, outerDirection);
    ASSERT(subgrid.isSubgrid(subgridDirection));
    return isFlippedDirection(grid, outerDirection) != isFlippedDirection(subgrid, subgridDirection);
}

unsigned alignmentContextForBaselineAlignment(const GridSpan& span, const ItemPosition& alignment)
{
    ASSERT(alignment == ItemPosition::Baseline || alignment == ItemPosition::LastBaseline);
    if (alignment == ItemPosition::Baseline)
        return span.startLine();
    return span.endLine() - 1;
}

void setOverridingContentSizeForGridItem(const RenderGrid& renderGrid, RenderBox& gridItem, LayoutUnit logicalSize, Style::GridTrackSizingDirection direction)
{
    if (!isOrthogonalGridItem(renderGrid, gridItem))
        direction == Style::GridTrackSizingDirection::Columns ? gridItem.setOverridingBorderBoxLogicalWidth(logicalSize) : gridItem.setOverridingBorderBoxLogicalHeight(logicalSize);
    else
        direction == Style::GridTrackSizingDirection::Columns ? gridItem.setOverridingBorderBoxLogicalHeight(logicalSize) : gridItem.setOverridingBorderBoxLogicalWidth(logicalSize);
}

void clearOverridingContentSizeForGridItem(const RenderGrid& renderGrid, RenderBox &gridItem, Style::GridTrackSizingDirection direction)
{
    if (!isOrthogonalGridItem(renderGrid, gridItem))
        direction == Style::GridTrackSizingDirection::Columns ? gridItem.clearOverridingBorderBoxLogicalWidth() : gridItem.clearOverridingBorderBoxLogicalHeight();
    else
        direction == Style::GridTrackSizingDirection::Columns ? gridItem.clearOverridingBorderBoxLogicalHeight() : gridItem.clearOverridingBorderBoxLogicalWidth();
}

bool hasAutoMarginsInColumnAxis(const RenderBox& gridItem, WritingMode parentWritingMode)
{
    if (parentWritingMode.isHorizontal())
        return gridItem.style().marginTop().isAuto() || gridItem.style().marginBottom().isAuto();
    return gridItem.style().marginLeft().isAuto() || gridItem.style().marginRight().isAuto();
}

bool hasAutoMarginsInRowAxis(const RenderBox& gridItem, WritingMode parentWritingMode)
{
    if (parentWritingMode.isHorizontal())
        return gridItem.style().marginLeft().isAuto() || gridItem.style().marginRight().isAuto();
    return gridItem.style().marginTop().isAuto() || gridItem.style().marginBottom().isAuto();
}

bool hasAutoSizeInColumnAxis(const RenderBox& gridItem, WritingMode parentWritingMode)
{
    if (gridItem.style().hasAspectRatio()) {
        // FIXME: should align-items + align-self: auto/justify-items + justify-self: auto be taken into account?
        if (parentWritingMode.isHorizontal() == gridItem.isHorizontalWritingMode() && gridItem.style().alignSelf().position() != ItemPosition::Stretch) {
            // A non-auto inline size means the same for block size (column axis size) because of the aspect ratio.
            if (!gridItem.style().logicalWidth().isAuto())
                return false;
        } else if (gridItem.style().justifySelf().position() != ItemPosition::Stretch) {
            auto& logicalHeight = gridItem.style().logicalHeight();
            if (logicalHeight.isFixed() || (logicalHeight.isPercentOrCalculated() && gridItem.percentageLogicalHeightIsResolvable()))
                return false;
        }
    }
    return parentWritingMode.isHorizontal() ? gridItem.style().height().isAuto() : gridItem.style().width().isAuto();
}

bool hasAutoSizeInRowAxis(const RenderBox& gridItem, WritingMode parentWritingMode)
{
    if (gridItem.style().hasAspectRatio()) {
        // FIXME: should align-items + align-self: auto/justify-items + justify-self: auto be taken into account?
        if (parentWritingMode.isHorizontal() == gridItem.isHorizontalWritingMode() && gridItem.style().alignSelf().position() != ItemPosition::Stretch) {
            // A non-auto block size means the same for inline size (row axis size) because of the aspect ratio.
            auto& logicalHeight = gridItem.style().logicalHeight();
            if (logicalHeight.isFixed() || (logicalHeight.isPercentOrCalculated() && gridItem.percentageLogicalHeightIsResolvable()))
                return false;
        } else if (gridItem.style().justifySelf().position() != ItemPosition::Stretch) {
            if (!gridItem.style().logicalWidth().isAuto())
                return false;
        }
    }
    return parentWritingMode.isHorizontal() ? gridItem.style().width().isAuto() : gridItem.style().height().isAuto();
}

bool allowedToStretchGridItemAlongColumnAxis(const RenderBox& gridItem, ItemPosition alignSelf, WritingMode writingMode)
{
    return alignSelf == ItemPosition::Stretch && hasAutoSizeInColumnAxis(gridItem, writingMode) && !hasAutoMarginsInColumnAxis(gridItem, writingMode);
}

bool allowedToStretchGridItemAlongRowAxis(const RenderBox& gridItem, ItemPosition justifySelf, WritingMode writingMode)
{
    return justifySelf == ItemPosition::Stretch && hasAutoSizeInRowAxis(gridItem, writingMode) && !hasAutoMarginsInRowAxis(gridItem, writingMode);
}

LayoutUnit availableAlignmentSpaceForGridItemBeforeStretching(const RenderGrid& grid, LayoutUnit gridAreaBreadthForGridItem, const RenderBox& gridItem, Style::GridTrackSizingDirection direction)
{
    // Because we want to avoid multiple layouts, stretching logic might be performed before
    // grid items are laid out, so we can't use the grid item cached values. Hence, we need to
    // compute margins in order to determine the available height before stretching.
    auto gridItemFlowDirection = flowAwareDirectionForGridItem(grid, gridItem, direction);
    return std::max(0_lu, gridAreaBreadthForGridItem - marginLogicalSizeForGridItem(grid, gridItemFlowDirection, gridItem));
}

void updateAutoMarginsIfNeeded(RenderBox& gridItem, WritingMode writingMode)
{
    updateAutoMarginsInRowAxisIfNeeded(gridItem, writingMode);
    updateAutoMarginsInColumnAxisIfNeeded(gridItem, writingMode);
}

void updateAutoMarginsInRowAxisIfNeeded(RenderBox& gridItem, WritingMode writingMode)
{
    ASSERT(!gridItem.isOutOfFlowPositioned());

    auto& marginStart = gridItem.style().marginStart(writingMode);
    auto& marginEnd = gridItem.style().marginEnd(writingMode);
    LayoutUnit marginLogicalWidth;
    // We should only consider computed margins if their specified value isn't
    // 'auto', since such computed value may come from a previous layout and may
    // be incorrect now.
    if (!marginStart.isAuto())
        marginLogicalWidth += gridItem.marginStart();
    if (!marginEnd.isAuto())
        marginLogicalWidth += gridItem.marginEnd();

    auto availableAlignmentSpace = gridItem.gridAreaContentLogicalWidth()->value() - gridItem.logicalWidth() - marginLogicalWidth;
    if (availableAlignmentSpace <= 0)
        return;

    if (marginStart.isAuto() && marginEnd.isAuto()) {
        gridItem.setMarginStart(availableAlignmentSpace / 2, writingMode);
        gridItem.setMarginEnd(availableAlignmentSpace / 2, writingMode);
    } else if (marginStart.isAuto()) {
        gridItem.setMarginStart(availableAlignmentSpace, writingMode);
    } else if (marginEnd.isAuto())
        gridItem.setMarginEnd(availableAlignmentSpace, writingMode);
}

void updateAutoMarginsInColumnAxisIfNeeded(RenderBox& gridItem, WritingMode writingMode)
{
    ASSERT(!gridItem.isOutOfFlowPositioned());

    auto& marginBefore = gridItem.style().marginBefore(writingMode);
    auto& marginAfter = gridItem.style().marginAfter(writingMode);
    LayoutUnit marginLogicalHeight;
    // We should only consider computed margins if their specified value isn't
    // 'auto', since such computed value may come from a previous layout and may
    // be incorrect now.
    if (!marginBefore.isAuto())
        marginLogicalHeight += gridItem.marginBefore();
    if (!marginAfter.isAuto())
        marginLogicalHeight += gridItem.marginAfter();

    auto availableAlignmentSpace = gridItem.gridAreaContentLogicalHeight()->value() - gridItem.logicalHeight() - marginLogicalHeight;
    if (availableAlignmentSpace <= 0)
        return;

    if (marginBefore.isAuto() && marginAfter.isAuto()) {
        gridItem.setMarginBefore(availableAlignmentSpace / 2, writingMode);
        gridItem.setMarginAfter(availableAlignmentSpace / 2, writingMode);
    } else if (marginBefore.isAuto()) {
        gridItem.setMarginBefore(availableAlignmentSpace, writingMode);
    } else if (marginAfter.isAuto())
        gridItem.setMarginAfter(availableAlignmentSpace, writingMode);
}

bool isRelativeGridTrackBreadthAsAuto(const Style::GridTrackFitContentLength& length, std::optional<LayoutUnit> availableSpace)
{
    return length.isPercentOrCalculated() && !availableSpace;
}
bool isRelativeGridTrackBreadthAsAuto(const Style::GridTrackBreadth& length, std::optional<LayoutUnit> availableSpace)
{
    return length.isPercentOrCalculated() && !availableSpace;
}

} // namespace GridLayoutFunctions

} // namespace WebCore
