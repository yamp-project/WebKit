/*
 * Copyright (C) 2016 Apple Inc. All rights reserved.
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

#include <WebCore/ActiveDOMObject.h>
#include <wtf/Threading.h>

namespace WebCore {

class IDBActiveDOMObject : public ActiveDOMObject {
public:
    Thread& originThread() const { return m_originThread.get(); }

    void contextDestroyed() final {
        ASSERT(canCurrentThreadAccessThreadLocalData(originThread()));

        Locker<Lock> lock(m_scriptExecutionContextLock);
        ActiveDOMObject::contextDestroyed();
    }

    template<typename T, typename... Parameters, typename... Arguments>
    inline void performCallbackOnOriginThread(T&, void (T::*)(Parameters...), Arguments&&...);

    inline void callFunctionOnOriginThread(Function<void()>&&);

protected:
    IDBActiveDOMObject(ScriptExecutionContext* context)
        : ActiveDOMObject(context)
    {
        ASSERT(context);
    }

private:
    const Ref<Thread> m_originThread { Thread::currentSingleton() };
    Lock m_scriptExecutionContextLock;
};

} // namespace WebCore
