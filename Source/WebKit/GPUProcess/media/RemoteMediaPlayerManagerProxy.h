/*
 * Copyright (C) 2019 Apple Inc. All rights reserved.
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

#if ENABLE(GPU_PROCESS) && ENABLE(VIDEO)

#include "Connection.h"
#include "GPUConnectionToWebProcess.h"
#include "MessageReceiver.h"
#include "SandboxExtension.h"
#include <WebCore/HTMLMediaElementIdentifier.h>
#include <WebCore/MediaPlayer.h>
#include <WebCore/MediaPlayerIdentifier.h>
#include <WebCore/ShareableBitmap.h>
#include <WebCore/VideoTarget.h>
#include <wtf/HashMap.h>
#include <wtf/Lock.h>
#include <wtf/Logger.h>
#include <wtf/TZoneMalloc.h>
#include <wtf/ThreadSafeWeakPtr.h>
#include <wtf/WeakPtr.h>

#if ENABLE(MEDIA_SOURCE)
#include "RemoteMediaSourceIdentifier.h"
#endif

namespace WebKit {

class RemoteMediaPlayerProxy;
struct RemoteMediaPlayerConfiguration;
struct RemoteMediaPlayerProxyConfiguration;
struct SharedPreferencesForWebProcess;
class RemoteMediaSourceProxy;
class VideoReceiverEndpointMessage;
class VideoReceiverSwapEndpointsMessage;

class RemoteMediaPlayerManagerProxy
    : public RefCounted<RemoteMediaPlayerManagerProxy>, public IPC::MessageReceiver
{
    WTF_MAKE_TZONE_ALLOCATED(RemoteMediaPlayerManagerProxy);
public:
    static Ref<RemoteMediaPlayerManagerProxy> create(GPUConnectionToWebProcess& gpuConnectionToWebProcess)
    {
        return adoptRef(*new RemoteMediaPlayerManagerProxy(gpuConnectionToWebProcess));
    }

    ~RemoteMediaPlayerManagerProxy();

    void ref() const final { RefCounted::ref(); }
    void deref() const final { RefCounted::deref(); }

    RefPtr<GPUConnectionToWebProcess> gpuConnectionToWebProcess() { return m_gpuConnectionToWebProcess.get(); }
    void clear();

#if !RELEASE_LOG_DISABLED
    Logger& logger() { return m_logger; }
#endif

    void didReceiveMessageFromWebProcess(IPC::Connection& connection, IPC::Decoder& decoder) { didReceiveMessage(connection, decoder); }
    void didReceiveSyncMessageFromWebProcess(IPC::Connection& connection, IPC::Decoder& decoder, UniqueRef<IPC::Encoder>& encoder) { return didReceiveSyncMessage(connection, decoder, encoder); }
    void didReceivePlayerMessage(IPC::Connection&, IPC::Decoder&);
    void didReceiveSyncPlayerMessage(IPC::Connection&, IPC::Decoder&, UniqueRef<IPC::Encoder>&);

    RefPtr<WebCore::MediaPlayer> mediaPlayer(std::optional<WebCore::MediaPlayerIdentifier>);

    std::optional<WebCore::ShareableBitmap::Handle> bitmapImageForCurrentTime(WebCore::MediaPlayerIdentifier);

#if ENABLE(MEDIA_SOURCE)
    RefPtr<RemoteMediaSourceProxy> pendingMediaSource(RemoteMediaSourceIdentifier);
    void registerMediaSource(RemoteMediaSourceIdentifier, RemoteMediaSourceProxy&);
    void invalidateMediaSource(RemoteMediaSourceIdentifier);
#endif

    std::optional<SharedPreferencesForWebProcess> sharedPreferencesForWebProcess() const;
private:
    explicit RemoteMediaPlayerManagerProxy(GPUConnectionToWebProcess&);

    // IPC::MessageReceiver
    void didReceiveMessage(IPC::Connection&, IPC::Decoder&) final;
    void didReceiveSyncMessage(IPC::Connection&, IPC::Decoder&, UniqueRef<IPC::Encoder>&) final;

    void createMediaPlayer(WebCore::MediaPlayerIdentifier, WebCore::MediaPlayerClientIdentifier, WebCore::MediaPlayerEnums::MediaEngineIdentifier, RemoteMediaPlayerProxyConfiguration&&);
    void deleteMediaPlayer(WebCore::MediaPlayerIdentifier);

    // Media player factory
    void getSupportedTypes(WebCore::MediaPlayerEnums::MediaEngineIdentifier, CompletionHandler<void(Vector<String>&&)>&&);
    void supportsTypeAndCodecs(WebCore::MediaPlayerEnums::MediaEngineIdentifier, const WebCore::MediaEngineSupportParameters&&, CompletionHandler<void(WebCore::MediaPlayer::SupportsType)>&&);
    void supportsKeySystem(WebCore::MediaPlayerEnums::MediaEngineIdentifier, const String&&, const String&&, CompletionHandler<void(bool)>&&);

#if !RELEASE_LOG_DISABLED
    ASCIILiteral logClassName() const { return "RemoteMediaPlayerManagerProxy"; }
    WTFLogChannel& logChannel() const;
    uint64_t logIdentifier() const { return m_logIdentifier; }
#endif

    HashMap<WebCore::MediaPlayerIdentifier, Ref<RemoteMediaPlayerProxy>> m_proxies;
    ThreadSafeWeakPtr<GPUConnectionToWebProcess> m_gpuConnectionToWebProcess;

#if ENABLE(MEDIA_SOURCE)
    HashMap<RemoteMediaSourceIdentifier, RefPtr<RemoteMediaSourceProxy>> m_pendingMediaSources;
#endif

#if !RELEASE_LOG_DISABLED
    uint64_t m_logIdentifier { 0 };
    Ref<Logger> m_logger;
#endif
};

} // namespace WebKit

#endif // ENABLE(GPU_PROCESS) && ENABLE(VIDEO)
