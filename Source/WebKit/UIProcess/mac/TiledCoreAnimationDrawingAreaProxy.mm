/*
 * Copyright (C) 2011-2025 Apple Inc. All rights reserved.
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
#import "TiledCoreAnimationDrawingAreaProxy.h"

#if ENABLE(TILED_CA_DRAWING_AREA)

#import "DrawingAreaMessages.h"
#import "DrawingAreaProxyMessages.h"
#import "LayerTreeContext.h"
#import "MessageSenderInlines.h"
#import "WebPageProxy.h"
#import "WebPageProxyMessages.h"
#import "WebProcessProxy.h"
#import <pal/spi/cocoa/QuartzCoreSPI.h>
#import <wtf/BlockPtr.h>
#import <wtf/MachSendRight.h>
#import <wtf/TZoneMallocInlines.h>

namespace WebKit {
using namespace IPC;
using namespace WebCore;

WTF_MAKE_TZONE_ALLOCATED_IMPL(TiledCoreAnimationDrawingAreaProxy);

Ref<TiledCoreAnimationDrawingAreaProxy> TiledCoreAnimationDrawingAreaProxy::create(WebPageProxy& page, WebProcessProxy& webProcessProxy)
{
    return adoptRef(*new TiledCoreAnimationDrawingAreaProxy(page, webProcessProxy));
}

TiledCoreAnimationDrawingAreaProxy::TiledCoreAnimationDrawingAreaProxy(WebPageProxy& webPageProxy, WebProcessProxy& webProcessProxy)
    : DrawingAreaProxy(webPageProxy, webProcessProxy)
{
}

TiledCoreAnimationDrawingAreaProxy::~TiledCoreAnimationDrawingAreaProxy() = default;

void TiledCoreAnimationDrawingAreaProxy::deviceScaleFactorDidChange(CompletionHandler<void()>&& completionHandler)
{
    if (RefPtr page = this->page())
        sendWithAsyncReply(Messages::DrawingArea::SetDeviceScaleFactor(page->deviceScaleFactor()), WTFMove(completionHandler));
}

void TiledCoreAnimationDrawingAreaProxy::sizeDidChange()
{
    RefPtr page = this->page();
    if (!page || !page->hasRunningProcess())
        return;

    // We only want one UpdateGeometry message in flight at once, so if we've already sent one but
    // haven't yet received the reply we'll just return early here.
    if (m_isWaitingForDidUpdateGeometry)
        return;

    sendUpdateGeometry();
}

void TiledCoreAnimationDrawingAreaProxy::colorSpaceDidChange()
{
    if (RefPtr page = this->page())
        send(Messages::DrawingArea::SetColorSpace(page->colorSpace()));
}

void TiledCoreAnimationDrawingAreaProxy::minimumSizeForAutoLayoutDidChange()
{
    RefPtr page = this->page();
    if (!page || !page->hasRunningProcess())
        return;

    // We only want one UpdateGeometry message in flight at once, so if we've already sent one but
    // haven't yet received the reply we'll just return early here.
    if (m_isWaitingForDidUpdateGeometry)
        return;

    sendUpdateGeometry();
}

void TiledCoreAnimationDrawingAreaProxy::sizeToContentAutoSizeMaximumSizeDidChange()
{
    RefPtr page = this->page();
    if (!page || !page->hasRunningProcess())
        return;

    // We only want one UpdateGeometry message in flight at once, so if we've already sent one but
    // haven't yet received the reply we'll just return early here.
    if (m_isWaitingForDidUpdateGeometry)
        return;

    sendUpdateGeometry();
}

void TiledCoreAnimationDrawingAreaProxy::enterAcceleratedCompositingMode(uint64_t backingStoreStateID, const LayerTreeContext& layerTreeContext)
{
    if (RefPtr page = this->page())
        page->enterAcceleratedCompositingMode(layerTreeContext);
}

void TiledCoreAnimationDrawingAreaProxy::updateAcceleratedCompositingMode(uint64_t backingStoreStateID, const LayerTreeContext& layerTreeContext)
{
    if (RefPtr page = this->page())
        page->updateAcceleratedCompositingMode(layerTreeContext);
}

void TiledCoreAnimationDrawingAreaProxy::didFirstLayerFlush(uint64_t /* backingStoreStateID */, const LayerTreeContext& layerTreeContext)
{
    if (RefPtr page = this->page())
        page->didFirstLayerFlush(layerTreeContext);
}

