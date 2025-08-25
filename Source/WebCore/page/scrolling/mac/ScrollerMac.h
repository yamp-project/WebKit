/*
 * Copyright (C) 2019 Apple Inc. All rights reserved.
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

#include <wtf/Platform.h>
#if PLATFORM(MAC)

#include <WebCore/ScrollTypes.h>
#include <WebCore/UserInterfaceLayoutDirection.h>
#include <wtf/RecursiveLockAdapter.h>
#include <wtf/RetainPtr.h>

OBJC_CLASS CALayer;
OBJC_CLASS NSColor;
OBJC_CLASS NSScrollerImp;
OBJC_CLASS WebScrollerImpDelegateMac;

enum class FeatureToAnimate;

namespace WebCore {

class FloatPoint;
class ScrollerPairMac;

struct ScrollbarColor;

class ScrollerMac final : public CanMakeThreadSafeCheckedPtr<ScrollerMac> {
    WTF_DEPRECATED_MAKE_FAST_ALLOCATED(ScrollerMac);
    WTF_OVERRIDE_DELETE_FOR_CHECKED_PTR(ScrollerMac);
    friend class ScrollerPairMac;
public:
    ScrollerMac(ScrollerPairMac&, ScrollbarOrientation);

    ~ScrollerMac();

    void attach();

    RefPtr<ScrollerPairMac> pair() const { return m_pair.get(); }

    ScrollbarOrientation orientation() const { return m_orientation; }

    CALayer *hostLayer() const { return m_hostLayer.get(); }
    void setHostLayer(CALayer *);

    RetainPtr<NSScrollerImp> takeScrollerImp();
    void setScrollerImp(NSScrollerImp *imp);
    void updateScrollbarStyle();
    void updatePairScrollerImps();

    void setHiddenByStyle(NativeScrollbarVisibility);

    void updateValues();
    
    String scrollbarState() const;
    
    void mouseEnteredScrollbar();
    void mouseExitedScrollbar();    
    void setLastKnownMousePositionInScrollbar(IntPoint position) { m_lastKnownMousePositionInScrollbar = position; }
    IntPoint lastKnownMousePositionInScrollbar() const;
    void visibilityChanged(bool);
    void updateMinimumKnobLength(int);
    void detach();
    void setEnabled(bool flag) { m_isEnabled = flag; }
    void setScrollbarLayoutDirection(UserInterfaceLayoutDirection);
    void scrollbarColorChanged(const std::optional<ScrollbarColor>&);
    void setUsePresentationValue(bool inMomentumPhase);

    void setNeedsDisplay();

    void updateProgress(FeatureToAnimate, double);
    bool isScrollerFor(NSScrollerImp*);
    double knobAlpha();
    double trackAlpha();
    bool hasScrollerImp();
    RecursiveLock& scrollerImpLock() const { return m_scrollerImpLock; }

private:
    int m_minimumKnobLength { 0 };

    bool m_isEnabled { false };
    bool m_isVisible { false };
    bool m_isHiddenByStyle { false };

    ThreadSafeWeakPtr<ScrollerPairMac> m_pair;
    const ScrollbarOrientation m_orientation;
    IntPoint m_lastKnownMousePositionInScrollbar;
    UserInterfaceLayoutDirection m_scrollbarLayoutDirection { UserInterfaceLayoutDirection::LTR };
    RetainPtr<NSColor> m_trackColor;
    RetainPtr<NSColor> m_thumbColor;

    RetainPtr<CALayer> m_hostLayer;
    mutable RecursiveLock m_scrollerImpLock;
    RetainPtr<NSScrollerImp> m_scrollerImp;
    RetainPtr<WebScrollerImpDelegateMac> m_scrollerImpDelegate;
};

}

#endif
