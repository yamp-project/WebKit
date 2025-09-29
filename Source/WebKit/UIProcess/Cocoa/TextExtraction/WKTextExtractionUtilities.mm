/*
 * Copyright (C) 2024 Apple Inc. All rights reserved.
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
#import "WKTextExtractionUtilities.h"

#if USE(APPLE_INTERNAL_SDK) || (!PLATFORM(WATCHOS) && !PLATFORM(APPLETV))

#import "TextExtractionFilter.h"
#import "_WKTextExtractionInternal.h"
#import <WebCore/TextExtraction.h>
#import <wtf/CallbackAggregator.h>
#import <wtf/cocoa/TypeCastsCocoa.h>
#import <wtf/cocoa/VectorCocoa.h>

namespace WebKit {
using namespace WebCore;

inline static WKTextExtractionContainer containerType(TextExtraction::ContainerType type)
{
    switch (type) {
    case TextExtraction::ContainerType::Root:
        return WKTextExtractionContainerRoot;
    case TextExtraction::ContainerType::ViewportConstrained:
        return WKTextExtractionContainerViewportConstrained;
    case TextExtraction::ContainerType::List:
        return WKTextExtractionContainerList;
    case TextExtraction::ContainerType::ListItem:
        return WKTextExtractionContainerListItem;
    case TextExtraction::ContainerType::BlockQuote:
        return WKTextExtractionContainerBlockQuote;
    case TextExtraction::ContainerType::Article:
        return WKTextExtractionContainerArticle;
    case TextExtraction::ContainerType::Section:
        return WKTextExtractionContainerSection;
    case TextExtraction::ContainerType::Nav:
        return WKTextExtractionContainerNav;
    case TextExtraction::ContainerType::Button:
        return WKTextExtractionContainerButton;
    case TextExtraction::ContainerType::Canvas:
        return WKTextExtractionContainerCanvas;
    case TextExtraction::ContainerType::Generic:
        return WKTextExtractionContainerGeneric;
    }
}

static WKTextExtractionEventListenerTypes eventListenerTypes(OptionSet<TextExtraction::EventListenerCategory> eventListeners)
{
    WKTextExtractionEventListenerTypes result = WKTextExtractionEventListenerTypeNone;
    if (eventListeners.contains(TextExtraction::EventListenerCategory::Click))
        result |= WKTextExtractionEventListenerTypeClick;
    if (eventListeners.contains(TextExtraction::EventListenerCategory::Hover))
        result |= WKTextExtractionEventListenerTypeHover;
    if (eventListeners.contains(TextExtraction::EventListenerCategory::Touch))
        result |= WKTextExtractionEventListenerTypeTouch;
    if (eventListeners.contains(TextExtraction::EventListenerCategory::Wheel))
        result |= WKTextExtractionEventListenerTypeWheel;
    if (eventListeners.contains(TextExtraction::EventListenerCategory::Keyboard))
        result |= WKTextExtractionEventListenerTypeKeyboard;
    return result;
}

static RetainPtr<WKTextExtractionEditable> createWKEditable(const TextExtraction::Editable& editable)
{
    return adoptNS([[WKTextExtractionEditable alloc]
        initWithLabel:editable.label.createNSString().get()
        placeholder:editable.placeholder.createNSString().get()
        isSecure:static_cast<BOOL>(editable.isSecure)
        isFocused:static_cast<BOOL>(editable.isFocused)]);
}

inline static RetainPtr<WKTextExtractionItem> createItemWithChildren(const TextExtraction::Item& item, const RootViewToWebViewConverter& converter, NSArray<WKTextExtractionItem *> *children)
{
    auto rectInWebView = converter(item.rectInRootView);
    auto eventListeners = eventListenerTypes(item.eventListeners);
    RetainPtr<NSString> nodeIdentifier;
    if (item.nodeIdentifier)
        nodeIdentifier = [NSString stringWithFormat:@"%llu", item.nodeIdentifier->toUInt64()];

    RetainPtr accessibilityRole = item.accessibilityRole.createNSString();
    RetainPtr ariaAttributes = adoptNS([[NSMutableDictionary alloc] initWithCapacity:item.ariaAttributes.size()]);
    for (auto& [attribute, value] : item.ariaAttributes)
        [ariaAttributes setObject:value.createNSString().get() forKey:attribute.createNSString().get()];

    return WTF::switchOn(item.data,
        [&](const TextExtraction::TextItemData& data) -> RetainPtr<WKTextExtractionItem> {
            RetainPtr<WKTextExtractionEditable> editable;
            if (data.editable)
                editable = createWKEditable(*data.editable);

            auto selectedRange = NSMakeRange(NSNotFound, 0);
            if (auto range = data.selectedRange) {
                if (range->location + range->length <= data.content.length()) [[likely]]
                    selectedRange = NSMakeRange(range->location, range->length);
            }

            auto links = createNSArray(data.links, [&](auto& linkAndRange) -> RetainPtr<WKTextExtractionLink> {
                auto& [url, range] = linkAndRange;
                if (range.location + range.length > data.content.length()) [[unlikely]]
                    return { };

                RetainPtr nsURL = url.createNSURL();
                if (!nsURL)
                    return { };

                return adoptNS([[WKTextExtractionLink alloc] initWithURL:nsURL.get() range:NSMakeRange(range.location, range.length)]);
            });

            return adoptNS([[WKTextExtractionTextItem alloc]
                initWithContent:data.content.createNSString().get()
                selectedRange:selectedRange
                links:links.get()
                editable:editable.get()
                rectInWebView:rectInWebView
                children:children
                eventListeners:eventListeners
                ariaAttributes:ariaAttributes.get()
                accessibilityRole:accessibilityRole.get()
                nodeIdentifier:nodeIdentifier.get()]);
        }, [&](const TextExtraction::ScrollableItemData& data) -> RetainPtr<WKTextExtractionItem> {
            return adoptNS([[WKTextExtractionScrollableItem alloc]
                initWithContentSize:data.contentSize
                rectInWebView:rectInWebView
                children:children
                eventListeners:eventListeners
                ariaAttributes:ariaAttributes.get()
                accessibilityRole:accessibilityRole.get()
                nodeIdentifier:nodeIdentifier.get()]);
        }, [&](const TextExtraction::SelectData& data) -> RetainPtr<WKTextExtractionItem> {
            return adoptNS([[WKTextExtractionSelectItem alloc]
                initWithSelectedValues:createNSArray(data.selectedValues).get()
                supportsMultiple:data.isMultiple
                rectInWebView:rectInWebView
                children:children
                eventListeners:eventListeners
                ariaAttributes:ariaAttributes.get()
                accessibilityRole:accessibilityRole.get()
                nodeIdentifier:nodeIdentifier.get()]);
        }, [&](const TextExtraction::ImageItemData& data) -> RetainPtr<WKTextExtractionItem> {
            return adoptNS([[WKTextExtractionImageItem alloc]
                initWithName:data.name.createNSString().get()
                altText:data.altText.createNSString().get()
                rectInWebView:rectInWebView
                children:children
                eventListeners:eventListeners
                ariaAttributes:ariaAttributes.get()
                accessibilityRole:accessibilityRole.get()
                nodeIdentifier:nodeIdentifier.get()]);
        }, [&](const TextExtraction::ContentEditableData& data) -> RetainPtr<WKTextExtractionItem> {
            return adoptNS([[WKTextExtractionContentEditableItem alloc]
                initWithContentEditableType:data.isPlainTextOnly ? WKTextExtractionEditablePlainTextOnly : WKTextExtractionEditableRichText
                isFocused:data.isFocused
                rectInWebView:rectInWebView
                children:children
                eventListeners:eventListeners
                ariaAttributes:ariaAttributes.get()
                accessibilityRole:accessibilityRole.get()
                nodeIdentifier:nodeIdentifier.get()]);
        }, [&](const TextExtraction::TextFormControlData& data) -> RetainPtr<WKTextExtractionItem> {
            return adoptNS([[WKTextExtractionTextFormControlItem alloc]
                initWithEditable:createWKEditable(data.editable).get()
                controlType:data.controlType.createNSString().get()
                autocomplete:data.autocomplete.createNSString().get()
                isReadonly:data.isReadonly
                isDisabled:data.isDisabled
                isChecked:data.isChecked
                rectInWebView:rectInWebView
                children:children
                eventListeners:eventListeners
                ariaAttributes:ariaAttributes.get()
                accessibilityRole:accessibilityRole.get()
                nodeIdentifier:nodeIdentifier.get()]);
        }, [&](const TextExtraction::LinkItemData& data) -> RetainPtr<WKTextExtractionItem> {
            return adoptNS([[WKTextExtractionLinkItem alloc]
                initWithTarget:data.target.createNSString().get()
                url:data.completedURL.createNSURL().get()
                rectInWebView:rectInWebView
                children:children
                eventListeners:eventListeners
                ariaAttributes:ariaAttributes.get()
                accessibilityRole:accessibilityRole.get()
                nodeIdentifier:nodeIdentifier.get()]);
        }, [&](TextExtraction::ContainerType type) -> RetainPtr<WKTextExtractionItem> {
            return adoptNS([[WKTextExtractionContainerItem alloc]
                initWithContainer:containerType(type)
                rectInWebView:rectInWebView
                children:children
                eventListeners:eventListeners
                ariaAttributes:ariaAttributes.get()
                accessibilityRole:accessibilityRole.get()
                nodeIdentifier:nodeIdentifier.get()]);
        }
    );
}

static RetainPtr<WKTextExtractionItem> createItemRecursive(const TextExtraction::Item& item, const RootViewToWebViewConverter& converter)
{
    return createItemWithChildren(item, converter, createNSArray(item.children, [&](auto& child) {
        return createItemRecursive(child, converter);
    }).get());
}

RetainPtr<WKTextExtractionItem> createItem(const TextExtraction::Item& item, RootViewToWebViewConverter&& converter)
{
    if (!std::holds_alternative<TextExtraction::ContainerType>(item.data)) {
        ASSERT_NOT_REACHED();
        return nil;
    }

    if (std::get<TextExtraction::ContainerType>(item.data) != TextExtraction::ContainerType::Root) {
        ASSERT_NOT_REACHED();
        return nil;
    }

    return createItemRecursive(item, WTFMove(converter));
}

#if ENABLE(TEXT_EXTRACTION_FILTER)

static void filterTextRecursive(WKTextExtractionItem *item, MainRunLoopCallbackAggregator& aggregator)
{
    if (RetainPtr textItem = dynamic_objc_cast<WKTextExtractionTextItem>(item)) {
        TextExtractionFilter::singleton().shouldFilter([textItem content], [textItem, aggregator = Ref { aggregator }](bool shouldFilter) mutable {
            if (!shouldFilter)
                return;

            [textItem setContent:@""];
            [textItem setSelectedRange:NSMakeRange(NSNotFound, 0)];
        });
    }

    for (WKTextExtractionItem *child in item.children)
        filterTextRecursive(child, aggregator);
}

void filterText(WKTextExtractionItem *item, CompletionHandler<void()>&& completion)
{
    Ref aggregator = MainRunLoopCallbackAggregator::create(WTFMove(completion));
    filterTextRecursive(item, aggregator.get());
}

#endif // ENABLE(TEXT_EXTRACTION_FILTER)

} // namespace WebKit

#endif // USE(APPLE_INTERNAL_SDK) || (!PLATFORM(WATCHOS) && !PLATFORM(APPLETV))
