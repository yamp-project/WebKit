/*
 * Copyright (c) 2021-2023 Apple Inc. All rights reserved.
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

#import "config.h"
#import "Instance.h"

#import "APIConversions.h"
#import "Adapter.h"
#import "DDMesh.h"
#import "HardwareCapabilities.h"
#import "PresentationContext.h"
#import <cstring>
#import <dlfcn.h>
#import <wtf/BlockPtr.h>
#import <wtf/MachSendRight.h>
#import <wtf/StdLibExtras.h>
#import <wtf/TZoneMallocInlines.h>

#if ENABLE(WEBGPU_SWIFT)
#import "WebGPUSwiftInternal.h"
#endif

namespace WebGPU {

WTF_MAKE_TZONE_ALLOCATED_IMPL(Instance);

static NSArray<id<MTLDevice>>* getDevices()
{
#if PLATFORM(MAC) || PLATFORM(MACCATALYST)
    NSArray<id<MTLDevice>> *devices = MTLCopyAllDevices();
#else
    NSMutableArray<id<MTLDevice>> *devices = [NSMutableArray array];
    if (id<MTLDevice> device = MTLCreateSystemDefaultDevice())
        [devices addObject:device];
#endif
    return devices;
}

Ref<Instance> Instance::create(const WGPUInstanceDescriptor& descriptor)
{
    const WGPUInstanceCocoaDescriptor& cocoaDescriptor = descriptor.cocoaDescriptor;

    return adoptRef(*new Instance(cocoaDescriptor.scheduleWorkBlock, reinterpret_cast<const WTF::MachSendRight*>(cocoaDescriptor.webProcessResourceOwner)));
}

Instance::Instance(WGPUScheduleWorkBlock scheduleWorkBlock, const MachSendRight* webProcessResourceOwner)
    : m_webProcessID(webProcessResourceOwner ? std::optional<MachSendRight>(*webProcessResourceOwner) : std::nullopt)
    , m_scheduleWorkBlock(scheduleWorkBlock ? WTFMove(scheduleWorkBlock) : ^(WGPUWorkItem workItem) { defaultScheduleWork(WTFMove(workItem)); })
{
#if ENABLE(WEBGPU_SWIFT)
    m_ddReceiver = [[DDBridgeReceiver alloc] init];
    [m_ddReceiver setDeviceWithDevice:getDevices()[0]];
#endif
    m_ddMeshIdentifier = [[NSUUID alloc] init];
}

Instance::Instance()
    : m_scheduleWorkBlock(^(WGPUWorkItem workItem) { defaultScheduleWork(WTFMove(workItem)); })
    , m_isValid(false)
{
}

Instance::~Instance() = default;

Ref<PresentationContext> Instance::createSurface(const WGPUSurfaceDescriptor& descriptor)
{
    return PresentationContext::create(descriptor, *this);
}

void Instance::scheduleWork(WorkItem&& workItem)
{
    m_scheduleWorkBlock(makeBlockPtr(WTFMove(workItem)).get());
}

const std::optional<const MachSendRight>& Instance::webProcessID() const
{
    return m_webProcessID;
}

void Instance::defaultScheduleWork(WGPUWorkItem&& workItem)
{
    Locker locker(m_lock);
    m_pendingWork.append(WTFMove(workItem));
}

void Instance::processEvents()
{
    while (true) {
        Deque<WGPUWorkItem> localWork;
        {
            Locker locker(m_lock);
            std::swap(m_pendingWork, localWork);
        }
        if (localWork.isEmpty())
            return;
        for (auto& workItem : localWork)
            workItem();
    }
}

static NSArray<id<MTLDevice>> *sortedDevices(NSArray<id<MTLDevice>> *devices, WGPUPowerPreference powerPreference)
{
    switch (powerPreference) {
    case WGPUPowerPreference_Undefined:
        return devices;
    case WGPUPowerPreference_LowPower:
#if PLATFORM(MAC) || PLATFORM(MACCATALYST)
        return [devices sortedArrayWithOptions:NSSortStable usingComparator:^NSComparisonResult (id<MTLDevice> obj1, id<MTLDevice> obj2)
        {
            if (obj1.lowPower == obj2.lowPower)
                return NSOrderedSame;
            if (obj1.lowPower)
                return NSOrderedAscending;
            return NSOrderedDescending;
        }];
#else
        return devices;
#endif
    case WGPUPowerPreference_HighPerformance:
#if PLATFORM(MAC) || PLATFORM(MACCATALYST)
        return [devices sortedArrayWithOptions:NSSortStable usingComparator:^NSComparisonResult (id<MTLDevice> obj1, id<MTLDevice> obj2)
        {
            if (obj1.lowPower == obj2.lowPower)
                return NSOrderedSame;
            if (obj1.lowPower)
                return NSOrderedDescending;
            return NSOrderedAscending;
        }];
#else
        return devices;
#endif
    case WGPUPowerPreference_Force32:
        ASSERT_NOT_REACHED();
        return nil;
    }
}

void Instance::requestAdapter(const WGPURequestAdapterOptions& options, CompletionHandler<void(WGPURequestAdapterStatus, Ref<Adapter>&&, String&&)>&& callback)
{
    auto devices = getDevices();

    // FIXME: Deal with options.compatibleSurface.

    auto sortedDevices = WebGPU::sortedDevices(devices, options.powerPreference);

    if (options.forceFallbackAdapter) {
        callback(WGPURequestAdapterStatus_Unavailable, Adapter::createInvalid(*this), "No adapters present"_s);
        return;
    }

    if (!sortedDevices) {
        callback(WGPURequestAdapterStatus_Error, Adapter::createInvalid(*this), "Unknown power preference"_s);
        return;
    }

    if (!sortedDevices.count) {
        callback(WGPURequestAdapterStatus_Unavailable, Adapter::createInvalid(*this), "No adapters present"_s);
        return;
    }

    if (!sortedDevices[0]) {
        callback(WGPURequestAdapterStatus_Error, Adapter::createInvalid(*this), "Adapter is internally null"_s);
        return;
    }

    auto device = sortedDevices[0];

    auto deviceCapabilities = hardwareCapabilities(device);

    if (!deviceCapabilities) {
        callback(WGPURequestAdapterStatus_Error, Adapter::createInvalid(*this), "Device does not support WebGPU"_s);
        return;
    }

    // FIXME: this should be asynchronous
    callback(WGPURequestAdapterStatus_Success, Adapter::create(sortedDevices[0], *this, options.xrCompatible, WTFMove(*deviceCapabilities)), { });
}

void Instance::retainDevice(Device& device, id<MTLCommandBuffer> commandBuffer)
{
    Locker locker(m_lock);
    CommandBufferContainer* container = nullptr;
    if (auto it = retainedDeviceInstances.find(&device); it != retainedDeviceInstances.end())
        container = &it->value;
    else
        container = &retainedDeviceInstances.add(&device, CommandBufferContainer { }).iterator->value;

    container->append(commandBuffer);

    for (auto& [device, container] : retainedDeviceInstances) {
        container.removeAllMatching([&] (auto& pair) {
            return !pair;
        });
    }
    retainedDeviceInstances.removeIf([&] (auto& pair) {
        return !pair.value.size();
    });
}

void Instance::updateModel(Texture& texture)
{
    m_lastMeshTexture = texture.texture();
#if ENABLE(WEBGPU_SWIFT)
    [m_ddReceiver renderWithTexture:m_lastMeshTexture];
#endif
}

#if ENABLE(WEBGPU_SWIFT)
static DDBridgeChainedFloat4x4 *convertFloats(const Vector<simd_float4x4>& fs)
{
    auto count = fs.size();
    if (!count)
        return nil;

    DDBridgeChainedFloat4x4 *result = [[DDBridgeChainedFloat4x4 alloc] initWithTransform:fs[0]];
    DDBridgeChainedFloat4x4 *current = result;

    for (uint32_t i = 1; i < count; ++i) {
        DDBridgeChainedFloat4x4 *next = [[DDBridgeChainedFloat4x4 alloc] initWithTransform:fs[i]];
        current.next = next;
    }

    return result;
}

static DDBridgeMeshPart *convertPart(const WGPUDDMeshPart& part)
{
    return [[DDBridgeMeshPart alloc] initWithIndexOffset:part.indexOffset indexCount:part.indexCount topology:part.topology materialIndex:part.materialIndex boundsMin:part.boundsMin boundsMax:part.boundsMax];
}

static NSArray<DDBridgeSetPart*> *convertParts(const Vector<KeyValuePair<int32_t, WGPUDDMeshPart>>& parts)
{
    NSMutableArray<DDBridgeSetPart*>* result = [NSMutableArray array];
    for (auto& kvp : parts) {
        DDBridgeSetPart* part = [[DDBridgeSetPart alloc] initWithIndex:kvp.key part:convertPart(kvp.value)];
        [result addObject:part];
    }
    return result;
}

static NSArray<NSUUID *> *convertMaterialIDs(const Vector<String>& ids)
{
    NSMutableArray<NSUUID *>* result = [NSMutableArray array];
    for (auto& i : ids)
        [result addObject:[[NSUUID alloc] initWithUUIDString:i.createNSString().get()]];
    return result;
}

static NSData* convertUint8s(const Vector<uint8_t>& data)
{
WTF_ALLOW_UNSAFE_BUFFER_USAGE_BEGIN
    return [[NSData alloc] initWithBytes:data.span().data() length:data.sizeInBytes()];
WTF_ALLOW_UNSAFE_BUFFER_USAGE_END
}

static NSArray<DDBridgeSetRenderFlags*> *convertRenderFlags(const Vector<KeyValuePair<int32_t, uint64_t>>& renderFlags)
{
    NSMutableArray<DDBridgeSetRenderFlags*>* result = [NSMutableArray array];
    for (auto& r : renderFlags)
        [result addObject:[[DDBridgeSetRenderFlags alloc] initWithIndex:r.key renderFlags:r.value]];
    return result;
}

static NSArray<DDBridgeReplaceVertices*> *convertVertices(const Vector<WGPUDDReplaceVertices>& vertices)
{
    NSMutableArray<DDBridgeReplaceVertices*>* result = [NSMutableArray array];
    for (auto& v : vertices)
        [result addObject:[[DDBridgeReplaceVertices alloc] initWithBufferIndex:v.bufferIndex buffer:convertUint8s(v.buffer)]];

    return result;
}
#endif

#if ENABLE(WEBGPU_SWIFT)
static DDBridgeUpdateMesh *convertDescriptor(WGPUDDUpdateMeshDescriptor& descriptor)
{
    DDBridgeUpdateMesh *result = [[DDBridgeUpdateMesh alloc] initWithPartCount:descriptor.partCount
        parts:convertParts(descriptor.parts)
        renderFlags:convertRenderFlags(descriptor.renderFlags)
        vertices:convertVertices(descriptor.vertices)
        indices:convertUint8s(descriptor.indices)
        transform:descriptor.transform
        instanceTransforms:convertFloats(descriptor.instanceTransforms4x4)
        materialIds:convertMaterialIDs(descriptor.materialIds)];

    return result;
}
#endif

void Instance::updateMesh(DDMesh&, WGPUDDUpdateMeshDescriptor& descriptor)
{
#if ENABLE(WEBGPU_SWIFT)
    [m_ddReceiver updateMesh:convertDescriptor(descriptor) identifier:m_ddMeshIdentifier];
    [m_ddReceiver renderWithTexture:m_lastMeshTexture];
#else
    UNUSED_PARAM(descriptor);
#endif
}

} // namespace WebGPU

#pragma mark WGPU Stubs

void wgpuInstanceReference(WGPUInstance instance)
{
    WebGPU::fromAPI(instance).ref();
}

void wgpuInstanceRelease(WGPUInstance instance)
{
    WebGPU::fromAPI(instance).deref();
}

WGPUInstance wgpuCreateInstance(const WGPUInstanceDescriptor* descriptor)
{
    return WebGPU::releaseToAPI(WebGPU::Instance::create(*descriptor));
}

WGPUProc wgpuGetProcAddress(WGPUDevice, const char*)
{
    return nullptr;
}

WGPUSurface wgpuInstanceCreateSurface(WGPUInstance instance, const WGPUSurfaceDescriptor* descriptor)
{
    return WebGPU::releaseToAPI(WebGPU::protectedFromAPI(instance)->createSurface(*descriptor));
}

void wgpuInstanceProcessEvents(WGPUInstance instance)
{
    WebGPU::protectedFromAPI(instance)->processEvents();
}

void wgpuInstanceRequestAdapter(WGPUInstance instance, const WGPURequestAdapterOptions* options, WGPURequestAdapterCallback callback, void* userdata)
{
    WebGPU::protectedFromAPI(instance)->requestAdapter(*options, [callback, userdata](WGPURequestAdapterStatus status, Ref<WebGPU::Adapter>&& adapter, String&& message) {
        if (status != WGPURequestAdapterStatus_Success) {
            callback(status, nullptr, message.utf8().data(), userdata);
            return;
        }

        callback(status, WebGPU::releaseToAPI(WTFMove(adapter)), message.utf8().data(), userdata);
    });
}

void wgpuInstanceRequestAdapterWithBlock(WGPUInstance instance, WGPURequestAdapterOptions const * options, WGPURequestAdapterBlockCallback callback)
{
    WebGPU::protectedFromAPI(instance)->requestAdapter(*options, [callback = WebGPU::fromAPI(WTFMove(callback))](WGPURequestAdapterStatus status, Ref<WebGPU::Adapter>&& adapter, String&& message) {
        if (status != WGPURequestAdapterStatus_Success) {
            callback(status, nullptr, message.utf8().data());
            return;
        }

        callback(status, WebGPU::releaseToAPI(WTFMove(adapter)), message.utf8().data());
    });
}

// Fuzzer things
WGPUBool wgpuBufferIsValid(WGPUBuffer buffer)
{
    return WebGPU::protectedFromAPI(buffer)->isValid();
}

WGPUBool wgpuAdapterIsValid(WGPUAdapter adapter)
{
    return WebGPU::protectedFromAPI(adapter)->isValid();
}

WGPUBool wgpuBindGroupIsValid(WGPUBindGroup bindGroup)
{
    return WebGPU::protectedFromAPI(bindGroup)->isValid();
}

WGPUBool wgpuBindGroupLayoutIsValid(WGPUBindGroupLayout bindGroupLayout)
{
    return WebGPU::protectedFromAPI(bindGroupLayout)->isValid();
}

WGPUBool wgpuCommandBufferIsValid(WGPUCommandBuffer commandBuffer)
{
    return WebGPU::protectedFromAPI(commandBuffer)->isValid();
}

WGPUBool wgpuCommandEncoderIsValid(WGPUCommandEncoder commandEncoder)
{
    return WebGPU::protectedFromAPI(commandEncoder)->isValid();
}

WGPUBool wgpuComputePassEncoderIsValid(WGPUComputePassEncoder computePassEncoder)
{
    return WebGPU::protectedFromAPI(computePassEncoder)->isValid();
}

WGPUBool wgpuComputePipelineIsValid(WGPUComputePipeline computePipeline)
{
    return WebGPU::protectedFromAPI(computePipeline)->isValid();
}

WGPUBool wgpuDeviceIsValid(WGPUDevice device)
{
    return WebGPU::protectedFromAPI(device)->isValid();
}

WGPUBool wgpuExternalTextureIsValid(WGPUExternalTexture externalTexture)
{
    return WebGPU::protectedFromAPI(externalTexture)->isValid();
}

WGPUBool wgpuPipelineLayoutIsValid(WGPUPipelineLayout pipelineLayout)
{
    return WebGPU::protectedFromAPI(pipelineLayout)->isValid();
}

WGPUBool wgpuPresentationContextIsValid(WGPUSurface presentationContext)
{
    return WebGPU::protectedFromAPI(presentationContext)->isValid();
}

WGPUBool wgpuQuerySetIsValid(WGPUQuerySet querySet)
{
    return WebGPU::protectedFromAPI(querySet)->isValid();
}

WGPUBool wgpuQueueIsValid(WGPUQueue queue)
{
    return WebGPU::protectedFromAPI(queue)->isValid();
}

WGPUBool wgpuRenderBundleEncoderIsValid(WGPURenderBundleEncoder renderBundleEncoder)
{
    return WebGPU::protectedFromAPI(renderBundleEncoder)->isValid();
}

WGPUBool wgpuRenderBundleIsValid(WGPURenderBundle renderBundle)
{
    return WebGPU::protectedFromAPI(renderBundle)->isValid();
}

WGPUBool wgpuRenderPassEncoderIsValid(WGPURenderPassEncoder renderPassEncoder)
{
    return WebGPU::protectedFromAPI(renderPassEncoder)->isValid();
}

WGPUBool wgpuRenderPipelineIsValid(WGPURenderPipeline renderPipeline)
{
    return WebGPU::protectedFromAPI(renderPipeline)->isValid();
}

WGPUBool wgpuSamplerIsValid(WGPUSampler sampler)
{
    return WebGPU::protectedFromAPI(sampler)->isValid();
}

WGPUBool wgpuShaderModuleIsValid(WGPUShaderModule shaderModule)
{
    return WebGPU::protectedFromAPI(shaderModule)->isValid();
}

WGPUBool wgpuTextureIsValid(WGPUTexture texture)
{
    return WebGPU::protectedFromAPI(texture)->isValid();
}

WGPUBool wgpuTextureViewIsValid(WGPUTextureView textureView)
{
    return WebGPU::protectedFromAPI(textureView)->isValid();
}

WGPUBool wgpuXRBindingIsValid(WGPUXRBinding binding)
{
    return WebGPU::protectedFromAPI(binding)->isValid();
}

WGPUBool wgpuXRSubImageIsValid(WGPUXRSubImage subImage)
{
    return WebGPU::protectedFromAPI(subImage)->isValid();
}

WGPUBool wgpuXRProjectionLayerIsValid(WGPUXRProjectionLayer layer)
{
    return WebGPU::protectedFromAPI(layer)->isValid();
}

WGPUBool wgpuXRViewIsValid(WGPUXRView view)
{
    return WebGPU::protectedFromAPI(view)->isValid();
}

WGPUDDMesh wgpuDDMeshCreate(WGPUInstance instance, const WGPUDDMeshDescriptor* descriptor)
{
    return WebGPU::releaseToAPI(WebGPU::protectedFromAPI(instance)->createMesh(*descriptor));
}
