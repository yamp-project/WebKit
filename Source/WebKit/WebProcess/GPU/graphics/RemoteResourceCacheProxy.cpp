/*
 * Copyright (C) 2020-2023 Apple Inc. All rights reserved.
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
#include "RemoteResourceCacheProxy.h"

#if ENABLE(GPU_PROCESS)

#include "ArgumentCoders.h"
#include "Logging.h"
#include "RemoteImageBufferProxy.h"
#include "RemoteRenderingBackendProxy.h"
#include "WebProcess.h"
#include <WebCore/FontCustomPlatformData.h>

namespace WebKit {
using namespace WebCore;

namespace {
struct CreateShareableBitmapResult {
    Ref<ShareableBitmap> bitmap;
    PlatformImagePtr platformImage;
};
}

static std::optional<CreateShareableBitmapResult> createShareableBitmapForNativeImage(NativeImage& image, const DestinationColorSpace& fallbackColorSpace)
{
    RefPtr<ShareableBitmap> bitmap;
    PlatformImagePtr platformImage;
#if USE(CG)
    bitmap = ShareableBitmap::createFromImagePixels(image);
    if (bitmap)
        platformImage = bitmap->createPlatformImage(DontCopyBackingStore, ShouldInterpolate::Yes);
#endif

    // If we failed to create ShareableBitmap or PlatformImage, fall back to image-draw method.
    if (!platformImage) {
        bitmap = ShareableBitmap::createFromImageDraw(image, fallbackColorSpace);
        if (bitmap)
            platformImage = bitmap->createPlatformImage(DontCopyBackingStore, ShouldInterpolate::Yes);

        // If createGraphicsContext() failed because the image fallbackColorSpace is not
        // supported for output, fallback to SRGB.
        if (!platformImage) {
            bitmap = ShareableBitmap::createFromImageDraw(image, DestinationColorSpace::SRGB());
            if (bitmap)
                platformImage = bitmap->createPlatformImage(DontCopyBackingStore, ShouldInterpolate::Yes);
        }
    }
    if (!platformImage)
        return std::nullopt;
    return CreateShareableBitmapResult { bitmap.releaseNonNull(), WTFMove(platformImage) };
}



RemoteResourceCacheProxy::RemoteResourceCacheProxy(RemoteRenderingBackendProxy& remoteRenderingBackendProxy)
    : m_remoteRenderingBackendProxy(remoteRenderingBackendProxy)
{
}

RemoteResourceCacheProxy::~RemoteResourceCacheProxy()
{
}

RemoteGradientIdentifier RemoteResourceCacheProxy::recordGradientUse(Gradient& gradient)
{
    auto result = m_gradients.ensure(&gradient, [] {
        return RemoteGradientIdentifier::generate();
    });
    auto identifier = result.iterator->value;
    if (result.isNewEntry) {
        gradient.addObserver(m_resourceObserverWeakFactory.createWeakPtr(static_cast<RenderingResourceObserver&>(*this)).releaseNonNull());
        m_remoteRenderingBackendProxy->cacheGradient(gradient, identifier);
    }
    return identifier;
}

void RemoteResourceCacheProxy::recordFilterUse(Filter& filter)
{
    if (m_filters.add(filter.renderingResourceIdentifier()).isNewEntry) {
        filter.addObserver(m_resourceObserverWeakFactory.createWeakPtr(static_cast<RenderingResourceObserver&>(*this)).releaseNonNull());
        m_remoteRenderingBackendProxy->cacheFilter(filter);
    }
}

void RemoteResourceCacheProxy::recordNativeImageUse(NativeImage& image, const DestinationColorSpace& fallbackColorSpace)
{
    if (isMainRunLoop())
        WebProcess::singleton().deferNonVisibleProcessEarlyMemoryCleanupTimer();
    std::optional<ShareableBitmap::Handle> handle;
    auto entry = m_nativeImages.find(&image);
    if (entry != m_nativeImages.end()) {
        if (entry->value.existsInRemote)
            return;
        handle = RefPtr { entry->value.bitmap }->createHandle();
        if (handle)
            entry->value.existsInRemote = true;
    } else {
        auto result = createShareableBitmapForNativeImage(image, fallbackColorSpace);
        if (result) {
            Ref bitmap = WTFMove(result->bitmap);
            PlatformImagePtr platformImage = WTFMove(result->platformImage);
            handle = bitmap->createHandle();
            if (handle) {
                handle->takeOwnershipOfMemory(MemoryLedger::Graphics);
                m_nativeImages.add(&image, NativeImageEntry { WTFMove(bitmap), true });
                // Set itself as an observer to NativeImage, so releaseNativeImage()
                // gets called when NativeImage is being deleted.
                image.addObserver(m_nativeImageResourceObserverWeakFactory.createWeakPtr(static_cast<RenderingResourceObserver&>(*this)).releaseNonNull());
                // Replace the contents of the original NativeImage to save memory.
                image.replacePlatformImage(WTFMove(platformImage));
            }
        }
    }
    if (!handle) {
        // FIXME: Failing to send the image to GPUP will crash it when referencing this image.
        LOG_WITH_STREAM(Images, stream
            << "RemoteResourceCacheProxy::recordNativeImageUse() " << this
            << " image.size(): " << image.size()
            << " image.colorSpace(): " << image.colorSpace()
            << " ShareableBitmap could not be created; bailing.");
        return;
    }
    m_remoteRenderingBackendProxy->cacheNativeImage(WTFMove(*handle), image.renderingResourceIdentifier());
}

void RemoteResourceCacheProxy::recordFontUse(Font& font)
{
    if (RefPtr platformData = font.platformData().customPlatformData())
        recordFontCustomPlatformDataUse(*platformData);

    auto result = m_fonts.add(font.renderingResourceIdentifier(), m_renderingUpdateID);

    if (result.isNewEntry) {
        auto renderingResourceIdentifier = font.platformData().customPlatformData() ? std::optional(font.platformData().customPlatformData()->m_renderingResourceIdentifier) : std::nullopt;
        m_remoteRenderingBackendProxy->cacheFont(font.attributes(), font.platformData().attributes(), renderingResourceIdentifier);
        ++m_numberOfFontsUsedInCurrentRenderingUpdate;
        return;
    }

    auto& currentState = result.iterator->value;
    if (currentState != m_renderingUpdateID) {
        currentState = m_renderingUpdateID;
        ++m_numberOfFontsUsedInCurrentRenderingUpdate;
    }
}

void RemoteResourceCacheProxy::recordFontCustomPlatformDataUse(const FontCustomPlatformData& customPlatformData)
{
    auto result = m_fontCustomPlatformDatas.add(customPlatformData.m_renderingResourceIdentifier, m_renderingUpdateID);

    if (result.isNewEntry) {
        m_remoteRenderingBackendProxy->cacheFontCustomPlatformData(customPlatformData);
        ++m_numberOfFontCustomPlatformDatasUsedInCurrentRenderingUpdate;
        return;
    }

    auto& currentState = result.iterator->value;
    if (currentState != m_renderingUpdateID) {
        currentState = m_renderingUpdateID;
        ++m_numberOfFontCustomPlatformDatasUsedInCurrentRenderingUpdate;
    }
}

RemoteDisplayListIdentifier RemoteResourceCacheProxy::recordDisplayListUse(const DisplayList::DisplayList& displayList)
{
    auto result = m_displayLists.ensure(&displayList, [] {
        return RemoteDisplayListIdentifier::generate();
    });
    auto identifier = result.iterator->value; // Stash the identifier since the next call will recurse.
    if (result.isNewEntry) {
        displayList.addObserver(m_resourceObserverWeakFactory.createWeakPtr(static_cast<RenderingResourceObserver&>(*this)).releaseNonNull());
        // Note: this might recurse back to RemoteResourceCacheProxy::recordDisplayListUse().
        // thus we must ensure that we are not in m_displayLists.ensure.. call stack.
        m_remoteRenderingBackendProxy->cacheDisplayList(identifier, displayList);
        // result.iterator is not valid anymore.
    }
    return identifier;
}

void RemoteResourceCacheProxy::willDestroyNativeImage(const NativeImage& image)
{
    auto entry = m_nativeImages.takeOptional(&image);
    RELEASE_ASSERT(entry);
    if (!entry->existsInRemote)
        return;
    m_remoteRenderingBackendProxy->releaseNativeImage(image.renderingResourceIdentifier());
}

void RemoteResourceCacheProxy::willDestroyGradient(const Gradient& gradient)
{
    auto identifier = m_gradients.take(&gradient);
    RELEASE_ASSERT(identifier);
    m_remoteRenderingBackendProxy->releaseGradient(*identifier);
}

void RemoteResourceCacheProxy::willDestroyFilter(RenderingResourceIdentifier identifier)
{
    bool removed = m_filters.remove(identifier);
    RELEASE_ASSERT(removed);
    m_remoteRenderingBackendProxy->releaseFilter(identifier);
}

void RemoteResourceCacheProxy::willDestroyDisplayList(const DisplayList::DisplayList& displayList)
{
    auto identifier = m_displayLists.take(&displayList);
    RELEASE_ASSERT(identifier);
    m_remoteRenderingBackendProxy->releaseDisplayList(*identifier);
}

void RemoteResourceCacheProxy::releaseNativeImages()
{
    m_nativeImageResourceObserverWeakFactory.revokeAll();
    m_nativeImages.clear();
}

void RemoteResourceCacheProxy::prepareForNextRenderingUpdate()
{
    m_numberOfFontsUsedInCurrentRenderingUpdate = 0;
    m_numberOfFontCustomPlatformDatasUsedInCurrentRenderingUpdate = 0;
}

void RemoteResourceCacheProxy::releaseFonts()
{
    m_fonts.clear();
    m_numberOfFontsUsedInCurrentRenderingUpdate = 0;
}

void RemoteResourceCacheProxy::releaseFontCustomPlatformDatas()
{
    m_fontCustomPlatformDatas.clear();
    m_numberOfFontCustomPlatformDatasUsedInCurrentRenderingUpdate = 0;
}

void RemoteResourceCacheProxy::finalizeRenderingUpdateForFonts()
{
    static constexpr unsigned minimumRenderingUpdateCountToKeepFontAlive = 4;

    unsigned totalFontCount = m_fonts.size();
    RELEASE_ASSERT(m_numberOfFontsUsedInCurrentRenderingUpdate <= totalFontCount);
    if (totalFontCount != m_numberOfFontsUsedInCurrentRenderingUpdate) {
        HashSet<WebCore::RenderingResourceIdentifier> toRemove;
        auto renderingUpdateID = m_renderingUpdateID;
        for (auto& item : m_fonts) {
            if (renderingUpdateID - item.value >= minimumRenderingUpdateCountToKeepFontAlive) {
                toRemove.add(item.key);
                m_remoteRenderingBackendProxy->releaseFont(item.key);
            }
        }

        m_fonts.removeIf([&](const auto& bucket) {
            return toRemove.contains(bucket.key);
        });
    }

    totalFontCount = m_fontCustomPlatformDatas.size();
    RELEASE_ASSERT(m_numberOfFontCustomPlatformDatasUsedInCurrentRenderingUpdate <= totalFontCount);
    if (totalFontCount != m_numberOfFontCustomPlatformDatasUsedInCurrentRenderingUpdate) {
        HashSet<WebCore::RenderingResourceIdentifier> toRemove;
        auto renderingUpdateID = m_renderingUpdateID;
        for (auto& item : m_fontCustomPlatformDatas) {
            if (renderingUpdateID - item.value >= minimumRenderingUpdateCountToKeepFontAlive) {
                toRemove.add(item.key);
                m_remoteRenderingBackendProxy->releaseFontCustomPlatformData(item.key);
            }
        }

        m_fontCustomPlatformDatas.removeIf([&](const auto& bucket) {
            return toRemove.contains(bucket.key);
        });
    }
}

void RemoteResourceCacheProxy::didPaintLayers()
{
    finalizeRenderingUpdateForFonts();
    prepareForNextRenderingUpdate();
    m_renderingUpdateID++;
}

void RemoteResourceCacheProxy::releaseMemory()
{
    m_resourceObserverWeakFactory.revokeAll();
    m_filters.clear();
    m_gradients.clear();
    m_displayLists.clear();
    releaseNativeImages();
    releaseFonts();
    releaseFontCustomPlatformDatas();
}

void RemoteResourceCacheProxy::disconnect()
{
    m_resourceObserverWeakFactory.revokeAll();
    m_filters.clear();
    m_gradients.clear();
    m_displayLists.clear();
    releaseFonts();
    releaseFontCustomPlatformDatas();

    for (auto& value : m_nativeImages.values())
        value.existsInRemote = false;
}

} // namespace WebKit

#endif // ENABLE(GPU_PROCESS)
