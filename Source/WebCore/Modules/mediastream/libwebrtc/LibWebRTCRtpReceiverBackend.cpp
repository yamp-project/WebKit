/*
 * Copyright (C) 2018 Apple Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1.  Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 * 2.  Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON
 * ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 * SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "LibWebRTCRtpReceiverBackend.h"

#if ENABLE(WEB_RTC) && USE(LIBWEBRTC)

#include "Document.h"
#include "DocumentInlines.h"
#include "LibWebRTCAudioModule.h"
#include "LibWebRTCDtlsTransportBackend.h"
#include "LibWebRTCProvider.h"
#include "LibWebRTCRtpReceiverTransformBackend.h"
#include "LibWebRTCUtils.h"
#include "Page.h"
#include "RTCRtpTransformBackend.h"
#include "RealtimeIncomingAudioSource.h"
#include "RealtimeIncomingVideoSource.h"
#include "Settings.h"
#include <wtf/TZoneMallocInlines.h>

namespace WebCore {

WTF_MAKE_TZONE_ALLOCATED_IMPL(LibWebRTCRtpReceiverBackend);

LibWebRTCRtpReceiverBackend::LibWebRTCRtpReceiverBackend(Ref<webrtc::RtpReceiverInterface>&& rtcReceiver)
    : m_rtcReceiver(WTFMove(rtcReceiver))
{
}

LibWebRTCRtpReceiverBackend::~LibWebRTCRtpReceiverBackend() = default;

RTCRtpParameters LibWebRTCRtpReceiverBackend::getParameters()
{
    return toRTCRtpParameters(m_rtcReceiver->GetParameters());
}

static inline void fillRTCRtpContributingSource(RTCRtpContributingSource& source, const webrtc::RtpSource& rtcSource)
{
    source.timestamp = rtcSource.timestamp().ms();
    source.rtpTimestamp = rtcSource.rtp_timestamp();
    source.source = rtcSource.source_id();
    if (rtcSource.audio_level())
        source.audioLevel = (*rtcSource.audio_level() == 127) ? 0 : pow(10, -*rtcSource.audio_level() / 20);
}

static inline RTCRtpContributingSource toRTCRtpContributingSource(const webrtc::RtpSource& rtcSource)
{
    RTCRtpContributingSource source;
    fillRTCRtpContributingSource(source, rtcSource);
    return source;
}

static inline RTCRtpSynchronizationSource toRTCRtpSynchronizationSource(const webrtc::RtpSource& rtcSource)
{
    RTCRtpSynchronizationSource source;
    fillRTCRtpContributingSource(source, rtcSource);
    return source;
}

Vector<RTCRtpContributingSource> LibWebRTCRtpReceiverBackend::getContributingSources() const
{
    Vector<RTCRtpContributingSource> sources;
    for (auto& rtcSource : m_rtcReceiver->GetSources()) {
        if (rtcSource.source_type() == webrtc::RtpSourceType::CSRC)
            sources.append(toRTCRtpContributingSource(rtcSource));
    }
    return sources;
}

Vector<RTCRtpSynchronizationSource> LibWebRTCRtpReceiverBackend::getSynchronizationSources() const
{
    Vector<RTCRtpSynchronizationSource> sources;
    for (auto& rtcSource : m_rtcReceiver->GetSources()) {
        if (rtcSource.source_type() == webrtc::RtpSourceType::SSRC)
            sources.append(toRTCRtpSynchronizationSource(rtcSource));
    }
    return sources;
}

Ref<RealtimeMediaSource> LibWebRTCRtpReceiverBackend::createSource(Document& document)
{
    auto rtcTrack = m_rtcReceiver->track();
    switch (m_rtcReceiver->media_type()) {
    case webrtc::MediaType::ANY:
    case webrtc::MediaType::DATA:
    case webrtc::MediaType::UNSUPPORTED:
        break;
    case webrtc::MediaType::AUDIO: {
        webrtc::scoped_refptr<webrtc::AudioTrackInterface> audioTrack { static_cast<webrtc::AudioTrackInterface*>(rtcTrack.get()) };
        Ref source = RealtimeIncomingAudioSource::create(toRef(WTFMove(audioTrack)), fromStdString(rtcTrack->id()));
        if (document.page()) {
            auto& webRTCProvider = reinterpret_cast<LibWebRTCProvider&>(document.page()->webRTCProvider());
            source->setAudioModule(webRTCProvider.audioModule());
        }
        return source;
    }
    case webrtc::MediaType::VIDEO: {
        webrtc::scoped_refptr<webrtc::VideoTrackInterface> videoTrack { static_cast<webrtc::VideoTrackInterface*>(rtcTrack.get()) };
        Ref source = RealtimeIncomingVideoSource::create(toRef(WTFMove(videoTrack)), fromStdString(rtcTrack->id()));
        if (document.settings().webRTCMediaPipelineAdditionalLoggingEnabled())
            source->enableFrameRatedMonitoring();
        return source;
    }
    }
    RELEASE_ASSERT_NOT_REACHED();
}

Ref<RTCRtpTransformBackend> LibWebRTCRtpReceiverBackend::rtcRtpTransformBackend()
{
    if (!m_transformBackend)
        lazyInitialize(m_transformBackend, LibWebRTCRtpReceiverTransformBackend::create(m_rtcReceiver.get()));
    return *m_transformBackend;
}

std::unique_ptr<RTCDtlsTransportBackend> LibWebRTCRtpReceiverBackend::dtlsTransportBackend()
{
    RefPtr backend = toRefPtr(m_rtcReceiver->dtls_transport());
    return backend ? makeUnique<LibWebRTCDtlsTransportBackend>(backend.releaseNonNull()) : nullptr;
}

} // namespace WebCore

#endif // ENABLE(WEB_RTC) && USE(LIBWEBRTC)
