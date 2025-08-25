/*
 * Copyright (C) 2008-2025 Apple Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1.  Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer. 
 * 2.  Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution. 
 * 3.  Neither the name of Apple Inc. ("Apple") nor the names of
 *     its contributors may be used to endorse or promote products derived
 *     from this software without specific prior written permission. 
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE AND ITS CONTRIBUTORS "AS IS" AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL APPLE OR ITS CONTRIBUTORS BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#pragma once

#include <wtf/Compiler.h>

WTF_ALLOW_UNSAFE_BUFFER_USAGE_BEGIN

#include <JavaScriptCore/CalleeBits.h>
#include <JavaScriptCore/CodeSpecializationKind.h>
#include <JavaScriptCore/ConcurrentJSLock.h>
#include <JavaScriptCore/DFGDoesGCCheck.h>
#include <JavaScriptCore/DeleteAllCodeEffort.h>
#include <JavaScriptCore/ExceptionEventLocation.h>
#include <JavaScriptCore/FunctionHasExecutedCache.h>
#include <JavaScriptCore/Heap.h>
#include <JavaScriptCore/ImplementationVisibility.h>
#include <JavaScriptCore/IndexingType.h>
#include <JavaScriptCore/Integrity.h>
#include <JavaScriptCore/Interpreter.h>
#include <JavaScriptCore/Intrinsic.h>
#include <JavaScriptCore/JSCJSValue.h>
#include <JavaScriptCore/JSDateMath.h>
#include <JavaScriptCore/JSLock.h>
#include <JavaScriptCore/JSONAtomStringCache.h>
#include <JavaScriptCore/KeyAtomStringCache.h>
#include <JavaScriptCore/MicrotaskQueue.h>
#include <JavaScriptCore/NativeFunction.h>
#include <JavaScriptCore/NumericStrings.h>
#include <JavaScriptCore/SlotVisitorMacros.h>
#include <JavaScriptCore/SmallStrings.h>
#include <JavaScriptCore/SourceTaintedOrigin.h>
#include <JavaScriptCore/StringReplaceCache.h>
#include <JavaScriptCore/StringSplitCache.h>
#include <JavaScriptCore/Strong.h>
#include <JavaScriptCore/SubspaceAccess.h>
#include <JavaScriptCore/ThunkGenerator.h>
#include <JavaScriptCore/VMTraps.h>
#include <JavaScriptCore/WasmContext.h>
#include <JavaScriptCore/WeakGCMap.h>
#include <JavaScriptCore/WriteBarrier.h>
#include <wtf/BumpPointerAllocator.h>
#include <wtf/CheckedArithmetic.h>
#include <wtf/DoublyLinkedList.h>
#include <wtf/Forward.h>
#include <wtf/Gigacage.h>
#include <wtf/HashMap.h>
#include <wtf/LazyRef.h>
#include <wtf/LazyUniqueRef.h>
#include <wtf/MallocPtr.h>
#include <wtf/SetForScope.h>
#include <wtf/StackPointer.h>
#include <wtf/Stopwatch.h>
#include <wtf/TZoneMalloc.h>
#include <wtf/ThreadSafeRefCountedWithSuppressingSaferCPPChecking.h>
#include <wtf/ThreadSafeWeakHashSet.h>
#include <wtf/UniqueArray.h>
#include <wtf/WeakRandom.h>
#include <wtf/text/AdaptiveStringSearcher.h>
#include <wtf/text/StringImpl.h>
#include <wtf/text/SymbolImpl.h>
#include <wtf/text/SymbolRegistry.h>
#include <wtf/text/UniquedStringImpl.h>

#if ENABLE(REGEXP_TRACING)
#include <wtf/ListHashSet.h>
#endif

// Enable the Objective-C API for platforms with a modern runtime. This has to match exactly what we
// have in JSBase.h.
#if !defined(JSC_OBJC_API_ENABLED)
#if (defined(__clang__) && defined(__APPLE__) && (defined(__MAC_OS_X_VERSION_MIN_REQUIRED) || (defined(TARGET_OS_IPHONE) && TARGET_OS_IPHONE)))
#define JSC_OBJC_API_ENABLED 1
#else
#define JSC_OBJC_API_ENABLED 0
#endif
#endif

namespace WTF {
class RunLoop;
class SimpleStats;
class StackTrace;
} // namespace WTF
using WTF::SimpleStats;
using WTF::StackTrace;

namespace JSC {

class ArgList;
class BuiltinExecutables;
class BytecodeIntrinsicRegistry;
class CallFrame;
enum class CallMode;
enum class CommonJITThunkID : uint8_t;
struct CheckpointOSRExitSideState;
class CodeBlock;
class CodeCache;
class CommonIdentifiers;
class CompactTDZEnvironmentMap;
class ConservativeRoots;
class ControlFlowProfiler;
class Exception;
class ExceptionScope;
class FuzzerAgent;
class HasOwnPropertyCache;
class HeapAnalyzer;
class HeapProfiler;
class IntlCache;
class JSDestructibleObjectHeapCellType;
class JSGlobalObject;
class JSObject;
class JSPromise;
class JSPropertyNameEnumerator;
class JITSizeStatistics;
class JITThunks;
class MegamorphicCache;
class NativeExecutable;
class Debugger;
class DeferredWorkTimer;
class RegExp;
class RegExpCache;
class Register;
#if ENABLE(SAMPLING_PROFILER)
class SamplingProfiler;
#endif
class ShadowChicken;
class SharedJITStubSet;
class SourceProvider;
class SourceProviderCache;
class StackFrame;
class Structure;
class Symbol;
class TypedArrayController;
class VMEntryScope;
class TypeProfiler;
class TypeProfilerLog;
class Watchdog;
class WatchpointSet;
class Waiter;

constexpr bool validateDFGDoesGC = ENABLE_DFG_DOES_GC_VALIDATION;

#if ENABLE(FTL_JIT)
namespace FTL {
class Thunks;
}
#endif // ENABLE(FTL_JIT)
namespace Profiler {
class Database;
}
namespace DOMJIT {
class Signature;
}

struct EntryFrame;

DECLARE_ALLOCATOR_WITH_HEAP_IDENTIFIER(VM);

struct ScratchBuffer {
    ScratchBuffer()
    {
        u.m_activeLength = 0;
    }

    static ScratchBuffer* create(size_t size)
    {
        ScratchBuffer* result = new (VMMalloc::malloc(ScratchBuffer::allocationSize(size))) ScratchBuffer;
        return result;
    }

    static ScratchBuffer* fromData(void* buffer)
    {
        return std::bit_cast<ScratchBuffer*>(static_cast<char*>(buffer) - OBJECT_OFFSETOF(ScratchBuffer, m_buffer));
    }

    static size_t allocationSize(Checked<size_t> bufferSize) { return sizeof(ScratchBuffer) + bufferSize; }
    void setActiveLength(size_t activeLength) { u.m_activeLength = activeLength; }
    size_t activeLength() const { return u.m_activeLength; };
    size_t* addressOfActiveLength() { return &u.m_activeLength; };
    void* dataBuffer() { return m_buffer; }

    union {
        size_t m_activeLength;
        double pad; // Make sure m_buffer is double aligned.
    } u;
    void* m_buffer[0];
};

class ActiveScratchBufferScope {
public:
    ActiveScratchBufferScope(ScratchBuffer*, size_t activeScratchBufferSizeInJSValues);
    ~ActiveScratchBufferScope();

private:
    ScratchBuffer* m_scratchBuffer;
};

enum VMIdentifierType { };
using VMIdentifier = AtomicObjectIdentifier<VMIdentifierType>;

class VM : public ThreadSafeRefCountedWithSuppressingSaferCPPChecking<VM>, public DoublyLinkedListNode<VM> {
    WTF_DEPRECATED_MAKE_FAST_ALLOCATED_WITH_HEAP_IDENTIFIER(VM, VM);
public:
    // WebCore has a one-to-one mapping of threads to VMs;
    // create() should only be called once
    // on a thread, this is the 'default' VM (it uses the
    // thread's default string uniquing table from Thread::currentSingleton()).
    enum class VMType { Default, APIContextGroup };

    struct ClientData {
        JS_EXPORT_PRIVATE virtual ~ClientData() { };

        JS_EXPORT_PRIVATE virtual String overrideSourceURL(const StackFrame&, const String& originalSourceURL) const = 0;

        virtual bool isWebCoreJSClientData() const { return false; }
    };

    bool usingAPI() { return vmType != VMType::Default; }

    JS_EXPORT_PRIVATE static Ref<VM> create(HeapType = HeapType::Small, WTF::RunLoop* = nullptr);
    JS_EXPORT_PRIVATE static RefPtr<VM> tryCreate(HeapType = HeapType::Small, WTF::RunLoop* = nullptr);
    static Ref<VM> createContextGroup(HeapType = HeapType::Small);
    JS_EXPORT_PRIVATE ~VM();

    Watchdog* watchdog() { return m_watchdog.getIfExists(); }
    Watchdog& ensureWatchdog() { return m_watchdog.get(*this); }

    HeapProfiler* heapProfiler() { return m_heapProfiler.getIfExists(); }
    HeapProfiler& ensureHeapProfiler() { return m_heapProfiler.get(*this); }

    AdaptiveStringSearcherTables& adaptiveStringSearcherTables() { return m_stringSearcherTables.get(*this); }

    bool isAnalyzingHeap() const { return m_activeHeapAnalyzer; }
    HeapAnalyzer* activeHeapAnalyzer() const { return m_activeHeapAnalyzer; }
    void setActiveHeapAnalyzer(HeapAnalyzer* analyzer) { m_activeHeapAnalyzer = analyzer; }

#if ENABLE(SAMPLING_PROFILER)
    SamplingProfiler* samplingProfiler() { return m_samplingProfiler.get(); }
    JS_EXPORT_PRIVATE SamplingProfiler& ensureSamplingProfiler(Ref<Stopwatch>&&);

    JS_EXPORT_PRIVATE void enableSamplingProfiler();
    JS_EXPORT_PRIVATE void disableSamplingProfiler();
    JS_EXPORT_PRIVATE RefPtr<JSON::Value> takeSamplingProfilerSamplesAsJSON();
#endif

    FuzzerAgent* fuzzerAgent() const { return m_fuzzerAgent.get(); }
    void setFuzzerAgent(std::unique_ptr<FuzzerAgent>&&);

    VMIdentifier identifier() const { return m_identifier; }
    bool isEntered() const { return !!entryScope; }

    inline CallFrame* topJSCallFrame() const;

    // Global object in which execution began.
    JS_EXPORT_PRIVATE JSGlobalObject* deprecatedVMEntryGlobalObject(JSGlobalObject*) const;

    WeakRandom& random() { return m_random; }
    WeakRandom& heapRandom() { return m_heapRandom; }
    Integrity::Random& integrityRandom() { return m_integrityRandom; }

    template<typename Type, typename Functor>
    Type& ensureSideData(void* key, const Functor&);

    bool hasTerminationRequest() const { return m_hasTerminationRequest; }
    void clearHasTerminationRequest()
    {
        m_hasTerminationRequest = false;
        clearEntryScopeService(EntryScopeService::ResetTerminationRequest);
    }
    void setHasTerminationRequest()
    {
        m_hasTerminationRequest = true;
        requestEntryScopeService(EntryScopeService::ResetTerminationRequest);
    }

    bool executionForbidden() const { return m_executionForbidden; }
    void setExecutionForbidden() { m_executionForbidden = true; }

    // Setting this means that the VM can never recover from a TerminationException.
    // Currently, we'll only set this for worker threads. Ideally, we want this
    // to always be true. However, we're only limiting it to workers for now until
    // we can be sure that clients using the JSC watchdog (which uses termination)
    // isn't broken by this change.
    void forbidExecutionOnTermination() { m_executionForbiddenOnTermination = true; }

    JS_EXPORT_PRIVATE Exception* ensureTerminationException();
    Exception* terminationException() const
    {
        ASSERT(m_terminationException);
        return m_terminationException;
    }
    bool isTerminationException(Exception* exception) const
    {
        ASSERT(exception);
        return exception == m_terminationException;
    }
    bool hasPendingTerminationException() const
    {
        return m_exception && isTerminationException(m_exception);
    }

    void throwTerminationException();

    enum class EntryScopeService : uint8_t {
        // Sticky services i.e. if set, these will never be cleared.
        SamplingProfiler = 1 << 0,
        TracePoints = 1 << 1,
        Watchdog = 1 << 2,

        // Transient services i.e. these will never be cleared after they are serviced once, and can be set again later.
        ClearScratchBuffers = 1 << 3,
        FirePrimitiveGigacageEnabled = 1 << 4,
        PopListeners = 1 << 5,
        ResetTerminationRequest = 1 << 6,
    };

    bool hasAnyEntryScopeServiceRequest() { return !m_entryScopeServices.isEmpty(); }
    void executeEntryScopeServicesOnEntry();
    void executeEntryScopeServicesOnExit();

    void requestEntryScopeService(EntryScopeService service)
    {
        m_entryScopeServices.add(service);
    }

    enum class SchedulerOptions : uint8_t {
        HasImminentlyScheduledWork = 1 << 0,
    };
    JS_EXPORT_PRIVATE void performOpportunisticallyScheduledTasks(MonotonicTime deadline, OptionSet<SchedulerOptions>);

    Structure* cellButterflyStructure(IndexingType indexingType) { return rawImmutableButterflyStructure(indexingType).get(); }

    // Keep super frequently accessed fields top in VM.
    unsigned disallowVMEntryCount { 0 };
private:
    Exception* m_exception { nullptr };
    Exception* m_terminationException { nullptr };
    Exception* m_lastException { nullptr };
public:
    // NOTE: When throwing an exception while rolling back the call frame, this may be equal to
    // topEntryFrame.
    // FIXME: This should be a void*, because it might not point to a CallFrame.
    // https://bugs.webkit.org/show_bug.cgi?id=160441
    CallFrame* topCallFrame { nullptr };
    EntryFrame* topEntryFrame { nullptr };
private:
    OptionSet<EntryScopeService> m_entryScopeServices;
public:
    bool didEnterVM { false };
private:
    VMTraps m_traps;

    VMIdentifier m_identifier;
    const Ref<JSLock> m_apiLock;
    const Ref<WTF::RunLoop> m_runLoop;

    WeakRandom m_random;
    WeakRandom m_heapRandom;
    Integrity::Random m_integrityRandom;

    bool hasEntryScopeServiceRequest(EntryScopeService service)
    {
        return m_entryScopeServices.contains(service);
    }

    void clearEntryScopeService(EntryScopeService service)
    {
        m_entryScopeServices.remove(service);
    }

    WriteBarrier<Structure>& rawImmutableButterflyStructure(IndexingType indexingType) { return cellButterflyStructures[arrayIndexFromIndexingType(indexingType) - NumberOfIndexingShapes]; }

public:
    Heap heap;
    GCClient::Heap clientHeap;

    bool isInService() const { return m_isInService; }

    const HeapCellType& cellHeapCellType() { return heap.cellHeapCellType; }
    const JSDestructibleObjectHeapCellType& destructibleObjectHeapCellType() { return heap.destructibleObjectHeapCellType; };

#if ENABLE(JIT)
    std::unique_ptr<JITSizeStatistics> jitSizeStatistics;
#endif
    
    ALWAYS_INLINE CompleteSubspace& primitiveGigacageAuxiliarySpace() { return heap.primitiveGigacageAuxiliarySpace; }
    ALWAYS_INLINE CompleteSubspace& auxiliarySpace() { return heap.auxiliarySpace; }
    ALWAYS_INLINE CompleteSubspace& immutableButterflyAuxiliarySpace() { return heap.immutableButterflyAuxiliarySpace; }
    ALWAYS_INLINE CompleteSubspace& gigacageAuxiliarySpace(Gigacage::Kind kind) { return heap.gigacageAuxiliarySpace(kind); }
    ALWAYS_INLINE CompleteSubspace& cellSpace() { return heap.cellSpace; }
    ALWAYS_INLINE CompleteSubspace& destructibleObjectSpace() { return heap.destructibleObjectSpace; }
#if ENABLE(WEBASSEMBLY)
    template<SubspaceAccess mode>
    ALWAYS_INLINE GCClient::PreciseSubspace* webAssemblyInstanceSpace() { return heap.webAssemblyInstanceSpace<mode>(); }
#endif

#define DEFINE_ISO_SUBSPACE_ACCESSOR(name, heapCellType, type) \
    ALWAYS_INLINE GCClient::IsoSubspace& name() { return clientHeap.name; }

    FOR_EACH_JSC_ISO_SUBSPACE(DEFINE_ISO_SUBSPACE_ACCESSOR)
#undef DEFINE_ISO_SUBSPACE_ACCESSOR

#define DEFINE_DYNAMIC_ISO_SUBSPACE_ACCESSOR_IMPL(name, heapCellType, type) \
    template<SubspaceAccess mode> \
    ALWAYS_INLINE GCClient::IsoSubspace* name() { return clientHeap.name<mode>(); }

#define DEFINE_DYNAMIC_ISO_SUBSPACE_ACCESSOR(name) \
    DEFINE_DYNAMIC_ISO_SUBSPACE_ACCESSOR_IMPL(name, unused, unused2)

    FOR_EACH_JSC_DYNAMIC_ISO_SUBSPACE(DEFINE_DYNAMIC_ISO_SUBSPACE_ACCESSOR_IMPL)

    ALWAYS_INLINE GCClient::IsoSubspace& codeBlockSpace() { return clientHeap.codeBlockSpace; }

    DEFINE_DYNAMIC_ISO_SUBSPACE_ACCESSOR(evalExecutableSpace)
    DEFINE_DYNAMIC_ISO_SUBSPACE_ACCESSOR(moduleProgramExecutableSpace)

#undef DEFINE_DYNAMIC_ISO_SUBSPACE_ACCESSOR_IMPL
#undef DEFINE_DYNAMIC_ISO_SUBSPACE_GETTER

    ALWAYS_INLINE GCClient::IsoSubspace& functionExecutableSpace() { return clientHeap.functionExecutableSpace; }
    ALWAYS_INLINE GCClient::IsoSubspace& programExecutableSpace() { return clientHeap.programExecutableSpace; }
    ALWAYS_INLINE GCClient::IsoSubspace& unlinkedFunctionExecutableSpace() { return clientHeap.unlinkedFunctionExecutableSpace; }

    VMType vmType;
    bool m_mightBeExecutingTaintedCode { false };
    ClientData* clientData { nullptr };
#if ENABLE(WEBASSEMBLY)
    Wasm::Context wasmContext;
#endif
    WriteBarrier<Structure> structureStructure;
    WriteBarrier<Structure> structureRareDataStructure;
    WriteBarrier<Structure> stringStructure;
    WriteBarrier<Structure> propertyNameEnumeratorStructure;
    WriteBarrier<Structure> getterSetterStructure;
    WriteBarrier<Structure> customGetterSetterStructure;
    WriteBarrier<Structure> domAttributeGetterSetterStructure;
    WriteBarrier<Structure> scopedArgumentsTableStructure;
    WriteBarrier<Structure> apiWrapperStructure;
    WriteBarrier<Structure> nativeExecutableStructure;
    WriteBarrier<Structure> evalExecutableStructure;
    WriteBarrier<Structure> programExecutableStructure;
    WriteBarrier<Structure> functionExecutableStructure;
#if ENABLE(WEBASSEMBLY)
    WriteBarrier<Structure> webAssemblyCalleeGroupStructure;
#endif
    WriteBarrier<Structure> moduleProgramExecutableStructure;
    WriteBarrier<Structure> regExpStructure;
    WriteBarrier<Structure> symbolStructure;
    WriteBarrier<Structure> symbolTableStructure;
    std::array<WriteBarrier<Structure>, NumberOfCopyOnWriteIndexingModes> cellButterflyStructures;
    WriteBarrier<Structure> cellButterflyOnlyAtomStringsStructure;
    WriteBarrier<Structure> sourceCodeStructure;
    WriteBarrier<Structure> scriptFetcherStructure;
    WriteBarrier<Structure> scriptFetchParametersStructure;
    WriteBarrier<Structure> structureChainStructure;
    WriteBarrier<Structure> sparseArrayValueMapStructure;
    WriteBarrier<Structure> templateObjectDescriptorStructure;
    WriteBarrier<Structure> unlinkedFunctionExecutableStructure;
    WriteBarrier<Structure> unlinkedProgramCodeBlockStructure;
    WriteBarrier<Structure> unlinkedEvalCodeBlockStructure;
    WriteBarrier<Structure> unlinkedFunctionCodeBlockStructure;
    WriteBarrier<Structure> unlinkedModuleProgramCodeBlockStructure;
    WriteBarrier<Structure> propertyTableStructure;
    WriteBarrier<Structure> functionRareDataStructure;
    WriteBarrier<Structure> exceptionStructure;
    WriteBarrier<Structure> programCodeBlockStructure;
    WriteBarrier<Structure> moduleProgramCodeBlockStructure;
    WriteBarrier<Structure> evalCodeBlockStructure;
    WriteBarrier<Structure> functionCodeBlockStructure;
    WriteBarrier<Structure> hashMapBucketSetStructure;
    WriteBarrier<Structure> hashMapBucketMapStructure;
    WriteBarrier<Structure> bigIntStructure;

    WriteBarrier<JSPropertyNameEnumerator> m_emptyPropertyNameEnumerator;

    WriteBarrier<JSCell> m_orderedHashTableDeletedValue;
    WriteBarrier<JSCell> m_orderedHashTableSentinel;

    WriteBarrier<NativeExecutable> m_fastCanConstructBoundExecutable;
    WriteBarrier<NativeExecutable> m_slowCanConstructBoundExecutable;

    Weak<NativeExecutable> m_fastRemoteFunctionExecutable;
    Weak<NativeExecutable> m_slowRemoteFunctionExecutable;

    const Ref<DeferredWorkTimer> deferredWorkTimer;

    JSCell* currentlyDestructingCallbackObject { nullptr };
    const ClassInfo* currentlyDestructingCallbackObjectClassInfo { nullptr };

    AtomStringTable* m_atomStringTable;
    WTF::SymbolRegistry m_symbolRegistry;
    WTF::SymbolRegistry m_privateSymbolRegistry { WTF::SymbolRegistry::Type::PrivateSymbol };
    CommonIdentifiers* propertyNames { nullptr };
    const ArgList* emptyList;
    SmallStrings smallStrings;
    NumericStrings numericStrings;
    std::unique_ptr<SimpleStats> machineCodeBytesPerBytecodeWordForBaselineJIT;
    WriteBarrier<JSString> lastCachedString;
    Ref<StringImpl> lastAtomizedIdentifierStringImpl { *StringImpl::empty() };
    Ref<AtomStringImpl> lastAtomizedIdentifierAtomStringImpl { *static_cast<AtomStringImpl*>(StringImpl::empty()) };
    JSONAtomStringCache jsonAtomStringCache;
    KeyAtomStringCache keyAtomStringCache;
    StringSplitCache stringSplitCache;
    Vector<unsigned> stringSplitIndice;
    StringReplaceCache stringReplaceCache;

    bool mightBeExecutingTaintedCode() const { return m_mightBeExecutingTaintedCode; }
    bool* addressOfMightBeExecutingTaintedCode() { return &m_mightBeExecutingTaintedCode; }
    void setMightBeExecutingTaintedCode(bool value = true) { m_mightBeExecutingTaintedCode = value; }

    AtomStringTable* atomStringTable() const { return m_atomStringTable; }
    WTF::SymbolRegistry& symbolRegistry() { return m_symbolRegistry; }
    WTF::SymbolRegistry& privateSymbolRegistry() { return m_privateSymbolRegistry; }

    WriteBarrier<JSBigInt> heapBigIntConstantOne;

    JSCell* orderedHashTableDeletedValue()
    {
        if (m_orderedHashTableDeletedValue) [[likely]]
            return m_orderedHashTableDeletedValue.get();
        return orderedHashTableDeletedValueSlow();
    }

    JSCell* orderedHashTableSentinel()
    {
        if (m_orderedHashTableSentinel) [[likely]]
            return m_orderedHashTableSentinel.get();
        return orderedHashTableSentinelSlow();
    }

    JSPropertyNameEnumerator* emptyPropertyNameEnumerator()
    {
        if (m_emptyPropertyNameEnumerator) [[likely]]
            return m_emptyPropertyNameEnumerator.get();
        return emptyPropertyNameEnumeratorSlow();
    }

    WeakGCMap<SymbolImpl*, Symbol, PtrHash<SymbolImpl*>> symbolImplToSymbolMap;
    WeakGCMap<StringImpl*, JSString, PtrHash<StringImpl*>> atomStringToJSStringMap;

    enum class DeletePropertyMode {
        // Default behaviour of deleteProperty, matching the spec.
        Default,
        // This setting causes deleteProperty to force deletion of all
        // properties including those that are non-configurable (DontDelete).
        IgnoreConfigurable
    };

    DeletePropertyMode deletePropertyMode()
    {
        return m_deletePropertyMode;
    }

    class DeletePropertyModeScope {
    public:
        DeletePropertyModeScope(VM& vm, DeletePropertyMode mode)
            : m_vm(vm)
            , m_previousMode(vm.m_deletePropertyMode)
        {
            m_vm.m_deletePropertyMode = mode;
        }

        ~DeletePropertyModeScope()
        {
            m_vm.m_deletePropertyMode = m_previousMode;
        }

    private:
        VM& m_vm;
        DeletePropertyMode m_previousMode;
    };

    static JS_EXPORT_PRIVATE bool canUseAssembler();
    static bool isInMiniMode()
    {
        return !Options::useJIT() || Options::forceMiniVMMode();
    }

    static bool useUnlinkedCodeBlockJettisoning()
    {
        return Options::useUnlinkedCodeBlockJettisoning() || isInMiniMode();
    }

    static void computeCanUseJIT();

    SourceProviderCache* addSourceProviderCache(SourceProvider*);
    void clearSourceProviderCaches();

    typedef UncheckedKeyHashMap<RefPtr<SourceProvider>, RefPtr<SourceProviderCache>> SourceProviderCacheMap;
    SourceProviderCacheMap sourceProviderCacheMap;
#if ENABLE(JIT)
    std::unique_ptr<JITThunks> jitStubs;
    MacroAssemblerCodeRef<JITThunkPtrTag> getCTIStub(ThunkGenerator);
    MacroAssemblerCodeRef<JITThunkPtrTag> getCTIStub(CommonJITThunkID);
    std::unique_ptr<SharedJITStubSet> m_sharedJITStubs;
#endif
#if ENABLE(FTL_JIT)
    std::unique_ptr<FTL::Thunks> ftlThunks;
#endif

    NativeExecutable* getHostFunction(NativeFunction, ImplementationVisibility, NativeFunction constructor, const String& name);
    NativeExecutable* getHostFunction(NativeFunction, ImplementationVisibility, Intrinsic, NativeFunction constructor, const DOMJIT::Signature*, const String& name);

    NativeExecutable* getBoundFunction(bool isJSFunction, SourceTaintedOrigin taintedness);
    NativeExecutable* getRemoteFunction(bool isJSFunction);

    CodePtr<JSEntryPtrTag> getCTIInternalFunctionTrampolineFor(CodeSpecializationKind);
    MacroAssemblerCodeRef<JSEntryPtrTag> getCTIThrowExceptionFromCallSlowPath();
    MacroAssemblerCodeRef<JITStubRoutinePtrTag> getCTIVirtualCall(CallMode);

    static constexpr ptrdiff_t exceptionOffset()
    {
        return OBJECT_OFFSETOF(VM, m_exception);
    }

    static constexpr ptrdiff_t callFrameForCatchOffset()
    {
        return OBJECT_OFFSETOF(VM, callFrameForCatch);
    }

    static constexpr ptrdiff_t topEntryFrameOffset()
    {
        return OBJECT_OFFSETOF(VM, topEntryFrame);
    }

    static constexpr ptrdiff_t offsetOfEncodedHostCallReturnValue()
    {
        return OBJECT_OFFSETOF(VM, encodedHostCallReturnValue);
    }

    static constexpr ptrdiff_t offsetOfHeapBarrierThreshold()
    {
        return OBJECT_OFFSETOF(VM, heap) + OBJECT_OFFSETOF(Heap, m_barrierThreshold);
    }

    static constexpr ptrdiff_t offsetOfHeapMutatorShouldBeFenced()
    {
        return OBJECT_OFFSETOF(VM, heap) + OBJECT_OFFSETOF(Heap, m_mutatorShouldBeFenced);
    }

    static constexpr ptrdiff_t offsetOfTraps()
    {
        return OBJECT_OFFSETOF(VM, m_traps);
    }

    static constexpr ptrdiff_t offsetOfTrapsBits()
    {
        return offsetOfTraps() + VMTraps::offsetOfTrapsBits();
    }

    static constexpr ptrdiff_t offsetOfSoftStackLimit()
    {
        return offsetOfTraps() + VMTraps::offsetOfSoftStackLimit();
    }

    void clearLastException() { m_lastException = nullptr; }

    CallFrame** addressOfCallFrameForCatch() { return &callFrameForCatch; }

    JSCell** addressOfException() { return reinterpret_cast<JSCell**>(&m_exception); }

    Exception* lastException() const { return m_lastException; }
    JSCell** addressOfLastException() { return reinterpret_cast<JSCell**>(&m_lastException); }

    // This should only be used for code that wants to check for any pending
    // exception without interfering with Throw/CatchScopes.
    Exception* exceptionForInspection() const { return m_exception; }

    void setFailNextNewCodeBlock() { m_failNextNewCodeBlock = true; }
    bool getAndClearFailNextNewCodeBlock()
    {
        bool result = m_failNextNewCodeBlock;
        m_failNextNewCodeBlock = false;
        return result;
    }
    
    void* stackPointerAtVMEntry() const { return m_stackPointerAtVMEntry; }
    void setStackPointerAtVMEntry(void*);

    size_t softReservedZoneSize() const { return m_currentSoftReservedZoneSize; }
    size_t updateSoftReservedZoneSize(size_t softReservedZoneSize);
    
    static size_t committedStackByteCount();
    inline bool ensureJSStackCapacityFor(Register* newTopOfStack);

    void* stackLimit() { return m_stackLimit; }
    ALWAYS_INLINE void* softStackLimit() const { return m_traps.softStackLimit(); }
    ALWAYS_INLINE void** addressOfSoftStackLimit() { return m_traps.addressOfSoftStackLimit(); }

    inline bool isSafeToRecurseSoft() const;
    bool isSafeToRecurse() const
    {
        return isSafeToRecurse(m_stackLimit);
    }

    void* lastStackTop() { return m_lastStackTop; }
    void setLastStackTop(const Thread&);
    
#if ENABLE(C_LOOP)
    ALWAYS_INLINE CLoopStack& cloopStack() { return m_traps.cloopStack(); }
    ALWAYS_INLINE const CLoopStack& cloopStack() const { return m_traps.cloopStack(); }
    ALWAYS_INLINE void* cloopStackLimit() { return m_traps.cloopStackLimit(); }
    ALWAYS_INLINE void* currentCLoopStackPointer() const { return m_traps.currentCLoopStackPointer(); }
#endif

    EncodedJSValue encodedHostCallReturnValue { };
    CallFrame* newCallFrameReturnValue;
    CallFrame* callFrameForCatch { nullptr };
    void* targetMachinePCForThrow;
    void* targetMachinePCAfterCatch;
    JSOrWasmInstruction targetInterpreterPCForThrow;
    uintptr_t targetInterpreterMetadataPCForThrow;
    uint32_t targetTryDepthForThrow;

    unsigned varargsLength;
    uint32_t osrExitIndex;
    void* osrExitJumpDestination;
    RegExp* m_executingRegExp { nullptr };

    // The threading protocol here is as follows:
    // - You can call scratchBufferForSize from any thread.
    // - You can only set the ScratchBuffer's activeLength from the main thread.
    // - You can only write to entries in the ScratchBuffer from the main thread.
    ScratchBuffer* scratchBufferForSize(size_t size);
    void clearScratchBuffers();
    bool isScratchBuffer(void*);

    EncodedJSValue* exceptionFuzzingBuffer(size_t size)
    {
        ASSERT(Options::useExceptionFuzz());
        if (!m_exceptionFuzzBuffer)
            m_exceptionFuzzBuffer = MallocPtr<EncodedJSValue, VMMalloc>::malloc(size);
        return m_exceptionFuzzBuffer.get();
    }

    void gatherScratchBufferRoots(ConservativeRoots&);

    static constexpr unsigned expectedMaxActiveSideStateCount = 4;
    void pushCheckpointOSRSideState(std::unique_ptr<CheckpointOSRExitSideState>&&);
    std::unique_ptr<CheckpointOSRExitSideState> popCheckpointOSRSideState(CallFrame* expectedFrame);
    void popAllCheckpointOSRSideStateUntil(CallFrame* targetFrame);
    bool hasCheckpointOSRSideState() const { return m_checkpointSideState.size(); }
    void scanSideState(ConservativeRoots&) const;

    Interpreter interpreter;
    VMEntryScope* entryScope { nullptr };

    JSObject* stringRecursionCheckFirstObject { nullptr };
    UncheckedKeyHashSet<JSObject*> stringRecursionCheckVisitedObjects;

    DateCache dateCache;

    std::unique_ptr<Profiler::Database> m_perBytecodeProfiler;
    RefPtr<TypedArrayController> m_typedArrayController;
    std::unique_ptr<RegExpCache> m_regExpCache;
    BumpPointerAllocator m_regExpAllocator;
    ConcurrentJSLock m_regExpAllocatorLock;

#if ENABLE(YARR_JIT_ALL_PARENS_EXPRESSIONS)
    static constexpr size_t patternContextBufferSize = 8192; // Space allocated to save nested parenthesis context
    Lock m_regExpPatternContextLock;
    UniqueArray<char> m_regExpPatternContexBuffer;
    char* acquireRegExpPatternContexBuffer() WTF_ACQUIRES_LOCK(m_regExpPatternContextLock);
    void releaseRegExpPatternContexBuffer() WTF_RELEASES_LOCK(m_regExpPatternContextLock);
#else
    static constexpr size_t patternContextBufferSize = 0; // Space allocated to save nested parenthesis context
#endif

    const Ref<CompactTDZEnvironmentMap> m_compactVariableMap;

    LazyUniqueRef<VM, HasOwnPropertyCache> m_hasOwnPropertyCache;
    ALWAYS_INLINE HasOwnPropertyCache* hasOwnPropertyCache() { return m_hasOwnPropertyCache.getIfExists(); }
    HasOwnPropertyCache& ensureHasOwnPropertyCache() { return m_hasOwnPropertyCache.get(*this); }

    LazyUniqueRef<VM, MegamorphicCache> m_megamorphicCache;
    ALWAYS_INLINE MegamorphicCache* megamorphicCache() { return m_megamorphicCache.getIfExists(); }
    MegamorphicCache& ensureMegamorphicCache() { return m_megamorphicCache.get(*this); }

    enum class StructureChainIntegrityEvent : uint8_t {
        Add,
        Remove,
        Change,
        Prototype,
    };
    JS_EXPORT_PRIVATE void invalidateStructureChainIntegrity(StructureChainIntegrityEvent);

#if ENABLE(REGEXP_TRACING)
    using RTTraceList = ListHashSet<RegExp*>;
    RTTraceList m_rtTraceList;
    void addRegExpToTrace(RegExp*);
    JS_EXPORT_PRIVATE void dumpRegExpTrace();
#endif

    bool hasTimeZoneChange() { return dateCache.hasTimeZoneChange(); }

    RegExpCache* regExpCache() { return m_regExpCache.get(); }

    bool isCollectorBusyOnCurrentThread() { return heap.currentThreadIsDoingGCWork(); }

#if ENABLE(GC_VALIDATION)
    bool isInitializingObject() const; 
    void setInitializingObjectClass(const ClassInfo*);
#endif

    bool currentThreadIsHoldingAPILock() const { return m_apiLock->currentThreadIsHoldingLock(); }

    JSLock& apiLock() { return m_apiLock.get(); }
    CodeCache* codeCache() { return m_codeCache.get(); }
    IntlCache& intlCache() { return *m_intlCache; }

    JS_EXPORT_PRIVATE void whenIdle(Function<void()>&&);

    JS_EXPORT_PRIVATE void deleteAllCode(DeleteAllCodeEffort);
    JS_EXPORT_PRIVATE void deleteAllLinkedCode(DeleteAllCodeEffort);

    void shrinkFootprintWhenIdle();

    WatchpointSet* ensureWatchpointSetForImpureProperty(UniquedStringImpl*);
    
    // FIXME: Use AtomString once it got merged with Identifier.
    JS_EXPORT_PRIVATE void addImpureProperty(UniquedStringImpl*);
    
    InlineWatchpointSet& primitiveGigacageEnabled() { return m_primitiveGigacageEnabled; }

    BuiltinExecutables* builtinExecutables() { return m_builtinExecutables.get(); }

    bool enableTypeProfiler();
    bool disableTypeProfiler();
    TypeProfilerLog* typeProfilerLog() { return m_typeProfilerLog.get(); }
    TypeProfiler* typeProfiler() { return m_typeProfiler.get(); }
    JS_EXPORT_PRIVATE void dumpTypeProfilerData();

    FunctionHasExecutedCache* functionHasExecutedCache() { return &m_functionHasExecutedCache; }

    ControlFlowProfiler* controlFlowProfiler() { return m_controlFlowProfiler.get(); }
    bool enableControlFlowProfiler();
    bool disableControlFlowProfiler();

    class JS_EXPORT_PRIVATE DrainMicrotaskDelayScope {
    public:
        explicit DrainMicrotaskDelayScope(VM&);
        ~DrainMicrotaskDelayScope();

        DrainMicrotaskDelayScope(DrainMicrotaskDelayScope&&) = default;
        DrainMicrotaskDelayScope& operator=(DrainMicrotaskDelayScope&&);
        DrainMicrotaskDelayScope(const DrainMicrotaskDelayScope&);
        DrainMicrotaskDelayScope& operator=(const DrainMicrotaskDelayScope&);

    private:
        void increment();
        void decrement();

        RefPtr<VM> m_vm;
    };

    DrainMicrotaskDelayScope drainMicrotaskDelayScope() { return DrainMicrotaskDelayScope { *this }; }
    void queueMicrotask(QueuedTask&&);
    JS_EXPORT_PRIVATE void drainMicrotasks();
    void setOnEachMicrotaskTick(WTF::Function<void(VM&)>&& func) { m_onEachMicrotaskTick = WTFMove(func); }
    void callOnEachMicrotaskTick()
    {
        if (m_onEachMicrotaskTick)
            m_onEachMicrotaskTick(*this);
    }
    void finalizeSynchronousJSExecution()
    {
        ASSERT(currentThreadIsHoldingAPILock());
        m_currentWeakRefVersion++;
        setMightBeExecutingTaintedCode(false);
    }
    uintptr_t currentWeakRefVersion() const { return m_currentWeakRefVersion; }

    void setGlobalConstRedeclarationShouldThrow(bool globalConstRedeclarationThrow) { m_globalConstRedeclarationShouldThrow = globalConstRedeclarationThrow; }
    ALWAYS_INLINE bool globalConstRedeclarationShouldThrow() const { return m_globalConstRedeclarationShouldThrow; }

    void setShouldBuildPCToCodeOriginMapping() { m_shouldBuildPCToCodeOriginMapping = true; }
    bool shouldBuilderPCToCodeOriginMapping() const { return m_shouldBuildPCToCodeOriginMapping; }

    BytecodeIntrinsicRegistry& bytecodeIntrinsicRegistry() { return *m_bytecodeIntrinsicRegistry; }
    
    ShadowChicken* shadowChicken() { return m_shadowChicken.getIfExists(); }
    ShadowChicken& ensureShadowChicken() { return m_shadowChicken.get(*this); }
    
    template<typename Func>
    void logEvent(CodeBlock*, const char* summary, const Func& func);

    std::optional<RefPtr<Thread>> ownerThread() const { return m_apiLock->ownerThread(); }

    VMTraps& traps() { return m_traps; }

    JS_EXPORT_PRIVATE bool hasExceptionsAfterHandlingTraps();

    // These may be called concurrently from another thread.
    void notifyNeedDebuggerBreak() { m_traps.fireTrap(VMTraps::NeedDebuggerBreak); }
    void notifyNeedShellTimeoutCheck() { m_traps.fireTrap(VMTraps::NeedShellTimeoutCheck); }
    void notifyNeedTermination()
    {
        setHasTerminationRequest();
        m_traps.fireTrap(VMTraps::NeedTermination);
    }
    void notifyNeedWatchdogCheck() { m_traps.fireTrap(VMTraps::NeedWatchdogCheck); }

    void promiseRejected(JSPromise*);

#if ENABLE(EXCEPTION_SCOPE_VERIFICATION)
    StackTrace* nativeStackTraceOfLastThrow() const { return m_nativeStackTraceOfLastThrow.get(); }
    Thread* throwingThread() const { return m_throwingThread.get(); }
    bool needExceptionCheck() const { return m_needExceptionCheck; }
#endif

    WTF::RunLoop& runLoop() const { return m_runLoop; }

    static void setCrashOnVMCreation(bool);

    void addLoopHintExecutionCounter(const JSInstruction*);
    uintptr_t* getLoopHintExecutionCounter(const JSInstruction*);
    void removeLoopHintExecutionCounter(const JSInstruction*);

    ALWAYS_INLINE void writeBarrier(const JSCell* from) { heap.writeBarrier(from); }
    ALWAYS_INLINE void writeBarrier(const JSCell* from, JSValue to) { heap.writeBarrier(from, to); }
    ALWAYS_INLINE void writeBarrier(const JSCell* from, JSCell* to) { heap.writeBarrier(from, to); }
    ALWAYS_INLINE void writeBarrierSlowPath(const JSCell* from) { heap.writeBarrierSlowPath(from); }

    ALWAYS_INLINE void mutatorFence() { heap.mutatorFence(); }

#if ENABLE(DFG_DOES_GC_VALIDATION)
    DoesGCCheck* addressOfDoesGC() { return &m_doesGC; }
    void setDoesGCExpectation(bool expectDoesGC, unsigned nodeIndex, unsigned nodeOp) { m_doesGC.set(expectDoesGC, nodeIndex, nodeOp); }
    void setDoesGCExpectation(bool expectDoesGC, DoesGCCheck::Special special) { m_doesGC.set(expectDoesGC, special); }
    void verifyCanGC() { m_doesGC.verifyCanGC(*this); }
#else
    DoesGCCheck* addressOfDoesGC() { UNREACHABLE_FOR_PLATFORM(); return nullptr; }
    void setDoesGCExpectation(bool, unsigned, unsigned) { }
    void setDoesGCExpectation(bool, DoesGCCheck::Special) { }
    void verifyCanGC() { }
#endif

    void beginMarking();
    DECLARE_VISIT_AGGREGATE;

    void addDebugger(Debugger&);
    void removeDebugger(Debugger&);
    template<typename Func>
    void forEachDebugger(const Func&);

    void changeNumberOfActiveJITPlans(int64_t value)
    {
        m_numberOfActiveJITPlans.fetch_add(value, std::memory_order_relaxed);
    }

    int64_t numberOfActiveJITPlans() const { return m_numberOfActiveJITPlans.load(std::memory_order_relaxed); }

    Ref<Waiter> syncWaiter();

    void notifyDebuggerHookInjected() { m_isDebuggerHookInjected = true; }
    bool isDebuggerHookInjected() const { return m_isDebuggerHookInjected; }

private:
    VM(VMType, HeapType, WTF::RunLoop* = nullptr, bool* success = nullptr);
    static VM*& sharedInstanceInternal();
    void createNativeThunk();

    JS_EXPORT_PRIVATE JSCell* orderedHashTableDeletedValueSlow();
    JS_EXPORT_PRIVATE JSCell* orderedHashTableSentinelSlow();
    JSPropertyNameEnumerator* emptyPropertyNameEnumeratorSlow();

    void updateStackLimits();

    bool isSafeToRecurse(void* stackLimit) const
    {
        void* curr = currentStackPointer();
        return curr >= stackLimit;
    }

    Exception* exception() const
    {
#if ENABLE(EXCEPTION_SCOPE_VERIFICATION)
        m_needExceptionCheck = false;
#endif
        return m_exception;
    }

    void clearException()
    {
#if ENABLE(EXCEPTION_SCOPE_VERIFICATION)
        m_needExceptionCheck = false;
        m_nativeStackTraceOfLastThrow = nullptr;
        m_throwingThread = nullptr;
#endif
        m_exception = nullptr;
        traps().clearTrap(VMTraps::NeedExceptionHandling);
    }

    JS_EXPORT_PRIVATE void setException(Exception*);

    JS_EXPORT_PRIVATE Exception* throwException(JSGlobalObject*, Exception*);
    JS_EXPORT_PRIVATE Exception* throwException(JSGlobalObject*, JSValue);
    JS_EXPORT_PRIVATE Exception* throwException(JSGlobalObject*, JSObject*);

#if ENABLE(EXCEPTION_SCOPE_VERIFICATION)
    void verifyExceptionCheckNeedIsSatisfied(unsigned depth, ExceptionEventLocation&);
#endif
    
    static void primitiveGigacageDisabledCallback(void*);
    void primitiveGigacageDisabled();

    void callPromiseRejectionCallback(Strong<JSPromise>&);
    void didExhaustMicrotaskQueue();

#if ENABLE(GC_VALIDATION)
    const ClassInfo* m_initializingObjectClass { nullptr };
#endif

    void* m_stackPointerAtVMEntry { nullptr };
    size_t m_currentSoftReservedZoneSize;
    void* m_stackLimit { nullptr };
    void* m_lastStackTop { nullptr };

#if ENABLE(EXCEPTION_SCOPE_VERIFICATION)
    ExceptionScope* m_topExceptionScope { nullptr };
    ExceptionEventLocation m_simulatedThrowPointLocation;
    unsigned m_simulatedThrowPointRecursionDepth { 0 };
    mutable bool m_needExceptionCheck { false };
    std::unique_ptr<StackTrace> m_nativeStackTraceOfLastThrow;
    std::unique_ptr<StackTrace> m_nativeStackTraceOfLastSimulatedThrow;
    RefPtr<Thread> m_throwingThread;
#endif

public:
    SentinelLinkedList<MicrotaskQueue, BasicRawSentinelNode<MicrotaskQueue>> m_microtaskQueues;
private:
    bool m_failNextNewCodeBlock { false };
    bool m_globalConstRedeclarationShouldThrow { true };
    bool m_shouldBuildPCToCodeOriginMapping { false };
    DeletePropertyMode m_deletePropertyMode { DeletePropertyMode::Default };
    HeapAnalyzer* m_activeHeapAnalyzer { nullptr };
    std::unique_ptr<CodeCache> m_codeCache;
    std::unique_ptr<IntlCache> m_intlCache;
    std::unique_ptr<BuiltinExecutables> m_builtinExecutables;
    UncheckedKeyHashMap<RefPtr<UniquedStringImpl>, RefPtr<WatchpointSet>> m_impurePropertyWatchpointSets;
    std::unique_ptr<TypeProfiler> m_typeProfiler;
    std::unique_ptr<TypeProfilerLog> m_typeProfilerLog;
    unsigned m_typeProfilerEnabledCount { 0 };
    bool m_isInService { false };
    Lock m_scratchBufferLock;
    Vector<ScratchBuffer*> m_scratchBuffers;
    size_t m_sizeOfLastScratchBuffer { 0 };
    Vector<std::unique_ptr<CheckpointOSRExitSideState>, expectedMaxActiveSideStateCount> m_checkpointSideState;
    InlineWatchpointSet m_primitiveGigacageEnabled { IsWatched };
    FunctionHasExecutedCache m_functionHasExecutedCache;
    std::unique_ptr<ControlFlowProfiler> m_controlFlowProfiler;
    unsigned m_controlFlowProfilerEnabledCount { 0 };
    MallocPtr<EncodedJSValue, VMMalloc> m_exceptionFuzzBuffer;
    LazyRef<VM, Watchdog> m_watchdog;
    LazyUniqueRef<VM, HeapProfiler> m_heapProfiler;
    LazyUniqueRef<VM, AdaptiveStringSearcherTables> m_stringSearcherTables;
#if ENABLE(SAMPLING_PROFILER)
    const RefPtr<SamplingProfiler> m_samplingProfiler;
#endif
    std::unique_ptr<FuzzerAgent> m_fuzzerAgent;
    LazyUniqueRef<VM, ShadowChicken> m_shadowChicken;
    std::unique_ptr<BytecodeIntrinsicRegistry> m_bytecodeIntrinsicRegistry;
    uint64_t m_drainMicrotaskDelayScopeCount { 0 };

    // FIXME: We should remove handled promises from this list at GC flip. <https://webkit.org/b/201005>
    Vector<Strong<JSPromise>> m_aboutToBeNotifiedRejectedPromises;

    WTF::Function<void(VM&)> m_onEachMicrotaskTick;
    uintptr_t m_currentWeakRefVersion { 0 };

    bool m_hasSideData { false };
    bool m_hasTerminationRequest { false };
    bool m_executionForbidden { false };
    bool m_executionForbiddenOnTermination { false };
    bool m_isDebuggerHookInjected { false };

    Lock m_loopHintExecutionCountLock;
    UncheckedKeyHashMap<const JSInstruction*, std::pair<unsigned, std::unique_ptr<uintptr_t>>> m_loopHintExecutionCounts;

    MicrotaskQueue m_defaultMicrotaskQueue;
    const Ref<Waiter> m_syncWaiter;

    std::atomic<int64_t> m_numberOfActiveJITPlans { 0 };

    Vector<Function<void()>> m_didPopListeners;

#if ENABLE(DFG_DOES_GC_VALIDATION)
    DoesGCCheck m_doesGC;
#endif

    DoublyLinkedList<Debugger> m_debuggers;

    VM* m_prev; // Required by DoublyLinkedListNode.
    VM* m_next; // Required by DoublyLinkedListNode.

    friend class Heap;
    friend class CatchScope; // Friend for exception checking purpose only.
    friend class ExceptionScope; // Friend for exception checking purpose only.
    friend class JSDollarVMHelper;
    friend class LLIntOffsetsExtractor;
    friend class SuspendExceptionScope;
    friend class ThrowScope; // Friend for exception checking purpose only.
    friend class VMTraps;
    friend class WTF::DoublyLinkedListNode<VM>;
};

#if ENABLE(GC_VALIDATION)
inline bool VM::isInitializingObject() const
{
    return !!m_initializingObjectClass;
}

inline void VM::setInitializingObjectClass(const ClassInfo* initializingObjectClass)
{
    m_initializingObjectClass = initializingObjectClass;
}
#endif

inline Heap* WeakSet::heap() const
{
    return &m_vm->heap;
}

#if !ENABLE(C_LOOP)
extern "C" void SYSV_ABI sanitizeStackForVMImpl(VM*);
#endif

JS_EXPORT_PRIVATE void sanitizeStackForVM(VM&);

} // namespace JSC


namespace WTF {

// Unfortunately we have a lot of code that uses JSC::VM without locally
// verifying its lifetime. Safer CPP checker needs to understand JSC::VM's
// lifetime threaded from JSC entrance. Until that, we explicitly suppress
// Ref<VM> lifetime checking by using ThreadSafeRefCountedWithSuppressingSaferCPPChecking.
template<> struct DefaultRefDerefTraits<JSC::VM> {
    static ALWAYS_INLINE JSC::VM* refIfNotNull(JSC::VM* ptr)
    {
        if (ptr) [[likely]]
            ptr->refSuppressingSaferCPPChecking();
        return ptr;
    }

    static ALWAYS_INLINE JSC::VM& ref(JSC::VM& ref)
    {
        ref.refSuppressingSaferCPPChecking();
        return ref;
    }

    static ALWAYS_INLINE void derefIfNotNull(JSC::VM* ptr)
    {
        if (ptr) [[likely]]
            ptr->derefSuppressingSaferCPPChecking();
    }
};

} // namespace WTF


WTF_ALLOW_UNSAFE_BUFFER_USAGE_END
