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

#include "config.h"
#include "UIGamepadProvider.h"

#if ENABLE(GAMEPAD)

#include "APIPageConfiguration.h"
#include "GamepadData.h"
#include "UIGamepad.h"
#include "WebPageProxy.h"
#include "WebProcessPool.h"
#include <WebCore/MockGamepadProvider.h>
#include <WebCore/PlatformGamepad.h>
#include <wtf/NeverDestroyed.h>

namespace WebKit {
using namespace WebCore;

static const Seconds maximumGamepadUpdateInterval { 1_s / 120. };

UIGamepadProvider& UIGamepadProvider::singleton()
{
    static NeverDestroyed<UIGamepadProvider> sharedProvider;
    return sharedProvider;
}

UIGamepadProvider::UIGamepadProvider()
    : m_gamepadSyncTimer(RunLoop::mainSingleton(), "UIGamepadProvider::GamepadSyncTimer"_s, this, &UIGamepadProvider::gamepadSyncTimerFired)
{
    platformSetDefaultGamepadProvider();
}

UIGamepadProvider::~UIGamepadProvider()
{
    if (!m_processPoolsUsingGamepads.isEmptyIgnoringNullReferences())
        GamepadProvider::singleton().stopMonitoringGamepads(*this);
}

void UIGamepadProvider::gamepadSyncTimerFired()
{
    RefPtr webPageProxy = platformWebPageProxyForGamepadInput();
    if (!webPageProxy || !m_processPoolsUsingGamepads.contains(webPageProxy->configuration().processPool()))
        return;

    webPageProxy->gamepadActivity(snapshotGamepads(), m_shouldMakeGamepadsVisibleOnSync ? EventMakesGamepadsVisible::Yes : EventMakesGamepadsVisible::No);

#if PLATFORM(VISION)
    webPageProxy->setGamepadsConnected(isAnyGamepadConnected());
#endif

    m_shouldMakeGamepadsVisibleOnSync = false;
}

#if PLATFORM(VISION)
bool UIGamepadProvider::isAnyGamepadConnected() const
{
    bool anyGamepadConnected = false;
    for (auto& gamepad : m_gamepads) {
        if (gamepad) {
            anyGamepadConnected = true;
            break;
        }
    }
    return anyGamepadConnected;
}
#endif

void UIGamepadProvider::scheduleGamepadStateSync()
{
    if (!m_isMonitoringGamepads || m_gamepadSyncTimer.isActive())
        return;

    if (m_gamepads.isEmpty() || m_processPoolsUsingGamepads.isEmptyIgnoringNullReferences()) {
        m_gamepadSyncTimer.stop();
        return;
    }

    m_gamepadSyncTimer.startOneShot(maximumGamepadUpdateInterval);
}

void UIGamepadProvider::platformGamepadConnected(PlatformGamepad& gamepad, EventMakesGamepadsVisible eventVisibility)
{
    RELEASE_ASSERT(isMainRunLoop());
    RELEASE_LOG(Gamepad, "UIGamepadProvider::platformGamepadConnected - Gamepad index %i attached (visibility: %i, currently m_gamepads.size: %i)\n", gamepad.index(), (int)eventVisibility, (int)m_gamepads.size());

    if (m_gamepads.size() <= gamepad.index())
        m_gamepads.grow(gamepad.index() + 1);

    ASSERT(!m_gamepads[gamepad.index()]);
    m_gamepads[gamepad.index()] = makeUnique<UIGamepad>(gamepad);

    scheduleGamepadStateSync();

    for (Ref pool : m_processPoolsUsingGamepads)
        pool->gamepadConnected(*m_gamepads[gamepad.index()], eventVisibility);
}

void UIGamepadProvider::platformGamepadDisconnected(PlatformGamepad& gamepad)
{
    RELEASE_ASSERT(isMainRunLoop());
    RELEASE_LOG(Gamepad, "UIGamepadProvider::platformGamepadConnected - Detaching gamepad index %i (Current m_gamepads size: %i)\n", gamepad.index(), (int)m_gamepads.size());

    ASSERT(gamepad.index() < m_gamepads.size());
    ASSERT(m_gamepads[gamepad.index()]);

    if (gamepad.index() >= m_gamepads.size()) {
#if PLATFORM(COCOA)
        auto reason = makeString("Unknown platform gamepad disconnect: Index "_s, gamepad.index(), " with "_s, m_gamepads.size(), " known gamepads"_s);
        os_fault_with_payload(OS_REASON_WEBKIT, 0, nullptr, 0, reason.utf8().data(), 0);
#else
        RELEASE_LOG_ERROR(Gamepad, "Unknown platform gamepad disconnect: Index %zu with %zu known gamepads", gamepad.index(), m_gamepads.size());
#endif
        return;
    }

    std::unique_ptr<UIGamepad> disconnectedGamepad = WTFMove(m_gamepads[gamepad.index()]);

    scheduleGamepadStateSync();

    for (Ref pool : m_processPoolsUsingGamepads)
        pool->gamepadDisconnected(*disconnectedGamepad);
}

void UIGamepadProvider::platformGamepadInputActivity(EventMakesGamepadsVisible eventVisibility)
{
    auto platformGamepads = GamepadProvider::singleton().platformGamepads();

    auto end = std::min(m_gamepads.size(), platformGamepads.size());
    for (size_t i = 0; i < end; ++i) {
        if (!m_gamepads[i] || !platformGamepads[i])
            continue;

        m_gamepads[i]->updateFromPlatformGamepad(*platformGamepads[i]);
    }

    if (eventVisibility == EventMakesGamepadsVisible::Yes)
        m_shouldMakeGamepadsVisibleOnSync = true;

    scheduleGamepadStateSync();
}

void UIGamepadProvider::processPoolStartedUsingGamepads(WebProcessPool& pool)
{
    RELEASE_ASSERT(isMainRunLoop());
    ASSERT(!m_processPoolsUsingGamepads.contains(pool));
    m_processPoolsUsingGamepads.add(pool);

    if (!m_isMonitoringGamepads && platformWebPageProxyForGamepadInput())
        startMonitoringGamepads();
}

void UIGamepadProvider::processPoolStoppedUsingGamepads(WebProcessPool& pool)
{
    RELEASE_ASSERT(isMainRunLoop());
    ASSERT(m_processPoolsUsingGamepads.contains(pool));
    m_processPoolsUsingGamepads.remove(pool);

    if (m_isMonitoringGamepads && !platformWebPageProxyForGamepadInput())
        platformStopMonitoringInput();
}

void UIGamepadProvider::viewBecameActive(WebPageProxy& page)
{
    if (!m_processPoolsUsingGamepads.contains(page.configuration().processPool()))
        return;

    if (!m_isMonitoringGamepads)
        startMonitoringGamepads();

#if PLATFORM(VISION)
    page.setGamepadsConnected(isAnyGamepadConnected());
#endif

    if (platformWebPageProxyForGamepadInput())
        platformStartMonitoringInput();
}

void UIGamepadProvider::viewBecameInactive(WebPageProxy& page)
{
#if PLATFORM(VISION)
    page.setGamepadsConnected(false);
#endif

    RefPtr pageForGamepadInput = platformWebPageProxyForGamepadInput();
    if (!pageForGamepadInput || pageForGamepadInput == &page)
        platformStopMonitoringInput();
}

void UIGamepadProvider::startMonitoringGamepads()
{
    RELEASE_ASSERT(isMainRunLoop());

    if (m_isMonitoringGamepads)
        return;

    RELEASE_LOG(Gamepad, "UIGamepadProvider::startMonitoringGamepads - Starting gamepad monitoring");

    m_isMonitoringGamepads = true;
    ASSERT(!m_processPoolsUsingGamepads.isEmptyIgnoringNullReferences());
    GamepadProvider::singleton().startMonitoringGamepads(*this);
}

void UIGamepadProvider::stopMonitoringGamepads()
{
    RELEASE_ASSERT(isMainRunLoop());

    if (!m_isMonitoringGamepads)
        return;

    RELEASE_LOG(Gamepad, "UIGamepadProvider::stopMonitoringGamepads - Clearing m_gamepads vector of size %i", (int)m_gamepads.size());

    m_isMonitoringGamepads = false;

    ASSERT(m_processPoolsUsingGamepads.isEmptyIgnoringNullReferences());
    GamepadProvider::singleton().stopMonitoringGamepads(*this);

    m_gamepads.clear();
}

Vector<std::optional<GamepadData>> UIGamepadProvider::snapshotGamepads()
{
    return m_gamepads.map([](auto& gamepad) {
        return gamepad ? std::optional<GamepadData>(gamepad->gamepadData()) : std::nullopt;
    });
}

#if !PLATFORM(COCOA) && !(USE(MANETTE) && OS(LINUX)) && !USE(LIBWPE) && !USE(WPE_PLATFORM)

void UIGamepadProvider::platformSetDefaultGamepadProvider()
{
    // FIXME: Implement for other platforms
}

WebPageProxy* UIGamepadProvider::platformWebPageProxyForGamepadInput()
{
    // FIXME: Implement for other platforms
    return nullptr;
}

void UIGamepadProvider::platformStopMonitoringInput()
{
}

void UIGamepadProvider::platformStartMonitoringInput()
{
}

#endif // !PLATFORM(COCOA) && !(USE(MANETTE) && OS(LINUX)) && !USE(LIBWPE) && !USE(WPE_PLATFORM)

}

#endif // ENABLE(GAMEPAD)
