/*
 *  Copyright (C) 2017-2022 Igalia S.L. All rights reserved.
 *  Copyright (C) 2022 Metrological Group B.V.
 *
 *  This library is free software; you can redistribute it and/or
 *  modify it under the terms of the GNU Lesser General Public
 *  License as published by the Free Software Foundation; either
 *  version 2 of the License, or (at your option) any later version.
 *
 *  This library is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 *  Lesser General Public License for more details.
 *
 *  You should have received a copy of the GNU Lesser General Public
 *  License along with this library; if not, write to the Free Software
 *  Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
 */

#pragma once

#if USE(GSTREAMER_WEBRTC)

#include "GRefPtrGStreamer.h"
#include "RealtimeMediaSource.h"

namespace WebCore {

class RealtimeIncomingSourceGStreamer : public RealtimeMediaSource, public ThreadSafeRefCountedAndCanMakeThreadSafeWeakPtr<RealtimeIncomingSourceGStreamer> {
public:
    WTF_ABSTRACT_THREAD_SAFE_REF_COUNTED_AND_CAN_MAKE_WEAK_PTR_IMPL;

    GstElement* bin() const { return m_bin.get(); }
    bool setBin(GRefPtr<GstElement> &&);

    void tearDown();

    bool hasClient(const GRefPtr<GstElement>&);
    int registerClient(GRefPtr<GstElement>&&);
    void unregisterClient(int);

    void handleUpstreamEvent(GRefPtr<GstEvent>&&);
    bool handleUpstreamQuery(GstQuery*);
    GstPadProbeReturn handleDownstreamEvent(GRefPtr<GstEvent>&&);

protected:
    RealtimeIncomingSourceGStreamer(const CaptureDevice&);

private:
    // RealtimeMediaSource API
    const RealtimeMediaSourceCapabilities& capabilities() final;

    virtual void dispatchSample(GRefPtr<GstSample>&&) = 0;

    void forEachClient(Function<void(GstElement*)>&&);

    GRefPtr<GstElement> m_bin;
    GRefPtr<GstElement> m_sink;
    unsigned long m_sinkPadProbeId;
    Lock m_clientLock;
    HashMap<int, GRefPtr<GstElement>> m_clients WTF_GUARDED_BY_LOCK(m_clientLock);
    MonotonicTime m_lastTagUpdate;
};

} // namespace WebCore

#endif // USE(GSTREAMER_WEBRTC)
