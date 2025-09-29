/*
 * Copyright (C) 2024 Apple Inc. All rights reserved.
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

#include "MessageReceiver.h"
#include <WebCore/PageIdentifier.h>
#include <wtf/RefCounted.h>
#include <wtf/TZoneMalloc.h>

namespace IPC {
class Connection;
class Decoder;
}

namespace WebKit {

class WebPage;

class WebPageTesting : public IPC::MessageReceiver, public RefCounted<WebPageTesting> {
    WTF_MAKE_TZONE_ALLOCATED(WebPageTesting);
    WTF_MAKE_NONCOPYABLE(WebPageTesting);
public:
    static Ref<WebPageTesting> create(WebPage&);
    virtual ~WebPageTesting();

    void ref() const final { RefCounted::ref(); }
    void deref() const final { RefCounted::deref(); }

private:
    explicit WebPageTesting(WebPage&);

    void didReceiveMessage(IPC::Connection&, IPC::Decoder&) override;
    void didReceiveSyncMessage(IPC::Connection&, IPC::Decoder&, UniqueRef<IPC::Encoder>&) override;

    void isLayerTreeFrozen(CompletionHandler<void(bool)>&&);
    void setPermissionLevel(const String& origin, bool allowed);
    void isEditingCommandEnabled(const String& commandName, CompletionHandler<void(bool)>&&);
    void resetStateBetweenTests();
    void clearCachedBackForwardListCounts(CompletionHandler<void()>&&);
    void setTracksRepaints(bool, CompletionHandler<void()>&&);
    void displayAndTrackRepaints(CompletionHandler<void()>&&);

#if ENABLE(NOTIFICATIONS)
    void clearNotificationPermissionState();
#endif

    void setObscuredContentInsets(float top, float right, float bottom, float left, CompletionHandler<void()>&&);

    void clearWheelEventTestMonitor();

    WeakPtr<WebPage> m_page;
    WebCore::PageIdentifier m_pageIdentifier;
};

} // namespace WebKit
