/*
 * Copyright (C) 2018-2023 Apple Inc. All rights reserved.
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

#pragma once

#include "RemoteAcceleratedEffectStack.h"
#include "RemoteLayerBackingStore.h"
#include <WebCore/EventRegion.h>
#include <WebCore/IOSurface.h>
#include <WebCore/LayerHostingContextIdentifier.h>
#include <WebCore/PlatformLayerIdentifier.h>
#include <WebCore/RenderingResourceIdentifier.h>
#include <WebCore/ScrollTypes.h>
#include <wtf/RefCountedAndCanMakeWeakPtr.h>
#include <wtf/RetainPtr.h>
#include <wtf/TZoneMalloc.h>
#include <wtf/Vector.h>

OBJC_CLASS CALayer;
OBJC_CLASS NSString;
#if PLATFORM(IOS_FAMILY)
OBJC_CLASS UIView;
#endif

namespace WebKit {

class RemoteLayerTreeHost;
class RemoteLayerTreeScrollbars;

class RemoteLayerTreeNode final : public RefCountedAndCanMakeWeakPtr<RemoteLayerTreeNode> {
    WTF_MAKE_TZONE_ALLOCATED(RemoteLayerTreeNode);
public:
    static Ref<RemoteLayerTreeNode> create(WebCore::PlatformLayerIdentifier, Markable<WebCore::LayerHostingContextIdentifier>, RetainPtr<CALayer>);
#if PLATFORM(IOS_FAMILY)
    static Ref<RemoteLayerTreeNode> create(WebCore::PlatformLayerIdentifier, Markable<WebCore::LayerHostingContextIdentifier>, RetainPtr<UIView>);
#endif
    ~RemoteLayerTreeNode();

    static Ref<RemoteLayerTreeNode> createWithPlainLayer(WebCore::PlatformLayerIdentifier);

    CALayer *layer() const { return m_layer.get(); }
    RetainPtr<CALayer> protectedLayer() const;
#if ENABLE(GAZE_GLOW_FOR_INTERACTION_REGIONS) || HAVE(CORE_ANIMATION_SEPARATED_LAYERS)
    const Markable<WebCore::FloatRect> visibleRect() const { return m_visibleRect; }
    void setVisibleRect(const WebCore::FloatRect& value) { m_visibleRect = value; }
#endif

#if ENABLE(GAZE_GLOW_FOR_INTERACTION_REGIONS)
    CALayer *ensureInteractionRegionsContainer();
    void removeInteractionRegionsContainer();
    void updateInteractionRegionAfterHierarchyChange();
#endif

#if HAVE(CORE_ANIMATION_SEPARATED_LAYERS)
    bool shouldBeSeparated() const { return m_shouldBeSeparated; }
    void setShouldBeSeparated(bool value) { m_shouldBeSeparated = value; }
#endif

#if PLATFORM(IOS_FAMILY)
    UIView *uiView() const { return m_uiView.get(); }
#endif

    WebCore::PlatformLayerIdentifier layerID() const { return m_layerID; }

    const WebCore::EventRegion& eventRegion() const { return m_eventRegion; }
    void setEventRegion(const WebCore::EventRegion&);

    // Non-ancestor scroller that controls positioning of the layer.
    std::optional<WebCore::PlatformLayerIdentifier> actingScrollContainerID() const { return m_actingScrollContainerID.asOptional(); }
    // Ancestor scrollers that don't affect positioning of the layer.
    const Vector<WebCore::PlatformLayerIdentifier>& stationaryScrollContainerIDs() const { return m_stationaryScrollContainerIDs; }

    void setActingScrollContainerID(std::optional<WebCore::PlatformLayerIdentifier> value) { m_actingScrollContainerID = value; }
    void setStationaryScrollContainerIDs(Vector<WebCore::PlatformLayerIdentifier>&& value) { m_stationaryScrollContainerIDs = WTFMove(value); }

    void detachFromParent();

    static std::optional<WebCore::PlatformLayerIdentifier> layerID(CALayer *);
    static RemoteLayerTreeNode* forCALayer(CALayer *);

    static NSString *appendLayerDescription(NSString *description, CALayer *);

#if ENABLE(SCROLLING_THREAD)
    std::optional<WebCore::ScrollingNodeID> scrollingNodeID() const { return m_scrollingNodeID; }
    void setScrollingNodeID(std::optional<WebCore::ScrollingNodeID> nodeID) { m_scrollingNodeID = nodeID; }
#endif

    Markable<WebCore::LayerHostingContextIdentifier> remoteContextHostingIdentifier() const { return m_remoteContextHostingIdentifier; }
    Markable<WebCore::LayerHostingContextIdentifier> remoteContextHostedIdentifier() const { return m_remoteContextHostedIdentifier; }
    void setRemoteContextHostedIdentifier(WebCore::LayerHostingContextIdentifier identifier) { m_remoteContextHostedIdentifier = identifier; }
    void addToHostingNode(RemoteLayerTreeNode&);
    void removeFromHostingNode();

    void applyBackingStore(RemoteLayerTreeHost*, RemoteLayerBackingStoreProperties&);

    // A cached CAIOSurface object to retain CA render resources.
    struct CachedContentsBuffer {
        BufferAndBackendInfo imageBufferInfo;
        RetainPtr<id> buffer;
        std::unique_ptr<WebCore::IOSurface> ioSurface;
    };

    Vector<CachedContentsBuffer> takeCachedContentsBuffers() { return std::exchange(m_cachedContentsBuffers, { }); }
    void setCachedContentsBuffers(Vector<CachedContentsBuffer>&& buffers)
    {
        m_cachedContentsBuffers = WTFMove(buffers);
    }

    std::optional<WebCore::RenderingResourceIdentifier> asyncContentsIdentifier() const
    {
        return m_asyncContentsIdentifier;
    }

    void setAsyncContentsIdentifier(std::optional<WebCore::RenderingResourceIdentifier> identifier)
    {
        m_asyncContentsIdentifier = identifier;
    }

#if ENABLE(THREADED_ANIMATION_RESOLUTION)
    void setAcceleratedEffectsAndBaseValues(const WebCore::AcceleratedEffects&, const WebCore::AcceleratedEffectValues&, RemoteLayerTreeHost&);
    const RemoteAcceleratedEffectStack* effectStack() const { return m_effectStack.get(); }
    RefPtr<RemoteAcceleratedEffectStack> takeEffectStack() { return std::exchange(m_effectStack, nullptr); }
#endif

    bool backdropRootIsOpaque() const { return m_backdropRootIsOpaque; }
    void setBackdropRootIsOpaque(bool backdropRootIsOpaque) { m_backdropRootIsOpaque = backdropRootIsOpaque; }

private:
    RemoteLayerTreeNode(WebCore::PlatformLayerIdentifier, Markable<WebCore::LayerHostingContextIdentifier>, RetainPtr<CALayer>);
#if PLATFORM(IOS_FAMILY)
    RemoteLayerTreeNode(WebCore::PlatformLayerIdentifier, Markable<WebCore::LayerHostingContextIdentifier>, RetainPtr<UIView>);
#endif

    void initializeLayer();

    const WebCore::PlatformLayerIdentifier m_layerID;
    Markable<WebCore::LayerHostingContextIdentifier> m_remoteContextHostingIdentifier;
    Markable<WebCore::LayerHostingContextIdentifier> m_remoteContextHostedIdentifier;

    const RetainPtr<CALayer> m_layer;

#if ENABLE(GAZE_GLOW_FOR_INTERACTION_REGIONS) || HAVE(CORE_ANIMATION_SEPARATED_LAYERS)
    Markable<WebCore::FloatRect> m_visibleRect;
#endif

#if ENABLE(GAZE_GLOW_FOR_INTERACTION_REGIONS)
    void repositionInteractionRegionsContainerIfNeeded();
    enum class InteractionRegionsInSubtree : bool { Yes, Unknown };
    void propagateInteractionRegionsChangeInHierarchy(InteractionRegionsInSubtree);

    bool hasInteractionRegions() const;
    bool hasInteractionRegionsDescendant() const { return m_hasInteractionRegionsDescendant; }
    void setHasInteractionRegionsDescendant(bool value) { m_hasInteractionRegionsDescendant = value; }

    bool m_hasInteractionRegionsDescendant { false };
    RetainPtr<UIView> m_interactionRegionsContainer;
#endif

#if HAVE(CORE_ANIMATION_SEPARATED_LAYERS)
    bool m_shouldBeSeparated { false };
#endif

#if PLATFORM(IOS_FAMILY)
    RetainPtr<UIView> m_uiView;
#endif

    WebCore::EventRegion m_eventRegion;

#if ENABLE(SCROLLING_THREAD)
    Markable<WebCore::ScrollingNodeID> m_scrollingNodeID;
#endif

    Markable<WebCore::PlatformLayerIdentifier> m_actingScrollContainerID;
    Vector<WebCore::PlatformLayerIdentifier> m_stationaryScrollContainerIDs;

    Vector<CachedContentsBuffer> m_cachedContentsBuffers;
    std::optional<WebCore::RenderingResourceIdentifier> m_asyncContentsIdentifier;

#if ENABLE(THREADED_ANIMATION_RESOLUTION)
    RefPtr<RemoteAcceleratedEffectStack> m_effectStack;
#endif
    bool m_backdropRootIsOpaque { false };
};

}
