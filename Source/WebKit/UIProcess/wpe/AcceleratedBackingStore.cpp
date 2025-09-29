/*
 * Copyright (C) 2023 Igalia S.L.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE COMPUTER, INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE COMPUTER, INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "AcceleratedBackingStore.h"

#if ENABLE(WPE_PLATFORM)
#include "AcceleratedBackingStoreMessages.h"
#include "AcceleratedSurfaceMessages.h"
#include "WebPageProxy.h"
#include "WebProcessProxy.h"
#include <WebCore/ShareableBitmap.h>
#include <wpe/wpe-platform.h>
#include <wtf/TZoneMallocInlines.h>
#include <wtf/glib/GUniquePtr.h>

#if USE(LIBDRM)
#include <drm_fourcc.h>
#endif

namespace WebKit {

WTF_MAKE_TZONE_ALLOCATED_IMPL(AcceleratedBackingStore);

Ref<AcceleratedBackingStore> AcceleratedBackingStore::create(WebPageProxy& webPage, WPEView* view)
{
    return adoptRef(*new AcceleratedBackingStore(webPage, view));
}

AcceleratedBackingStore::AcceleratedBackingStore(WebPageProxy& webPage, WPEView* view)
    : m_webPage(webPage)
    , m_wpeView(view)
    , m_fenceMonitor([this] {
        if (m_webPage && m_pendingBuffer)
            renderPendingBuffer();
    })
    , m_legacyMainFrameProcess(webPage.legacyMainFrameProcess())
{
    g_signal_connect(m_wpeView.get(), "buffer-rendered", G_CALLBACK(+[](WPEView*, WPEBuffer*, gpointer userData) {
        auto& backingStore = *static_cast<AcceleratedBackingStore*>(userData);
        backingStore.bufferRendered();
    }), this);
    g_signal_connect(m_wpeView.get(), "buffer-released", G_CALLBACK(+[](WPEView*, WPEBuffer* buffer, gpointer userData) {
        auto& backingStore = *static_cast<AcceleratedBackingStore*>(userData);
        backingStore.bufferReleased(buffer);
    }), this);
}

AcceleratedBackingStore::~AcceleratedBackingStore()
{
    if (m_surfaceID) {
        if (RefPtr legacyMainFrameProcess = m_legacyMainFrameProcess.get())
            legacyMainFrameProcess->removeMessageReceiver(Messages::AcceleratedBackingStore::messageReceiverName(), m_surfaceID);
    }
    g_signal_handlers_disconnect_by_data(m_wpeView.get(), this);
}

void AcceleratedBackingStore::updateSurfaceID(uint64_t surfaceID)
{
    if (m_surfaceID == surfaceID)
        return;

    if (m_surfaceID) {
        if (m_pendingBuffer) {
            frameDone();
            m_pendingBuffer = nullptr;
            m_pendingDamageRects = { };
        }
        m_buffers.clear();
        m_bufferIDs.clear();
        if (RefPtr legacyMainFrameProcess = m_legacyMainFrameProcess.get())
            legacyMainFrameProcess->removeMessageReceiver(Messages::AcceleratedBackingStore::messageReceiverName(), m_surfaceID);
    }

    m_surfaceID = surfaceID;
    if (m_surfaceID && m_webPage) {
        m_legacyMainFrameProcess = m_webPage->legacyMainFrameProcess();
        Ref { *m_legacyMainFrameProcess }->addMessageReceiver(Messages::AcceleratedBackingStore::messageReceiverName(), m_surfaceID, *this);
    }
}

void AcceleratedBackingStore::didCreateDMABufBuffer(uint64_t id, const WebCore::IntSize& size, uint32_t format, Vector<WTF::UnixFileDescriptor>&& fds, Vector<uint32_t>&& offsets, Vector<uint32_t>&& strides, uint64_t modifier, RendererBufferFormat::Usage usage)
{
    Vector<int> fileDescriptors;
    fileDescriptors.reserveInitialCapacity(fds.size());
    for (auto& fd : fds)
        fileDescriptors.append(fd.release());
    GRefPtr<WPEBuffer> buffer = adoptGRef(WPE_BUFFER(wpe_buffer_dma_buf_new(wpe_view_get_display(m_wpeView.get()), size.width(), size.height(), format, fds.size(), fileDescriptors.mutableSpan().data(), offsets.mutableSpan().data(), strides.mutableSpan().data(), modifier)));
    g_object_set_data(G_OBJECT(buffer.get()), "wk-buffer-format-usage", GUINT_TO_POINTER(usage));
    m_bufferIDs.add(buffer.get(), id);
    m_buffers.add(id, WTFMove(buffer));
}

void AcceleratedBackingStore::didCreateSHMBuffer(uint64_t id, WebCore::ShareableBitmap::Handle&& handle)
{
    auto bitmap = WebCore::ShareableBitmap::create(WTFMove(handle), WebCore::SharedMemory::Protection::ReadOnly);
    if (!bitmap)
        return;

    auto size = bitmap->size();
    auto data = bitmap->span();
    auto stride = bitmap->bytesPerRow();
    GRefPtr<GBytes> bytes = adoptGRef(g_bytes_new_with_free_func(data.data(), data.size(), [](gpointer userData) {
        delete static_cast<WebCore::ShareableBitmap*>(userData);
    }, bitmap.leakRef()));

    GRefPtr<WPEBuffer> buffer = adoptGRef(WPE_BUFFER(wpe_buffer_shm_new(wpe_view_get_display(m_wpeView.get()), size.width(), size.height(), WPE_PIXEL_FORMAT_ARGB8888, bytes.get(), stride)));
    m_bufferIDs.add(buffer.get(), id);
    m_buffers.add(id, WTFMove(buffer));
}

void AcceleratedBackingStore::didDestroyBuffer(uint64_t id)
{
    if (auto buffer = m_buffers.take(id))
        m_bufferIDs.remove(buffer.get());
}

void AcceleratedBackingStore::frame(uint64_t bufferID, Rects&& damageRects, WTF::UnixFileDescriptor&& renderingFenceFD)
{
    ASSERT(!m_pendingBuffer);
    auto* buffer = m_buffers.get(bufferID);
    if (!buffer) {
        frameDone();
        return;
    }

    m_pendingBuffer = buffer;
    m_pendingDamageRects = WTFMove(damageRects);
    if (wpe_display_use_explicit_sync(wpe_view_get_display(m_wpeView.get()))) {
        wpe_buffer_set_rendering_fence(m_pendingBuffer.get(), renderingFenceFD.release());
        renderPendingBuffer();
    } else
        m_fenceMonitor.addFileDescriptor(WTFMove(renderingFenceFD));
}

void AcceleratedBackingStore::renderPendingBuffer()
{
    // Rely on the layout of IntRect matching that of WPERectangle
    // to pass directly a pointer below instead of using copies.
    static_assert(sizeof(WebCore::IntRect) == sizeof(WPERectangle));

    ASSERT(m_pendingDamageRects.size() <= std::numeric_limits<guint>::max());
    const auto* rects = !m_pendingDamageRects.isEmpty() ? reinterpret_cast<const WPERectangle*>(m_pendingDamageRects.span().data()) : nullptr;

    GUniqueOutPtr<GError> error;
    if (!wpe_view_render_buffer(m_wpeView.get(), m_pendingBuffer.get(), rects, m_pendingDamageRects.size(), &error.outPtr())) {
        g_warning("Failed to render frame: %s", error->message);
        frameDone();
        m_pendingBuffer = nullptr;
    }
    m_pendingDamageRects = { };
}

void AcceleratedBackingStore::frameDone()
{
    if (RefPtr legacyMainFrameProcess = m_legacyMainFrameProcess.get())
        legacyMainFrameProcess->send(Messages::AcceleratedSurface::FrameDone(), m_surfaceID);
}

void AcceleratedBackingStore::bufferRendered()
{
    frameDone();
    m_committedBuffer = WTFMove(m_pendingBuffer);
}

void AcceleratedBackingStore::bufferReleased(WPEBuffer* buffer)
{
    if (auto id = m_bufferIDs.get(buffer)) {
        auto releaseFence = UnixFileDescriptor { wpe_buffer_take_release_fence(buffer), UnixFileDescriptor::Adopt };

        if (RefPtr legacyMainFrameProcess = m_legacyMainFrameProcess.get())
            legacyMainFrameProcess->send(Messages::AcceleratedSurface::ReleaseBuffer(id, WTFMove(releaseFence)), m_surfaceID);
    }
}

RendererBufferDescription AcceleratedBackingStore::bufferDescription() const
{
    RendererBufferDescription description;
    auto* buffer = m_committedBuffer ? m_committedBuffer.get() : m_pendingBuffer.get();
    if (!buffer)
        return description;

    if (WPE_IS_BUFFER_DMA_BUF(buffer)) {
        auto* dmabuf = WPE_BUFFER_DMA_BUF(buffer);
        description.type = RendererBufferDescription::Type::DMABuf;
        description.fourcc = wpe_buffer_dma_buf_get_format(dmabuf);
        description.modifier = wpe_buffer_dma_buf_get_modifier(dmabuf);
        description.usage = static_cast<RendererBufferFormat::Usage>(GPOINTER_TO_UINT(g_object_get_data(G_OBJECT(buffer), "wk-buffer-format-usage")));
    } else if (WPE_IS_BUFFER_SHM(buffer)) {
        description.type = RendererBufferDescription::Type::SharedMemory;
        switch (wpe_buffer_shm_get_format(WPE_BUFFER_SHM(buffer))) {
        case WPE_PIXEL_FORMAT_ARGB8888:
#if USE(LIBDRM)
            description.fourcc = DRM_FORMAT_ARGB8888;
#endif
            break;
        }
        description.usage = RendererBufferFormat::Usage::Rendering;
    }

    return description;
}

} // namespace WebKit

#endif // ENABLE(WPE_PLATFORM)
