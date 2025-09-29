// Copyright 2014 The Chromium Authors. All rights reserved.
// Copyright (C) 2016-2025 Apple Inc. All rights reserved.
// Copyright (C) 2025 Samuel Weinig <sam@webkit.org>
//
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are
// met:
//
//    * Redistributions of source code must retain the above copyright
// notice, this list of conditions and the following disclaimer.
//    * Redistributions in binary form must reproduce the above
// copyright notice, this list of conditions and the following disclaimer
// in the documentation and/or other materials provided with the
// distribution.
//    * Neither the name of Google Inc. nor the names of its
// contributors may be used to endorse or promote products derived from
// this software without specific prior written permission.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
// "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
// LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
// A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
// OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
// SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
// LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
// DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
// THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
// (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
// OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

#include "config.h"
#include "CSSParser.h"

#include "CSSAtRuleID.h"
#include "CSSCounterStyleRule.h"
#include "CSSCustomPropertySyntax.h"
#include "CSSCustomPropertyValue.h"
#include "CSSFontFeatureValuesRule.h"
#include "CSSKeyframeRule.h"
#include "CSSKeyframesRule.h"
#include "CSSParserEnum.h"
#include "CSSParserFastPaths.h"
#include "CSSParserIdioms.h"
#include "CSSParserObserver.h"
#include "CSSParserObserverWrapper.h"
#include "CSSParserToken.h"
#include "CSSPositionTryRule.h"
#include "CSSPropertyParser.h"
#include "CSSPropertyParserConsumer+Animations.h"
#include "CSSPropertyParserConsumer+CSSPrimitiveValueResolver.h"
#include "CSSPropertyParserConsumer+CounterStyles.h"
#include "CSSPropertyParserConsumer+Font.h"
#include "CSSPropertyParserConsumer+Ident.h"
#include "CSSPropertyParserConsumer+IntegerDefinitions.h"
#include "CSSPropertyParserConsumer+Primitives.h"
#include "CSSPropertyParserConsumer+Timeline.h"
#include "CSSSelectorParser.h"
#include "CSSStyleSheet.h"
#include "CSSSupportsParser.h"
#include "CSSTokenizer.h"
#include "CSSValueList.h"
#include "CSSValuePair.h"
#include "CSSVariableParser.h"
#include "CSSViewTransitionRule.h"
#include "ComputedStyleDependencies.h"
#include "ContainerQueryParser.h"
#include "Document.h"
#include "Element.h"
#include "FontPaletteValues.h"
#include "MediaList.h"
#include "MediaQueryParser.h"
#include "MediaQueryParserContext.h"
#include "MutableCSSSelector.h"
#include "NodeInlines.h"
#include "NestingLevelIncrementer.h"
#include "StylePropertiesInlines.h"
#include "StyleRule.h"
#include "StyleRuleFunction.h"
#include "StyleRuleImport.h"
#include "StyleSheetContents.h"
#include <bitset>
#include <memory>
#include <optional>
#include <wtf/StdLibExtras.h>

