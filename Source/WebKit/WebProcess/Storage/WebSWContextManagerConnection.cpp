/*
 * Copyright (C) 2017-2022 Apple Inc. All rights reserved.
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
#include "WebSWContextManagerConnection.h"

#include "FormDataReference.h"
#include "Logging.h"
#include "NetworkConnectionToWebProcessMessages.h"
#include "NetworkProcessMessages.h"
#include "RemoteWebLockRegistry.h"
#include "RemoteWorkerFrameLoaderClient.h"
#include "RemoteWorkerInitializationData.h"
#include "RemoteWorkerLibWebRTCProvider.h"
#include "ServiceWorkerFetchTaskMessages.h"
#include "WebBadgeClient.h"
#include "WebBroadcastChannelRegistry.h"
#include "WebCacheStorageProvider.h"
#include "WebCompiledContentRuleListData.h"
#include "WebCookieJar.h"
#include "WebCryptoClient.h"
#include "WebDatabaseProvider.h"
#include "WebLocalFrameLoaderClient.h"
#include "WebMessagePortChannelProvider.h"
#include "WebNotificationClient.h"
#include "WebPage.h"
#include "WebPreferencesKeys.h"
#include "WebPreferencesStore.h"
#include "WebProcess.h"
#include "WebProcessPoolMessages.h"
#include "WebProcessProxyMessages.h"
#include "WebSWContextManagerConnectionMessages.h"
#include "WebSWServerToContextConnectionMessages.h"
#include "WebServiceWorkerFetchTaskClient.h"
#include "WebSocketProvider.h"
#include "WebStorageProvider.h"
#include "WebUserContentController.h"
#include "WebWorkerClient.h"
#include <WebCore/EditorClient.h>
#include <WebCore/EmptyClients.h>
#include <WebCore/MessageWithMessagePorts.h>
#include <WebCore/NotificationData.h>
#include <WebCore/PageConfiguration.h>
#include <WebCore/RemoteFrameClient.h>
#include <WebCore/ScriptExecutionContextIdentifier.h>
#include <WebCore/SerializedScriptValue.h>
#include <WebCore/ServiceWorkerClientData.h>
#include <WebCore/ServiceWorkerClientQueryOptions.h>
#include <WebCore/ServiceWorkerJobDataIdentifier.h>
#include <WebCore/UserAgent.h>
#include <WebCore/UserContentURLPattern.h>
#include <wtf/ProcessID.h>

#if USE(QUICK_LOOK)
#include <WebCore/LegacyPreviewLoaderClient.h>
#endif

#if ENABLE(REMOTE_INSPECTOR) && PLATFORM(COCOA)
#include "ServiceWorkerDebuggableFrontendChannel.h"
#endif

namespace WebKit {
using namespace PAL;
using namespace WebCore;

WebSWContextManagerConnection::WebSWContextManagerConnection(Ref<IPC::Connection>&& connection, Site&& site, std::optional<ScriptExecutionContextIdentifier> serviceWorkerPageIdentifier, PageGroupIdentifier pageGroupID, WebPageProxyIdentifier webPageProxyID, PageIdentifier pageID, const WebPreferencesStore& store, RemoteWorkerInitializationData&& initializationData)
    : m_connectionToNetworkProcess(WTFMove(connection))
    , m_site(WTFMove(site))
    , m_serviceWorkerPageIdentifier(serviceWorkerPageIdentifier)
    , m_pageGroupID(pageGroupID)
    , m_webPageProxyID(webPageProxyID)
    , m_pageID(pageID)
#if PLATFORM(COCOA)
    , m_userAgent(standardUserAgentWithApplicationName({ }))
#else
    , m_userAgent(standardUserAgent())
#endif
    , m_userContentController(WebUserContentController::getOrCreate(WTFMove(initializationData.userContentControllerParameters)))
    , m_queue(WorkQueue::create("WebSWContextManagerConnection queue"_s, WorkQueue::QOS::UserInitiated))
{
    WebPage::updatePreferencesGenerated(store);
    m_preferencesStore = store;

    WebProcess::singleton().disableTermination();
}

WebSWContextManagerConnection::~WebSWContextManagerConnection() = default;

void WebSWContextManagerConnection::establishConnection(CompletionHandler<void()>&& completionHandler)
{
    m_connectionToNetworkProcess->addWorkQueueMessageReceiver(Messages::WebSWContextManagerConnection::messageReceiverName(), m_queue.get(), *this);
    m_connectionToNetworkProcess->sendWithAsyncReply(Messages::NetworkConnectionToWebProcess::EstablishSWContextConnection { m_webPageProxyID, m_site, m_serviceWorkerPageIdentifier }, WTFMove(completionHandler), 0);
}

void WebSWContextManagerConnection::stop()
{
    ASSERT(isMainRunLoop());

    m_connectionToNetworkProcess->removeWorkQueueMessageReceiver(Messages::WebSWContextManagerConnection::messageReceiverName());
}

void WebSWContextManagerConnection::updatePreferencesStore(WebPreferencesStore&& store)
{
    if (!isMainRunLoop()) {
        callOnMainRunLoop([protectedThis = Ref { *this }, store = WTFMove(store).isolatedCopy()]() mutable {
            protectedThis->updatePreferencesStore(WTFMove(store));
        });
        return;
    }

    WebPage::updatePreferencesGenerated(store);
    m_preferencesStore = WTFMove(store);
}

void WebSWContextManagerConnection::updateAppInitiatedValue(ServiceWorkerIdentifier serviceWorkerIdentifier, WebCore::LastNavigationWasAppInitiated lastNavigationWasAppInitiated)
{
    if (!isMainRunLoop()) {
        callOnMainRunLoop([protectedThis = Ref { *this }, serviceWorkerIdentifier, lastNavigationWasAppInitiated]() mutable {
            protectedThis->updateAppInitiatedValue(serviceWorkerIdentifier, lastNavigationWasAppInitiated);
        });
        return;
    }

    if (RefPtr serviceWorkerThreadProxy = SWContextManager::singleton().serviceWorkerThreadProxy(serviceWorkerIdentifier))
        serviceWorkerThreadProxy->setLastNavigationWasAppInitiated(lastNavigationWasAppInitiated == WebCore::LastNavigationWasAppInitiated::Yes);
}

void WebSWContextManagerConnection::installServiceWorker(ServiceWorkerContextData&& contextData, ServiceWorkerData&& workerData, String&& userAgent, WorkerThreadMode workerThreadMode, ServiceWorkerIsInspectable inspectable, OptionSet<AdvancedPrivacyProtections> advancedPrivacyProtections)
{
    assertIsCurrent(m_queue.get());

    callOnMainRunLoopAndWait([this, protectedThis = Ref { *this }, contextData = WTFMove(contextData).isolatedCopy(), workerData = WTFMove(workerData).isolatedCopy(), userAgent = WTFMove(userAgent).isolatedCopy(), workerThreadMode, inspectable, advancedPrivacyProtections]() mutable {
        auto pageConfiguration = pageConfigurationWithEmptyClients(m_pageID, WebProcess::singleton().sessionID());
        pageConfiguration.badgeClient = WebBadgeClient::create();
        pageConfiguration.databaseProvider = WebDatabaseProvider::getOrCreate(m_pageGroupID);
        pageConfiguration.socketProvider = WebSocketProvider::create(m_webPageProxyID);
        pageConfiguration.broadcastChannelRegistry = WebProcess::singleton().broadcastChannelRegistry();
        pageConfiguration.userContentProvider = m_userContentController;
        pageConfiguration.cookieJar = WebCookieJar::create();
        pageConfiguration.cryptoClient = makeUniqueRef<WebCryptoClient>();
#if ENABLE(WEB_RTC)
        pageConfiguration.webRTCProvider = makeUniqueRef<RemoteWorkerLibWebRTCProvider>();
#endif
        pageConfiguration.storageProvider = makeUniqueRef<WebStorageProvider>(WebProcess::singleton().mediaKeysStorageDirectory(), WebProcess::singleton().mediaKeysStorageSalt());

        if (RefPtr serviceWorkerPage = m_serviceWorkerPageIdentifier ? Page::serviceWorkerPage(*m_serviceWorkerPageIdentifier) : nullptr)
            pageConfiguration.corsDisablingPatterns = serviceWorkerPage->corsDisablingPatterns();

        auto effectiveUserAgent =  WTFMove(userAgent);
        if (effectiveUserAgent.isNull())
            effectiveUserAgent = m_userAgent;

        pageConfiguration.mainFrameCreationParameters = PageConfiguration::LocalMainFrameCreationParameters {
            CompletionHandler<UniqueRef<WebCore::LocalFrameLoaderClient>(WebCore::LocalFrame&, WebCore::FrameLoader&)> { [webPageProxyID = m_webPageProxyID, pageID = m_pageID, effectiveUserAgent, serviceWorkerPageIdentifier = contextData.serviceWorkerPageIdentifier] (auto&, auto& frameLoader) mutable {
                auto client = makeUniqueRefWithoutRefCountedCheck<RemoteWorkerFrameLoaderClient>(frameLoader, webPageProxyID, pageID, effectiveUserAgent);
                if (serviceWorkerPageIdentifier)
                    client->setServiceWorkerPageIdentifier(*serviceWorkerPageIdentifier);
                return client;
            } }, SandboxFlags { },
            ReferrerPolicy::EmptyString
        };

        [[maybe_unused]] auto serviceWorkerIdentifier = contextData.serviceWorkerIdentifier;
        [[maybe_unused]] auto scopeURL = contextData.registration.scopeURL;

        auto lastNavigationWasAppInitiated = contextData.lastNavigationWasAppInitiated;
        Ref page = Page::create(WTFMove(pageConfiguration));
        if (m_preferencesStore) {
            WebPage::updateSettingsGenerated(*m_preferencesStore, page->settings());
            page->settings().setStorageBlockingPolicy(static_cast<StorageBlockingPolicy>(m_preferencesStore->getUInt32ValueForKey(WebPreferencesKey::storageBlockingPolicyKey())));
        }
        if (WebProcess::singleton().isLockdownModeEnabled())
            WebPage::adjustSettingsForLockdownMode(page->settings(), m_preferencesStore ? &m_preferencesStore.value() : nullptr);

        page->setupForRemoteWorker(contextData.scriptURL, contextData.registration.key.topOrigin(), contextData.referrerPolicy, advancedPrivacyProtections);
#if ENABLE(REMOTE_INSPECTOR)
        page->setInspectable(inspectable == ServiceWorkerIsInspectable::Yes);
#endif // ENABLE(REMOTE_INSPECTOR)

        std::unique_ptr<WebCore::NotificationClient> notificationClient;
#if ENABLE(NOTIFICATIONS)
        notificationClient = makeUnique<WebNotificationClient>(nullptr);
#endif

        auto serviceWorkerThreadProxy = ServiceWorkerThreadProxy::create(Ref { page }, WTFMove(contextData), WTFMove(workerData), WTFMove(effectiveUserAgent), workerThreadMode, WebProcess::singleton().cacheStorageProvider(), WTFMove(notificationClient));

        auto workerClient = WebWorkerClient::create(WTFMove(page), serviceWorkerThreadProxy->thread());
        serviceWorkerThreadProxy->thread().setWorkerClient(workerClient.moveToUniquePtr());

        if (lastNavigationWasAppInitiated)
            serviceWorkerThreadProxy->setLastNavigationWasAppInitiated(lastNavigationWasAppInitiated == WebCore::LastNavigationWasAppInitiated::Yes);

        // Set the service worker's inspectability and potentially provide automatic inspection support.
        //
        // REMOVE_XPC_AND_MACH_SANDBOX_EXTENSIONS_IN_WEBCONTENT means we should use a ServiceWorkerDebuggableProxy in the
        // UI process as the debuggable, instead of the traditional ServiceWorkerDebuggable owned by the thread proxy.
        //
        // REMOTE_INSPECTOR_SERVICE_WORKER_AUTO_INSPECTION means the ServiceWorkerThread starts in the WaitForInspector
        // mode, and script evaluation should be prevented until automatic inspection is resolved.

        Function<void()> handleThreadDebuggerTasksStarted = { };

#if ENABLE(REMOTE_INSPECTOR)
#if PLATFORM(COCOA) && ENABLE(REMOVE_XPC_AND_MACH_SANDBOX_EXTENSIONS_IN_WEBCONTENT)

#if ENABLE(REMOTE_INSPECTOR_SERVICE_WORKER_AUTO_INSPECTION)
        ASSERT(RunLoop::isMain());
        handleThreadDebuggerTasksStarted = [serviceWorkerIdentifier, scopeURL, inspectable] {
            // This may or may not be called on the main thread.
            CompletionHandler<void(bool)> handleDebuggableCreated { [serviceWorkerIdentifier](bool shouldWait) {
                ASSERT(RunLoop::isMain());
                if (!shouldWait)
                    SWContextManager::singleton().stopRunningDebuggerTasksOnServiceWorker(serviceWorkerIdentifier);
                // Otherwise, let the worker remain paused until the auto-launched inspector's frontendInitialized.
            }, CompletionHandlerCallThread::MainThread };
            WebProcess::singleton().sendWithAsyncReply(Messages::WebProcessProxy::CreateServiceWorkerDebuggable(serviceWorkerIdentifier, scopeURL, inspectable), WTFMove(handleDebuggableCreated));
        };
#else
        WebProcess::singleton().send(Messages::WebProcessProxy::CreateServiceWorkerDebuggable(serviceWorkerIdentifier, scopeURL, inspectable));
#endif

#else // not (PLATFORM(COCOA) && ENABLE(REMOVE_XPC_AND_MACH_SANDBOX_EXTENSIONS_IN_WEBCONTENT))

#if ENABLE(REMOTE_INSPECTOR_SERVICE_WORKER_AUTO_INSPECTION)
        handleThreadDebuggerTasksStarted = [serviceWorkerThreadProxy, serviceWorkerIdentifier, inspectable] {
            serviceWorkerThreadProxy->remoteDebuggable().setInspectable(inspectable == ServiceWorkerIsInspectable::Yes);
            // setInspectable will block until automatic inspection is resolved (rejected or frontend-initialized).
            SWContextManager::singleton().stopRunningDebuggerTasksOnServiceWorker(serviceWorkerIdentifier);
        };
#else
        serviceWorkerThreadProxy->remoteDebuggable().setInspectable(inspectable == ServiceWorkerIsInspectable::Yes);
#endif

#endif // not (PLATFORM(COCOA) && ENABLE(REMOVE_XPC_AND_MACH_SANDBOX_EXTENSIONS_IN_WEBCONTENT))
#endif // ENABLE(REMOTE_INSPECTOR)

        SWContextManager::singleton().registerServiceWorkerThreadForInstall(WTFMove(serviceWorkerThreadProxy), WTFMove(handleThreadDebuggerTasksStarted));

        RELEASE_LOG(ServiceWorker, "Created service worker %" PRIu64 " in process PID %i", serviceWorkerIdentifier.toUInt64(), getCurrentProcessID());
    });
}

void WebSWContextManagerConnection::setUserAgent(String&& userAgent)
{
    if (!isMainThread()) {
        callOnMainRunLoop([protectedThis = Ref { *this }, userAgent = WTFMove(userAgent).isolatedCopy()]() mutable {
            protectedThis->setUserAgent(WTFMove(userAgent));
        });
        return;
    }
    m_userAgent = WTFMove(userAgent);
}

void WebSWContextManagerConnection::serviceWorkerStarted(std::optional<ServiceWorkerJobDataIdentifier> jobDataIdentifier, ServiceWorkerIdentifier serviceWorkerIdentifier, bool doesHandleFetch)
{
    m_connectionToNetworkProcess->send(Messages::WebSWServerToContextConnection::ScriptContextStarted { jobDataIdentifier, serviceWorkerIdentifier, doesHandleFetch }, 0);
}

void WebSWContextManagerConnection::serviceWorkerFailedToStart(std::optional<ServiceWorkerJobDataIdentifier> jobDataIdentifier, ServiceWorkerIdentifier serviceWorkerIdentifier, const String& exceptionMessage)
{
    m_connectionToNetworkProcess->send(Messages::WebSWServerToContextConnection::ScriptContextFailedToStart { jobDataIdentifier, serviceWorkerIdentifier, exceptionMessage }, 0);
}

void WebSWContextManagerConnection::cancelFetch(SWServerConnectionIdentifier serverConnectionIdentifier, ServiceWorkerIdentifier serviceWorkerIdentifier, FetchIdentifier fetchIdentifier)
{
    assertIsCurrent(m_queue.get());

    if (auto serviceWorkerThreadProxy = SWContextManager::singleton().serviceWorkerThreadProxyFromBackgroundThread(serviceWorkerIdentifier))
        serviceWorkerThreadProxy->cancelFetch(serverConnectionIdentifier, fetchIdentifier);
    m_ongoingNavigationFetchTasks.remove({ serverConnectionIdentifier, fetchIdentifier });
}

void WebSWContextManagerConnection::continueDidReceiveFetchResponse(SWServerConnectionIdentifier serverConnectionIdentifier, ServiceWorkerIdentifier serviceWorkerIdentifier, FetchIdentifier fetchIdentifier)
{
    assertIsCurrent(m_queue.get());

    if (auto task = m_ongoingNavigationFetchTasks.take({ serverConnectionIdentifier, fetchIdentifier }))
        task->continueDidReceiveResponse();
}

void WebSWContextManagerConnection::startFetch(SWServerConnectionIdentifier serverConnectionIdentifier, ServiceWorkerIdentifier serviceWorkerIdentifier, FetchIdentifier fetchIdentifier, ResourceRequest&& request, FetchOptions&& options, IPC::FormDataReference&& formData, String&& referrer, bool isServiceWorkerNavigationPreloadEnabled, String&& clientIdentifier, String&& resultingClientIdentifier)
{
    assertIsCurrent(m_queue.get());

    auto serviceWorkerThreadProxy = SWContextManager::singleton().serviceWorkerThreadProxyFromBackgroundThread(serviceWorkerIdentifier);
    if (!serviceWorkerThreadProxy) {
        m_connectionToNetworkProcess->send(Messages::ServiceWorkerFetchTask::DidNotHandle { }, fetchIdentifier);
        return;
    }

    callOnMainRunLoop([serviceWorkerThreadProxy, isAppInitiated = request.isAppInitiated()]() mutable {
        serviceWorkerThreadProxy->setLastNavigationWasAppInitiated(isAppInitiated);
    });

    bool needsContinueDidReceiveResponseMessage = request.requester() == ResourceRequestRequester::Main;
    auto client = WebServiceWorkerFetchTaskClient::create(m_connectionToNetworkProcess.copyRef(), serviceWorkerIdentifier, serverConnectionIdentifier, fetchIdentifier, needsContinueDidReceiveResponseMessage);
    if (needsContinueDidReceiveResponseMessage)
        m_ongoingNavigationFetchTasks.add({ serverConnectionIdentifier, fetchIdentifier }, Ref { client });

    request.setHTTPBody(formData.takeData());
    serviceWorkerThreadProxy->startFetch(serverConnectionIdentifier, fetchIdentifier, WTFMove(client), WTFMove(request), WTFMove(referrer), WTFMove(options), isServiceWorkerNavigationPreloadEnabled, WTFMove(clientIdentifier), WTFMove(resultingClientIdentifier));
}

void WebSWContextManagerConnection::postMessageToServiceWorker(ServiceWorkerIdentifier serviceWorkerIdentifier, MessageWithMessagePorts&& message, ServiceWorkerOrClientData&& sourceData)
{
    assertIsCurrent(m_queue.get());

    if (auto serviceWorkerThreadProxy = SWContextManager::singleton().serviceWorkerThreadProxyFromBackgroundThread(serviceWorkerIdentifier))
        serviceWorkerThreadProxy->fireMessageEvent(WTFMove(message), WTFMove(sourceData));
}

void WebSWContextManagerConnection::fireInstallEvent(ServiceWorkerIdentifier identifier)
{
    assertIsCurrent(m_queue.get());

    if (auto serviceWorkerThreadProxy = SWContextManager::singleton().serviceWorkerThreadProxyFromBackgroundThread(identifier))
        serviceWorkerThreadProxy->fireInstallEvent();
}

void WebSWContextManagerConnection::fireActivateEvent(ServiceWorkerIdentifier identifier)
{
    assertIsCurrent(m_queue.get());

    if (auto serviceWorkerThreadProxy = SWContextManager::singleton().serviceWorkerThreadProxyFromBackgroundThread(identifier))
        serviceWorkerThreadProxy->fireActivateEvent();
}

void WebSWContextManagerConnection::firePushEvent(ServiceWorkerIdentifier identifier, std::optional<std::span<const uint8_t>> ipcData, std::optional<NotificationPayload>&& proposedPayload, CompletionHandler<void(bool, std::optional<NotificationPayload>&&)>&& callback)
{
    assertIsCurrent(m_queue.get());

    std::optional<Vector<uint8_t>> data;
    if (ipcData)
        data = Vector<uint8_t> { *ipcData };

    auto inQueueCallback = [queue = m_queue, callback = WTFMove(callback)](bool result, std::optional<NotificationPayload>&& resultPayload) mutable {
        queue->dispatch([result, resultPayload = crossThreadCopy(WTFMove(resultPayload)), callback = WTFMove(callback)]() mutable {
            callback(result, WTFMove(resultPayload));
        });
    };

    callOnMainRunLoop([identifier, data = WTFMove(data), proposedPayload = crossThreadCopy(WTFMove(proposedPayload)), callback = WTFMove(inQueueCallback)]() mutable {
        SWContextManager::singleton().firePushEvent(identifier, WTFMove(data), WTFMove(proposedPayload), WTFMove(callback));
    });
}

void WebSWContextManagerConnection::fireNotificationEvent(ServiceWorkerIdentifier identifier, NotificationData&& data, NotificationEventType eventType, CompletionHandler<void(bool)>&& callback)
{
    assertIsCurrent(m_queue.get());

    auto inQueueCallback = [queue = m_queue, callback = WTFMove(callback)](bool result) mutable {
        queue->dispatch([result, callback = WTFMove(callback)]() mutable {
            callback(result);
        });
    };
    callOnMainRunLoop([identifier, data = WTFMove(data).isolatedCopy(), eventType, callback = WTFMove(inQueueCallback)]() mutable {
        SWContextManager::singleton().fireNotificationEvent(identifier, WTFMove(data), eventType, WTFMove(callback));
    });
}

void WebSWContextManagerConnection::fireBackgroundFetchEvent(ServiceWorkerIdentifier identifier, BackgroundFetchInformation&& info, CompletionHandler<void(bool)>&& callback)
{
    assertIsCurrent(m_queue.get());

    auto inQueueCallback = [queue = m_queue, callback = WTFMove(callback)](bool result) mutable {
        queue->dispatch([result, callback = WTFMove(callback)]() mutable {
            callback(result);
        });
    };
    callOnMainRunLoop([identifier, info = WTFMove(info).isolatedCopy(), callback = WTFMove(inQueueCallback)]() mutable {
        SWContextManager::singleton().fireBackgroundFetchEvent(identifier, WTFMove(info), WTFMove(callback));
    });
}

void WebSWContextManagerConnection::fireBackgroundFetchClickEvent(ServiceWorkerIdentifier identifier, BackgroundFetchInformation&& info, CompletionHandler<void(bool)>&& callback)
{
    assertIsCurrent(m_queue.get());

    auto inQueueCallback = [queue = m_queue, callback = WTFMove(callback)](bool result) mutable {
        queue->dispatch([result, callback = WTFMove(callback)]() mutable {
            callback(result);
        });
    };
    callOnMainRunLoop([identifier, info = WTFMove(info).isolatedCopy(), callback = WTFMove(inQueueCallback)]() mutable {
        SWContextManager::singleton().fireBackgroundFetchClickEvent(identifier, WTFMove(info), WTFMove(callback));
    });
}

void WebSWContextManagerConnection::terminateWorker(ServiceWorkerIdentifier identifier)
{
    assertIsCurrent(m_queue.get());

    callOnMainRunLoop([identifier] {
        SWContextManager::singleton().terminateWorker(identifier, SWContextManager::workerTerminationTimeout, nullptr);
    });
}

#if ENABLE(SHAREABLE_RESOURCE) && PLATFORM(COCOA)
void WebSWContextManagerConnection::didSaveScriptsToDisk(WebCore::ServiceWorkerIdentifier serviceWorkerIdentifier, ScriptBuffer&& script, HashMap<URL, ScriptBuffer>&& importedScripts)
{
    assertIsCurrent(m_queue.get());

    if (auto serviceWorkerThreadProxy = SWContextManager::singleton().serviceWorkerThreadProxyFromBackgroundThread(serviceWorkerIdentifier))
        serviceWorkerThreadProxy->didSaveScriptsToDisk(WTFMove(script), WTFMove(importedScripts));
}
#endif

void WebSWContextManagerConnection::convertFetchToDownload(SWServerConnectionIdentifier serverConnectionIdentifier, ServiceWorkerIdentifier serviceWorkerIdentifier, FetchIdentifier fetchIdentifier)
{
    assertIsCurrent(m_queue.get());

    if (auto task = m_ongoingNavigationFetchTasks.take({ serverConnectionIdentifier, fetchIdentifier }))
        task->convertFetchToDownload();
}

void WebSWContextManagerConnection::navigationPreloadIsReady(SWServerConnectionIdentifier serverConnectionIdentifier, ServiceWorkerIdentifier serviceWorkerIdentifier, FetchIdentifier fetchIdentifier, ResourceResponse&& response)
{
    assertIsCurrent(m_queue.get());

    if (auto serviceWorkerThreadProxy = SWContextManager::singleton().serviceWorkerThreadProxyFromBackgroundThread(serviceWorkerIdentifier))
        serviceWorkerThreadProxy->navigationPreloadIsReady(serverConnectionIdentifier, fetchIdentifier, WTFMove(response));
}

void WebSWContextManagerConnection::navigationPreloadFailed(SWServerConnectionIdentifier serverConnectionIdentifier, ServiceWorkerIdentifier serviceWorkerIdentifier, FetchIdentifier fetchIdentifier, ResourceError&& error)
{
    assertIsCurrent(m_queue.get());

    if (auto serviceWorkerThreadProxy = SWContextManager::singleton().serviceWorkerThreadProxyFromBackgroundThread(serviceWorkerIdentifier))
        serviceWorkerThreadProxy->navigationPreloadFailed(serverConnectionIdentifier, fetchIdentifier, WTFMove(error));
}

void WebSWContextManagerConnection::updateRegistrationState(WebCore::ServiceWorkerRegistrationIdentifier identifier, WebCore::ServiceWorkerRegistrationState state, const std::optional<WebCore::ServiceWorkerData>& serviceWorkerData)
{
    assertIsCurrent(m_queue.get());

    SWContextManager::singleton().updateRegistrationState(identifier, state, serviceWorkerData);
}

void WebSWContextManagerConnection::updateWorkerState(WebCore::ServiceWorkerIdentifier identifier, WebCore::ServiceWorkerState state)
{
    assertIsCurrent(m_queue.get());

    SWContextManager::singleton().updateWorkerState(identifier, state);
}

void WebSWContextManagerConnection::fireUpdateFoundEvent(WebCore::ServiceWorkerRegistrationIdentifier identifier)
{
    assertIsCurrent(m_queue.get());

    SWContextManager::singleton().fireUpdateFoundEvent(identifier);
}

void WebSWContextManagerConnection::setRegistrationLastUpdateTime(WebCore::ServiceWorkerRegistrationIdentifier identifier, WallTime time)
{
    assertIsCurrent(m_queue.get());

    SWContextManager::singleton().setRegistrationLastUpdateTime(identifier, time);
}

void WebSWContextManagerConnection::setRegistrationUpdateViaCache(WebCore::ServiceWorkerRegistrationIdentifier identifier, WebCore::ServiceWorkerUpdateViaCache value)
{
    assertIsCurrent(m_queue.get());

    SWContextManager::singleton().setRegistrationUpdateViaCache(identifier, value);
}

void WebSWContextManagerConnection::postMessageToServiceWorkerClient(const ScriptExecutionContextIdentifier& destinationIdentifier, const MessageWithMessagePorts& message, ServiceWorkerIdentifier sourceIdentifier, const String& sourceOrigin)
{
    for (auto& port : message.transferredPorts)
        WebMessagePortChannelProvider::singleton().messagePortSentToRemote(port.first);

    m_connectionToNetworkProcess->send(Messages::WebSWServerToContextConnection::PostMessageToServiceWorkerClient(destinationIdentifier, message, sourceIdentifier, sourceOrigin), 0);
}

void WebSWContextManagerConnection::didFinishInstall(std::optional<ServiceWorkerJobDataIdentifier> jobDataIdentifier, ServiceWorkerIdentifier serviceWorkerIdentifier, bool wasSuccessful)
{
    m_connectionToNetworkProcess->send(Messages::WebSWServerToContextConnection::DidFinishInstall(jobDataIdentifier, serviceWorkerIdentifier, wasSuccessful), 0);
}

void WebSWContextManagerConnection::didFinishActivation(ServiceWorkerIdentifier serviceWorkerIdentifier)
{
    m_connectionToNetworkProcess->send(Messages::WebSWServerToContextConnection::DidFinishActivation(serviceWorkerIdentifier), 0);
}

void WebSWContextManagerConnection::setServiceWorkerHasPendingEvents(ServiceWorkerIdentifier serviceWorkerIdentifier, bool hasPendingEvents)
{
    m_connectionToNetworkProcess->send(Messages::WebSWServerToContextConnection::SetServiceWorkerHasPendingEvents(serviceWorkerIdentifier, hasPendingEvents), 0);
}

void WebSWContextManagerConnection::skipWaiting(ServiceWorkerIdentifier serviceWorkerIdentifier, CompletionHandler<void()>&& callback)
{
    m_connectionToNetworkProcess->sendWithAsyncReply(Messages::WebSWServerToContextConnection::SkipWaiting(serviceWorkerIdentifier), WTFMove(callback), 0);
}

void WebSWContextManagerConnection::setScriptResource(ServiceWorkerIdentifier serviceWorkerIdentifier, const URL& url, const ServiceWorkerContextData::ImportedScript& script)
{
    m_connectionToNetworkProcess->send(Messages::WebSWServerToContextConnection::SetScriptResource { serviceWorkerIdentifier, url, script }, 0);
}

void WebSWContextManagerConnection::workerTerminated(ServiceWorkerIdentifier serviceWorkerIdentifier)
{
    RELEASE_LOG(ServiceWorker, "WebSWContextManagerConnection::workerTerminated %" PRIu64, serviceWorkerIdentifier.toUInt64());
#if ENABLE(REMOTE_INSPECTOR) && PLATFORM(COCOA)
    WebProcess::singleton().send(Messages::WebProcessProxy::DeleteServiceWorkerDebuggable(serviceWorkerIdentifier));
#endif
    m_connectionToNetworkProcess->send(Messages::WebSWServerToContextConnection::WorkerTerminated(serviceWorkerIdentifier), 0);
}

void WebSWContextManagerConnection::findClientByVisibleIdentifier(WebCore::ServiceWorkerIdentifier serviceWorkerIdentifier, const String& clientIdentifier, FindClientByIdentifierCallback&& callback)
{
    m_connectionToNetworkProcess->sendWithAsyncReply(Messages::WebSWServerToContextConnection::FindClientByVisibleIdentifier { serviceWorkerIdentifier, clientIdentifier }, WTFMove(callback));
}

void WebSWContextManagerConnection::matchAll(WebCore::ServiceWorkerIdentifier serviceWorkerIdentifier, const ServiceWorkerClientQueryOptions& options, ServiceWorkerClientsMatchAllCallback&& callback)
{
    m_connectionToNetworkProcess->sendWithAsyncReply(Messages::WebSWServerToContextConnection::MatchAll { serviceWorkerIdentifier, options }, WTFMove(callback), 0);
}

void WebSWContextManagerConnection::openWindow(WebCore::ServiceWorkerIdentifier serviceWorkerIdentifier, const URL& url, OpenWindowCallback&& callback)
{
    m_connectionToNetworkProcess->sendWithAsyncReply(Messages::WebSWServerToContextConnection::OpenWindow { serviceWorkerIdentifier, url }, [callback = WTFMove(callback)] (auto&& result) mutable {
        if (!result.has_value()) {
            callback(result.error().toException());
            return;
        }
        callback(WTFMove(result.value()));
    });
}

void WebSWContextManagerConnection::claim(ServiceWorkerIdentifier serviceWorkerIdentifier, CompletionHandler<void(ExceptionOr<void>&&)>&& callback)
{
    m_connectionToNetworkProcess->sendWithAsyncReply(Messages::WebSWServerToContextConnection::Claim { serviceWorkerIdentifier }, [callback = WTFMove(callback)](auto&& result) mutable {
        callback(result ? result->toException() : ExceptionOr<void> { });
    });
}

void WebSWContextManagerConnection::navigate(ScriptExecutionContextIdentifier clientIdentifier, ServiceWorkerIdentifier serviceWorkerIdentifier, const URL& url, NavigateCallback&& callback)
{
    m_connectionToNetworkProcess->sendWithAsyncReply(Messages::WebSWServerToContextConnection::Navigate { clientIdentifier, serviceWorkerIdentifier, url }, [callback = WTFMove(callback)](auto&& result) mutable {
        if (!result.has_value()) {
            callback(WTFMove(result).error().toException());
            return;
        }
        callback(std::forward<decltype(result)>(result).value());
    });
}

void WebSWContextManagerConnection::focus(ScriptExecutionContextIdentifier clientIdentifier, CompletionHandler<void(std::optional<WebCore::ServiceWorkerClientData>&&)>&& callback)
{
    m_connectionToNetworkProcess->sendWithAsyncReply(Messages::WebSWServerToContextConnection::Focus { clientIdentifier }, WTFMove(callback));
}

void WebSWContextManagerConnection::close()
{
    if (!isMainRunLoop()) {
        callOnMainRunLoop([protectedThis = Ref { *this }] {
            protectedThis->close();
        });
        return;
    }

    RELEASE_LOG(ServiceWorker, "Service worker process is requested to stop all service workers (already stopped = %d)", isClosed());
    if (isClosed())
        return;

    setAsClosed();

    m_connectionToNetworkProcess->send(Messages::NetworkConnectionToWebProcess::CloseSWContextConnection { }, 0);
    SWContextManager::singleton().stopAllServiceWorkers();
    WebProcess::singleton().enableTermination();
}

void WebSWContextManagerConnection::setThrottleState(bool isThrottleable)
{
    assertIsCurrent(m_queue.get());

    callOnMainRunLoop([protectedThis = Ref { *this }, isThrottleable] {
        RELEASE_LOG(ServiceWorker, "Service worker throttleable state is set to %d", isThrottleable);
        protectedThis->m_isThrottleable = isThrottleable;
        WebProcess::singleton().setProcessSuppressionEnabled(isThrottleable);
    });
}

void WebSWContextManagerConnection::setInspectable(ServiceWorkerIsInspectable inspectable)
{
    assertIsCurrent(m_queue.get());

    callOnMainRunLoop([inspectable] {
        SWContextManager::singleton().setInspectable(inspectable == ServiceWorkerIsInspectable::Yes);
    });
}

bool WebSWContextManagerConnection::isThrottleable() const
{
    return m_isThrottleable;
}

void WebSWContextManagerConnection::didFailHeartBeatCheck(ServiceWorkerIdentifier serviceWorkerIdentifier)
{
    m_connectionToNetworkProcess->send(Messages::WebSWServerToContextConnection::DidFailHeartBeatCheck { serviceWorkerIdentifier }, 0);
}

void WebSWContextManagerConnection::setAsInspected(WebCore::ServiceWorkerIdentifier identifier, bool isInspected)
{
    m_connectionToNetworkProcess->send(Messages::WebSWServerToContextConnection::SetAsInspected { identifier, isInspected }, 0);
}

void WebSWContextManagerConnection::reportConsoleMessage(WebCore::ServiceWorkerIdentifier identifier, MessageSource source, MessageLevel level, const String& message, unsigned long requestIdentifier)
{
    m_connectionToNetworkProcess->send(Messages::WebSWServerToContextConnection::ReportConsoleMessage { identifier, source, level, message, requestIdentifier }, 0);
}

void WebSWContextManagerConnection::removeNavigationFetch(WebCore::SWServerConnectionIdentifier serverConnectionIdentifier, WebCore::FetchIdentifier fetchIdentifier)
{
    m_queue->dispatch([protectedThis = Ref { *this }, serverConnectionIdentifier, fetchIdentifier] {
        assertIsCurrent(protectedThis->m_queue.get());
        protectedThis->m_ongoingNavigationFetchTasks.remove({ serverConnectionIdentifier, fetchIdentifier });
    });
}

#if ENABLE(REMOTE_INSPECTOR) && PLATFORM(COCOA)
void WebSWContextManagerConnection::connectToInspector(WebCore::ServiceWorkerIdentifier serviceWorkerIdentifier, bool isAutomaticConnection, bool immediatelyPause)
{
    Ref channel = ServiceWorkerDebuggableFrontendChannel::create(serviceWorkerIdentifier);
    m_channels.add(serviceWorkerIdentifier, channel);
    if (RefPtr serviceWorkerThreadProxy = SWContextManager::singleton().serviceWorkerThreadProxy(serviceWorkerIdentifier))
        serviceWorkerThreadProxy->inspectorProxy().connectToWorker(channel, isAutomaticConnection, immediatelyPause);
}

void WebSWContextManagerConnection::disconnectFromInspector(WebCore::ServiceWorkerIdentifier serviceWorkerIdentifier)
{
    RefPtr channel = m_channels.take(serviceWorkerIdentifier);
    if (RefPtr serviceWorkerThreadProxy = SWContextManager::singleton().serviceWorkerThreadProxy(serviceWorkerIdentifier))
        serviceWorkerThreadProxy->inspectorProxy().disconnectFromWorker(*channel);
}

void WebSWContextManagerConnection::dispatchMessageFromInspector(WebCore::ServiceWorkerIdentifier identifier, String&& message)
{
    if (RefPtr serviceWorkerThreadProxy = SWContextManager::singleton().serviceWorkerThreadProxy(identifier))
        serviceWorkerThreadProxy->inspectorProxy().sendMessageToWorker(WTFMove(message));
}

#if ENABLE(REMOTE_INSPECTOR_SERVICE_WORKER_AUTO_INSPECTION)
void WebSWContextManagerConnection::unpauseServiceWorkerForRejectedAutomaticInspection(WebCore::ServiceWorkerIdentifier identifier)
{
    SWContextManager::singleton().stopRunningDebuggerTasksOnServiceWorker(identifier);
}
#endif
#endif // ENABLE(REMOTE_INSPECTOR) && PLATFORM(COCOA)

} // namespace WebCore
