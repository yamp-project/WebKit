/*
 * Copyright (C) 2013-2024 Apple Inc. All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#import "config.h"
#import "MediaPlayerPrivateMediaSourceAVFObjC.h"

#if ENABLE(MEDIA_SOURCE) && USE(AVFOUNDATION)

#import "AVAssetMIMETypeCache.h"
#import "AVAssetTrackUtilities.h"
#import "AVStreamDataParserMIMETypeCache.h"
#import "AudioMediaStreamTrackRenderer.h"
#import "CDMSessionAVContentKeySession.h"
#import "ContentTypeUtilities.h"
#import "EffectiveRateChangedListener.h"
#import "GraphicsContext.h"
#import "IOSurface.h"
#import "Logging.h"
#import "MediaSessionManagerCocoa.h"
#import "MediaSourcePrivate.h"
#import "MediaSourcePrivateAVFObjC.h"
#import "MediaSourcePrivateClient.h"
#import "MessageClientForTesting.h"
#import "MessageForTesting.h"
#import "PixelBufferConformerCV.h"
#import "PlatformDynamicRangeLimitCocoa.h"
#import "PlatformScreen.h"
#import "SourceBufferPrivateAVFObjC.h"
#import "SpatialAudioExperienceHelper.h"
#import "TextTrackRepresentation.h"
#import "VideoFrameCV.h"
#import "VideoLayerManagerObjC.h"
#import "VideoMediaSampleRenderer.h"
#import "WebSampleBufferVideoRendering.h"
#import <AVFoundation/AVFoundation.h>
#import <CoreMedia/CMTime.h>
#import <QuartzCore/CALayer.h>
#import <objc_runtime.h>
#import <pal/avfoundation/MediaTimeAVFoundation.h>
#import <pal/spi/cf/CFNotificationCenterSPI.h>
#import <pal/spi/cocoa/AVFoundationSPI.h>
#import <pal/spi/cocoa/QuartzCoreSPI.h>
#import <wtf/BlockObjCExceptions.h>
#import <wtf/BlockPtr.h>
#import <wtf/Deque.h>
#import <wtf/FileSystem.h>
#import <wtf/MachSendRightAnnotated.h>
#import <wtf/MainThread.h>
#import <wtf/NativePromise.h>
#import <wtf/NeverDestroyed.h>
#import <wtf/TZoneMallocInlines.h>
#import <wtf/WeakPtr.h>
#import <wtf/darwin/DispatchExtras.h>
#import <wtf/spi/cocoa/NSObjCRuntimeSPI.h>

#import "CoreVideoSoftLink.h"
#import <pal/cf/CoreMediaSoftLink.h>
#import <pal/cocoa/AVFoundationSoftLink.h>
#import <pal/cocoa/MediaToolboxSoftLink.h>

@interface AVSampleBufferDisplayLayer (Staging_100128644)
@property (assign, nonatomic) BOOL preventsAutomaticBackgroundingDuringVideoPlayback;
@end

namespace WebCore {

String convertEnumerationToString(MediaPlayerPrivateMediaSourceAVFObjC::SeekState enumerationValue)
{
    static const std::array<NeverDestroyed<String>, 3> values {
        MAKE_STATIC_STRING_IMPL("Seeking"),
        MAKE_STATIC_STRING_IMPL("WaitingForAvailableFame"),
        MAKE_STATIC_STRING_IMPL("SeekCompleted"),
    };
    static_assert(!static_cast<size_t>(MediaPlayerPrivateMediaSourceAVFObjC::SeekState::Seeking), "MediaPlayerPrivateMediaSourceAVFObjC::SeekState::Seeking is not 0 as expected");
    static_assert(static_cast<size_t>(MediaPlayerPrivateMediaSourceAVFObjC::SeekState::WaitingForAvailableFame) == 1, "MediaPlayerPrivateMediaSourceAVFObjC::SeekState::WaitingForAvailableFame is not 1 as expected");
    static_assert(static_cast<size_t>(MediaPlayerPrivateMediaSourceAVFObjC::SeekState::SeekCompleted) == 2, "MediaPlayerPrivateMediaSourceAVFObjC::SeekState::SeekCompleted is not 2 as expected");
    ASSERT(static_cast<size_t>(enumerationValue) < std::size(values));
    return values[static_cast<size_t>(enumerationValue)];
}

#pragma mark -
#pragma mark MediaPlayerPrivateMediaSourceAVFObjC

MediaPlayerPrivateMediaSourceAVFObjC::MediaPlayerPrivateMediaSourceAVFObjC(MediaPlayer* player)
    : m_player(player)
    , m_synchronizer(adoptNS([PAL::allocAVSampleBufferRenderSynchronizerInstance() init]))
    , m_seekTimer(*this, &MediaPlayerPrivateMediaSourceAVFObjC::seekInternal)
    , m_networkState(MediaPlayer::NetworkState::Empty)
    , m_readyState(MediaPlayer::ReadyState::HaveNothing)
    , m_logger(player->mediaPlayerLogger())
    , m_logIdentifier(player->mediaPlayerLogIdentifier())
    , m_videoLayerManager(makeUniqueRef<VideoLayerManagerObjC>(m_logger, m_logIdentifier))
    , m_effectiveRateChangedListener(EffectiveRateChangedListener::create([weakThis = WeakPtr { *this }](double) {
        callOnMainThread([weakThis] {
            if (RefPtr protectedThis = weakThis.get())
                protectedThis->effectiveRateChanged();
        });
    }, [m_synchronizer timebase]))
{
    auto logSiteIdentifier = LOGIDENTIFIER;
    ALWAYS_LOG(logSiteIdentifier);
    UNUSED_PARAM(logSiteIdentifier);

#if HAVE(SPATIAL_TRACKING_LABEL)
    m_defaultSpatialTrackingLabel = player->defaultSpatialTrackingLabel();
    m_spatialTrackingLabel = player->spatialTrackingLabel();
#endif

    // addPeriodicTimeObserverForInterval: throws an exception if you pass a non-numeric CMTime, so just use
    // an arbitrarily large time value of once an hour:
    __block WeakPtr weakThis { *this };
    m_timeJumpedObserver = [m_synchronizer addPeriodicTimeObserverForInterval:PAL::toCMTime(MediaTime::createWithDouble(3600)) queue:mainDispatchQueueSingleton() usingBlock:^(CMTime time) {
#if LOG_DISABLED
        UNUSED_PARAM(time);
#endif
        if (!weakThis)
            return;

        auto clampedTime = CMTIME_IS_NUMERIC(time) ? clampTimeToSensicalValue(PAL::toMediaTime(time)) : MediaTime::zeroTime();
        ALWAYS_LOG(logSiteIdentifier, "synchronizer fired: time clamped = ", clampedTime, ", seeking = ", m_isSynchronizerSeeking, ", pending = ", !!m_pendingSeek);

        if (m_isSynchronizerSeeking && !m_pendingSeek) {
            m_isSynchronizerSeeking = false;
            maybeCompleteSeek();
        }

        if (m_pendingSeek)
            seekInternal();

        if (m_currentTimeDidChangeCallback)
            m_currentTimeDidChangeCallback(clampedTime);
    }];

#if ENABLE(LINEAR_MEDIA_PLAYER)
    RetainPtr videoTarget = player->videoTarget();
    m_acceleratedVideoMode = videoTarget ? AcceleratedVideoMode::VideoRenderer : AcceleratedVideoMode::Layer;
    setVideoTarget(videoTarget.get());
#endif
}

MediaPlayerPrivateMediaSourceAVFObjC::~MediaPlayerPrivateMediaSourceAVFObjC()
{
    ALWAYS_LOG(LOGIDENTIFIER);

    m_effectiveRateChangedListener->stop();

    if (m_timeJumpedObserver)
        [m_synchronizer removeTimeObserver:m_timeJumpedObserver.get()];
    if (m_timeChangedObserver)
        [m_synchronizer removeTimeObserver:m_timeChangedObserver.get()];
    if (m_videoFrameMetadataGatheringObserver)
        [m_synchronizer removeTimeObserver:m_videoFrameMetadataGatheringObserver.get()];
    if (m_gapObserver)
        [m_synchronizer removeTimeObserver:m_gapObserver.get()];
    flushPendingSizeChanges();

    destroyLayer();

    m_seekTimer.stop();
}

#pragma mark -
#pragma mark MediaPlayer Factory Methods

class MediaPlayerFactoryMediaSourceAVFObjC final : public MediaPlayerFactory {
    WTF_MAKE_TZONE_ALLOCATED_INLINE(MediaPlayerFactoryMediaSourceAVFObjC);
private:
    MediaPlayerEnums::MediaEngineIdentifier identifier() const final { return MediaPlayerEnums::MediaEngineIdentifier::AVFoundationMSE; };

    Ref<MediaPlayerPrivateInterface> createMediaEnginePlayer(MediaPlayer* player) const final
    {
        return adoptRef(*new MediaPlayerPrivateMediaSourceAVFObjC(player));
    }

    void getSupportedTypes(HashSet<String>& types) const final
    {
        return MediaPlayerPrivateMediaSourceAVFObjC::getSupportedTypes(types);
    }

    MediaPlayer::SupportsType supportsTypeAndCodecs(const MediaEngineSupportParameters& parameters) const final
    {
        return MediaPlayerPrivateMediaSourceAVFObjC::supportsTypeAndCodecs(parameters);
    }
};

void MediaPlayerPrivateMediaSourceAVFObjC::registerMediaEngine(MediaEngineRegistrar registrar)
{
    if (!isAvailable())
        return;

    ASSERT(AVAssetMIMETypeCache::singleton().isAvailable());

    registrar(makeUnique<MediaPlayerFactoryMediaSourceAVFObjC>());
}

bool MediaPlayerPrivateMediaSourceAVFObjC::isAvailable()
{
    return PAL::isAVFoundationFrameworkAvailable()
        && PAL::isCoreMediaFrameworkAvailable()
        && PAL::getAVStreamDataParserClassSingleton()
        && PAL::getAVSampleBufferAudioRendererClassSingleton()
        && PAL::getAVSampleBufferRenderSynchronizerClassSingleton()
        && class_getInstanceMethod(PAL::getAVSampleBufferAudioRendererClassSingleton(), @selector(setMuted:));
}

void MediaPlayerPrivateMediaSourceAVFObjC::getSupportedTypes(HashSet<String>& types)
{
    types = AVStreamDataParserMIMETypeCache::singleton().supportedTypes();
}

MediaPlayer::SupportsType MediaPlayerPrivateMediaSourceAVFObjC::supportsTypeAndCodecs(const MediaEngineSupportParameters& parameters)
{
    // This engine does not support non-media-source sources.
    if (!parameters.isMediaSource)
        return MediaPlayer::SupportsType::IsNotSupported;

    if (!contentTypeMeetsContainerAndCodecTypeRequirements(parameters.type, parameters.allowedMediaContainerTypes, parameters.allowedMediaCodecTypes))
        return MediaPlayer::SupportsType::IsNotSupported;

    auto supported = SourceBufferParser::isContentTypeSupported(parameters.type);

    if (supported != MediaPlayer::SupportsType::IsSupported)
        return supported;

    if (!contentTypeMeetsHardwareDecodeRequirements(parameters.type, parameters.contentTypesRequiringHardwareSupport))
        return MediaPlayer::SupportsType::IsNotSupported;

    return MediaPlayer::SupportsType::IsSupported;
}

#pragma mark -
#pragma mark MediaPlayerPrivateInterface Overrides

void MediaPlayerPrivateMediaSourceAVFObjC::load(const String&)
{
    // This media engine only supports MediaSource URLs.
    m_networkState = MediaPlayer::NetworkState::FormatError;
    if (auto player = m_player.get())
        player->networkStateChanged();
}

void MediaPlayerPrivateMediaSourceAVFObjC::load(const URL&, const LoadOptions& options, MediaSourcePrivateClient& client)
{
    ALWAYS_LOG(LOGIDENTIFIER);

    if (RefPtr mediaSourcePrivate = downcast<MediaSourcePrivateAVFObjC>(client.mediaSourcePrivate())) {
        mediaSourcePrivate->setPlayer(this);
        m_mediaSourcePrivate = WTFMove(mediaSourcePrivate);
        client.reOpen();
    } else
        m_mediaSourcePrivate = MediaSourcePrivateAVFObjC::create(*this, client);

    m_loadOptions = options;

    RefPtr mediaSourcePrivate = m_mediaSourcePrivate;
    mediaSourcePrivate->setResourceOwner(m_resourceOwner);
    if (canUseDecompressionSession())
        setVideoRenderer(acceleratedVideoMode() == AcceleratedVideoMode::Layer ? m_sampleBufferDisplayLayer.get() : static_cast<WebSampleBufferVideoRendering *>(m_sampleBufferVideoRenderer.get()));
    else
        mediaSourcePrivate->setVideoRenderer(layerOrVideoRenderer().get());

    acceleratedRenderingStateChanged();
}

#if ENABLE(MEDIA_STREAM)
void MediaPlayerPrivateMediaSourceAVFObjC::load(MediaStreamPrivate&)
{
    setNetworkState(MediaPlayer::NetworkState::FormatError);
}
#endif

void MediaPlayerPrivateMediaSourceAVFObjC::cancelLoad()
{
}

void MediaPlayerPrivateMediaSourceAVFObjC::prepareToPlay()
{
}

PlatformLayer* MediaPlayerPrivateMediaSourceAVFObjC::platformLayer() const
{
    return m_videoLayerManager->videoInlineLayer();
}

void MediaPlayerPrivateMediaSourceAVFObjC::play()
{
    ALWAYS_LOG(LOGIDENTIFIER);
    playInternal();
}

void MediaPlayerPrivateMediaSourceAVFObjC::playInternal(std::optional<MonotonicTime>&& hostTime)
{
    RefPtr mediaSourcePrivate = m_mediaSourcePrivate;
    if (!mediaSourcePrivate)
        return;

    if (currentTime() >= mediaSourcePrivate->duration()) {
        ALWAYS_LOG(LOGIDENTIFIER, "bailing, current time: ", currentTime(), " greater than duration ", mediaSourcePrivate->duration());
        return;
    }

    ALWAYS_LOG(LOGIDENTIFIER);
    mediaSourcePrivate->flushActiveSourceBuffersIfNeeded();
    m_isPlaying = true;
    if (!shouldBePlaying())
        return;

    setSynchronizerRate(m_rate, WTFMove(hostTime));
}

void MediaPlayerPrivateMediaSourceAVFObjC::pause()
{
    ALWAYS_LOG(LOGIDENTIFIER);
    pauseInternal();
}

void MediaPlayerPrivateMediaSourceAVFObjC::pauseInternal(std::optional<MonotonicTime>&& hostTime)
{
    ALWAYS_LOG(LOGIDENTIFIER);
    m_isPlaying = false;

    setSynchronizerRate(0, WTFMove(hostTime));
}

bool MediaPlayerPrivateMediaSourceAVFObjC::paused() const
{
    return !m_isPlaying;
}

void MediaPlayerPrivateMediaSourceAVFObjC::setVolume(float volume)
{
    ALWAYS_LOG(LOGIDENTIFIER, volume);
    for (const auto& key : m_sampleBufferAudioRendererMap.keys())
        [(__bridge AVSampleBufferAudioRenderer *)key.get() setVolume:volume];
}

bool MediaPlayerPrivateMediaSourceAVFObjC::supportsScanning() const
{
    return true;
}

void MediaPlayerPrivateMediaSourceAVFObjC::setMuted(bool muted)
{
    ALWAYS_LOG(LOGIDENTIFIER, muted);
    for (const auto& key : m_sampleBufferAudioRendererMap.keys())
        [(__bridge AVSampleBufferAudioRenderer *)key.get() setMuted:muted];
}

FloatSize MediaPlayerPrivateMediaSourceAVFObjC::naturalSize() const
{
    return m_naturalSize;
}

bool MediaPlayerPrivateMediaSourceAVFObjC::hasVideo() const
{
    RefPtr mediaSourcePrivate = m_mediaSourcePrivate;
    return mediaSourcePrivate && mediaSourcePrivate->hasVideo();
}

bool MediaPlayerPrivateMediaSourceAVFObjC::hasAudio() const
{
    RefPtr mediaSourcePrivate = m_mediaSourcePrivate;
    return mediaSourcePrivate && mediaSourcePrivate->hasAudio();
}

void MediaPlayerPrivateMediaSourceAVFObjC::setPageIsVisible(bool visible)
{
    if (m_visible == visible)
        return;

    ALWAYS_LOG(LOGIDENTIFIER, visible);
    m_visible = visible;
    if (m_visible) {
        acceleratedRenderingStateChanged();

        // Rendering may have been interrupted while the page was in a non-visible
        // state, which would require a flush to resume decoding.
        if (RefPtr mediaSourcePrivate = m_mediaSourcePrivate) {
            SetForScope(m_flushingActiveSourceBuffersDueToVisibilityChange, true, false);
            mediaSourcePrivate->flushActiveSourceBuffersIfNeeded();
        }
    }

#if HAVE(SPATIAL_TRACKING_LABEL)
    updateSpatialTrackingLabel();
#endif
}

MediaTime MediaPlayerPrivateMediaSourceAVFObjC::duration() const
{
    RefPtr mediaSourcePrivate = m_mediaSourcePrivate;
    return mediaSourcePrivate ? mediaSourcePrivate->duration() : MediaTime::zeroTime();
}

MediaTime MediaPlayerPrivateMediaSourceAVFObjC::currentTime() const
{
    if (seeking())
        return m_pendingSeek ? m_pendingSeek->time : m_lastSeekTime;
    return clampTimeToSensicalValue(PAL::toMediaTime(PAL::CMTimebaseGetTime([m_synchronizer timebase])));
}

bool MediaPlayerPrivateMediaSourceAVFObjC::timeIsProgressing() const
{
    return m_isPlaying && [m_synchronizer rate];
}

MediaTime MediaPlayerPrivateMediaSourceAVFObjC::clampTimeToSensicalValue(const MediaTime& time) const
{
    if (m_lastSeekTime.isFinite() && time < m_lastSeekTime)
        return m_lastSeekTime;

    if (time < MediaTime::zeroTime())
        return MediaTime::zeroTime();
    if (time > duration())
        return duration();
    return time;
}

bool MediaPlayerPrivateMediaSourceAVFObjC::setCurrentTimeDidChangeCallback(MediaPlayer::CurrentTimeDidChangeCallback&& callback)
{
    m_currentTimeDidChangeCallback = WTFMove(callback);

    if (m_currentTimeDidChangeCallback) {
        m_timeChangedObserver = [m_synchronizer addPeriodicTimeObserverForInterval:PAL::CMTimeMake(1, 10) queue:mainDispatchQueueSingleton() usingBlock:^(CMTime time) {
            if (!m_currentTimeDidChangeCallback)
                return;

            auto clampedTime = CMTIME_IS_NUMERIC(time) ? clampTimeToSensicalValue(PAL::toMediaTime(time)) : MediaTime::zeroTime();
            m_currentTimeDidChangeCallback(clampedTime);
        }];

    } else
        m_timeChangedObserver = nullptr;

    return true;
}

bool MediaPlayerPrivateMediaSourceAVFObjC::playAtHostTime(const MonotonicTime& time)
{
    ALWAYS_LOG(LOGIDENTIFIER);
    playInternal(time);
    return true;
}

bool MediaPlayerPrivateMediaSourceAVFObjC::pauseAtHostTime(const MonotonicTime& time)
{
    ALWAYS_LOG(LOGIDENTIFIER);
    pauseInternal(time);
    return true;
}

MediaTime MediaPlayerPrivateMediaSourceAVFObjC::startTime() const
{
    return MediaTime::zeroTime();
}

MediaTime MediaPlayerPrivateMediaSourceAVFObjC::initialTime() const
{
    return MediaTime::zeroTime();
}

void MediaPlayerPrivateMediaSourceAVFObjC::seekToTarget(const SeekTarget& target)
{
    ALWAYS_LOG(LOGIDENTIFIER, "time = ", target.time, ", negativeThreshold = ", target.negativeThreshold, ", positiveThreshold = ", target.positiveThreshold);

    m_pendingSeek = target;

    if (m_seekTimer.isActive())
        m_seekTimer.stop();
    m_seekTimer.startOneShot(0_s);
}

void MediaPlayerPrivateMediaSourceAVFObjC::seekInternal()
{
    if (!m_pendingSeek)
        return;

    RefPtr mediaSourcePrivate = m_mediaSourcePrivate;
    if (!mediaSourcePrivate)
        return;

    auto pendingSeek = std::exchange(m_pendingSeek, { }).value();
    m_lastSeekTime = pendingSeek.time;

    m_seekState = Seeking;
    mediaSourcePrivate->waitForTarget(pendingSeek)->whenSettled(RunLoop::currentSingleton(), [weakThis = WeakPtr { *this }] (auto&& result) mutable {
        RefPtr protectedThis = weakThis.get();
        if (!protectedThis)
            return;
        if (protectedThis->m_seekState != Seeking || !result) {
            ALWAYS_LOG_WITH_THIS(protectedThis, LOGIDENTIFIER_WITH_THIS(protectedThis), "seek Interrupted, aborting");
            return;
        }
        auto seekedTime = *result;
        protectedThis->m_lastSeekTime = seekedTime;

        ALWAYS_LOG_WITH_THIS(protectedThis, LOGIDENTIFIER_WITH_THIS(protectedThis));
        MediaTime synchronizerTime = PAL::toMediaTime([protectedThis->m_synchronizer currentTime]);

        protectedThis->m_isSynchronizerSeeking = protectedThis->m_isSynchronizerSeeking = std::abs((synchronizerTime - seekedTime).toMicroseconds()) > 1000;

        ALWAYS_LOG_WITH_THIS(protectedThis, LOGIDENTIFIER_WITH_THIS(protectedThis), "seekedTime = ", seekedTime, ", synchronizerTime = ", synchronizerTime, "synchronizer seeking = ", protectedThis->m_isSynchronizerSeeking);

        if (!protectedThis->m_isSynchronizerSeeking) {
            // In cases where the destination seek time precisely matches the synchronizer's existing time
            // no time jumped notification will be issued. In this case, just notify the MediaPlayer that
            // the seek completed successfully.
            protectedThis->maybeCompleteSeek();
            return;
        }
        RefPtr mediaSourcePrivate = protectedThis->m_mediaSourcePrivate;
        mediaSourcePrivate->willSeek();
        [protectedThis->m_synchronizer setRate:0 time:PAL::toCMTime(seekedTime)];

        mediaSourcePrivate->seekToTime(seekedTime);
    });
}

void MediaPlayerPrivateMediaSourceAVFObjC::maybeCompleteSeek()
{
    if (m_seekState == SeekCompleted)
        return;
    if (hasVideo() && hasVideoRenderer() && !m_hasAvailableVideoFrame) {
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
        setSynchronizerRate(m_rate);
    if (auto player = m_player.get()) {
        player->seeked(m_lastSeekTime);
        player->timeChanged();
    }
}

bool MediaPlayerPrivateMediaSourceAVFObjC::seeking() const
{
    return m_pendingSeek || m_seekState != SeekCompleted;
}

void MediaPlayerPrivateMediaSourceAVFObjC::setRateDouble(double rate)
{
    // AVSampleBufferRenderSynchronizer does not support negative rate yet.
    m_rate = std::max<double>(rate, 0);

    if (auto player = m_player.get()) {
        auto algorithm = MediaSessionManagerCocoa::audioTimePitchAlgorithmForMediaPlayerPitchCorrectionAlgorithm(player->pitchCorrectionAlgorithm(), player->preservesPitch(), m_rate);
        for (const auto& key : m_sampleBufferAudioRendererMap.keys())
            [(__bridge AVSampleBufferAudioRenderer *)key.get() setAudioTimePitchAlgorithm:algorithm.createNSString().get()];
    }

    if (shouldBePlaying())
        setSynchronizerRate(m_rate);
}

double MediaPlayerPrivateMediaSourceAVFObjC::rate() const
{
    return m_rate;
}

double MediaPlayerPrivateMediaSourceAVFObjC::effectiveRate() const
{
    return PAL::CMTimebaseGetRate([m_synchronizer timebase]);
}

void MediaPlayerPrivateMediaSourceAVFObjC::setPreservesPitch(bool preservesPitch)
{
    ALWAYS_LOG(LOGIDENTIFIER, preservesPitch);
    if (auto player = m_player.get()) {
        auto algorithm = MediaSessionManagerCocoa::audioTimePitchAlgorithmForMediaPlayerPitchCorrectionAlgorithm(player->pitchCorrectionAlgorithm(), preservesPitch, m_rate);
        for (const auto& key : m_sampleBufferAudioRendererMap.keys())
            [(__bridge AVSampleBufferAudioRenderer *)key.get() setAudioTimePitchAlgorithm:algorithm.createNSString().get()];
    }
}

MediaPlayer::NetworkState MediaPlayerPrivateMediaSourceAVFObjC::networkState() const
{
    return m_networkState;
}

MediaPlayer::ReadyState MediaPlayerPrivateMediaSourceAVFObjC::readyState() const
{
    return m_readyState;
}

MediaTime MediaPlayerPrivateMediaSourceAVFObjC::maxTimeSeekable() const
{
    return duration();
}

MediaTime MediaPlayerPrivateMediaSourceAVFObjC::minTimeSeekable() const
{
    return startTime();
}

const PlatformTimeRanges& MediaPlayerPrivateMediaSourceAVFObjC::buffered() const
{
    ASSERT_NOT_REACHED();
    return PlatformTimeRanges::emptyRanges();
}

RefPtr<MediaSourcePrivateAVFObjC> MediaPlayerPrivateMediaSourceAVFObjC::protectedMediaSourcePrivate() const
{
    return m_mediaSourcePrivate;
}

void MediaPlayerPrivateMediaSourceAVFObjC::bufferedChanged()
{
    if (m_gapObserver) {
        [m_synchronizer removeTimeObserver:m_gapObserver.get()];
        m_gapObserver = nullptr;
    }

    auto ranges = protectedMediaSourcePrivate()->buffered();
    auto currentTime = this->currentTime();
    size_t index = ranges.find(currentTime);
    if (index == notFound)
        return;
    // Find the next gap (or end of media)
    for (; index < ranges.length(); index++) {
        if ((index < ranges.length() - 1 && ranges.start(index + 1) - ranges.end(index) > m_mediaSourcePrivate->timeFudgeFactor())
            || (index == ranges.length() - 1 && ranges.end(index) > currentTime)) {
            auto gapStart = ranges.end(index);
            NSArray* times = @[[NSValue valueWithCMTime:PAL::toCMTime(gapStart)]];

            auto logSiteIdentifier = LOGIDENTIFIER;
            UNUSED_PARAM(logSiteIdentifier);

            m_gapObserver = [m_synchronizer addBoundaryTimeObserverForTimes:times queue:mainDispatchQueueSingleton() usingBlock:[weakThis = WeakPtr { *this }, logSiteIdentifier, gapStart] {
                RefPtr protectedThis = weakThis.get();
                if (!protectedThis)
                    return;
                if (protectedThis->protectedMediaSourcePrivate()->hasFutureTime(gapStart))
                    return; // New data was added, don't stall.
                MediaTime now = protectedThis->currentTime();
                ALWAYS_LOG_WITH_THIS(protectedThis, logSiteIdentifier, "boundary time observer called, now = ", now);

                if (gapStart == protectedThis->duration())
                    protectedThis->pauseInternal();
                // Experimentation shows that between the time the boundary time observer is called, the time have progressed by a few milliseconds. Re-adjust time. This seek doesn't require re-enqueuing/flushing.
                [protectedThis->m_synchronizer setRate:0 time:PAL::toCMTime(gapStart)];
                if (RefPtr player = protectedThis->m_player.get())
                    player->timeChanged();
            }];
            return;
        }
    }
}

bool MediaPlayerPrivateMediaSourceAVFObjC::didLoadingProgress() const
{
    bool loadingProgressed = m_loadingProgressed;
    m_loadingProgressed = false;
    return loadingProgressed;
}

RefPtr<NativeImage> MediaPlayerPrivateMediaSourceAVFObjC::nativeImageForCurrentTime()
{
    updateLastImage();
    return m_lastImage;
}

bool MediaPlayerPrivateMediaSourceAVFObjC::updateLastPixelBuffer()
{
    RefPtr renderer = layerOrVideoRenderer();
    if (!renderer)
        return false;

    auto entry = renderer->copyDisplayedPixelBuffer();
    if (!entry.pixelBuffer)
        return false;

    INFO_LOG(LOGIDENTIFIER, "displayed pixelbuffer copied for time ", entry.presentationTimeStamp);
    m_lastPixelBuffer = WTFMove(entry.pixelBuffer);
    m_lastPixelBufferPresentationTimeStamp = entry.presentationTimeStamp;
    return true;
}

bool MediaPlayerPrivateMediaSourceAVFObjC::updateLastImage()
{
    if (m_isGatheringVideoFrameMetadata) {
        if (!m_lastPixelBuffer)
            return false;
        if (m_sampleCount == m_lastConvertedSampleCount)
            return false;
        m_lastConvertedSampleCount = m_sampleCount;
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

void MediaPlayerPrivateMediaSourceAVFObjC::maybePurgeLastImage()
{
    // If we are in the middle of a rVFC operation, do not purge anything:
    if (m_isGatheringVideoFrameMetadata)
        return;

    m_lastImage = nullptr;
    m_lastPixelBuffer = nullptr;
}

void MediaPlayerPrivateMediaSourceAVFObjC::paint(GraphicsContext& context, const FloatRect& rect)
{
    paintCurrentFrameInContext(context, rect);
}

void MediaPlayerPrivateMediaSourceAVFObjC::paintCurrentFrameInContext(GraphicsContext& context, const FloatRect& outputRect)
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

RefPtr<VideoFrame> MediaPlayerPrivateMediaSourceAVFObjC::videoFrameForCurrentTime()
{
    if (!m_isGatheringVideoFrameMetadata)
        updateLastPixelBuffer();
    if (!m_lastPixelBuffer)
        return nullptr;
    return VideoFrameCV::create(currentTime(), false, VideoFrame::Rotation::None, RetainPtr { m_lastPixelBuffer });
}

DestinationColorSpace MediaPlayerPrivateMediaSourceAVFObjC::colorSpace()
{
    updateLastImage();
    RefPtr lastImage = m_lastImage;
    return lastImage ? lastImage->colorSpace() : DestinationColorSpace::SRGB();
}

bool MediaPlayerPrivateMediaSourceAVFObjC::hasAvailableVideoFrame() const
{
    return m_hasAvailableVideoFrame;
}

bool MediaPlayerPrivateMediaSourceAVFObjC::supportsAcceleratedRendering() const
{
    return true;
}

bool MediaPlayerPrivateMediaSourceAVFObjC::shouldEnsureLayerOrVideoRenderer() const
{
    // Decompression sessions do not support encrypted content; force layer
    // creation.
    if (!canUseDecompressionSession())
        return true;
    RefPtr player = m_player.get();
    return player && player->renderingCanBeAccelerated();
}

void MediaPlayerPrivateMediaSourceAVFObjC::setPresentationSize(const IntSize& newSize)
{
    if (!layerOrVideoRenderer() && !newSize.isEmpty())
        updateDisplayLayer();
}

void MediaPlayerPrivateMediaSourceAVFObjC::setVideoLayerSizeFenced(const FloatSize& newSize, WTF::MachSendRightAnnotated&&)
{
    if (!layerOrVideoRenderer() && !newSize.isEmpty())
        updateDisplayLayer();
}

void MediaPlayerPrivateMediaSourceAVFObjC::acceleratedRenderingStateChanged()
{
    RefPtr renderer = layerOrVideoRenderer();
    if (willUseDecompressionSessionIfNeeded()) {
        if (renderer && !renderer->isUsingDecompressionSession()) {
            // Gathering video frame metadata changed.
            if (RefPtr mediaSourcePrivate = m_mediaSourcePrivate)
                mediaSourcePrivate->videoRendererWillReconfigure(*renderer);
            renderer->setPreferences(m_loadOptions.videoMediaSampleRendererPreferences | VideoMediaSampleRendererPreference::PrefersDecompressionSession);
            if (RefPtr mediaSourcePrivate = m_mediaSourcePrivate)
                mediaSourcePrivate->videoRendererDidReconfigure(*renderer);
            return;
        }
        if (renderer)
            return; // With a decompression session we can continue using the existing VideoMediaSampleRenderer. No need to re-create one.
    }
    updateDisplayLayer();
}

void MediaPlayerPrivateMediaSourceAVFObjC::updateDisplayLayer()
{
#if ENABLE(LINEAR_MEDIA_PLAYER)
    m_updateDisplayLayerPending = false;
#endif

    if (shouldEnsureLayerOrVideoRenderer() || willUseDecompressionSessionIfNeeded()) {
        auto needsRenderingModeChanged = destroyRenderlessVideoMediaSampleRenderer();
        ensureLayerOrVideoRenderer(needsRenderingModeChanged);
        return;
    }

    ensureRenderlessVideoMediaSampleRenderer();
}

void MediaPlayerPrivateMediaSourceAVFObjC::notifyActiveSourceBuffersChanged()
{
    if (auto player = m_player.get())
        player->activeSourceBuffersChanged();
}

MediaPlayer::MovieLoadType MediaPlayerPrivateMediaSourceAVFObjC::movieLoadType() const
{
    return MediaPlayer::MovieLoadType::StoredStream;
}

void MediaPlayerPrivateMediaSourceAVFObjC::prepareForRendering()
{
    // No-op.
}

String MediaPlayerPrivateMediaSourceAVFObjC::engineDescription() const
{
    static NeverDestroyed<String> description(MAKE_STATIC_STRING_IMPL("AVFoundation MediaSource Engine"));
    return description;
}

String MediaPlayerPrivateMediaSourceAVFObjC::languageOfPrimaryAudioTrack() const
{
    // FIXME(125158): implement languageOfPrimaryAudioTrack()
    return emptyString();
}

size_t MediaPlayerPrivateMediaSourceAVFObjC::extraMemoryCost() const
{
    return 0;
}

std::optional<VideoPlaybackQualityMetrics> MediaPlayerPrivateMediaSourceAVFObjC::videoPlaybackQualityMetrics()
{
    if (RefPtr renderer = layerOrVideoRenderer()) {
        return VideoPlaybackQualityMetrics {
            renderer->totalVideoFrames(),
            renderer->droppedVideoFrames(),
            renderer->corruptedVideoFrames(),
            renderer->totalFrameDelay().toDouble(),
            0,
        };
    }
    return { };
}

#pragma mark -
#pragma mark Utility Methods

void MediaPlayerPrivateMediaSourceAVFObjC::destroyVideoLayerIfNeeded()
{
    if (!m_needsDestroyVideoLayer)
        return;
    m_needsDestroyVideoLayer = false;
    m_videoLayerManager->didDestroyVideoLayer();
}

void MediaPlayerPrivateMediaSourceAVFObjC::ensureLayer()
{
    if (m_sampleBufferDisplayLayer)
        return;

    ALWAYS_LOG(LOGIDENTIFIER);

    destroyVideoLayerIfNeeded();

    m_sampleBufferDisplayLayer = adoptNS([PAL::allocAVSampleBufferDisplayLayerInstance() init]);
    if (!m_sampleBufferDisplayLayer)
        return;

#ifndef NDEBUG
    [m_sampleBufferDisplayLayer setName:@"MediaPlayerPrivateMediaSource AVSampleBufferDisplayLayer"];
#endif
    [m_sampleBufferDisplayLayer setVideoGravity: (m_shouldMaintainAspectRatio ? AVLayerVideoGravityResizeAspect : AVLayerVideoGravityResize)];

    configureLayerOrVideoRenderer(m_sampleBufferDisplayLayer.get());

    if (RefPtr player = m_player.get()) {
        if ([m_sampleBufferDisplayLayer respondsToSelector:@selector(setToneMapToStandardDynamicRange:)])
            [m_sampleBufferDisplayLayer setToneMapToStandardDynamicRange:player->shouldDisableHDR()];

        setLayerDynamicRangeLimit(m_sampleBufferDisplayLayer.get(), player->platformDynamicRangeLimit());

        m_videoLayerManager->setVideoLayer(m_sampleBufferDisplayLayer.get(), player->presentationSize());
    }

    m_rendererWithSampleBufferDisplayLayer = createVideoMediaSampleRendererForRendererer(m_sampleBufferDisplayLayer.get());
}

void MediaPlayerPrivateMediaSourceAVFObjC::destroyLayer()
{
    if (!m_sampleBufferDisplayLayer)
        return;

    ALWAYS_LOG(LOGIDENTIFIER);

    CMTime currentTime = PAL::CMTimebaseGetTime([m_synchronizer timebase]);
    [m_synchronizer removeRenderer:m_sampleBufferDisplayLayer.get() atTime:currentTime completionHandler:nil];
    m_videoLayerManager->didDestroyVideoLayer();
    m_sampleBufferDisplayLayer = nullptr;
    m_rendererWithSampleBufferDisplayLayer = nullptr;
    m_needsDestroyVideoLayer = false;
}

void MediaPlayerPrivateMediaSourceAVFObjC::ensureVideoRenderer()
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

    m_rendererWithSampleBufferVideoRenderer = createVideoMediaSampleRendererForRendererer(m_sampleBufferVideoRenderer.get());
#endif // ENABLE(LINEAR_MEDIA_PLAYER)
}

void MediaPlayerPrivateMediaSourceAVFObjC::destroyVideoRenderer()
{
#if ENABLE(LINEAR_MEDIA_PLAYER)
    if (!m_sampleBufferVideoRenderer)
        return;

    ALWAYS_LOG(LOGIDENTIFIER);

    CMTime currentTime = PAL::CMTimebaseGetTime([m_synchronizer timebase]);
    [m_synchronizer removeRenderer:m_sampleBufferVideoRenderer.get() atTime:currentTime completionHandler:nil];

    m_sampleBufferVideoRenderer = nullptr;
    m_rendererWithSampleBufferVideoRenderer = nullptr;
#endif // ENABLE(LINEAR_MEDIA_PLAYER)
}

bool MediaPlayerPrivateMediaSourceAVFObjC::isUsingRenderlessMediaSampleRenderer() const
{
    return !m_sampleBufferDisplayLayer && !willUseDecompressionSessionIfNeeded();
}

void MediaPlayerPrivateMediaSourceAVFObjC::ensureRenderlessVideoMediaSampleRenderer()
{
    destroyLayerOrVideoRenderer();

    if (canUseDecompressionSession()) {
        setVideoRenderer(nil);

        if (RefPtr player = m_player.get())
            player->renderingModeChanged();
        return;
    }

    if (isUsingRenderlessMediaSampleRenderer())
        return;
    if (RefPtr mediaSourcePrivate = m_mediaSourcePrivate)
        mediaSourcePrivate->setVideoRenderer(nullptr);

    setHasAvailableVideoFrame(false);
    m_rendererWithSampleBufferDisplayLayer = createVideoMediaSampleRendererForRendererer(nil);
    if (RefPtr mediaSourcePrivate = m_mediaSourcePrivate)
        mediaSourcePrivate->setVideoRenderer(m_rendererWithSampleBufferDisplayLayer.get());

    if (auto player = m_player.get())
        player->renderingModeChanged();
}

MediaPlayerEnums::NeedsRenderingModeChanged MediaPlayerPrivateMediaSourceAVFObjC::destroyRenderlessVideoMediaSampleRenderer()
{
    if (!isUsingRenderlessMediaSampleRenderer())
        return MediaPlayerEnums::NeedsRenderingModeChanged::No;

    ALWAYS_LOG(LOGIDENTIFIER);
    ASSERT(!m_rendererWithSampleBufferVideoRenderer && !m_sampleBufferVideoRenderer, "No layer or renderer can be in use when a render-less VideoMediaSampleRenderer is active");

    if (RefPtr mediaSourcePrivate = m_mediaSourcePrivate)
        mediaSourcePrivate->setVideoRenderer(nullptr);

    m_sampleBufferDisplayLayer = nullptr;
    setHasAvailableVideoFrame(false);
    return MediaPlayerEnums::NeedsRenderingModeChanged::Yes;
}

void MediaPlayerPrivateMediaSourceAVFObjC::ensureLayerOrVideoRendererWithDecompressionSession(MediaPlayerEnums::NeedsRenderingModeChanged needsRenderingModeChanged)
{
    auto videoMode = acceleratedVideoMode();
    switch (videoMode) {
    case AcceleratedVideoMode::Layer:
        ensureLayer();
        break;
    case AcceleratedVideoMode::VideoRenderer:
        ensureVideoRenderer();
        break;
    case AcceleratedVideoMode::StagedVideoRenderer:
    case AcceleratedVideoMode::StagedLayer:
        ASSERT_NOT_REACHED();
        break;
    }

    RefPtr mediaSourcePrivate = m_mediaSourcePrivate;
    RetainPtr renderer = acceleratedVideoMode() == AcceleratedVideoMode::Layer ? m_sampleBufferDisplayLayer.get() : static_cast<WebSampleBufferVideoRendering *>(m_sampleBufferVideoRenderer.get());

    if (!renderer) {
        ERROR_LOG(LOGIDENTIFIER, "Failed to create AVSampleBufferDisplayLayer or AVSampleBufferVideoRenderer");
        if (mediaSourcePrivate)
            mediaSourcePrivate->failedToCreateRenderer(MediaSourcePrivateAVFObjC::RendererType::Video);
        setNetworkState(MediaPlayer::NetworkState::DecodeError);
        return;
    }

    ALWAYS_LOG(LOGIDENTIFIER, acceleratedVideoMode(), ", renderer=", !!renderer);

    switch (videoMode) {
    case AcceleratedVideoMode::Layer:
        needsRenderingModeChanged = MediaPlayerEnums::NeedsRenderingModeChanged::Yes;
        [[fallthrough]];
    case AcceleratedVideoMode::VideoRenderer:
        setVideoRenderer(renderer.get());
        break;
    case AcceleratedVideoMode::StagedLayer:
    case AcceleratedVideoMode::StagedVideoRenderer:
        ASSERT_NOT_REACHED();
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

void MediaPlayerPrivateMediaSourceAVFObjC::ensureLayerOrVideoRenderer(MediaPlayerEnums::NeedsRenderingModeChanged needsRenderingModeChanged)
{
    if (canUseDecompressionSession())
        return ensureLayerOrVideoRendererWithDecompressionSession(needsRenderingModeChanged);

    switch (acceleratedVideoMode()) {
    case AcceleratedVideoMode::Layer:
        destroyVideoRenderer();
        [[fallthrough]];
    case AcceleratedVideoMode::StagedLayer:
        ensureLayer();
        break;
    case AcceleratedVideoMode::VideoRenderer:
        destroyLayer();
        [[fallthrough]];
    case AcceleratedVideoMode::StagedVideoRenderer:
        ensureVideoRenderer();
        break;
    }

    RefPtr mediaSourcePrivate = m_mediaSourcePrivate;
    RefPtr renderer = layerOrVideoRenderer();

    if (!renderer) {
        ERROR_LOG(LOGIDENTIFIER, "Failed to create AVSampleBufferDisplayLayer or AVSampleBufferVideoRenderer");
        if (mediaSourcePrivate)
            mediaSourcePrivate->failedToCreateRenderer(MediaSourcePrivateAVFObjC::RendererType::Video);
        setNetworkState(MediaPlayer::NetworkState::DecodeError);
        return;
    }

    ALWAYS_LOG(LOGIDENTIFIER, acceleratedVideoMode(), ", renderer=", !!renderer);

    switch (acceleratedVideoMode()) {
    case AcceleratedVideoMode::Layer:
        needsRenderingModeChanged = MediaPlayerEnums::NeedsRenderingModeChanged::Yes;
        [[fallthrough]];
    case AcceleratedVideoMode::VideoRenderer:
        if (mediaSourcePrivate)
            mediaSourcePrivate->setVideoRenderer(renderer.get());
        break;
    case AcceleratedVideoMode::StagedLayer:
        ASSERT(!canUseDecompressionSession());
        needsRenderingModeChanged = MediaPlayerEnums::NeedsRenderingModeChanged::Yes;
        [[fallthrough]];
    case AcceleratedVideoMode::StagedVideoRenderer:
        ASSERT(!canUseDecompressionSession());
        if (mediaSourcePrivate)
            mediaSourcePrivate->stageVideoRenderer(renderer.get());
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

void MediaPlayerPrivateMediaSourceAVFObjC::destroyLayerOrVideoRenderer()
{
    if (!isUsingRenderlessMediaSampleRenderer())
        destroyLayer();
    destroyVideoRenderer();

    if (RefPtr mediaSourcePrivate = m_mediaSourcePrivate)
        mediaSourcePrivate->setVideoRenderer(nullptr);

    setHasAvailableVideoFrame(false);

    if (RefPtr player = m_player.get())
        player->renderingModeChanged();
}

void MediaPlayerPrivateMediaSourceAVFObjC::configureLayerOrVideoRenderer(WebSampleBufferVideoRendering *renderer)
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

void MediaPlayerPrivateMediaSourceAVFObjC::setVideoRenderer(WebSampleBufferVideoRendering *renderer)
{
    ALWAYS_LOG(LOGIDENTIFIER, "!!renderer = ", !!renderer);
    RELEASE_ASSERT(canUseDecompressionSession());

    if (m_videoRenderer)
        return stageVideoRenderer(renderer);

    if (m_rendererWithSampleBufferDisplayLayer) {
        m_videoRenderer = m_rendererWithSampleBufferDisplayLayer;
        return stageVideoRenderer(renderer);
    }
    m_videoRenderer = createVideoMediaSampleRendererForRendererer(renderer);
    if (RefPtr mediaSourcePrivate = m_mediaSourcePrivate)
        mediaSourcePrivate->setVideoRenderer(m_videoRenderer.get());
}

void MediaPlayerPrivateMediaSourceAVFObjC::stageVideoRenderer(WebSampleBufferVideoRendering *renderer)
{
    ASSERT(m_videoRenderer);
    RELEASE_ASSERT(canUseDecompressionSession());

    if (RefPtr mediaSourcePrivate = m_mediaSourcePrivate)
        mediaSourcePrivate->setVideoRenderer(m_videoRenderer.get());

    RefPtr videoRenderer = m_videoRenderer;
    if (renderer == videoRenderer->renderer())
        return;

    ALWAYS_LOG(LOGIDENTIFIER, "!!renderer = ", !!renderer);

    Vector<RetainPtr<WebSampleBufferVideoRendering>> renderersToExpire { 2u };
    if (renderer) {
        switch (acceleratedVideoMode()) {
        case AcceleratedVideoMode::Layer:
            renderersToExpire.append(std::exchange(m_sampleBufferVideoRenderer, { }));
            m_needsDestroyVideoLayer = true;
            break;
        case AcceleratedVideoMode::VideoRenderer:
            renderersToExpire.append(std::exchange(m_sampleBufferDisplayLayer, { }));
            break;
        case AcceleratedVideoMode::StagedVideoRenderer:
        case AcceleratedVideoMode::StagedLayer:
            RELEASE_ASSERT_NOT_REACHED();
            break;
        }
    } else {
        renderersToExpire.append(m_sampleBufferVideoRenderer);
        renderersToExpire.append(m_sampleBufferDisplayLayer);
    }

    videoRenderer->changeRenderer(renderer)->whenSettled(RunLoop::mainSingleton(), [weakThis = WeakPtr { *this }, renderersToExpire = WTFMove(renderersToExpire)] {
        for (auto& rendererToExpire : renderersToExpire) {
            if (!rendererToExpire)
                continue;
            if (RefPtr protectedThis = weakThis.get()) {
                CMTime currentTime = PAL::CMTimebaseGetTime([protectedThis->m_synchronizer timebase]);
                [protectedThis->m_synchronizer removeRenderer:rendererToExpire.get() atTime:currentTime completionHandler:nil];
            }
        }
    });
}

Ref<VideoMediaSampleRenderer> MediaPlayerPrivateMediaSourceAVFObjC::createVideoMediaSampleRendererForRendererer(WebSampleBufferVideoRendering *renderer)
{
    Ref videoRenderer = VideoMediaSampleRenderer::create(renderer);
    videoRenderer->setTimebase([m_synchronizer timebase]);
    videoRenderer->notifyFirstFrameAvailable([weakThis = WeakPtr { *this }](const MediaTime&, double) {
        if (RefPtr protectedThis = weakThis.get())
            protectedThis->setHasAvailableVideoFrame(true);
    });
    setVideoFrameMetadataGatheringCallbackIfNeeded(videoRenderer);
    videoRenderer->setResourceOwner(m_resourceOwner);
    videoRenderer->setPreferences(m_loadOptions.videoMediaSampleRendererPreferences);
    return videoRenderer;
}

MediaPlayerPrivateMediaSourceAVFObjC::AcceleratedVideoMode MediaPlayerPrivateMediaSourceAVFObjC::acceleratedVideoMode() const
{
    return m_acceleratedVideoMode;
}

bool MediaPlayerPrivateMediaSourceAVFObjC::canUseDecompressionSession() const
{
    if (!m_loadOptions.videoMediaSampleRendererPreferences.contains(VideoMediaSampleRendererPreference::PrefersDecompressionSession))
        return false;
    if (m_loadOptions.videoMediaSampleRendererPreferences.contains(VideoMediaSampleRendererPreference::UseDecompressionSessionForProtectedContent))
        return true;
    RefPtr mediaSourcePrivate = m_mediaSourcePrivate;
    return !mediaSourcePrivate || (!mediaSourcePrivate->cdmInstance() && !mediaSourcePrivate->needsVideoLayer());
}

bool MediaPlayerPrivateMediaSourceAVFObjC::isUsingDecompressionSession() const
{
    if (RefPtr renderer = m_videoRenderer)
        return renderer->isUsingDecompressionSession();
    if (RefPtr renderer = layerOrVideoRenderer())
        return renderer->isUsingDecompressionSession();
    return false;
}

bool MediaPlayerPrivateMediaSourceAVFObjC::willUseDecompressionSessionIfNeeded() const
{
    if (!canUseDecompressionSession())
        return false;

    return m_loadOptions.videoMediaSampleRendererPreferences.contains(VideoMediaSampleRendererPreference::PrefersDecompressionSession) || m_isGatheringVideoFrameMetadata;
}

bool MediaPlayerPrivateMediaSourceAVFObjC::shouldBePlaying() const
{
    return m_isPlaying && !seeking() && (m_flushingActiveSourceBuffersDueToVisibilityChange || allRenderersHaveAvailableSamples()) && m_readyState >= MediaPlayer::ReadyState::HaveFutureData;
}

void MediaPlayerPrivateMediaSourceAVFObjC::setSynchronizerRate(double rate, std::optional<MonotonicTime>&& hostTime)
{
    if (hostTime) {
        auto cmHostTime = PAL::CMClockMakeHostTimeFromSystemUnits(hostTime->toMachAbsoluteTime());
        ALWAYS_LOG(LOGIDENTIFIER, "setting rate to ", m_rate, " at host time ", PAL::CMTimeGetSeconds(cmHostTime));
        [m_synchronizer setRate:rate time:PAL::kCMTimeInvalid atHostTime:cmHostTime];
    } else
        [m_synchronizer setRate:rate];

    // If we are pausing the synchronizer, update the last image to ensure we have something
    // to display if and when the decoders are purged while in the background. And vice-versa,
    // purge our retained images and pixel buffers when playing the synchronizer, to release that
    // retained memory.
    if (!rate)
        updateLastPixelBuffer();
    else
        maybePurgeLastImage();
}

void MediaPlayerPrivateMediaSourceAVFObjC::setHasAvailableVideoFrame(bool flag)
{
#if ENABLE(LINEAR_MEDIA_PLAYER)
    if (flag && m_needNewFrameToProgressStaging) {
        m_needNewFrameToProgressStaging = false;
        maybeUpdateDisplayLayer();
    }
#endif

    if (m_hasAvailableVideoFrame == flag)
        return;

    ALWAYS_LOG(LOGIDENTIFIER, flag);
    m_hasAvailableVideoFrame = flag;
    updateAllRenderersHaveAvailableSamples();

    if (!m_hasAvailableVideoFrame)
        return;

    setNeedsPlaceholderImage(false);

    auto player = m_player.get();
    if (player)
        player->firstVideoFrameAvailable();

    if (m_seekState == WaitingForAvailableFame)
        maybeCompleteSeek();

    if (m_readyStateIsWaitingForAvailableFrame) {
        m_readyStateIsWaitingForAvailableFrame = false;
        if (player)
            player->readyStateChanged();
    }
}

ALLOW_NEW_API_WITHOUT_GUARDS_BEGIN
void MediaPlayerPrivateMediaSourceAVFObjC::setHasAvailableAudioSample(AVSampleBufferAudioRenderer* renderer, bool flag)
ALLOW_NEW_API_WITHOUT_GUARDS_END
{
    auto iter = m_sampleBufferAudioRendererMap.find((__bridge CFTypeRef)renderer);
    if (iter == m_sampleBufferAudioRendererMap.end())
        return;

    auto& properties = iter->value;
    if (properties.hasAudibleSample == flag)
        return;
    ALWAYS_LOG(LOGIDENTIFIER, flag);
    properties.hasAudibleSample = flag;
    updateAllRenderersHaveAvailableSamples();
}

void MediaPlayerPrivateMediaSourceAVFObjC::updateAllRenderersHaveAvailableSamples()
{
    bool allRenderersHaveAvailableSamples = true;

    do {
        if (hasVideo() && hasVideoRenderer() && !m_hasAvailableVideoFrame) {
            allRenderersHaveAvailableSamples = false;
            break;
        }

        for (auto& properties : m_sampleBufferAudioRendererMap.values()) {
            if (!properties.hasAudibleSample) {
                allRenderersHaveAvailableSamples = false;
                break;
            }
        }
    } while (0);

    if (m_allRenderersHaveAvailableSamples == allRenderersHaveAvailableSamples)
        return;

    DEBUG_LOG(LOGIDENTIFIER, allRenderersHaveAvailableSamples);
    m_allRenderersHaveAvailableSamples = allRenderersHaveAvailableSamples;

    if (shouldBePlaying() && [m_synchronizer rate] != m_rate)
        setSynchronizerRate(m_rate);
    else if (!shouldBePlaying() && [m_synchronizer rate])
        setSynchronizerRate(0);
}

void MediaPlayerPrivateMediaSourceAVFObjC::durationChanged()
{
    RefPtr mediaSourcePrivate = m_mediaSourcePrivate;
    if (!mediaSourcePrivate)
        return;

    MediaTime duration = mediaSourcePrivate->duration();
    // Avoid emiting durationchanged in the case where the previous duration was unkniwn as that case is already handled
    // by the HTMLMediaElement.
    if (m_duration != duration && m_duration.isValid()) {
        if (auto player = m_player.get())
            player->durationChanged();
    }
    m_duration = duration;
}

void MediaPlayerPrivateMediaSourceAVFObjC::effectiveRateChanged()
{
    ALWAYS_LOG(LOGIDENTIFIER, effectiveRate());
    if (auto player = m_player.get())
        player->rateChanged();
}

void MediaPlayerPrivateMediaSourceAVFObjC::sizeWillChangeAtTime(const MediaTime& time, const FloatSize& size)
{
    auto weakThis = m_sizeChangeObserverWeakPtrFactory.createWeakPtr(*this);
    NSArray* times = @[[NSValue valueWithCMTime:PAL::toCMTime(time)]];
    RetainPtr<id> observer = [m_synchronizer addBoundaryTimeObserverForTimes:times queue:mainDispatchQueueSingleton() usingBlock:[weakThis = WTFMove(weakThis), size] {
        RefPtr protectedThis = weakThis.get();
        if (!protectedThis)
            return;

        ASSERT(!protectedThis->m_sizeChangeObservers.isEmpty());
        if (!protectedThis->m_sizeChangeObservers.isEmpty()) {
            RetainPtr<id> observer = protectedThis->m_sizeChangeObservers.takeFirst();
            [protectedThis->m_synchronizer removeTimeObserver:observer.get()];
        }
        protectedThis->setNaturalSize(size);
    }];
    m_sizeChangeObservers.append(WTFMove(observer));

    if (currentTime() >= time)
        setNaturalSize(size);
}

void MediaPlayerPrivateMediaSourceAVFObjC::setNaturalSize(const FloatSize& size)
{
    if (size == m_naturalSize)
        return;

    ALWAYS_LOG(LOGIDENTIFIER, size);

    m_naturalSize = size;
    if (auto player = m_player.get())
        player->sizeChanged();
}

void MediaPlayerPrivateMediaSourceAVFObjC::flushPendingSizeChanges()
{
    while (!m_sizeChangeObservers.isEmpty()) {
        RetainPtr<id> observer = m_sizeChangeObservers.takeFirst();
        [m_synchronizer removeTimeObserver:observer.get()];
    }
    m_sizeChangeObserverWeakPtrFactory.revokeAll();
}

#if ENABLE(LEGACY_ENCRYPTED_MEDIA)
RefPtr<CDMSessionAVContentKeySession> MediaPlayerPrivateMediaSourceAVFObjC::cdmSession() const
{
    return m_session.get();
}

void MediaPlayerPrivateMediaSourceAVFObjC::setCDMSession(LegacyCDMSession* session)
{
    if (session == m_session.get())
        return;

    ALWAYS_LOG(LOGIDENTIFIER);

    // FIXME: This is a false positive. Remove the suppression once rdar://145631564 is fixed.
    SUPPRESS_UNCOUNTED_ARG m_session = toCDMSessionAVContentKeySession(session);

    if (RefPtr mediaSourcePrivate = m_mediaSourcePrivate)
        mediaSourcePrivate->setCDMSession(session);
}

void MediaPlayerPrivateMediaSourceAVFObjC::keyAdded()
{
    if (RefPtr mediaSourcePrivate = m_mediaSourcePrivate)
        mediaSourcePrivate->keyAdded();
}

#endif // ENABLE(LEGACY_ENCRYPTED_MEDIA)

#if ENABLE(LEGACY_ENCRYPTED_MEDIA) || ENABLE(ENCRYPTED_MEDIA)
void MediaPlayerPrivateMediaSourceAVFObjC::keyNeeded(const SharedBuffer& initData)
{
    if (auto player = m_player.get())
        player->keyNeeded(initData);
}
#endif

void MediaPlayerPrivateMediaSourceAVFObjC::outputObscuredDueToInsufficientExternalProtectionChanged(bool obscured)
{
#if ENABLE(ENCRYPTED_MEDIA)
    ALWAYS_LOG(LOGIDENTIFIER, obscured);
    if (RefPtr mediaSourcePrivate = m_mediaSourcePrivate)
        mediaSourcePrivate->outputObscuredDueToInsufficientExternalProtectionChanged(obscured);
#else
    UNUSED_PARAM(obscured);
#endif
}

#if ENABLE(ENCRYPTED_MEDIA)
void MediaPlayerPrivateMediaSourceAVFObjC::cdmInstanceAttached(CDMInstance& instance)
{
    ALWAYS_LOG(LOGIDENTIFIER);
    if (RefPtr mediaSourcePrivate = m_mediaSourcePrivate)
        mediaSourcePrivate->cdmInstanceAttached(instance);

    updateDisplayLayer();
}

void MediaPlayerPrivateMediaSourceAVFObjC::cdmInstanceDetached(CDMInstance& instance)
{
    ALWAYS_LOG(LOGIDENTIFIER);
    if (RefPtr mediaSourcePrivate = m_mediaSourcePrivate)
        mediaSourcePrivate->cdmInstanceDetached(instance);

    updateDisplayLayer();
}

void MediaPlayerPrivateMediaSourceAVFObjC::attemptToDecryptWithInstance(CDMInstance& instance)
{
    ALWAYS_LOG(LOGIDENTIFIER);
    if (RefPtr mediaSourcePrivate = m_mediaSourcePrivate)
        mediaSourcePrivate->attemptToDecryptWithInstance(instance);
}

bool MediaPlayerPrivateMediaSourceAVFObjC::waitingForKey() const
{
    RefPtr mediaSourcePrivate = m_mediaSourcePrivate;
    return mediaSourcePrivate && mediaSourcePrivate->waitingForKey();
}

void MediaPlayerPrivateMediaSourceAVFObjC::waitingForKeyChanged()
{
    ALWAYS_LOG(LOGIDENTIFIER);
    if (auto player = m_player.get())
        player->waitingForKeyChanged();
}

void MediaPlayerPrivateMediaSourceAVFObjC::initializationDataEncountered(const String& initDataType, RefPtr<ArrayBuffer>&& initData)
{
    ALWAYS_LOG(LOGIDENTIFIER, initDataType);
    if (auto player = m_player.get())
        player->initializationDataEncountered(initDataType, WTFMove(initData));
}
#endif

const Vector<ContentType>& MediaPlayerPrivateMediaSourceAVFObjC::mediaContentTypesRequiringHardwareSupport() const
{
    return m_player.get()->mediaContentTypesRequiringHardwareSupport();
}

void MediaPlayerPrivateMediaSourceAVFObjC::needsVideoLayerChanged()
{
    updateDisplayLayer();
}

void MediaPlayerPrivateMediaSourceAVFObjC::setNeedsPlaceholderImage(bool needsPlaceholder)
{
    if (m_needsPlaceholderImage == needsPlaceholder)
        return;

    m_needsPlaceholderImage = needsPlaceholder;

    if (!m_sampleBufferDisplayLayer)
        return;

    RetainPtr displayLayer = m_sampleBufferDisplayLayer;
    if (m_needsPlaceholderImage)
        [displayLayer setContents:(id)m_lastPixelBuffer.get()];
    else
        [displayLayer setContents:nil];
}

void MediaPlayerPrivateMediaSourceAVFObjC::setReadyState(MediaPlayer::ReadyState readyState)
{
    if (m_readyState == readyState)
        return;

    if (m_readyState > MediaPlayer::ReadyState::HaveCurrentData && readyState == MediaPlayer::ReadyState::HaveCurrentData)
        ALWAYS_LOG(LOGIDENTIFIER, "stall detected currentTime:", currentTime());

    m_readyState = readyState;

    if (shouldBePlaying())
        setSynchronizerRate(m_rate);
    else
        setSynchronizerRate(0);

    if (m_readyState >= MediaPlayer::ReadyState::HaveCurrentData && hasVideo() && hasVideoRenderer() && !m_hasAvailableVideoFrame) {
        m_readyStateIsWaitingForAvailableFrame = true;
        return;
    }

    if (auto player = m_player.get())
        player->readyStateChanged();
}

void MediaPlayerPrivateMediaSourceAVFObjC::setNetworkState(MediaPlayer::NetworkState networkState)
{
    if (m_networkState == networkState)
        return;

    ALWAYS_LOG(LOGIDENTIFIER, networkState);
    m_networkState = networkState;
    if (auto player = m_player.get())
        player->networkStateChanged();
}

ALLOW_NEW_API_WITHOUT_GUARDS_BEGIN
void MediaPlayerPrivateMediaSourceAVFObjC::addAudioRenderer(AVSampleBufferAudioRenderer* audioRenderer)
ALLOW_NEW_API_WITHOUT_GUARDS_END
{
    if (!audioRenderer) {
        ASSERT_NOT_REACHED();
        return;
    }

    if (!m_sampleBufferAudioRendererMap.add((__bridge CFTypeRef)audioRenderer, AudioRendererProperties()).isNewEntry)
        return;

    auto player = m_player.get();
    if (!player)
        return;

    [audioRenderer setMuted:player->muted()];
    [audioRenderer setVolume:player->volume()];
    auto algorithm = MediaSessionManagerCocoa::audioTimePitchAlgorithmForMediaPlayerPitchCorrectionAlgorithm(player->pitchCorrectionAlgorithm(), player->preservesPitch(), m_rate);
    [audioRenderer setAudioTimePitchAlgorithm:algorithm.createNSString().get()];
#if PLATFORM(MAC)
ALLOW_NEW_API_WITHOUT_GUARDS_BEGIN
    if ([audioRenderer respondsToSelector:@selector(setIsUnaccompaniedByVisuals:)])
        [audioRenderer setIsUnaccompaniedByVisuals:!player->isVideoPlayer()];
ALLOW_NEW_API_WITHOUT_GUARDS_END
#endif

#if HAVE(AUDIO_OUTPUT_DEVICE_UNIQUE_ID)
    auto deviceId = player->audioOutputDeviceIdOverride();
    if (!deviceId.isNull()) {
        if (deviceId.isEmpty() || deviceId == AudioMediaStreamTrackRenderer::defaultDeviceID()) {
            // FIXME(rdar://155986053): Remove the @try/@catch when this exception is resolved.
            @try {
                audioRenderer.audioOutputDeviceUniqueID = nil;
            } @catch(NSException *exception) {
                ERROR_LOG(LOGIDENTIFIER, "-[AVSampleBufferRenderSynchronizer setAudioOutputDeviceUniqueID:] threw an exception: ", exception.name, ", reason : ", exception.reason);
            }
        } else
            audioRenderer.audioOutputDeviceUniqueID = deviceId.createNSString().get();
    }
#endif

    @try {
        [m_synchronizer addRenderer:audioRenderer];
    } @catch(NSException *exception) {
        ERROR_LOG(LOGIDENTIFIER, "-[AVSampleBufferRenderSynchronizer addRenderer:] threw an exception: ", exception.name, ", reason : ", exception.reason);
        ASSERT_NOT_REACHED();

        setNetworkState(MediaPlayer::NetworkState::DecodeError);
        return;
    }

    player->characteristicChanged();
}

ALLOW_NEW_API_WITHOUT_GUARDS_BEGIN
void MediaPlayerPrivateMediaSourceAVFObjC::removeAudioRenderer(AVSampleBufferAudioRenderer* audioRenderer)
ALLOW_NEW_API_WITHOUT_GUARDS_END
{
    auto iter = m_sampleBufferAudioRendererMap.find((__bridge CFTypeRef)audioRenderer);
    if (iter == m_sampleBufferAudioRendererMap.end())
        return;

    CMTime currentTime = PAL::CMTimebaseGetTime([m_synchronizer timebase]);
    [m_synchronizer removeRenderer:audioRenderer atTime:currentTime completionHandler:nil];

    m_sampleBufferAudioRendererMap.remove(iter);
}

void MediaPlayerPrivateMediaSourceAVFObjC::removeAudioTrack(AudioTrackPrivate& track)
{
    if (auto player = m_player.get())
        player->removeAudioTrack(track);
}

void MediaPlayerPrivateMediaSourceAVFObjC::removeVideoTrack(VideoTrackPrivate& track)
{
    if (auto player = m_player.get())
        player->removeVideoTrack(track);
}

void MediaPlayerPrivateMediaSourceAVFObjC::removeTextTrack(InbandTextTrackPrivate& track)
{
    if (auto player = m_player.get())
        player->removeTextTrack(track);
}

void MediaPlayerPrivateMediaSourceAVFObjC::characteristicsChanged()
{
    updateAllRenderersHaveAvailableSamples();
    if (auto player = m_player.get())
        player->characteristicChanged();
}

RetainPtr<PlatformLayer> MediaPlayerPrivateMediaSourceAVFObjC::createVideoFullscreenLayer()
{
    return adoptNS([[CALayer alloc] init]);
}

void MediaPlayerPrivateMediaSourceAVFObjC::setVideoFullscreenLayer(PlatformLayer *videoFullscreenLayer, WTF::Function<void()>&& completionHandler)
{
    updateLastImage();
    RefPtr lastImage = m_lastImage;
    m_videoLayerManager->setVideoFullscreenLayer(videoFullscreenLayer, WTFMove(completionHandler), lastImage ? lastImage->platformImage() : nullptr);
}

void MediaPlayerPrivateMediaSourceAVFObjC::setVideoFullscreenFrame(const FloatRect& frame)
{
    m_videoLayerManager->setVideoFullscreenFrame(frame);
}

void MediaPlayerPrivateMediaSourceAVFObjC::syncTextTrackBounds()
{
    m_videoLayerManager->syncTextTrackBounds();
}

void MediaPlayerPrivateMediaSourceAVFObjC::setTextTrackRepresentation(TextTrackRepresentation* representation)
{
    auto* representationLayer = representation ? representation->platformLayer() : nil;
    m_videoLayerManager->setTextTrackRepresentationLayer(representationLayer);
}

#if ENABLE(WIRELESS_PLAYBACK_TARGET)
void MediaPlayerPrivateMediaSourceAVFObjC::setWirelessPlaybackTarget(Ref<MediaPlaybackTarget>&& target)
{
    ALWAYS_LOG(LOGIDENTIFIER);
    m_playbackTarget = WTFMove(target);
}

void MediaPlayerPrivateMediaSourceAVFObjC::setShouldPlayToPlaybackTarget(bool shouldPlayToTarget)
{
    if (shouldPlayToTarget == m_shouldPlayToTarget)
        return;

    ALWAYS_LOG(LOGIDENTIFIER, shouldPlayToTarget);
    m_shouldPlayToTarget = shouldPlayToTarget;

    if (auto player = m_player.get())
        player->currentPlaybackTargetIsWirelessChanged(isCurrentPlaybackTargetWireless());
}

bool MediaPlayerPrivateMediaSourceAVFObjC::isCurrentPlaybackTargetWireless() const
{
    RefPtr playbackTarget = m_playbackTarget;
    if (!playbackTarget)
        return false;

    auto hasTarget = m_shouldPlayToTarget && playbackTarget->hasActiveRoute();
    INFO_LOG(LOGIDENTIFIER, hasTarget);
    return hasTarget;
}
#endif

bool MediaPlayerPrivateMediaSourceAVFObjC::performTaskAtTime(WTF::Function<void(const MediaTime&)>&& task, const MediaTime& time)
{
    if (m_performTaskObserver)
        [m_synchronizer removeTimeObserver:m_performTaskObserver.get()];

    m_performTaskObserver = [m_synchronizer addBoundaryTimeObserverForTimes:@[[NSValue valueWithCMTime:PAL::toCMTime(time)]] queue:mainDispatchQueueSingleton() usingBlock:makeBlockPtr([weakThis = WeakPtr { *this }, task = WTFMove(task)] {
        if (RefPtr protectedThis = weakThis.get())
            task(protectedThis->currentTime());
    }).get()];
    return true;
}

void MediaPlayerPrivateMediaSourceAVFObjC::audioOutputDeviceChanged()
{
#if HAVE(AUDIO_OUTPUT_DEVICE_UNIQUE_ID)
    auto player = m_player.get();
    if (!player)
        return;
    auto deviceId = player->audioOutputDeviceId();
    for (auto& key : m_sampleBufferAudioRendererMap.keys()) {
        auto renderer = ((__bridge AVSampleBufferAudioRenderer *)key.get());
        if (deviceId.isEmpty() || deviceId == AudioMediaStreamTrackRenderer::defaultDeviceID()) {
            // FIXME(rdar://155986053): Remove the @try/catch when this exception is resolved.
            @try {
                renderer.audioOutputDeviceUniqueID = nil;
            } @catch(NSException *exception) {
                ERROR_LOG(LOGIDENTIFIER, "-[AVSampleBufferRenderSynchronizer setAudioOutputDeviceUniqueID:] threw an exception: ", exception.name, ", reason : ", exception.reason);
            }
        } else
            renderer.audioOutputDeviceUniqueID = deviceId.createNSString().get();
    }
#endif
}

void MediaPlayerPrivateMediaSourceAVFObjC::setVideoFrameMetadataGatheringCallbackIfNeeded(VideoMediaSampleRenderer& videoRenderer)
{
    if (!m_isGatheringVideoFrameMetadata)
        return;
    videoRenderer.notifyWhenHasAvailableVideoFrame([weakThis = WeakPtr { *this }](const MediaTime& presentationTime, double displayTime) {
        if (RefPtr protectedThis = weakThis.get())
            protectedThis->checkNewVideoFrameMetadata(presentationTime, displayTime);
    });
}

void MediaPlayerPrivateMediaSourceAVFObjC::startVideoFrameMetadataGathering()
{
    if (m_isGatheringVideoFrameMetadata)
        return;
    m_isGatheringVideoFrameMetadata = true;
    if (RefPtr videoRenderer = layerOrVideoRenderer())
        setVideoFrameMetadataGatheringCallbackIfNeeded(*videoRenderer);

    if (isUsingDecompressionSession())
        return;

    acceleratedRenderingStateChanged();

    if (willUseDecompressionSessionIfNeeded())
        return;

    ASSERT(m_synchronizer);
    m_videoFrameMetadataGatheringObserver = [m_synchronizer addPeriodicTimeObserverForInterval:PAL::CMTimeMake(1, 60) queue:mainDispatchQueueSingleton() usingBlock:[weakThis = WeakPtr { *this }](CMTime currentCMTime) {
        ensureOnMainThread([weakThis, currentCMTime] {
            if (!weakThis)
                return;
            auto currentTime = PAL::toMediaTime(currentCMTime);
            auto presentationTime = weakThis->m_lastPixelBufferPresentationTimeStamp;
            if (!presentationTime.isValid())
                presentationTime = currentTime;

            auto displayTime = MonotonicTime::now().secondsSinceEpoch().seconds() - (currentTime - presentationTime).toDouble();
            weakThis->checkNewVideoFrameMetadata(currentTime, displayTime);
        });
    }];
}

void MediaPlayerPrivateMediaSourceAVFObjC::checkNewVideoFrameMetadata(MediaTime presentationTime, double displayTime)
{
    auto player = m_player.get();
    if (!player)
        return;

    if (!updateLastPixelBuffer())
        return;

#ifndef NDEBUG
    if (isUsingDecompressionSession() && m_lastPixelBufferPresentationTimeStamp != presentationTime)
        ALWAYS_LOG(LOGIDENTIFIER, "notification of new frame delayed retrieved:", m_lastPixelBufferPresentationTimeStamp, " expected:", presentationTime);
#endif

    VideoFrameMetadata metadata;
    metadata.width = m_naturalSize.width();
    metadata.height = m_naturalSize.height();
    metadata.presentedFrames = isUsingDecompressionSession() ? layerOrVideoRenderer()->totalDisplayedFrames() : ++m_sampleCount;
    metadata.presentationTime = displayTime;
    metadata.expectedDisplayTime = displayTime;
    metadata.mediaTime = (m_lastPixelBufferPresentationTimeStamp.isValid() ? m_lastPixelBufferPresentationTimeStamp : presentationTime).toDouble();

    m_videoFrameMetadata = metadata;
    player->onNewVideoFrameMetadata(WTFMove(metadata), m_lastPixelBuffer.get());
}

void MediaPlayerPrivateMediaSourceAVFObjC::stopVideoFrameMetadataGathering()
{
    m_isGatheringVideoFrameMetadata = false;
    if (RefPtr videoRenderer = layerOrVideoRenderer())
        videoRenderer->notifyWhenHasAvailableVideoFrame(nullptr);
    acceleratedRenderingStateChanged();
    m_videoFrameMetadata = { };

    if (m_videoFrameMetadataGatheringObserver)
        [m_synchronizer removeTimeObserver:m_videoFrameMetadataGatheringObserver.get()];
    m_videoFrameMetadataGatheringObserver = nil;
}

void MediaPlayerPrivateMediaSourceAVFObjC::setShouldDisableHDR(bool shouldDisable)
{
    if (!m_sampleBufferDisplayLayer)
        return;

    if (![m_sampleBufferDisplayLayer respondsToSelector:@selector(setToneMapToStandardDynamicRange:)])
        return;

    ALWAYS_LOG(LOGIDENTIFIER, shouldDisable);
    [m_sampleBufferDisplayLayer setToneMapToStandardDynamicRange:shouldDisable];
}

void MediaPlayerPrivateMediaSourceAVFObjC::setPlatformDynamicRangeLimit(PlatformDynamicRangeLimit platformDynamicRangeLimit)
{
    if (!m_sampleBufferDisplayLayer)
        return;

    setLayerDynamicRangeLimit(m_sampleBufferDisplayLayer.get(), platformDynamicRangeLimit);
}

void MediaPlayerPrivateMediaSourceAVFObjC::playerContentBoxRectChanged(const LayoutRect& newRect)
{
    if (!layerOrVideoRenderer() && !newRect.isEmpty())
        updateDisplayLayer();
}

WTFLogChannel& MediaPlayerPrivateMediaSourceAVFObjC::logChannel() const
{
    return LogMediaSource;
}

void MediaPlayerPrivateMediaSourceAVFObjC::setShouldMaintainAspectRatio(bool shouldMaintainAspectRatio)
{
    if (m_shouldMaintainAspectRatio == shouldMaintainAspectRatio)
        return;

    m_shouldMaintainAspectRatio = shouldMaintainAspectRatio;
    if (!m_sampleBufferDisplayLayer || isUsingRenderlessMediaSampleRenderer())
        return;

    [CATransaction begin];
    [CATransaction setAnimationDuration:0];
    [CATransaction setDisableActions:YES];

    [m_sampleBufferDisplayLayer setVideoGravity: (m_shouldMaintainAspectRatio ? AVLayerVideoGravityResizeAspect : AVLayerVideoGravityResize)];

    [CATransaction commit];
}

#if HAVE(SPATIAL_TRACKING_LABEL)
const String& MediaPlayerPrivateMediaSourceAVFObjC::defaultSpatialTrackingLabel() const
{
    return m_defaultSpatialTrackingLabel;
}

void MediaPlayerPrivateMediaSourceAVFObjC::setDefaultSpatialTrackingLabel(const String& defaultSpatialTrackingLabel)
{
    if (m_defaultSpatialTrackingLabel == defaultSpatialTrackingLabel)
        return;
    m_defaultSpatialTrackingLabel = defaultSpatialTrackingLabel;
    updateSpatialTrackingLabel();
}

const String& MediaPlayerPrivateMediaSourceAVFObjC::spatialTrackingLabel() const
{
    return m_spatialTrackingLabel;
}

void MediaPlayerPrivateMediaSourceAVFObjC::setSpatialTrackingLabel(const String& spatialTrackingLabel)
{
    if (m_spatialTrackingLabel == spatialTrackingLabel)
        return;
    m_spatialTrackingLabel = spatialTrackingLabel;
    updateSpatialTrackingLabel();
}

void MediaPlayerPrivateMediaSourceAVFObjC::updateSpatialTrackingLabel()
{
    auto player = m_player.get();
    if (!player)
        return;

    if ((!m_sampleBufferDisplayLayer && !m_sampleBufferVideoRenderer) || isUsingRenderlessMediaSampleRenderer())
        return;
    RetainPtr renderer = m_sampleBufferVideoRenderer ? m_sampleBufferVideoRenderer : [m_sampleBufferDisplayLayer sampleBufferRenderer];
    ASSERT(renderer);

#if HAVE(SPATIAL_AUDIO_EXPERIENCE)
    if (player->prefersSpatialAudioExperience()) {
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
        ALWAYS_LOG(LOGIDENTIFIER, "Setting spatialAudioExperience: ", spatialAudioExperienceDescription(experience.get()));
        [m_synchronizer setIntendedSpatialAudioExperience:experience.get()];

        if (RefPtr client = player->messageClientForTesting())
            client->sendInternalMessage({ "media-player-spatial-experience-change"_s, spatialAudioExperienceDescription(experience.get()) });

        return;
    }
#endif

    if (!m_spatialTrackingLabel.isNull()) {
        ALWAYS_LOG(LOGIDENTIFIER, "Explicitly set STSLabel: ", m_spatialTrackingLabel);
        [renderer setSTSLabel:m_spatialTrackingLabel.createNSString().get()];
        return;
    }

    if (renderer && m_visible) {
        // If the media player has a renderer, and that renderer belongs to a page that is visible,
        // then let AVSBRS manage setting the spatial tracking label in its video renderer itself.
        ALWAYS_LOG(LOGIDENTIFIER, "Has visible renderer, set STSLabel: nil");
        [renderer setSTSLabel:nil];
        return;
    }

    if (m_sampleBufferAudioRendererMap.isEmpty()) {
        ALWAYS_LOG(LOGIDENTIFIER, "No audio renderers - no-op");
        // If there are no audio renderers, there's nothing to do.
        return;
    }

    // If there is no video renderer, use the default spatial tracking label if available, or
    // the session's spatial tracking label if not, and set the label directly on each audio
    // renderer.
    AVAudioSession *session = [PAL::getAVAudioSessionClassSingleton() sharedInstance];
    RetainPtr<NSString> defaultLabel;
    if (!m_defaultSpatialTrackingLabel.isNull()) {
        ALWAYS_LOG(LOGIDENTIFIER, "Default STSLabel: ", m_defaultSpatialTrackingLabel);
        defaultLabel = m_defaultSpatialTrackingLabel.createNSString();
    } else {
        ALWAYS_LOG(LOGIDENTIFIER, "AVAudioSession label: ", session.spatialTrackingLabel);
        defaultLabel = session.spatialTrackingLabel;
    }
    for (const auto &key : m_sampleBufferAudioRendererMap.keys())
        [(__bridge AVSampleBufferAudioRenderer *)key.get() setSTSLabel:defaultLabel.get()];
}
#endif

bool MediaPlayerPrivateMediaSourceAVFObjC::hasVideoRenderer() const
{
    return layerOrVideoRenderer() && layerOrVideoRenderer()->renderer();
}

RefPtr<VideoMediaSampleRenderer> MediaPlayerPrivateMediaSourceAVFObjC::layerOrVideoRenderer() const
{
    if (canUseDecompressionSession() && m_videoRenderer)
        return m_videoRenderer;
#if ENABLE(LINEAR_MEDIA_PLAYER)
    switch (acceleratedVideoMode()) {
    case AcceleratedVideoMode::Layer:
    case AcceleratedVideoMode::StagedLayer:
        return m_rendererWithSampleBufferDisplayLayer;
    case AcceleratedVideoMode::VideoRenderer:
    case AcceleratedVideoMode::StagedVideoRenderer:
        return m_rendererWithSampleBufferVideoRenderer;
    }
#else
    return m_rendererWithSampleBufferDisplayLayer;
#endif
}

#if ENABLE(LINEAR_MEDIA_PLAYER)
void MediaPlayerPrivateMediaSourceAVFObjC::setVideoTarget(const PlatformVideoTarget& videoTarget)
{
    if (canUseDecompressionSession()) {
        m_acceleratedVideoMode = !!videoTarget ? AcceleratedVideoMode::VideoRenderer : AcceleratedVideoMode::Layer;
        m_videoTarget = videoTarget;
        updateDisplayLayer();
        return;
    }

    RefPtr player = m_player.get();
    bool isAlreadyInFullscreen = player && player->isInFullscreenOrPictureInPicture();

    // Transition to docking goes: Layer -> StagedVideoRenderer -> Renderer
    // Transition from docking goes: Renderer -> StagedLayer -> Layer
    // Transition to external playback goes: Layer -> StagedVideoRenderer -> Renderer
    // Transition from external playback goes: Renderer -> StagedLayer -> Layer
    auto oldAcceleratedVideoMode = m_acceleratedVideoMode;
    switch (oldAcceleratedVideoMode) {
    case AcceleratedVideoMode::Layer:
    case AcceleratedVideoMode::StagedLayer:
        m_acceleratedVideoMode = !!videoTarget ? AcceleratedVideoMode::StagedVideoRenderer : oldAcceleratedVideoMode;
        break;
    case AcceleratedVideoMode::VideoRenderer:
    case AcceleratedVideoMode::StagedVideoRenderer:
        m_acceleratedVideoMode = !!videoTarget ? oldAcceleratedVideoMode : AcceleratedVideoMode::StagedLayer;
        break;
    }
    ALWAYS_LOG(LOGIDENTIFIER, "videoTarget:", !!videoTarget, " oldAcceleratedVideoMode:", oldAcceleratedVideoMode, " newAcceleratedVideoMode:", m_acceleratedVideoMode, " fullscreen:", isAlreadyInFullscreen);
    if (oldAcceleratedVideoMode != m_acceleratedVideoMode && m_acceleratedVideoMode == AcceleratedVideoMode::StagedLayer)
        m_needNewFrameToProgressStaging = true;
    m_videoTarget = videoTarget;
    updateDisplayLayer();
}
#endif

#if PLATFORM(IOS_FAMILY)
void MediaPlayerPrivateMediaSourceAVFObjC::sceneIdentifierDidChange()
{
#if HAVE(SPATIAL_TRACKING_LABEL)
    updateSpatialTrackingLabel();
#endif
}

void MediaPlayerPrivateMediaSourceAVFObjC::applicationWillResignActive()
{
    if (RefPtr mediaSourcePrivate = m_mediaSourcePrivate)
        mediaSourcePrivate->applicationWillResignActive();

    RefPtr videoRenderer = m_videoRenderer;
    if (!videoRenderer || !videoRenderer->isUsingDecompressionSession())
        return;

    if (!paused()) {
        ALWAYS_LOG(LOGIDENTIFIER, "Playing; not invalidating VideoMediaSampleRenderer Decompression Session");
        return;
    }

    videoRenderer->invalidateDecompressionSession();
    ALWAYS_LOG(LOGIDENTIFIER, "Paused; invalidating VideoMediaSampleRenderer Decompression Session");
}

void MediaPlayerPrivateMediaSourceAVFObjC::applicationDidBecomeActive()
{
    if (RefPtr mediaSourcePrivate = m_mediaSourcePrivate)
        mediaSourcePrivate->applicationDidBecomeActive();
}
#endif

void MediaPlayerPrivateMediaSourceAVFObjC::isInFullscreenOrPictureInPictureChanged(bool isInFullscreenOrPictureInPicture)
{
#if ENABLE(LINEAR_MEDIA_PLAYER)
    ALWAYS_LOG(LOGIDENTIFIER, isInFullscreenOrPictureInPicture, " acceleratedVideoMode:", m_acceleratedVideoMode);

    if (m_acceleratedVideoMode == AcceleratedVideoMode::VideoRenderer)
        destroyVideoLayerIfNeeded();

    if (m_acceleratedVideoMode == AcceleratedVideoMode::Layer || m_acceleratedVideoMode == AcceleratedVideoMode::VideoRenderer)
        return;
    m_updateDisplayLayerPending = true;
    maybeUpdateDisplayLayer();
#else
    UNUSED_PARAM(isInFullscreenOrPictureInPicture);
#endif
}

#if ENABLE(LINEAR_MEDIA_PLAYER)
void MediaPlayerPrivateMediaSourceAVFObjC::maybeUpdateDisplayLayer()
{
    if (m_videoRenderer)
        return;
    ALWAYS_LOG(LOGIDENTIFIER, "updateLayerPending:", m_updateDisplayLayerPending, " acceleratedVideoMode:", m_acceleratedVideoMode, " needNewFrame:", m_needNewFrameToProgressStaging);
    if (m_acceleratedVideoMode == AcceleratedVideoMode::Layer || m_acceleratedVideoMode == AcceleratedVideoMode::VideoRenderer) {
        m_updateDisplayLayerPending = false;
        return;
    }
    if (m_needNewFrameToProgressStaging || !m_updateDisplayLayerPending)
        return;
    // Transition to/out fullscreen is now complete.
    if (m_acceleratedVideoMode == AcceleratedVideoMode::StagedLayer)
        m_acceleratedVideoMode = AcceleratedVideoMode::Layer;
    else if (m_acceleratedVideoMode == AcceleratedVideoMode::StagedVideoRenderer)
        m_acceleratedVideoMode = AcceleratedVideoMode::VideoRenderer;
    updateDisplayLayer();
}
#endif

} // namespace WebCore

#endif
