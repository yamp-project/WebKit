/*
 * Copyright (C) 2010 Apple Inc. All rights reserved.
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

#include "APIObject.h"
#include <WebCore/ProcessQualified.h>
#include <wtf/Markable.h>
#include <wtf/Ref.h>
#include <wtf/RefPtr.h>
#include <wtf/WeakPtr.h>
#include <wtf/text/WTFString.h>

namespace WebCore {
class DOMWrapperWorld;
}

namespace WebKit {

struct ContentWorldIdentifierType;
using ContentWorldIdentifier = WebCore::ProcessQualified<ObjectIdentifier<ContentWorldIdentifierType>>;

class InjectedBundleScriptWorld : public API::ObjectImpl<API::Object::Type::BundleScriptWorld>, public CanMakeWeakPtr<InjectedBundleScriptWorld> {
public:
    enum class Type { User, Internal };
    static Ref<InjectedBundleScriptWorld> create(ContentWorldIdentifier, Type = Type::Internal);
    static Ref<InjectedBundleScriptWorld> create(ContentWorldIdentifier, const String& name, Type = Type::Internal);
    static Ref<InjectedBundleScriptWorld> getOrCreate(WebCore::DOMWrapperWorld&);
    static RefPtr<InjectedBundleScriptWorld> get(WebCore::DOMWrapperWorld&);
    static InjectedBundleScriptWorld* find(const String&);
    static InjectedBundleScriptWorld& normalWorldSingleton();

    virtual ~InjectedBundleScriptWorld();

    const WebCore::DOMWrapperWorld& coreWorld() const;
    WebCore::DOMWrapperWorld& coreWorld();
    Ref<const WebCore::DOMWrapperWorld> protectedCoreWorld() const;
    Ref<WebCore::DOMWrapperWorld> protectedCoreWorld();

    void clearWrappers();
    void setAllowAutofill();
    void setAllowElementUserInfo();
    void makeAllShadowRootsOpen();
    void exposeClosedShadowRootsForExtensions();
    void disableOverrideBuiltinsBehavior();
    void setAllowJSHandleCreation();
    void setAllowNodeSerialization();
    void setAllowPostingLegacySynchronousMessages();

    ContentWorldIdentifier identifier() const { return m_identifier; }
    const String& name() const { return m_name; }

private:
    InjectedBundleScriptWorld(ContentWorldIdentifier, WebCore::DOMWrapperWorld&, const String&);

    const ContentWorldIdentifier m_identifier;
    const Ref<WebCore::DOMWrapperWorld> m_world;
    const String m_name;
};

} // namespace WebKit

SPECIALIZE_TYPE_TRAITS_BEGIN(WebKit::InjectedBundleScriptWorld)
static bool isType(const API::Object& object) { return object.type() == API::Object::Type::BundleScriptWorld; }
SPECIALIZE_TYPE_TRAITS_END()
