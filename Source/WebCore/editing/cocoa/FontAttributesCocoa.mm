/*
 * Copyright (C) 2018 Apple Inc. All rights reserved.
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

#import "config.h"
#import "FontAttributes.h"

#import "CSSValueKeywords.h"
#import "ColorCocoa.h"
#import "FontCocoa.h"
#import <pal/spi/cocoa/NSAttributedStringSPI.h>
#import <wtf/cocoa/TypeCastsCocoa.h>
#import <wtf/cocoa/VectorCocoa.h>

#if PLATFORM(IOS_FAMILY)
#import <pal/ios/UIKitSoftLink.h>
#endif

namespace WebCore {

static RetainPtr<NSString> cocoaTextListMarkerName(const Style::ListStyleType& styleType, bool ordered)
{
    // See https://developer.apple.com/documentation/appkit/nstextlist/init(markerformat:options:)
    // for the description of the expected format string for a marker format.

    return WTF::switchOn(styleType,
        [&](const Style::CounterStyle& counterStyle) -> RetainPtr<NSString> {
            // Each of these keywords intrinsically denotes if the NSTextList is considered ordered or not,
            // which may or may not be logically consistent with the semantics of an element's <ol> / <ul> tag,
            // but *is* consistent with the list style type, which takes precedent per the specification.
            //
            // Consequently, a <ul> element with a list-style-type of `decimal` for example will have the
            // following properties when converted to an NSTextList:
            //
            // - The marker format would be NSTextListMarkerDecimal aka `{decimal}`
            // - The marker would be `n` where `n` is the list element ordinal
            // - The list will be considered "ordered".

            if (counterStyle == CSSValueDisc)
                return NSTextListMarkerDisc;
            if (counterStyle == CSSValueCircle)
                return NSTextListMarkerCircle;
            if (counterStyle == CSSValueSquare)
                return NSTextListMarkerSquare;
            if (counterStyle == CSSValueDecimal)
                return NSTextListMarkerDecimal;
            if (counterStyle == CSSValueOctal)
                return NSTextListMarkerOctal;
            if (counterStyle == CSSValueLowerRoman)
                return NSTextListMarkerLowercaseRoman;
            if (counterStyle == CSSValueUpperRoman)
                return NSTextListMarkerUppercaseRoman;
            if (counterStyle == CSSValueLowerAlpha)
                return NSTextListMarkerLowercaseAlpha;
            if (counterStyle == CSSValueUpperAlpha)
                return NSTextListMarkerUppercaseAlpha;
            if (counterStyle == CSSValueLowerLatin)
                return NSTextListMarkerLowercaseLatin;
            if (counterStyle == CSSValueUpperLatin)
                return NSTextListMarkerUppercaseLatin;
            if (counterStyle == CSSValueLowerHexadecimal)
                return NSTextListMarkerLowercaseHexadecimal;
            if (counterStyle == CSSValueUpperHexadecimal)
                return NSTextListMarkerUppercaseHexadecimal;

            // The remaining web-exposed list style types have no Cocoa equivalents.
            // Fall back to default styles for ordered and unordered lists.
            return ordered ? NSTextListMarkerDecimal : NSTextListMarkerDisc;
        },
        [&](const CSS::Keyword::None&) {
            return retainPtr(ordered ? NSTextListMarkerDecimal : NSTextListMarkerDisc);
        },
        [&](const AtomString& identifier) {
            // The marker for the marker format `{<identifier>}` is `n` where `n` is the item ordinal, and the list is considered ordered.
            // The marker for the marker format `<identifier>` is `<identifier>`, and the list is considered unordered.
            auto format = ordered ? makeString("{"_s, identifier, "}"_s) : identifier;
            return format.createNSString();
        }
    );
}

RetainPtr<NSTextList> TextList::createTextList() const
{
#if PLATFORM(MAC)
    Class textListClass = NSTextList.class;
#else
    Class textListClass = PAL::getNSTextListClassSingleton();
#endif
    RetainPtr result = adoptNS([[textListClass alloc] initWithMarkerFormat:cocoaTextListMarkerName(styleType, ordered).get() options:0]);
    [result setStartingItemNumber:startingItemNumber];
    return result;
}

RetainPtr<NSDictionary> FontAttributes::createDictionary() const
{
    NSMutableDictionary *attributes = [NSMutableDictionary dictionary];
    if (RetainPtr cocoaFont = font ? bridge_cast(font->ctFont()) : nil)
        attributes[NSFontAttributeName] = cocoaFont.get();

    if (foregroundColor.isValid())
        attributes[NSForegroundColorAttributeName] = cocoaColor(foregroundColor).get();

    if (backgroundColor.isValid())
        attributes[NSBackgroundColorAttributeName] = cocoaColor(backgroundColor).get();

    if (fontShadow.color.isValid() && (!fontShadow.offset.isZero() || fontShadow.blurRadius))
        attributes[NSShadowAttributeName] = fontShadow.createShadow().get();

    if (subscriptOrSuperscript == SubscriptOrSuperscript::Subscript)
        attributes[NSSuperscriptAttributeName] = @(-1);
    else if (subscriptOrSuperscript == SubscriptOrSuperscript::Superscript)
        attributes[NSSuperscriptAttributeName] = @1;

#if PLATFORM(MAC)
    Class paragraphStyleClass = NSParagraphStyle.class;
#else
    Class paragraphStyleClass = PAL::getNSParagraphStyleClassSingleton();
#endif
    auto style = adoptNS([[paragraphStyleClass defaultParagraphStyle] mutableCopy]);

    switch (horizontalAlignment) {
    case HorizontalAlignment::Left:
        [style setAlignment:NSTextAlignmentLeft];
        break;
    case HorizontalAlignment::Center:
        [style setAlignment:NSTextAlignmentCenter];
        break;
    case HorizontalAlignment::Right:
        [style setAlignment:NSTextAlignmentRight];
        break;
    case HorizontalAlignment::Justify:
        [style setAlignment:NSTextAlignmentJustified];
        break;
    case HorizontalAlignment::Natural:
        [style setAlignment:NSTextAlignmentNatural];
        break;
    }

    if (!textLists.isEmpty()) {
        [style setTextLists:createNSArray(textLists, [] (auto& textList) {
            return textList.createTextList();
        }).get()];
    }

    attributes[NSParagraphStyleAttributeName] = style.get();

    if (hasUnderline)
        attributes[NSUnderlineStyleAttributeName] = @(NSUnderlineStyleSingle);

    if (hasStrikeThrough)
        attributes[NSStrikethroughStyleAttributeName] = @(NSUnderlineStyleSingle);

    return attributes;
}

} // namespace WebCore
