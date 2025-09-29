/*
 * Copyright (C) 2012 Google Inc. All rights reserved.
 * Copyright (C) 2013-2025 Apple Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1.  Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 * 2.  Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 * 3.  Neither the name of Apple Inc. ("Apple") nor the names of
 *     its contributors may be used to endorse or promote products derived
 *     from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE AND ITS CONTRIBUTORS "AS IS" AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL APPLE OR ITS CONTRIBUTORS BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "MixedContentChecker.h"

#include "DocumentInlines.h"
#include "Document.h"
#include "LegacySchemeRegistry.h"
#include "LocalFrame.h"
#include "SecurityOrigin.h"
#include <JavaScriptCore/ConsoleTypes.h>
#include <wtf/text/MakeString.h>

namespace WebCore {

static bool isDocumentSecure(const Document& document)
{
    // FIXME: Use document.isDocumentSecure(), instead of comparing against "https" scheme, when all ports stop using loopback in LayoutTests
    // sandboxed iframes have an opaque origin so we should perform the mixed content check considering the origin
    // the iframe would have had if it were not sandboxed.
    return document.securityOrigin().protocol() == "https"_s || (document.securityOrigin().isOpaque() && document.url().protocolIs("https"_s));
}

static bool isDataContextSecure(const LocalFrame& frame)
{
    RefPtr document = frame.document();

    while (document) {
        if (isDocumentSecure(*document))
            return true;

        RefPtr frame = document->frame();
        if (!frame || frame->isMainFrame())
            break;

        RefPtr parentFrame = frame->tree().parent();
        if (!parentFrame)
            break;

        if (RefPtr localParentFrame = dynamicDowncast<LocalFrame>(parentFrame.get()))
            document = localParentFrame->document();
        else {
            // FIXME: <rdar://116259764> Make mixed content checks work correctly with site isolated iframes.
            break;
        }
    }

    return false;
}

static bool isMixedContent(const Document& document, const URL& url)
{
    if (isDocumentSecure(document) || (document.url().protocolIs("data"_s) && isDataContextSecure(*document.frame())))
        return !SecurityOrigin::isSecure(url);

    return false;
}

static void logConsoleWarning(const LocalFrame& frame, bool blocked, const URL& target, bool isUpgradingIPAddressAndLocalhostEnabled)
{
    auto isUpgradingLocalhostDisabled = !isUpgradingIPAddressAndLocalhostEnabled && shouldTreatAsPotentiallyTrustworthy(target);
    ASCIILiteral errorString = [&] {
    if (blocked)
        return "blocked and must"_s;
    if (isUpgradingLocalhostDisabled)
        return "not upgraded to HTTPS and must be served from the local host."_s;
    return "automatically upgraded and should"_s;
    }();

    auto message = makeString((!blocked ? ""_s : "[blocked] "_s), "The page at "_s, frame.document()->url().stringCenterEllipsizedToLength(), " requested insecure content from "_s, target.stringCenterEllipsizedToLength(), ". This content was "_s, errorString, !isUpgradingLocalhostDisabled ? " be served over HTTPS.\n"_s : "\n"_s);
    frame.protectedDocument()->addConsoleMessage(MessageSource::Security, MessageLevel::Warning, message);
}

static bool destinationIsImageAudioOrVideo(FetchOptions::Destination destination)
{
    return destination == FetchOptions::Destination::Audio || destination == FetchOptions::Destination::Image || destination == FetchOptions::Destination::Video;
}

static bool destinationIsImageAndInitiatorIsImageset(FetchOptions::Destination destination, Initiator initiator)
{
    return destination == FetchOptions::Destination::Image && initiator == Initiator::Imageset;
}

bool MixedContentChecker::shouldUpgradeInsecureContent(LocalFrame& frame, IsUpgradable isUpgradable, const URL& url, FetchOptions::Destination destination, Initiator initiator)
{
    RefPtr document = frame.document();
    if (!document || isUpgradable != IsUpgradable::Yes)
        return false;

    // https://www.w3.org/TR/mixed-content/#upgrade-algorithm
    // Editor’s Draft, 23 February 2023
    // 4.1. Upgrade a mixed content request to a potentially trustworthy URL, if appropriate
    if (!isMixedContent(*frame.document(), url))
        return false;

    auto shouldUpgradeIPAddressAndLocalhostForTesting = document->settings().iPAddressAndLocalhostMixedContentUpgradeTestingEnabled();

    // 4.1 The request's URL is not upgraded in the following cases.
    if (!canModifyRequest(url, destination, initiator))
        return false;

    logConsoleWarning(frame, /* blocked */ false, url, shouldUpgradeIPAddressAndLocalhostForTesting);
    return true;
}

bool MixedContentChecker::canModifyRequest(const URL& url, FetchOptions::Destination destination, Initiator initiator)
{
    // 4.1.1 request’s URL is a potentially trustworthy URL.
    if (url.protocolIs("https"_s))
        return false;
    // 4.1.2 request’s URL’s host is an IP address.
    if (URL::hostIsIPAddress(url.host()) && !shouldTreatAsPotentiallyTrustworthy(url))
        return false;
    // 4.1.4 request’s destination is not "image", "audio", or "video".
    if (!destinationIsImageAudioOrVideo(destination))
        return false;
    // 4.1.5 request’s destination is "image" and request’s initiator is "imageset".
    auto schemeIsHandledBySchemeHandler = LegacySchemeRegistry::schemeIsHandledBySchemeHandler(url.protocol());
    if (!schemeIsHandledBySchemeHandler && destinationIsImageAndInitiatorIsImageset(destination, initiator))
        return false;
    return true;
}

bool MixedContentChecker::shouldBlockRequest(LocalFrame& frame, const URL& url, IsUpgradable isUpgradable)
{
    RefPtr document = frame.document();
    if (!document)
        return false;
    if (!isMixedContent(*frame.document(), url))
        return false;
    if ((LegacySchemeRegistry::schemeIsHandledBySchemeHandler(url.protocol()) || shouldTreatAsPotentiallyTrustworthy(url)) && isUpgradable == IsUpgradable::Yes)
        return false;
    logConsoleWarning(frame, /* blocked */ true, url, document->settings().iPAddressAndLocalhostMixedContentUpgradeTestingEnabled());
    return true;
}

} // namespace WebCore
