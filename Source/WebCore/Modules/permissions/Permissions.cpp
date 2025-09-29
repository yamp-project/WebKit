/*
 * Copyright (C) 2021 Apple Inc. All rights reserved.
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
#include "Permissions.h"

#include "ContextDestructionObserverInlines.h"
#include "DedicatedWorkerGlobalScope.h"
#include "DocumentInlines.h"
#include "Exception.h"
#include "Geolocation.h"
#include "JSDOMPromiseDeferred.h"
#include "JSPermissionDescriptor.h"
#include "JSPermissionStatus.h"
#include "LocalFrame.h"
#include "Navigator.h"
#include "NavigatorBase.h"
#include "NavigatorGeolocation.h"
#include "Page.h"
#include "PermissionController.h"
#include "PermissionDescriptor.h"
#include "PermissionName.h"
#include "PermissionQuerySource.h"
#include "PermissionsPolicy.h"
#include "Quirks.h"
#include "ScriptExecutionContext.h"
#include "SecurityOrigin.h"
#include "ServiceWorkerGlobalScope.h"
#include "SharedWorkerGlobalScope.h"
#include "WorkerGlobalScope.h"
#include "WorkerLoaderProxy.h"
#include "WorkerThread.h"
#include <optional>
#include <wtf/TZoneMallocInlines.h>
#include <wtf/TypeCasts.h>
#include <wtf/text/WTFString.h>

namespace WebCore {

WTF_MAKE_TZONE_OR_ISO_ALLOCATED_IMPL(Permissions);

Ref<Permissions> Permissions::create(NavigatorBase& navigator)
{
    return adoptRef(*new Permissions(navigator));
}

Permissions::Permissions(NavigatorBase& navigator)
    : m_navigator(navigator)
{
}

NavigatorBase* Permissions::navigator()
{
    return m_navigator.get();
}

Permissions::~Permissions() = default;

static bool isAllowedByPermissionsPolicy(const Document& document, PermissionName name)
{
    switch (name) {
    case PermissionName::Camera:
        return PermissionsPolicy::isFeatureEnabled(PermissionsPolicy::Feature::Camera, document, PermissionsPolicy::ShouldReportViolation::No);
    case PermissionName::Geolocation:
        return PermissionsPolicy::isFeatureEnabled(PermissionsPolicy::Feature::Geolocation, document, PermissionsPolicy::ShouldReportViolation::No);
    case PermissionName::Microphone:
        return PermissionsPolicy::isFeatureEnabled(PermissionsPolicy::Feature::Microphone, document, PermissionsPolicy::ShouldReportViolation::No);
    case PermissionName::StorageAccess:
        return PermissionsPolicy::isFeatureEnabled(PermissionsPolicy::Feature::StorageAccess, document, PermissionsPolicy::ShouldReportViolation::No);
    default:
        return true;
    }
}

std::optional<PermissionQuerySource> Permissions::sourceFromContext(const ScriptExecutionContext& context)
{
    if (is<Document>(context))
        return PermissionQuerySource::Window;
    if (is<DedicatedWorkerGlobalScope>(context))
        return PermissionQuerySource::DedicatedWorker;
    if (is<SharedWorkerGlobalScope>(context))
        return PermissionQuerySource::SharedWorker;
    if (is<ServiceWorkerGlobalScope>(context))
        return PermissionQuerySource::ServiceWorker;
    return std::nullopt;
}


std::optional<PermissionName> Permissions::toPermissionName(const String& name)
{
    if (name == "camera"_s)
        return PermissionName::Camera;
    if (name == "geolocation"_s)
        return PermissionName::Geolocation;
    if (name == "microphone"_s)
        return PermissionName::Microphone;
    if (name == "notifications"_s)
        return PermissionName::Notifications;
    if (name == "push"_s)
        return PermissionName::Push;
    if (name == "storage-access"_s)
        return PermissionName::StorageAccess;
    return std::nullopt;
}

void Permissions::query(JSC::Strong<JSC::JSObject> permissionDescriptorValue, DOMPromiseDeferred<IDLInterface<PermissionStatus>>&& promise)
{
    RefPtr context = m_navigator ? m_navigator->scriptExecutionContext() : nullptr;
    if (!context || !context->globalObject()) {
        promise.reject(Exception { ExceptionCode::InvalidStateError, "The context is invalid"_s });
        return;
    }

    auto source = sourceFromContext(*context);
    if (!source) {
        promise.reject(Exception { ExceptionCode::NotSupportedError, "Permissions::query is not supported in this context"_s  });
        return;
    }

    RefPtr document = dynamicDowncast<Document>(*context);
    if (document && !document->isFullyActive()) {
        promise.reject(Exception { ExceptionCode::InvalidStateError, "The document is not fully active"_s });
        return; 
    }

    auto& vm = context->globalObject()->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    auto permissionDescriptorConversionResult = convert<IDLDictionary<PermissionDescriptor>>(*context->globalObject(), permissionDescriptorValue.get());
    if (permissionDescriptorConversionResult.hasException(scope)) [[unlikely]] {
        promise.reject(Exception { ExceptionCode::ExistingExceptionError });
        return;
    }

    auto permissionDescriptor = permissionDescriptorConversionResult.releaseReturnValue();

    RefPtr origin = context->securityOrigin();
    auto originData = origin ? origin->data() : SecurityOriginData { };

    if (document) {
        WeakPtr page = document->page();
        if (!page) {
            promise.reject(Exception { ExceptionCode::InvalidStateError, "The page does not exist"_s });
            return;
        }

        if (!isAllowedByPermissionsPolicy(*document, permissionDescriptor.name)) {
            promise.resolve(PermissionStatus::create(*context, PermissionState::Denied, permissionDescriptor, PermissionQuerySource::Window, *page));
            return;
        }

        PermissionController::singleton().query(ClientOrigin { document->topOrigin().data(), WTFMove(originData) }, permissionDescriptor, *page, *source, [document = Ref { *document }, page, permissionDescriptor, promise = WTFMove(promise)](auto permissionState) mutable {
            if (!permissionState) {
                promise.reject(Exception { ExceptionCode::NotSupportedError, "Permissions::query does not support this API"_s });
                return;
            }

#if ENABLE(GEOLOCATION)
            if (permissionDescriptor.name == PermissionName::Geolocation) {
                if (auto geolocationPermissionState = determineGeolocationPermissionState(*permissionState, document))
                    permissionState = geolocationPermissionState;
                else {
                    promise.reject(Exception { ExceptionCode::InvalidStateError, "The Document does not have a Geolocation object"_s });
                    return;
                }
            }
#endif

#if ENABLE(MEDIA_STREAM)
            if (document->quirks().shouldEnableCameraAndMicrophonePermissionStateQuirk() && (permissionDescriptor.name == PermissionName::Camera || permissionDescriptor.name == PermissionName::Microphone) && *permissionState == PermissionState::Prompt)
                permissionState = PermissionState::Granted;
#endif
            promise.resolve(PermissionStatus::create(document, *permissionState, permissionDescriptor, PermissionQuerySource::Window, WTFMove(page)));
        });
        return;
    }

    Ref workerGlobalScope = downcast<WorkerGlobalScope>(*context);
    auto completionHandler = [originData = WTFMove(originData).isolatedCopy(), permissionDescriptor, contextIdentifier = workerGlobalScope->identifier(), source = *source, promise = WTFMove(promise)] (auto& context) mutable {
        ASSERT(isMainThread());

        Ref document = downcast<Document>(context);
        if (!document->page()) {
            ScriptExecutionContext::postTaskTo(contextIdentifier, [promise = WTFMove(promise)](auto&) mutable {
                promise.reject(Exception { ExceptionCode::InvalidStateError, "The page does not exist"_s });
            });
            return;
        }

        auto page = source == PermissionQuerySource::DedicatedWorker ? WeakPtr { *document->page() } : nullptr;

        PermissionController::singleton().query(ClientOrigin { document->topOrigin().data(), WTFMove(originData) }, permissionDescriptor, page, source, [contextIdentifier, permissionDescriptor, promise = WTFMove(promise), source, page, document](auto permissionState) mutable {
            ASSERT(isMainThread());

            if (!permissionState) {
                ScriptExecutionContext::postTaskTo(contextIdentifier, [promise = WTFMove(promise)](auto&) mutable {
                    promise.reject(Exception { ExceptionCode::NotSupportedError, "Permissions::query does not support this API"_s });
                });

                return;
            }

#if ENABLE(GEOLOCATION)
            if (permissionDescriptor.name == PermissionName::Geolocation) {
                if (auto geolocationPermissionState = determineGeolocationPermissionState(*permissionState, document))
                    permissionState = geolocationPermissionState;
                else {
                    ScriptExecutionContext::postTaskTo(contextIdentifier, [promise = WTFMove(promise)](auto&) mutable {
                        promise.reject(Exception { ExceptionCode::InvalidStateError, "The Document does not have a Geolocation object"_s });
                    });

                    return;
                }
            }
#endif

            ScriptExecutionContext::postTaskTo(contextIdentifier, [promise = WTFMove(promise), permissionState, permissionDescriptor, source, page = WTFMove(page)](auto& context) mutable {
                promise.resolve(PermissionStatus::create(context, *permissionState, permissionDescriptor, source, WTFMove(page)));
            });
        });
    };

    if (CheckedPtr workerLoaderProxy = workerGlobalScope->thread().workerLoaderProxy())
        workerLoaderProxy->postTaskToLoader(WTFMove(completionHandler));
}

#if ENABLE(GEOLOCATION)

static std::optional<PermissionState> determineGeolocationPermissionState(PermissionState permissionState, const Document& document)
{
    RefPtr window = document.window();
    if (!window)
        return std::nullopt;

    RefPtr geolocation = NavigatorGeolocation::optionalGeolocation(window->protectedNavigator());

    switch (permissionState) {
    case PermissionState::Granted:
        return PermissionState::Granted;
    case PermissionState::Denied:
        if (!geolocation || !geolocation->hasBeenRequested())
            return PermissionState::Prompt;
        return PermissionState::Denied;
    case PermissionState::Prompt:
        if (!geolocation || !geolocation->hasBeenRequested())
            return PermissionState::Prompt;
        return geolocation->isAllowed() ? PermissionState::Granted : PermissionState::Denied;
    };

    return std::nullopt;
}

#endif // ENABLE(GEOLOCATION)

} // namespace WebCore
