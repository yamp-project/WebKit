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

#include "config.h"
#include "InjectedBundleScriptWorld.h"

#include "ContentWorldShared.h"
#include <WebCore/DOMWrapperWorld.h>
#include <WebCore/ScriptController.h>
#include <wtf/CheckedPtr.h>
#include <wtf/HashMap.h>
#include <wtf/NeverDestroyed.h>
#include <wtf/text/MakeString.h>
#include <wtf/text/WTFString.h>

namespace WebKit {
using namespace WebCore;

using WorldMap = HashMap<SingleThreadWeakRef<DOMWrapperWorld>, WeakRef<InjectedBundleScriptWorld>>;

static WorldMap& allWorlds()
{
    static NeverDestroyed<WorldMap> map;
    return map;
}

static String uniqueWorldName()
{
    static uint64_t uniqueWorldNameNumber = 0;
    return makeString("UniqueWorld_"_s, uniqueWorldNameNumber++);
}

Ref<InjectedBundleScriptWorld> InjectedBundleScriptWorld::create(ContentWorldIdentifier identifier, Type type)
{
    return InjectedBundleScriptWorld::create(identifier, uniqueWorldName(), type);
}

Ref<InjectedBundleScriptWorld> InjectedBundleScriptWorld::create(ContentWorldIdentifier identifier, const String& name, Type type)
{
    return adoptRef(*new InjectedBundleScriptWorld(identifier, ScriptController::createWorld(name, type == Type::User ? ScriptController::WorldType::User : ScriptController::WorldType::Internal), name));
}

Ref<InjectedBundleScriptWorld> InjectedBundleScriptWorld::getOrCreate(DOMWrapperWorld& world)
{
    if (RefPtr existingWorld = get(world))
        return existingWorld.releaseNonNull();
    return adoptRef(*new InjectedBundleScriptWorld(ContentWorldIdentifier::generate(), world, uniqueWorldName()));
}

RefPtr<InjectedBundleScriptWorld> InjectedBundleScriptWorld::get(WebCore::DOMWrapperWorld& world)
{
    if (&world == &mainThreadNormalWorldSingleton())
        return normalWorldSingleton();

    if (auto existingWorld = allWorlds().get(world))
        return *existingWorld;

    return nullptr;
}

InjectedBundleScriptWorld* InjectedBundleScriptWorld::find(const String& name)
{
    for (auto& world : allWorlds().values()) {
        if (world->name() == name)
            return world.ptr();
    }
    return nullptr;
}

InjectedBundleScriptWorld& InjectedBundleScriptWorld::normalWorldSingleton()
{
    static NeverDestroyed<Ref<InjectedBundleScriptWorld>> world = adoptRef(*new InjectedBundleScriptWorld(pageContentWorldIdentifier(), mainThreadNormalWorldSingleton(), String())).leakRef();
    return world.get();
}

InjectedBundleScriptWorld::InjectedBundleScriptWorld(ContentWorldIdentifier identifier, DOMWrapperWorld& world, const String& name)
    : m_identifier(identifier)
    , m_world(world)
    , m_name(name)
{
    ASSERT(!allWorlds().contains(world));
    allWorlds().add(world, *this);
}

InjectedBundleScriptWorld::~InjectedBundleScriptWorld()
{
    ASSERT(allWorlds().contains(m_world.get()));
    allWorlds().remove(m_world.get());
}

const DOMWrapperWorld& InjectedBundleScriptWorld::coreWorld() const
{
    return m_world;
}

DOMWrapperWorld& InjectedBundleScriptWorld::coreWorld()
{
    return m_world;
}
    
void InjectedBundleScriptWorld::clearWrappers()
{
    m_world->clearWrappers();
}

void InjectedBundleScriptWorld::setAllowAutofill()
{
    m_world->setAllowAutofill();
}

void InjectedBundleScriptWorld::setAllowJSHandleCreation()
{
    m_world->setAllowsJSHandleCreation();
}

void InjectedBundleScriptWorld::setAllowNodeSerialization()
{
    m_world->setAllowNodeSerialization();
}

void InjectedBundleScriptWorld::setAllowPostingLegacySynchronousMessages()
{
    m_world->setAllowPostLegacySynchronousMessage();
}

void InjectedBundleScriptWorld::setAllowElementUserInfo()
{
    m_world->setAllowElementUserInfo();
}

void InjectedBundleScriptWorld::makeAllShadowRootsOpen()
{
    m_world->setShadowRootIsAlwaysOpen();
}

void InjectedBundleScriptWorld::exposeClosedShadowRootsForExtensions()
{
    m_world->setClosedShadowRootIsExposedForExtensions();
}

void InjectedBundleScriptWorld::disableOverrideBuiltinsBehavior()
{
    m_world->disableLegacyOverrideBuiltInsBehavior();
}

Ref<const WebCore::DOMWrapperWorld> InjectedBundleScriptWorld::protectedCoreWorld() const
{
    return m_world;
}

Ref<WebCore::DOMWrapperWorld> InjectedBundleScriptWorld::protectedCoreWorld()
{
    return m_world;
}

} // namespace WebKit
