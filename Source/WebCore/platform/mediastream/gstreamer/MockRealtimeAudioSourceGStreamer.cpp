/*
 * Copyright (C) 2018 Metrological Group B.V.
 * Copyright (C) 2020 Igalia S.L.
 * Author: Thibault Saunier <tsaunier@igalia.com>
 * Author: Alejandro G. Castro  <alex@igalia.com>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public License
 * aint with this library; see the file COPYING.LIB.  If not, write to
 * the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 */

#include "config.h"

#if ENABLE(MEDIA_STREAM) && USE(GSTREAMER)
#include "MockRealtimeAudioSourceGStreamer.h"

#include "GStreamerCaptureDeviceManager.h"
#include "MockRealtimeMediaSourceCenter.h"
#include <gst/app/gstappsrc.h>
#include <numbers>
#include <wtf/IndexedRange.h>
#include <wtf/MediaTime.h>

namespace WebCore {

static constexpr double s_Tau = 2 * std::numbers::pi;
static constexpr double s_BipBopDuration = 0.07;
static constexpr double s_BipBopVolume = 0.5;
static constexpr double s_BipFrequency = 1500;
static constexpr double s_BopFrequency = 500;
static constexpr double s_HumFrequency = 150;
static constexpr double s_HumVolume = 0.1;
static constexpr double s_NoiseFrequency = 3000;
static constexpr double s_NoiseVolume = 0.05;

static HashSet<MockRealtimeAudioSource*>& allMockRealtimeAudioSourcesStorage()
{
    static MainThreadNeverDestroyed<HashSet<MockRealtimeAudioSource*>> audioSources;
    return audioSources;
}

const HashSet<MockRealtimeAudioSource*>& MockRealtimeAudioSourceGStreamer::allMockRealtimeAudioSources()
{
    return allMockRealtimeAudioSourcesStorage();
}

CaptureSourceOrError MockRealtimeAudioSource::create(String&& deviceID, AtomString&& name, MediaDeviceHashSalts&& hashSalts, const MediaConstraints* constraints, std::optional<PageIdentifier>)
{
#ifndef NDEBUG
    auto device = MockRealtimeMediaSourceCenter::mockDeviceWithPersistentID(deviceID);
    ASSERT(device);
    if (!device)
        return CaptureSourceOrError({ "No mock microphone device"_s, MediaAccessDenialReason::PermissionDenied });
#endif

    auto source = adoptRef(*new MockRealtimeAudioSourceGStreamer(WTFMove(deviceID), WTFMove(name), WTFMove(hashSalts)));
    if (constraints) {
        if (auto error = source->applyConstraints(*constraints))
            return CaptureSourceOrError(CaptureSourceError { error->invalidConstraint });
    }

    return CaptureSourceOrError(WTFMove(source));
}

Ref<MockRealtimeAudioSource> MockRealtimeAudioSourceGStreamer::createForMockAudioCapturer(String&& deviceID, AtomString&& name, MediaDeviceHashSalts&& hashSalts)
{
    return adoptRef(*new MockRealtimeAudioSourceGStreamer(WTFMove(deviceID), WTFMove(name), WTFMove(hashSalts)));
}

MockRealtimeAudioSourceGStreamer::MockRealtimeAudioSourceGStreamer(String&& deviceID, AtomString&& name, MediaDeviceHashSalts&& hashSalts)
    : MockRealtimeAudioSource(WTFMove(deviceID), WTFMove(name), WTFMove(hashSalts), { })
{
    ensureGStreamerInitialized();
    allMockRealtimeAudioSourcesStorage().add(this);

    auto& singleton = GStreamerAudioCaptureDeviceManager::singleton();
    auto device = singleton.gstreamerDeviceWithUID(this->captureDevice().persistentId());
    ASSERT(device);
    if (!device)
        return;

    device->setIsMockDevice(true);
    m_capturer = adoptRef(*new GStreamerAudioCapturer(WTFMove(*device)));
    m_capturer->addObserver(*this);
    m_capturer->setupPipeline();
    m_capturer->setSinkAudioCallback([this](auto&& sample, auto&& presentationTime) {
        const auto& info = m_streamFormat->getInfo();
        auto samplesCount = gst_buffer_get_size(gst_sample_get_buffer(sample.get())) / m_streamFormat->bytesPerFrame();
        GStreamerAudioData data(WTFMove(sample), info);
        audioSamplesAvailable(presentationTime, data, *m_streamFormat, samplesCount);
    });
    singleton.registerCapturer(m_capturer);
}

MockRealtimeAudioSourceGStreamer::~MockRealtimeAudioSourceGStreamer()
{
    allMockRealtimeAudioSourcesStorage().remove(this);

    m_capturer->stop();
    m_capturer->removeObserver(*this);

    auto& singleton = GStreamerAudioCaptureDeviceManager::singleton();
    singleton.unregisterCapturer(*m_capturer);
}

void MockRealtimeAudioSourceGStreamer::startProducingData()
{
    m_capturer->start();

    MockRealtimeAudioSource::startProducingData();
}

void MockRealtimeAudioSourceGStreamer::stopProducingData()
{
    m_capturer->stop();
    MockRealtimeAudioSource::stopProducingData();
    m_caps = nullptr;
    m_streamFormat.reset();
}

void MockRealtimeAudioSourceGStreamer::captureEnded()
{
    captureFailed();
}

void MockRealtimeAudioSourceGStreamer::captureDeviceUpdated(const GStreamerCaptureDevice& device)
{
    setName(AtomString { device.label() });
    setPersistentId(device.persistentId());

    configurationChanged();
}

std::pair<GstClockTime, GstClockTime> MockRealtimeAudioSourceGStreamer::queryCaptureLatency() const
{
    if (!m_capturer)
        return { GST_CLOCK_TIME_NONE, GST_CLOCK_TIME_NONE };

    return m_capturer->queryLatency();
}

static void addHum(float amplitude, float frequency, float sampleRate, uint64_t start, std::span<float> destination)
{
    float humPeriod = sampleRate / frequency;
    for (auto [i, destinationValue] : indexedRange(destination)) {
        float a = amplitude * sin((start + i) * s_Tau / humPeriod);
        destinationValue += a;
    }
}

void MockRealtimeAudioSourceGStreamer::render(Seconds delta)
{
    if (!m_bipBopBuffer.size() || !m_streamFormat)
        reconfigure();

    uint32_t totalFrameCount = GST_ROUND_UP_16(static_cast<size_t>(delta.seconds() * sampleRate()));
    uint32_t frameCount = std::min(totalFrameCount, m_maximiumFrameCount);

    while (frameCount) {
        uint32_t bipBopStart = m_samplesRendered % m_bipBopBuffer.size();
        uint32_t bipBopRemain = m_bipBopBuffer.size() - bipBopStart;
        uint32_t bipBopCount = std::min(frameCount, bipBopRemain);

        // We might have stopped producing data. Break out of the loop earlier if that happens.
        if (!isProducingData())
            break;

        ASSERT(m_streamFormat);
        const auto& info = m_streamFormat->getInfo();
        GRefPtr<GstBuffer> buffer = adoptGRef(gst_buffer_new_allocate(nullptr, bipBopCount * m_streamFormat->bytesPerFrame(), nullptr));
        {
            GstMappedBuffer map(buffer.get(), GST_MAP_WRITE);

            if (muted())
                webkitGstAudioFormatFillSilence(info.finfo, map.data(), map.size());
            else {
                auto destination = map.mutableSpan<float>();
                memcpySpan(destination, m_bipBopBuffer.subspan(bipBopStart, bipBopCount));
                addHum(s_HumVolume, s_HumFrequency, sampleRate(), m_samplesRendered, destination);
            }
        }

        m_samplesRendered += bipBopCount;
        totalFrameCount -= bipBopCount;
        frameCount = std::min(totalFrameCount, m_maximiumFrameCount);

        MediaTime presentationTime((m_samplesRendered * G_USEC_PER_SEC) / sampleRate(), G_USEC_PER_SEC);
        GST_BUFFER_PTS(buffer.get()) = toGstClockTime(presentationTime);
        GST_BUFFER_FLAG_SET(buffer.get(), GST_BUFFER_FLAG_LIVE);

        auto sample = adoptGRef(gst_sample_new(buffer.get(), m_caps.get(), nullptr, nullptr));
        // Mock GstDevice is an appsrc, see webkitMockDeviceCreateElement().
        ASSERT(GST_IS_APP_SRC(m_capturer->source()));
        gst_app_src_push_sample(GST_APP_SRC_CAST(m_capturer->source()), sample.get());
    }
}

void MockRealtimeAudioSourceGStreamer::settingsDidChange(OptionSet<RealtimeMediaSourceSettings::Flag> flags)
{
    MockRealtimeAudioSource::settingsDidChange(flags);
    reconfigure();
}

void MockRealtimeAudioSourceGStreamer::reconfigure()
{
    GstAudioInfo info;
    auto rate = sampleRate();
    size_t sampleCount = 2 * rate;

    m_maximiumFrameCount = roundUpToPowerOfTwo<uint32_t>(renderInterval().seconds() * sampleRate());
    gst_audio_info_set_format(&info, GST_AUDIO_FORMAT_F32LE, rate, 1, nullptr);
    m_streamFormat = GStreamerAudioStreamDescription(info);
    m_caps = adoptGRef(gst_audio_info_to_caps(&info));

    m_bipBopBuffer.resize(sampleCount);
    m_bipBopBuffer.fill(0);

    size_t bipBopSampleCount = ceil(s_BipBopDuration * rate);
    size_t bipStart = 0;
    size_t bopStart = rate;

    addHum(s_BipBopVolume, s_BipFrequency, rate, 0, m_bipBopBuffer.mutableSpan().subspan(bipStart, bipBopSampleCount));
    addHum(s_BipBopVolume, s_BopFrequency, rate, 0, m_bipBopBuffer.mutableSpan().subspan(bopStart, bipBopSampleCount));

    if (!echoCancellation())
        addHum(s_NoiseVolume, s_NoiseFrequency, rate, 0, m_bipBopBuffer.mutableSpan().first(sampleCount));
}

void MockRealtimeAudioSourceGStreamer::setInterruptedForTesting(bool isInterrupted)
{
    m_isInterrupted = isInterrupted;
    MockRealtimeAudioSource::setInterruptedForTesting(isInterrupted);
}

} // namespace WebCore

#endif // ENABLE(MEDIA_STREAM) && USE(GSTREAMER)
