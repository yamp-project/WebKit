/*
 * Copyright (C) 2016-2025 Apple Inc. All rights reserved.
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

#include "config.h"
#include "NativeImage.h"

#if USE(CG)

#include "CGSubimageCacheWithTimer.h"
#include "GeometryUtilities.h"
#include "GraphicsContextCG.h"
#include "ImageBuffer.h"
#include <limits>
#include <pal/spi/cg/CoreGraphicsSPI.h>

namespace WebCore {


RefPtr<NativeImage> NativeImage::create(PlatformImagePtr&& image, RenderingResourceIdentifier renderingResourceIdentifier)
{
    if (!image)
        return nullptr;
    if (CGImageGetWidth(image.get()) > std::numeric_limits<int>::max() || CGImageGetHeight(image.get()) > std::numeric_limits<int>::max())
        return nullptr;
    return adoptRef(*new NativeImage(WTFMove(image), renderingResourceIdentifier));
}

RefPtr<NativeImage> NativeImage::createTransient(PlatformImagePtr&& image, RenderingResourceIdentifier identifier)
{
    if (!image)
        return nullptr;
    // FIXME: GraphicsContextCG caching should be made better and this should be the default mode
    // for NativeImage, as we cannot guarantee all the places that draw images to not cache unwanted
    // images.
    RetainPtr<CGImage> transientImage = adoptCF(CGImageCreateCopy(image.get())); // Make a shallow copy so the metadata change doesn't affect the caller.
    if (!transientImage)
        return nullptr;
    image = nullptr;
    CGImageSetCachingFlags(transientImage.get(), kCGImageCachingTransient);
    return create(WTFMove(transientImage), identifier);
}

IntSize NativeImage::size() const
{
    return IntSize(CGImageGetWidth(m_platformImage.get()), CGImageGetHeight(m_platformImage.get()));
}

bool NativeImage::hasAlpha() const
{
    CGImageAlphaInfo info = CGImageGetAlphaInfo(m_platformImage.get());
    return (info >= kCGImageAlphaPremultipliedLast) && (info <= kCGImageAlphaFirst);
}

DestinationColorSpace NativeImage::colorSpace() const
{
    return DestinationColorSpace(CGImageGetColorSpace(m_platformImage.get()));
}

Headroom NativeImage::headroom() const
{
#if HAVE(SUPPORT_HDR_DISPLAY)
    float headroom = CGImageGetContentHeadroom(m_platformImage.get());
    return Headroom(std::max<float>(headroom, Headroom::None));
#else
    return Headroom::None;
#endif
}

std::optional<Color> NativeImage::singlePixelSolidColor() const
{
    if (size() != IntSize(1, 1))
        return std::nullopt;

    std::array<uint8_t, 4> pixel; // RGBA
    auto bitmapContext = adoptCF(CGBitmapContextCreate(pixel.data(), 1, 1, 8, pixel.size(), sRGBColorSpaceSingleton(), static_cast<uint32_t>(kCGImageAlphaPremultipliedLast) | static_cast<uint32_t>(kCGBitmapByteOrder32Big)));

    if (!bitmapContext)
        return std::nullopt;

    CGContextSetBlendMode(bitmapContext.get(), kCGBlendModeCopy);
    CGContextDrawImage(bitmapContext.get(), CGRectMake(0, 0, 1, 1), platformImage().get());

    if (!pixel[3])
        return Color::transparentBlack;

    return makeFromComponentsClampingExceptAlpha<SRGBA<uint8_t>>(pixel[0] * 255 / pixel[3], pixel[1] * 255 / pixel[3], pixel[2] * 255 / pixel[3], pixel[3]);
}

void NativeImage::draw(GraphicsContext& context, const FloatRect& destinationRect, const FloatRect& sourceRect, ImagePaintingOptions options)
{
    if (sourceRect.isEmpty() || !hasHDRContent()) {
        context.drawNativeImageInternal(*this, destinationRect, sourceRect, options);
        return;
    }

#if !HAVE(FIX_FOR_RADAR_93560567)
    if (!context.colorSpace().usesITUR_2100TF()) {
        drawWithToneMapping(context, destinationRect, sourceRect, options);
        return;
    }
#endif

    context.drawNativeImageInternal(*this, destinationRect, sourceRect, options);
}

void NativeImage::clearSubimages()
{
#if CACHE_SUBIMAGES
    CGSubimageCacheWithTimer::clearImage(platformImage().get());
#endif
}


} // namespace WebCore

#endif // USE(CG)
