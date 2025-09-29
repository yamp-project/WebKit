/*
 * Copyright (C) 2017-2024 Apple Inc. All rights reserved.
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

#if ENABLE(WEBASSEMBLY)

#include <JavaScriptCore/MacroAssemblerCodeRef.h>
#include <JavaScriptCore/MemoryMode.h>
#include <JavaScriptCore/WasmCallee.h>
#include <JavaScriptCore/WasmJS.h>
#include <wtf/CrossThreadCopier.h>
#include <wtf/FixedBitVector.h>
#include <wtf/FixedVector.h>
#include <wtf/Lock.h>
#include <wtf/RefPtr.h>
#include <wtf/SharedTask.h>
#include <wtf/ThreadSafeRefCounted.h>
#include <wtf/text/WTFString.h>

namespace JSC {

class VM;

namespace Wasm {

class EntryPlan;
struct ModuleInformation;
struct UnlinkedWasmToWasmCall;

class CalleeGroup final : public ThreadSafeRefCounted<CalleeGroup> {
public:
    typedef void CallbackType(Ref<CalleeGroup>&&, bool);
    using AsyncCompilationCallback = RefPtr<WTF::SharedTask<CallbackType>>;

    struct OptimizedCallees {
#if ENABLE(WEBASSEMBLY_BBQJIT)
        mutable Lock m_bbqCalleeLock;
        ThreadSafeWeakOrStrongPtr<BBQCallee> m_bbqCallee WTF_GUARDED_BY_LOCK(m_bbqCalleeLock);
#endif
#if ENABLE(WEBASSEMBLY_OMGJIT)
        RefPtr<OMGCallee> m_omgCallee;
#endif
    };


    static Ref<CalleeGroup> createFromIPInt(VM&, MemoryMode, ModuleInformation&, Ref<IPIntCallees>&&);
    static Ref<CalleeGroup> createFromExisting(MemoryMode, const CalleeGroup&);

    void waitUntilFinished();
    void compileAsync(VM&, AsyncCompilationCallback&&);

    bool compilationFinished()
    {
        return m_compilationFinished.load();
    }
    bool runnable() { return compilationFinished() && !m_errorMessage; }

    // Note, we do this copy to ensure it's thread safe to have this
    // called from multiple threads simultaneously.
    String errorMessage()
    {
        ASSERT(!runnable());
        return crossThreadCopy(m_errorMessage);
    }

    unsigned functionImportCount() const { return m_wasmToWasmExitStubs.size(); }
    FunctionSpaceIndex toSpaceIndex(FunctionCodeIndex codeIndex) const
    {
        ASSERT(codeIndex < m_calleeCount);
        return FunctionSpaceIndex(codeIndex + functionImportCount());
    }
    FunctionCodeIndex toCodeIndex(FunctionSpaceIndex spaceIndex) const
    {
        ASSERT(functionImportCount() <= spaceIndex);
        ASSERT(spaceIndex < m_calleeCount + functionImportCount());
        return FunctionCodeIndex(spaceIndex - functionImportCount());
    }

    // These two callee getters are only valid once the callees have been populated.

    JSToWasmCallee& jsToWasmCalleeFromFunctionIndexSpace(FunctionSpaceIndex functionIndexSpace)
    {
        ASSERT(runnable());
        ASSERT(functionIndexSpace >= functionImportCount());
        unsigned calleeIndex = functionIndexSpace - functionImportCount();

        auto callee = m_jsToWasmCallees.get(calleeIndex);
        RELEASE_ASSERT(callee);
        return *callee;
    }

    RefPtr<JITCallee> replacement(const AbstractLocker& locker, FunctionSpaceIndex functionIndexSpace) WTF_REQUIRES_LOCK(m_lock)
    {
        ASSERT(runnable());
        ASSERT(functionIndexSpace >= functionImportCount());
        if (auto* tuple = optimizedCalleesTuple(locker, toCodeIndex(functionIndexSpace))) {
            UNUSED_VARIABLE(tuple);
#if ENABLE(WEBASSEMBLY_OMGJIT)
            if (RefPtr callee = tuple->m_omgCallee)
                return callee;
#endif
#if ENABLE(WEBASSEMBLY_BBQJIT)
            {
                Locker locker { tuple->m_bbqCalleeLock };
                if (RefPtr callee = tuple->m_bbqCallee.get())
                    return callee;
            }
#endif
        }
        return nullptr;
    }

    RefPtr<JITCallee> tryGetReplacementConcurrently(FunctionCodeIndex functionIndex) const WTF_IGNORES_THREAD_SAFETY_ANALYSIS;
#if ENABLE(WEBASSEMBLY_BBQJIT)
    RefPtr<BBQCallee> tryGetBBQCalleeForLoopOSRConcurrently(VM&, FunctionCodeIndex) WTF_IGNORES_THREAD_SAFETY_ANALYSIS;
#endif
#if ENABLE(WEBASSEMBLY_OMGJIT)
    RefPtr<OMGCallee> tryGetOMGCalleeConcurrently(FunctionCodeIndex) WTF_IGNORES_THREAD_SAFETY_ANALYSIS;
#endif

    Ref<Callee> wasmEntrypointCalleeFromFunctionIndexSpace(const AbstractLocker& locker, FunctionSpaceIndex functionIndexSpace) WTF_REQUIRES_LOCK(m_lock)
    {

        if (RefPtr replacement = this->replacement(locker, functionIndexSpace))
            return replacement.releaseNonNull();
        unsigned calleeIndex = functionIndexSpace - functionImportCount();
        return m_ipintCallees->at(calleeIndex).get();
    }

    Ref<IPIntCallee> ipintCalleeFromFunctionIndexSpace(FunctionSpaceIndex functionIndexSpace) const
    {
        ASSERT(functionIndexSpace >= functionImportCount());
        unsigned calleeIndex = functionIndexSpace - functionImportCount();
        return m_ipintCallees->at(calleeIndex).get();
    }

#if ENABLE(WEBASSEMBLY_BBQJIT) || ENABLE(WEBASSEMBLY_OMGJIT)
    void installOptimizedCallee(const AbstractLocker&, const ModuleInformation&, FunctionCodeIndex, Ref<OptimizingJITCallee>&&, const FixedBitVector& outgoingJITDirectCallees) WTF_REQUIRES_LOCK(m_lock);
#endif

#if ENABLE(WEBASSEMBLY_BBQJIT)
    RefPtr<BBQCallee> bbqCallee(const AbstractLocker& locker, FunctionCodeIndex functionIndex) WTF_REQUIRES_LOCK(m_lock)
    {
        if (auto* tuple = optimizedCalleesTuple(locker, functionIndex)) {
            Locker locker { tuple->m_bbqCalleeLock };
            return tuple->m_bbqCallee.get();
        }
        return nullptr;
    }


    void releaseBBQCallee(const AbstractLocker&, FunctionCodeIndex) WTF_REQUIRES_LOCK(m_lock);
#endif

#if ENABLE(WEBASSEMBLY_OMGJIT)
    OMGCallee* omgCallee(const AbstractLocker& locker, FunctionCodeIndex functionIndex) WTF_REQUIRES_LOCK(m_lock)
    {
        if (auto* tuple = optimizedCalleesTuple(locker, functionIndex))
            return tuple->m_omgCallee.get();
        return nullptr;
    }

    void recordOMGOSREntryCallee(const AbstractLocker&, FunctionCodeIndex functionIndex, OMGOSREntryCallee& callee) WTF_REQUIRES_LOCK(m_lock)
    {
        auto result = m_osrEntryCallees.add(functionIndex, callee);
        ASSERT_UNUSED(result, result.isNewEntry);
    }
#endif

    CodePtr<WasmEntryPtrTag>* entrypointLoadLocationFromFunctionIndexSpace(FunctionSpaceIndex functionIndexSpace)
    {
        RELEASE_ASSERT(functionIndexSpace >= functionImportCount());
        unsigned calleeIndex = functionIndexSpace - functionImportCount();
        return &m_wasmIndirectCallEntrypoints[calleeIndex];
    }

    RefPtr<Wasm::IPIntCallee> wasmCalleeFromFunctionIndexSpace(FunctionSpaceIndex functionIndexSpace)
    {
        RELEASE_ASSERT(functionIndexSpace >= functionImportCount());
        unsigned calleeIndex = functionIndexSpace - functionImportCount();
        return m_wasmIndirectCallWasmCallees[calleeIndex];
    }

    CodePtr<WasmEntryPtrTag> wasmToWasmExitStub(FunctionSpaceIndex functionIndex)
    {
        return m_wasmToWasmExitStubs[functionIndex].code();
    }

    bool isSafeToRun(MemoryMode);

    MemoryMode mode() const { return m_mode; }

    // TriState::Indeterminate means weakly referenced.
    TriState calleeIsReferenced(const AbstractLocker&, Wasm::Callee*) const WTF_REQUIRES_LOCK(m_lock);

    ~CalleeGroup();
private:
    friend class Plan;
#if ENABLE(WEBASSEMBLY_BBQJIT)
    friend class BBQPlan;
#endif
#if ENABLE(WEBASSEMBLY_OMGJIT)
    friend class OMGPlan;
    friend class OSREntryPlan;
#endif

    CalleeGroup(VM&, MemoryMode, ModuleInformation&, Ref<IPIntCallees>&&);
    CalleeGroup(MemoryMode, const CalleeGroup&);
    void setCompilationFinished();

    OptimizedCallees* optimizedCalleesTuple(const AbstractLocker&, FunctionCodeIndex index) WTF_REQUIRES_LOCK(m_lock)
    {
        if (m_currentlyInstallingOptimizedCalleesIndex == index)
            return &m_currentlyInstallingOptimizedCallees;
        if (m_optimizedCallees.isEmpty())
            return nullptr;
        return &m_optimizedCallees[index];
    }

    const OptimizedCallees* optimizedCalleesTuple(const AbstractLocker&, FunctionCodeIndex index) const WTF_REQUIRES_LOCK(m_lock)
    {
        if (m_currentlyInstallingOptimizedCalleesIndex == index)
            return &m_currentlyInstallingOptimizedCallees;
        if (m_optimizedCallees.isEmpty())
            return nullptr;
        return &m_optimizedCallees[index];
    }

    void ensureOptimizedCalleesSlow(const AbstractLocker&) WTF_REQUIRES_LOCK(m_lock);

#if ENABLE(WEBASSEMBLY_OMGJIT) || ENABLE(WEBASSEMBLY_BBQJIT)
    void startInstallingCallee(const AbstractLocker&, FunctionCodeIndex, OptimizingJITCallee&) WTF_REQUIRES_LOCK(m_lock);
    void finalizeInstallingCallee(const AbstractLocker&, FunctionCodeIndex) WTF_REQUIRES_LOCK(m_lock);
    void updateCallsitesToCallUs(const AbstractLocker&, CodeLocationLabel<WasmEntryPtrTag> entrypoint, FunctionCodeIndex functionIndex) WTF_REQUIRES_LOCK(m_lock);
    void reportCallees(const AbstractLocker&, JITCallee* caller, const FixedBitVector& callees) WTF_REQUIRES_LOCK(m_lock);
#endif

    unsigned m_calleeCount;
    MemoryMode m_mode;

    FunctionCodeIndex m_currentlyInstallingOptimizedCalleesIndex WTF_GUARDED_BY_LOCK(m_lock) { };
    OptimizedCallees m_currentlyInstallingOptimizedCallees WTF_GUARDED_BY_LOCK(m_lock) { };
    FixedVector<OptimizedCallees> m_optimizedCallees WTF_GUARDED_BY_LOCK(m_lock);
    const Ref<IPIntCallees> m_ipintCallees;
    UncheckedKeyHashMap<uint32_t, RefPtr<JSToWasmCallee>, DefaultHash<uint32_t>, WTF::UnsignedWithZeroKeyHashTraits<uint32_t>> m_jsToWasmCallees;
#if ENABLE(WEBASSEMBLY_BBQJIT) || ENABLE(WEBASSEMBLY_OMGJIT)
    // FIXME: We should probably find some way to prune dead entries periodically.
    UncheckedKeyHashMap<uint32_t, ThreadSafeWeakPtr<OMGOSREntryCallee>, DefaultHash<uint32_t>, WTF::UnsignedWithZeroKeyHashTraits<uint32_t>> m_osrEntryCallees WTF_GUARDED_BY_LOCK(m_lock);
#endif

    // functionCodeIndex -> functionCodeIndex of internal functions that have direct JIT callsites to the lhs.
    // Note, this can grow over time since OMG inlining can add to the set of callers and we'll tranisition from
    // a sparse adjacency matrix to a bit vector based one if that's more space efficient.
    // FIXME: This should be a WTF class and we should use it in the JIT Plans.
    using SparseCallers = UncheckedKeyHashSet<uint32_t, DefaultHash<uint32_t>, WTF::UnsignedWithZeroKeyHashTraits<uint32_t>>;
    using DenseCallers = BitVector;
    FixedVector<Variant<SparseCallers, DenseCallers>> m_callers WTF_GUARDED_BY_LOCK(m_lock);
    FixedVector<CodePtr<WasmEntryPtrTag>> m_wasmIndirectCallEntrypoints;
    FixedVector<RefPtr<Wasm::IPIntCallee>> m_wasmIndirectCallWasmCallees;
    FixedVector<MacroAssemblerCodeRef<WasmEntryPtrTag>> m_wasmToWasmExitStubs;
    RefPtr<EntryPlan> m_plan;
    std::atomic<bool> m_compilationFinished { false };
    String m_errorMessage;
public:
    Lock m_lock;
};

} } // namespace JSC::Wasm

#endif // ENABLE(WEBASSEMBLY)
