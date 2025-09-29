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

#if ENABLE(GPU_PROCESS)

#include "DDModelIdentifier.h"
#include "ModelConvertFromBackingContext.h"
#include "ScopedActiveMessageReceiveQueue.h"
#include <WebCore/WebGPU.h>
#include <functional>
#include <wtf/HashMap.h>
#include <wtf/Ref.h>
#include <wtf/RefCountedAndCanMakeWeakPtr.h>
#include <wtf/TZoneMalloc.h>

namespace WebCore::DDModel {
class DDMesh;
}

namespace WebKit {
class RemoteDDMesh;
}

namespace WebKit::DDModel {

class ObjectHeap final : public RefCountedAndCanMakeWeakPtr<ObjectHeap>, public ConvertFromBackingContext {
    WTF_MAKE_TZONE_ALLOCATED(ObjectHeap);
public:
    static Ref<ObjectHeap> create()
    {
        return adoptRef(*new ObjectHeap());
    }

    ~ObjectHeap();

    void addObject(DDModelIdentifier, RemoteDDMesh&);

    void removeObject(DDModelIdentifier);

    void clear();

    WeakPtr<WebCore::DDModel::DDMesh> convertDDMeshFromBacking(DDModelIdentifier) final;

    struct ExistsAndValid {
        bool exists { false };
        bool valid { false };
    };
    ExistsAndValid objectExistsAndValid(const WebCore::WebGPU::GPU&, DDModelIdentifier) const;
private:
    ObjectHeap();

    using Object = Variant<
        std::monostate,
        IPC::ScopedActiveMessageReceiveQueue<RemoteDDMesh>
    >;

    HashMap<DDModelIdentifier, Object> m_objects;
};

}

#endif // ENABLE(GPU_PROCESS)