namespace WebCore {

CSSParser::~CSSParser() = default;

CSSParser::CSSParser(const CSSParserContext& context, StyleSheetContents* styleSheet)
    : m_context(context)
    , m_styleSheet(styleSheet)
{
}

CSSParser::CSSParser(const CSSParserContext& context, const String& string, StyleSheetContents* styleSheet, CSSParserObserverWrapper* wrapper, CSSParserEnum::NestedContext nestedContext)
    : m_context(context)
    , m_styleSheet(styleSheet)
    , m_tokenizer(wrapper ? CSSTokenizer::tryCreate(string, *wrapper) : CSSTokenizer::tryCreate(string))
    , m_observerWrapper(wrapper)
{
    // With CSSOM, we might want the parser to start in an already nested state.
    if (nestedContext)
        m_ancestorRuleTypeStack.append(*nestedContext);
}

auto CSSParser::parseValue(MutableStyleProperties& declaration, CSSPropertyID propertyID, const String& string, IsImportant important, const CSSParserContext& context) -> ParseResult
{
    auto ruleType = context.enclosingRuleType.value_or(StyleRuleType::Style);

    auto state = CSS::PropertyParserState {
        .context = context,
        .currentRule = ruleType,
        .currentProperty = propertyID,
        .important = important,
    };
    if (RefPtr value = CSSParserFastPaths::maybeParseValue(propertyID, string, state))
        return declaration.addParsedProperty(CSSProperty(propertyID, value.releaseNonNull(), important)) ? ParseResult::Changed : ParseResult::Unchanged;

    CSSParser parser(context, string);
    parser.consumeDeclarationValue(parser.tokenizer()->tokenRange(), propertyID, important, ruleType);
    if (parser.topContext().m_parsedProperties.isEmpty())
        return ParseResult::Error;
    return declaration.addParsedProperties(parser.topContext().m_parsedProperties) ? ParseResult::Changed : ParseResult::Unchanged;
}

auto CSSParser::parseCustomPropertyValue(MutableStyleProperties& declaration, const AtomString& propertyName, const String& string, IsImportant important, const CSSParserContext& context) -> ParseResult
{
    CSSParser parser(context, string);

    auto range = parser.tokenizer()->tokenRange();
    range.consumeWhitespace();
    range.trimTrailingWhitespace();
    parser.consumeCustomPropertyValue(range, propertyName, important);

    if (parser.topContext().m_parsedProperties.isEmpty())
        return ParseResult::Error;
    return declaration.addParsedProperties(parser.topContext().m_parsedProperties) ? ParseResult::Changed : ParseResult::Unchanged;
}

static inline void filterProperties(IsImportant important, const ParsedPropertyVector& input, ParsedPropertyVector& output, size_t& unusedEntries, std::bitset<numCSSProperties>& seenProperties, HashSet<AtomString>& seenCustomProperties)
{
    // Add properties in reverse order so that highest priority definitions are reached first. Duplicate definitions can then be ignored when found.
    for (size_t i = input.size(); i--;) {
        const CSSProperty& property = input[i];
        if ((property.isImportant() && important == IsImportant::No) || (!property.isImportant() && important == IsImportant::Yes))
            continue;
        const unsigned propertyIDIndex = property.id() - firstCSSProperty;

        if (property.id() == CSSPropertyCustom) {
            auto& name = downcast<CSSCustomPropertyValue>(*property.value()).name();
            if (!seenCustomProperties.add(name).isNewEntry)
                continue;
            output[--unusedEntries] = property;
            continue;
        }

        auto seenPropertyBit = seenProperties[propertyIDIndex];
        if (seenPropertyBit)
            continue;
        seenPropertyBit = true;

        output[--unusedEntries] = property;
    }
}

static Ref<ImmutableStyleProperties> createStyleProperties(ParsedPropertyVector& parsedProperties, CSSParserMode mode)
{
    std::bitset<numCSSProperties> seenProperties;
    size_t unusedEntries = parsedProperties.size();
    ParsedPropertyVector results(unusedEntries);
    HashSet<AtomString> seenCustomProperties;

    filterProperties(IsImportant::Yes, parsedProperties, results, unusedEntries, seenProperties, seenCustomProperties);
    filterProperties(IsImportant::No, parsedProperties, results, unusedEntries, seenProperties, seenCustomProperties);

    Ref result = ImmutableStyleProperties::createDeduplicating(results.subspan(unusedEntries), mode);
    parsedProperties.clear();
    return result;
}

Ref<ImmutableStyleProperties> CSSParser::parseInlineStyleDeclaration(const String& string, const Element& element)
{
    CSSParserContext context(element.document());
    context.mode = strictToCSSParserMode(element.isHTMLElement() && !element.document().inQuirksMode());

    CSSParser parser(context, string);
    parser.consumeDeclarationList(parser.tokenizer()->tokenRange(), StyleRuleType::Style);
    return createStyleProperties(parser.topContext().m_parsedProperties, context.mode);
}

bool CSSParser::parseDeclarationList(MutableStyleProperties& declaration, const String& string, const CSSParserContext& context)
{
    CSSParser parser(context, string);
    auto ruleType = context.enclosingRuleType.value_or(StyleRuleType::Style);
    parser.consumeDeclarationList(parser.tokenizer()->tokenRange(), ruleType);
    if (parser.topContext().m_parsedProperties.isEmpty())
        return false;

    std::bitset<numCSSProperties> seenProperties;
    size_t unusedEntries = parser.topContext().m_parsedProperties.size();
    ParsedPropertyVector results(unusedEntries);
    HashSet<AtomString> seenCustomProperties;
    filterProperties(IsImportant::Yes, parser.topContext().m_parsedProperties, results, unusedEntries, seenProperties, seenCustomProperties);
    filterProperties(IsImportant::No, parser.topContext().m_parsedProperties, results, unusedEntries, seenProperties, seenCustomProperties);
    if (unusedEntries)
        results.removeAt(0, unusedEntries);
    return declaration.addParsedProperties(results);
}

RefPtr<StyleRuleBase> CSSParser::parseRule(const String& string, const CSSParserContext& context, StyleSheetContents* styleSheet, AllowedRules allowedRules, CSSParserEnum::NestedContext nestedContext)
{
    CSSParser parser(context, string, styleSheet, nullptr, nestedContext);
    CSSParserTokenRange range = parser.tokenizer()->tokenRange();
    range.consumeWhitespace();
    if (range.atEnd())
        return nullptr; // Parse error, empty rule
    RefPtr<StyleRuleBase> rule;
    if (range.peek().type() == AtKeywordToken)
        rule = parser.consumeAtRule(range, allowedRules);
    else
        rule = parser.consumeQualifiedRule(range, allowedRules);
    if (!rule)
        return nullptr; // Parse error, failed to consume rule
    range.consumeWhitespace();
    if (!rule || !range.atEnd())
        return nullptr; // Parse error, trailing garbage
    return rule;
}

RefPtr<StyleRuleKeyframe> CSSParser::parseKeyframeRule(const String& string, const CSSParserContext& context)
{
    RefPtr keyframe = parseRule(string, context, nullptr, CSSParser::AllowedRules::KeyframeRules);
    return downcast<StyleRuleKeyframe>(keyframe.get());
}

RefPtr<StyleRuleNestedDeclarations> CSSParser::parseNestedDeclarations(const CSSParserContext&context , const String& string)
{
    auto properties = MutableStyleProperties::createEmpty();
    if (!parseDeclarationList(properties, string , context))
        return { };

    return StyleRuleNestedDeclarations::create(WTFMove(properties));
}

void CSSParser::parseStyleSheet(const String& string, const CSSParserContext& context, StyleSheetContents& styleSheet)
{
    CSSParser parser(context, string, &styleSheet, nullptr);
    bool firstRuleValid = parser.consumeRuleList(parser.tokenizer()->tokenRange(), RuleList::TopLevel, [&](Ref<StyleRuleBase> rule) {
        if (rule->isCharsetRule())
            return;
        if (context.shouldIgnoreImportRules && rule->isImportRule())
            return;
        styleSheet.parserAppendRule(WTFMove(rule));
    });
    styleSheet.setHasSyntacticallyValidCSSHeader(firstRuleValid);
    styleSheet.shrinkToFit();
}

CSSSelectorList CSSParser::parsePageSelector(CSSParserTokenRange range, StyleSheetContents* styleSheet)
{
    // We only support a small subset of the css-page spec.
    range.consumeWhitespace();
    AtomString typeSelector;
    if (range.peek().type() == IdentToken)
        typeSelector = range.consume().value().toAtomString();

    StringView pseudo;
    if (range.peek().type() == ColonToken) {
        range.consume();
        if (range.peek().type() != IdentToken)
            return { };
        pseudo = range.consume().value();
    }

    range.consumeWhitespace();
    if (!range.atEnd())
        return { }; // Parse error; extra tokens in @page selector

    std::unique_ptr<MutableCSSSelector> selector;
    if (!typeSelector.isNull() && pseudo.isNull())
        selector = makeUnique<MutableCSSSelector>(QualifiedName(nullAtom(), typeSelector, styleSheet->defaultNamespace()));
    else {
        selector = makeUnique<MutableCSSSelector>();
        if (!pseudo.isNull()) {
            selector = std::unique_ptr<MutableCSSSelector>(MutableCSSSelector::parsePagePseudoSelector(pseudo));
            if (!selector || selector->match() != CSSSelector::Match::PagePseudoClass)
                return { };
        }
        if (!typeSelector.isNull())
            selector->appendTagInComplexSelector(QualifiedName(nullAtom(), typeSelector, styleSheet->defaultNamespace()));
    }

    selector->setForPage();
    return CSSSelectorList { MutableCSSSelectorList::from(WTFMove(selector)) };
}

bool CSSParser::supportsDeclaration(CSSParserTokenRange& range)
{
    bool result = false;

    // We create a new nesting context to isolate the parsing of the @supports(...) prelude from declarations before or after.
    // This only concerns the prelude,
    // (the content of the block will also be in its own nesting context but it's not done here (cf consumeRegularRuleList))
    runInNewNestingContext([&] {
        ASSERT(topContext().m_parsedProperties.isEmpty());
        result = consumeDeclaration(range, StyleRuleType::Style);
    });

    return result;
}

void CSSParser::parseDeclarationListForInspector(const String& declaration, const CSSParserContext& context, CSSParserObserver& observer)
{
    Ref wrapper = CSSParserObserverWrapper::create(observer);
    CSSParser parser(context, declaration, nullptr, wrapper.ptr());
    observer.startRuleHeader(StyleRuleType::Style, 0);
    observer.endRuleHeader(1);
    parser.consumeDeclarationList(parser.tokenizer()->tokenRange(), StyleRuleType::Style);
}

void CSSParser::parseStyleSheetForInspector(const String& string, const CSSParserContext& context, StyleSheetContents& styleSheet, CSSParserObserver& observer)
{
    Ref wrapper = CSSParserObserverWrapper::create(observer);
    CSSParser parser(context, string, &styleSheet, wrapper.ptr());
    bool firstRuleValid = parser.consumeRuleList(parser.tokenizer()->tokenRange(), RuleList::TopLevel, [&styleSheet](Ref<StyleRuleBase> rule) {
        if (rule->isCharsetRule())
            return;
        styleSheet.parserAppendRule(WTFMove(rule));
    });
    styleSheet.setHasSyntacticallyValidCSSHeader(firstRuleValid);
}

static CSSParser::AllowedRules computeNewAllowedRules(CSSParser::AllowedRules allowedRules, StyleRuleBase* rule)
{
    if (!rule || allowedRules == CSSParser::AllowedRules::FontFeatureValuesRules || allowedRules == CSSParser::AllowedRules::KeyframeRules || allowedRules == CSSParser::AllowedRules::NoRules)
        return allowedRules;

    ASSERT(allowedRules <= CSSParser::AllowedRules::RegularRules);
    if (rule->isCharsetRule())
        return CSSParser::AllowedRules::LayerStatementRules;
    if (allowedRules <= CSSParser::AllowedRules::LayerStatementRules && rule->isLayerRule() && downcast<StyleRuleLayer>(*rule).isStatement())
        return CSSParser::AllowedRules::LayerStatementRules;
    if (rule->isImportRule())
        return CSSParser::AllowedRules::ImportRules;
    if (rule->isNamespaceRule())
        return CSSParser::AllowedRules::NamespaceRules;
    return CSSParser::AllowedRules::RegularRules;
}

template<typename T>
bool CSSParser::consumeRuleList(CSSParserTokenRange range, RuleList ruleListType, NOESCAPE const T& callback)
{
    auto allowedRules = AllowedRules::RegularRules;
    switch (ruleListType) {
    case RuleList::TopLevel:
        allowedRules = AllowedRules::CharsetRules;
        break;
    case RuleList::Regular:
        allowedRules = AllowedRules::RegularRules;
        break;
    case RuleList::Keyframes:
        allowedRules = AllowedRules::KeyframeRules;
        break;
    case RuleList::FontFeatureValues:
        allowedRules = AllowedRules::FontFeatureValuesRules;
        break;
    default:
        ASSERT_NOT_REACHED();
    }

    bool seenRule = false;
    bool firstRuleValid = false;
    while (!range.atEnd()) {
        RefPtr<StyleRuleBase> rule;
        switch (range.peek().type()) {
        case NonNewlineWhitespaceToken:
        case NewlineToken:
            range.consumeWhitespace();
            continue;
        case AtKeywordToken:
            rule = consumeAtRule(range, allowedRules);
            break;
        case CDOToken:
        case CDCToken:
            if (ruleListType == RuleList::TopLevel) {
                range.consume();
                continue;
            }
            [[fallthrough]];
        default:
            rule = consumeQualifiedRule(range, allowedRules);
            break;
        }
        if (!seenRule) {
            seenRule = true;
            firstRuleValid = rule;
        }
        if (rule) {
            allowedRules = computeNewAllowedRules(allowedRules, rule.get());
            callback(Ref { *rule });
        }
    }

    return firstRuleValid;
}

RefPtr<StyleRuleBase> CSSParser::consumeAtRule(CSSParserTokenRange& range, AllowedRules allowedRules)
{
    ASSERT(range.peek().type() == AtKeywordToken);
    const StringView name = range.consumeIncludingWhitespace().value();
    auto preludeStart = range;
    while (!range.atEnd() && range.peek().type() != LeftBraceToken && range.peek().type() != SemicolonToken)
        range.consumeComponentValue();

    auto prelude = preludeStart.rangeUntil(range);
    CSSAtRuleID id = cssAtRuleID(name);

    if (range.atEnd() || range.peek().type() == SemicolonToken) {
        range.consume();
        if (allowedRules == AllowedRules::CharsetRules && id == CSSAtRuleCharset)
            return consumeCharsetRule(prelude);
        if (allowedRules <= AllowedRules::ImportRules && id == CSSAtRuleImport)
            return consumeImportRule(prelude);
        if (allowedRules <= AllowedRules::NamespaceRules && id == CSSAtRuleNamespace)
            return consumeNamespaceRule(prelude);
        if (allowedRules <= AllowedRules::RegularRules && id == CSSAtRuleLayer)
            return consumeLayerRule(prelude, { });
        return nullptr; // Parse error, unrecognised at-rule without block
    }

    CSSParserTokenRange block = range.consumeBlock();
    if (allowedRules == AllowedRules::KeyframeRules)
        return nullptr; // Parse error, no at-rules supported inside @keyframes
    if (allowedRules == AllowedRules::NoRules)
        return nullptr;

    if (allowedRules == AllowedRules::ConditionalGroupRules) {
        switch (id) {
        case CSSAtRuleMedia:
        case CSSAtRuleSupports:
        case CSSAtRuleContainer:
            break;
        case CSSAtRuleFunction:
            if (!isFunctionNestedContext())
                return nullptr;
            break;
        default:
            return nullptr;
        }
    };

    switch (id) {
    case CSSAtRuleMedia:
        return consumeMediaRule(prelude, block);
    case CSSAtRuleSupports:
        return consumeSupportsRule(prelude, block);
    case CSSAtRuleFontFace:
        return consumeFontFaceRule(prelude, block);
    case CSSAtRuleFontFeatureValues:
        return consumeFontFeatureValuesRule(prelude, block);
    case CSSAtRuleStyleset:
    case CSSAtRuleStylistic:
    case CSSAtRuleCharacterVariant:
    case CSSAtRuleSwash:
    case CSSAtRuleOrnaments:
    case CSSAtRuleAnnotation:
        return allowedRules == AllowedRules::FontFeatureValuesRules ? consumeFontFeatureValuesRuleBlock(id, prelude, block) : nullptr;
    case CSSAtRuleFontPaletteValues:
        return consumeFontPaletteValuesRule(prelude, block);
    case CSSAtRuleWebkitKeyframes:
    case CSSAtRuleKeyframes:
        return consumeKeyframesRule(prelude, block);
    case CSSAtRulePage:
        return consumePageRule(prelude, block);
    case CSSAtRuleCounterStyle:
        return consumeCounterStyleRule(prelude, block);
    case CSSAtRuleLayer:
        return consumeLayerRule(prelude, block);
    case CSSAtRuleContainer:
        return consumeContainerRule(prelude, block);
    case CSSAtRuleProperty:
        return consumePropertyRule(prelude, block);
    case CSSAtRuleScope:
        return consumeScopeRule(prelude, block);
    case CSSAtRuleStartingStyle:
        return consumeStartingStyleRule(prelude, block);
    case CSSAtRuleViewTransition:
        return consumeViewTransitionRule(prelude, block);
    case CSSAtRulePositionTry:
        return consumePositionTryRule(prelude, block);
    case CSSAtRuleFunction:
        return consumeFunctionRule(prelude, block);
    case CSSAtRuleInternalBaseAppearance:
        return consumeInternalBaseAppearanceRule(prelude, block);
    default:
        return nullptr; // Parse error, unrecognised at-rule with block
    }
}

// https://drafts.csswg.org/css-syntax/#consume-a-qualified-rule
RefPtr<StyleRuleBase> CSSParser::consumeQualifiedRule(CSSParserTokenRange& range, AllowedRules allowedRules)
{
    const auto initialRange = range;

    auto isNestedStyleRule = [&] {
        return hasStyleRuleAncestor() && allowedRules <= AllowedRules::RegularRules;
    };

    auto preludeStart = range;

    // Parsing a selector (aka a component value) should stop at the first semicolon (and goes to error recovery)
    // instead of consuming the whole list of declarations (in nested context).
    // At top level (aka non nested context), it's the normal rule list error recovery and we don't need this.
    while (!range.atEnd() && range.peek().type() != LeftBraceToken && (!isNestedStyleRule() || range.peek().type() != SemicolonToken))
        range.consumeComponentValue();

    if (range.atEnd())
        return { }; // Parse error, EOF instead of qualified rule block

    // See comment above
    if (isNestedStyleRule() && range.peek().type() == SemicolonToken) {
        range.consume();
        return { };
    }

    // https://github.com/w3c/csswg-drafts/issues/9336#issuecomment-1719806755
    if (range.peek().type() == LeftBraceToken) {
        auto rangeCopyForDashedIdent = initialRange;
        auto customProperty = CSSPropertyParserHelpers::consumeDashedIdent(rangeCopyForDashedIdent);
        // This rule is ambigous with a custom property because it looks like "--ident: ...."
        if (customProperty && rangeCopyForDashedIdent.peek().type() == ColonToken) {
            if (isStyleNestedContext()) {
                // Error, consume until semicolon or end of block.
                while (!range.atEnd() && range.peek().type() != SemicolonToken)
                    range.consumeComponentValue();
                if (range.peek().type() == SemicolonToken)
                    range.consume();
                return { };
            }
            // Error, consume until end of block.
            range.consumeBlock();
            return { };
        }
    }

    auto prelude = preludeStart.rangeUntil(range);
    CSSParserTokenRange block = range.consumeBlockCheckingForEditability(m_styleSheet.get());

    if (allowedRules <= AllowedRules::RegularRules)
        return consumeStyleRule(prelude, block);

    if (allowedRules == AllowedRules::KeyframeRules)
        return consumeKeyframeStyleRule(prelude, block);

    return { };
}

// This may still consume tokens if it fails
static AtomString consumeStringOrURI(CSSParserTokenRange& range)
{
    const CSSParserToken& token = range.peek();

    if (token.type() == StringToken || token.type() == UrlToken)
        return range.consumeIncludingWhitespace().value().toAtomString();

    if (token.type() != FunctionToken || !equalLettersIgnoringASCIICase(token.value(), "url"_s))
        return AtomString();

    CSSParserTokenRange contents = range.consumeBlock();
    const CSSParserToken& uri = contents.consumeIncludingWhitespace();
    if (uri.type() == BadStringToken || !contents.atEnd())
        return AtomString();
    return uri.value().toAtomString();
}

RefPtr<StyleRuleCharset> CSSParser::consumeCharsetRule(CSSParserTokenRange prelude)
{
    const CSSParserToken& string = prelude.consumeIncludingWhitespace();
    if (string.type() != StringToken || !prelude.atEnd())
        return nullptr; // Parse error, expected a single string
    return StyleRuleCharset::create();
}

enum class AllowAnonymous : bool { No, Yes };
static std::optional<CascadeLayerName> consumeCascadeLayerName(CSSParserTokenRange& range, AllowAnonymous allowAnonymous)
{
    CascadeLayerName name;
    if (range.atEnd()) {
        if (allowAnonymous == AllowAnonymous::Yes)
            return name;
        return { };
    }

    while (true) {
        auto nameToken = range.consume();
        if (nameToken.type() != IdentToken)
            return { };

        name.append(nameToken.value().toAtomString());

        if (range.peek().type() != DelimiterToken || range.peek().delimiter() != '.')
            break;
        range.consume();
    }

    range.consumeWhitespace();
    return name;
}

RefPtr<StyleRuleImport> CSSParser::consumeImportRule(CSSParserTokenRange prelude)
{
    AtomString uri(consumeStringOrURI(prelude));
    if (uri.isNull())
        return nullptr; // Parse error, expected string or URI

    if (RefPtr observerWrapper = m_observerWrapper.get()) {
        unsigned endOffset = observerWrapper->endOffset(prelude);
        observerWrapper->observer().startRuleHeader(StyleRuleType::Import, observerWrapper->startOffset(prelude));
        observerWrapper->observer().endRuleHeader(endOffset);
        observerWrapper->observer().startRuleBody(endOffset);
        observerWrapper->observer().endRuleBody(endOffset);
    }

    prelude.consumeWhitespace();

    auto consumeCascadeLayer = [&]() -> std::optional<CascadeLayerName> {
        auto& token = prelude.peek();
        if (token.type() == FunctionToken && equalLettersIgnoringASCIICase(token.value(), "layer"_s)) {
            auto savedPreludeForFailure = prelude;
            auto contents = CSSPropertyParserHelpers::consumeFunction(prelude);
            auto layerName = consumeCascadeLayerName(contents, AllowAnonymous::No);
            if (!layerName || !contents.atEnd()) {
                prelude = savedPreludeForFailure;
                return { };
            }
            return layerName;
        }
        if (token.type() == IdentToken && equalLettersIgnoringASCIICase(token.value(), "layer"_s)) {
            prelude.consumeIncludingWhitespace();
            return CascadeLayerName { };
        }
        return { };
    };

    auto consumeSupports = [&] () -> std::optional<StyleRuleImport::SupportsCondition> {
        auto& token = prelude.peek();
        if (token.type() == FunctionToken && equalLettersIgnoringASCIICase(token.value(), "supports"_s)) {
            auto arguments = CSSPropertyParserHelpers::consumeFunction(prelude);
            auto supported = CSSSupportsParser::supportsCondition(arguments, *this, CSSSupportsParser::ParsingMode::AllowBareDeclarationAndGeneralEnclosed);
            if (supported == CSSSupportsParser::Invalid)
                return { }; // Discard import rule.
            return StyleRuleImport::SupportsCondition { arguments.serialize(), supported == CSSSupportsParser::Supported };
        }
        return StyleRuleImport::SupportsCondition { };
    };

    auto cascadeLayerName = consumeCascadeLayer();
    auto supports = consumeSupports();
    if (!supports)
        return nullptr; // Discard import rule with incorrect syntax.
    auto mediaQueries = MQ::MediaQueryParser::parse(prelude, m_context);

    return StyleRuleImport::create(uri, WTFMove(mediaQueries), WTFMove(cascadeLayerName), WTFMove(*supports));
}

RefPtr<StyleRuleNamespace> CSSParser::consumeNamespaceRule(CSSParserTokenRange prelude)
{
    AtomString namespacePrefix;
    if (prelude.peek().type() == IdentToken)
        namespacePrefix = prelude.consumeIncludingWhitespace().value().toAtomString();

    AtomString uri(consumeStringOrURI(prelude));
    if (uri.isNull() || !prelude.atEnd())
        return nullptr; // Parse error, expected string or URI

    return StyleRuleNamespace::create(namespacePrefix, uri);
}

void CSSParser::runInNewNestingContext(auto&& run)
{
    m_nestingContextStack.append(NestingContext { });
    run();
    m_nestingContextStack.removeLast();
}

Ref<StyleRuleBase> CSSParser::createNestedDeclarationsRule()
{
    auto properties = createStyleProperties(topContext().m_parsedProperties, m_context.mode);
    return StyleRuleNestedDeclarations::create(WTFMove(properties));
}

RefPtr<StyleSheetContents> CSSParser::protectedStyleSheet() const
{
    return m_styleSheet;
}

Vector<Ref<StyleRuleBase>> CSSParser::consumeNestedGroupRules(CSSParserTokenRange block)
{
    NestingLevelIncrementer incrementer { m_ruleListNestingLevel };

    static constexpr auto maximumRuleListNestingLevel = 128;
    if (m_ruleListNestingLevel > maximumRuleListNestingLevel)
        return { };

    Vector<Ref<StyleRuleBase>> rules;
    // Declarations are allowed if there is either a parent style rule or parent scope rule.
    // https://drafts.csswg.org/css-cascade-6/#scoped-declarations
    if (isStyleNestedContext()) {
        runInNewNestingContext([&] {
            consumeStyleBlock(block, StyleRuleType::Style, ParsingStyleDeclarationsInRuleList::Yes);

            if (!topContext().m_parsedProperties.isEmpty()) {
                // This at-rule contains orphan declarations, we attach them to a nested declaration rule. Web
                // Inspector expects this rule to occur first in the children rules, and to contain all orphaned
                // property declarations.
                rules.append(createNestedDeclarationsRule());

                if (m_observerWrapper)
                    m_observerWrapper->observer().markRuleBodyContainsImplicitlyNestedProperties();
            }
            rules.appendVector(topContext().m_parsedRules);
        });
    } else if (isFunctionNestedContext()) {
        // Only allow <declaration-rule-list> in @function context.
        rules.appendVector(consumeDeclarationRuleListInNewNestingContext(block, StyleRuleType::Function));
    } else {
        consumeRuleList(block, RuleList::Regular, [&rules](Ref<StyleRuleBase>&& rule) {
            rules.append(WTFMove(rule));
        });
    }
    rules.shrinkToFit();
    return rules;
}

RefPtr<StyleRuleMedia> CSSParser::consumeMediaRule(CSSParserTokenRange prelude, CSSParserTokenRange block)
{
    if (RefPtr observerWrapper = m_observerWrapper.get()) {
        observerWrapper->observer().startRuleHeader(StyleRuleType::Media, observerWrapper->startOffset(prelude));
        observerWrapper->observer().endRuleHeader(observerWrapper->endOffset(prelude));
        observerWrapper->observer().startRuleBody(observerWrapper->previousTokenStartOffset(block));
    }

    auto rules = consumeNestedGroupRules(block);

    if (RefPtr observerWrapper = m_observerWrapper.get())
        observerWrapper->observer().endRuleBody(observerWrapper->endOffset(block));

    return StyleRuleMedia::create(MQ::MediaQueryParser::parse(prelude, m_context), WTFMove(rules));
}

RefPtr<StyleRuleSupports> CSSParser::consumeSupportsRule(CSSParserTokenRange prelude, CSSParserTokenRange block)
{
    auto supported = CSSSupportsParser::supportsCondition(prelude, *this, CSSSupportsParser::ParsingMode::ForAtRuleSupports);
    if (supported == CSSSupportsParser::Invalid)
        return nullptr; // Parse error, invalid @supports condition

    if (RefPtr observerWrapper = m_observerWrapper.get()) {
        observerWrapper->observer().startRuleHeader(StyleRuleType::Supports, observerWrapper->startOffset(prelude));
        observerWrapper->observer().endRuleHeader(observerWrapper->endOffset(prelude));
        observerWrapper->observer().startRuleBody(observerWrapper->previousTokenStartOffset(block));
    }

    auto rules = consumeNestedGroupRules(block);

    if (RefPtr observerWrapper = m_observerWrapper.get())
        observerWrapper->observer().endRuleBody(observerWrapper->endOffset(block));

    return StyleRuleSupports::create(prelude.serialize().trim(deprecatedIsSpaceOrNewline), supported, WTFMove(rules));
}

RefPtr<StyleRuleFontFace> CSSParser::consumeFontFaceRule(CSSParserTokenRange prelude, CSSParserTokenRange block)
{
    if (!prelude.atEnd())
        return nullptr; // Parse error; @font-face prelude should be empty

    if (RefPtr observerWrapper = m_observerWrapper.get()) {
        unsigned endOffset = observerWrapper->endOffset(prelude);
        observerWrapper->observer().startRuleHeader(StyleRuleType::FontFace, observerWrapper->startOffset(prelude));
        observerWrapper->observer().endRuleHeader(endOffset);
        observerWrapper->observer().startRuleBody(endOffset);
        observerWrapper->observer().endRuleBody(endOffset);
    }

    auto declarations = consumeDeclarationListInNewNestingContext(block, StyleRuleType::FontFace);
    return StyleRuleFontFace::create(createStyleProperties(declarations, m_context.mode));
}

// The associated number represents the maximum number of allowed values for this font-feature-values type.
// No value means unlimited (for styleset).
static std::pair<FontFeatureValuesType, std::optional<unsigned>> fontFeatureValuesTypeMappings(CSSAtRuleID id)
{
    switch (id) {
    case CSSAtRuleStyleset:
        return { FontFeatureValuesType::Styleset, { } };
    case CSSAtRuleStylistic:
        return { FontFeatureValuesType::Stylistic, 1 };
    case CSSAtRuleCharacterVariant:
        return { FontFeatureValuesType::CharacterVariant, 2 };
    case CSSAtRuleSwash:
        return { FontFeatureValuesType::Swash, 1 };
    case CSSAtRuleOrnaments:
        return { FontFeatureValuesType::Ornaments, 1 };
    case CSSAtRuleAnnotation:
        return { FontFeatureValuesType::Annotation, 1 };
    default:
        ASSERT_NOT_REACHED();
        return { };
    }
}

RefPtr<StyleRuleFontFeatureValuesBlock> CSSParser::consumeFontFeatureValuesRuleBlock(CSSAtRuleID id, CSSParserTokenRange prelude, CSSParserTokenRange range)
{
    // <feature-value-block> = <font-feature-value-type> { <declaration-list> }
    // <font-feature-value-type> = @stylistic | @historical-forms | @styleset | @character-variant | @swash | @ornaments | @annotation

    // Prelude should be empty.
    if (!prelude.atEnd())
        return { };

    // Block should be present.
    if (range.atEnd())
        return { };

    if (RefPtr observerWrapper = m_observerWrapper.get()) {
        observerWrapper->observer().startRuleHeader(StyleRuleType::FontFeatureValuesBlock, observerWrapper->startOffset(prelude));
        observerWrapper->observer().endRuleHeader(observerWrapper->endOffset(prelude));
        observerWrapper->observer().startRuleBody(observerWrapper->previousTokenStartOffset(range));
        observerWrapper->observer().endRuleBody(observerWrapper->endOffset(range));
    }

    auto [type, maxValues] = fontFeatureValuesTypeMappings(id);

    auto consumeTag = [this](CSSParserTokenRange range, std::optional<unsigned> maxValues) -> std::optional<FontFeatureValuesTag> {
        if (range.peek().type() != IdentToken)
            return { };
        auto name = range.consumeIncludingWhitespace().value();
        if (range.consume().type() != ColonToken)
            return { };
        range.consumeWhitespace();

        auto state = CSS::PropertyParserState { .context = m_context };
        Vector<unsigned> values;
        while (!range.atEnd()) {
            auto value = CSSPropertyParserHelpers::CSSPrimitiveValueResolver<CSS::Integer<CSS::Nonnegative>>::consumeAndResolve(range, state);
            if (!value)
                return { };
            ASSERT(value->isInteger());
            auto tagInteger = value->resolveAsIntegerDeprecated();
            ASSERT(tagInteger >= 0);
            values.append(unsignedCast(tagInteger));
            if (maxValues && values.size() > *maxValues)
                return { };
        }
        if (values.isEmpty())
            return { };

        return { FontFeatureValuesTag { name.toString(), values } };
    };

    Vector<FontFeatureValuesTag> tags;
    while (!range.atEnd()) {
        switch (range.peek().type()) {
        case NonNewlineWhitespaceToken:
        case NewlineToken:
        case SemicolonToken:
            range.consume();
            break;
        case IdentToken: {
            auto declarationStart = range;

            while (!range.atEnd() && range.peek().type() != SemicolonToken)
                range.consumeComponentValue();

            if (auto tag = consumeTag(declarationStart.rangeUntil(range), maxValues))
                tags.append(*tag);

            break;
        }
        default: // Parse error, unexpected token in declaration list
            while (!range.atEnd() && range.peek().type() != SemicolonToken)
                range.consumeComponentValue();
            break;
        }
    }
    return StyleRuleFontFeatureValuesBlock::create(type, tags);
}

RefPtr<StyleRuleFontFeatureValues> CSSParser::consumeFontFeatureValuesRule(CSSParserTokenRange prelude, CSSParserTokenRange block)
{
    // @font-feature-values <family-name># { <declaration-list> }

    auto originalPrelude = prelude;
    auto fontFamilies = CSSPropertyParserHelpers::consumeFontFeatureValuesPreludeFamilyNameList(prelude, m_context);
    if (fontFamilies.isEmpty() || !prelude.atEnd())
        return nullptr;

    if (RefPtr observerWrapper = m_observerWrapper.get()) {
        observerWrapper->observer().startRuleHeader(StyleRuleType::FontFeatureValues, observerWrapper->startOffset(originalPrelude));
        observerWrapper->observer().endRuleHeader(observerWrapper->endOffset(prelude));
        observerWrapper->observer().startRuleBody(observerWrapper->previousTokenStartOffset(block));
    }

    Vector<Ref<StyleRuleBase>> rules;
    consumeRuleList(block, RuleList::FontFeatureValues, [&rules](auto rule) {
        rules.append(rule);
    });

    if (RefPtr observerWrapper = m_observerWrapper.get())
        observerWrapper->observer().endRuleBody(observerWrapper->endOffset(block));

    // Convert block rules to value (remove duplicate...etc)
    auto fontFeatureValues = FontFeatureValues::create();

    for (auto& block : rules) {
        if (RefPtr fontFeatureValuesBlockRule = dynamicDowncast<StyleRuleFontFeatureValuesBlock>(block.get()))
            fontFeatureValues->updateOrInsertForType(fontFeatureValuesBlockRule->fontFeatureValuesType(), fontFeatureValuesBlockRule->tags());
    }

    return StyleRuleFontFeatureValues::create(fontFamilies, WTFMove(fontFeatureValues));
}

RefPtr<StyleRuleFontPaletteValues> CSSParser::consumeFontPaletteValuesRule(CSSParserTokenRange prelude, CSSParserTokenRange block)
{
    RefPtr name = CSSPropertyParserHelpers::consumeDashedIdent(prelude);
    if (!name || !prelude.atEnd())
        return nullptr; // Parse error; expected custom ident in @font-palette-values header

    if (RefPtr observerWrapper = m_observerWrapper.get()) {
        unsigned endOffset = observerWrapper->endOffset(prelude);
        observerWrapper->observer().startRuleHeader(StyleRuleType::FontPaletteValues, observerWrapper->startOffset(prelude));
        observerWrapper->observer().endRuleHeader(endOffset);
        observerWrapper->observer().startRuleBody(endOffset);
        observerWrapper->observer().endRuleBody(endOffset);
    }

    auto declarations = consumeDeclarationListInNewNestingContext(block, StyleRuleType::FontPaletteValues);
    Ref properties = createStyleProperties(declarations, m_context.mode);

    auto fontFamilies = [&] {
        Vector<AtomString> fontFamilies;
        auto append = [&](auto& value) {
            if (value.isFontFamily())
                fontFamilies.append(AtomString { value.stringValue() });
        };
        RefPtr cssFontFamily = properties->getPropertyCSSValue(CSSPropertyFontFamily);
        if (!cssFontFamily)
            return fontFamilies;
        if (RefPtr families = dynamicDowncast<CSSValueList>(*cssFontFamily)) {
            for (Ref item : *families)
                append(downcast<CSSPrimitiveValue>(item.get()));
            return fontFamilies;
        }
        if (RefPtr family = dynamicDowncast<CSSPrimitiveValue>(cssFontFamily.releaseNonNull()))
            append(*family);
        return fontFamilies;
    }();

    std::optional<FontPaletteIndex> basePalette;
    if (auto basePaletteValue = properties->getPropertyCSSValue(CSSPropertyBasePalette)) {
        const auto& primitiveValue = downcast<CSSPrimitiveValue>(*basePaletteValue);
        if (primitiveValue.isInteger())
            basePalette = FontPaletteIndex(primitiveValue.resolveAsIntegerDeprecated<unsigned>());
        else if (primitiveValue.valueID() == CSSValueLight)
            basePalette = FontPaletteIndex(FontPaletteIndex::Type::Light);
        else if (primitiveValue.valueID() == CSSValueDark)
            basePalette = FontPaletteIndex(FontPaletteIndex::Type::Dark);
    }

    Vector<FontPaletteValues::OverriddenColor> overrideColors;
    if (auto overrideColorsValue = properties->getPropertyCSSValue(CSSPropertyOverrideColors)) {
        overrideColors = WTF::compactMap(downcast<CSSValueList>(*overrideColorsValue), [](const auto& item) -> std::optional<FontPaletteValues::OverriddenColor> {
            Ref pair = downcast<CSSValuePair>(item);
            Ref first = pair->first();
            Ref second = pair->second();

            auto key = downcast<CSSPrimitiveValue>(first)->template resolveAsIntegerDeprecated<unsigned>();
            auto color = CSSColorValue::absoluteColor(second);
            if (!color.isValid())
                return { };

            return { { key, WTFMove(color) } };
        });
    }

    return StyleRuleFontPaletteValues::create(AtomString { name->stringValue() }, WTFMove(fontFamilies), WTFMove(basePalette), WTFMove(overrideColors));
}

RefPtr<StyleRuleKeyframes> CSSParser::consumeKeyframesRule(CSSParserTokenRange prelude, CSSParserTokenRange block)
{
    CSSParserTokenRange rangeCopy = prelude; // For inspector callbacks
    const CSSParserToken& nameToken = prelude.consumeIncludingWhitespace();
    if (!prelude.atEnd())
        return nullptr; // Parse error; expected single non-whitespace token in @keyframes header

    if (nameToken.type() == IdentToken) {
        // According to the CSS Values specification, identifier-based keyframe names
        // are not allowed to be CSS wide keywords or "default". And CSS Animations
        // additionally excludes the "none" keyword.
        if (!isValidCustomIdentifier(nameToken.id()) || nameToken.id() == CSSValueNone)
            return nullptr;
    } else if (nameToken.type() != StringToken)
        return nullptr; // Parse error; expected ident token or string in @keyframes header

    auto name = nameToken.value().toAtomString();

    if (RefPtr observerWrapper = m_observerWrapper.get()) {
        observerWrapper->observer().startRuleHeader(StyleRuleType::Keyframes, observerWrapper->startOffset(rangeCopy));
        observerWrapper->observer().endRuleHeader(observerWrapper->endOffset(prelude));
        observerWrapper->observer().startRuleBody(observerWrapper->previousTokenStartOffset(block));
        observerWrapper->observer().endRuleBody(observerWrapper->endOffset(block));
    }

    auto keyframeRule = StyleRuleKeyframes::create(name);
    consumeRuleList(block, RuleList::Keyframes, [keyframeRule](Ref<StyleRuleBase> keyframe) {
        keyframeRule->parserAppendKeyframe(downcast<const StyleRuleKeyframe>(keyframe.ptr()));
    });

    keyframeRule->shrinkToFit();
    return keyframeRule;
}

RefPtr<StyleRulePage> CSSParser::consumePageRule(CSSParserTokenRange prelude, CSSParserTokenRange block)
{
    auto selectorList = parsePageSelector(prelude, protectedStyleSheet().get());
    if (selectorList.isEmpty())
        return nullptr; // Parse error, invalid @page selector

    if (RefPtr observerWrapper = m_observerWrapper.get()) {
        unsigned endOffset = observerWrapper->endOffset(prelude);
        observerWrapper->observer().startRuleHeader(StyleRuleType::Page, observerWrapper->startOffset(prelude));
        observerWrapper->observer().endRuleHeader(endOffset);
    }

    auto declarations = consumeDeclarationListInNewNestingContext(block, StyleRuleType::Page);

    return StyleRulePage::create(createStyleProperties(declarations, m_context.mode), WTFMove(selectorList));
}

RefPtr<StyleRuleCounterStyle> CSSParser::consumeCounterStyleRule(CSSParserTokenRange prelude, CSSParserTokenRange block)
{
    auto rangeCopy = prelude; // For inspector callbacks
    auto name = CSSPropertyParserHelpers::consumeCounterStyleNameInPrelude(rangeCopy, m_context.mode);
    if (name.isNull())
        return nullptr;

    if (RefPtr observerWrapper = m_observerWrapper.get()) {
        observerWrapper->observer().startRuleHeader(StyleRuleType::CounterStyle, observerWrapper->startOffset(rangeCopy));
        observerWrapper->observer().endRuleHeader(observerWrapper->endOffset(prelude));
        observerWrapper->observer().startRuleBody(observerWrapper->previousTokenStartOffset(block));
        observerWrapper->observer().endRuleBody(observerWrapper->endOffset(block));
    }

    auto declarations = consumeDeclarationListInNewNestingContext(block, StyleRuleType::CounterStyle);
    auto descriptors = CSSCounterStyleDescriptors::create(name, createStyleProperties(declarations, m_context.mode));
    if (!descriptors.isValid())
        return nullptr;
    return StyleRuleCounterStyle::create(name, WTFMove(descriptors));
}

RefPtr<StyleRuleViewTransition> CSSParser::consumeViewTransitionRule(CSSParserTokenRange prelude, CSSParserTokenRange block)
{
    if (!m_context.propertySettings.crossDocumentViewTransitionsEnabled)
        return nullptr;

    if (!prelude.atEnd())
        return nullptr; // Parse error; @view-transition prelude should be empty

    if (RefPtr observerWrapper = m_observerWrapper.get()) {
        unsigned endOffset = observerWrapper->endOffset(prelude);
        observerWrapper->observer().startRuleHeader(StyleRuleType::ViewTransition, observerWrapper->startOffset(prelude));
        observerWrapper->observer().endRuleHeader(endOffset);
        observerWrapper->observer().startRuleBody(endOffset);
        observerWrapper->observer().endRuleBody(endOffset);
    }

    auto declarations = consumeDeclarationListInNewNestingContext(block, StyleRuleType::ViewTransition);
    return StyleRuleViewTransition::create(createStyleProperties(declarations, m_context.mode));
}

RefPtr<StyleRulePositionTry> CSSParser::consumePositionTryRule(CSSParserTokenRange prelude, CSSParserTokenRange block)
{
    if (!m_context.propertySettings.cssAnchorPositioningEnabled)
        return nullptr;

    // Prelude should ONLY be a <dashed-ident>.
    AtomString ruleName { CSSPropertyParserHelpers::consumeDashedIdentRaw(prelude) };
    if (!ruleName)
        return nullptr;
    if (!prelude.atEnd())
        return nullptr;

    if (RefPtr observerWrapper = m_observerWrapper.get()) {
        unsigned endOffset = observerWrapper->endOffset(prelude);
        observerWrapper->observer().startRuleHeader(StyleRuleType::PositionTry, observerWrapper->startOffset(prelude));
        observerWrapper->observer().endRuleHeader(endOffset);
        observerWrapper->observer().startRuleBody(endOffset);
        observerWrapper->observer().endRuleBody(endOffset);
    }

    auto declarations = consumeDeclarationListInNewNestingContext(block, StyleRuleType::PositionTry);
    return StyleRulePositionTry::create(WTFMove(ruleName), createStyleProperties(declarations, m_context.mode));
}

RefPtr<StyleRuleFunction> CSSParser::consumeFunctionRule(CSSParserTokenRange prelude, CSSParserTokenRange block)
{
    if (!m_context.propertySettings.cssFunctionAtRuleEnabled)
        return nullptr;

    // https://drafts.csswg.org/css-mixins/#function-rule
    // <@function> = @function <function-token> <function-parameter>#? ) [ returns <css-type> ]?

    if (prelude.peek().type() != FunctionToken)
        return nullptr;

    auto name = prelude.peek().value().toAtomString();
    auto parametersRange = CSSPropertyParserHelpers::consumeFunction(prelude);

    // <function-parameter>#?
    Vector<StyleRuleFunction::Parameter> parameters;
    while (!parametersRange.atEnd()) {
        auto consumeParameter = [&]() -> std::optional<StyleRuleFunction::Parameter> {
            // <function-parameter> = <custom-property-name> <css-type>? [ : <default-value> ]?

            auto nameToken = parametersRange.consumeIncludingWhitespace();
            if (nameToken.type() != IdentToken)
                return { };

            auto parameter = StyleRuleFunction::Parameter { };

            // <custom-property-name>
            parameter.name = nameToken.value().toAtomString();
            if (!isCustomPropertyName(parameter.name))
                return { };

            if (parametersRange.atEnd() || parametersRange.peek().type() == CommaToken)
                return parameter;

            // <css-type>?
            if (parametersRange.peek().type() != ColonToken) {
                auto type = CSSCustomPropertySyntax::consumeType(parametersRange);
                if (!type)
                    return { };
                parameter.type = *type;
            }

            // [ : <default-value> ]?
            if (parametersRange.peek().type() == ColonToken) {
                parametersRange.consumeIncludingWhitespace();
                // <default-value> = <declaration-value>
                auto defaultRangeStart = parametersRange;
                while (!parametersRange.atEnd() && parametersRange.peek().type() != CommaToken) {
                    if (parametersRange.peek().type() == DelimiterToken && parametersRange.peek().delimiter() == '!')
                        return { };
                    parametersRange.consumeIncludingWhitespace();
                }

                auto defaultRange = defaultRangeStart.rangeUntil(parametersRange);

                // "If a default value and a parameter type are both provided, then the default value must parse
                // successfully according to that parameter type’s syntax. Otherwise, the @function rule is invalid."
                if (!CSSPropertyParser::isValidCustomPropertyValueForSyntax(parameter.type, defaultRange, m_context))
                    return { };

                parameter.defaultValue = CSSVariableData::create(defaultRange);
            }

            if (parametersRange.atEnd() || parametersRange.peek().type() == CommaToken)
                return parameter;

            return { };
        };

        auto parameter = consumeParameter();
        if (!parameter)
            return nullptr;
        parameters.append(*parameter);

        if (parametersRange.peek().type() == CommaToken)
            parametersRange.consumeIncludingWhitespace();
    }

    auto returnType = CSSCustomPropertySyntax::universal();

    // [ returns <css-type> ]?
    if (prelude.peek().type() == IdentToken && equalLettersIgnoringASCIICase(prelude.peek().value(), "returns"_s)) {
        prelude.consumeIncludingWhitespace();

        auto specifiedReturnType = CSSCustomPropertySyntax::consumeType(prelude);
        if (!specifiedReturnType)
            return nullptr;
        returnType = *specifiedReturnType;
    }

    if (!prelude.atEnd())
        return nullptr;

    m_ancestorRuleTypeStack.append(CSSParserEnum::NestedContextType::Function);
    auto functionBody = consumeDeclarationRuleListInNewNestingContext(block, StyleRuleType::Function);
    m_ancestorRuleTypeStack.removeLast();

    return StyleRuleFunction::create(name, WTFMove(parameters), WTFMove(returnType), WTFMove(functionBody));
}

RefPtr<StyleRuleScope> CSSParser::consumeScopeRule(CSSParserTokenRange prelude, CSSParserTokenRange block)
{
    auto preludeRangeCopy = prelude;
    CSSSelectorList scopeStart;
    CSSSelectorList scopeEnd;

    if (!prelude.atEnd()) {
        auto consumePrelude = [&] {
            auto consumeScope = [&](auto& scope, auto ancestorRuleType) {
                // Consume the left parenthesis
                if (prelude.peek().type() != LeftParenthesisToken)
                    return false;
                prelude.consumeIncludingWhitespace();

                // Determine the range for the selector list
                auto selectorListRangeStart = prelude;
                while (!prelude.atEnd() && prelude.peek().type() != RightParenthesisToken)
                    prelude.consumeComponentValue();
                auto selectorListRange = selectorListRangeStart.rangeUntil(prelude);

                // Parse the selector list range
                auto mutableSelectorList = parseMutableCSSSelectorList(selectorListRange, m_context, protectedStyleSheet().get(), ancestorRuleType, CSSParserEnum::IsForgiving::No, CSSSelectorParser::DisallowPseudoElement::Yes);
                if (mutableSelectorList.isEmpty())
                    return false;

                // Consume the right parenthesis
                if (prelude.peek().type() != RightParenthesisToken)
                    return false;
                prelude.consumeIncludingWhitespace();

                // Return the correctly parsed scope
                scope = CSSSelectorList { WTFMove(mutableSelectorList) };
                return true;
            };
            auto successScopeStart = consumeScope(scopeStart, lastAncestorRuleType());
            if (successScopeStart && prelude.atEnd())
                return true;
            if (prelude.peek().type() != IdentToken)
                return false;
            auto to = prelude.consumeIncludingWhitespace();
            if (!equalLettersIgnoringASCIICase(to.value(), "to"_s))
                return false;
            if (!consumeScope(scopeEnd, CSSParserEnum::NestedContextType::Scope)) // scopeEnd is always considered nested, at least by the scopeStart
                return false;
            if (!prelude.atEnd())
                return false;
            return true;
        };
        if (!consumePrelude())
            return nullptr;
    }

    if (RefPtr observerWrapper = m_observerWrapper.get()) {
        observerWrapper->observer().startRuleHeader(StyleRuleType::Scope, observerWrapper->startOffset(preludeRangeCopy));
        observerWrapper->observer().endRuleHeader(observerWrapper->endOffset(prelude));
        observerWrapper->observer().startRuleBody(observerWrapper->previousTokenStartOffset(block));
        observerWrapper->observer().endRuleBody(observerWrapper->endOffset(block));
    }

    m_ancestorRuleTypeStack.append(CSSParserEnum::NestedContextType::Scope);
    auto rules = consumeNestedGroupRules(block);
    m_ancestorRuleTypeStack.removeLast();
    Ref rule = StyleRuleScope::create(WTFMove(scopeStart), WTFMove(scopeEnd), WTFMove(rules));
    if (RefPtr styleSheet = m_styleSheet)
        rule->setStyleSheetContents(*styleSheet);
    return rule;
}

RefPtr<StyleRuleStartingStyle> CSSParser::consumeStartingStyleRule(CSSParserTokenRange prelude, CSSParserTokenRange block)
{
    if (!prelude.atEnd())
        return nullptr;

    if (RefPtr observerWrapper = m_observerWrapper.get()) {
        observerWrapper->observer().startRuleHeader(StyleRuleType::StartingStyle, observerWrapper->startOffset(prelude));
        observerWrapper->observer().endRuleHeader(observerWrapper->endOffset(prelude));
        observerWrapper->observer().startRuleBody(observerWrapper->previousTokenStartOffset(block));
    }

    auto rules = consumeNestedGroupRules(block);

    if (RefPtr observerWrapper = m_observerWrapper.get())
        observerWrapper->observer().endRuleBody(observerWrapper->endOffset(block));

    return StyleRuleStartingStyle::create(WTFMove(rules));
}

RefPtr<StyleRuleInternalBaseAppearance> CSSParser::consumeInternalBaseAppearanceRule(CSSParserTokenRange prelude, CSSParserTokenRange block)
{
    if (m_context.mode != UASheetMode)
        return nullptr;

    if (!prelude.atEnd())
        return nullptr;

    if (RefPtr observerWrapper = m_observerWrapper.get()) {
        observerWrapper->observer().startRuleHeader(StyleRuleType::InternalBaseAppearance, observerWrapper->startOffset(prelude));
        observerWrapper->observer().endRuleHeader(observerWrapper->endOffset(prelude));
        observerWrapper->observer().startRuleBody(observerWrapper->previousTokenStartOffset(block));
    }

    auto rules = consumeNestedGroupRules(block);

    if (RefPtr observerWrapper = m_observerWrapper.get())
        observerWrapper->observer().endRuleBody(observerWrapper->endOffset(block));

    return StyleRuleInternalBaseAppearance::create(WTFMove(rules));
}

RefPtr<StyleRuleLayer> CSSParser::consumeLayerRule(CSSParserTokenRange prelude, std::optional<CSSParserTokenRange> block)
{
    auto preludeCopy = prelude;

    if (!block) {
        // List syntax.
        Vector<CascadeLayerName> nameList;
        while (true) {
            auto name = consumeCascadeLayerName(prelude, AllowAnonymous::No);
            if (!name)
                return nullptr;
            nameList.append(*name);

            if (prelude.atEnd())
                break;

            auto commaToken = prelude.consumeIncludingWhitespace();
            if (commaToken.type() != CommaToken)
                return { };
        }

        if (RefPtr observerWrapper = m_observerWrapper.get()) {
            unsigned endOffset = observerWrapper->endOffset(preludeCopy);
            observerWrapper->observer().startRuleHeader(StyleRuleType::LayerStatement, observerWrapper->startOffset(preludeCopy));
            observerWrapper->observer().endRuleHeader(endOffset);
            observerWrapper->observer().startRuleBody(endOffset);
            observerWrapper->observer().endRuleBody(endOffset);
        }

        return StyleRuleLayer::createStatement(WTFMove(nameList));
    }

    auto name = consumeCascadeLayerName(prelude, AllowAnonymous::Yes);
    if (!name)
        return nullptr;

    // No comma separated list when using the block syntax.
    if (!prelude.atEnd())
        return nullptr;

    if (RefPtr observerWrapper = m_observerWrapper.get()) {
        observerWrapper->observer().startRuleHeader(StyleRuleType::LayerBlock, observerWrapper->startOffset(preludeCopy));
        observerWrapper->observer().endRuleHeader(observerWrapper->endOffset(preludeCopy));
        observerWrapper->observer().startRuleBody(observerWrapper->previousTokenStartOffset(*block));
    }

    auto rules = consumeNestedGroupRules(*block);

    if (RefPtr observerWrapper = m_observerWrapper.get())
        observerWrapper->observer().endRuleBody(observerWrapper->endOffset(*block));

    return StyleRuleLayer::createBlock(WTFMove(*name), WTFMove(rules));
}

RefPtr<StyleRuleContainer> CSSParser::consumeContainerRule(CSSParserTokenRange prelude, CSSParserTokenRange block)
{
    if (prelude.atEnd())
        return nullptr;

    auto originalPreludeRange = prelude;

    auto query = CQ::ContainerQueryParser::consumeContainerQuery(prelude, m_context);
    if (!query)
        return nullptr;

    prelude.consumeWhitespace();
    if (!prelude.atEnd())
        return nullptr;

    if (RefPtr observerWrapper = m_observerWrapper.get()) {
        observerWrapper->observer().startRuleHeader(StyleRuleType::Container, observerWrapper->startOffset(originalPreludeRange));
        observerWrapper->observer().endRuleHeader(observerWrapper->endOffset(originalPreludeRange));
        observerWrapper->observer().startRuleBody(observerWrapper->previousTokenStartOffset(block));
    }

    auto rules = consumeNestedGroupRules(block);

    if (RefPtr observerWrapper = m_observerWrapper.get())
        observerWrapper->observer().endRuleBody(observerWrapper->endOffset(block));

    return StyleRuleContainer::create(WTFMove(*query), WTFMove(rules));
}

RefPtr<StyleRuleProperty> CSSParser::consumePropertyRule(CSSParserTokenRange prelude, CSSParserTokenRange block)
{
    auto nameToken = prelude.consumeIncludingWhitespace();
    if (nameToken.type() != IdentToken || !prelude.atEnd())
        return nullptr;

    auto name = nameToken.value().toAtomString();
    if (!isCustomPropertyName(name))
        return nullptr;

    auto declarations = consumeDeclarationListInNewNestingContext(block, StyleRuleType::Property);

    auto descriptor = StyleRuleProperty::Descriptor { name };

    for (auto& property : declarations) {
        switch (property.id()) {
        case CSSPropertySyntax:
            descriptor.syntax = Ref { downcast<CSSPrimitiveValue>(*property.value()) }->stringValue();
            continue;
        case CSSPropertyInherits:
            descriptor.inherits = property.value()->valueID() == CSSValueTrue;
            break;
        case CSSPropertyInitialValue:
            descriptor.initialValue = Ref { downcast<CSSCustomPropertyValue>(*property.value()) }->asVariableData();
            break;
        default:
            break;
        };
    };

    // "The inherits descriptor is required for the @property rule to be valid; if it’s missing, the @property rule is invalid."
    // https://drafts.css-houdini.org/css-properties-values-api/#inherits-descriptor
    if (!descriptor.inherits)
        return nullptr;

    // "If the provided string is not a valid syntax string, the descriptor is invalid and must be ignored."
    // https://drafts.css-houdini.org/css-properties-values-api/#the-syntax-descriptor
    if (descriptor.syntax.isNull())
        return nullptr;
    auto syntax = CSSCustomPropertySyntax::parse(descriptor.syntax);
    if (!syntax)
        return nullptr;

    // "The initial-value descriptor is optional only if the syntax is the universal syntax definition,
    // otherwise the descriptor is required; if it's missing, the entire rule is invalid and must be ignored."
    if (!syntax->isUniversal()) {
        if (!descriptor.initialValue)
            return nullptr;
    }

    auto initialValueIsValid = [&] {
        auto tokenRange = descriptor.initialValue->tokenRange();
        auto dependencies = CSSPropertyParser::collectParsedCustomPropertyValueDependencies(*syntax, tokenRange, m_context);
        if (!dependencies.isComputationallyIndependent())
            return false;
        auto containsVariable = CSSVariableParser::containsValidVariableReferences(descriptor.initialValue->tokenRange(), m_context);
        if (containsVariable)
            return false;
        return true;
    };
    if (descriptor.initialValue && !initialValueIsValid())
        return nullptr;

    return StyleRuleProperty::create(WTFMove(descriptor));
}

RefPtr<StyleRuleKeyframe> CSSParser::consumeKeyframeStyleRule(CSSParserTokenRange prelude, CSSParserTokenRange block)
{
    auto state = CSS::PropertyParserState { .context = m_context };
    auto keyList = CSSPropertyParserHelpers::consumeKeyframeKeyList(prelude, state);
    if (keyList.isEmpty())
        return nullptr;

    if (RefPtr observerWrapper = m_observerWrapper.get()) {
        observerWrapper->observer().startRuleHeader(StyleRuleType::Keyframe, observerWrapper->startOffset(prelude));
        observerWrapper->observer().endRuleHeader(observerWrapper->endOffset(prelude));
    }

    auto declarations = consumeDeclarationListInNewNestingContext(block, StyleRuleType::Keyframe);

    return StyleRuleKeyframe::create(WTFMove(keyList), createStyleProperties(declarations, m_context.mode));
}

static void observeSelectors(CSSParserObserverWrapper& wrapper, CSSParserTokenRange selectors)
{
    // This is easier than hooking into the CSSSelectorParser
    selectors.consumeWhitespace();
    CSSParserTokenRange originalRange = selectors;
    wrapper.observer().startRuleHeader(StyleRuleType::Style, wrapper.startOffset(originalRange));

    while (!selectors.atEnd()) {
        auto selectorStart = selectors;
        while (!selectors.atEnd() && selectors.peek().type() != CommaToken)
            selectors.consumeComponentValue();
        auto selector = selectorStart.rangeUntil(selectors);
        selectors.consumeIncludingWhitespace();

        wrapper.observer().observeSelector(wrapper.startOffset(selector), wrapper.endOffset(selector));
    }

    wrapper.observer().endRuleHeader(wrapper.endOffset(originalRange));
}

RefPtr<StyleRuleBase> CSSParser::consumeStyleRule(CSSParserTokenRange prelude, CSSParserTokenRange block)
{
    auto preludeCopyForInspector = prelude;
    auto mutableSelectorList = parseMutableCSSSelectorList(prelude, m_context, protectedStyleSheet().get(), lastAncestorRuleType(), CSSParserEnum::IsForgiving::No, CSSSelectorParser::DisallowPseudoElement::No);

    if (mutableSelectorList.isEmpty())
        return nullptr; // Parse error, invalid selector list

    CSSSelectorList selectorList { WTFMove(mutableSelectorList) };
    ASSERT(!selectorList.isEmpty());

    if (RefPtr observerWrapper = m_observerWrapper.get())
        observeSelectors(*observerWrapper, preludeCopyForInspector);

    RefPtr<StyleRuleBase> styleRule;

    runInNewNestingContext([&] {
        {
            m_ancestorRuleTypeStack.append(CSSParserEnum::NestedContextType::Style);
            consumeStyleBlock(block, StyleRuleType::Style);
            m_ancestorRuleTypeStack.removeLast();
        }

        auto nestedRules = WTFMove(topContext().m_parsedRules);
        Ref properties = createStyleProperties(topContext().m_parsedProperties, m_context.mode);

        // We save memory by creating a simple StyleRule instead of a heavier StyleRuleWithNesting when we don't need the CSS Nesting features.
        if (nestedRules.isEmpty() && !selectorList.hasExplicitNestingParent() && !isStyleNestedContext())
            styleRule = StyleRule::create(WTFMove(properties), m_context.hasDocumentSecurityOrigin, WTFMove(selectorList));
        else
            styleRule = StyleRuleWithNesting::create(WTFMove(properties), m_context.hasDocumentSecurityOrigin, WTFMove(selectorList), WTFMove(nestedRules));
    });

    return styleRule;
}

// https://drafts.csswg.org/css-syntax/#consume-block-contents
// https://drafts.csswg.org/css-syntax/#block-contents
void CSSParser::consumeBlockContent(CSSParserTokenRange range, StyleRuleType ruleType, OptionSet<BlockAllowedRule> blockAllowedRules, ParsingStyleDeclarationsInRuleList isParsingStyleDeclarationsInRuleList)
{
    ASSERT(topContext().m_parsedProperties.isEmpty());
    ASSERT(topContext().m_parsedRules.isEmpty());

    // All the current callers support declarations so the no-declarations case is not implemented.
    ASSERT(blockAllowedRules.contains(BlockAllowedRule::Declarations));

    RefPtr observerWrapper = m_observerWrapper.get();

    bool useObserver = observerWrapper && (ruleType == StyleRuleType::Style || ruleType == StyleRuleType::Keyframe || ruleType == StyleRuleType::Page);
    if (useObserver) {
        if (isParsingStyleDeclarationsInRuleList == ParsingStyleDeclarationsInRuleList::No)
            observerWrapper->observer().startRuleBody(observerWrapper->previousTokenStartOffset(range));
        observerWrapper->skipCommentsBefore(range, true);
    }

    auto consumeUntilSemicolon = [&] {
        while (!range.atEnd() && range.peek().type() != SemicolonToken)
            range.consumeComponentValue();
    };

    ParsedPropertyVector initialDeclarationBlock;
    bool initialDeclarationBlockFinished = false;
    auto storeDeclarations = [&] {
        // We don't wrap the first declaration block, we store it until the end of the style rule.
        // For @function we always use the declaration block.
        if (!initialDeclarationBlockFinished && ruleType != StyleRuleType::Function) {
            initialDeclarationBlockFinished = true;
            std::swap(initialDeclarationBlock, topContext().m_parsedProperties);
            return;
        }

        // Nothing to wrap
        if (topContext().m_parsedProperties.isEmpty())
            return;

        ParsedPropertyVector properties;
        std::swap(properties, topContext().m_parsedProperties);

        if (ruleType == StyleRuleType::Function) {
            auto rule = StyleRuleFunctionDeclarations::create(createStyleProperties(properties, m_context.mode));
            topContext().m_parsedRules.append(WTFMove(rule));
            return;
        }

        auto rule = StyleRuleNestedDeclarations::create(createStyleProperties(properties, m_context.mode));
        topContext().m_parsedRules.append(WTFMove(rule));
    };

    while (!range.atEnd()) {
        const auto initialRange = range;

        auto consumeNestedRuleOrInvalidSyntax = [&] {
            if (blockAllowedRules.contains(BlockAllowedRule::QualifiedRules)) {
                ASSERT(isStyleNestedContext());
                // For block, we try to consume a qualified rule (~= a style rule).
                // This consumes tokens and deals with error recovery
                // in the case of invalid syntax.
                RefPtr rule = consumeQualifiedRule(range, AllowedRules::RegularRules);
                if (!rule)
                    return;
                if (!rule->isStyleRule())
                    return;
                storeDeclarations();
                topContext().m_parsedRules.append(rule.releaseNonNull());
            } else {
                // https://drafts.csswg.org/css-syntax/#typedef-declaration-list
                // For declaration list, we consume invalid tokens until next recovery point.
                range = initialRange;
                consumeUntilSemicolon();
            }
        };

        switch (range.peek().type()) {
        case NonNewlineWhitespaceToken:
        case NewlineToken:
        case SemicolonToken:
            range.consume();
            break;
        case IdentToken: {
            auto declarationStart = range;

            if (useObserver)
                observerWrapper->yieldCommentsBefore(range);

            consumeUntilSemicolon();

            auto declarationRange = declarationStart.rangeUntil(range);
            auto isValidDeclaration = consumeDeclaration(declarationRange, ruleType);

            if (useObserver)
                observerWrapper->skipCommentsBefore(range, false);

            if (!isValidDeclaration) {
                // If it's not a valid declaration, we rewind the parser and try to parse it as a nested style rule.
                range = initialRange;
                consumeNestedRuleOrInvalidSyntax();
            }
            break;
        }
        case AtKeywordToken: {
            if (blockAllowedRules.contains(BlockAllowedRule::AtRules)) {
                auto allowedRules = ruleType == StyleRuleType::Function ? AllowedRules::ConditionalGroupRules : AllowedRules::RegularRules;
                RefPtr rule = consumeAtRule(range, allowedRules);
                if (!rule)
                    break;
                auto lastAncestor = lastAncestorRuleType();
                ASSERT(lastAncestor);
                // Style rule only support nested group rule.
                if (*lastAncestor == CSSParserEnum::NestedContextType::Style && !rule->isGroupRule())
                    break;
                storeDeclarations();
                topContext().m_parsedRules.append(rule.releaseNonNull());
            } else {
                // Rule will be ignored, but consuming the tokens is necessary.
                RefPtr rule = consumeAtRule(range, AllowedRules::NoRules);
                ASSERT_UNUSED(rule, !rule);
            }
            break;
        }
        default:
            consumeNestedRuleOrInvalidSyntax();
        }
    }

    // Store trailing declarations if any
    storeDeclarations();

    // Restore the initial declaration block
    if (!initialDeclarationBlock.isEmpty())
        std::swap(initialDeclarationBlock, topContext().m_parsedProperties);

    // Yield remaining comments
    if (useObserver) {
        observerWrapper->yieldCommentsBefore(range);
        if (isParsingStyleDeclarationsInRuleList == ParsingStyleDeclarationsInRuleList::No)
            observerWrapper->observer().endRuleBody(observerWrapper->endOffset(range));
    }
}

ParsedPropertyVector CSSParser::consumeDeclarationListInNewNestingContext(CSSParserTokenRange range, StyleRuleType ruleType)
{
    ParsedPropertyVector result;
    runInNewNestingContext([&] {
        consumeDeclarationList(range, ruleType);
        result = WTFMove(topContext().m_parsedProperties);
    });
    return result;
}

Vector<Ref<StyleRuleBase>> CSSParser::consumeDeclarationRuleListInNewNestingContext(CSSParserTokenRange range, StyleRuleType ruleType)
{
    Vector<Ref<StyleRuleBase>> rules;
    runInNewNestingContext([&] {
        consumeDeclarationRuleList(range, ruleType);
        rules.appendVector(topContext().m_parsedRules);
    });
    return rules;
}

void CSSParser::consumeDeclarationList(CSSParserTokenRange range, StyleRuleType ruleType)
{
    // https://drafts.csswg.org/css-syntax-3/#block-contents
    // <declaration-list>: only declarations are allowed; at-rules and qualified rules are automatically invalid.
    consumeBlockContent(range, ruleType, BlockAllowedRule::Declarations);
}

void CSSParser::consumeDeclarationRuleList(CSSParserTokenRange range, StyleRuleType ruleType)
{
    // <declaration-rule-list>: declarations and at-rules are allowed; qualified rules are automatically invalid.
    consumeBlockContent(range, ruleType, { BlockAllowedRule::Declarations, BlockAllowedRule::AtRules });
}

void CSSParser::consumeStyleBlock(CSSParserTokenRange range, StyleRuleType ruleType, ParsingStyleDeclarationsInRuleList isParsingStyleDeclarationsInRuleList)
{
    // <block-contents>
    consumeBlockContent(range, ruleType, { BlockAllowedRule::Declarations, BlockAllowedRule::QualifiedRules, BlockAllowedRule::AtRules }, isParsingStyleDeclarationsInRuleList);
}

IsImportant CSSParser::consumeTrailingImportantAndWhitespace(CSSParserTokenRange& range)
{
    range.trimTrailingWhitespace();
    if (range.size() < 2)
        return IsImportant::No;

    auto removeImportantRange = range;
    if (auto& last = removeImportantRange.consumeLast(); last.type() != IdentToken || !equalLettersIgnoringASCIICase(last.value(), "important"_s))
        return IsImportant::No;

    removeImportantRange.trimTrailingWhitespace();
    if (auto& last = removeImportantRange.consumeLast(); last.type() != DelimiterToken || last.delimiter() != '!')
        return IsImportant::No;

    removeImportantRange.trimTrailingWhitespace();
    range = removeImportantRange;
    return IsImportant::Yes;
}

// Check if a CSS rule type does not allow declarations with !important.
static bool ruleDoesNotAllowImportant(StyleRuleType type)
{
    return type == StyleRuleType::CounterStyle
        || type == StyleRuleType::FontFace
        || type == StyleRuleType::FontPaletteValues
        || type == StyleRuleType::Keyframe
        || type == StyleRuleType::PositionTry
        || type == StyleRuleType::ViewTransition
        || type == StyleRuleType::Function;
}

// https://drafts.csswg.org/css-syntax/#consume-declaration
bool CSSParser::consumeDeclaration(CSSParserTokenRange range, StyleRuleType ruleType)
{
    CSSParserTokenRange rangeCopy = range; // For inspector callbacks

    ASSERT(range.peek().type() == IdentToken);
    auto& token = range.consumeIncludingWhitespace();
    auto propertyID = token.parseAsCSSPropertyID();
    if (range.consume().type() != ColonToken)
        return false; // Parse error

    range.consumeWhitespace();

    auto important = consumeTrailingImportantAndWhitespace(range);
    if (important == IsImportant::Yes && ruleDoesNotAllowImportant(ruleType))
        return false;

    const size_t oldPropertiesCount = topContext().m_parsedProperties.size();
    auto didParseNewProperties = [&] {
        return topContext().m_parsedProperties.size() != oldPropertiesCount;
    };

    if (!isExposed(propertyID, &m_context.propertySettings))
        propertyID = CSSPropertyInvalid;

    // @position-try doesn't allow custom properties.
    // FIXME: maybe make this logic more elegant?
    if (propertyID == CSSPropertyInvalid && CSSVariableParser::isValidVariableName(token) && ruleType != StyleRuleType::PositionTry) {
        AtomString variableName = token.value().toAtomString();
        consumeCustomPropertyValue(range, variableName, important);
    }

    if (propertyID != CSSPropertyInvalid)
        consumeDeclarationValue(range, propertyID, important, ruleType);

    RefPtr observerWrapper = m_observerWrapper.get();
    if (observerWrapper&& (ruleType == StyleRuleType::Style || ruleType == StyleRuleType::Keyframe || ruleType == StyleRuleType::Page)) {
        observerWrapper->observer().observeProperty(
            observerWrapper->startOffset(rangeCopy), observerWrapper->endOffset(rangeCopy),
            important == IsImportant::Yes, didParseNewProperties());
    }

    return didParseNewProperties();
}

void CSSParser::consumeCustomPropertyValue(CSSParserTokenRange range, const AtomString& variableName, IsImportant important)
{
    if (range.atEnd())
        topContext().m_parsedProperties.append(CSSProperty(CSSPropertyCustom, CSSCustomPropertyValue::createEmpty(variableName), important));
    else if (auto value = CSSVariableParser::parseDeclarationValue(variableName, range, m_context))
        topContext().m_parsedProperties.append(CSSProperty(CSSPropertyCustom, value.releaseNonNull(), important));
}

void CSSParser::consumeDeclarationValue(CSSParserTokenRange range, CSSPropertyID propertyID, IsImportant important, StyleRuleType ruleType)
{
    CSSPropertyParser::parseValue(propertyID, important, range, m_context, topContext().m_parsedProperties, ruleType);
}

} // namespace WebCore
