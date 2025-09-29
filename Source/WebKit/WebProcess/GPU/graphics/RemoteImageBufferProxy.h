/*
 * Copyright (C) 2020-2022 Apple Inc. All rights reserved.
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

#pragma once

#if ENABLE(GPU_PROCESS)

#include "ImageBufferBackendHandle.h"
#include "RemoteGraphicsContextProxy.h"
#include "RemoteRenderingBackendIdentifier.h"
#include "RemoteSerializedImageBufferIdentifier.h"
#include <WebCore/ImageBuffer.h>
#include <WebCore/ImageBufferBackend.h>
#include <wtf/Condition.h>
#include <wtf/Identified.h>
#include <wtf/Lock.h>
#include <wtf/TZoneMalloc.h>

namespace IPC {
class Connection;
}

namespace WebKit {

class RemoteRenderingBackendProxy;

class RemoteImageBufferProxy final : public WebCore::ImageBuffer {
    WTF_MAKE_TZONE_ALLOCATED(RemoteImageBufferProxy);
    friend class RemoteSerializedImageBufferProxy;
public:
    template<typename BackendType>
    static RefPtr<RemoteImageBufferProxy> create(const WebCore::FloatSize& size, float resolutionScale, const WebCore::DestinationColorSpace& colorSpace, WebCore::ImageBufferFormat bufferFormat, WebCore::RenderingPurpose purpose, RemoteRenderingBackendProxy& remoteRenderingBackendProxy)
    {
        Parameters parameters { size, resolutionScale, colorSpace, bufferFormat, purpose };
        auto backendParameters = ImageBuffer::backendParameters(parameters);
        if (BackendType::calculateSafeBackendSize(backendParameters).isEmpty())
            return nullptr;
        auto info = populateBackendInfo<BackendType>(backendParameters);
        return adoptRef(new RemoteImageBufferProxy(parameters, info, remoteRenderingBackendProxy));
    }
    static Ref<RemoteImageBufferProxy> create(const WebCore::ImageBuffer::Parameters& parameters, const WebCore::ImageBufferBackend::Info& info, RemoteRenderingBackendProxy& renderingBackend)
    {
        return adoptRef(*new RemoteImageBufferProxy(parameters, info, renderingBackend));
    }

    ~RemoteImageBufferProxy();
    bool isValid() const;

    void disconnect();

    WebCore::ImageBufferBackend* ensureBackend() const final;

    void backingStoreWillChange();
    std::unique_ptr<WebCore::SerializedImageBuffer> sinkIntoSerializedImageBuffer() final;

    void didReceiveMessage(IPC::Connection&, IPC::Decoder&);

    // Messages
    void didCreateBackend(std::optional<ImageBufferBackendHandle>);

    RemoteGraphicsContextIdentifier contextIdentifier() const { return m_context.identifier(); }
private:
    RemoteImageBufferProxy(Parameters, const WebCore::ImageBufferBackend::Info&, RemoteRenderingBackendProxy&);

    RefPtr<WebCore::NativeImage> copyNativeImage() const final;
    RefPtr<WebCore::NativeImage> createNativeImageReference() const final;
    RefPtr<WebCore::NativeImage> sinkIntoNativeImage() final;

    RefPtr<ImageBuffer> sinkIntoBufferForDifferentThread() final;

    RefPtr<WebCore::NativeImage> filteredNativeImage(WebCore::Filter&) final;

    WebCore::GraphicsContext& context() const final;

    RefPtr<WebCore::PixelBuffer> getPixelBuffer(const WebCore::PixelBufferFormat& destinationFormat, const WebCore::IntRect& srcRect, const WebCore::ImageBufferAllocator&) const final;
    void putPixelBuffer(const WebCore::PixelBufferSourceView&, const WebCore::IntRect& srcRect, const WebCore::IntPoint& destPoint = { }, WebCore::AlphaPremultiplication = WebCore::AlphaPremultiplication::Premultiplied) final;

    void convertToLuminanceMask() final;
    void transformToColorSpace(const WebCore::DestinationColorSpace&) final;

    void flushDrawingContext() final;
    bool flushDrawingContextAsync() final;

    void prepareForBackingStoreChange();

    void assertDispatcherIsCurrent() const;
    template<typename T> void send(T&& message);
    template<typename T> auto sendSync(T&& message);
    RefPtr<IPC::StreamClientConnection> connection() const;
    void didBecomeUnresponsive() const;

    mutable RemoteGraphicsContextProxy m_context;
    WeakPtr<RemoteRenderingBackendProxy> m_renderingBackend;
};

class RemoteSerializedImageBufferProxy : public WebCore::SerializedImageBuffer, public Identified<RemoteSerializedImageBufferIdentifier> {
    WTF_MAKE_TZONE_ALLOCATED(RemoteSerializedImageBufferProxy);
    friend class RemoteRenderingBackendProxy;
public:
    ~RemoteSerializedImageBufferProxy();

    static RefPtr<WebCore::ImageBuffer> sinkIntoImageBuffer(std::unique_ptr<RemoteSerializedImageBufferProxy>, RemoteRenderingBackendProxy&);

    RemoteSerializedImageBufferProxy(WebCore::ImageBuffer::Parameters, const WebCore::ImageBufferBackend::Info&, RemoteRenderingBackendProxy&);

    size_t memoryCost() const final
    {
        return m_info.memoryCost;
    }

    const WebCore::ImageBuffer::Parameters& parameters() const { return m_parameters; }
    const WebCore::ImageBufferBackend::Info& info() const { return m_info; }
private:
    RefPtr<WebCore::ImageBuffer> sinkIntoImageBuffer() final
    {
        ASSERT_NOT_REACHED();
        return nullptr;
    }

    bool isRemoteSerializedImageBufferProxy() const final { return true; }

    const WebCore::ImageBuffer::Parameters m_parameters;
    const WebCore::ImageBufferBackend::Info m_info;
    RefPtr<IPC::Connection> m_connection;
};

} // namespace WebKit

SPECIALIZE_TYPE_TRAITS_BEGIN(WebKit::RemoteSerializedImageBufferProxy)
    static bool isType(const WebCore::SerializedImageBuffer& buffer) { return buffer.isRemoteSerializedImageBufferProxy(); }
SPECIALIZE_TYPE_TRAITS_END()


#endif // ENABLE(GPU_PROCESS)
