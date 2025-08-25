/*
 * Copyright (C) 2013, 2014 Apple Inc. All rights reserved.
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

#import "config.h"
#import "ViewGestureController.h"

#if PLATFORM(MAC)

#import "APINavigation.h"
#import "DrawingAreaProxy.h"
#import "FrameLoadState.h"
#import "Logging.h"
#import "NativeWebWheelEvent.h"
#import "ProvisionalPageProxy.h"
#import "ViewGestureControllerMessages.h"
#import "ViewGestureGeometryCollectorMessages.h"
#import "ViewSnapshotStore.h"
#import "WebBackForwardList.h"
#import "WebPageGroup.h"
#import "WebPageMessages.h"
#import "WebPageProxy.h"
#import "WebPreferences.h"
#import "WebProcessProxy.h"
#import <Cocoa/Cocoa.h>
#import <WebCore/IOSurface.h>
#import <WebCore/NotImplemented.h>
#import <WebCore/WebActionDisablingCALayerDelegate.h>
#import <pal/spi/cocoa/QuartzCoreSPI.h>
#import <pal/spi/mac/NSEventSPI.h>
#import <wtf/BlockObjCExceptions.h>

static const double zoomOutBoost = 1.6;
static const double zoomOutResistance = 0.10;

static const float smartMagnificationElementPadding = 0.05;
static const float smartMagnificationPanScrollThreshold = 100;

static const double swipeOverlayShadowOpacity = 0.06;
static const double swipeOverlayDimmingOpacity = 0.12;
static const CGFloat swipeOverlayShadowWidth = 81;

@interface WKSwipeCancellationTracker : NSObject {
@private
    BOOL _isCancelled;
}

@property (nonatomic) BOOL isCancelled;

@end

@implementation WKSwipeCancellationTracker
@synthesize isCancelled = _isCancelled;
@end

namespace WebKit {
using namespace WebCore;

void ViewGestureController::platformTeardown()
{
    if (m_swipeCancellationTracker)
        [m_swipeCancellationTracker setIsCancelled:YES];

    if (m_activeGestureType == ViewGestureType::Swipe)
        removeSwipeSnapshot();
}

double ViewGestureController::resistanceForDelta(double deltaScale, double currentScale, double minMagnification, double maxMagnification)
{
    // Zoom out with slight acceleration, until we reach minimum scale.
    if (deltaScale < 0 && currentScale > minMagnification)
        return zoomOutBoost;

    // Zoom in with no acceleration, until we reach maximum scale.
    if (deltaScale > 0 && currentScale < maxMagnification)
        return 1;

    // Outside of the extremes, resist further scaling.
    double limit = currentScale < minMagnification ? minMagnification : maxMagnification;
    double scaleDistance = std::abs(limit - currentScale);
    double scalePercent = std::min(std::max(scaleDistance / limit, 0.), 1.);
    double resistance = zoomOutResistance + scalePercent * (0.01 - zoomOutResistance);

    return resistance;
}

void ViewGestureController::gestureEventWasNotHandledByWebCore(NSEvent *event, FloatPoint origin)
{
    if (event.type == NSEventTypeMagnify)
        handleMagnificationGestureEvent(event, origin);
}

void ViewGestureController::handleMagnificationGestureEvent(NSEvent *event, FloatPoint origin)
{
    RefPtr page = m_webPageProxy.get();
    if (!page)
        return;

    auto obscuredContentInsets = page->obscuredContentInsets();
    origin.move(-obscuredContentInsets.left(), -obscuredContentInsets.top());

    ASSERT(m_activeGestureType == ViewGestureType::None || m_activeGestureType == ViewGestureType::Magnification);

    if (m_activeGestureType == ViewGestureType::None) {
        if (event.phase != NSEventPhaseBegan)
            return;

        // FIXME: We drop the first frame of the gesture on the floor, because we don't have the visible content bounds yet.
        prepareMagnificationGesture(origin);

        return;
    }

    // We're still waiting for the DidCollectGeometry callback.
    if (!m_visibleContentRectIsValid)
        return;

    willBeginGesture(ViewGestureType::Magnification);

    auto minMagnification = page->minPageZoomFactor();
    auto maxMagnification = page->maxPageZoomFactor();

    double scale = event.magnification;
    double scaleWithResistance = resistanceForDelta(scale, m_magnification, minMagnification, maxMagnification) * scale;

    auto minElasticMagnification = minMagnification * 0.75;
    auto maxElasticMagnification = maxMagnification * 1.333;

    m_magnification += m_magnification * scaleWithResistance;
    m_magnification = std::min(std::max(m_magnification, minElasticMagnification), maxElasticMagnification);

    LOG_WITH_STREAM(ViewGestures, stream << "ViewGestureController::handleMagnificationGestureEvent - gesture scale " << scale << " with resistance " << scaleWithResistance << " clamped to " << m_magnification << " origin in view coords " << origin);

    m_magnificationOrigin = origin;

    applyMagnification();

    if (event.phase == NSEventPhaseEnded || event.phase == NSEventPhaseCancelled)
        endMagnificationGesture();
}

void ViewGestureController::handleSmartMagnificationGesture(FloatPoint gestureLocationInViewCoordinates)
{
    if (m_activeGestureType != ViewGestureType::None)
        return;

    LOG_WITH_STREAM(ViewGestures, stream << "ViewGestureController::handleSmartMagnificationGesture - gesture location " << gestureLocationInViewCoordinates);

    if (RefPtr page = m_webPageProxy.get())
        page->protectedLegacyMainFrameProcess()->send(Messages::ViewGestureGeometryCollector::CollectGeometryForSmartMagnificationGesture(gestureLocationInViewCoordinates), page->webPageIDInMainFrameProcess());
}

static float maximumRectangleComponentDelta(FloatRect a, FloatRect b)
{
    return std::max(std::abs(a.x() - b.x()), std::max(std::abs(a.y() - b.y()), std::max(std::abs(a.width() - b.width()), std::abs(a.height() - b.height()))));
}

void ViewGestureController::didCollectGeometryForSmartMagnificationGesture(FloatPoint gestureLocationInViewCoordinates, FloatRect absoluteTargetRect, FloatRect visibleContentRect, bool fitEntireRect, double viewportMinimumScale, double viewportMaximumScale)
{
    RefPtr page = m_webPageProxy.get();
    if (!page)
        return;

    double currentScaleFactor = page->pageScaleFactor();

    LOG_WITH_STREAM(ViewGestures, stream << "ViewGestureController::didCollectGeometryForSmartMagnificationGesture - gesture location " << gestureLocationInViewCoordinates << " absoluteTargetRect " << absoluteTargetRect);

    auto unscaledTargetRect = absoluteTargetRect;

    // If there was no usable element under the cursor, we'll scale towards the cursor instead.
    if (unscaledTargetRect.isEmpty())
        unscaledTargetRect.setLocation(gestureLocationInViewCoordinates);

    unscaledTargetRect.scale(1 / currentScaleFactor);
    unscaledTargetRect.inflateX(unscaledTargetRect.width() * smartMagnificationElementPadding);
    unscaledTargetRect.inflateY(unscaledTargetRect.height() * smartMagnificationElementPadding);

    double targetMagnification = visibleContentRect.width() / unscaledTargetRect.width();

    auto unscaledVisibleContentRect = visibleContentRect;
    unscaledVisibleContentRect.scale(1 / currentScaleFactor);
    auto viewportConstrainedUnscaledTargetRect = unscaledTargetRect;
    viewportConstrainedUnscaledTargetRect.intersect(unscaledVisibleContentRect);

    if (unscaledTargetRect.width() > viewportConstrainedUnscaledTargetRect.width())
        viewportConstrainedUnscaledTargetRect.setX(unscaledVisibleContentRect.x() + (gestureLocationInViewCoordinates.x() / currentScaleFactor) - viewportConstrainedUnscaledTargetRect.width() / 2);
    if (unscaledTargetRect.height() > viewportConstrainedUnscaledTargetRect.height())
        viewportConstrainedUnscaledTargetRect.setY(unscaledVisibleContentRect.y() + (gestureLocationInViewCoordinates.y() / currentScaleFactor) - viewportConstrainedUnscaledTargetRect.height() / 2);

    // For replaced elements like images, we want to fit the whole element
    // in the view, so scale it down enough to make both dimensions fit if possible.
    if (fitEntireRect)
        targetMagnification = std::min(targetMagnification, static_cast<double>(visibleContentRect.height() / viewportConstrainedUnscaledTargetRect.height()));

    auto minMagnification = page->minPageZoomFactor();
    auto maxMagnification = page->maxPageZoomFactor();
    targetMagnification = std::min(std::max(targetMagnification, minMagnification), maxMagnification);

    // Allow panning between elements via double-tap while magnified, unless the target rect is
    // similar to the last one or the mouse has not moved, in which case we'll zoom all the way out.
    if (currentScaleFactor > 1 && m_lastMagnificationGestureWasSmartMagnification) {
        if (maximumRectangleComponentDelta(m_lastSmartMagnificationUnscaledTargetRect, unscaledTargetRect) < smartMagnificationPanScrollThreshold)
            targetMagnification = 1;

        if (m_lastSmartMagnificationOrigin == gestureLocationInViewCoordinates)
            targetMagnification = 1;
    }

    auto targetRect = viewportConstrainedUnscaledTargetRect;
    targetRect.scale(targetMagnification);

    auto targetOrigin = visibleContentRect.center();
    auto targetCenter = targetRect.center();
    targetOrigin.moveBy(-targetCenter);

    m_initialMagnification = page->pageScaleFactor();
    m_initialMagnificationOrigin = { };

    Ref drawingArea = *page->drawingArea();
    auto pageScaleFactor = page->pageScaleFactor();
    drawingArea->adjustTransientZoom(pageScaleFactor, scaledMagnificationOrigin(FloatPoint(), pageScaleFactor), m_magnificationOrigin);
    drawingArea->commitTransientZoom(targetMagnification, targetOrigin);

    m_lastSmartMagnificationUnscaledTargetRect = unscaledTargetRect;
    m_lastSmartMagnificationOrigin = gestureLocationInViewCoordinates;

    m_lastMagnificationGestureWasSmartMagnification = true;
}

bool ViewGestureController::PendingSwipeTracker::scrollEventCanStartSwipe(NSEvent *event)
{
    return event.phase == NSEventPhaseBegan;
}

bool ViewGestureController::PendingSwipeTracker::scrollEventCanEndSwipe(NSEvent *event)
{
    return event.phase == NSEventPhaseEnded;
}

bool ViewGestureController::PendingSwipeTracker::scrollEventCanInfluenceSwipe(NSEvent *event)
{
    return event.hasPreciseScrollingDeltas && [NSEvent isSwipeTrackingFromScrollEventsEnabled];
}

FloatSize ViewGestureController::PendingSwipeTracker::scrollEventGetScrollingDeltas(NSEvent *event)
{
    return FloatSize(event.scrollingDeltaX, event.scrollingDeltaY);
}

bool ViewGestureController::handleScrollWheelEvent(NSEvent *event)
{
    if (m_activeGestureType != ViewGestureType::None)
        return false;
    return m_pendingSwipeTracker.handleEvent(event);
}

void ViewGestureController::trackSwipeGesture(PlatformScrollEvent event, SwipeDirection direction, RefPtr<WebBackForwardListItem> targetItem)
{
    BOOL swipingLeft = isPhysicallySwipingLeft(direction);
    CGFloat maxProgress = swipingLeft ? 1 : 0;
    CGFloat minProgress = !swipingLeft ? -1 : 0;

    __block bool swipeCancelled = false;

    ASSERT(!m_swipeCancellationTracker);
    RetainPtr<WKSwipeCancellationTracker> swipeCancellationTracker = adoptNS([[WKSwipeCancellationTracker alloc] init]);
    m_swipeCancellationTracker = swipeCancellationTracker;

    BEGIN_BLOCK_OBJC_EXCEPTIONS
    [event trackSwipeEventWithOptions:NSEventSwipeTrackingConsumeMouseEvents dampenAmountThresholdMin:minProgress max:maxProgress usingHandler:^(CGFloat progress, NSEventPhase phase, BOOL isComplete, BOOL *stop) {
        if ([swipeCancellationTracker isCancelled]) {
            *stop = YES;
            return;
        }
        if (phase == NSEventPhaseBegan)
            this->beginSwipeGesture(targetItem.get(), direction);
        CGFloat clampedProgress = std::min(std::max(progress, minProgress), maxProgress);
        this->handleSwipeGesture(targetItem.get(), clampedProgress, direction);
        if (phase == NSEventPhaseCancelled)
            swipeCancelled = true;
        if (phase == NSEventPhaseEnded || phase == NSEventPhaseCancelled)
            this->willEndSwipeGesture(*targetItem, swipeCancelled);
        if (isComplete)
            this->endSwipeGesture(targetItem.get(), swipeCancelled);
    }];
    END_BLOCK_OBJC_EXCEPTIONS
}

FloatRect ViewGestureController::windowRelativeBoundsForCustomSwipeViews() const
{
    FloatRect swipeArea;
    for (const auto& view : m_customSwipeViews)
        swipeArea.unite([view convertRect:[view bounds] toView:nil]);
    swipeArea.move(m_customSwipeViewsObscuredContentInsets.left(), m_customSwipeViewsObscuredContentInsets.bottom());
    swipeArea.contract({
        m_customSwipeViewsObscuredContentInsets.left() + m_customSwipeViewsObscuredContentInsets.right(),
        m_customSwipeViewsObscuredContentInsets.bottom() + m_customSwipeViewsObscuredContentInsets.top(),
    });
    return swipeArea;
}

static RetainPtr<CALayer> leastCommonAncestorLayer(const Vector<RetainPtr<CALayer>>& layers)
{
    Vector<Vector<CALayer *>> liveLayerPathsFromRoot(layers.size());

    size_t shortestPathLength = std::numeric_limits<size_t>::max();

    for (size_t layerIndex = 0; layerIndex < layers.size(); layerIndex++) {
        RetainPtr parent = [layers[layerIndex] superlayer];
        while (parent) {
            liveLayerPathsFromRoot[layerIndex].insert(0, parent.get());
            shortestPathLength = std::min(shortestPathLength, liveLayerPathsFromRoot[layerIndex].size());
            parent = parent.get().superlayer;
        }
    }

    for (size_t pathIndex = 0; pathIndex < shortestPathLength; pathIndex++) {
        RetainPtr firstPathLayer = liveLayerPathsFromRoot[0][pathIndex];
        for (size_t layerIndex = 1; layerIndex < layers.size(); layerIndex++) {
            if (liveLayerPathsFromRoot[layerIndex][pathIndex] != firstPathLayer.get())
                return firstPathLayer;
        }
    }

    return liveLayerPathsFromRoot[0][shortestPathLength];
}

RetainPtr<CALayer> ViewGestureController::determineSnapshotLayerParent() const
{
    if (m_currentSwipeLiveLayers.size() == 1)
        return [m_currentSwipeLiveLayers[0] superlayer];

    // We insert our snapshot into the first shared superlayer of the custom views' layer, above the frontmost or below the bottommost layer.
    return leastCommonAncestorLayer(m_currentSwipeLiveLayers);
}

RetainPtr<CALayer> ViewGestureController::determineLayerAdjacentToSnapshotForParent(SwipeDirection direction, CALayer *snapshotLayerParent) const
{
    // If we have custom swiping views, we assume that the views were passed to us in back-to-front z-order.
    RetainPtr layerAdjacentToSnapshot = isPhysicallySwipingLeft(direction) ? m_currentSwipeLiveLayers.first().get() : m_currentSwipeLiveLayers.last().get();

    if (m_currentSwipeLiveLayers.size() == 1)
        return layerAdjacentToSnapshot;

    // If the layers are not all siblings, find the child of the layer we're going to insert the snapshot into which has the frontmost/bottommost layer as a child.
    while (snapshotLayerParent != layerAdjacentToSnapshot.get().superlayer)
        layerAdjacentToSnapshot = layerAdjacentToSnapshot.get().superlayer;
    return layerAdjacentToSnapshot;
}

static bool layerGeometryFlippedToRoot(CALayer *layer)
{
    bool flipped = false;
    RetainPtr parent = layer;
    while (parent) {
        if (parent.get().isGeometryFlipped)
            flipped = !flipped;
        parent = parent.get().superlayer;
    }
    return flipped;
}

void ViewGestureController::applyDebuggingPropertiesToSwipeViews()
{
    RetainPtr filter = [CAFilter filterWithType:kCAFilterColorInvert];
    [m_swipeLayer setFilters:@[ filter.get() ]];
    [m_swipeLayer setBackgroundColor:[NSColor blueColor].CGColor];
    [m_swipeLayer setBorderColor:[NSColor yellowColor].CGColor];
    [m_swipeLayer setBorderWidth:4];

    [m_swipeSnapshotLayer setBackgroundColor:[NSColor greenColor].CGColor];
    [m_swipeSnapshotLayer setBorderColor:[NSColor redColor].CGColor];
    [m_swipeSnapshotLayer setBorderWidth:2];
}

void ViewGestureController::beginSwipeGesture(WebBackForwardListItem* targetItem, SwipeDirection direction)
{
    ASSERT(m_currentSwipeLiveLayers.isEmpty());

    RefPtr page = m_webPageProxy.get();
    if (!page)
        return;

    page->navigationGestureDidBegin();

    willBeginGesture(ViewGestureType::Swipe);

    RetainPtr rootContentLayer = page->acceleratedCompositingRootLayer();

    m_swipeLayer = adoptNS([[CALayer alloc] init]);
    m_swipeSnapshotLayer = adoptNS([[CALayer alloc] init]);
    m_currentSwipeCustomViewBounds = windowRelativeBoundsForCustomSwipeViews();

    FloatRect swipeArea;
    FloatBoxExtent obscuredContentInsets;
    if (!m_customSwipeViews.isEmpty()) {
        obscuredContentInsets = m_customSwipeViewsObscuredContentInsets;
        swipeArea = m_currentSwipeCustomViewBounds;
        swipeArea.expand({
            obscuredContentInsets.left() + obscuredContentInsets.right(),
            obscuredContentInsets.bottom() + obscuredContentInsets.top(),
        });
        swipeArea.move(-obscuredContentInsets.left(), -obscuredContentInsets.bottom());

        for (const auto& view : m_customSwipeViews) {
            RetainPtr layer = [view layer];
            ASSERT(layer);
            m_currentSwipeLiveLayers.append(layer.get());
        }
    } else {
        swipeArea = [rootContentLayer convertRect:CGRectMake(0, 0, page->viewSize().width(), page->viewSize().height()) toLayer:nil];
        obscuredContentInsets = page->obscuredContentInsets();
        m_currentSwipeLiveLayers.append(rootContentLayer.get());
    }

    RetainPtr snapshotLayerParent = determineSnapshotLayerParent();
    bool geometryIsFlippedToRoot = layerGeometryFlippedToRoot(snapshotLayerParent.get());

    RetainPtr backgroundColor = CGColorGetConstantColor(kCGColorWhite);
    if (RefPtr snapshot = targetItem->snapshot()) {
        if (shouldUseSnapshotForSize(*snapshot, swipeArea.size(), obscuredContentInsets))
            [m_swipeSnapshotLayer setContents:snapshot->asLayerContents()];

        Color coreColor = snapshot->backgroundColor();
        if (coreColor.isValid())
            backgroundColor = cachedCGColor(coreColor);
        m_currentSwipeSnapshot = snapshot;
    }

    [m_swipeLayer setBackgroundColor:backgroundColor.get()];
    [m_swipeLayer setAnchorPoint:CGPointZero];
    [m_swipeLayer setFrame:[snapshotLayerParent convertRect:swipeArea fromLayer:nil]];
    [m_swipeLayer setName:@"Gesture Swipe Root Layer"];
    [m_swipeLayer setGeometryFlipped:geometryIsFlippedToRoot];
    [m_swipeLayer setDelegate:[WebActionDisablingCALayerDelegate shared]];

    float deviceScaleFactor = page->deviceScaleFactor();
    [m_swipeSnapshotLayer setContentsGravity:kCAGravityTopLeft];
    [m_swipeSnapshotLayer setContentsScale:deviceScaleFactor];
    [m_swipeSnapshotLayer setAnchorPoint:CGPointZero];
    [m_swipeSnapshotLayer setFrame:CGRectMake(
        obscuredContentInsets.left(),
        obscuredContentInsets.bottom(),
        swipeArea.width() - obscuredContentInsets.right() - obscuredContentInsets.left(),
        swipeArea.height() - obscuredContentInsets.top() - obscuredContentInsets.bottom()
    )];
    [m_swipeSnapshotLayer setName:@"Gesture Swipe Snapshot Layer"];
    [m_swipeSnapshotLayer setDelegate:[WebActionDisablingCALayerDelegate shared]];

    [m_swipeLayer addSublayer:m_swipeSnapshotLayer.get()];

    if (page->protectedPreferences()->viewGestureDebuggingEnabled())
        applyDebuggingPropertiesToSwipeViews();

    m_didCallEndSwipeGesture = false;
    m_removeSnapshotImmediatelyWhenGestureEnds = false;

    RetainPtr layerAdjacentToSnapshot = determineLayerAdjacentToSnapshotForParent(direction, snapshotLayerParent.get());
    BOOL swipingLeft = isPhysicallySwipingLeft(direction);
    if (swipingLeft)
        [snapshotLayerParent insertSublayer:m_swipeLayer.get() below:layerAdjacentToSnapshot.get()];
    else
        [snapshotLayerParent insertSublayer:m_swipeLayer.get() above:layerAdjacentToSnapshot.get()];

    // We don't know enough about the custom views' hierarchy to apply a shadow.
    if (m_customSwipeViews.isEmpty()) {
        FloatRect dimmingRect(FloatPoint(), page->viewSize());
        m_swipeDimmingLayer = adoptNS([[CALayer alloc] init]);
        [m_swipeDimmingLayer setName:@"Gesture Swipe Dimming Layer"];
        [m_swipeDimmingLayer setBackgroundColor:[NSColor blackColor].CGColor];
        [m_swipeDimmingLayer setOpacity:swipeOverlayDimmingOpacity];
        [m_swipeDimmingLayer setAnchorPoint:CGPointZero];
        [m_swipeDimmingLayer setFrame:dimmingRect];
        [m_swipeDimmingLayer setGeometryFlipped:geometryIsFlippedToRoot];
        [m_swipeDimmingLayer setDelegate:[WebActionDisablingCALayerDelegate shared]];

        FloatRect shadowRect(-swipeOverlayShadowWidth, obscuredContentInsets.top(), swipeOverlayShadowWidth, page->viewSize().height() - obscuredContentInsets.top());
        m_swipeShadowLayer = adoptNS([[CAGradientLayer alloc] init]);
        [m_swipeShadowLayer setName:@"Gesture Swipe Shadow Layer"];
        [m_swipeShadowLayer setColors:@[
            (__bridge id)adoptCF(CGColorCreateGenericGray(0, 1.)).get(),
            (__bridge id)adoptCF(CGColorCreateGenericGray(0, 0.99)).get(),
            (__bridge id)adoptCF(CGColorCreateGenericGray(0, 0.98)).get(),
            (__bridge id)adoptCF(CGColorCreateGenericGray(0, 0.95)).get(),
            (__bridge id)adoptCF(CGColorCreateGenericGray(0, 0.92)).get(),
            (__bridge id)adoptCF(CGColorCreateGenericGray(0, 0.82)).get(),
            (__bridge id)adoptCF(CGColorCreateGenericGray(0, 0.71)).get(),
            (__bridge id)adoptCF(CGColorCreateGenericGray(0, 0.46)).get(),
            (__bridge id)adoptCF(CGColorCreateGenericGray(0, 0.35)).get(),
            (__bridge id)adoptCF(CGColorCreateGenericGray(0, 0.25)).get(),
            (__bridge id)adoptCF(CGColorCreateGenericGray(0, 0.17)).get(),
            (__bridge id)adoptCF(CGColorCreateGenericGray(0, 0.11)).get(),
            (__bridge id)adoptCF(CGColorCreateGenericGray(0, 0.07)).get(),
            (__bridge id)adoptCF(CGColorCreateGenericGray(0, 0.04)).get(),
            (__bridge id)adoptCF(CGColorCreateGenericGray(0, 0.01)).get(),
            (__bridge id)adoptCF(CGColorCreateGenericGray(0, 0.)).get(),
        ]];
        [m_swipeShadowLayer setLocations:@[
            @0,
            @0.03125,
            @0.0625,
            @0.0938,
            @0.125,
            @0.1875,
            @0.25,
            @0.375,
            @0.4375,
            @0.5,
            @0.5625,
            @0.625,
            @0.6875,
            @0.75,
            @0.875,
            @1,
        ]];
        [m_swipeShadowLayer setStartPoint:CGPointMake(1, 0)];
        [m_swipeShadowLayer setEndPoint:CGPointMake(0, 0)];
        [m_swipeShadowLayer setOpacity:swipeOverlayShadowOpacity];
        [m_swipeShadowLayer setAnchorPoint:CGPointZero];
        [m_swipeShadowLayer setFrame:shadowRect];
        [m_swipeShadowLayer setGeometryFlipped:geometryIsFlippedToRoot];
        [m_swipeShadowLayer setDelegate:[WebActionDisablingCALayerDelegate shared]];

        if (swipingLeft)
            [snapshotLayerParent insertSublayer:m_swipeDimmingLayer.get() above:m_swipeLayer.get()];
        else
            [snapshotLayerParent insertSublayer:m_swipeDimmingLayer.get() below:m_swipeLayer.get()];

        [snapshotLayerParent insertSublayer:m_swipeShadowLayer.get() above:m_swipeLayer.get()];
    }
}

void ViewGestureController::handleSwipeGesture(WebBackForwardListItem* targetItem, double progress, SwipeDirection direction)
{
    ASSERT(m_activeGestureType == ViewGestureType::Swipe);

    if (!m_webPageProxy)
        return;

    if (!m_webPageProxy->drawingArea())
        return;

    bool swipingLeft = isPhysicallySwipingLeft(direction);

    double width;
    if (!m_customSwipeViews.isEmpty())
        width = m_currentSwipeCustomViewBounds.width();
    else
        width = m_webPageProxy->drawingArea()->size().width();

    double swipingLayerOffset = floor(width * progress);

    double dimmingProgress = swipingLeft ? 1 - progress : -progress;
    dimmingProgress = std::min(1., std::max(dimmingProgress, 0.));
    [m_swipeDimmingLayer setOpacity:dimmingProgress * swipeOverlayDimmingOpacity];

    double absoluteProgress = std::abs(progress);
    double remainingSwipeDistance = width - std::abs(absoluteProgress * width);
    double shadowFadeDistance = [m_swipeShadowLayer bounds].size.width;
    if (remainingSwipeDistance < shadowFadeDistance)
        [m_swipeShadowLayer setOpacity:(remainingSwipeDistance / shadowFadeDistance) * swipeOverlayShadowOpacity];
    else
        [m_swipeShadowLayer setOpacity:swipeOverlayShadowOpacity];

    [m_swipeShadowLayer setTransform:CATransform3DMakeTranslation((swipingLeft ? 0 : width) + swipingLayerOffset, 0, 0)];

    if (swipingLeft) {
        for (const auto& layer : m_currentSwipeLiveLayers)
            [layer setTransform:CATransform3DMakeTranslation(swipingLayerOffset, 0, 0)];
    } else {
        [m_swipeLayer setTransform:CATransform3DMakeTranslation(width + swipingLayerOffset, 0, 0)];
        didMoveSwipeSnapshotLayer();
    }
}

void ViewGestureController::didMoveSwipeSnapshotLayer()
{
    if (!m_didMoveSwipeSnapshotCallback)
        return;

    if (RefPtr page = m_webPageProxy.get())
        m_didMoveSwipeSnapshotCallback(page->boundsOfLayerInLayerBackedWindowCoordinates(m_swipeLayer.get()));
}

void ViewGestureController::removeSwipeSnapshot()
{
    m_snapshotRemovalTracker.reset();

    m_hasOutstandingRepaintRequest = false;

    if (m_activeGestureType != ViewGestureType::Swipe)
        return;

    if (!m_didCallEndSwipeGesture) {
        m_removeSnapshotImmediatelyWhenGestureEnds = true;
        return;
    }

    resetState();
}

void ViewGestureController::resetState()
{
    if (RefPtr currentSwipeSnapshot = m_currentSwipeSnapshot)
        currentSwipeSnapshot->setVolatile(true);
    m_currentSwipeSnapshot = nullptr;

    if (m_swipeCancellationTracker)
        [m_swipeCancellationTracker setIsCancelled:YES];
    m_swipeCancellationTracker = nil;

    for (const auto& layer : m_currentSwipeLiveLayers)
        [layer setTransform:CATransform3DIdentity];

    [m_swipeSnapshotLayer removeFromSuperlayer];
    m_swipeSnapshotLayer = nullptr;

    [m_swipeLayer removeFromSuperlayer];
    m_swipeLayer = nullptr;

    [m_swipeShadowLayer removeFromSuperlayer];
    m_swipeShadowLayer = nullptr;

    [m_swipeDimmingLayer removeFromSuperlayer];
    m_swipeDimmingLayer = nullptr;

    m_currentSwipeLiveLayers.clear();

    if (RefPtr page = m_webPageProxy.get())
        page->navigationGestureSnapshotWasRemoved();

    m_backgroundColorForCurrentSnapshot = Color();

    m_pendingNavigation = nullptr;

    didEndGesture();
}

void ViewGestureController::reset()
{
    removeSwipeSnapshot();
    resetState();
}

bool ViewGestureController::beginSimulatedSwipeInDirectionForTesting(SwipeDirection direction)
{
    RefPtr item = itemForSwipeDirection(direction);
    if (!item)
        return false;

    beginSwipeGesture(item.get(), direction);
    return true;
}

bool ViewGestureController::completeSimulatedSwipeInDirectionForTesting(SwipeDirection direction)
{
    RefPtr item = itemForSwipeDirection(direction);
    if (!item)
        return false;

    willEndSwipeGesture(*item, false);
    endSwipeGesture(item.get(), false);
    return true;
}

WebBackForwardList* ViewGestureController::backForwardListForNavigation() const
{
    if (RefPtr page = m_webPageProxy.get())
        return &page->backForwardList();

    return nullptr;
}

} // namespace WebKit

#endif // PLATFORM(MAC)
