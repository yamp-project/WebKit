/*
 * Copyright (C) 2003 Lars Knoll (knoll@kde.org)
 * Copyright (C) 2004, 2005, 2006, 2008, 2014 Apple Inc. All rights reserved.
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

#include "config.h"
#include "MutableCSSSelector.h"

#include "CSSSelector.h"
#include "CSSSelectorInlines.h"
#include "CSSSelectorList.h"
#include "SelectorPseudoTypeMap.h"
#include <wtf/TZoneMallocInlines.h>

namespace WebCore {

WTF_MAKE_TZONE_ALLOCATED_IMPL(MutableCSSSelector);

std::unique_ptr<MutableCSSSelector> MutableCSSSelector::parsePagePseudoSelector(StringView pseudoTypeString)
{
    CSSSelector::PagePseudoClass pseudoType;
    if (equalLettersIgnoringASCIICase(pseudoTypeString, "first"_s))
        pseudoType = CSSSelector::PagePseudoClass::First;
    else if (equalLettersIgnoringASCIICase(pseudoTypeString, "left"_s))
        pseudoType = CSSSelector::PagePseudoClass::Left;
    else if (equalLettersIgnoringASCIICase(pseudoTypeString, "right"_s))
        pseudoType = CSSSelector::PagePseudoClass::Right;
    else
        return nullptr;

    auto selector = makeUnique<MutableCSSSelector>();
    selector->m_selector->setMatch(CSSSelector::Match::PagePseudoClass);
    selector->m_selector->setPagePseudoClass(pseudoType);
    return selector;
}

std::unique_ptr<MutableCSSSelector> MutableCSSSelector::parsePseudoElementSelector(StringView pseudoTypeString, const CSSSelectorParserContext& context)
{
    auto pseudoType = CSSSelector::parsePseudoElementName(pseudoTypeString, context);
    if (!pseudoType)
        return nullptr;

    auto selector = makeUnique<MutableCSSSelector>();
    selector->m_selector->setMatch(CSSSelector::Match::PseudoElement);
    selector->m_selector->setPseudoElement(*pseudoType);
    AtomString name;
    if (*pseudoType == CSSSelector::PseudoElement::UserAgentPartLegacyAlias)
        name = CSSSelector::nameForUserAgentPartLegacyAlias(pseudoTypeString);
    else
        name = pseudoTypeString.convertToASCIILowercaseAtom();
    selector->m_selector->setValue(name);
    return selector;
}

std::unique_ptr<MutableCSSSelector> MutableCSSSelector::parsePseudoClassSelector(StringView pseudoTypeString, const CSSSelectorParserContext& context)
{
    auto pseudoType = findPseudoClassAndCompatibilityElementName(pseudoTypeString);
    if (pseudoType.pseudoClass) {
        if (!CSSSelector::isPseudoClassEnabled(*pseudoType.pseudoClass, context))
            return nullptr;
        auto selector = makeUnique<MutableCSSSelector>();
        selector->m_selector->setMatch(CSSSelector::Match::PseudoClass);
        selector->m_selector->setPseudoClass(*pseudoType.pseudoClass);
        return selector;
    }
    if (pseudoType.compatibilityPseudoElement) {
        ASSERT(CSSSelector::isPseudoElementEnabled(*pseudoType.compatibilityPseudoElement, pseudoTypeString, context));
        auto selector = makeUnique<MutableCSSSelector>();
        selector->m_selector->setMatch(CSSSelector::Match::PseudoElement);
        selector->m_selector->setPseudoElement(*pseudoType.compatibilityPseudoElement);
        selector->m_selector->setValue(pseudoTypeString.convertToASCIILowercaseAtom());
        return selector;
    }
    return nullptr;
}

MutableCSSSelector::MutableCSSSelector()
    : m_selector(makeUnique<CSSSelector>())
{
}

MutableCSSSelector::MutableCSSSelector(const QualifiedName& tagQName)
    : m_selector(makeUnique<CSSSelector>(tagQName))
{
}

MutableCSSSelector::MutableCSSSelector(const CSSSelector& selector)
    : m_selector(makeUnique<CSSSelector>(selector, CSSSelector::MutableSelectorCopy))
{
    if (auto preceding = selector.precedingInComplexSelector())
        m_precedingInComplexSelector = makeUnique<MutableCSSSelector>(*preceding);
}

MutableCSSSelector::MutableCSSSelector(const CSSSelector& selector, SimpleSelectorTag)
    : m_selector(makeUnique<CSSSelector>(selector, CSSSelector::MutableSelectorCopy))
{
}


MutableCSSSelector::~MutableCSSSelector()
{
    if (!m_precedingInComplexSelector)
        return;
    Vector<std::unique_ptr<MutableCSSSelector>, 16> toDelete;
    std::unique_ptr<MutableCSSSelector> selector = WTFMove(m_precedingInComplexSelector);
    while (true) {
        std::unique_ptr<MutableCSSSelector> next = WTFMove(selector->m_precedingInComplexSelector);
        toDelete.append(WTFMove(selector));
        if (!next)
            break;
        selector = WTFMove(next);
    }
}

void MutableCSSSelector::adoptSelectorVector(MutableCSSSelectorList&& selectorVector)
{
    m_selector->setSelectorList(makeUnique<CSSSelectorList>(WTFMove(selectorVector)));
}

void MutableCSSSelector::setArgumentList(FixedVector<AtomString> list)
{
    ASSERT(!list.isEmpty());
    m_selector->setArgumentList(WTFMove(list));
}

void MutableCSSSelector::setLangList(FixedVector<PossiblyQuotedIdentifier> list)
{
    ASSERT(!list.isEmpty());
    m_selector->setLangList(WTFMove(list));
}

void MutableCSSSelector::setSelectorList(std::unique_ptr<CSSSelectorList> selectorList)
{
    m_selector->setSelectorList(WTFMove(selectorList));
}

const MutableCSSSelector* MutableCSSSelector::leftmostSimpleSelector() const
{
    auto selector = this;
    while (auto next = selector->precedingInComplexSelector())
        selector = next;
    return selector;
}

MutableCSSSelector* MutableCSSSelector::leftmostSimpleSelector()
{
    auto selector = this;
    while (auto next = selector->precedingInComplexSelector())
        selector = next;
    return selector;
}

bool MutableCSSSelector::hasExplicitNestingParent() const
{
    auto selector = this;
    while (selector) {
        if (selector->selector()->hasExplicitNestingParent())
            return true;

        selector = selector->precedingInComplexSelector();
    }
    return false;
}

bool MutableCSSSelector::hasExplicitPseudoClassScope() const
{
    auto selector = this;
    while (selector) {
        if (selector->selector()->hasExplicitPseudoClassScope())
            return true;

        selector = selector->precedingInComplexSelector();
    }
    return false;
}

static bool selectorListMatchesPseudoElement(const CSSSelectorList* selectorList)
{
    if (!selectorList)
        return false;

    for (auto& subSelector : *selectorList) {
        for (const CSSSelector* selector = &subSelector; selector; selector = selector->precedingInComplexSelector()) {
            if (selector->matchesPseudoElement())
                return true;
            if (const CSSSelectorList* subselectorList = selector->selectorList()) {
                if (selectorListMatchesPseudoElement(subselectorList))
                    return true;
            }
        }
    }
    return false;
}

bool MutableCSSSelector::matchesPseudoElement() const
{
    return m_selector->matchesPseudoElement() || selectorListMatchesPseudoElement(m_selector->selectorList());
}

void MutableCSSSelector::prependInComplexSelector(CSSSelector::Relation relation, std::unique_ptr<MutableCSSSelector> selector)
{
    auto* first = this;
    while (first->precedingInComplexSelector())
        first = first->precedingInComplexSelector();

    first->setRelation(relation);
    first->setPrecedingInComplexSelector(WTFMove(selector));
}

void MutableCSSSelector::prependInComplexSelectorAsRelative(std::unique_ptr<MutableCSSSelector> selector)
{
    auto firstSelector = leftmostSimpleSelector()->selector();
    ASSERT(firstSelector);

    // Relation is Descendant by default.
    auto relation = firstSelector->relation();
    if (relation == CSSSelector::Relation::Subselector)
        relation = CSSSelector::Relation::DescendantSpace;

    prependInComplexSelector(relation, WTFMove(selector));
}

void MutableCSSSelector::appendTagInComplexSelector(const QualifiedName& tagQName, bool tagIsForNamespaceRule)
{
    // Make the current last selector the second to last.
    auto currentLast = makeUnique<MutableCSSSelector>();
    currentLast->m_selector = WTFMove(m_selector);
    currentLast->m_precedingInComplexSelector = WTFMove(m_precedingInComplexSelector);
    m_precedingInComplexSelector = WTFMove(currentLast);

    // Change the last selector to be the tag selector.
    m_selector = makeUnique<CSSSelector>(tagQName, tagIsForNamespaceRule);
    m_selector->setRelation(CSSSelector::Relation::Subselector);
}

std::unique_ptr<MutableCSSSelector> MutableCSSSelector::releaseFromComplexSelector()
{
    setRelation(CSSSelector::Relation::Subselector);
    return WTFMove(m_precedingInComplexSelector);
}

bool MutableCSSSelector::startsWithExplicitCombinator() const
{
    auto relation = leftmostSimpleSelector()->selector()->relation();
    return relation != CSSSelector::Relation::Subselector && relation != CSSSelector::Relation::DescendantSpace;
}

}
