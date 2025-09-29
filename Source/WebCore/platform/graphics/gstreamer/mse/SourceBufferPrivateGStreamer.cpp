/*
 * Copyright (C) 2013 Google Inc. All rights reserved.
 * Copyright (C) 2013 Orange
 * Copyright (C) 2014 Sebastian Dröge <sebastian@centricular.com>
 * Copyright (C) 2015, 2016 Metrological Group B.V.
 * Copyright (C) 2015, 2016 Igalia, S.L
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "SourceBufferPrivateGStreamer.h"

#if ENABLE(MEDIA_SOURCE) && USE(GSTREAMER)

#include "AppendPipeline.h"
#include "AudioTrackPrivateGStreamer.h"
#include "ContentType.h"
#include "GStreamerCommon.h"
#include "InbandTextTrackPrivate.h"
#include "InbandTextTrackPrivateGStreamer.h"
#include "MediaPlayerPrivateGStreamerMSE.h"
#include "MediaSample.h"
#include "MediaSourcePrivateGStreamer.h"
#include "MediaSourceTrackGStreamer.h"
#include "NotImplemented.h"
#include "VideoTrackPrivateGStreamer.h"
#include "WebKitMediaSourceGStreamer.h"
#include <wtf/NativePromise.h>
#include <wtf/text/StringToIntegerConversion.h>

GST_DEBUG_CATEGORY_STATIC(webkit_mse_sourcebuffer_debug);
#define GST_CAT_DEFAULT webkit_mse_sourcebuffer_debug

namespace WebCore {

bool SourceBufferPrivateGStreamer::isContentTypeSupported(const ContentType& type)
{
    const auto& containerType = type.containerType();
    return containerType == "audio/mpeg"_s || containerType.endsWith("mp4"_s) || containerType.endsWith("aac"_s) || containerType.endsWith("webm"_s);
}

Ref<SourceBufferPrivateGStreamer> SourceBufferPrivateGStreamer::create(MediaSourcePrivateGStreamer& mediaSource, const ContentType& contentType)
{
    return adoptRef(*new SourceBufferPrivateGStreamer(mediaSource, contentType));
}

SourceBufferPrivateGStreamer::SourceBufferPrivateGStreamer(MediaSourcePrivateGStreamer& mediaSource, const ContentType& contentType)
    : SourceBufferPrivate(mediaSource)
    , m_type(contentType)
    , m_appendPipeline(makeUnique<AppendPipeline>(*this, *player()))
#if !RELEASE_LOG_DISABLED
    , m_logger(mediaSource.logger())
    , m_logIdentifier(mediaSource.nextSourceBufferLogIdentifier())
#endif
{
    static std::once_flag debugRegisteredFlag;
    std::call_once(debugRegisteredFlag, [] {
        GST_DEBUG_CATEGORY_INIT(webkit_mse_sourcebuffer_debug, "webkitmsesourcebuffer", 0, "WebKit MSE SourceBuffer");
    });
}

SourceBufferPrivateGStreamer::~SourceBufferPrivateGStreamer()
{
    if (!m_appendPromise)
        return;

    m_appendPromise->reject(PlatformMediaError::BufferRemoved);
    m_appendPromise.reset();
}

Ref<MediaPromise> SourceBufferPrivateGStreamer::appendInternal(Ref<SharedBuffer>&& data)
{
    ASSERT(isMainThread());

    if (RefPtr player = this->player())
        GST_DEBUG_OBJECT(player->pipeline(), "Appending %zu bytes", data->size());

    ASSERT(!m_appendPromise);
    m_appendPromise.emplace();
    gpointer bufferData = const_cast<uint8_t*>(data->span().data());
    auto bufferLength = data->size();
    GRefPtr<GstBuffer> buffer = adoptGRef(gst_buffer_new_wrapped_full(static_cast<GstMemoryFlags>(0), bufferData, bufferLength, 0, bufferLength, &data.leakRef(),
        [](gpointer data)
        {
            static_cast<SharedBuffer*>(data)->deref();
        }));

    m_appendPipeline->pushNewBuffer(WTFMove(buffer));
    return *m_appendPromise;
}

void SourceBufferPrivateGStreamer::resetParserStateInternal()
{
    ASSERT(isMainThread());
    if (!m_appendPipeline)
        return;

    if (RefPtr player = this->player())
        GST_DEBUG_OBJECT(player->pipeline(), "resetting parser state");
    m_appendPipeline->resetParserState();
}

void SourceBufferPrivateGStreamer::removedFromMediaSource()
{
    ASSERT(isMainThread());

    for (auto& [_, track] : tracks())
        track->remove();
    m_hasBeenRemovedFromMediaSource = true;

    m_appendPipeline->stopParser();

    // Release the resources used by the AppendPipeline. This effectively makes the
    // SourceBufferPrivate useless. Ideally the entire instance should be destroyed. For now we
    // explicitely release the AppendPipeline because that's the biggest resource user. In case the
    // process remains alive, GC might kick in later on and release the SourceBufferPrivate.
    m_appendPipeline = nullptr;

    SourceBufferPrivate::removedFromMediaSource();
}

void SourceBufferPrivateGStreamer::flush(TrackID trackId)
{
    ASSERT(isMainThread());

    // This is only for on-the-fly reenqueues after appends. When seeking, the seek will do its own flush.

    RefPtr mediaSource = m_mediaSource.get();
    if (!mediaSource)
        return;

    RefPtr player = this->player();

    ASSERT(m_tracks.contains(trackId));
    auto track = m_tracks[trackId];
    if (!downcast<MediaSourcePrivateGStreamer>(mediaSource)->hasAllTracks()) {
        if (player)
            GST_DEBUG_OBJECT(player->pipeline(), "Source element has not emitted tracks yet, so we only need to clear the queue. trackId = '%" PRIu64 "'", track->id());
        track->clearQueue();
        return;
    }

    if (track->type() == TrackPrivateBaseGStreamer::Text) {
        if (player)
            GST_DEBUG_OBJECT(player->pipeline(), "Track is a text stream, so we only need to clear the queue. trackId = '%" PRIu64 "'", track->id());
        track->clearQueue();
        return;
    }

    if (!player)
        return;
    GST_DEBUG_OBJECT(player->pipeline(), "Source element has emitted tracks, let it handle the flush, which may cause a pipeline flush as well. trackId = '%" PRIu64 "'", track->id());
    webKitMediaSrcFlush(player->webKitMediaSrc(), track->id());
}

void SourceBufferPrivateGStreamer::enqueueSample(Ref<MediaSample>&& sample, TrackID trackId)
{
    ASSERT(isMainThread());

    GRefPtr<GstSample> gstSample = sample->platformSample().gstSample();
    ASSERT(gstSample);

#ifndef GST_DISABLE_GST_DEBUG
    RefPtr player = this->player();
    if (player) {
        const auto& size = sample->presentationSize();
        GST_TRACE_OBJECT(player->pipeline(), "enqueing sample trackId=%" PRIu64 " presentationSize=%.0fx%.0f at PTS %" GST_TIME_FORMAT " duration: %" GST_TIME_FORMAT,
            trackId, size.width(), size.height(),
            GST_TIME_ARGS(toGstClockTime(sample->presentationTime())),
            GST_TIME_ARGS(toGstClockTime(sample->duration())));
    }
#endif
    ASSERT(m_tracks.contains(trackId));
    auto track = m_tracks[trackId];

#ifndef GST_DISABLE_GST_DEBUG
    if (player && track->type() == TrackPrivateBaseGStreamer::Text) {
        GstMappedBuffer mappedBuffer(gst_sample_get_buffer(gstSample.get()), GST_MAP_READ);

        if (mappedBuffer) [[likely]] {
            auto message = makeString("Text sample (trackId="_s, trackId, ')');
            GST_MEMDUMP_OBJECT(player->pipeline(), message.utf8().data(), mappedBuffer.data(), mappedBuffer.size());
        }
    }
#endif
    track->enqueueObject(adoptGRef(GST_MINI_OBJECT(gstSample.leakRef())));
}

bool SourceBufferPrivateGStreamer::isReadyForMoreSamples(TrackID trackId)
{
    ASSERT(isMainThread());
    ASSERT(m_tracks.contains(trackId));
    auto track = m_tracks[trackId];
    bool ret = track->isReadyForMoreSamples();
    if (RefPtr player = this->player())
        GST_TRACE_OBJECT(player->pipeline(), "track %" PRIu64 "isReadyForMoreSamples: %s", trackId, boolForPrinting(ret));
    return ret;
}

void SourceBufferPrivateGStreamer::notifyClientWhenReadyForMoreSamples(TrackID trackId)
{
    ASSERT(isMainThread());
    ASSERT(m_tracks.contains(trackId));
    auto track = m_tracks[trackId];
    track->notifyWhenReadyForMoreSamples([weakPtr = WeakPtr { *this }, this, trackId]() mutable {
        RunLoop::mainSingleton().dispatch([weakPtr = WTFMove(weakPtr), this, trackId]() {
            if (!weakPtr)
                return;
            if (!m_hasBeenRemovedFromMediaSource)
                provideMediaData(trackId);
        });
    });
}

void SourceBufferPrivateGStreamer::allSamplesInTrackEnqueued(TrackID trackId)
{
    ASSERT(isMainThread());
    ASSERT(m_tracks.contains(trackId));
    auto track = m_tracks[trackId];
    if (RefPtr player = this->player())
        GST_DEBUG_OBJECT(player->pipeline(), "Enqueueing EOS for track '%" PRIu64 "'", track->id());
    track->enqueueObject(adoptGRef(GST_MINI_OBJECT(gst_event_new_eos())));
}

bool SourceBufferPrivateGStreamer::precheckInitializationSegment(const InitializationSegment& segment)
{
    for (auto& trackInfo : segment.videoTracks) {
        auto* videoTrackInfo = static_cast<VideoTrackPrivateGStreamer*>(trackInfo.track.get());
        GRefPtr<GstCaps> initialCaps = videoTrackInfo->initialCaps();
        ASSERT(initialCaps);
        if (!m_tracks.contains(videoTrackInfo->id()))
            m_tracks.try_emplace(videoTrackInfo->id(), MediaSourceTrackGStreamer::create(TrackPrivateBaseGStreamer::TrackType::Video, videoTrackInfo->id(), WTFMove(initialCaps)));
    }
    for (auto& trackInfo : segment.audioTracks) {
        auto* audioTrackInfo = static_cast<AudioTrackPrivateGStreamer*>(trackInfo.track.get());
        GRefPtr<GstCaps> initialCaps = audioTrackInfo->initialCaps();
        ASSERT(initialCaps);
        if (!m_tracks.contains(audioTrackInfo->id()))
            m_tracks.try_emplace(audioTrackInfo->id(), MediaSourceTrackGStreamer::create(TrackPrivateBaseGStreamer::TrackType::Audio, audioTrackInfo->id(), WTFMove(initialCaps)));
    }
    for (auto& trackInfo : segment.textTracks) {
        auto* textTrackInfo = static_cast<InbandTextTrackPrivateGStreamer*>(trackInfo.track.get());
        GRefPtr<GstCaps> initialCaps = textTrackInfo->initialCaps();
        ASSERT(initialCaps);
        if (!m_tracks.contains(textTrackInfo->id()))
            m_tracks.try_emplace(textTrackInfo->id(), MediaSourceTrackGStreamer::create(TrackPrivateBaseGStreamer::TrackType::Text, textTrackInfo->id(), WTFMove(initialCaps)));
    }

    return true;
}

void SourceBufferPrivateGStreamer::processInitializationSegment(std::optional<InitializationSegment>&& segment)
{
    if (RefPtr mediaSource = m_mediaSource.get(); mediaSource && segment)
        downcast<MediaSourcePrivateGStreamer>(mediaSource)->startPlaybackIfHasAllTracks();
}

void SourceBufferPrivateGStreamer::didReceiveAllPendingSamples()
{
    // TODO: didReceiveAllPendingSamples is called even when an error occurred.
    if (m_appendPromise) {
        m_appendPromise->resolve();
        m_appendPromise.reset();
    }
}

void SourceBufferPrivateGStreamer::appendParsingFailed()
{
    if (m_appendPromise) {
        m_appendPromise->reject(PlatformMediaError::ParsingError);
        m_appendPromise.reset();
    }
}

#if !RELEASE_LOG_DISABLED
WTFLogChannel& SourceBufferPrivateGStreamer::logChannel() const
{
    return LogMediaSource;
}
#endif

RegisteredTrack SourceBufferPrivateGStreamer::registerTrack(TrackID preferredId, StreamType streamType)
{
    ASSERT(isMainThread());

    RefPtr mediaSource = m_mediaSource.get();
    ASSERT(mediaSource);

    return downcast<MediaSourcePrivateGStreamer>(mediaSource)->registerTrack(preferredId, streamType);
}

void SourceBufferPrivateGStreamer::unregisterTrack(TrackID trackId)
{
    ASSERT(isMainThread());

    RefPtr mediaSource = m_mediaSource.get();
    ASSERT(mediaSource);

    downcast<MediaSourcePrivateGStreamer>(mediaSource)->unregisterTrack(trackId);
}

size_t SourceBufferPrivateGStreamer::platformMaximumBufferSize() const
{
#if PLATFORM(WPE)
    static size_t maxBufferSizeVideo = 0;
    static size_t maxBufferSizeAudio = 0;
    static size_t maxBufferSizeText = 0;

    static std::once_flag once;
    std::call_once(once, []() {
        // Syntax: Case insensitive, full type (audio, video, text), compact type (a, v, t),
        //         wildcard (*), unit multipliers (M=Mb, K=Kb, <empty>=bytes).
        // Examples: MSE_MAX_BUFFER_SIZE='V:50M,audio:12k,TeXT:500K'
        //           MSE_MAX_BUFFER_SIZE='*:100M'
        //           MSE_MAX_BUFFER_SIZE='video:90M,T:100000'

        auto s = String::fromLatin1(std::getenv("MSE_MAX_BUFFER_SIZE"));
        if (!s.isEmpty()) {
            Vector<String> entries = s.split(',');
            for (const String& entry : entries) {
                Vector<String> keyvalue = entry.split(':');
                if (keyvalue.size() != 2)
                    continue;
                auto key = keyvalue[0].trim(deprecatedIsSpaceOrNewline).convertToLowercaseWithoutLocale();
                auto value = keyvalue[1].trim(deprecatedIsSpaceOrNewline).convertToLowercaseWithoutLocale();
                size_t units = 1;
                if (value.endsWith('k'))
                    units = 1024;
                else if (value.endsWith('m'))
                    units = 1024 * 1024;
                if (units != 1)
                    value = value.left(value.length()-1);
                auto parsedSize = parseInteger<size_t>(value);
                if (!parsedSize)
                    continue;
                size_t size = *parsedSize;

                if (key == "a"_s || key == "audio"_s || key == "*"_s)
                    maxBufferSizeAudio = size * units;
                if (key == "v"_s || key == "video"_s || key == "*"_s)
                    maxBufferSizeVideo = size * units;
                if (key == "t"_s || key == "text"_s || key == "*"_s)
                    maxBufferSizeText = size * units;
            }
        }
    });

    // If any track type size isn't specified, we consider that it has no limit and the values from the
    // element have to be used. Otherwise, the track limits are accumulative. If everything is specified
    // but there's no track (eg: because we're processing an init segment that we don't know yet which
    // kind of track(s) is going to generate) we assume that the 3 kind of tracks might appear (audio,
    // video, text) and use all the accumulated limits at once to make room for any possible outcome.
    do {
        bool hasVideo = false;
        bool hasAudio = false;
        bool hasText = false;
        size_t bufferSize = 0;

        for (auto& [_, track] : m_tracks) {
            switch (track->type()) {
            case TrackPrivateBaseGStreamer::Video:
                hasVideo = true;
                break;
            case TrackPrivateBaseGStreamer::Audio:
                hasAudio = true;
                break;
            case TrackPrivateBaseGStreamer::Text:
                hasText = true;
                break;
            default:
                break;
            }
        }

        if (hasVideo || m_tracks.empty()) {
            if (maxBufferSizeVideo)
                bufferSize += maxBufferSizeVideo;
            else
                break;
        }
        if (hasAudio || m_tracks.empty()) {
            if (maxBufferSizeAudio)
                bufferSize += maxBufferSizeAudio;
            else
                break;
        }
        if (hasText || m_tracks.empty()) {
            if (maxBufferSizeText)
                bufferSize += maxBufferSizeText;
            else
                break;
        }
        if (bufferSize)
            return bufferSize;
    } while (false);
#endif

    return 0;
}

size_t SourceBufferPrivateGStreamer::platformEvictionThreshold() const
{
    static size_t evictionThreshold = 0;
    static std::once_flag once;
    std::call_once(once, []() {
        auto stringView = StringView::fromLatin1(std::getenv("MSE_BUFFER_SAMPLES_EVICTION_THRESHOLD"));
        if (!stringView.isEmpty())
            evictionThreshold = parseInteger<size_t>(stringView, 10).value_or(0);
    });
    return evictionThreshold;
}

RefPtr<MediaPlayerPrivateGStreamerMSE> SourceBufferPrivateGStreamer::player() const
{
    if (RefPtr mediaSource = m_mediaSource.get())
        return downcast<MediaPlayerPrivateGStreamerMSE>(mediaSource->player());
    return nullptr;
}

void SourceBufferPrivateGStreamer::detach()
{
    for (auto& track : m_tracks)
        flush(track.first);

    if (RefPtr mediaSource = m_mediaSource.get())
        downcast<MediaSourcePrivateGStreamer>(mediaSource)->detach();
}

void SourceBufferPrivateGStreamer::willSeek()
{
    ALWAYS_LOG(LOGIDENTIFIER);
    m_seeking = true;
}

bool SourceBufferPrivateGStreamer::isSeeking() const
{
    return m_seeking;
}

void SourceBufferPrivateGStreamer::seekToTime(const MediaTime& time)
{
    m_seeking = false;
    // WebKit now has the samples to complete the seek and is about to enqueue them.
    SourceBufferPrivate::seekToTime(time);
}

#undef GST_CAT_DEFAULT

} // namespace WebCore

#endif // ENABLE(MEDIA_SOURCE) && USE(GSTREAMER)
