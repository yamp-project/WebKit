/*
 * Copyright (C) 2014-2025 Apple Inc. All rights reserved.
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

#include "config.h"
#include "ContentExtensionsBackend.h"

#if ENABLE(CONTENT_EXTENSIONS)

#include "Chrome.h"
#include "ChromeClient.h"
#include "CompiledContentExtension.h"
#include "ContentExtension.h"
#include "ContentExtensionsDebugging.h"
#include "ContentRuleListMatchedRule.h"
#include "ContentRuleListResults.h"
#include "DFABytecodeInterpreter.h"
#include "Document.h"
#include "DocumentInlines.h"
#include "DocumentLoader.h"
#include "ExtensionStyleSheets.h"
#include "LocalFrame.h"
#include "LocalFrameLoaderClient.h"
#include "Page.h"
#include "RegistrableDomain.h"
#include "ResourceLoadInfo.h"
#include "ScriptController.h"
#include "ScriptSourceCode.h"
#include "Settings.h"
#include <wtf/URL.h>
#include "UserContentController.h"
#include <ranges>
#include <wtf/NeverDestroyed.h>
#include <wtf/TZoneMallocInlines.h>
#include <wtf/text/CString.h>
#include <wtf/text/MakeString.h>

namespace WebCore::ContentExtensions {

WTF_MAKE_TZONE_ALLOCATED_IMPL(ContentExtensionsBackend);

#if USE(APPLE_INTERNAL_SDK)
#import <WebKitAdditions/ContentRuleListAdditions.mm>
#else
static void makeSecureIfNecessary(ContentRuleListResults& results, const URL& url, const URL& redirectFrom = { })
{
    if (redirectFrom.host() == url.host() && redirectFrom.protocolIs("https"_s))
        return;

    if (!url.protocolIs("http"_s))
        return;
    if (url.host() == "www.opengl.org"_s
        || url.host() == "webkit.org"_s
        || url.host() == "download"_s)
        results.summary.madeHTTPS = true;
}
#endif

bool ContentExtensionsBackend::shouldBeMadeSecure(const URL& url)
{
    ContentRuleListResults results;
    makeSecureIfNecessary(results, url);
    return results.summary.madeHTTPS;
}

void ContentExtensionsBackend::addContentExtension(const String& identifier, Ref<CompiledContentExtension> compiledContentExtension, URL&& extensionBaseURL, ContentExtension::ShouldCompileCSS shouldCompileCSS)
{
    ASSERT(!identifier.isEmpty());
    if (identifier.isEmpty())
        return;
    
    auto contentExtension = ContentExtension::create(identifier, WTFMove(compiledContentExtension), WTFMove(extensionBaseURL), shouldCompileCSS);
    m_contentExtensions.set(identifier, WTFMove(contentExtension));
}

void ContentExtensionsBackend::removeContentExtension(const String& identifier)
{
    m_contentExtensions.remove(identifier);
}

void ContentExtensionsBackend::removeAllContentExtensions()
{
    m_contentExtensions.clear();
}

auto ContentExtensionsBackend::actionsFromContentRuleList(const ContentExtension& contentExtension, const String& urlString, const ResourceLoadInfo& resourceLoadInfo, ResourceFlags flags) const -> ActionsFromContentRuleList
{
    ActionsFromContentRuleList actionsStruct;
    actionsStruct.contentRuleListIdentifier = contentExtension.identifier();

    const auto& compiledExtension = contentExtension.compiledExtension();

    DFABytecodeInterpreter interpreter(compiledExtension.urlFiltersBytecode());
    auto actionLocations = interpreter.interpret(urlString, flags);
    auto& topURLActions = contentExtension.topURLActions(resourceLoadInfo.mainDocumentURL);
    auto& frameURLActions = contentExtension.frameURLActions(resourceLoadInfo.frameURL);

    actionLocations.removeIf([&](uint64_t actionAndFlags) {
        ResourceFlags flags = actionAndFlags >> 32;
        auto actionCondition = static_cast<ActionCondition>(flags & ActionConditionMask);
        switch (actionCondition) {
        case ActionCondition::None:
            return false;
        case ActionCondition::IfTopURL:
            return !topURLActions.contains(actionAndFlags);
        case ActionCondition::UnlessTopURL:
            return topURLActions.contains(actionAndFlags);
        case ActionCondition::IfFrameURL:
            return !frameURLActions.contains(actionAndFlags);
        case ActionCondition::UnlessFrameURL:
            return frameURLActions.contains(actionAndFlags);
        }
        ASSERT_NOT_REACHED();
        return false;
    });

    auto serializedActions = compiledExtension.serializedActions();

    const auto& universalActions = contentExtension.universalActions();
    if (auto totalActionCount = actionLocations.size() + universalActions.size()) {
        Vector<uint32_t> vector;
        vector.reserveInitialCapacity(totalActionCount);
        vector.appendContainerWithMapping(actionLocations, [](uint64_t actionLocation) {
            return static_cast<uint32_t>(actionLocation);
        });
        vector.appendContainerWithMapping(universalActions, [](uint64_t actionLocation) {
            return static_cast<uint32_t>(actionLocation);
        });
        std::ranges::sort(vector);

        // We need to handle IgnoreFollowingRules...
        for (size_t i = 0; i < vector.size(); i++) {
            auto action = DeserializedAction::deserialize(serializedActions, vector[i]);
            if (std::holds_alternative<IgnoreFollowingRulesAction>(action.data()))
                break;
            actionsStruct.actions.append(WTFMove(action));
        }

        // ...and iterate in reverse order to properly deal with IgnorePreviousRules.
        for (auto i = actionsStruct.actions.size(); i; i--) {
            auto action = actionsStruct.actions[i - 1];
            if (std::holds_alternative<IgnorePreviousRulesAction>(action.data())) {
                actionsStruct.sawIgnorePreviousRules = true;
                actionsStruct.actions.removeAt(0, i);
                break;
            }
        }
    }
    return actionsStruct;
}

auto ContentExtensionsBackend::actionsForResourceLoad(const ResourceLoadInfo& resourceLoadInfo, const RuleListFilter& ruleListFilter) const -> Vector<ActionsFromContentRuleList>
{
#if CONTENT_EXTENSIONS_PERFORMANCE_REPORTING
    MonotonicTime addedTimeStart = MonotonicTime::now();
#endif
    if (m_contentExtensions.isEmpty()
        || !resourceLoadInfo.resourceURL.isValid()
        || resourceLoadInfo.resourceURL.protocolIsData())
        return { };

    const String& urlString = resourceLoadInfo.resourceURL.string();
    ASSERT_WITH_MESSAGE(urlString.containsOnlyASCII(), "A decoded URL should only contain ASCII characters. The matching algorithm assumes the input is ASCII.");

    ASSERT(!(resourceLoadInfo.getResourceFlags() & ActionConditionMask));
    const ResourceFlags flags = resourceLoadInfo.getResourceFlags() | ActionConditionMask;
    Vector<ActionsFromContentRuleList> actionsVector = WTF::compactMap(m_contentExtensions, [&](auto& entry) -> std::optional<ActionsFromContentRuleList> {
        auto& [identifier, contentExtension] = entry;
        if (ruleListFilter(identifier) == ShouldSkipRuleList::Yes)
            return std::nullopt;
        return actionsFromContentRuleList(contentExtension.get(), urlString, resourceLoadInfo, flags);
    });

#if CONTENT_EXTENSIONS_PERFORMANCE_REPORTING
    MonotonicTime addedTimeEnd = MonotonicTime::now();
    dataLogF("Time added: %f microseconds %s \n", (addedTimeEnd - addedTimeStart).microseconds(), resourceLoadInfo.resourceURL.string().utf8().data());
#endif
    return actionsVector;
}

void ContentExtensionsBackend::forEach(NOESCAPE const Function<void(const String&, ContentExtension&)>& apply) const
{
    for (auto& pair : m_contentExtensions)
        apply(pair.key, pair.value);
}

StyleSheetContents* ContentExtensionsBackend::globalDisplayNoneStyleSheet(const String& identifier) const
{
    const auto& contentExtension = m_contentExtensions.get(identifier);
    return contentExtension ? contentExtension->globalDisplayNoneStyleSheet() : nullptr;
}

std::optional<String> customTrackerBlockingMessageForConsole(const ContentRuleListResults& results, const URL& requestURL, const URL& documentURL)
{
#if ENABLE(ADVANCED_PRIVACY_PROTECTIONS)
    bool blockedKnownTracker = results.results.containsIf([](auto& identifierAndResult) {
        auto& [identifier, result] = identifierAndResult;
        if (!result.blockedLoad)
            return false;
        return identifier.startsWith("com.apple."_s) && identifier.endsWith(".TrackingResourceRequestContentBlocker"_s);
    });

    if (!blockedKnownTracker)
        return std::nullopt;

    auto trackerBlockingMessage = "Blocked connection to known tracker"_s;
    if (!requestURL.isEmpty() && !documentURL.isEmpty())
        return makeString(trackerBlockingMessage, ' ', requestURL.string(), " in frame displaying "_s, documentURL.string());

    if (!requestURL.isEmpty())
        return makeString(trackerBlockingMessage, ' ', requestURL.string());

    return trackerBlockingMessage;
#else
    UNUSED_PARAM(results);
    UNUSED_PARAM(requestURL);
    UNUSED_PARAM(documentURL);
    return std::nullopt;
#endif
}

ContentRuleListResults ContentExtensionsBackend::processContentRuleListsForLoad(Page& page, const URL& url, OptionSet<ResourceType> resourceType, DocumentLoader& initiatingDocumentLoader, const URL& redirectFrom, const RuleListFilter& ruleListFilter) const
{
    Document* currentDocument = nullptr;
    URL mainDocumentURL;
    URL frameURL;
    bool mainFrameContext = false;
    RequestMethod requestMethod = readRequestMethod(initiatingDocumentLoader.request().httpMethod()).value_or(RequestMethod::None);
    auto requestId = WTF::UUID::createVersion4Weak().toString();
    double frameId;
    double parentFrameId;

    if (auto* frame = initiatingDocumentLoader.frame()) {
        mainFrameContext = frame->isMainFrame();
        currentDocument = frame->document();
        frameId = mainFrameContext ? 0 : static_cast<double>(frame->frameID().toUInt64());
        parentFrameId = !mainFrameContext && frame->tree().parent() ? static_cast<double>(frame->tree().parent()->frameID().toUInt64()) : -1;

        if (initiatingDocumentLoader.isLoadingMainResource()
            && frame->isMainFrame()
            && resourceType.containsAny({ ResourceType::TopDocument, ResourceType::ChildDocument }))
            mainDocumentURL = url;
        else if (auto* page = frame->page())
            mainDocumentURL = page->mainFrameURL();
    }

    if (currentDocument && currentDocument->url().isValid())
        frameURL = currentDocument->url();
    else
        frameURL = url;

    ResourceLoadInfo resourceLoadInfo { url, mainDocumentURL, frameURL, resourceType, mainFrameContext, requestMethod };
    auto actions = actionsForResourceLoad(resourceLoadInfo, ruleListFilter);

    ContentRuleListResults results;
    if (page.httpsUpgradeEnabled())
        makeSecureIfNecessary(results, url, redirectFrom);
    results.results.reserveInitialCapacity(actions.size());
    for (const auto& actionsFromContentRuleList : actions) {
        const String& contentRuleListIdentifier = actionsFromContentRuleList.contentRuleListIdentifier;
        ContentRuleListResults::Result result;
        for (const auto& action : actionsFromContentRuleList.actions) {
            WTF::visit(WTF::makeVisitor([&](const BlockLoadAction&) {
                if (results.summary.redirected)
                    return;

                results.summary.blockedLoad = true;
                result.blockedLoad = true;
            }, [&](const BlockCookiesAction&) {
                results.summary.blockedCookies = true;
                result.blockedCookies = true;
            }, [&](const CSSDisplayNoneSelectorAction& actionData) {
                if (resourceType.containsAny({ ResourceType::TopDocument, ResourceType::ChildDocument }))
                    initiatingDocumentLoader.addPendingContentExtensionDisplayNoneSelector(contentRuleListIdentifier, actionData.string, action.actionID());
                else if (currentDocument)
                    currentDocument->extensionStyleSheets().addDisplayNoneSelector(contentRuleListIdentifier, actionData.string, action.actionID());
            }, [&](const NotifyAction& actionData) {
                results.summary.hasNotifications = true;
                result.notifications.append(actionData.string);
            }, [&](const MakeHTTPSAction&) {
                if ((url.protocolIs("http"_s) || url.protocolIs("ws"_s))
                    && (!url.port() || WTF::isDefaultPortForProtocol(url.port().value(), url.protocol()))) {
                    results.summary.madeHTTPS = true;
                    result.madeHTTPS = true;
                }
            }, [&](const IgnorePreviousRulesAction&) {
                RELEASE_ASSERT_NOT_REACHED();
            }, [&](const IgnoreFollowingRulesAction&) {
                RELEASE_ASSERT_NOT_REACHED();
            }, [&] (const ModifyHeadersAction& action) {
                if (initiatingDocumentLoader.allowsActiveContentRuleListActionsForURL(contentRuleListIdentifier, url)) {
                    result.modifiedHeaders = true;
                    results.summary.modifyHeadersActions.append(action);
                }
            }, [&] (const RedirectAction& redirectAction) {
                if (initiatingDocumentLoader.allowsActiveContentRuleListActionsForURL(contentRuleListIdentifier, url)) {
                    if (results.summary.blockedLoad)
                        return;

                    result.redirected = true;
                    results.summary.redirected = true;
                    results.summary.redirectActions.append({ redirectAction, m_contentExtensions.get(contentRuleListIdentifier)->extensionBaseURL() });
                }
            }, [&] (const ReportIdentifierAction& reportIdentifierAction) {
                std::optional<String> initiator;
                std::optional<String> documentId;
                std::optional<String> frameType;

                // FIXME: <rdar://159289161> Include the parentDocumentId parameter once we can make it work with site isolation
                if (currentDocument && resourceType.containsAny({ ResourceType::TopDocument, ResourceType::ChildDocument }))
                    documentId = currentDocument->identifier().toString();

                if (resourceType == ResourceType::TopDocument)
                    frameType = "outermost_frame"_s;
                else if (resourceType == ResourceType::ChildDocument)
                    frameType = "sub_frame"_s;

                if (currentDocument && currentDocument->url().isValid()) {
                    auto domain = RegistrableDomain { frameURL };

                    if (!domain.isEmpty())
                        initiator = domain.string();
                }

                // We set the tabId to -1 because it will be filled in by the web extension context.
                // We create a requestId here since ResourceRequest objects don't have one, and it's a non-optional parameter.
                // We set documentLifecycle to null because that will require Safari API to be implemented.
                page.chrome().client().contentRuleListMatchedRule({ { reportIdentifierAction.identifier, reportIdentifierAction.string, contentRuleListIdentifier }, { frameId, parentFrameId, initiatingDocumentLoader.request().httpMethod(), requestId, -1, resourceTypeToStringForMatchedRule(resourceType), url.string(), initiator, documentId, std::nullopt, frameType, std::nullopt } });
            }), action.data());
        }

        if (!actionsFromContentRuleList.sawIgnorePreviousRules) {
            if (auto* styleSheetContents = globalDisplayNoneStyleSheet(contentRuleListIdentifier)) {
                if (resourceType.containsAny({ ResourceType::TopDocument, ResourceType::ChildDocument }))
                    initiatingDocumentLoader.addPendingContentExtensionSheet(contentRuleListIdentifier, *styleSheetContents);
                else if (currentDocument)
                    currentDocument->extensionStyleSheets().maybeAddContentExtensionSheet(contentRuleListIdentifier, *styleSheetContents);
            }
        }

        results.results.append({ contentRuleListIdentifier, WTFMove(result) });
    }

    if (currentDocument) {
        if (results.summary.madeHTTPS) {
            ASSERT(url.protocolIs("http"_s) || url.protocolIs("ws"_s));
            String newProtocol = url.protocolIs("http"_s) ? "https"_s : "wss"_s;
            currentDocument->addConsoleMessage(MessageSource::ContentBlocker, MessageLevel::Info, makeString("Promoted URL from "_s, url.string(), " to "_s, newProtocol));
        }

        if (results.shouldBlock()) {
            String consoleMessage;
            if (auto message = customTrackerBlockingMessageForConsole(results, url, mainDocumentURL))
                consoleMessage = WTFMove(*message);
            else
                consoleMessage = makeString("Content blocker prevented frame displaying "_s, mainDocumentURL.string(), " from loading a resource from "_s, url.string());
            currentDocument->addConsoleMessage(MessageSource::ContentBlocker, MessageLevel::Info, WTFMove(consoleMessage));
        
            // Quirk for content-blocker interference with Google's anti-flicker optimization (rdar://problem/45968770).
            // https://developers.google.com/optimize/
            if (currentDocument->settings().googleAntiFlickerOptimizationQuirkEnabled()
                && ((equalLettersIgnoringASCIICase(url.host(), "www.google-analytics.com"_s) && equalLettersIgnoringASCIICase(url.path(), "/analytics.js"_s))
                    || (equalLettersIgnoringASCIICase(url.host(), "www.googletagmanager.com"_s) && equalLettersIgnoringASCIICase(url.path(), "/gtm.js"_s)))) {
                if (auto* frame = currentDocument->frame())
                    frame->script().evaluateIgnoringException(ScriptSourceCode { "try { window.dataLayer.hide.end(); console.log('Called window.dataLayer.hide.end() in frame ' + document.URL + ' because the content blocker blocked the load of the https://www.google-analytics.com/analytics.js script'); } catch (e) { }"_s, JSC::SourceTaintedOrigin::Untainted });
            }
        }
    }

    return results;
}

ContentRuleListResults ContentExtensionsBackend::processContentRuleListsForPingLoad(const URL& url, const URL& mainDocumentURL, const URL& frameURL, const String& httpMethod)
{
    RequestMethod requestMethod = readRequestMethod(httpMethod).value_or(RequestMethod::None);
    ResourceLoadInfo resourceLoadInfo { url, mainDocumentURL, frameURL, ResourceType::Ping, false, requestMethod };
    auto actions = actionsForResourceLoad(resourceLoadInfo);

    ContentRuleListResults results;
    makeSecureIfNecessary(results, url);
    for (const auto& actionsFromContentRuleList : actions) {
        for (const auto& action : actionsFromContentRuleList.actions) {
            WTF::visit(WTF::makeVisitor([&](const BlockLoadAction&) {
                results.summary.blockedLoad = true;
            }, [&](const BlockCookiesAction&) {
                results.summary.blockedCookies = true;
            }, [&](const CSSDisplayNoneSelectorAction&) {
            }, [&](const NotifyAction&) {
                // We currently have not implemented notifications from the NetworkProcess to the UIProcess.
            }, [&](const MakeHTTPSAction&) {
                if ((url.protocolIs("http"_s) || url.protocolIs("ws"_s)) && (!url.port() || WTF::isDefaultPortForProtocol(url.port().value(), url.protocol())))
                    results.summary.madeHTTPS = true;
            }, [&](const IgnorePreviousRulesAction&) {
                RELEASE_ASSERT_NOT_REACHED();
            }, [&](const IgnoreFollowingRulesAction&) {
                RELEASE_ASSERT_NOT_REACHED();
            }, [&] (const ModifyHeadersAction&) {
                // We currently have not implemented active actions from the network process (CORS preflight).
            }, [&] (const RedirectAction&) {
                // We currently have not implemented active actions from the network process (CORS preflight).
            }, [&] (const ReportIdentifierAction&) {
                // We currently have not implemented notifications from the NetworkProcess to the UIProcess.
            }), action.data());
        }
    }

    return results;
}

bool ContentExtensionsBackend::processContentRuleListsForResourceMonitoring(const URL& url, const URL& mainDocumentURL, const URL& frameURL, OptionSet<ResourceType> resourceType)
{
    ResourceLoadInfo resourceLoadInfo { url, mainDocumentURL, frameURL, resourceType };
    auto actions = actionsForResourceLoad(resourceLoadInfo);

    bool matched = false;
    for (const auto& actionsFromContentRuleList : actions) {
        for (const auto& action : actionsFromContentRuleList.actions) {
            WTF::visit(WTF::makeVisitor([&](const BlockLoadAction&) {
                matched = true;
            }, [&](const BlockCookiesAction&) {
            }, [&](const CSSDisplayNoneSelectorAction&) {
            }, [&](const NotifyAction&) {
            }, [&](const MakeHTTPSAction&) {
            }, [&](const IgnorePreviousRulesAction&) {
                RELEASE_ASSERT_NOT_REACHED();
            }, [&](const IgnoreFollowingRulesAction&) {
                RELEASE_ASSERT_NOT_REACHED();
            }, [&] (const ModifyHeadersAction&) {
            }, [&] (const RedirectAction&) {
            }, [&] (const ReportIdentifierAction&) {
            }), action.data());
        }
    }

    return matched;
}

const String& ContentExtensionsBackend::displayNoneCSSRule()
{
    static NeverDestroyed<const String> rule(MAKE_STATIC_STRING_IMPL("display:none !important;"));
    return rule;
}

void applyResultsToRequestIfCrossOriginRedirect(ContentRuleListResults&& results, Page* page, ResourceRequest& request)
{
    if (!results.summary.redirected)
        return;

    URL url = request.url();
    for (auto& pair : results.summary.redirectActions)
        pair.first.modifyURL(url, pair.second);

    if (RegistrableDomain { request.url() } == RegistrableDomain { url })
        return;

    applyResultsToRequest(WTFMove(results), page, request, url);
}

void applyResultsToRequest(ContentRuleListResults&& results, Page* page, ResourceRequest& request, const URL& redirectURL)
{
    if (results.summary.blockedCookies)
        request.setAllowCookies(false);

    if (results.summary.madeHTTPS) {
        ASSERT(!request.url().port() || WTF::isDefaultPortForProtocol(request.url().port().value(), request.url().protocol()));
        request.upgradeInsecureRequest();
    }

    std::ranges::sort(results.summary.modifyHeadersActions, std::ranges::greater { }, &ModifyHeadersAction::priority);

    HashMap<String, ModifyHeadersAction::ModifyHeadersOperationType> headerNameToFirstOperationApplied;
    for (auto& action : results.summary.modifyHeadersActions)
        action.applyToRequest(request, headerNameToFirstOperationApplied);

    if (redirectURL.isEmpty()) {
        for (auto& pair : results.summary.redirectActions)
            pair.first.applyToRequest(request, pair.second);
    } else
        request.setURL(URL { redirectURL });

    if (page && results.shouldNotifyApplication()) {
        results.results.removeAllMatching([](const auto& pair) {
            return !pair.second.shouldNotifyApplication();
        });
        page->chrome().client().contentRuleListNotification(request.url(), results);
    }
}
    
} // namespace WebCore::ContentExtensions

#endif // ENABLE(CONTENT_EXTENSIONS)