void TiledCoreAnimationDrawingAreaProxy::didUpdateGeometry()
{
    ASSERT(m_isWaitingForDidUpdateGeometry);

    m_isWaitingForDidUpdateGeometry = false;

    RefPtr page = this->page();
    if (!page)
        return;

    IntSize minimumSizeForAutoLayout = page->minimumSizeForAutoLayout();
    IntSize sizeToContentAutoSizeMaximumSize = page->sizeToContentAutoSizeMaximumSize();

    // If the WKView was resized while we were waiting for a DidUpdateGeometry reply from the web process,
    // we need to resend the new size here.
    if (m_lastSentSize != size() || m_lastSentMinimumSizeForAutoLayout != minimumSizeForAutoLayout || m_lastSentSizeToContentAutoSizeMaximumSize != sizeToContentAutoSizeMaximumSize)
        sendUpdateGeometry();
}

void TiledCoreAnimationDrawingAreaProxy::waitForDidUpdateActivityState(ActivityStateChangeID)
{
    RefPtr page = this->page();
    if (!page)
        return;

    Seconds activityStateUpdateTimeout = Seconds::fromMilliseconds(250);
    webProcessProxy().protectedConnection()->waitForAndDispatchImmediately<Messages::WebPageProxy::DidUpdateActivityState>(page->webPageIDInMainFrameProcess(), activityStateUpdateTimeout, IPC::WaitForOption::InterruptWaitingIfSyncMessageArrives);
}

void TiledCoreAnimationDrawingAreaProxy::willSendUpdateGeometry()
{
    RefPtr page = this->page();
    if (!page)
        return;

    m_lastSentMinimumSizeForAutoLayout = page->minimumSizeForAutoLayout();
    m_lastSentSizeToContentAutoSizeMaximumSize = page->sizeToContentAutoSizeMaximumSize();
    m_lastSentSize = size();
    m_isWaitingForDidUpdateGeometry = true;
}

MachSendRight TiledCoreAnimationDrawingAreaProxy::createFence()
{
    RefPtr page = this->page();
    if (!page)
        return MachSendRight();

    RetainPtr<CAContext> rootLayerContext = [page->acceleratedCompositingRootLayer() context];
    if (!rootLayerContext)
        return MachSendRight();

    // Don't fence if we don't have a connection, because the message
    // will likely get dropped on the floor (if the Web process is terminated)
    // or queued up until process launch completes, and there's nothing useful
    // to synchronize in these cases.
    if (!webProcessProxy().hasConnection())
        return MachSendRight();

    Ref connection = webProcessProxy().connection();

    // Don't fence if we have incoming synchronous messages, because we may not
    // be able to reply to the message until the fence times out.
    if (connection->hasIncomingSyncMessage())
        return MachSendRight();

    MachSendRight fencePort = MachSendRight::adopt([rootLayerContext createFencePort]);

    // Invalidate the fence if a synchronous message arrives while it's installed,
    // because we won't be able to reply during the fence-wait.
    uint64_t callbackID = connection->installIncomingSyncMessageCallback([rootLayerContext] {
        [rootLayerContext invalidateFences];
    });
    [CATransaction addCommitHandler:[callbackID, connection = WTFMove(connection)] () mutable {
        connection->uninstallIncomingSyncMessageCallback(callbackID);
    } forPhase:kCATransactionPhasePostCommit];

    return fencePort;
}

void TiledCoreAnimationDrawingAreaProxy::sendUpdateGeometry()
{
    ASSERT(!m_isWaitingForDidUpdateGeometry);

    willSendUpdateGeometry();
    sendWithAsyncReply(Messages::DrawingArea::UpdateGeometry(size(), true /* flushSynchronously */, createFence()), [weakThis = WeakPtr { *this }] {
        if (!weakThis)
            return;
        weakThis->didUpdateGeometry();
    });
}

void TiledCoreAnimationDrawingAreaProxy::adjustTransientZoom(double scale, FloatPoint originInLayerForPageScale, WebCore::FloatPoint)
{
    send(Messages::DrawingArea::AdjustTransientZoom(scale, originInLayerForPageScale));
}

void TiledCoreAnimationDrawingAreaProxy::commitTransientZoom(double scale, FloatPoint origin)
{
    sendWithAsyncReply(Messages::DrawingArea::CommitTransientZoom(scale, origin), [] { });
}

void TiledCoreAnimationDrawingAreaProxy::dispatchPresentationCallbacksAfterFlushingLayers(IPC::Connection& connection, Vector<IPC::AsyncReplyID>&& callbackIDs)
{
    for (auto& callbackID : callbackIDs) {
        removeOutstandingPresentationUpdateCallback(connection, callbackID);
        if (auto callback = connection.takeAsyncReplyHandler(callbackID))
            callback(nullptr, nullptr);
    }
}

std::optional<WebCore::FramesPerSecond> TiledCoreAnimationDrawingAreaProxy::displayNominalFramesPerSecond()
{
    if (!page() || !page()->displayID())
        return std::nullopt;
    return webProcessProxy().nominalFramesPerSecondForDisplay(*page()->displayID());
}

} // namespace WebKit

#endif // ENABLE(TILED_CA_DRAWING_AREA)
