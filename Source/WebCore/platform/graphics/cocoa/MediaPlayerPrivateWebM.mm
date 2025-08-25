/*
 * Copyright (C) 2022-2025 Apple Inc. All rights reserved.
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
#import "MediaPlayerPrivateWebM.h"

#if ENABLE(COCOA_WEBM_PLAYER)

#import "AudioMediaStreamTrackRenderer.h"
#import "AudioTrackPrivateWebM.h"
#import "FloatSize.h"
#import "GraphicsContext.h"
#import "GraphicsContextStateSaver.h"
#import "IOSurface.h"
#import "Logging.h"
#import "MediaPlaybackTarget.h"
#import "MediaPlayer.h"
#import "MediaSampleAVFObjC.h"
#import "NativeImage.h"
#import "NotImplemented.h"
#import "PixelBufferConformerCV.h"
#import "PlatformDynamicRangeLimitCocoa.h"
#import "PlatformMediaResourceLoader.h"
#import "ResourceError.h"
#import "ResourceRequest.h"
#import "ResourceResponse.h"
#import "SampleMap.h"
#import "SecurityOrigin.h"
#import "SpatialAudioExperienceHelper.h"
#import "TextTrackRepresentation.h"
#import "TrackBuffer.h"
#import "VideoFrameCV.h"
#import "VideoLayerManagerObjC.h"
#import "VideoMediaSampleRenderer.h"
#import "VideoTrackPrivateWebM.h"
#import "WebMResourceClient.h"
#import "WebSampleBufferVideoRendering.h"
#import <AVFoundation/AVFoundation.h>
#import <pal/avfoundation/MediaTimeAVFoundation.h>
#import <pal/spi/cocoa/AVFoundationSPI.h>
#import <pal/spi/cocoa/QuartzCoreSPI.h>
#import <wtf/BlockObjCExceptions.h>
#import <wtf/MainThread.h>
#import <wtf/NativePromise.h>
#import <wtf/SoftLinking.h>
#import <wtf/TZoneMallocInlines.h>
#import <wtf/WeakPtr.h>
#import <wtf/WorkQueue.h>

#pragma mark - Soft Linking

#import "CoreVideoSoftLink.h"
#import "VideoToolboxSoftLink.h"
#import <pal/cf/CoreMediaSoftLink.h>
#import <pal/cocoa/AVFoundationSoftLink.h>

@interface AVSampleBufferDisplayLayer (Staging_100128644)
@property (assign, nonatomic) BOOL preventsAutomaticBackgroundingDuringVideoPlayback;
@end
#if ENABLE(LINEAR_MEDIA_PLAYER)
@interface AVSampleBufferVideoRenderer (Staging_127455709)
- (void)removeAllVideoTargets;
@end
#endif

#pragma mark -

namespace WebCore {

WTF_MAKE_TZONE_ALLOCATED_IMPL(MediaPlayerPrivateWebM);

static const MediaTime discontinuityTolerance = MediaTime(1, 1);

MediaPlayerPrivateWebM::MediaPlayerPrivateWebM(MediaPlayer* player)
    : m_player(player)
    , m_synchronizer(adoptNS([PAL::allocAVSampleBufferRenderSynchronizerInstance() init]))
    , m_parser(SourceBufferParserWebM::create().releaseNonNull())
    , m_appendQueue(WorkQueue::create("MediaPlayerPrivateWebM data parser queue"_s))
    , m_logger(player->mediaPlayerLogger())
    , m_logIdentifier(player->mediaPlayerLogIdentifier())
    , m_videoLayerManager(makeUniqueRef<VideoLayerManagerObjC>(m_logger, m_logIdentifier))
    , m_listener(WebAVSampleBufferListener::create(*this))
    , m_seekTimer(*this, &MediaPlayerPrivateWebM::seekInternal)
{
    ALWAYS_LOG(LOGIDENTIFIER);
    m_parser->setLogger(m_logger, m_logIdentifier);
    m_parser->setDidParseInitializationDataCallback([weakThis = ThreadSafeWeakPtr { *this }, this] (InitializationSegment&& segment) {
        if (RefPtr protectedThis = weakThis.get())
            didParseInitializationData(WTFMove(segment));
    });

    m_parser->setDidProvideMediaDataCallback([weakThis = ThreadSafeWeakPtr { *this }, this] (Ref<MediaSampleAVFObjC>&& sample, TrackID trackId, const String& mediaType) {
        if (RefPtr protectedThis = weakThis.get())
            didProvideMediaDataForTrackId(WTFMove(sample), trackId, mediaType);
    });

    // addPeriodicTimeObserverForInterval: throws an exception if you pass a non-numeric CMTime, so just use
    // an arbitrarily large time value of once an hour:
    __block WeakPtr weakThis { *this };
    m_timeJumpedObserver = [m_synchronizer addPeriodicTimeObserverForInterval:PAL::toCMTime(MediaTime::createWithDouble(3600)) queue:dispatch_get_main_queue() usingBlock:^(CMTime time) {
#if LOG_DISABLED
        UNUSED_PARAM(time);
#endif

        if (!weakThis)
            return;

        auto clampedTime = CMTIME_IS_NUMERIC(time) ? clampTimeToLastSeekTime(PAL::toMediaTime(time)) : MediaTime::zeroTime();
        ALWAYS_LOG(LOGIDENTIFIER, "synchronizer fired: time clamped = ", clampedTime, ", seeking = ", m_isSynchronizerSeeking, ", pending = ", !!m_pendingSeek);

        if (m_isSynchronizerSeeking && !m_pendingSeek) {
            m_isSynchronizerSeeking = false;
            maybeCompleteSeek();
        }
    }];
    ALWAYS_LOG(LOGIDENTIFIER, "synchronizer initial rate:", [m_synchronizer rate]);
    [m_synchronizer setRate:0];
#if ENABLE(LINEAR_MEDIA_PLAYER)
    setVideoTarget(player->videoTarget());
#endif

#if HAVE(SPATIAL_TRACKING_LABEL)
    m_defaultSpatialTrackingLabel = player->defaultSpatialTrackingLabel();
    m_spatialTrackingLabel = player->spatialTrackingLabel();
#endif
}

MediaPlayerPrivateWebM::~MediaPlayerPrivateWebM()
{
    ALWAYS_LOG(LOGIDENTIFIER);

    if (m_seekPromise)
        m_seekPromise->reject();

    if (m_durationObserver)
        [m_synchronizer removeTimeObserver:m_durationObserver.get()];
    if (m_timeJumpedObserver)
        [m_synchronizer removeTimeObserver:m_timeJumpedObserver.get()];

    destroyLayer();
    destroyAudioRenderers();
    m_listener->invalidate();

    clearTracks();

    cancelLoad();
}

static HashSet<String>& mimeTypeCache()
{
    static NeverDestroyed cache = HashSet<String>();
    if (cache->isEmpty())
        cache->addAll(SourceBufferParserWebM::supportedMIMETypes());
    return cache;
}

void MediaPlayerPrivateWebM::getSupportedTypes(HashSet<String>& types)
{
    types = mimeTypeCache();
}

MediaPlayer::SupportsType MediaPlayerPrivateWebM::supportsType(const MediaEngineSupportParameters& parameters)
{
    if (parameters.isMediaSource || parameters.isMediaStream || parameters.requiresRemotePlayback)
        return MediaPlayer::SupportsType::IsNotSupported;

    return SourceBufferParserWebM::isContentTypeSupported(parameters.type, parameters.supportsLimitedMatroska);
}

void MediaPlayerPrivateWebM::setPreload(MediaPlayer::Preload preload)
{
    ALWAYS_LOG(LOGIDENTIFIER, " - ", static_cast<int>(preload));
    if (preload == std::exchange(m_preload, preload))
        return;
    doPreload();
}

void MediaPlayerPrivateWebM::doPreload()
{
    if (m_assetURL.isEmpty() || m_networkState >= MediaPlayerNetworkState::FormatError) {
        INFO_LOG(LOGIDENTIFIER, " - hasURL = ", static_cast<int>(m_assetURL.isEmpty()), " networkState = ", static_cast<int>(m_networkState));
        return;
    }

    auto player = m_player.get();
    if (!player)
        return;

    auto mimeType = player->contentMIMEType();
    if (mimeType.isEmpty() || !mimeTypeCache().contains(mimeType)) {
        ERROR_LOG(LOGIDENTIFIER, "mime type = ", mimeType, " not supported");
        setNetworkState(MediaPlayer::NetworkState::FormatError);
        return;
    }

    if (m_preload >= MediaPlayer::Preload::MetaData && needsResourceClient()) {
        if (!createResourceClientIfNeeded()) {
            ERROR_LOG(LOGIDENTIFIER, "could not create resource client");
            setNetworkState(MediaPlayer::NetworkState::NetworkError);
            setReadyState(MediaPlayer::ReadyState::HaveNothing);
        } else
            setNetworkState(MediaPlayer::NetworkState::Loading);
    }

    if (m_preload > MediaPlayer::Preload::MetaData) {
        for (auto it = m_readyForMoreSamplesMap.begin(); it != m_readyForMoreSamplesMap.end(); ++it)
            notifyClientWhenReadyForMoreSamples(it->first);
    }
}

void MediaPlayerPrivateWebM::load(const URL& url, const LoadOptions& options)
{
    ALWAYS_LOG(LOGIDENTIFIER);

    setReadyState(MediaPlayer::ReadyState::HaveNothing);

    m_assetURL = url;
    if (options.supportsLimitedMatroska)
        m_parser->allowLimitedMatroska();

    doPreload();
}

bool MediaPlayerPrivateWebM::needsResourceClient() const
{
    return !m_resourceClient && m_needsResourceClient;
}

bool MediaPlayerPrivateWebM::createResourceClientIfNeeded()
{
    ALWAYS_LOG(LOGIDENTIFIER);

    ASSERT(needsResourceClient());

    RefPtr player = m_player.get();
    if (!player)
        return false;

    ResourceRequest request(URL { m_assetURL });
    request.setAllowCookies(true);
    if (m_contentReceived) {
        if (!m_contentLength)
            return false;
        if (m_contentLength <= m_contentReceived) {
            m_needsResourceClient = false;
            return true;
        }
        request.addHTTPHeaderField(HTTPHeaderName::Range, makeString("bytes="_s, m_contentReceived, '-', m_contentLength));
    }

    m_resourceClient = WebMResourceClient::create(*this, player->mediaResourceLoader(), WTFMove(request));

    return !!m_resourceClient;
}

#if ENABLE(MEDIA_SOURCE)
void MediaPlayerPrivateWebM::load(const URL&, const LoadOptions&, MediaSourcePrivateClient&)
{
    ERROR_LOG(LOGIDENTIFIER, "tried to load as mediasource");

    setNetworkState(MediaPlayer::NetworkState::FormatError);
}
#endif

#if ENABLE(MEDIA_STREAM)
void MediaPlayerPrivateWebM::load(MediaStreamPrivate&)
{
    ERROR_LOG(LOGIDENTIFIER, "tried to load as mediastream");

    setNetworkState(MediaPlayer::NetworkState::FormatError);
}
#endif

void MediaPlayerPrivateWebM::dataLengthReceived(size_t length)
{
    callOnMainThread([protectedThis = Ref { *this }, length] {
        protectedThis->m_contentLength = length;
    });
}

void MediaPlayerPrivateWebM::dataReceived(const SharedBuffer& buffer)
{
    ALWAYS_LOG(LOGIDENTIFIER, "data length = ", buffer.size());

    callOnMainThread([protectedThis = Ref { *this }, this, size = buffer.size()] {
        setNetworkState(MediaPlayer::NetworkState::Loading);
        m_pendingAppends++;
        m_contentReceived += size;
    });

    invokeAsync(m_appendQueue, [buffer = Ref { buffer }, parser = m_parser]() mutable {
        return MediaPromise::createAndSettle(parser->appendData(WTFMove(buffer)));
    })->whenSettled(RunLoop::mainSingleton(), [weakThis = ThreadSafeWeakPtr { *this }](auto&& result) {
        if (RefPtr protectedThis = weakThis.get())
            protectedThis->appendCompleted(!!result);
    });
}

void MediaPlayerPrivateWebM::loadFailed(const ResourceError& error)
{
    ERROR_LOG(LOGIDENTIFIER, "resource failed to load with code ", error.errorCode());
    callOnMainThread([protectedThis = Ref { *this }] {
        protectedThis->setNetworkState(MediaPlayer::NetworkState::NetworkError);
    });
}

void MediaPlayerPrivateWebM::loadFinished()
{
    ALWAYS_LOG(LOGIDENTIFIER);
    callOnMainThread([protectedThis = Ref { *this }] {
        protectedThis->m_loadFinished = true;
        protectedThis->maybeFinishLoading();
    });
}

void MediaPlayerPrivateWebM::cancelLoad()
{
    if (RefPtr resourceClient = m_resourceClient) {
        resourceClient->stop();
        m_resourceClient = nullptr;
    }
    setNetworkState(MediaPlayer::NetworkState::Idle);
}

PlatformLayer* MediaPlayerPrivateWebM::platformLayer() const
{
    if (!m_videoRenderer)
        return nullptr;
    return m_videoLayerManager->videoInlineLayer();
}

void MediaPlayerPrivateWebM::prepareToPlay()
{
    ALWAYS_LOG(LOGIDENTIFIER);
    setPreload(MediaPlayer::Preload::Auto);
}

void MediaPlayerPrivateWebM::play()
{
#if PLATFORM(IOS_FAMILY)
    flushIfNeeded();
#endif

    m_isPlaying = true;
    if (!shouldBePlaying())
        return;

    [m_synchronizer setRate:m_rate];

    if (currentTime() >= duration())
        seekToTarget(SeekTarget::zero());
}

void MediaPlayerPrivateWebM::pause()
{
    m_isPlaying = false;
    [m_synchronizer setRate:0];
}

bool MediaPlayerPrivateWebM::paused() const
{
    return !m_isPlaying;
}

bool MediaPlayerPrivateWebM::timeIsProgressing() const
{
    return m_isPlaying && [m_synchronizer rate];
}

void MediaPlayerPrivateWebM::setPageIsVisible(bool visible)
{
    if (m_visible == visible)
        return;

    ALWAYS_LOG(LOGIDENTIFIER, visible);
    m_visible = visible;

#if HAVE(SPATIAL_TRACKING_LABEL)
    updateSpatialTrackingLabel();
#endif
}

MediaTime MediaPlayerPrivateWebM::currentTime() const
{
    if (seeking())
        return m_lastSeekTime;

    MediaTime synchronizerTime = clampTimeToLastSeekTime(PAL::toMediaTime(PAL::CMTimebaseGetTime([m_synchronizer timebase])));
    if (synchronizerTime < MediaTime::zeroTime())
        return MediaTime::zeroTime();

    return synchronizerTime;
}

void MediaPlayerPrivateWebM::seekToTarget(const SeekTarget& target)
{
    ALWAYS_LOG(LOGIDENTIFIER, "time = ", target.time, ", negativeThreshold = ", target.negativeThreshold, ", positiveThreshold = ", target.positiveThreshold);

    m_pendingSeek = target;

    if (m_seekTimer.isActive())
        m_seekTimer.stop();
    m_seekTimer.startOneShot(0_s);
}

void MediaPlayerPrivateWebM::seekInternal()
{
    if (!m_pendingSeek)
        return;

    auto pendingSeek = std::exchange(m_pendingSeek, { }).value();
    m_lastSeekTime = pendingSeek.time;

    m_seekState = Seeking;

    seekTo(m_lastSeekTime)->whenSettled(RunLoop::mainSingleton(), [weakThis = ThreadSafeWeakPtr { *this }](auto&& result) {
        if (!result)
            return; // seek cancelled.

        if (RefPtr protectedThis = weakThis.get()) {
            MediaTime synchronizerTime = PAL::toMediaTime([protectedThis->m_synchronizer currentTime]);

            protectedThis->m_isSynchronizerSeeking = std::abs((synchronizerTime - protectedThis->m_lastSeekTime).toMicroseconds()) > 1000;
            ALWAYS_LOG_WITH_THIS(protectedThis, LOGIDENTIFIER_WITH_THIS(protectedThis), "seekedTime = ", protectedThis->m_lastSeekTime, ", synchronizerTime = ", synchronizerTime, "synchronizer seeking = ", protectedThis->m_isSynchronizerSeeking);

            if (!protectedThis->m_isSynchronizerSeeking) {
                // In cases where the destination seek time precisely matches the synchronizer's existing time
                // no time jumped notification will be issued. In this case, just notify the MediaPlayer that
                // the seek completed successfully.
                protectedThis->maybeCompleteSeek();
                return;
            }

            protectedThis->flush();
            [protectedThis->m_synchronizer setRate:0 time:PAL::toCMTime(protectedThis->m_lastSeekTime)];

            for (auto& trackBufferPair : protectedThis->m_trackBufferMap) {
                TrackBuffer& trackBuffer = trackBufferPair.second;
                auto trackId = trackBufferPair.first;

                trackBuffer.setNeedsReenqueueing(true);
                protectedThis->reenqueueMediaForTime(trackBuffer, trackId, protectedThis->m_lastSeekTime, NeedsFlush::No);
            }

            protectedThis->maybeCompleteSeek();
        }
    });
}

Ref<GenericPromise> MediaPlayerPrivateWebM::seekTo(const MediaTime& time)
{
    if (m_seekPromise) {
        m_seekPromise->reject();
        m_seekPromise.reset();
    }

    if (m_buffered.contain(time))
        return GenericPromise::createAndResolve();

    [m_synchronizer setRate:0];
    setReadyState(MediaPlayer::ReadyState::HaveMetadata);

    m_seekPromise.emplace();
    return m_seekPromise->promise();
}

void MediaPlayerPrivateWebM::maybeCompleteSeek()
{
    if (m_seekState == SeekCompleted)
        return;
    if (hasVideo() && !m_hasAvailableVideoFrame) {
        ALWAYS_LOG(LOGIDENTIFIER, "waiting for video frame");
        m_seekState = WaitingForAvailableFame;
        return;
    }
    m_seekState = Seeking;
    ALWAYS_LOG(LOGIDENTIFIER);
    if (m_isSynchronizerSeeking) {
        ALWAYS_LOG(LOGIDENTIFIER, "Synchronizer still seeking, bailing out");
        return;
    }
    m_seekState = SeekCompleted;
    if (shouldBePlaying())
        [m_synchronizer setRate:m_rate];
    if (auto player = m_player.get()) {
        player->seeked(m_lastSeekTime);
        player->timeChanged();
    }
}

bool MediaPlayerPrivateWebM::seeking() const
{
    return m_pendingSeek || m_seekState != SeekCompleted;
}

MediaTime MediaPlayerPrivateWebM::clampTimeToLastSeekTime(const MediaTime& time) const
{
    if (m_lastSeekTime.isFinite() && time < m_lastSeekTime)
        return m_lastSeekTime;

    return time;
}

bool MediaPlayerPrivateWebM::shouldBePlaying() const
{
    return m_isPlaying && !seeking();
}

void MediaPlayerPrivateWebM::setRateDouble(double rate)
{
    if (rate == m_rate)
        return;

    m_rate = std::max<double>(rate, 0);

    if (shouldBePlaying())
        [m_synchronizer setRate:m_rate];

    if (auto player = m_player.get())
        player->rateChanged();
}

double MediaPlayerPrivateWebM::effectiveRate() const
{
    return PAL::CMTimebaseGetRate([m_synchronizer timebase]);
}

void MediaPlayerPrivateWebM::setVolume(float volume)
{
    for (auto& pair : m_audioRenderers) {
        auto& renderer = pair.second;
        [renderer setVolume:volume];
    }
}

void MediaPlayerPrivateWebM::setMuted(bool muted)
{
    for (auto& pair : m_audioRenderers) {
        auto& renderer = pair.second;
        [renderer setMuted:muted];
    }
}

const PlatformTimeRanges& MediaPlayerPrivateWebM::buffered() const
{
    return m_buffered;
}

void MediaPlayerPrivateWebM::setBufferedRanges(PlatformTimeRanges timeRanges)
{
    if (m_buffered == timeRanges)
        return;
    m_buffered = WTFMove(timeRanges);
    if (auto player = m_player.get()) {
        player->bufferedTimeRangesChanged();
        player->seekableTimeRangesChanged();
    }
}

void MediaPlayerPrivateWebM::updateBufferedFromTrackBuffers(bool ended)
{
    MediaTime highestEndTime = MediaTime::negativeInfiniteTime();
    for (auto& pair : m_trackBufferMap) {
        auto& trackBuffer = pair.second;
        if (!trackBuffer->buffered().length())
            continue;
        highestEndTime = std::max(highestEndTime, trackBuffer->maximumBufferedTime());
    }

    // NOTE: Short circuit the following if none of the TrackBuffers have buffered ranges to avoid generating
    // a single range of {0, 0}.
    if (highestEndTime.isNegativeInfinite()) {
        setBufferedRanges(PlatformTimeRanges());
        return;
    }

    PlatformTimeRanges intersectionRanges { MediaTime::zeroTime(), highestEndTime };

    for (auto& pair : m_trackBufferMap) {
        auto& trackBuffer = pair.second;
        if (!trackBuffer->buffered().length())
            continue;

        PlatformTimeRanges trackRanges = trackBuffer->buffered();

        if (ended)
            trackRanges.add(trackRanges.maximumBufferedTime(), highestEndTime);

        intersectionRanges.intersectWith(trackRanges);
    }

    setBufferedRanges(WTFMove(intersectionRanges));
}

void MediaPlayerPrivateWebM::updateDurationFromTrackBuffers()
{
    MediaTime highestEndTime = MediaTime::zeroTime();
    for (auto& pair : m_trackBufferMap) {
        auto& trackBuffer = pair.second;
        if (!trackBuffer->highestPresentationTimestamp())
            continue;
        highestEndTime = std::max(highestEndTime, trackBuffer->highestPresentationTimestamp());
    }

    setDuration(WTFMove(highestEndTime));
}

void MediaPlayerPrivateWebM::setLoadingProgresssed(bool loadingProgressed)
{
    INFO_LOG(LOGIDENTIFIER, loadingProgressed);
    m_loadingProgressed = loadingProgressed;
}

bool MediaPlayerPrivateWebM::didLoadingProgress() const
{
    return std::exchange(m_loadingProgressed, false);
}

RefPtr<NativeImage> MediaPlayerPrivateWebM::nativeImageForCurrentTime()
{
    updateLastImage();
    return m_lastImage;
}

bool MediaPlayerPrivateWebM::updateLastPixelBuffer()
{
    RefPtr videoRenderer = m_videoRenderer;
    if (!videoRenderer)
        return false;

    auto entry = videoRenderer->copyDisplayedPixelBuffer();
    if (!entry.pixelBuffer)
        return false;

    INFO_LOG(LOGIDENTIFIER, "displayed pixelbuffer copied for time ", entry.presentationTimeStamp);
    m_lastPixelBuffer = WTFMove(entry.pixelBuffer);
    m_lastPixelBufferPresentationTimeStamp = entry.presentationTimeStamp;
    return true;
}

bool MediaPlayerPrivateWebM::updateLastImage()
{
    if (m_isGatheringVideoFrameMetadata) {
        if (!m_lastPixelBuffer)
            return false;
        RefPtr videoRenderer = m_videoRenderer;
        auto sampleCount = videoRenderer ? videoRenderer->totalDisplayedFrames() : 0;
        if (sampleCount == m_lastConvertedSampleCount)
            return false;
        m_lastConvertedSampleCount = sampleCount;
    } else if (!updateLastPixelBuffer())
        return false;

    ASSERT(m_lastPixelBuffer);

    if (!m_rgbConformer) {
        auto attributes = @{ (__bridge NSString *)kCVPixelBufferPixelFormatTypeKey: @(kCVPixelFormatType_32BGRA) };
        m_rgbConformer = makeUnique<PixelBufferConformerCV>((__bridge CFDictionaryRef)attributes);
    }

    m_lastImage = NativeImage::create(m_rgbConformer->createImageFromPixelBuffer(m_lastPixelBuffer.get()));
    return true;
}

void MediaPlayerPrivateWebM::paint(GraphicsContext& context, const FloatRect& rect)
{
    paintCurrentFrameInContext(context, rect);
}

void MediaPlayerPrivateWebM::paintCurrentFrameInContext(GraphicsContext& context, const FloatRect& outputRect)
{
    if (context.paintingDisabled())
        return;

    auto image = nativeImageForCurrentTime();
    if (!image)
        return;

    GraphicsContextStateSaver stateSaver(context);
    FloatRect imageRect { FloatPoint::zero(), image->size() };
    context.drawNativeImage(*image, outputRect, imageRect);
}

RefPtr<VideoFrame> MediaPlayerPrivateWebM::videoFrameForCurrentTime()
{
    if (!m_isGatheringVideoFrameMetadata)
        updateLastPixelBuffer();
    if (!m_lastPixelBuffer)
        return nullptr;
    return VideoFrameCV::create(currentTime(), false, VideoFrame::Rotation::None, RetainPtr { m_lastPixelBuffer });
}

DestinationColorSpace MediaPlayerPrivateWebM::colorSpace()
{
    updateLastImage();
    RefPtr lastImage = m_lastImage;
    return lastImage ? lastImage->colorSpace() : DestinationColorSpace::SRGB();
}

void MediaPlayerPrivateWebM::setNaturalSize(FloatSize size)
{
    auto oldSize = m_naturalSize;
    m_naturalSize = size;
    if (oldSize != m_naturalSize) {
        INFO_LOG(LOGIDENTIFIER, "was ", oldSize, ", is ", size);
        if (auto player = m_player.get())
            player->sizeChanged();
    }
}

void MediaPlayerPrivateWebM::setHasAudio(bool hasAudio)
{
    if (hasAudio == m_hasAudio)
        return;

    m_hasAudio = hasAudio;
    characteristicsChanged();
}

void MediaPlayerPrivateWebM::setHasVideo(bool hasVideo)
{
    if (hasVideo == m_hasVideo)
        return;

    m_hasVideo = hasVideo;
    characteristicsChanged();
}

void MediaPlayerPrivateWebM::setHasAvailableVideoFrame(bool hasAvailableVideoFrame)
{
    if (m_hasAvailableVideoFrame == hasAvailableVideoFrame)
        return;

    ALWAYS_LOG(LOGIDENTIFIER, hasAvailableVideoFrame);
    m_hasAvailableVideoFrame = hasAvailableVideoFrame;

    if (!m_hasAvailableVideoFrame)
        return;

    if (auto player = m_player.get())
        player->firstVideoFrameAvailable();

    if (m_seekState == WaitingForAvailableFame)
        maybeCompleteSeek();

    setReadyState(MediaPlayer::ReadyState::HaveEnoughData);
}

void MediaPlayerPrivateWebM::setDuration(MediaTime duration)
{
    if (duration == m_duration)
        return;

    if (m_durationObserver)
        [m_synchronizer removeTimeObserver:m_durationObserver.get()];

    NSArray* times = @[[NSValue valueWithCMTime:PAL::toCMTime(duration)]];

    auto logSiteIdentifier = LOGIDENTIFIER;
    DEBUG_LOG(logSiteIdentifier, duration);
    UNUSED_PARAM(logSiteIdentifier);

    m_durationObserver = [m_synchronizer addBoundaryTimeObserverForTimes:times queue:dispatch_get_main_queue() usingBlock:[weakThis = ThreadSafeWeakPtr { *this }, duration, logSiteIdentifier] {
        RefPtr protectedThis = weakThis.get();
        if (!protectedThis)
            return;

        MediaTime now = protectedThis->currentTime();
        ALWAYS_LOG_WITH_THIS(protectedThis, logSiteIdentifier, "boundary time observer called, now = ", now);

        protectedThis->pause();
        if (now < duration) {
            ERROR_LOG_WITH_THIS(protectedThis, logSiteIdentifier, "ERROR: boundary time observer called before duration");
            [protectedThis->m_synchronizer setRate:0 time:PAL::toCMTime(duration)];
        }
        if (auto player = protectedThis->m_player.get())
            player->timeChanged();

    }];

    m_duration = WTFMove(duration);
    if (auto player = m_player.get())
        player->durationChanged();
}

void MediaPlayerPrivateWebM::setNetworkState(MediaPlayer::NetworkState state)
{
    if (state == m_networkState)
        return;

    ALWAYS_LOG(LOGIDENTIFIER, state);
    m_networkState = state;
    if (auto player = m_player.get())
        player->networkStateChanged();
}

void MediaPlayerPrivateWebM::setReadyState(MediaPlayer::ReadyState state)
{
    if (state == m_readyState)
        return;

    ALWAYS_LOG(LOGIDENTIFIER, state);
    m_readyState = state;

    if (auto player = m_player.get())
        player->readyStateChanged();
}

void MediaPlayerPrivateWebM::characteristicsChanged()
{
    if (auto player = m_player.get())
        player->characteristicChanged();
}

bool MediaPlayerPrivateWebM::shouldEnsureLayerOrVideoRenderer() const
{
    auto player = m_player.get();
    return ((m_sampleBufferDisplayLayer && !CGRectIsEmpty([m_sampleBufferDisplayLayer bounds])) || (player && !player->presentationSize().isEmpty()));
}

void MediaPlayerPrivateWebM::setPresentationSize(const IntSize& newSize)
{
    if (m_hasVideo && !m_videoRenderer && !newSize.isEmpty())
        updateDisplayLayer();
}

void MediaPlayerPrivateWebM::acceleratedRenderingStateChanged()
{
    if (m_hasVideo)
        updateDisplayLayer();
}

void MediaPlayerPrivateWebM::updateDisplayLayer()
{
    if (shouldEnsureLayerOrVideoRenderer()) {
        RefPtr videoRenderer = m_videoRenderer;
        auto needsRenderingModeChanged = !videoRenderer || videoRenderer->renderer() ? MediaPlayerEnums::NeedsRenderingModeChanged::No : MediaPlayerEnums::NeedsRenderingModeChanged::Yes;
        ensureLayerOrVideoRenderer(needsRenderingModeChanged);
        return;
    }
    destroyLayerOrVideoRendererAndCreateRenderlessVideoMediaSampleRenderer();
}

RetainPtr<PlatformLayer> MediaPlayerPrivateWebM::createVideoFullscreenLayer()
{
    return adoptNS([[CALayer alloc] init]);
}

void MediaPlayerPrivateWebM::setVideoFullscreenLayer(PlatformLayer *videoFullscreenLayer, WTF::Function<void()>&& completionHandler)
{
    updateLastImage();
    RefPtr lastImage = m_lastImage;
    m_videoLayerManager->setVideoFullscreenLayer(videoFullscreenLayer, WTFMove(completionHandler), lastImage ? lastImage->platformImage() : nullptr);
}

void MediaPlayerPrivateWebM::setVideoFullscreenFrame(FloatRect frame)
{
    m_videoLayerManager->setVideoFullscreenFrame(frame);
}

void MediaPlayerPrivateWebM::syncTextTrackBounds()
{
    m_videoLayerManager->syncTextTrackBounds();
}

void MediaPlayerPrivateWebM::setTextTrackRepresentation(TextTrackRepresentation* representation)
{
    auto* representationLayer = representation ? representation->platformLayer() : nil;
    m_videoLayerManager->setTextTrackRepresentationLayer(representationLayer);
}

String MediaPlayerPrivateWebM::engineDescription() const
{
    static NeverDestroyed<String> description(MAKE_STATIC_STRING_IMPL("Cocoa WebM Engine"));
    return description;
}

#if ENABLE(WIRELESS_PLAYBACK_TARGET)
void MediaPlayerPrivateWebM::setWirelessPlaybackTarget(Ref<MediaPlaybackTarget>&& target)
{
    ALWAYS_LOG(LOGIDENTIFIER);
    m_playbackTarget = WTFMove(target);
}

void MediaPlayerPrivateWebM::setShouldPlayToPlaybackTarget(bool shouldPlayToTarget)
{
    if (shouldPlayToTarget == m_shouldPlayToTarget)
        return;

    ALWAYS_LOG(LOGIDENTIFIER, shouldPlayToTarget);
    m_shouldPlayToTarget = shouldPlayToTarget;

    if (auto player = m_player.get())
        player->currentPlaybackTargetIsWirelessChanged(isCurrentPlaybackTargetWireless());
}

bool MediaPlayerPrivateWebM::isCurrentPlaybackTargetWireless() const
{
    RefPtr playbackTarget = m_playbackTarget;
    if (!playbackTarget)
        return false;

    auto hasTarget = m_shouldPlayToTarget && playbackTarget->hasActiveRoute();
    INFO_LOG(LOGIDENTIFIER, hasTarget);
    return hasTarget;
}
#endif

void MediaPlayerPrivateWebM::enqueueSample(Ref<MediaSample>&& sample, TrackID trackId)
{
    if (!isEnabledVideoTrackID(trackId) && !m_audioRenderers.contains(trackId))
        return;

    auto logSiteIdentifier = LOGIDENTIFIER;
    DEBUG_LOG(logSiteIdentifier, "track ID = ", trackId, ", sample = ", sample.get());

    PlatformSample platformSample = sample->platformSample();

    CMFormatDescriptionRef formatDescription = PAL::CMSampleBufferGetFormatDescription(platformSample.sample.cmSampleBuffer);
    ASSERT(formatDescription);
    if (!formatDescription) {
        ERROR_LOG(logSiteIdentifier, "Received sample with a null formatDescription. Bailing.");
        return;
    }
    auto mediaType = PAL::CMFormatDescriptionGetMediaType(formatDescription);

    if (isEnabledVideoTrackID(trackId)) {
        // AVSampleBufferDisplayLayer will throw an un-documented exception if passed a sample
        // whose media type is not kCMMediaType_Video. This condition is exceptional; we should
        // never enqueue a non-video sample in a AVSampleBufferDisplayLayer.
        ASSERT(mediaType == kCMMediaType_Video);
        if (mediaType != kCMMediaType_Video) {
            ERROR_LOG(logSiteIdentifier, "Expected sample of type '", FourCC(kCMMediaType_Video), "', got '", FourCC(mediaType), "'. Bailing.");
            return;
        }

        FloatSize formatSize = FloatSize(PAL::CMVideoFormatDescriptionGetPresentationDimensions(formatDescription, true, true));
        if (formatSize != m_naturalSize)
            setNaturalSize(formatSize);

        if (RefPtr videoRenderer = m_videoRenderer)
            videoRenderer->enqueueSample(sample, sample->presentationTime());

        return;
    }
    // AVSampleBufferAudioRenderer will throw an un-documented exception if passed a sample
    // whose media type is not kCMMediaType_Audio. This condition is exceptional; we should
    // never enqueue a non-video sample in a AVSampleBufferAudioRenderer.
    ASSERT(mediaType == kCMMediaType_Audio);
    if (mediaType != kCMMediaType_Audio) {
        ERROR_LOG(logSiteIdentifier, "Expected sample of type '", FourCC(kCMMediaType_Audio), "', got '", FourCC(mediaType), "'. Bailing.");
        return;
    }

    if (m_readyState < MediaPlayer::ReadyState::HaveEnoughData && !m_enabledVideoTrackID)
        setReadyState(MediaPlayer::ReadyState::HaveEnoughData);

    auto itRenderer = m_audioRenderers.find(trackId);
    ASSERT(itRenderer != m_audioRenderers.end());
    [itRenderer->second enqueueSampleBuffer:platformSample.sample.cmSampleBuffer];
}

void MediaPlayerPrivateWebM::reenqueSamples(TrackID trackId, NeedsFlush needsFlush)
{
    auto it = m_trackBufferMap.find(trackId);
    if (it == m_trackBufferMap.end())
        return;
    TrackBuffer& trackBuffer = it->second;
    trackBuffer.setNeedsReenqueueing(true);
    reenqueueMediaForTime(trackBuffer, trackId, currentTime(), needsFlush);
}

void MediaPlayerPrivateWebM::reenqueueMediaForTime(TrackBuffer& trackBuffer, TrackID trackId, const MediaTime& time, NeedsFlush needsFlush)
{
    if (needsFlush == NeedsFlush::Yes)
        flushTrack(trackId);
    if (trackBuffer.reenqueueMediaForTime(time, timeFudgeFactor(), m_loadFinished))
        provideMediaData(trackBuffer, trackId);
}

void MediaPlayerPrivateWebM::notifyClientWhenReadyForMoreSamples(TrackID trackId)
{
    if (isEnabledVideoTrackID(trackId)) {
        RefPtr videoRenderer = m_videoRenderer;
        if (!videoRenderer)
            return;

        videoRenderer->requestMediaDataWhenReady([weakThis = ThreadSafeWeakPtr { *this }, trackId] {
            if (RefPtr protectedThis = weakThis.get())
                protectedThis->didBecomeReadyForMoreSamples(trackId);
        });
        return;
    }

    if (auto itAudioRenderer = m_audioRenderers.find(trackId); itAudioRenderer != m_audioRenderers.end()) {
        ThreadSafeWeakPtr weakThis { *this };
        [itAudioRenderer->second requestMediaDataWhenReadyOnQueue:dispatch_get_main_queue() usingBlock:^{
            if (RefPtr protectedThis = weakThis.get())
                didBecomeReadyForMoreSamples(trackId);
        }];
    }
}

void MediaPlayerPrivateWebM::setMinimumUpcomingPresentationTime(TrackID trackId, const MediaTime& presentationTime)
{
    if (isEnabledVideoTrackID(trackId)) {
        if (RefPtr videoRenderer = m_videoRenderer)
            videoRenderer->expectMinimumUpcomingSampleBufferPresentationTime(presentationTime);
    }
}

bool MediaPlayerPrivateWebM::isReadyForMoreSamples(TrackID trackId)
{
    if (isEnabledVideoTrackID(trackId)) {
#if PLATFORM(IOS_FAMILY)
        if (m_layerRequiresFlush)
            return false;
#endif
        return protectedVideoRenderer()->isReadyForMoreMediaData();
    }

    if (auto itAudioRenderer = m_audioRenderers.find(trackId); itAudioRenderer != m_audioRenderers.end())
        return [itAudioRenderer->second isReadyForMoreMediaData];

    return false;
}

void MediaPlayerPrivateWebM::didBecomeReadyForMoreSamples(TrackID trackId)
{
    INFO_LOG(LOGIDENTIFIER, trackId);

    if (isEnabledVideoTrackID(trackId)) {
        if (RefPtr videoRenderer = m_videoRenderer)
            videoRenderer->stopRequestingMediaData();
    } else if (auto itAudioRenderer = m_audioRenderers.find(trackId); itAudioRenderer != m_audioRenderers.end())
        [itAudioRenderer->second stopRequestingMediaData];
    else
        return;

    provideMediaData(trackId);
}

void MediaPlayerPrivateWebM::appendCompleted(bool success)
{
    assertIsMainThread();

    ASSERT(m_pendingAppends > 0);
    m_pendingAppends--;
    INFO_LOG(LOGIDENTIFIER, "pending appends = ", m_pendingAppends, " success = ", success);
    setLoadingProgresssed(true);
    m_errored |= !success;
    if (!m_errored)
        updateBufferedFromTrackBuffers(m_loadFinished && !m_pendingAppends);

    if (m_seekPromise && m_buffered.contain(m_lastSeekTime)) {
        m_seekPromise->resolve();
        m_seekPromise.reset();
    }

    maybeFinishLoading();
}

void MediaPlayerPrivateWebM::maybeFinishLoading()
{
    if (m_loadFinished && !m_pendingAppends) {
        if (!m_hasVideo && !m_hasAudio) {
            ERROR_LOG(LOGIDENTIFIER, "could not load audio or video tracks");
            setNetworkState(MediaPlayer::NetworkState::FormatError);
            setReadyState(MediaPlayer::ReadyState::HaveNothing);
            return;
        }
        if (m_errored) {
            ERROR_LOG(LOGIDENTIFIER, "parsing error");
            setNetworkState(m_readyState >= MediaPlayer::ReadyState::HaveMetadata ? MediaPlayer::NetworkState::DecodeError : MediaPlayer::NetworkState::FormatError);
            return;
        }
        setNetworkState(MediaPlayer::NetworkState::Idle);

        updateDurationFromTrackBuffers();
    }
}

void MediaPlayerPrivateWebM::provideMediaData(TrackID trackId)
{
    auto it = m_trackBufferMap.find(trackId);
    if (it == m_trackBufferMap.end())
        return;

    provideMediaData(it->second, trackId);
}

void MediaPlayerPrivateWebM::provideMediaData(TrackBuffer& trackBuffer, TrackID trackId)
{
    if (m_errored)
        return;

    unsigned enqueuedSamples = 0;

    while (true) {
        if (!isReadyForMoreSamples(trackId)) {
            DEBUG_LOG(LOGIDENTIFIER, "bailing early, track id ", trackId, " is not ready for more data");
            notifyClientWhenReadyForMoreSamples(trackId);
            break;
        }

        RefPtr sample = trackBuffer.nextSample();
        if (!sample)
            break;
        enqueueSample(sample.releaseNonNull(), trackId);
        ++enqueuedSamples;
    }

    if (isEnabledVideoTrackID(trackId))
        setMinimumUpcomingPresentationTime(trackId, trackBuffer.minimumEnqueuedPresentationTime());

    DEBUG_LOG(LOGIDENTIFIER, "enqueued ", enqueuedSamples, " samples, ", trackBuffer.remainingSamples(), " remaining");
}

void MediaPlayerPrivateWebM::trackDidChangeSelected(VideoTrackPrivate& track, bool selected)
{
    auto trackId = track.id();

    if (!m_trackBufferMap.contains(trackId))
        return;

    ALWAYS_LOG(LOGIDENTIFIER, "video trackID = ", trackId, ", selected = ", selected);

    if (selected) {
        m_enabledVideoTrackID = trackId;
        updateDisplayLayer();
        return;
    }

    if (isEnabledVideoTrackID(trackId)) {
        m_enabledVideoTrackID.reset();
        m_readyForMoreSamplesMap.erase(trackId);
        if (RefPtr videoRenderer = m_videoRenderer)
            videoRenderer->stopRequestingMediaData();
    }
}

void MediaPlayerPrivateWebM::trackDidChangeEnabled(AudioTrackPrivate& track, bool enabled)
{
    auto trackId = track.id();

    if (!m_trackBufferMap.contains(trackId))
        return;

    ALWAYS_LOG(LOGIDENTIFIER, "audio trackID = ", trackId, ", enabled = ", enabled);

    if (enabled) {
        addAudioRenderer(trackId);
        m_readyForMoreSamplesMap[trackId] = true;
        return;
    }

    m_readyForMoreSamplesMap.erase(trackId);
    removeAudioRenderer(trackId);
}

void MediaPlayerPrivateWebM::didParseInitializationData(InitializationSegment&& segment)
{
    ALWAYS_LOG(LOGIDENTIFIER);

    if (m_preload == MediaPlayer::Preload::MetaData && !m_loadFinished)
        cancelLoad();

    clearTracks();

    if (segment.duration.isValid())
        setDuration(WTFMove(segment.duration));
    else
        setDuration(MediaTime::positiveInfiniteTime());

    auto player = m_player.get();
    for (auto videoTrackInfo : segment.videoTracks) {
        if (videoTrackInfo.track) {
            auto track = static_pointer_cast<VideoTrackPrivateWebM>(videoTrackInfo.track);
#if PLATFORM(IOS_FAMILY)
            if (shouldCheckHardwareSupport() && (videoTrackInfo.description->codec() == "vp8"_s || (videoTrackInfo.description->codec() == "vp9"_s && !(canLoad_VideoToolbox_VTIsHardwareDecodeSupported() && VTIsHardwareDecodeSupported(kCMVideoCodecType_VP9))))) {
                m_errored = true;
                return;
            }
#endif
            addTrackBuffer(track->id(), WTFMove(videoTrackInfo.description));

            track->setSelectedChangedCallback([weakThis = ThreadSafeWeakPtr { *this }] (VideoTrackPrivate& track, bool selected) {
                RefPtr protectedThis = weakThis.get();
                if (!protectedThis)
                    return;

                auto videoTrackSelectedChanged = [weakThis, trackRef = Ref { track }, selected] {
                    if (RefPtr protectedThis = weakThis.get())
                        protectedThis->trackDidChangeSelected(trackRef, selected);
                };

                if (!protectedThis->m_processingInitializationSegment) {
                    videoTrackSelectedChanged();
                    return;
                }
            });

            if (m_videoTracks.isEmpty()) {
                setNaturalSize({ float(track->width()), float(track->height()) });
                track->setSelected(true);
            }

            m_videoTracks.append(track);
            if (player)
                player->addVideoTrack(*track);
        }
    }

    for (auto audioTrackInfo : segment.audioTracks) {
        if (audioTrackInfo.track) {
            auto track = static_pointer_cast<AudioTrackPrivateWebM>(audioTrackInfo.track);
            addTrackBuffer(track->id(), WTFMove(audioTrackInfo.description));

            track->setEnabledChangedCallback([weakThis = ThreadSafeWeakPtr { *this }] (AudioTrackPrivate& track, bool enabled) {
                RefPtr protectedThis = weakThis.get();
                if (!protectedThis)
                    return;

                auto audioTrackEnabledChanged = [weakThis, trackRef = Ref { track }, enabled] {
                    if (RefPtr protectedThis = weakThis.get())
                        protectedThis->trackDidChangeEnabled(trackRef, enabled);
                };

                if (!protectedThis->m_processingInitializationSegment) {
                    audioTrackEnabledChanged();
                    return;
                }
            });

            if (m_audioTracks.isEmpty())
                track->setEnabled(true);

            m_audioTracks.append(track);
            if (player)
                player->addAudioTrack(*track);
        }
    }

    setReadyState(MediaPlayer::ReadyState::HaveMetadata);
}

void MediaPlayerPrivateWebM::didProvideMediaDataForTrackId(Ref<MediaSampleAVFObjC>&& sample, TrackID trackId, const String& mediaType)
{
    UNUSED_PARAM(mediaType);

    auto it = m_trackBufferMap.find(trackId);
    if (it == m_trackBufferMap.end())
        return;
    TrackBuffer& trackBuffer = it->second;

    trackBuffer.addSample(sample);

    if (m_preload <= MediaPlayer::Preload::MetaData) {
        m_readyForMoreSamplesMap[trackId] = true;
        return;
    }
    notifyClientWhenReadyForMoreSamples(trackId);
}

void MediaPlayerPrivateWebM::flush()
{
    if (m_videoTracks.size())
        flushVideo();

    if (!m_audioTracks.size())
        return;

    for (auto& pair : m_audioRenderers) {
        auto& renderer = pair.second;
        flushAudio(renderer.get());
    }
}

void MediaPlayerPrivateWebM::flushIfNeeded()
{
#if PLATFORM(IOS_FAMILY)
    if (!m_layerRequiresFlush)
        return;

    m_layerRequiresFlush = false;
#endif

    if (m_videoTracks.size())
        flushVideo();

    // We initiatively enqueue samples instead of waiting for the
    // media data requests from m_displayLayer.
    // In addition, we need to enqueue a sync sample (IDR video frame) first.
    if (RefPtr videoRenderer = m_videoRenderer)
        videoRenderer->stopRequestingMediaData();

    if (m_enabledVideoTrackID)
        reenqueSamples(*m_enabledVideoTrackID);
}

void MediaPlayerPrivateWebM::flushTrack(TrackID trackId)
{
    DEBUG_LOG(LOGIDENTIFIER, trackId);

    if (isEnabledVideoTrackID(trackId)) {
        flushVideo();
        return;
    }

    if (auto itAudioRenderer = m_audioRenderers.find(trackId); itAudioRenderer != m_audioRenderers.end())
        flushAudio(itAudioRenderer->second.get());
}

void MediaPlayerPrivateWebM::flushVideo()
{
    DEBUG_LOG(LOGIDENTIFIER);
    if (RefPtr videoRenderer = m_videoRenderer)
        videoRenderer->flush();
    setHasAvailableVideoFrame(false);
}

void MediaPlayerPrivateWebM::flushAudio(AVSampleBufferAudioRenderer *renderer)
{
    DEBUG_LOG(LOGIDENTIFIER);
    [renderer flush];
}

void MediaPlayerPrivateWebM::addTrackBuffer(TrackID trackId, RefPtr<MediaDescription>&& description)
{
    ASSERT(!m_trackBufferMap.contains(trackId));

    setHasAudio(m_hasAudio || description->isAudio());
    setHasVideo(m_hasVideo || description->isVideo());

    auto trackBuffer = TrackBuffer::create(WTFMove(description), discontinuityTolerance);
    trackBuffer->setLogger(protectedLogger(), logIdentifier());
    m_trackBufferMap.try_emplace(trackId, WTFMove(trackBuffer));
}

void MediaPlayerPrivateWebM::destroyVideoLayerIfNeeded()
{
    if (!m_needsDestroyVideoLayer)
        return;
    m_needsDestroyVideoLayer = false;
    m_videoLayerManager->didDestroyVideoLayer();
}

void MediaPlayerPrivateWebM::ensureLayer()
{
    if (m_sampleBufferDisplayLayer)
        return;

    ALWAYS_LOG(LOGIDENTIFIER);

    destroyVideoLayerIfNeeded();

    m_sampleBufferDisplayLayer = adoptNS([PAL::allocAVSampleBufferDisplayLayerInstance() init]);
    if (!m_sampleBufferDisplayLayer)
        return;

    [m_sampleBufferDisplayLayer setName:@"MediaPlayerPrivateWebM AVSampleBufferDisplayLayer"];
    [m_sampleBufferDisplayLayer setVideoGravity: (m_shouldMaintainAspectRatio ? AVLayerVideoGravityResizeAspect : AVLayerVideoGravityResize)];

    configureLayerOrVideoRenderer(m_sampleBufferDisplayLayer.get());

    if (RefPtr player = m_player.get()) {
        if ([m_sampleBufferDisplayLayer respondsToSelector:@selector(setToneMapToStandardDynamicRange:)])
            [m_sampleBufferDisplayLayer setToneMapToStandardDynamicRange:player->shouldDisableHDR()];

        setLayerDynamicRangeLimit(m_sampleBufferDisplayLayer.get(), player->platformDynamicRangeLimit());

        m_videoLayerManager->setVideoLayer(m_sampleBufferDisplayLayer.get(), player->presentationSize());
    }
}

void MediaPlayerPrivateWebM::addAudioRenderer(TrackID trackId)
{
    if (m_audioRenderers.contains(trackId))
        return;

    auto renderer = adoptNS([PAL::allocAVSampleBufferAudioRendererInstance() init]);

    if (!renderer) {
        ERROR_LOG(LOGIDENTIFIER, "-[AVSampleBufferAudioRenderer init] returned nil! bailing!");
        ASSERT_NOT_REACHED();

        setNetworkState(MediaPlayer::NetworkState::DecodeError);
        return;
    }

    auto player = m_player.get();
    if (!player)
        return;

    [renderer setMuted:player->muted()];
    [renderer setVolume:player->volume()];
    [renderer setAudioTimePitchAlgorithm:(player->preservesPitch() ? AVAudioTimePitchAlgorithmSpectral : AVAudioTimePitchAlgorithmVarispeed)];

#if HAVE(AUDIO_OUTPUT_DEVICE_UNIQUE_ID)
    auto deviceId = player->audioOutputDeviceIdOverride();
    if (!deviceId.isNull() && renderer) {
        if (deviceId.isEmpty() || deviceId == AudioMediaStreamTrackRenderer::defaultDeviceID()) {
            // FIXME(rdar://155986053): Remove the @try/@catch when this exception is resolved.
            @try {
                renderer.get().audioOutputDeviceUniqueID = nil;
            } @catch(NSException *exception) {
                ERROR_LOG(LOGIDENTIFIER, "-[AVSampleBufferRenderSynchronizer setAudioOutputDeviceUniqueID:] threw an exception: ", exception.name, ", reason : ", exception.reason);
            }
        } else
            renderer.get().audioOutputDeviceUniqueID = deviceId.createNSString().get();
    }
#endif

    @try {
        [m_synchronizer addRenderer:renderer.get()];
    } @catch(NSException *exception) {
        ERROR_LOG(LOGIDENTIFIER, "-[AVSampleBufferRenderSynchronizer addRenderer:] threw an exception: ", exception.name, ", reason : ", exception.reason);
        ASSERT_NOT_REACHED();

        setNetworkState(MediaPlayer::NetworkState::DecodeError);
        return;
    }

    characteristicsChanged();

    m_audioRenderers.try_emplace(trackId, renderer);
    m_listener->beginObservingAudioRenderer(renderer.get());
}

void MediaPlayerPrivateWebM::removeAudioRenderer(TrackID trackId)
{
    auto itRenderer = m_audioRenderers.find(trackId);
    if (itRenderer == m_audioRenderers.end())
        return;
    destroyAudioRenderer(itRenderer->second);
    m_audioRenderers.erase(trackId);
}

void MediaPlayerPrivateWebM::destroyLayer()
{
    if (!m_sampleBufferDisplayLayer)
        return;

    ALWAYS_LOG(LOGIDENTIFIER);

    CMTime currentTime = PAL::CMTimebaseGetTime([m_synchronizer timebase]);
    [m_synchronizer removeRenderer:m_sampleBufferDisplayLayer.get() atTime:currentTime completionHandler:nil];

    m_videoLayerManager->didDestroyVideoLayer();
    m_sampleBufferDisplayLayer = nullptr;
    m_needsDestroyVideoLayer = false;
}

void MediaPlayerPrivateWebM::ensureVideoRenderer()
{
#if ENABLE(LINEAR_MEDIA_PLAYER)
    if (m_sampleBufferVideoRenderer)
        return;

    ALWAYS_LOG(LOGIDENTIFIER);

    m_sampleBufferVideoRenderer = adoptNS([PAL::allocAVSampleBufferVideoRendererInstance() init]);
    if (!m_sampleBufferVideoRenderer)
        return;

    [m_sampleBufferVideoRenderer addVideoTarget:m_videoTarget.get()];

    configureLayerOrVideoRenderer(m_sampleBufferVideoRenderer.get());
#endif // ENABLE(LINEAR_MEDIA_PLAYER)
}

void MediaPlayerPrivateWebM::destroyVideoRenderer()
{
#if ENABLE(LINEAR_MEDIA_PLAYER)
    if (!m_sampleBufferVideoRenderer)
        return;

    ALWAYS_LOG(LOGIDENTIFIER);

    CMTime currentTime = PAL::CMTimebaseGetTime([m_synchronizer timebase]);
    [m_synchronizer removeRenderer:m_sampleBufferVideoRenderer.get() atTime:currentTime completionHandler:nil];

    if ([m_sampleBufferVideoRenderer respondsToSelector:@selector(removeAllVideoTargets)])
        [m_sampleBufferVideoRenderer removeAllVideoTargets];
    m_sampleBufferVideoRenderer = nullptr;
#endif // ENABLE(LINEAR_MEDIA_PLAYER)
}

void MediaPlayerPrivateWebM::destroyAudioRenderer(RetainPtr<AVSampleBufferAudioRenderer> renderer)
{
    CMTime currentTime = PAL::CMTimebaseGetTime([m_synchronizer timebase]);
    [m_synchronizer removeRenderer:renderer.get() atTime:currentTime completionHandler:nil];

    m_listener->stopObservingAudioRenderer(renderer.get());
    [renderer flush];
    [renderer stopRequestingMediaData];
}

void MediaPlayerPrivateWebM::destroyAudioRenderers()
{
    for (auto& pair : m_audioRenderers) {
        auto& renderer = pair.second;
        destroyAudioRenderer(renderer);
    }
    m_audioRenderers.clear();
}

void MediaPlayerPrivateWebM::clearTracks()
{
    auto player = m_player.get();
    for (auto& track : m_videoTracks) {
        track->setSelectedChangedCallback(nullptr);
        if (player)
            player->removeVideoTrack(*track);
    }
    m_videoTracks.clear();

    for (auto& track : m_audioTracks) {
        track->setEnabledChangedCallback(nullptr);
        if (player)
            player->removeAudioTrack(*track);
    }
    m_audioTracks.clear();
}

void MediaPlayerPrivateWebM::setVideoFrameMetadataGatheringCallbackIfNeeded(VideoMediaSampleRenderer& videoRenderer)
{
    if (!m_isGatheringVideoFrameMetadata)
        return;
    videoRenderer.notifyWhenHasAvailableVideoFrame([weakThis = WeakPtr { *this }](const MediaTime& presentationTime, double displayTime) {
        if (RefPtr protectedThis = weakThis.get())
            protectedThis->checkNewVideoFrameMetadata(presentationTime, displayTime);
    });
}

void MediaPlayerPrivateWebM::startVideoFrameMetadataGathering()
{
    ASSERT(m_synchronizer);
    m_isGatheringVideoFrameMetadata = true;
    if (RefPtr videoRenderer = m_videoRenderer)
        setVideoFrameMetadataGatheringCallbackIfNeeded(*videoRenderer);
}

void MediaPlayerPrivateWebM::stopVideoFrameMetadataGathering()
{
    m_isGatheringVideoFrameMetadata = false;
    m_videoFrameMetadata = { };
    if (RefPtr videoRenderer = m_videoRenderer)
        videoRenderer->notifyWhenHasAvailableVideoFrame(nullptr);
}

void MediaPlayerPrivateWebM::checkNewVideoFrameMetadata(const MediaTime& presentationTime, double displayTime)
{
    auto player = m_player.get();
    if (!player)
        return;

    if (!updateLastPixelBuffer())
        return;

#ifndef NDEBUG
    if (m_lastPixelBufferPresentationTimeStamp != presentationTime)
        ALWAYS_LOG(LOGIDENTIFIER, "notification of new frame delayed retrieved:", m_lastPixelBufferPresentationTimeStamp, " expected:", presentationTime);
#endif
    VideoFrameMetadata metadata;
    metadata.width = m_naturalSize.width();
    metadata.height = m_naturalSize.height();
    metadata.presentedFrames = protectedVideoRenderer()->totalDisplayedFrames();
    metadata.presentationTime = displayTime;
    metadata.expectedDisplayTime = displayTime;
    metadata.mediaTime = (m_lastPixelBufferPresentationTimeStamp.isValid() ? m_lastPixelBufferPresentationTimeStamp : presentationTime).toDouble();

    m_videoFrameMetadata = metadata;
    player->onNewVideoFrameMetadata(WTFMove(metadata), m_lastPixelBuffer.get());
}

WTFLogChannel& MediaPlayerPrivateWebM::logChannel() const
{
    return LogMedia;
}

class MediaPlayerFactoryWebM final : public MediaPlayerFactory {
    WTF_MAKE_TZONE_ALLOCATED_INLINE(MediaPlayerFactoryWebM);
private:
    MediaPlayerEnums::MediaEngineIdentifier identifier() const final { return MediaPlayerEnums::MediaEngineIdentifier::CocoaWebM; };

    Ref<MediaPlayerPrivateInterface> createMediaEnginePlayer(MediaPlayer* player) const final
    {
        return adoptRef(*new MediaPlayerPrivateWebM(player));
    }

    void getSupportedTypes(HashSet<String>& types) const final
    {
        return MediaPlayerPrivateWebM::getSupportedTypes(types);
    }

    MediaPlayer::SupportsType supportsTypeAndCodecs(const MediaEngineSupportParameters& parameters) const final
    {
        return MediaPlayerPrivateWebM::supportsType(parameters);
    }
};

void MediaPlayerPrivateWebM::registerMediaEngine(MediaEngineRegistrar registrar)
{
    if (!isAvailable())
        return;

    registrar(makeUnique<MediaPlayerFactoryWebM>());
}

bool MediaPlayerPrivateWebM::isAvailable()
{
    return SourceBufferParserWebM::isAvailable()
        && PAL::isAVFoundationFrameworkAvailable()
        && PAL::isCoreMediaFrameworkAvailable()
        && PAL::getAVSampleBufferAudioRendererClass()
        && PAL::getAVSampleBufferRenderSynchronizerClass()
        && class_getInstanceMethod(PAL::getAVSampleBufferAudioRendererClass(), @selector(setMuted:));
}

bool MediaPlayerPrivateWebM::isEnabledVideoTrackID(TrackID trackID) const
{
    return m_enabledVideoTrackID && *m_enabledVideoTrackID == trackID;
}

bool MediaPlayerPrivateWebM::hasSelectedVideo() const
{
    return !!m_enabledVideoTrackID;
}

void MediaPlayerPrivateWebM::audioRendererDidReceiveError(AVSampleBufferAudioRenderer *, NSError*)
{
    setNetworkState(MediaPlayer::NetworkState::DecodeError);
    setReadyState(MediaPlayer::ReadyState::HaveNothing);
    m_errored = true;
}

void MediaPlayerPrivateWebM::ensureLayerOrVideoRenderer(MediaPlayerEnums::NeedsRenderingModeChanged needsRenderingModeChanged)
{
    switch (acceleratedVideoMode()) {
    case AcceleratedVideoMode::Layer:
        ensureLayer();
        break;
    case AcceleratedVideoMode::VideoRenderer:
        ensureVideoRenderer();
        break;
    }

    RetainPtr renderer = layerOrVideoRenderer();

    if (!renderer) {
        ERROR_LOG(LOGIDENTIFIER, "Failed to create AVSampleBufferDisplayLayer or AVSampleBufferVideoRenderer");
        setNetworkState(MediaPlayer::NetworkState::DecodeError);
        return;
    }

    ALWAYS_LOG(LOGIDENTIFIER, acceleratedVideoMode(), ", renderer=", !!renderer);

    switch (acceleratedVideoMode()) {
    case AcceleratedVideoMode::Layer:
#if ENABLE(LINEAR_MEDIA_PLAYER)
        if (!m_usingLinearMediaPlayer)
            needsRenderingModeChanged = MediaPlayerEnums::NeedsRenderingModeChanged::Yes;
#else
        needsRenderingModeChanged = MediaPlayerEnums::NeedsRenderingModeChanged::Yes;
#endif
        [[fallthrough]];
    case AcceleratedVideoMode::VideoRenderer:
        setVideoRenderer(renderer.get());
        break;
    }

    switch (needsRenderingModeChanged) {
    case MediaPlayerEnums::NeedsRenderingModeChanged::Yes:
        if (RefPtr player = m_player.get())
            player->renderingModeChanged();
        break;
    case MediaPlayerEnums::NeedsRenderingModeChanged::No:
        break;
    }
}

void MediaPlayerPrivateWebM::setShouldDisableHDR(bool shouldDisable)
{
    if (![m_sampleBufferDisplayLayer respondsToSelector:@selector(setToneMapToStandardDynamicRange:)])
        return;

    ALWAYS_LOG(LOGIDENTIFIER, shouldDisable);
    [m_sampleBufferDisplayLayer setToneMapToStandardDynamicRange:shouldDisable];
}

void MediaPlayerPrivateWebM::setPlatformDynamicRangeLimit(PlatformDynamicRangeLimit platformDynamicRangeLimit)
{
    if (!m_sampleBufferDisplayLayer)
        return;

    setLayerDynamicRangeLimit(m_sampleBufferDisplayLayer.get(), platformDynamicRangeLimit);
}

void MediaPlayerPrivateWebM::playerContentBoxRectChanged(const LayoutRect& newRect)
{
    if (!layerOrVideoRenderer() && !newRect.isEmpty())
        updateDisplayLayer();
}

void MediaPlayerPrivateWebM::setShouldMaintainAspectRatio(bool shouldMaintainAspectRatio)
{
    if (m_shouldMaintainAspectRatio == shouldMaintainAspectRatio)
        return;

    m_shouldMaintainAspectRatio = shouldMaintainAspectRatio;
    if (!m_sampleBufferDisplayLayer)
        return;

    [CATransaction begin];
    [CATransaction setAnimationDuration:0];
    [CATransaction setDisableActions:YES];

    [m_sampleBufferDisplayLayer setVideoGravity: (m_shouldMaintainAspectRatio ? AVLayerVideoGravityResizeAspect : AVLayerVideoGravityResize)];

    [CATransaction commit];
}

#if HAVE(SPATIAL_TRACKING_LABEL)
const String& MediaPlayerPrivateWebM::defaultSpatialTrackingLabel() const
{
    return m_defaultSpatialTrackingLabel;
}

void MediaPlayerPrivateWebM::setDefaultSpatialTrackingLabel(const String& defaultSpatialTrackingLabel)
{
    if (m_defaultSpatialTrackingLabel == defaultSpatialTrackingLabel)
        return;
    m_defaultSpatialTrackingLabel = defaultSpatialTrackingLabel;
    updateSpatialTrackingLabel();
}

const String& MediaPlayerPrivateWebM::spatialTrackingLabel() const
{
    return m_spatialTrackingLabel;
}

void MediaPlayerPrivateWebM::setSpatialTrackingLabel(const String& spatialTrackingLabel)
{
    if (m_spatialTrackingLabel == spatialTrackingLabel)
        return;
    m_spatialTrackingLabel = spatialTrackingLabel;
    updateSpatialTrackingLabel();
}

void MediaPlayerPrivateWebM::updateSpatialTrackingLabel()
{
    auto *renderer = m_sampleBufferVideoRenderer ? m_sampleBufferVideoRenderer.get() : [m_sampleBufferDisplayLayer sampleBufferRenderer];

#if HAVE(SPATIAL_AUDIO_EXPERIENCE)
    if (RefPtr player = m_player.get(); player && player->prefersSpatialAudioExperience()) {
        RetainPtr experience = createSpatialAudioExperienceWithOptions({
            .hasLayer = !!renderer,
            .hasTarget = !!m_videoTarget,
            .isVisible = m_visible,
            .soundStageSize = player->soundStageSize(),
            .sceneIdentifier = player->sceneIdentifier(),
#if HAVE(SPATIAL_TRACKING_LABEL)
            .spatialTrackingLabel = m_spatialTrackingLabel,
#endif
        });
        [m_synchronizer setIntendedSpatialAudioExperience:experience.get()];
        return;
    }
#endif

    if (!m_spatialTrackingLabel.isNull()) {
        INFO_LOG(LOGIDENTIFIER, "Explicitly set STSLabel: ", m_spatialTrackingLabel);
        renderer.STSLabel = m_spatialTrackingLabel.createNSString().get();
        return;
    }

    if (renderer && m_visible) {
        // Let AVSBRS manage setting the spatial tracking label in its video renderer itself.
        INFO_LOG(LOGIDENTIFIER, "Has visible renderer, set STSLabel: nil");
        renderer.STSLabel = nil;
        return;
    }

    if (m_audioRenderers.empty()) {
        // If there are no audio renderers, there's nothing to do.
        INFO_LOG(LOGIDENTIFIER, "No audio renderers - no-op");
        return;
    }

    // If there is no video renderer, use the default spatial tracking label if available, or
    // the session's spatial tracking label if not, and set the label directly on each audio
    // renderer.
    AVAudioSession *session = [PAL::getAVAudioSessionClass() sharedInstance];
    RetainPtr<NSString> defaultLabel;
    if (!m_defaultSpatialTrackingLabel.isNull()) {
        INFO_LOG(LOGIDENTIFIER, "Default STSLabel: ", m_defaultSpatialTrackingLabel);
        defaultLabel = m_defaultSpatialTrackingLabel.createNSString();
    } else {
        INFO_LOG(LOGIDENTIFIER, "AVAudioSession label: ", session.spatialTrackingLabel);
        defaultLabel = session.spatialTrackingLabel;
    }
    for (auto& renderer : m_audioRenderers)
        [(__bridge AVSampleBufferAudioRenderer *)renderer.second.get() setSTSLabel:defaultLabel.get()];
}
#endif

void MediaPlayerPrivateWebM::destroyLayerOrVideoRendererAndCreateRenderlessVideoMediaSampleRenderer()
{
    setVideoRenderer(nil);

    if (RefPtr player = m_player.get())
        player->renderingModeChanged();
}

void MediaPlayerPrivateWebM::configureLayerOrVideoRenderer(WebSampleBufferVideoRendering *renderer)
{
#if HAVE(SPATIAL_TRACKING_LABEL)
    updateSpatialTrackingLabel();
#endif

    renderer.preventsDisplaySleepDuringVideoPlayback = NO;

    if ([renderer respondsToSelector:@selector(setPreventsAutomaticBackgroundingDuringVideoPlayback:)])
        renderer.preventsAutomaticBackgroundingDuringVideoPlayback = NO;

    @try {
        [m_synchronizer addRenderer:renderer];
    } @catch(NSException *exception) {
        ERROR_LOG(LOGIDENTIFIER, "-[AVSampleBufferRenderSynchronizer addRenderer:] threw an exception: ", exception.name, ", reason : ", exception.reason);
        ASSERT_NOT_REACHED();

        setNetworkState(MediaPlayer::NetworkState::DecodeError);
        return;
    }
}

void MediaPlayerPrivateWebM::configureVideoRenderer(VideoMediaSampleRenderer& videoRenderer)
{
    videoRenderer.setResourceOwner(m_resourceOwner);
}

void MediaPlayerPrivateWebM::invalidateVideoRenderer(VideoMediaSampleRenderer& videoRenderer)
{
    videoRenderer.flush();
    videoRenderer.stopRequestingMediaData();
    videoRenderer.notifyWhenVideoRendererRequiresFlushToResumeDecoding({ });
}

RefPtr<VideoMediaSampleRenderer> MediaPlayerPrivateWebM::protectedVideoRenderer() const
{
    return m_videoRenderer;
}

void MediaPlayerPrivateWebM::setVideoRenderer(WebSampleBufferVideoRendering *renderer)
{
    ALWAYS_LOG(LOGIDENTIFIER, "!!renderer = ", !!renderer);

    if (m_videoRenderer)
        return stageVideoRenderer(renderer);

    RefPtr videoRenderer = VideoMediaSampleRenderer::create(renderer);
    m_videoRenderer = videoRenderer;
    videoRenderer->setPreferences(VideoMediaSampleRendererPreference::PrefersDecompressionSession);
    videoRenderer->setTimebase([m_synchronizer timebase]);
    videoRenderer->notifyWhenDecodingErrorOccurred([weakThis = WeakPtr { *this }](NSError *) {
        RefPtr protectedThis = weakThis.get();
        if (!protectedThis)
            return;
        protectedThis->setNetworkState(MediaPlayer::NetworkState::DecodeError);
        protectedThis->setReadyState(MediaPlayer::ReadyState::HaveNothing);
        protectedThis->m_errored = true;
    });
    videoRenderer->notifyFirstFrameAvailable([weakThis = WeakPtr { *this }](const MediaTime&, double) {
        if (RefPtr protectedThis = weakThis.get())
            protectedThis->setHasAvailableVideoFrame(true);
    });
    setVideoFrameMetadataGatheringCallbackIfNeeded(*videoRenderer);
    videoRenderer->notifyWhenVideoRendererRequiresFlushToResumeDecoding([weakThis = ThreadSafeWeakPtr { *this }] {
        if (RefPtr protectedThis = weakThis.get())
            protectedThis->setLayerRequiresFlush();
    });
    configureVideoRenderer(*videoRenderer);
    if (m_enabledVideoTrackID)
        reenqueSamples(*m_enabledVideoTrackID, NeedsFlush::No);
}

void MediaPlayerPrivateWebM::stageVideoRenderer(WebSampleBufferVideoRendering *renderer)
{
    ASSERT(m_videoRenderer);

    RefPtr videoRenderer = m_videoRenderer;
    if (renderer == videoRenderer->renderer())
        return;

    ALWAYS_LOG(LOGIDENTIFIER, "!!renderer = ", !!renderer);
    ASSERT(!renderer || hasSelectedVideo());

    Vector<RetainPtr<WebSampleBufferVideoRendering>> renderersToExpire { 2u };
    if (renderer) {
        switch (acceleratedVideoMode()) {
        case AcceleratedVideoMode::Layer:
            renderersToExpire.append(std::exchange(m_sampleBufferVideoRenderer, { }));
            m_needsDestroyVideoLayer = true;
            break;
        case AcceleratedVideoMode::VideoRenderer:
            if (m_sampleBufferDisplayLayer)
                m_videoLayerManager->didDestroyVideoLayer();
            renderersToExpire.append(std::exchange(m_sampleBufferDisplayLayer, { }));
            break;
        }
    } else {
        renderersToExpire.append(m_sampleBufferVideoRenderer);
        renderersToExpire.append(m_sampleBufferDisplayLayer);
    }

    videoRenderer->changeRenderer(renderer)->whenSettled(RunLoop::mainSingleton(), [weakThis = ThreadSafeWeakPtr { *this }, renderersToExpire = WTFMove(renderersToExpire)] {
        for (auto& rendererToExpire : renderersToExpire) {
            if (!rendererToExpire)
                continue;
            if (RefPtr protectedThis = weakThis.get()) {
                CMTime currentTime = PAL::CMTimebaseGetTime([protectedThis->m_synchronizer timebase]);
                [protectedThis->m_synchronizer removeRenderer:rendererToExpire.get() atTime:currentTime completionHandler:nil];
            }
#if ENABLE(LINEAR_MEDIA_PLAYER)
            if (RetainPtr videoRenderer = dynamic_objc_cast<AVSampleBufferVideoRenderer>(rendererToExpire.get())) {
                [videoRenderer respondsToSelector:@selector(removeAllVideoTargets)];
                [videoRenderer removeAllVideoTargets];
            }
#endif
        }
    });
}

MediaPlayerPrivateWebM::AcceleratedVideoMode MediaPlayerPrivateWebM::acceleratedVideoMode() const
{
#if ENABLE(LINEAR_MEDIA_PLAYER)
    if (m_videoTarget)
        return AcceleratedVideoMode::VideoRenderer;
#endif // ENABLE(LINEAR_MEDIA_PLAYER)

    return AcceleratedVideoMode::Layer;
}

WebSampleBufferVideoRendering *MediaPlayerPrivateWebM::layerOrVideoRenderer() const
{
#if ENABLE(LINEAR_MEDIA_PLAYER)
    switch (acceleratedVideoMode()) {
    case AcceleratedVideoMode::Layer:
        return m_sampleBufferDisplayLayer.get();
    case AcceleratedVideoMode::VideoRenderer:
        return m_sampleBufferVideoRenderer.get();
    }
#else
    return m_sampleBufferDisplayLayer.get();
#endif
}

#if ENABLE(LINEAR_MEDIA_PLAYER)
void MediaPlayerPrivateWebM::setVideoTarget(const PlatformVideoTarget& videoTarget)
{
    ALWAYS_LOG(LOGIDENTIFIER, !!videoTarget);
    m_usingLinearMediaPlayer = !!videoTarget;
    m_videoTarget = videoTarget;
    updateDisplayLayer();
}
#endif

#if PLATFORM(IOS_FAMILY)
void MediaPlayerPrivateWebM::sceneIdentifierDidChange()
{
#if HAVE(SPATIAL_TRACKING_LABEL)
    updateSpatialTrackingLabel();
#endif
}

void MediaPlayerPrivateWebM::applicationWillResignActive()
{
    ALWAYS_LOG(LOGIDENTIFIER);
    m_applicationIsActive = false;
}

void MediaPlayerPrivateWebM::applicationDidBecomeActive()
{
    ALWAYS_LOG(LOGIDENTIFIER);
    m_applicationIsActive = true;
    flushIfNeeded();
}
#endif

void MediaPlayerPrivateWebM::isInFullscreenOrPictureInPictureChanged(bool isInFullscreenOrPictureInPicture)
{
#if ENABLE(LINEAR_MEDIA_PLAYER)
    ALWAYS_LOG(LOGIDENTIFIER, isInFullscreenOrPictureInPicture);
    if (!m_usingLinearMediaPlayer)
        return;
    destroyVideoLayerIfNeeded();
    updateDisplayLayer();
#else
    UNUSED_PARAM(isInFullscreenOrPictureInPicture);
#endif
}

void MediaPlayerPrivateWebM::setLayerRequiresFlush()
{
    ALWAYS_LOG(LOGIDENTIFIER);
#if PLATFORM(IOS_FAMILY)
    m_layerRequiresFlush = true;
    if (m_applicationIsActive)
        flushIfNeeded();
#else
    flushIfNeeded();
#endif
}

std::optional<VideoPlaybackQualityMetrics> MediaPlayerPrivateWebM::videoPlaybackQualityMetrics()
{
    RefPtr videoRenderer = m_videoRenderer;
    if (!videoRenderer)
        return std::nullopt;

    return VideoPlaybackQualityMetrics {
        videoRenderer->totalVideoFrames(),
        videoRenderer->droppedVideoFrames(),
        videoRenderer->corruptedVideoFrames(),
        videoRenderer->totalFrameDelay().toDouble(),
        videoRenderer->totalDisplayedFrames()
    };
}

} // namespace WebCore

#endif // ENABLE(COCOA_WEBM_PLAYER)
