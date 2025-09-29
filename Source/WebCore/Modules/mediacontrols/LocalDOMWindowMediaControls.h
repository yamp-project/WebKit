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

#if ENABLE(VIDEO)

#include "LocalDOMWindowProperty.h"
#include "Supplementable.h"
#include <wtf/TZoneMalloc.h>
#include <wtf/TypeCasts.h>

namespace WebCore {

class DOMWindow;
class LocalDOMWindow;
class MediaControlsUtils;

class LocalDOMWindowMediaControls : public Supplement<LocalDOMWindow>, public LocalDOMWindowProperty {
    WTF_MAKE_TZONE_ALLOCATED_EXPORT(LocalDOMWindowMediaControls, WEBCORE_EXPORT);
public:
    explicit LocalDOMWindowMediaControls(DOMWindow&);
    virtual ~LocalDOMWindowMediaControls();

    static LocalDOMWindowMediaControls* from(DOMWindow&);

    static RefPtr<MediaControlsUtils> utils(Document&, DOMWindow&);

private:
    bool isLocalDOMWindowMediaControls() const { return true; }
    static ASCIILiteral supplementName();

    Ref<MediaControlsUtils> ensureUtils(Document&);

    RefPtr<MediaControlsUtils> m_utils;
};

} // namespace WebCore

SPECIALIZE_TYPE_TRAITS_BEGIN(WebCore::LocalDOMWindowMediaControls)
    static bool isType(const WebCore::SupplementBase& supplement) { return supplement.isLocalDOMWindowMediaControls(); }
SPECIALIZE_TYPE_TRAITS_END()


#endif // ENABLE(VIDEO)
