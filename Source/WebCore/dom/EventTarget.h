/*
 * Copyright (C) 1999 Lars Knoll (knoll@kde.org)
 *           (C) 1999 Antti Koivisto (koivisto@kde.org)
 *           (C) 2001 Dirk Mueller (mueller@kde.org)
 * Copyright (C) 2004-2021 Apple Inc. All rights reserved.
 * Copyright (C) 2006 Alexey Proskuryakov (ap@webkit.org)
 *           (C) 2007, 2008 Nikolas Zimmermann <zimmermann@kde.org>
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

#include <WebCore/EventListenerMap.h>
#include <WebCore/EventListenerOptions.h>
#include <WebCore/PlatformExportMacros.h>
#include <WebCore/ScriptWrappable.h>
#include <bmalloc/TZoneHeap.h>
#include <memory>
#include <wtf/CanMakeWeakPtr.h>
#include <wtf/CheckedPtr.h>
#include <wtf/EnumTraits.h>
#include <wtf/Forward.h>
#include <wtf/TZoneMalloc.h>
#include <wtf/Variant.h>
#include <wtf/WeakPtr.h>
#include <wtf/WeakPtrFactory.h>
#include <wtf/WeakPtrImpl.h>

namespace JSC {
class JSValue;
class JSObject;
}

namespace WebCore {

enum class EventTargetInterfaceType : uint8_t;
class DOMWrapperWorld;
class EventTarget;
class JSEventListener;
struct AddEventListenerOptions;
template<typename> class ExceptionOr;

struct EventTargetData {
    WTF_MAKE_TZONE_ALLOCATED(EventTargetData);
    WTF_MAKE_NONCOPYABLE(EventTargetData);
public:
    EventTargetData() = default;

    void clear()
    {
        eventListenerMap.clearEntriesForTearDown();
    }

    EventListenerMap eventListenerMap;
};

// Do not make WeakPtrImplWithEventTargetData a derived class of DefaultWeakPtrImpl to catch the bug which uses incorrect impl class.
class WeakPtrImplWithEventTargetData final : public WTF::WeakPtrImplBase<WeakPtrImplWithEventTargetData> {
public:
    EventTargetData& eventTargetData() { return m_eventTargetData; }
    const EventTargetData& eventTargetData() const { return m_eventTargetData; }

    template<typename T> WeakPtrImplWithEventTargetData(T* ptr) : WTF::WeakPtrImplBase<WeakPtrImplWithEventTargetData>(ptr) { }

private:
    EventTargetData m_eventTargetData;
};

class WEBCORE_EXPORT EventTarget : public ScriptWrappable, public CanMakeWeakPtrWithBitField<EventTarget, WeakPtrFactoryInitialization::Lazy, WeakPtrImplWithEventTargetData> {
    WTF_MAKE_TZONE_OR_ISO_ALLOCATED(EventTarget);
public:
    static Ref<EventTarget> create(ScriptExecutionContext&);

    inline void ref(); // Defined in EventTargetInlines.h.
    inline void deref(); // Defined in EventTargetInlines.h.

    virtual enum EventTargetInterfaceType eventTargetInterface() const = 0;
    virtual ScriptExecutionContext* scriptExecutionContext() const = 0;

    virtual bool isPaymentRequest() const;

    using AddEventListenerOptionsOrBoolean = Variant<AddEventListenerOptions, bool>;
    void addEventListenerForBindings(const AtomString& eventType, RefPtr<EventListener>&&, AddEventListenerOptionsOrBoolean&&);
    using EventListenerOptionsOrBoolean = Variant<EventListenerOptions, bool>;
    void removeEventListenerForBindings(const AtomString& eventType, RefPtr<EventListener>&&, EventListenerOptionsOrBoolean&&);
    ExceptionOr<bool> dispatchEventForBindings(Event&);

    virtual bool addEventListener(const AtomString& eventType, Ref<EventListener>&&, const AddEventListenerOptions&);
    virtual bool removeEventListener(const AtomString& eventType, EventListener&, const EventListenerOptions& = { });

    virtual void removeAllEventListeners();
    virtual void dispatchEvent(Event&);
    virtual void uncaughtExceptionInEventHandler();

    static const AtomString& legacyTypeForEvent(const Event&);

    // Used for legacy "onevent" attributes.
    template<typename JSMaybeErrorEventListener>
    void setAttributeEventListener(const AtomString& eventType, JSC::JSValue listener, JSC::JSObject& jsEventTarget);
    bool setAttributeEventListener(const AtomString& eventType, RefPtr<EventListener>&&, DOMWrapperWorld&);
    RefPtr<JSEventListener> attributeEventListener(const AtomString& eventType, DOMWrapperWorld&);

    bool hasEventListeners() const;
    bool hasEventListeners(const AtomString& eventType) const;
    bool hasAnyEventListeners(Vector<AtomString> eventTypes) const;
    bool hasCapturingEventListeners(const AtomString& eventType);
    bool hasActiveEventListeners(const AtomString& eventType) const;

    Vector<AtomString> eventTypes() const;
    const EventListenerVector& eventListeners(const AtomString& eventType);

    enum class EventInvokePhase { Capturing, Bubbling };
    void fireEventListeners(Event&, EventInvokePhase);

    template<typename Visitor>
    inline void visitJSEventListeners(Visitor&);
    void invalidateJSEventListeners(JSC::JSObject*);

    inline const EventTargetData* eventTargetData() const;
    inline EventTargetData* eventTargetData();
    inline EventTargetData* eventTargetDataConcurrently();

    template<typename CallbackType>
    inline void enumerateEventListenerTypes(NOESCAPE const CallbackType&) const;

    template<typename CallbackType>
    inline bool containsMatchingEventListener(NOESCAPE const CallbackType&) const;

    bool hasEventTargetData() const { return hasEventTargetFlag(EventTargetFlag::HasEventTargetData); }
    bool isNode() const { return hasEventTargetFlag(EventTargetFlag::IsNode); }

    bool isInGCReacheableRefMap() const { return hasEventTargetFlag(EventTargetFlag::IsInGCReachableRefMap); }
    void setIsInGCReacheableRefMap(bool flag) { setEventTargetFlag(EventTargetFlag::IsInGCReachableRefMap, flag); }

    bool hasValidQuerySelectorAllResults() const { return hasEventTargetFlag(EventTargetFlag::HasValidQuerySelectorAllResults); }
    void setHasValidQuerySelectorAllResults(bool flag) { setEventTargetFlag(EventTargetFlag::HasValidQuerySelectorAllResults, flag); }

protected:
    enum ConstructNodeTag { ConstructNode };
    EventTarget() = default;
    EventTarget(ConstructNodeTag)
    {
        setEventTargetFlag(EventTargetFlag::IsNode, true);
    }

    virtual ~EventTarget();

    // Flags for ownership & relationship.
    enum class EventTargetFlag : uint16_t {
        HasEventTargetData = 1 << 0,
        IsNode = 1 << 1,
        IsInGCReachableRefMap = 1 << 2,
        // Node bits
        IsConnected = 1 << 3,
        IsInShadowTree = 1 << 4,
        HasBeenInUserAgentShadowTree = 1 << 5,
        HasValidQuerySelectorAllResults = 1 << 6,
        // Element bits
        HasSyntheticAttrChildNodes = 1 << 7,
        HasDuplicateAttribute = 1 << 8,
        HasLangAttr = 1 << 9,
        HasXMLLangAttr = 1 << 10,
        HasFormAssociatedCustomElementInterface = 1 << 11,
        HasShadowRootContainingSlots = 1 << 12,
        IsInTopLayer = 1 << 13,
        // 1-bit free
        // SVGElement bits
        HasPendingResources = 1 << 15,
    };

    EventTargetData& ensureEventTargetData();

    virtual void eventListenersDidChange() { }

    bool hasEventTargetFlag(EventTargetFlag flag) const { return weakPtrFactory().bitfield() & enumToUnderlyingType(flag); }
    void setEventTargetFlag(EventTargetFlag, bool = true);
    void clearEventTargetFlag(EventTargetFlag flag) { setEventTargetFlag(flag, false); }

private:
    virtual void refEventTarget() = 0;
    virtual void derefEventTarget() = 0;

    void innerInvokeEventListeners(Event&, EventListenerVector, EventInvokePhase);
};

inline void EventTarget::setEventTargetFlag(EventTargetFlag flag, bool value)
{
    auto flags = OptionSet<EventTargetFlag>::fromRaw(weakPtrFactory().bitfield());
    flags.set(flag, value);
    weakPtrFactory().setBitfield(flags.toRaw());
}

} // namespace WebCore
