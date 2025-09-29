/*
 * Copyright (C) 2022 Apple Inc. All rights reserved.
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
#include "WebExtensionController.h"

#if ENABLE(WK_WEB_EXTENSIONS)

#include "WebExtensionControllerParameters.h"
#include "WebExtensionControllerProxyMessages.h"
#include "WebPageProxy.h"
#include <wtf/HashMap.h>
#include <wtf/NeverDestroyed.h>

#if PLATFORM(COCOA)
#include <wtf/BlockPtr.h>
#include <wtf/darwin/DispatchExtras.h>
#endif

namespace WebKit {

#if PLATFORM(COCOA)
constexpr auto freshlyCreatedTimeout = 5_s;
#endif

static HashMap<WebExtensionControllerIdentifier, WeakPtr<WebExtensionController>>& webExtensionControllers()
{
    static MainRunLoopNeverDestroyed<HashMap<WebExtensionControllerIdentifier, WeakPtr<WebExtensionController>>> controllers;
    return controllers;
}

RefPtr<WebExtensionController> WebExtensionController::get(WebExtensionControllerIdentifier identifier)
{
    return webExtensionControllers().get(identifier).get();
}

WebExtensionController::WebExtensionController(Ref<WebExtensionControllerConfiguration> configuration)
    : m_configuration(configuration)
{
    ASSERT(!get(identifier()));
    webExtensionControllers().add(identifier(), *this);

    initializePlatform();

    // A freshly created extension controller will be used to determine if the startup event
    // should be fired for any loaded extensions during a brief time window. Start a timer
    // when the first extension is about to be loaded.

#if PLATFORM(COCOA)
    dispatch_after(dispatch_time(DISPATCH_TIME_NOW, (int64_t)(freshlyCreatedTimeout.seconds() * NSEC_PER_SEC)), mainDispatchQueueSingleton(), makeBlockPtr([this, weakThis = WeakPtr { *this }] {
        if (!weakThis)
            return;

        m_freshlyCreated = false;
    }).get());
#endif
}

WebExtensionController::~WebExtensionController()
{
    webExtensionControllers().remove(identifier());
    unloadAll();
}

WebExtensionControllerParameters WebExtensionController::parameters(const API::PageConfiguration& pageConfiguration) const
{
    return {
        .identifier = identifier(),
        .testingMode = inTestingMode(),
        .contextParameters = WTF::map(extensionContexts(), [&](auto& context) {
            bool isForThisExtension = context->isURLForThisExtension(pageConfiguration.requiredWebExtensionBaseURL());
            auto includePrivilegedIdentifier = isForThisExtension ? WebExtensionContext::IncludePrivilegedIdentifier::Yes : WebExtensionContext::IncludePrivilegedIdentifier::No;
            return context->parameters(includePrivilegedIdentifier);
        })
    };
}

WebExtensionController::WebProcessProxySet WebExtensionController::allProcesses() const
{
    WebProcessProxySet result;

    for (Ref page : m_pages) {
        page->forEachWebContentProcess([&](auto& webProcess, auto pageID) {
            result.addVoid(webProcess);
        });
    }

    return result;
}

} // namespace WebKit

#endif // ENABLE(WK_WEB_EXTENSIONS)
