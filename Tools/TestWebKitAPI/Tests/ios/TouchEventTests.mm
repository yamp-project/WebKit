/*
 * Copyright (C) 2021 Apple Inc. All rights reserved.
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

#if ENABLE(IOS_TOUCH_EVENTS)

#import "InstanceMethodSwizzler.h"
#import "PlatformUtilities.h"
#import "TestNavigationDelegate.h"
#import "TestWKWebView.h"
#import "UIKitSPIForTesting.h"
#import "WKTouchEventsGestureRecognizer.h"
#import <wtf/RetainPtr.h>
#import <wtf/darwin/DispatchExtras.h>

@interface UIView (WKContentView)
- (void)_touchEventsRecognized;
@end

static WKWebView *globalWebView = nil;

@interface TouchEventScriptMessageHandler : NSObject<WKScriptMessageHandler>
@end

@implementation TouchEventScriptMessageHandler

- (void)userContentController:(WKUserContentController *)userContentController didReceiveScriptMessage:(WKScriptMessage *)message
{
    if ([message.body isEqualToString:@"touchend"]) {
        @autoreleasepool {
            // This @autoreleasepool ensures that the content view is also deallocated upon releasing the web view.
            [globalWebView removeFromSuperview];
            [globalWebView release];
            globalWebView = nil;
        }
    }
}

@end

static Class touchEventsGestureRecognizerClassSingleton()
{
    static Class result = nil;
    static dispatch_once_t onceToken;
    dispatch_once(&onceToken, ^{
        result = NSClassFromString(@"WKTouchEventsGestureRecognizer");
    });
    return result;
}

namespace TestWebKitAPI {

static WebKit::WKTouchPoint globalTouchPoint { CGPointZero, CGPointZero, 100, UITouchPhaseBegan, 1, 0, 0, 0, 0, WebKit::WKTouchPointType::Direct };
static WebKit::WKTouchEvent globalTouchEvent { WebKit::WKTouchEventType::Begin, CACurrentMediaTime(), CGPointZero, 1, 0, false, { globalTouchPoint }, { }, { }, true };
static void updateSimulatedTouchEvent(CGPoint location, UITouchPhase phase)
{
    globalTouchEvent.locationInRootViewCoordinates = location;
    globalTouchPoint.phase = phase;
    switch (phase) {
    case UITouchPhaseBegan:
        globalTouchEvent.type = WebKit::WKTouchEventType::Begin;
        break;
    case UITouchPhaseMoved:
        globalTouchEvent.type = WebKit::WKTouchEventType::Change;
        break;
    case UITouchPhaseEnded:
        globalTouchEvent.type = WebKit::WKTouchEventType::End;
        break;
    case UITouchPhaseCancelled:
        globalTouchEvent.type = WebKit::WKTouchEventType::Cancel;
        break;
    default:
        break;
    }
}

static const WebKit::WKTouchEvent* simulatedTouchEvent(id, SEL)
{
    return &globalTouchEvent;
}

TEST(TouchEventTests, DestroyWebViewWhileHandlingTouchEnd)
{
    InstanceMethodSwizzler lastTouchEventSwizzler { touchEventsGestureRecognizerClassSingleton(), @selector(lastTouchEvent), reinterpret_cast<IMP>(simulatedTouchEvent) };
    @autoreleasepool {
        RetainPtr messageHandler = adoptNS([TouchEventScriptMessageHandler new]);
        RetainPtr controller = adoptNS([[WKUserContentController alloc] init]);
        [controller addScriptMessageHandler:messageHandler.get() name:@"testHandler"];

        RetainPtr configuration = adoptNS([[WKWebViewConfiguration alloc] init]);
        [configuration setUserContentController:controller.get()];

        globalWebView = [[WKWebView alloc] initWithFrame:CGRectMake(0, 0, 320, 500) configuration:configuration.get()];
        RetainPtr hostWindow = adoptNS([[UIWindow alloc] initWithFrame:CGRectMake(0, 0, 320, 500)]);
        [hostWindow setHidden:NO];
        [hostWindow addSubview:globalWebView];

        [globalWebView loadRequest:[NSURLRequest requestWithURL:[NSBundle.test_resourcesBundle URLForResource:@"active-touch-events" withExtension:@"html"]]];
        [globalWebView _test_waitForDidFinishNavigation];

        updateSimulatedTouchEvent(CGPointMake(100, 100), UITouchPhaseBegan);
        [[globalWebView textInputContentView] _touchEventsRecognized];

        updateSimulatedTouchEvent(CGPointMake(100, 100), UITouchPhaseEnded);
        [[globalWebView textInputContentView] _touchEventsRecognized];
    }

    __block bool done = false;
    dispatch_async(mainDispatchQueueSingleton(), ^{
        done = true;
    });
    TestWebKitAPI::Util::run(&done);
}

} // namespace TestWebKitAPI

#endif // ENABLE(IOS_TOUCH_EVENTS)
