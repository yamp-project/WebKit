/*
 * Copyright (C) 2013-2025 Apple Inc. All rights reserved.
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

#pragma once

#if ENABLE(LEGACY_ENCRYPTED_MEDIA)

#include "ActiveDOMObject.h"
#include "EventTarget.h"
#include "EventTargetInterfaces.h"
#include "LegacyCDMSession.h"
#include "Timer.h"
#include "WebKitMediaKeys.h"
#include <JavaScriptCore/Forward.h>
#include <wtf/Deque.h>

namespace WebCore {

class WebKitMediaKeyError;
template<typename> class ExceptionOr;

class WebKitMediaKeySession final : public RefCounted<WebKitMediaKeySession>, public EventTarget, public ActiveDOMObject, private LegacyCDMSessionClient {
    WTF_MAKE_TZONE_OR_ISO_ALLOCATED(WebKitMediaKeySession);
public:
    USING_CAN_MAKE_WEAKPTR(EventTarget);

    void ref() const final { RefCounted::ref(); }
    void deref() const final { RefCounted::deref(); }

    static Ref<WebKitMediaKeySession> create(Document&, WebKitMediaKeys&, const String& keySystem);
    ~WebKitMediaKeySession();

    WebKitMediaKeyError* error() { return m_error.get(); }
    const String& keySystem() const { return m_keySystem; }
    const String& sessionId() const { return m_sessionId; }
    ExceptionOr<void> update(Ref<Uint8Array>&& key);
    void close();

    LegacyCDMSession* session() { return m_session.get(); }

    void detachKeys() { m_keys = nullptr; }

    void generateKeyRequest(const String& mimeType, Ref<Uint8Array>&& initData, const String& mediaKeysHashSalt);
    RefPtr<ArrayBuffer> cachedKeyForKeyId(const String& keyId) const;

private:
    WebKitMediaKeySession(Document&, WebKitMediaKeys&, const String& keySystem);
    void keyRequestTimerFired();
    void addKeyTimerFired();

    void sendMessage(Uint8Array*, String destinationURL) final;
    void sendError(MediaKeyErrorCode, uint32_t systemCode) final;
    String mediaKeysStorageDirectory() const final;
    String mediaKeysHashSalt() const final { return m_mediaKeysHashSalt; }

    void refEventTarget() final { ref(); }
    void derefEventTarget() final { deref(); }

    // ActiveDOMObject.
    void stop() final;
    bool virtualHasPendingActivity() const final;

    enum EventTargetInterfaceType eventTargetInterface() const final { return EventTargetInterfaceType::WebKitMediaKeySession; }
    ScriptExecutionContext* scriptExecutionContext() const final;

#if !RELEASE_LOG_DISABLED
    const Logger& logger() const final { return m_logger; }
    uint64_t logIdentifier() const final { return m_logIdentifier; }
    ASCIILiteral logClassName() const { return "WebKitMediaKeySession"_s; }
    WTFLogChannel& logChannel() const;

    const Ref<const Logger> m_logger;
    const uint64_t m_logIdentifier;
#endif

    CheckedPtr<WebKitMediaKeys> m_keys;
    String m_keySystem;
    String m_sessionId;
    String m_mediaKeysHashSalt;
    RefPtr<WebKitMediaKeyError> m_error;
    RefPtr<LegacyCDMSession> m_session;

    struct PendingKeyRequest {
        String mimeType;
        Ref<Uint8Array> initData;
    };
    Deque<PendingKeyRequest> m_pendingKeyRequests;
    Timer m_keyRequestTimer;

    Deque<Ref<Uint8Array>> m_pendingKeys;
    Timer m_addKeyTimer;
};

}

#endif // ENABLE(LEGACY_ENCRYPTED_MEDIA)
