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

#include "config.h"
#include "APIJSHandle.h"

#include "WebProcessMessages.h"
#include "WebProcessProxy.h"

namespace API {

using HandleMap = HashMap<WebCore::JSHandleIdentifier, JSHandle*>;
static HandleMap& handleMap()
{
    static MainRunLoopNeverDestroyed<HandleMap> map;
    return map.get();
}

Ref<JSHandle> JSHandle::getOrCreate(WebKit::JSHandleInfo&& info)
{
    if (RefPtr existingHandle = handleMap().get(info.identifier))
        return existingHandle.releaseNonNull();
    return adoptRef(*new JSHandle(WTFMove(info)));
}

JSHandle::JSHandle(WebKit::JSHandleInfo&& info)
    : m_info(WTFMove(info))
{
    handleMap().add(m_info.identifier, this);
}

JSHandle::~JSHandle()
{
    ASSERT(handleMap().get(m_info.identifier) == this);
    handleMap().remove(m_info.identifier);
    if (RefPtr webProcess = WebKit::WebProcessProxy::processForIdentifier(m_info.identifier.processIdentifier()))
        webProcess->send(Messages::WebProcess::JSHandleDestroyed(m_info.identifier), 0);
}

} // namespace API
