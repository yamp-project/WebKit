/*
 * Copyright (C) 2014-2023 Apple Inc. All rights reserved.
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
#import "WebProcessProxy.h"
#include <WebCore/ServiceWorkerTypes.h>

#import "AccessibilitySupportSPI.h"
#import "CodeSigning.h"
#import "CoreIPCAuditToken.h"
#import "DefaultWebBrowserChecks.h"
#import "Logging.h"
#import "SandboxUtilities.h"
#import "SharedBufferReference.h"
#import "WKAPICast.h"
#import "WKBrowsingContextHandleInternal.h"
#import "WebProcessMessages.h"
#import "WebProcessPool.h"
#import <WebCore/ActivityState.h>
#import <pal/spi/ios/MobileGestaltSPI.h>
#import <sys/sysctl.h>
#import <wtf/NeverDestroyed.h>
#import <wtf/RuntimeApplicationChecks.h>
#import <wtf/Scope.h>
#import <wtf/cocoa/Entitlements.h>
#import <wtf/cocoa/TypeCastsCocoa.h>
#import <wtf/cocoa/VectorCocoa.h>
#import <wtf/spi/darwin/SandboxSPI.h>

#if PLATFORM(IOS_FAMILY)
#import "AccessibilitySupportSPI.h"
#endif

#if ENABLE(REMOTE_INSPECTOR)
#import "WebInspectorUtilities.h"
#import <JavaScriptCore/RemoteInspector.h>
#import <JavaScriptCore/RemoteInspectorConstants.h>
#endif

#if HAVE(MEDIA_ACCESSIBILITY_FRAMEWORK)
#import <WebCore/CaptionUserPreferencesMediaAF.h>
#endif

#if PLATFORM(MAC) || PLATFORM(MACCATALYST)
#import "WindowServerConnection.h"
#endif

#if PLATFORM(MAC) || PLATFORM(MACCATALYST)
#import "TCCSoftLink.h"
#endif

#define MESSAGE_CHECK(assertion) MESSAGE_CHECK_BASE(assertion, connection())
#define MESSAGE_CHECK_URL(url) MESSAGE_CHECK_BASE(checkURLReceivedFromWebProcess(url), connection())

namespace WebKit {

static const Seconds unexpectedActivityDuration = 10_s;

const MemoryCompactLookupOnlyRobinHoodHashSet<String>& WebProcessProxy::platformPathsWithAssumedReadAccess()
{
    static NeverDestroyed<MemoryCompactLookupOnlyRobinHoodHashSet<String>> platformPathsWithAssumedReadAccess(std::initializer_list<String> {
        [NSBundle bundleWithIdentifier:@"com.apple.WebCore"].resourcePath.stringByStandardizingPath,
        [NSBundle bundleForClass:NSClassFromString(@"WKWebView")].resourcePath.stringByStandardizingPath
    });

    return platformPathsWithAssumedReadAccess;
}

static Vector<String>& mediaTypeCache()
{
    ASSERT(RunLoop::isMain());
    static NeverDestroyed<Vector<String>> typeCache;
    return typeCache;
}

void WebProcessProxy::cacheMediaMIMETypes(const Vector<String>& types)
{
    if (!mediaTypeCache().isEmpty())
        return;

    mediaTypeCache() = types;
    for (Ref process : processPool().processes()) {
        if (process.ptr() != this)
            cacheMediaMIMETypesInternal(types);
    }
}

void WebProcessProxy::cacheMediaMIMETypesInternal(const Vector<String>& types)
{
    if (!mediaTypeCache().isEmpty())
        return;

    mediaTypeCache() = types;
    send(Messages::WebProcess::SetMediaMIMETypes(types), 0);
}

Vector<String> WebProcessProxy::mediaMIMETypes() const
{
    return mediaTypeCache();
}

#if ENABLE(REMOTE_INSPECTOR)
bool WebProcessProxy::shouldEnableRemoteInspector()
{
#if PLATFORM(IOS_FAMILY)
    return CFPreferencesGetAppIntegerValue(WIRRemoteInspectorEnabledKey, WIRRemoteInspectorDomainName, nullptr);
#else
    return CFPreferencesGetAppIntegerValue(CFSTR("ShowDevelopMenu"), bundleIdentifierForSandboxBroker(), nullptr);
#endif
}

void WebProcessProxy::enableRemoteInspectorIfNeeded()
{
    if (!shouldEnableRemoteInspector())
        return;
    send(Messages::WebProcess::EnableRemoteWebInspector(), 0);
}
#endif

#if HAVE(MEDIA_ACCESSIBILITY_FRAMEWORK)
void WebProcessProxy::setCaptionDisplayMode(WebCore::CaptionUserPreferences::CaptionDisplayMode displayMode)
{
    WebCore::CaptionUserPreferencesMediaAF::platformSetCaptionDisplayMode(displayMode);
}

void WebProcessProxy::setCaptionLanguage(const String& language)
{
    WebCore::CaptionUserPreferencesMediaAF::platformSetPreferredLanguage(language);
}
#endif

void WebProcessProxy::unblockAccessibilityServerIfNeeded()
{
    if (m_hasSentMessageToUnblockAccessibilityServer)
        return;
#if PLATFORM(IOS_FAMILY)
    if (!_AXSApplicationAccessibilityEnabled())
        return;
#endif
    if (!processID())
        return;
    if (!canSendMessage())
        return;

    Vector<SandboxExtension::Handle> handleArray;
#if PLATFORM(IOS_FAMILY)
    handleArray = SandboxExtension::createHandlesForMachLookup({ }, auditToken(), SandboxExtension::MachBootstrapOptions::EnableMachBootstrap);
#endif

    send(Messages::WebProcess::UnblockServicesRequiredByAccessibility(WTFMove(handleArray)), 0);
    m_hasSentMessageToUnblockAccessibilityServer = true;
}

#if PLATFORM(MAC) || PLATFORM(MACCATALYST)
void WebProcessProxy::isAXAuthenticated(CoreIPCAuditToken&& auditToken, CompletionHandler<void(bool)>&& completionHandler)
{
    auto authenticated = TCCAccessCheckAuditToken(get_TCC_kTCCServiceAccessibility(), auditToken.auditToken(), nullptr);
    completionHandler(authenticated);
}
#endif

#if PLATFORM(MAC) || PLATFORM(MACCATALYST)
void WebProcessProxy::hardwareConsoleStateChanged()
{
    m_isConnectedToHardwareConsole = WindowServerConnection::singleton().hardwareConsoleState() == WindowServerConnection::HardwareConsoleState::Connected;
    for (const auto& page : m_pageMap.values())
        page->activityStateDidChange(WebCore::ActivityState::IsConnectedToHardwareConsole);
}
#endif

#if HAVE(AUDIO_COMPONENT_SERVER_REGISTRATIONS)
void WebProcessProxy::sendAudioComponentRegistrations()
{
    dispatch_async(dispatch_get_global_queue(DISPATCH_QUEUE_PRIORITY_DEFAULT, 0), [weakThis = WeakPtr { *this }] () mutable {

        auto registrations = fetchAudioComponentServerRegistrations();
        if (!registrations)
            return;
        
        RunLoop::mainSingleton().dispatch([weakThis = WTFMove(weakThis), registrations = WTFMove(registrations)] () mutable {
            if (!weakThis)
                return;

            weakThis->send(Messages::WebProcess::ConsumeAudioComponentRegistrations(IPC::SharedBufferReference(WTFMove(registrations))), 0);
        });
    });
}
#endif

bool WebProcessProxy::messageSourceIsValidWebContentProcess()
{
    if (!hasConnection()) {
        ASSERT_NOT_REACHED();
        return false;
    }

#if USE(APPLE_INTERNAL_SDK)
#if PLATFORM(IOS) || PLATFORM(VISION)
    // FIXME(rdar://80908833): On iOS, we can only perform the below checks for platform binaries until rdar://80908833 is fixed.
    if (!currentProcessIsPlatformBinary())
        return true;
#endif

    // WebKitTestRunner does not pass the isPlatformBinary check, we should return early in this case.
    if (isRunningTest(applicationBundleIdentifier()))
        return true;

    // Confirm that the connection is from a WebContent process:
    auto [signingIdentifier, isPlatformBinary] = codeSigningIdentifierAndPlatformBinaryStatus(connection().xpcConnection());

    if (!isPlatformBinary || !signingIdentifier.startsWith("com.apple.WebKit.WebContent"_s)) {
        RELEASE_LOG_ERROR(Process, "Process is not an entitled WebContent process.");
        return false;
    }
#endif

    return true;
}

std::optional<audit_token_t> WebProcessProxy::auditToken() const
{
    if (!hasConnection())
        return std::nullopt;
    
    return protectedConnection()->getAuditToken();
}

std::optional<Vector<SandboxExtension::Handle>> WebProcessProxy::fontdMachExtensionHandles()
{
    if (std::exchange(m_sentFontdMachExtensionHandles, true))
        return std::nullopt;
    return SandboxExtension::createHandlesForMachLookup({ "com.apple.fonts"_s }, auditToken(), SandboxExtension::MachBootstrapOptions::EnableMachBootstrap);
}

#if USE(APPLE_INTERNAL_SDK) && __has_include(<WebKitAdditions/WebProcessProxyCocoaAdditions.mm>)
#import <WebKitAdditions/WebProcessProxyCocoaAdditions.mm>
#else
bool WebProcessProxy::shouldDisableJITCage() const
{
    return false;
}
#endif

#if ENABLE(LOGD_BLOCKING_IN_WEBCONTENT)
#if ENABLE(STREAMING_IPC_IN_LOG_FORWARDING)
void WebProcessProxy::createLogStream(IPC::StreamServerConnectionHandle&& serverConnection, LogStreamIdentifier identifier, CompletionHandler<void(IPC::Semaphore& streamWakeUpSemaphore, IPC::Semaphore& streamClientWaitSemaphore)>&& completionHandler)
{
    MESSAGE_CHECK(!m_logStream.get());
    m_logStream = LogStream::create(*this, WTFMove(serverConnection), identifier, WTFMove(completionHandler));
}
#else
void WebProcessProxy::createLogStream(LogStreamIdentifier identifier, CompletionHandler<void()>&& completionHandler)
{
    MESSAGE_CHECK(!m_logStream.get());
    Ref logStream = LogStream::create(*this, protectedConnection(), identifier);
    addMessageReceiver(Messages::LogStream::messageReceiverName(), logStream->identifier(), logStream);
    m_logStream = WTFMove(logStream);
    completionHandler();
}
#endif
#endif // ENABLE(LOGD_BLOCKING_IN_WEBCONTENT)

#if ENABLE(REMOTE_INSPECTOR)
void WebProcessProxy::createServiceWorkerDebuggable(WebCore::ServiceWorkerIdentifier identifier, URL&& url, WebCore::ServiceWorkerIsInspectable isInspectable, CompletionHandler<void(bool shouldWaitForAutoInspection)>&& completionHandler)
{
    MESSAGE_CHECK_URL(url);
    RELEASE_LOG(Inspector, "WebProcessProxy::createServiceWorkerDebuggable");
    if (!shouldEnableRemoteInspector()) {
        if (completionHandler)
            completionHandler(false);
        return;
    }

    Ref serviceWorkerDebuggableProxy = ServiceWorkerDebuggableProxy::create(url.string(), identifier, *this);
    m_serviceWorkerDebuggableProxies.add(identifier, serviceWorkerDebuggableProxy);
    serviceWorkerDebuggableProxy->init();
    serviceWorkerDebuggableProxy->setInspectable(isInspectable == WebCore::ServiceWorkerIsInspectable::Yes);

    if (completionHandler) {
#if ENABLE(REMOTE_INSPECTOR_SERVICE_WORKER_AUTO_INSPECTION)
        completionHandler(serviceWorkerDebuggableProxy->isPausedWaitingForAutomaticInspection());
#else
        completionHandler(false);
#endif
    }
}

void WebProcessProxy::deleteServiceWorkerDebuggable(WebCore::ServiceWorkerIdentifier identifier)
{
    RELEASE_LOG(Inspector, "WebProcessProxy::deleteServiceWorkerDebuggable");
    if (!shouldEnableRemoteInspector())
        return;
    m_serviceWorkerDebuggableProxies.remove(identifier);
}

void WebProcessProxy::sendMessageToInspector(WebCore::ServiceWorkerIdentifier identifier, String&& message)
{
    RELEASE_LOG(Inspector, "WebProcessProxy::sendMessageToInspector");
    if (!shouldEnableRemoteInspector())
        return;
    if (RefPtr serviceWorkerDebuggableProxy = m_serviceWorkerDebuggableProxies.get(identifier)) {
        auto targetID = serviceWorkerDebuggableProxy->targetIdentifier();
        Inspector::RemoteInspector::singleton().sendMessageToRemote(targetID, WTFMove(message));
    }
}
#endif

void WebProcessProxy::platformDestroy()
{
#if PLATFORM(IOS_FAMILY)
#if HAVE(MOUSE_DEVICE_OBSERVATION)
    [[WKMouseDeviceObserver sharedInstance] stop];
#endif
#if HAVE(STYLUS_DEVICE_OBSERVATION)
    [[WKStylusDeviceObserver sharedInstance] stop];
#endif
#endif // PLATFORM(IOS_FAMILY)

#if ENABLE(LOGD_BLOCKING_IN_WEBCONTENT)
    if (m_logStream.get()) {
#if !ENABLE(STREAMING_IPC_IN_LOG_FORWARDING)
        removeMessageReceiver(Messages::LogStream::messageReceiverName(), m_logStream->identifier());
#endif
        m_logStream.reset();
    }

#endif
}

void WebProcessProxy::platformResumeProcess()
{
    if (m_platformSuspendDidReleaseNearSuspendedAssertion) {
        m_platformSuspendDidReleaseNearSuspendedAssertion = false;
        protectedThrottler()->setShouldTakeNearSuspendedAssertion(true);
    }
}

void WebProcessProxy::platformSuspendProcess()
{
    m_platformSuspendDidReleaseNearSuspendedAssertion = throttler().isHoldingNearSuspendedAssertion();
    protectedThrottler()->setShouldTakeNearSuspendedAssertion(false);
}

}

#undef MESSAGE_CHECK_URL
#undef MESSAGE_CHECK
