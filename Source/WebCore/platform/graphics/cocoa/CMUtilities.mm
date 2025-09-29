/*
 * Copyright (C) 2020-2021 Apple Inc. All rights reserved.
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
#import "CMUtilities.h"

#if PLATFORM(COCOA)

#import "CAAudioStreamDescription.h"
#import "FormatDescriptionUtilities.h"
#import "Logging.h"
#import "MediaSampleAVFObjC.h"
#import "MediaSamplesBlock.h"
#import "MediaUtilities.h"
#import "SharedBuffer.h"
#import "WebMAudioUtilitiesCocoa.h"
#import <CoreMedia/CMFormatDescription.h>
#import <pal/avfoundation/MediaTimeAVFoundation.h>
#import <pal/spi/cocoa/AudioToolboxSPI.h>
#import <wtf/Expected.h>
#import <wtf/Scope.h>
#import <wtf/TZoneMallocInlines.h>
#import <wtf/cf/TypeCastsCF.h>

#import "CoreVideoSoftLink.h"
#import "VideoToolboxSoftLink.h"
#import <pal/cocoa/AVFoundationSoftLink.h>
#import <pal/cf/AudioToolboxSoftLink.h>
#import <pal/cf/CoreMediaSoftLink.h>

namespace WebCore {

WTF_MAKE_TZONE_ALLOCATED_IMPL(PacketDurationParser);

#if ENABLE(VORBIS)
constexpr uint32_t kAudioFormatVorbis = 'vorb';
#endif

CAAudioStreamDescription audioStreamDescriptionFromAudioInfo(const AudioInfo& info)
{
    ASSERT(info.codecName.value != kAudioFormatLinearPCM);
    AudioStreamBasicDescription asbd { };
    asbd.mFormatID = info.codecName.value;
    std::span<const uint8_t> cookieDataSpan { };
    RefPtr cookieData = info.cookieData;
    if (cookieData)
        cookieDataSpan = cookieData->span();
    UInt32 size = sizeof(asbd);
    if (auto error = PAL::AudioFormatGetProperty(kAudioFormatProperty_FormatInfo, cookieDataSpan.size(), cookieDataSpan.data(), &size, &asbd)) {
        RELEASE_LOG_DEBUG(Media, "kAudioFormatProperty_FormatInfo failed with error %d (%.4s)", static_cast<int>(error), (char *)&error);
        asbd.mSampleRate = info.rate;
        asbd.mFramesPerPacket = info.framesPerPacket;
        asbd.mChannelsPerFrame = info.channels;
        asbd.mBitsPerChannel = info.bitDepth;
    }

    return asbd;
}

static RetainPtr<CMFormatDescriptionRef> createAudioFormatDescription(const AudioInfo& info)
{
    auto streamDescription = audioStreamDescriptionFromAudioInfo(info);
    std::span<const uint8_t> cookie;
    RefPtr cookieData = info.cookieData;
    if (cookieData)
        cookie = cookieData->span();
    return createAudioFormatDescription(streamDescription, cookie);
}

static CFStringRef convertToCMColorPrimaries(PlatformVideoColorPrimaries primaries)
{
    switch (primaries) {
    case PlatformVideoColorPrimaries::Bt709:
        return kCVImageBufferColorPrimaries_ITU_R_709_2;
    case PlatformVideoColorPrimaries::JedecP22Phosphors:
        return kCVImageBufferColorPrimaries_EBU_3213;
    case PlatformVideoColorPrimaries::Smpte170m:
    case PlatformVideoColorPrimaries::Smpte240m:
        return kCVImageBufferColorPrimaries_SMPTE_C;
    case PlatformVideoColorPrimaries::SmpteRp431:
        return PAL::kCMFormatDescriptionColorPrimaries_DCI_P3;
    case PlatformVideoColorPrimaries::SmpteEg432:
        return PAL::kCMFormatDescriptionColorPrimaries_P3_D65;
    case PlatformVideoColorPrimaries::Bt2020:
        return PAL::kCMFormatDescriptionColorPrimaries_ITU_R_2020;
    default:
        return nullptr;
    }
}

static CFStringRef convertToCMTransferFunction(PlatformVideoTransferCharacteristics characteristics)
{
    switch (characteristics) {
    case PlatformVideoTransferCharacteristics::Smpte170m:
    case PlatformVideoTransferCharacteristics::Bt709:
        return kCVImageBufferTransferFunction_ITU_R_709_2;
    case PlatformVideoTransferCharacteristics::Smpte240m:
        return kCVImageBufferTransferFunction_SMPTE_240M_1995;
    case PlatformVideoTransferCharacteristics::SmpteSt2084:
        return PAL::kCMFormatDescriptionTransferFunction_SMPTE_ST_2084_PQ;
    case PlatformVideoTransferCharacteristics::Bt2020_10bit:
    case PlatformVideoTransferCharacteristics::Bt2020_12bit:
        return PAL::kCMFormatDescriptionTransferFunction_ITU_R_2020;
    case PlatformVideoTransferCharacteristics::SmpteSt4281:
        return PAL::kCMFormatDescriptionTransferFunction_SMPTE_ST_428_1;
    case PlatformVideoTransferCharacteristics::AribStdB67Hlg:
        return PAL::kCMFormatDescriptionTransferFunction_ITU_R_2100_HLG;
    case PlatformVideoTransferCharacteristics::Iec6196621:
        return PAL::canLoad_CoreMedia_kCMFormatDescriptionTransferFunction_sRGB() ? PAL::kCMFormatDescriptionTransferFunction_sRGB : nullptr;
    case PlatformVideoTransferCharacteristics::Linear:
        return PAL::kCMFormatDescriptionTransferFunction_Linear;
    default:
        return nullptr;
    }
}

static CFStringRef convertToCMYCbCRMatrix(PlatformVideoMatrixCoefficients coefficients)
{
    switch (coefficients) {
    case PlatformVideoMatrixCoefficients::Bt2020NonconstantLuminance:
        return PAL::kCMFormatDescriptionYCbCrMatrix_ITU_R_2020;
    case PlatformVideoMatrixCoefficients::Bt470bg:
    case PlatformVideoMatrixCoefficients::Smpte170m:
        return kCVImageBufferYCbCrMatrix_ITU_R_601_4;
    case PlatformVideoMatrixCoefficients::Bt709:
        return kCVImageBufferYCbCrMatrix_ITU_R_709_2;
    case PlatformVideoMatrixCoefficients::Smpte240m:
        return kCVImageBufferYCbCrMatrix_SMPTE_240M_1995;
    default:
        return nullptr;
    }
}

RetainPtr<CMFormatDescriptionRef> createFormatDescriptionFromTrackInfo(const TrackInfo& info)
{
    ASSERT(info.isVideo() || info.isAudio());

    if (auto* audioInfo = dynamicDowncast<AudioInfo>(info)) {
        if (audioInfo->codecName.value != kAudioFormatLinearPCM && (!audioInfo->cookieData || !audioInfo->cookieData->size()))
            return nullptr;

        switch (audioInfo->codecName.value) {
#if ENABLE(OPUS)
        case kAudioFormatOpus:
            if (!isOpusDecoderAvailable())
                return nullptr;
            return createAudioFormatDescription(*audioInfo);
#endif
#if ENABLE(VORBIS)
        case kAudioFormatVorbis:
            if (!isVorbisDecoderAvailable())
                return nullptr;
            return createAudioFormatDescription(*audioInfo);
#endif
        case kAudioFormatLinearPCM: {
            auto absd = CAAudioStreamDescription { static_cast<double>(audioInfo->rate), audioInfo->channels, AudioStreamDescription::Float32, CAAudioStreamDescription::IsInterleaved::Yes }.streamDescription();

            CMFormatDescriptionRef newFormat = nullptr;
            if (auto error = PAL::CMAudioFormatDescriptionCreate(kCFAllocatorDefault, &absd, 0, nullptr, 0, nullptr, nullptr, &newFormat)) {
                RELEASE_LOG_ERROR(MediaStream, "createFormatDescriptionFromTrackInfo: CMAudioFormatDescriptionCreate failed with error %d", (int)error);
                return nullptr;
            }
            return adoptCF(newFormat);
        }
        default:
            return createAudioFormatDescription(*audioInfo);
        }
    }

    auto& videoInfo = downcast<const VideoInfo>(info);

    RetainPtr extensions = adoptCF(CFDictionaryCreateMutable(kCFAllocatorDefault, 5, &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks));

    if (RefPtr atomData = videoInfo.atomData) {
        RetainPtr data = atomData->createCFData();
        RetainPtr<CFStringRef> keyName;
        if (!videoInfo.boxType.isEmpty())
            keyName = videoInfo.boxType.createCFString();
        else {
            ASSERT(videoInfo.codecName == kCMVideoCodecType_VP9 || videoInfo.codecName == 'vp08' || videoInfo.codecName == kCMVideoCodecType_H264 || videoInfo.codecName == kCMVideoCodecType_HEVC || videoInfo.codecName == kCMVideoCodecType_AV1);
            keyName = [](auto codec) {
                switch (codec) {
                case kCMVideoCodecType_VP9:
                case 'vp08':
                    return CFSTR("vpcC");
                case kCMVideoCodecType_H264:
                    return CFSTR("avcC");
                case kCMVideoCodecType_HEVC:
                    return CFSTR("hvcC");
                case kCMVideoCodecType_AV1:
                    return CFSTR("av1C");
                default:
                    ASSERT_NOT_REACHED();
                    return CFSTR("baad");
                }
            }(videoInfo.codecName.value);
        }
        CFTypeRef configurationKeys[] = { keyName.get() };
        CFTypeRef configurationValues[] = { data.get() };
        RetainPtr configurationDict = adoptCF(CFDictionaryCreate(kCFAllocatorDefault, configurationKeys, configurationValues, std::size(configurationKeys), &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks));
        CFDictionaryAddValue(extensions.get(), PAL::kCMFormatDescriptionExtension_SampleDescriptionExtensionAtoms, configurationDict.get());
    }

    if (videoInfo.colorSpace.fullRange && *videoInfo.colorSpace.fullRange)
        CFDictionaryAddValue(extensions.get(), PAL::kCMFormatDescriptionExtension_FullRangeVideo, kCFBooleanTrue);

    if (videoInfo.colorSpace.primaries) {
        if (RetainPtr cmColorPrimaries = convertToCMColorPrimaries(*videoInfo.colorSpace.primaries))
            CFDictionaryAddValue(extensions.get(), kCVImageBufferColorPrimariesKey, cmColorPrimaries.get());
    }
    if (videoInfo.colorSpace.transfer) {
        if (RetainPtr cmTransferFunction = convertToCMTransferFunction(*videoInfo.colorSpace.transfer))
            CFDictionaryAddValue(extensions.get(), kCVImageBufferTransferFunctionKey, cmTransferFunction.get());
    }

    if (videoInfo.colorSpace.matrix) {
        if (RetainPtr cmMatrix = convertToCMYCbCRMatrix(*videoInfo.colorSpace.matrix))
            CFDictionaryAddValue(extensions.get(), kCVImageBufferYCbCrMatrixKey, cmMatrix.get());
    }
    if (videoInfo.size != videoInfo.displaySize) {
        double horizontalRatio = videoInfo.displaySize.width() / videoInfo.size.width();
        double verticalRatio = videoInfo.displaySize.height() / videoInfo.size.height();
        CFDictionaryAddValue(extensions.get(), PAL::get_CoreMedia_kCMFormatDescriptionExtension_PixelAspectRatioSingleton(), @{
            (__bridge NSString*)PAL::get_CoreMedia_kCMFormatDescriptionKey_PixelAspectRatioHorizontalSpacingSingleton() : @(horizontalRatio),
            (__bridge NSString*)PAL::get_CoreMedia_kCMFormatDescriptionKey_PixelAspectRatioVerticalSpacingSingleton() : @(verticalRatio)
        });
    }

    CMVideoFormatDescriptionRef formatDescription = nullptr;
    auto error = PAL::CMVideoFormatDescriptionCreate(kCFAllocatorDefault, videoInfo.codecName.value, videoInfo.size.width(), videoInfo.size.height(), extensions.get(), &formatDescription);
    if (error != noErr) {
        RELEASE_LOG_ERROR(Media, "CMVideoFormatDescriptionCreate failed with error %d (%.4s)", (int)error, (char*)&error);
        return nullptr;
    }

    return adoptCF(formatDescription);
}

RefPtr<AudioInfo> createAudioInfoFromFormatDescription(CMFormatDescriptionRef description)
{
    // This method currently only works for compressed content.
    auto mediaType = PAL::CMFormatDescriptionGetMediaType(description);
    if (mediaType != kCMMediaType_Audio)
        return nullptr;
    const AudioStreamBasicDescription* asbd = PAL::CMAudioFormatDescriptionGetStreamBasicDescription(description);
    ASSERT(asbd);
    if (!asbd)
        return nullptr;
    Ref audioInfo = AudioInfo::create();
    audioInfo->codecName = asbd->mFormatID;
    audioInfo->rate = asbd->mSampleRate;
    audioInfo->channels = asbd->mChannelsPerFrame;
    audioInfo->framesPerPacket = asbd->mFramesPerPacket;
    audioInfo->bitDepth = asbd->mBitsPerChannel;
    size_t cookieSize = 0;
    const void* cookie = PAL::CMAudioFormatDescriptionGetMagicCookie(description, &cookieSize);
    if (cookieSize)
        audioInfo->cookieData = SharedBuffer::create(unsafeMakeSpan(static_cast<const uint8_t*>(cookie), cookieSize));
    return audioInfo;
}

RefPtr<VideoInfo> createVideoInfoFromFormatDescription(CMFormatDescriptionRef description)
{
    // This method currently only works for compressed content.
    auto mediaType = PAL::CMFormatDescriptionGetMediaType(description);
    if (mediaType != kCMMediaType_Video)
        return nullptr;

    Ref videoInfo = VideoInfo::create();
    videoInfo->codecName = PAL::CMFormatDescriptionGetMediaSubType(description);
    auto dimensions = PAL::CMVideoFormatDescriptionGetDimensions(description);
    videoInfo->size = IntSize { dimensions.width, dimensions.height };
    videoInfo->displaySize = presentationSizeFromFormatDescription(description);

    RetainPtr<CFDataRef> atomData;
    RetainPtr extensionAtoms = PAL::CMFormatDescriptionGetExtension(description, PAL::kCMFormatDescriptionExtension_SampleDescriptionExtensionAtoms);
    if (RetainPtr atomDictionary = dynamic_cf_cast<CFDictionaryRef>(extensionAtoms.get())) {
        CFIndex extensionCount = CFDictionaryGetCount(atomDictionary.get());
        if (extensionCount != 1)
            RELEASE_LOG_INFO(Media, "kCMFormatDescriptionExtension_SampleDescriptionExtensionAtoms having %ld keys keys expected only 1", extensionCount);
        else {
            Vector<const void*, 1> keys(extensionCount);
            Vector<const void*, 1> values(extensionCount);
            CFDictionaryGetKeysAndValues(atomDictionary.get(), keys.mutableSpan().data(), values.mutableSpan().data());
            if (RetainPtr key = dynamic_cf_cast<CFStringRef>(keys[0]))
                videoInfo->boxType = key.get();
            atomData = dynamic_cf_cast<CFDataRef>(values[0]);
        }
    } else if (RetainPtr atomArray = dynamic_cf_cast<CFArrayRef>(extensionAtoms.get()); atomArray && CFArrayGetCount(atomArray.get()) > 0)
        atomData = dynamic_cf_cast<CFDataRef>(CFArrayGetValueAtIndex(atomArray.get(), 0));
    if (atomData)
        videoInfo->atomData = SharedBuffer::create(atomData.get());
    else
        RELEASE_LOG_ERROR(Media, "Couldn't retrieve atomData from CMFormatDescription");

    int bitDepth;
    if (RetainPtr bitsPerComponent = dynamic_cf_cast<CFNumberRef>(PAL::CMFormatDescriptionGetExtension(description, PAL::kCMFormatDescriptionExtension_BitsPerComponent))) {
        CFNumberGetValue(bitsPerComponent.get(), kCFNumberIntType, &bitDepth);
        videoInfo->bitDepth = bitDepth;
    } else
        videoInfo->bitDepth = 8;

    if (auto colorSpace = colorSpaceFromFormatDescription(description))
        videoInfo->colorSpace = *colorSpace;

    return videoInfo;
}

Expected<RetainPtr<CMSampleBufferRef>, CString> toCMSampleBuffer(const MediaSamplesBlock& samples, CMFormatDescriptionRef formatDescription)
{
    if (!samples.info())
        return makeUnexpected("No TrackInfo found");

    RetainPtr format = formatDescription ? retainPtr(formatDescription) : createFormatDescriptionFromTrackInfo(*samples.protectedInfo());
    if (!format)
        return makeUnexpected("No CMFormatDescription available");

    RetainPtr<CMBlockBufferRef> completeBlockBuffers;
    if (samples.size() > 1) {
        // Optimisation so that we allocate the entire CMBlockBuffer at once if we have more than one to return.
        CMBlockBufferRef rawBlockBuffer = nullptr;
        auto err = PAL::CMBlockBufferCreateEmpty(kCFAllocatorDefault, samples.size(), 0, &rawBlockBuffer);
        if (err != kCMBlockBufferNoErr || !rawBlockBuffer)
            return makeUnexpected("CMBlockBufferCreateEmpty failed");
        completeBlockBuffers = adoptCF(rawBlockBuffer);
    }

    Vector<CMSampleTimingInfo> packetTimings;
    packetTimings.reserveInitialCapacity(samples.size());
    Vector<size_t> packetSizes;
    packetSizes.reserveInitialCapacity(samples.size());
    auto cumulativeTrimDuration = MediaTime::zeroTime();
    for (auto& sample : samples) {
        RefPtr sampleData = sample.data;
        auto blockBuffer = sampleData->createCMBlockBuffer();
        if (!blockBuffer)
            return makeUnexpected("Couldn't create CMBlockBuffer");

        if (!completeBlockBuffers)
            completeBlockBuffers = WTFMove(blockBuffer);
        else {
            auto err = PAL::CMBlockBufferAppendBufferReference(completeBlockBuffers.get(), blockBuffer.get(), 0, 0, 0);
            if (err != kCMBlockBufferNoErr)
                return makeUnexpected("CMBlockBufferAppendBufferReference failed");
        }
        packetTimings.append({ PAL::toCMTime(sample.duration), PAL::toCMTime(sample.presentationTime), PAL::toCMTime(sample.decodeTime) });
        packetSizes.append(sampleData->size());
        cumulativeTrimDuration += sample.trimInterval.first;
    }

    CMSampleBufferRef rawSampleBuffer = nullptr;
    if (PAL::CMSampleBufferCreateReady(kCFAllocatorDefault, completeBlockBuffers.get(), format.get(), packetSizes.size(), packetTimings.size(), packetTimings.span().data(), packetSizes.size(), packetSizes.span().data(), &rawSampleBuffer))
        return makeUnexpected("CMSampleBufferCreateReady failed: OOM");

    if (samples.isVideo() && samples.size()) {
        auto attachmentsArray = PAL::CMSampleBufferGetSampleAttachmentsArray(rawSampleBuffer, true);
        ASSERT(attachmentsArray);
        if (!attachmentsArray)
            return makeUnexpected("No sample attachment found");
        ASSERT(size_t(CFArrayGetCount(attachmentsArray)) == samples.size());
        for (CFIndex i = 0, count = CFArrayGetCount(attachmentsArray); i < count; ++i) {
            CFMutableDictionaryRef attachments = checked_cf_cast<CFMutableDictionaryRef>(CFArrayGetValueAtIndex(attachmentsArray, i));
            if (!(samples[i].flags & MediaSample::SampleFlags::IsSync))
                CFDictionarySetValue(attachments, PAL::kCMSampleAttachmentKey_NotSync, kCFBooleanTrue);

            if (samples[i].flags & MediaSample::SampleFlags::IsNonDisplaying)
                CFDictionarySetValue(attachments, PAL::kCMSampleAttachmentKey_DoNotDisplay, kCFBooleanTrue);

            // Attach HDR10+ (aka SMPTE ST 2094-40) metadata, if present:
            if (samples[i].hdrMetadataType == HdrMetadataType::SmpteSt209440 && samples[i].hdrMetadata)
                CFDictionarySetValue(attachments, PAL::kCMSampleAttachmentKey_HDR10PlusPerFrameData, Ref { *samples[i].hdrMetadata }->createCFData().get());
        }
    } else if (samples.isAudio() && samples.discontinuity())
        PAL::CMSetAttachment(rawSampleBuffer, PAL::kCMSampleBufferAttachmentKey_FillDiscontinuitiesWithSilence, *samples.discontinuity() ? kCFBooleanTrue : kCFBooleanFalse, kCMAttachmentMode_ShouldPropagate);

    if (cumulativeTrimDuration > MediaTime::zeroTime()) {
        auto trimDurationDict = adoptCF(PAL::softLink_CoreMedia_CMTimeCopyAsDictionary(PAL::toCMTime(cumulativeTrimDuration), kCFAllocatorDefault));
        PAL::CMSetAttachment(rawSampleBuffer, PAL::kCMSampleBufferAttachmentKey_TrimDurationAtStart, trimDurationDict.get(), kCMAttachmentMode_ShouldPropagate);
    }

    return adoptCF(rawSampleBuffer);
}

UniqueRef<MediaSamplesBlock> samplesBlockFromCMSampleBuffer(CMSampleBufferRef cmSample, const TrackInfo* trackInfo)
{
    ASSERT(cmSample);
    RefPtr info = trackInfo;
    if (!trackInfo) {
        // While this path is currently unused; we only support creating a TrackInfo from an Audio CMFormatDescription
        if (RetainPtr description = PAL::CMSampleBufferGetFormatDescription(cmSample)) {
            if (PAL::CMFormatDescriptionGetMediaType(description.get()) == kCMMediaType_Audio)
                info = createAudioInfoFromFormatDescription(description.get());
            else {
                ASSERT(PAL::CMFormatDescriptionGetMediaType(description.get()) == kCMMediaType_Video);
                info = createVideoInfoFromFormatDescription(description.get());
            }
        }
    }

    auto mediaSampleItemForSample = [](auto&& sample) {
        MediaTime duration = sample->duration();
        RetainPtr blockBuffer = PAL::CMSampleBufferGetDataBuffer(sample->sampleBuffer());
        auto trimDurationAtStart = MediaTime::zeroTime();
        if (auto* trimDurationDict = static_cast<CFDictionaryRef>(PAL::CMGetAttachment(sample->sampleBuffer(), PAL::kCMSampleBufferAttachmentKey_TrimDurationAtStart, nullptr)))
            trimDurationAtStart = PAL::toMediaTime(PAL::CMTimeMakeFromDictionary(trimDurationDict));
        auto trimDurationAtEnd = MediaTime::zeroTime();
        if (auto* trimDurationDict = static_cast<CFDictionaryRef>(PAL::CMGetAttachment(sample->sampleBuffer(), PAL::kCMSampleBufferAttachmentKey_TrimDurationAtEnd, nullptr)))
            trimDurationAtEnd = PAL::toMediaTime(PAL::CMTimeMakeFromDictionary(trimDurationDict));
        return MediaSamplesBlock::MediaSampleItem {
            .presentationTime = sample->presentationTime(),
            .decodeTime = sample->decodeTime(),
            .duration = sample->duration() + trimDurationAtStart + trimDurationAtEnd,
            .trimInterval = { trimDurationAtStart, trimDurationAtEnd },
            .data = sharedBufferFromCMBlockBuffer(blockBuffer.get()),
            .flags = sample->flags()
        };
    };

    if (info && info->codecName == kAudioFormatLinearPCM) {
        MediaSamplesBlock::SamplesVector sample;
        sample.reserveInitialCapacity(1);
        sample.append(mediaSampleItemForSample(MediaSampleAVFObjC::create(cmSample, info->trackID)));
        return makeUniqueRef<MediaSamplesBlock>(info.get(), WTFMove(sample));
    }

    auto subSamples = MediaSampleAVFObjC::create(cmSample, info ? info->trackID : 0)->divide();
    MediaSamplesBlock::SamplesVector samples(subSamples.size(), [&](auto index) {
        return mediaSampleItemForSample(subSamples[index]);
    });
    return makeUniqueRef<MediaSamplesBlock>(info.get(), WTFMove(samples));
}

void attachColorSpaceToPixelBuffer(const PlatformVideoColorSpace& colorSpace, CVPixelBufferRef pixelBuffer)
{
    ASSERT(pixelBuffer);
    if (!pixelBuffer)
        return;

    CVBufferRemoveAttachment(pixelBuffer, kCVImageBufferCGColorSpaceKey);
    if (colorSpace.primaries)
        CVBufferSetAttachment(pixelBuffer, kCVImageBufferColorPrimariesKey, convertToCMColorPrimaries(*colorSpace.primaries), kCVAttachmentMode_ShouldPropagate);
    if (colorSpace.transfer)
        CVBufferSetAttachment(pixelBuffer, kCVImageBufferTransferFunctionKey, convertToCMTransferFunction(*colorSpace.transfer), kCVAttachmentMode_ShouldPropagate);
    if (colorSpace.matrix)
        CVBufferSetAttachment(pixelBuffer, kCVImageBufferYCbCrMatrixKey, convertToCMYCbCRMatrix(*colorSpace.matrix), kCVAttachmentMode_ShouldPropagate);
}

PacketDurationParser::PacketDurationParser(const AudioInfo& info)
{
    AudioStreamBasicDescription asbd { };
    asbd.mFormatID = info.codecName.value;
    UInt32 size = sizeof(asbd);
    RefPtr cookieData = info.cookieData;
    auto cookieDataSpan = cookieData->span();
    auto error = PAL::AudioFormatGetProperty(kAudioFormatProperty_FormatInfo, cookieDataSpan.size(), cookieDataSpan.data(), &size, &asbd);
    if (error || !info.rate) {
        RELEASE_LOG_ERROR(Media, "createAudioFormatDescription failed with error %d (%.4s)", (int)error, (char*)&error);
        return;
    }
    m_audioFormatID = asbd.mFormatID;
    m_sampleRate = info.rate;
    m_constantFramesPerPacket = asbd.mFramesPerPacket;
#if HAVE(AUDIOFORMATPROPERTY_VARIABLEPACKET_SUPPORTED)
    switch (m_audioFormatID) {
#if ENABLE(VORBIS)
    case kAudioFormatVorbis: {
        AudioFormatInfo formatInfo = { asbd, cookieDataSpan.data(), (UInt32)cookieDataSpan.size() };
        UInt32 propertySize = sizeof(AudioFormatVorbisModeInfo);
        m_vorbisModeInfo = std::make_unique<AudioFormatVorbisModeInfo>();
        if (PAL::AudioFormatGetProperty(kAudioFormatProperty_VorbisModeInfo, sizeof(formatInfo), &formatInfo, &propertySize, m_vorbisModeInfo.get()) != noErr || !m_vorbisModeInfo->mModeCount) {
            m_vorbisModeInfo.reset();
            // No mode info or invalid mode info.
            return;
        }

        auto ilog = [] (uint32_t v) {
            int ret = 0;
            while (v) {
                ret++;
                v >>= 1;
            }
            return ret;
        };

        uint32_t modeBitCount = ilog(m_vorbisModeInfo->mModeCount - 1);
        for (uint32_t thisModeBit = 0; thisModeBit < modeBitCount; ++thisModeBit)
            m_vorbisModeMask |= 1 << thisModeBit;
        }
        break;
#endif
    default:
        // No need to examine the magic cookie.
        break;
    }
#endif
    m_isValid = true;
}

size_t PacketDurationParser::framesInPacket(std::span<const uint8_t> packet)
{
#if !HAVE(AUDIOFORMATPROPERTY_VARIABLEPACKET_SUPPORTED)
    UNUSED_PARAM(packet);
    return m_constantFramesPerPacket;
#else
    if (m_constantFramesPerPacket)
        return m_constantFramesPerPacket;

    if (packet.empty())
        return 0;

    switch (m_audioFormatID) {
#if ENABLE(OPUS)
    case kAudioFormatOpus: {
        OpusCookieContents cookie;
        if (!parseOpusTOCData(packet, cookie))
            return 0;
        return cookie.framesPerPacket * (cookie.frameDuration.seconds() * m_sampleRate);
        }
#endif
#if ENABLE(VORBIS)
    case kAudioFormatVorbis: {
        // The following calculation corresponds to the duration of the "finished audiodata"
        // produced by the decoder from the current packet in its position within
        // the stream, as documented by Xiph in the Vorbis I specification.
        // It also corresponds to the delta in granule position of the packet within
        // the same sequence of packets in an Ogg file, with the possible exception of
        // the ultimate packet, which may be assigned a smaller delta for the purpose
        // of trimming.
        constexpr uint8_t kVorbisPacketTypeFlag = 0b00000001;

        auto leadingByte = packet[0];
        if (leadingByte & kVorbisPacketTypeFlag)
            return 0; // Not an audio packet.

        uint32_t modeIndex = (leadingByte >> 1) & m_vorbisModeMask;
        if (modeIndex >= m_vorbisModeInfo->mModeCount)
            return 0; // Invalid mode.

        uint32_t blockSize = 0;
        if (!(m_vorbisModeInfo->mModeFlags & (1ULL << modeIndex)))
            blockSize = m_vorbisModeInfo->mShortBlockSize;
        else
            blockSize = m_vorbisModeInfo->mLongBlockSize;
        // The first vorbis packet decoded doesn't output audible content, and should be undetermined.
        // However as content could be fed in any order, we must assume that previous content could be available at some stage.
        size_t framesOfOutput = (blockSize + m_lastVorbisBlockSize) / 4;
        m_lastVorbisBlockSize = blockSize;

        return framesOfOutput;
        }
#endif
    default:
        return m_constantFramesPerPacket;
    }
#endif
}

void PacketDurationParser::reset()
{
#if ENABLE(VORBIS)
    if (m_audioFormatID == kAudioFormatVorbis)
        m_lastVorbisBlockSize = 0;
#endif
}

PacketDurationParser::~PacketDurationParser() = default;

Vector<AudioStreamPacketDescription> getPacketDescriptions(CMSampleBufferRef sampleBuffer)
{
    size_t packetDescriptionsSize;
    if (PAL::CMSampleBufferGetAudioStreamPacketDescriptions(sampleBuffer, 0, nullptr, &packetDescriptionsSize) != noErr) {
        RELEASE_LOG_FAULT(Media, "Unable to get packet description list size");
        return { };
    }
    size_t numDescriptions = packetDescriptionsSize / sizeof(AudioStreamPacketDescription);
    if (!numDescriptions) {
        RELEASE_LOG_DEBUG(Media, "No packet description found.");
        return { };
    }
    Vector<AudioStreamPacketDescription> descriptions(numDescriptions);
    if (PAL::CMSampleBufferGetAudioStreamPacketDescriptions(sampleBuffer, packetDescriptionsSize, descriptions.mutableSpan().data(), nullptr) != noErr) {
        RELEASE_LOG_FAULT(Media, "Unable to get packet description list");
        return { };
    }
    auto numPackets = PAL::CMSampleBufferGetNumSamples(sampleBuffer);
    if (numDescriptions != size_t(numPackets)) {
        RELEASE_LOG_FAULT(Media, "Unhandled CMSampleBuffer structure");
        return { };
    }
    return descriptions;
}

RetainPtr<CMBlockBufferRef> ensureContiguousBlockBuffer(CMBlockBufferRef rawBlockBuffer)
{
    if (PAL::CMBlockBufferIsRangeContiguous(rawBlockBuffer, 0, 0))
        return rawBlockBuffer;
    CMBlockBufferRef contiguousBuffer;
    if (auto status = PAL::CMBlockBufferCreateContiguous(nullptr, rawBlockBuffer, nullptr, nullptr, 0, 0, 0, &contiguousBuffer)) {
        RELEASE_LOG_FAULT(Media, "Failed to create contiguous blockBuffer with error:%d", static_cast<int>(status));
        return nullptr;
    }
    return adoptCF(contiguousBuffer);
}

Ref<SharedBuffer> sharedBufferFromCMBlockBuffer(CMBlockBufferRef blockBuffer)
{
    return SharedBuffer::create(DataSegment::Provider {
        [blockBuffer = ensureContiguousBlockBuffer(blockBuffer)]() -> std::span<const uint8_t> {
            if (!blockBuffer)
                return { };
            return PAL::CMBlockBufferGetDataSpan(blockBuffer.get());
        }
    });
}

} // namespace WebCore

#endif // PLATFORM(COCOA)
