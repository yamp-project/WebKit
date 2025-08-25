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
#import "config.h"

#import "PlatformUtilities.h"
#import "Test.h"
#import "TestWKWebView.h"
#import "WKWebViewConfigurationExtras.h"
#import <WebKit/WKWebViewConfigurationPrivate.h>
#import <WebKit/WKWebViewPrivate.h>
#import <WebKit/WKWebViewPrivateForTesting.h>
#import <wtf/RetainPtr.h>

namespace TestWebKitAPI {

RetainPtr<WKWebView> createWebViewForFragmentDirectiveGenerationWithHTML(NSString *HTMLString, NSString *javaScript)
{
    RetainPtr configuration = [WKWebViewConfiguration _test_configurationWithTestPlugInClassName:@"WebProcessPlugInWithInternals" configureJSCForTesting:YES];
    RetainPtr webView = adoptNS([[TestWKWebView alloc] initWithFrame:CGRectMake(0, 0, 320, 500) configuration:configuration.get()]);
    [webView synchronouslyLoadHTMLString:HTMLString];
    [webView stringByEvaluatingJavaScript:javaScript];

    return webView;
}

TEST(FragmentDirectiveGeneration, GenerateFragment)
{
    RetainPtr webView = createWebViewForFragmentDirectiveGenerationWithHTML(@"Test", @"document.execCommand('SelectAll')");

    __block bool done = false;
    [webView _textFragmentDirectiveFromSelectionWithCompletionHandler:^(NSURL* url) {
        EXPECT_WK_STREQ(":~:text=Test", url.fragment);
        done = true;
    }];
    TestWebKitAPI::Util::run(&done);
}

TEST(FragmentDirectiveGeneration, VerifyFragmentRanges)
{
    RetainPtr webView = createWebViewForFragmentDirectiveGenerationWithHTML(@"Test Page", @"location.href = \"#:~:text=Page\"");

    __block bool done = false;
    [webView _textFragmentRangesWithCompletionHandlerForTesting:^(NSArray<NSValue *> * fragmentRanges) {
        EXPECT_TRUE(NSEqualRanges(fragmentRanges[0].rangeValue, NSMakeRange(5, 4)));
        done = true;
    }];
    TestWebKitAPI::Util::run(&done);
}

} // namespace TestWebKitAPI
