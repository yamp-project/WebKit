/*
 * Copyright (C) 2025 Apple Inc. All rights reserved.
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

#include "GridTypeAliases.h"
#include "LayoutState.h"
#include "LayoutUnit.h"
#include <wtf/CheckedRef.h>

namespace WebCore {
namespace Layout {

class ElementBox;
class PlacedGridItem;

class UnplacedGridItem;

struct GridAreaLines;
struct UnplacedGridItems;

class GridFormattingContext : public CanMakeCheckedPtr<GridFormattingContext> {
    WTF_MAKE_TZONE_OR_ISO_ALLOCATED(GridFormattingContext);
    WTF_OVERRIDE_DELETE_FOR_CHECKED_PTR(GridFormattingContext);
public:

    struct GridLayoutConstraints {
        std::optional<LayoutUnit> inlineAxisAvailableSpace;
        std::optional<LayoutUnit> blockAxisAvailableSpace;
    };

    GridFormattingContext(const ElementBox& gridBox, LayoutState&);

    void layout(GridLayoutConstraints);

    PlacedGridItems constructPlacedGridItems(const GridAreas&) const;

    const ElementBox& root() const { return m_gridBox; }

private:
    UnplacedGridItems constructUnplacedGridItems() const;

    const CheckedRef<const ElementBox> m_gridBox;
    const CheckedRef<LayoutState> m_globalLayoutState;
};

} // namespace Layout
} // namespace WebCore
