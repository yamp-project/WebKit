/*
 * Copyright (C) 2020 Apple Inc. All rights reserved.
 * Copyright (C) 2021 Sony Interactive Entertainment Inc.
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

#include "config.h"
#include "RemoteGraphicsContextGL.h"

#if ENABLE(GPU_PROCESS) && ENABLE(WEBGL) && USE(GRAPHICS_LAYER_WC)

#include "GPUConnectionToWebProcess.h"
#include "WCContentBufferManager.h"
#include <wtf/TZoneMalloc.h>
#include <wtf/TZoneMallocInlines.h>

namespace WebKit {

class RemoteGraphicsContextGLWC final : public RemoteGraphicsContextGL {
    WTF_MAKE_TZONE_ALLOCATED(RemoteGraphicsContextGLWC);
    WTF_OVERRIDE_DELETE_FOR_CHECKED_PTR(RemoteGraphicsContextGLWC);

public:
    RemoteGraphicsContextGLWC(GPUConnectionToWebProcess&, RemoteGraphicsContextGLIdentifier, RemoteRenderingBackend&, Ref<IPC::StreamServerConnection>&&);
    ~RemoteGraphicsContextGLWC() final = default;

    // RemoteGraphicsContextGL overrides.
    void platformWorkQueueInitialize(WebCore::GraphicsContextGLAttributes&&) final;
    void prepareForDisplay(CompletionHandler<void(std::optional<WCContentBufferIdentifier>)>&&) final;
private:
    WebCore::ProcessIdentifier m_webProcessIdentifier;
};

WTF_MAKE_TZONE_ALLOCATED_IMPL(RemoteGraphicsContextGLWC);

Ref<RemoteGraphicsContextGL> RemoteGraphicsContextGL::create(GPUConnectionToWebProcess& gpuConnectionToWebProcess, WebCore::GraphicsContextGLAttributes&& attributes, RemoteGraphicsContextGLIdentifier graphicsContextGLIdentifier, RemoteRenderingBackend& renderingBackend, Ref<IPC::StreamServerConnection>&& streamConnection)
{
    auto instance = adoptRef(*new RemoteGraphicsContextGLWC(gpuConnectionToWebProcess, graphicsContextGLIdentifier, renderingBackend, WTFMove(streamConnection)));
    instance->initialize(WTFMove(attributes));
    return instance;
}

RemoteGraphicsContextGLWC::RemoteGraphicsContextGLWC(GPUConnectionToWebProcess& gpuConnectionToWebProcess, RemoteGraphicsContextGLIdentifier graphicsContextGLIdentifier, RemoteRenderingBackend& renderingBackend, Ref<IPC::StreamServerConnection>&& streamConnection)
    : RemoteGraphicsContextGL(gpuConnectionToWebProcess, graphicsContextGLIdentifier, renderingBackend, WTFMove(streamConnection))
    , m_webProcessIdentifier(gpuConnectionToWebProcess.webProcessIdentifier())
{
}

void RemoteGraphicsContextGLWC::platformWorkQueueInitialize(WebCore::GraphicsContextGLAttributes&& attributes)
{
    m_context = GCGLContext::create(WTFMove(attributes));
}

void RemoteGraphicsContextGLWC::prepareForDisplay(CompletionHandler<void(std::optional<WCContentBufferIdentifier>)>&& completionHandler)
{
    m_context->prepareForDisplay();
    auto identifier = WCContentBufferManager::singleton().acquireContentBufferIdentifier(m_webProcessIdentifier, m_context->layerContentsDisplayDelegate()->platformLayer());
    completionHandler(identifier);
}

}

#endif // ENABLE(GPU_PROCESS) && ENABLE(WEBGL) && USE(GRAPHICS_LAYER_WC)
