/*
 * (C) 1999 Lars Knoll (knoll@kde.org)
 * (C) 2000 Dirk Mueller (mueller@kde.org)
 * Copyright (C) 2004-2017 Apple Inc. All rights reserved.
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

#include "Color.h"
#include "FloatPoint.h"
#include "GraphicsContext.h"
#include "InlineTextBoxStyle.h"
#include "RenderStyleConstants.h"
#include <wtf/OptionSet.h>

namespace WebCore {

class FontCascade;
class RenderObject;
class RenderStyle;
class TextRun;
    
class TextDecorationPainter {
public:
    TextDecorationPainter(GraphicsContext&, const FontCascade&, const Style::TextShadows&, const Style::AppleColorFilter&, bool isPrinting, WritingMode);

    struct Styles {
        bool operator==(const Styles&) const;

        struct DecorationStyleAndColor {
            Color color;
            TextDecorationStyle decorationStyle { TextDecorationStyle::Solid };
        };
        DecorationStyleAndColor underline;
        DecorationStyleAndColor overline;
        DecorationStyleAndColor linethrough;
    };
    struct BackgroundDecorationGeometry {
        FloatPoint textOrigin;
        FloatPoint boxOrigin;
        float textBoxWidth { 0.f };
        float textDecorationThickness { 0.f };
        float underlineOffset { 0.f };
        float overlineOffset { 0.f };
        float linethroughCenter { 0.f };
        float clippingOffset { 0.f };
        WavyStrokeParameters wavyStrokeParameters;
    };
    void paintBackgroundDecorations(const RenderStyle&, const TextRun&, const BackgroundDecorationGeometry&, Style::TextDecorationLine, const Styles&);

    struct ForegroundDecorationGeometry {
        FloatPoint boxOrigin;
        float textBoxWidth { 0.f };
        float textDecorationThickness { 0.f };
        float linethroughCenter { 0.f };
        WavyStrokeParameters wavyStrokeParameters;
    };
    void paintForegroundDecorations(const ForegroundDecorationGeometry&, const Styles&);

    static Color decorationColor(const RenderStyle&, OptionSet<PaintBehavior> paintBehavior = { });
    static Styles stylesForRenderer(const RenderObject&, Style::TextDecorationLine requestedDecorations, bool firstLineStyle = false, OptionSet<PaintBehavior> paintBehavior = { }, PseudoId = PseudoId::None);
    static Style::TextDecorationLine textDecorationsInEffectForStyle(const TextDecorationPainter::Styles&);

private:
    void paintLineThrough(const ForegroundDecorationGeometry&, const Color&, const Styles&);

    GraphicsContext& m_context;
    bool m_isPrinting { false };
    WritingMode m_writingMode;
    const Style::TextShadows& m_shadow;
    const Style::AppleColorFilter& m_shadowColorFilter;
    const FontCascade& m_font;
};

} // namespace WebCore
