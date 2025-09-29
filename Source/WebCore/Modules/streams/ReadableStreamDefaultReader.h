/*
 * Copyright (C) 2025 Apple Inc. All rights reserved.
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

#include "InternalReadableStreamDefaultReader.h"
#include <JavaScriptCore/Strong.h>
#include <wtf/RefCountedAndCanMakeWeakPtr.h>
#include <wtf/WeakPtr.h>

namespace WebCore {

class DOMPromise;
class DeferredPromise;
class InternalReadableStreamDefaultReader;
class JSDOMGlobalObject;
class ReadableStream;

class ReadableStreamDefaultReader : public RefCountedAndCanMakeWeakPtr<ReadableStreamDefaultReader> {
public:
    static ExceptionOr<Ref<ReadableStreamDefaultReader>> create(JSDOMGlobalObject&, ReadableStream&);
    static ExceptionOr<Ref<ReadableStreamDefaultReader>> create(JSDOMGlobalObject&, InternalReadableStream&);
    static Ref<ReadableStreamDefaultReader> create(Ref<InternalReadableStreamDefaultReader>&&, Ref<DOMPromise>&&, Ref<DeferredPromise>&&);

    ~ReadableStreamDefaultReader();

    DOMPromise& closedPromise() const;
    void read(JSDOMGlobalObject&, Ref<DeferredPromise>&&);
    ExceptionOr<void> releaseLock(JSDOMGlobalObject&);

    InternalReadableStreamDefaultReader* internalDefaultReader() { return m_internalDefaultReader.get(); }
    size_t getNumReadRequests() const { return m_readRequests.size(); }
    void addReadRequest(Ref<DeferredPromise>&&);
    Ref<DeferredPromise> takeFirstReadRequest();

    void genericCancel(JSDOMGlobalObject&, JSC::JSValue, Ref<DeferredPromise>&&);

    void resolveClosedPromise();
    void rejectClosedPromise(JSC::JSValue);
    void errorReadRequests(JSC::JSValue);

    using ClosedRejectionCallback = Function<void(JSDOMGlobalObject&, JSC::JSValue)>;
    void onClosedPromiseRejection(ClosedRejectionCallback&&);
    void onClosedPromiseResolution(Function<void()>&&);

private:
    ReadableStreamDefaultReader(Ref<InternalReadableStreamDefaultReader>&&, Ref<DOMPromise>&&, Ref<DeferredPromise>&&);

    void setup(JSDOMGlobalObject&);
    void genericRelease(JSDOMGlobalObject&);
    void errorReadRequests(JSDOMGlobalObject&, const Exception&);

    Ref<DOMPromise> m_closedPromise;
    Ref<DeferredPromise> m_closedDeferred;
    RefPtr<ReadableStream> m_stream;
    Deque<Ref<DeferredPromise>> m_readRequests;

    const RefPtr<InternalReadableStreamDefaultReader> m_internalDefaultReader;
    ClosedRejectionCallback m_closedRejectionCallback;
    Function<void()> m_closedResolutionCallback;
};

} // namespace WebCore
