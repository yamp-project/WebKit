/*
 *  Copyright (C) 1999-2001 Harri Porten (porten@kde.org)
 *  Copyright (C) 2003, 2004, 2005, 2006, 2008, 2009 Apple Inc. All rights reserved.
 *  Copyright (C) 2007 Samuel Weinig <sam@webkit.org>
 *  Copyright (C) 2009 Google, Inc. All rights reserved.
 *
 *  This library is free software; you can redistribute it and/or
 *  modify it under the terms of the GNU Lesser General Public
 *  License as published by the Free Software Foundation; either
 *  version 2 of the License, or (at your option) any later version.
 *
 *  This library is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 *  Lesser General Public License for more details.
 *
 *  You should have received a copy of the GNU Lesser General Public
 *  License along with this library; if not, write to the Free Software
 *  Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
 */

#pragma once

#include <WebCore/JSDOMGlobalObject.h>
#include <wtf/Compiler.h>
#include <wtf/Forward.h>
#include <wtf/WeakPtr.h>

namespace WebCore {

class WindowProxy;

typedef HashMap<void*, JSC::Weak<JSC::JSObject>> DOMObjectWrapperMap;

class DOMWrapperWorld : public RefCounted<DOMWrapperWorld>, public CanMakeSingleThreadWeakPtr<DOMWrapperWorld> {
public:
    enum class Type {
        Normal,   // Main (e.g. Page)
        User,     // User Scripts (e.g. Extensions)
        Internal, // WebKit Internal (e.g. Media Controls)
    };

    static Ref<DOMWrapperWorld> create(JSC::VM& vm, Type type = Type::Internal, const String& name = { })
    {
        return adoptRef(*new DOMWrapperWorld(vm, type, name));
    }
    WEBCORE_EXPORT ~DOMWrapperWorld();

    // Free as much memory held onto by this world as possible.
    WEBCORE_EXPORT void clearWrappers();

    void didCreateWindowProxy(WindowProxy* controller) { m_jsWindowProxies.add(controller); }
    void didDestroyWindowProxy(WindowProxy* controller) { m_jsWindowProxies.remove(controller); }

    void setAllowAutofill() { m_allowAutofill = true; }
    bool allowAutofill() const { return m_allowAutofill; }

    bool allowsJSHandleCreation() const { return m_allowsJSHandleCreation; }
    void setAllowsJSHandleCreation() { m_allowsJSHandleCreation = true; }

    void setAllowNodeSerialization() { m_allowNodeSerialization = true; }
    bool allowNodeSerialization() const { return m_allowNodeSerialization; }

    void setAllowElementUserInfo() { m_allowElementUserInfo = true; }
    bool allowElementUserInfo() const { return m_allowElementUserInfo; }

    bool canAccessAnyShadowRoot() const { return shadowRootIsAlwaysOpen() || closedShadowRootIsExposedForExtensions(); }

    void setShadowRootIsAlwaysOpen() { m_shadowRootIsAlwaysOpen = true; }
    bool shadowRootIsAlwaysOpen() const { return m_shadowRootIsAlwaysOpen; }

    void setClosedShadowRootIsExposedForExtensions() { m_closedShadowRootIsExposedForExtensions = true; }
    bool closedShadowRootIsExposedForExtensions() const { return m_closedShadowRootIsExposedForExtensions; }

    void disableLegacyOverrideBuiltInsBehavior() { m_shouldDisableLegacyOverrideBuiltInsBehavior = true; }
    bool shouldDisableLegacyOverrideBuiltInsBehavior() const { return m_shouldDisableLegacyOverrideBuiltInsBehavior; }

    void setAllowPostLegacySynchronousMessage() { m_allowPostLegacySynchronousMessage = true; }
    bool allowPostLegacySynchronousMessage() const { return m_allowPostLegacySynchronousMessage; }

    void setIsMediaControls() { m_isMediaControls = true; }
    bool isMediaControls() const { return m_isMediaControls; }

    DOMObjectWrapperMap& wrappers() { return m_wrappers; }

    Type type() const { return m_type; }
    bool isNormal() const { return m_type == Type::Normal; }
    bool isUser() const { return m_type == Type::User; }

    const String& name() const { return m_name; }

    JSC::VM& vm() const { return m_vm; }

protected:
    DOMWrapperWorld(JSC::VM&, Type, const String& name);

private:
    JSC::VM& m_vm;
    HashSet<WindowProxy*> m_jsWindowProxies;
    DOMObjectWrapperMap m_wrappers;

    String m_name;
    Type m_type { Type::Internal };

    bool m_allowAutofill : 1 { false };
    bool m_allowElementUserInfo : 1 { false };
    bool m_shadowRootIsAlwaysOpen : 1 { false };
    bool m_closedShadowRootIsExposedForExtensions : 1 { false };
    bool m_shouldDisableLegacyOverrideBuiltInsBehavior : 1 { false };
    bool m_allowsJSHandleCreation : 1 { false };
    bool m_allowNodeSerialization : 1 { false };
    bool m_allowPostLegacySynchronousMessage : 1 { false };
    bool m_isMediaControls : 1 { false };
};

DOMWrapperWorld& normalWorld(JSC::VM&);
WEBCORE_EXPORT DOMWrapperWorld& mainThreadNormalWorldSingleton();
inline Ref<DOMWrapperWorld> protectedMainThreadNormalWorld() { return mainThreadNormalWorldSingleton(); }

inline DOMWrapperWorld& debuggerWorldSingleton() { return mainThreadNormalWorldSingleton(); }
inline DOMWrapperWorld& pluginWorldSingleton() { return mainThreadNormalWorldSingleton(); }

DOMWrapperWorld& currentWorld(JSC::JSGlobalObject&);
DOMWrapperWorld& worldForDOMObject(JSC::JSObject&);
Ref<DOMWrapperWorld> protectedWorldForDOMObject(JSC::JSObject&);

// Helper function for code paths that must not share objects across isolated DOM worlds.
bool isWorldCompatible(JSC::JSGlobalObject&, JSC::JSValue);

inline DOMWrapperWorld& currentWorld(JSC::JSGlobalObject& lexicalGlobalObject)
{
    return JSC::jsCast<JSDOMGlobalObject*>(&lexicalGlobalObject)->world();
}

inline DOMWrapperWorld& worldForDOMObject(JSC::JSObject& object)
{
    return JSC::jsCast<JSDOMGlobalObject*>(object.globalObject())->world();
}

inline Ref<DOMWrapperWorld> protectedWorldForDOMObject(JSC::JSObject& object)
{
    return worldForDOMObject(object);
}

inline bool isWorldCompatible(JSC::JSGlobalObject& lexicalGlobalObject, JSC::JSValue value)
{
    return !value.isObject() || &worldForDOMObject(*value.getObject()) == &currentWorld(lexicalGlobalObject);
}

} // namespace WebCore
