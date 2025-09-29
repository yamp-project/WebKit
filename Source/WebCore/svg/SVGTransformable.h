/*
 * Copyright (C) 2004, 2005, 2008 Nikolas Zimmermann <zimmermann@kde.org>
 * Copyright (C) 2004, 2005, 2006, 2007 Rob Buis <buis@kde.org>
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

#include "SVGLocatable.h"
#include "SVGTransformValue.h"

namespace WebCore {
    
class AffineTransform;
class SVGTransform;

class SVGTransformable : public SVGLocatable {
public:
    virtual ~SVGTransformable();

    template<typename CharacterType>
    static bool parseAndReplaceTransform(SVGTransformValue::SVGTransformType, StringParsingBuffer<CharacterType>&, SVGTransform&); // Defined in SVGTransformableInlines.h

    template<typename CharacterType>
    static RefPtr<SVGTransform> parseTransform(SVGTransformValue::SVGTransformType, StringParsingBuffer<CharacterType>&); // Defined in SVGTransformableInlines.h

    static std::optional<SVGTransformValue::SVGTransformType> parseTransformType(StringView);
    static std::optional<SVGTransformValue::SVGTransformType> parseTransformType(StringParsingBuffer<Latin1Character>&);
    static std::optional<SVGTransformValue::SVGTransformType> parseTransformType(StringParsingBuffer<char16_t>&);

    AffineTransform localCoordinateSpaceTransform(CTMScope) const override { return animatedLocalTransform(); }
    virtual AffineTransform animatedLocalTransform() const = 0;
};

} // namespace WebCore
