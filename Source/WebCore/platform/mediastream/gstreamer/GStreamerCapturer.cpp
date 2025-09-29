/*
 * Copyright (C) 2018 Metrological Group B.V.
 * Copyright (C) 2020 Igalia S.L.
 * Author: Thibault Saunier <tsaunier@igalia.com>
 * Author: Alejandro G. Castro <alex@igalia.com>
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

#if ENABLE(VIDEO) && ENABLE(MEDIA_STREAM) && USE(GSTREAMER)

#include "GStreamerCapturer.h"
#include "VideoFrameMetadataGStreamer.h"

#include <gst/app/gstappsink.h>
#include <mutex>
#include <wtf/HexNumber.h>
#include <wtf/MonotonicTime.h>
#include <wtf/PrintStream.h>
#include <wtf/text/MakeString.h>

GST_DEBUG_CATEGORY(webkit_capturer_debug);
#define GST_CAT_DEFAULT webkit_capturer_debug

namespace WebCore {

static void initializeCapturerDebugCategory()
{
    ensureGStreamerInitialized();

    static std::once_flag debugRegisteredFlag;
    std::call_once(debugRegisteredFlag, [] {
        GST_DEBUG_CATEGORY_INIT(webkit_capturer_debug, "webkitcapturer", 0, "WebKit Capturer");
    });
}

GStreamerCapturer::GStreamerCapturer(GStreamerCaptureDevice&& device, GRefPtr<GstCaps>&& caps)
    : m_caps(WTFMove(caps))
    , m_deviceType(device.type())
{
    initializeCapturerDebugCategory();
    m_device.emplace(WTFMove(device));
}

GStreamerCapturer::GStreamerCapturer(const PipeWireCaptureDevice& device)
    : m_deviceType(device.type())
{
    initializeCapturerDebugCategory();
    m_pipewireDevice.emplace(device);
    m_caps = device.caps();
}

GStreamerCapturer::~GStreamerCapturer()
{
    tearDown(true);
}

void GStreamerCapturer::tearDown(bool disconnectSignals)
{
    GST_DEBUG("Disposing capture pipeline %" GST_PTR_FORMAT " (disconnecting signals: %s)", m_pipeline.get(), boolForPrinting(disconnectSignals));
    if (disconnectSignals && m_sink)
        g_signal_handlers_disconnect_by_data(m_sink.get(), this);

    if (m_pipeline) {
        if (disconnectSignals) {
            unregisterPipeline(m_pipeline);
            disconnectSimpleBusMessageCallback(pipeline());
        }
        gst_element_set_state(pipeline(), GST_STATE_NULL);
    }

    if (!disconnectSignals)
        return;

    m_valve = nullptr;
    m_src = nullptr;
    m_capsfilter = nullptr;
    m_sink = nullptr;
    m_pipeline = nullptr;
    m_caps = nullptr;
}

GStreamerCapturerObserver::~GStreamerCapturerObserver()
{
}

void GStreamerCapturer::setDevice(std::optional<GStreamerCaptureDevice>&& device)
{
    if (device)
        GST_DEBUG_OBJECT(m_pipeline.get(), "Setting new capture device: %s", device->label().utf8().data());
    else
        GST_DEBUG_OBJECT(m_pipeline.get(), "Clearing capture device");
    tearDown(true);
    m_device = WTFMove(device);

    if (!m_device) [[unlikely]]
        return;

    setupPipeline();
    start();

#ifndef GST_DISABLE_GST_DEBUG
    auto totalObservers = m_observers.computeSize();
    GST_DEBUG_OBJECT(m_pipeline.get(), "Notifying %u observers that the capture device changed", totalObservers);
#endif
    forEachObserver([&](auto& observer) {
        observer.captureDeviceUpdated(*m_device);
    });
}

void GStreamerCapturer::addObserver(GStreamerCapturerObserver& observer)
{
    ASSERT(isMainThread());
    m_observers.add(observer);
}

void GStreamerCapturer::removeObserver(GStreamerCapturerObserver& observer)
{
    ASSERT(isMainThread());
    m_observers.remove(observer);
}

void GStreamerCapturer::forEachObserver(NOESCAPE const Function<void(GStreamerCapturerObserver&)>& apply)
{
    ASSERT(isMainThread());
    Ref protectedThis { *this };
    m_observers.forEach(apply);
}

GstElement* GStreamerCapturer::createSource()
{
    if (m_pipewireDevice) {
        m_src = makeElement("pipewiresrc");
        ASSERT(m_src);

        if (m_deviceType == CaptureDevice::DeviceType::Screen) {
            auto srcPad = adoptGRef(gst_element_get_static_pad(m_src.get(), "src"));
            gst_pad_add_probe(srcPad.get(), GST_PAD_PROBE_TYPE_EVENT_DOWNSTREAM, [](GstPad*, GstPadProbeInfo* info, void* userData) -> GstPadProbeReturn {
                auto* event = gst_pad_probe_info_get_event(info);
                if (GST_EVENT_TYPE(event) != GST_EVENT_CAPS)
                    return GST_PAD_PROBE_OK;

                auto self = reinterpret_cast<GStreamerCapturer*>(userData);
                callOnMainThread([event = GRefPtr(event), weakThis = ThreadSafeWeakPtr { *self }] {
                    RefPtr protectedThis = weakThis.get();
                    if (!protectedThis)
                        return;

                    GstCaps* caps;
                    gst_event_parse_caps(event.get(), &caps);
                    protectedThis->forEachObserver([caps](GStreamerCapturerObserver& observer) {
                        observer.sourceCapsChanged(caps);
                    });
                });
                return GST_PAD_PROBE_OK;
            }, this, nullptr);
        }

        auto path = AtomString::number(m_pipewireDevice->objectId());
        // FIXME: The path property is deprecated in favor of target-object but the portal doesn't expose this object.
        g_object_set(m_src.get(), "path", path.string().ascii().data(), "fd", m_pipewireDevice->fd(), nullptr);
    } else {
        ASSERT(m_device);
        auto sourceName = makeString(unsafeSpan(name()), hex(reinterpret_cast<uintptr_t>(this)));
        m_src = gst_device_create_element(m_device->device(), sourceName.ascii().data());
        ASSERT(m_src);
        g_object_set(m_src.get(), "do-timestamp", TRUE, nullptr);
    }

    GST_DEBUG_OBJECT(m_pipeline.get(), "Source element created: %" GST_PTR_FORMAT, m_src.get());

    if (gstElementFactoryEquals(m_src.get(), "pipewiresrc"_s)) {
        auto srcPad = adoptGRef(gst_element_get_static_pad(m_src.get(), "src"));
        gst_pad_add_probe(srcPad.get(), GST_PAD_PROBE_TYPE_EVENT_DOWNSTREAM, [](GstPad*, GstPadProbeInfo* info, void* userData) -> GstPadProbeReturn {
            auto* event = gst_pad_probe_info_get_event(info);
            if (GST_EVENT_TYPE(event) != GST_EVENT_CAPS)
                return GST_PAD_PROBE_OK;

            callOnMainThread([event, capturer = reinterpret_cast<GStreamerCapturer*>(userData)] {
                GstCaps* caps;
                gst_event_parse_caps(event, &caps);
                capturer->forEachObserver([caps](auto& observer) {
                    observer.sourceCapsChanged(caps);
                });
            });
            return GST_PAD_PROBE_OK;
        }, this, nullptr);
    }

    if (m_deviceType == CaptureDevice::DeviceType::Camera) {
        if (gstElementMatchesFactoryAndHasProperty(m_src.get(), "pipewiresrc"_s, "use-bufferpool"_s))
            g_object_set(m_src.get(), "use-bufferpool", FALSE, nullptr);

        auto srcPad = adoptGRef(gst_element_get_static_pad(m_src.get(), "src"));
        gst_pad_add_probe(srcPad.get(), static_cast<GstPadProbeType>(GST_PAD_PROBE_TYPE_PUSH | GST_PAD_PROBE_TYPE_BUFFER), [](GstPad*, GstPadProbeInfo* info, gpointer) -> GstPadProbeReturn {
            VideoFrameTimeMetadata metadata;
            metadata.captureTime = MonotonicTime::now().secondsSinceEpoch();
            auto buffer = GST_PAD_PROBE_INFO_BUFFER(info);
            auto [rotation, isMirrored] = webkitGstBufferGetVideoRotation(buffer);

            auto modifiedBuffer = webkitGstBufferSetVideoFrameMetadata(GRefPtr(buffer), metadata, rotation, isMirrored);
            gst_pad_probe_info_set_buffer(info, modifiedBuffer.leakRef());
            return GST_PAD_PROBE_OK;
        }, nullptr, nullptr);
    }

    return m_src.get();
}

GRefPtr<GstCaps> GStreamerCapturer::caps()
{
    if (m_pipewireDevice)
        return m_pipewireDevice->caps();

    ASSERT(m_device);
    return adoptGRef(gst_device_get_caps(m_device->device()));
}

void GStreamerCapturer::setupPipeline()
{
    if (m_pipeline) {
        unregisterPipeline(m_pipeline);
        disconnectSimpleBusMessageCallback(pipeline());
    }

    m_pipeline = makeElement("pipeline"_s);
    auto clock = adoptGRef(gst_system_clock_obtain());
    gst_pipeline_use_clock(GST_PIPELINE(m_pipeline.get()), clock.get());
    gst_element_set_base_time(m_pipeline.get(), 0);
    gst_element_set_start_time(m_pipeline.get(), GST_CLOCK_TIME_NONE);

    registerActivePipeline(m_pipeline);
    connectSimpleBusMessageCallback(pipeline());

    auto source = createSource();
    auto converter = createConverter();

    m_valve = gst_element_factory_make("valve", nullptr);
    m_capsfilter = gst_element_factory_make("capsfilter", nullptr);
    auto queue = gst_element_factory_make("queue", nullptr);
    if (!m_sink)
        m_sink = makeElement("appsink"_s);

    gst_util_set_object_arg(G_OBJECT(m_capsfilter.get()), "caps-change-mode", "delayed");

    gst_app_sink_set_emit_signals(GST_APP_SINK(m_sink.get()), TRUE);
    g_object_set(m_sink.get(), "enable-last-sample", FALSE, nullptr);
    g_object_set(m_capsfilter.get(), "caps", m_caps.get(), nullptr);

    gst_bin_add_many(GST_BIN_CAST(m_pipeline.get()), source, m_capsfilter.get(), m_valve.get(), queue, m_sink.get(), nullptr);
    auto tail = source;
    if (converter) {
        gst_bin_add(GST_BIN_CAST(m_pipeline.get()), converter);
        gst_element_link(source, converter);
        tail = converter;
    }
    gst_element_link_many(tail, m_capsfilter.get(), m_valve.get(), queue, m_sink.get(), nullptr);
}

GstElement* GStreamerCapturer::makeElement(ASCIILiteral factoryName)
{
    auto* element = makeGStreamerElement(factoryName);
    auto elementName = makeString(unsafeSpan(name()), "_capturer_"_s, unsafeSpan(GST_OBJECT_NAME(element)), '_', hex(reinterpret_cast<uintptr_t>(this)));
    gst_object_set_name(GST_OBJECT(element), elementName.ascii().data());

    return element;
}

void GStreamerCapturer::start()
{
    if (!m_pipeline)
        setupPipeline();

    ASSERT(m_pipeline);
    GST_INFO_OBJECT(pipeline(), "Starting");
    gst_element_set_state(pipeline(), GST_STATE_PLAYING);
}

void GStreamerCapturer::stop()
{
    GST_INFO_OBJECT(pipeline(), "Stopping");
    tearDown(false);
}

bool GStreamerCapturer::isStopped() const
{
    if (!m_pipeline)
        return true;

    return GST_STATE(m_pipeline.get()) == GST_STATE_NULL;
}

std::pair<GstClockTime, GstClockTime> GStreamerCapturer::queryLatency()
{
    if (!m_sink)
        return { GST_CLOCK_TIME_NONE, GST_CLOCK_TIME_NONE };

    GstClockTime minLatency, maxLatency;
    if (gst_base_sink_query_latency(GST_BASE_SINK_CAST(m_sink.get()), nullptr, nullptr, &minLatency, &maxLatency))
        return { minLatency, maxLatency };

    return { GST_CLOCK_TIME_NONE, GST_CLOCK_TIME_NONE };
}

bool GStreamerCapturer::isInterrupted() const
{
    gboolean isInterrupted;
    g_object_get(m_valve.get(), "drop", &isInterrupted, nullptr);
    return isInterrupted;
}

void GStreamerCapturer::setInterrupted(bool isInterrupted)
{
    g_object_set(m_valve.get(), "drop", isInterrupted, nullptr);
}

void GStreamerCapturer::stopDevice(bool disconnectSignals)
{
    forEachObserver([](auto& observer) {
        observer.captureEnded();
    });
    if (disconnectSignals) {
        m_device.reset();
        m_caps = nullptr;
    }
    tearDown(disconnectSignals);
}

#undef GST_CAT_DEFAULT

} // namespace WebCore

#endif // ENABLE(VIDEO) && ENABLE(MEDIA_STREAM) && USE(GSTREAMER)
