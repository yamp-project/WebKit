/*
 * Copyright (C) 2015-2025 Apple Inc. All rights reserved.
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
#import "NetworkSessionCocoa.h"

#import "AppStoreDaemonSPI.h"
#import "AuthenticationChallengeDisposition.h"
#import "AuthenticationManager.h"
#import "DefaultWebBrowserChecks.h"
#import "Download.h"
#import "LegacyCustomProtocolManager.h"
#import "Logging.h"
#import "NetworkDataTaskBlob.h"
#import "NetworkDataTaskCocoa.h"
#import "NetworkLoad.h"
#import "NetworkProcess.h"
#import "NetworkProcessProxyMessages.h"
#import "NetworkSessionCreationParameters.h"
#import "PrivateRelayed.h"
#import "WKURLSessionTaskDelegate.h"
#import "WebErrors.h"
#import "WebPageNetworkParameters.h"
#import "WebSocketTask.h"
#import <Foundation/NSURLSession.h>
#import <WebCore/AdvancedPrivacyProtections.h>
#import <WebCore/Credential.h>
#import <WebCore/FormDataStreamMac.h>
#import <WebCore/FrameLoaderTypes.h>
#import <WebCore/HTTPStatusCodes.h>
#import <WebCore/NetworkStorageSession.h>
#import <WebCore/NotImplemented.h>
#import <WebCore/ResourceError.h>
#import <WebCore/ResourceRequest.h>
#import <WebCore/ResourceResponse.h>
#import <WebCore/SharedBuffer.h>
#import <WebCore/ThreadableWebSocketChannel.h>
#import <WebCore/WebCoreURLResponse.h>
#import <pal/spi/cf/CFNetworkSPI.h>
#import <wtf/BlockPtr.h>
#import <wtf/MainThread.h>
#import <wtf/NeverDestroyed.h>
#import <wtf/ObjCRuntimeExtras.h>
#import <wtf/ProcessPrivilege.h>
#import <wtf/SoftLinking.h>
#import <wtf/StdLibExtras.h>
#import <wtf/TZoneMallocInlines.h>
#import <wtf/URL.h>
#import <wtf/WeakObjCPtr.h>
#import <wtf/cocoa/RuntimeApplicationChecksCocoa.h>
#import <wtf/cocoa/SpanCocoa.h>
#import <wtf/cocoa/TypeCastsCocoa.h>
#import <wtf/cocoa/VectorCocoa.h>
#import <wtf/darwin/WeakLinking.h>
#import <wtf/text/MakeString.h>
#import <wtf/text/WTFString.h>

#if USE(APPLE_INTERNAL_SDK)

#if ENABLE(APP_PRIVACY_REPORT) && HAVE(SYMPTOMS_FRAMEWORK)
#import <Symptoms/SymptomAnalytics.h>
#import <Symptoms/SymptomPresentationFeed.h>

SOFT_LINK_PRIVATE_FRAMEWORK_IN_UMBRELLA_OPTIONAL(Symptoms, SymptomAnalytics);
SOFT_LINK_PRIVATE_FRAMEWORK_IN_UMBRELLA_OPTIONAL(Symptoms, SymptomPresentationFeed);
SOFT_LINK_PRIVATE_FRAMEWORK_IN_UMBRELLA_OPTIONAL(Symptoms, SymptomPresentationLite);
SOFT_LINK_CLASS_OPTIONAL(SymptomAnalytics, AnalyticsWorkspace);
SOFT_LINK_CLASS_OPTIONAL(SymptomPresentationFeed, UsageFeed);
SOFT_LINK_CONSTANT_MAY_FAIL(SymptomPresentationFeed, kSymptomAnalyticsServiceDomainTrackingClearHistoryBundleIDs, const NSString *);
SOFT_LINK_CONSTANT_MAY_FAIL(SymptomPresentationFeed, kSymptomAnalyticsServiceDomainTrackingClearHistoryStartDate, const NSString *);
SOFT_LINK_CONSTANT_MAY_FAIL(SymptomPresentationFeed, kSymptomAnalyticsServiceDomainTrackingClearHistoryEndDate, const NSString *);
SOFT_LINK_CONSTANT_MAY_FAIL(SymptomPresentationFeed, kSymptomAnalyticsServiceDomainTrackingClearHistoryKey, const NSString *);
SOFT_LINK_CONSTANT_MAY_FAIL(SymptomPresentationLite, kSymptomAnalyticsServiceEndpoint, NSString *)
#endif

#else
void WebKit::NetworkSessionCocoa::removeNetworkWebsiteData(std::optional<WallTime>, std::optional<HashSet<WebCore::RegistrableDomain>>&&, CompletionHandler<void()>&& completionHandler) { completionHandler(); }
#endif

#if HAVE(NW_PROXY_CONFIG)
#if __has_include(<Network/NSURLSession+Network.h>)
#include <Network/NSURLSession+Network.h>
#endif
SOFT_LINK_LIBRARY_OPTIONAL(libnetwork)
SOFT_LINK_OPTIONAL(libnetwork, nw_context_add_proxy, void, __cdecl, (nw_context_t, nw_proxy_config_t))
SOFT_LINK_OPTIONAL(libnetwork, nw_context_clear_proxies, void, __cdecl, (nw_context_t))
SOFT_LINK_OPTIONAL(libnetwork, nw_proxy_config_create_with_agent_data, nw_proxy_config_t, __cdecl, (const uint8_t*, size_t, const uuid_t))
SOFT_LINK_OPTIONAL(libnetwork, nw_proxy_config_stack_requires_http_protocols, bool, __cdecl, (nw_proxy_config_t))
#endif

#import "DeviceManagementSoftLink.h"

using namespace WebKit;

WTF_MAKE_TZONE_ALLOCATED_IMPL(IsolatedSession);
WTF_MAKE_TZONE_ALLOCATED_IMPL(NetworkSessionCocoa);

constexpr unsigned maxNumberOfIsolatedSessions { 10 };

static NSURLSessionResponseDisposition toNSURLSessionResponseDisposition(WebCore::PolicyAction disposition)
{
    switch (disposition) {
    case WebCore::PolicyAction::LoadWillContinueInAnotherProcess:
        ASSERT_NOT_REACHED();
#if !ASSERT_ENABLED
        [[fallthrough]];
#endif
    case WebCore::PolicyAction::Ignore:
        return NSURLSessionResponseCancel;
    case WebCore::PolicyAction::Use:
        return NSURLSessionResponseAllow;
    case WebCore::PolicyAction::Download:
        return NSURLSessionResponseBecomeDownload;
    }
}

static NSURLSessionAuthChallengeDisposition toNSURLSessionAuthChallengeDisposition(WebKit::AuthenticationChallengeDisposition disposition)
{
    switch (disposition) {
    case WebKit::AuthenticationChallengeDisposition::UseCredential:
        return NSURLSessionAuthChallengeUseCredential;
    case WebKit::AuthenticationChallengeDisposition::PerformDefaultHandling:
        return NSURLSessionAuthChallengePerformDefaultHandling;
    case WebKit::AuthenticationChallengeDisposition::Cancel:
        return NSURLSessionAuthChallengeCancelAuthenticationChallenge;
    case WebKit::AuthenticationChallengeDisposition::RejectProtectionSpaceAndContinue:
        return NSURLSessionAuthChallengeRejectProtectionSpace;
    }
}

static WebCore::NetworkLoadPriority toNetworkLoadPriority(float priority)
{
    if (priority <= NSURLSessionTaskPriorityLow)
        return WebCore::NetworkLoadPriority::Low;
    if (priority >= NSURLSessionTaskPriorityHigh)
        return WebCore::NetworkLoadPriority::High;
    return WebCore::NetworkLoadPriority::Medium;
}

static WebCore::PrivacyStance toPrivacyStance(nw_connection_privacy_stance_t stance)
{
    switch (stance) {
    case nw_connection_privacy_stance_unknown:
        return WebCore::PrivacyStance::Unknown;
    case nw_connection_privacy_stance_not_eligible:
        return WebCore::PrivacyStance::NotEligible;
    case nw_connection_privacy_stance_proxied:
        return WebCore::PrivacyStance::Proxied;
    case nw_connection_privacy_stance_failed:
        return WebCore::PrivacyStance::Failed;
    case nw_connection_privacy_stance_direct:
        return WebCore::PrivacyStance::Direct;
#if defined(NW_CONNECTION_HAS_PRIVACY_STANCE_FAILED_UNREACHABLE)
    case nw_connection_privacy_stance_failed_unreachable:
        return WebCore::PrivacyStance::FailedUnreachable;
#endif
    }
    ASSERT_NOT_REACHED();
    return WebCore::PrivacyStance::Unknown;
}

static NSString* privacyStanceToString(WebCore::PrivacyStance stance)
{
    switch (stance) {
    case WebCore::PrivacyStance::Unknown:
        return @"Unknown";
    case WebCore::PrivacyStance::NotEligible:
        return @"NotEligible";
    case WebCore::PrivacyStance::Proxied:
        return @"Proxied";
    case WebCore::PrivacyStance::Failed:
        return @"Failed";
    case WebCore::PrivacyStance::Direct:
        return @"Direct";
    case WebCore::PrivacyStance::FailedUnreachable:
        return @"FailedUnreachable";
    }
    ASSERT_NOT_REACHED();
    return @"Unknown";
}

#if HAVE(CFNETWORK_METRICS_APIS_V4)
static String stringForTLSProtocolVersion(tls_protocol_version_t protocol)
{
ALLOW_DEPRECATED_DECLARATIONS_BEGIN
    switch (protocol) {
    case tls_protocol_version_TLSv10:
        return "TLS 1.0"_s;
    case tls_protocol_version_TLSv11:
        return "TLS 1.1"_s;
    case tls_protocol_version_TLSv12:
        return "TLS 1.2"_s;
    case tls_protocol_version_TLSv13:
        return "TLS 1.3"_s;
    case tls_protocol_version_DTLSv10:
        return "DTLS 1.0"_s;
    case tls_protocol_version_DTLSv12:
        return "DTLS 1.2"_s;
    }
ALLOW_DEPRECATED_DECLARATIONS_END
    return { };
}

static String stringForTLSCipherSuite(tls_ciphersuite_t suite)
{
#define STRINGIFY_CIPHER(cipher) \
    case tls_ciphersuite_##cipher: \
        return "" #cipher ""_s

// FIXME: rdar://128061442
ALLOW_DEPRECATED_DECLARATIONS_BEGIN
    switch (suite) {
        STRINGIFY_CIPHER(RSA_WITH_3DES_EDE_CBC_SHA);
        STRINGIFY_CIPHER(RSA_WITH_AES_128_CBC_SHA);
        STRINGIFY_CIPHER(RSA_WITH_AES_256_CBC_SHA);
        STRINGIFY_CIPHER(RSA_WITH_AES_128_GCM_SHA256);
        STRINGIFY_CIPHER(RSA_WITH_AES_256_GCM_SHA384);
        STRINGIFY_CIPHER(RSA_WITH_AES_128_CBC_SHA256);
        STRINGIFY_CIPHER(RSA_WITH_AES_256_CBC_SHA256);
        STRINGIFY_CIPHER(ECDHE_ECDSA_WITH_3DES_EDE_CBC_SHA);
        STRINGIFY_CIPHER(ECDHE_ECDSA_WITH_AES_128_CBC_SHA);
        STRINGIFY_CIPHER(ECDHE_ECDSA_WITH_AES_256_CBC_SHA);
        STRINGIFY_CIPHER(ECDHE_RSA_WITH_3DES_EDE_CBC_SHA);
        STRINGIFY_CIPHER(ECDHE_RSA_WITH_AES_128_CBC_SHA);
        STRINGIFY_CIPHER(ECDHE_RSA_WITH_AES_256_CBC_SHA);
        STRINGIFY_CIPHER(ECDHE_ECDSA_WITH_AES_128_CBC_SHA256);
        STRINGIFY_CIPHER(ECDHE_ECDSA_WITH_AES_256_CBC_SHA384);
        STRINGIFY_CIPHER(ECDHE_RSA_WITH_AES_128_CBC_SHA256);
        STRINGIFY_CIPHER(ECDHE_RSA_WITH_AES_256_CBC_SHA384);
        STRINGIFY_CIPHER(ECDHE_ECDSA_WITH_AES_128_GCM_SHA256);
        STRINGIFY_CIPHER(ECDHE_ECDSA_WITH_AES_256_GCM_SHA384);
        STRINGIFY_CIPHER(ECDHE_RSA_WITH_AES_128_GCM_SHA256);
        STRINGIFY_CIPHER(ECDHE_RSA_WITH_AES_256_GCM_SHA384);
        STRINGIFY_CIPHER(ECDHE_RSA_WITH_CHACHA20_POLY1305_SHA256);
        STRINGIFY_CIPHER(ECDHE_ECDSA_WITH_CHACHA20_POLY1305_SHA256);
        STRINGIFY_CIPHER(AES_128_GCM_SHA256);
        STRINGIFY_CIPHER(AES_256_GCM_SHA384);
        STRINGIFY_CIPHER(CHACHA20_POLY1305_SHA256);
    }
ALLOW_DEPRECATED_DECLARATIONS_END

    return { };

#undef STRINGIFY_CIPHER
}

#else // HAVE(CFNETWORK_METRICS_APIS_V4)

static String stringForSSLProtocol(SSLProtocol protocol)
{
ALLOW_DEPRECATED_DECLARATIONS_BEGIN
    switch (protocol) {
    case kDTLSProtocol1:
        return "DTLS 1.0"_s;
    case kSSLProtocol2:
        return "SSL 2.0"_s;
    case kSSLProtocol3:
        return "SSL 3.0"_s;
    case kSSLProtocol3Only:
        return "SSL 3.0 (Only)"_s;
    case kTLSProtocol1:
        return "TLS 1.0"_s;
    case kTLSProtocol1Only:
        return "TLS 1.0 (Only)"_s;
    case kTLSProtocol11:
        return "TLS 1.1"_s;
    case kTLSProtocol12:
        return "TLS 1.2"_s;
    case kTLSProtocol13:
        return "TLS 1.3"_s;
    case kSSLProtocolAll:
        return "All"_s;
    case kSSLProtocolUnknown:
        return "Unknown"_s;
    case kTLSProtocolMaxSupported:
    default:
        ASSERT_NOT_REACHED();
        return emptyString();
    }
ALLOW_DEPRECATED_DECLARATIONS_END
}

static String stringForSSLCipher(SSLCipherSuite cipher)
{
#define STRINGIFY_CIPHER(cipher) \
    case cipher: \
        return "" #cipher ""_s

// FIXME: rdar://128061442
ALLOW_DEPRECATED_DECLARATIONS_BEGIN
    switch (cipher) {
    STRINGIFY_CIPHER(SSL_RSA_EXPORT_WITH_RC4_40_MD5);
    STRINGIFY_CIPHER(SSL_RSA_EXPORT_WITH_RC2_CBC_40_MD5);
    STRINGIFY_CIPHER(SSL_RSA_WITH_IDEA_CBC_SHA);
    STRINGIFY_CIPHER(SSL_RSA_EXPORT_WITH_DES40_CBC_SHA);
    STRINGIFY_CIPHER(SSL_RSA_WITH_DES_CBC_SHA);
    STRINGIFY_CIPHER(SSL_DH_DSS_EXPORT_WITH_DES40_CBC_SHA);
    STRINGIFY_CIPHER(SSL_DH_DSS_WITH_DES_CBC_SHA);
    STRINGIFY_CIPHER(SSL_DH_RSA_EXPORT_WITH_DES40_CBC_SHA);
    STRINGIFY_CIPHER(SSL_DH_RSA_WITH_DES_CBC_SHA);
    STRINGIFY_CIPHER(SSL_DHE_DSS_EXPORT_WITH_DES40_CBC_SHA);
    STRINGIFY_CIPHER(SSL_DHE_DSS_WITH_DES_CBC_SHA);
    STRINGIFY_CIPHER(SSL_DHE_RSA_EXPORT_WITH_DES40_CBC_SHA);
    STRINGIFY_CIPHER(SSL_DHE_RSA_WITH_DES_CBC_SHA);
    STRINGIFY_CIPHER(SSL_DH_anon_EXPORT_WITH_RC4_40_MD5);
    STRINGIFY_CIPHER(SSL_DH_anon_EXPORT_WITH_DES40_CBC_SHA);
    STRINGIFY_CIPHER(SSL_DH_anon_WITH_DES_CBC_SHA);
    STRINGIFY_CIPHER(SSL_FORTEZZA_DMS_WITH_NULL_SHA);
    STRINGIFY_CIPHER(SSL_FORTEZZA_DMS_WITH_FORTEZZA_CBC_SHA);
    STRINGIFY_CIPHER(TLS_RSA_WITH_AES_128_CBC_SHA);
    STRINGIFY_CIPHER(TLS_DH_DSS_WITH_AES_128_CBC_SHA);
    STRINGIFY_CIPHER(TLS_DH_RSA_WITH_AES_128_CBC_SHA);
    STRINGIFY_CIPHER(TLS_DHE_DSS_WITH_AES_128_CBC_SHA);
    STRINGIFY_CIPHER(TLS_DHE_RSA_WITH_AES_128_CBC_SHA);
    STRINGIFY_CIPHER(TLS_DH_anon_WITH_AES_128_CBC_SHA);
    STRINGIFY_CIPHER(TLS_RSA_WITH_AES_256_CBC_SHA);
    STRINGIFY_CIPHER(TLS_DH_DSS_WITH_AES_256_CBC_SHA);
    STRINGIFY_CIPHER(TLS_DH_RSA_WITH_AES_256_CBC_SHA);
    STRINGIFY_CIPHER(TLS_DHE_DSS_WITH_AES_256_CBC_SHA);
    STRINGIFY_CIPHER(TLS_DHE_RSA_WITH_AES_256_CBC_SHA);
    STRINGIFY_CIPHER(TLS_DH_anon_WITH_AES_256_CBC_SHA);
    STRINGIFY_CIPHER(TLS_ECDH_ECDSA_WITH_NULL_SHA);
    STRINGIFY_CIPHER(TLS_ECDH_ECDSA_WITH_RC4_128_SHA);
    STRINGIFY_CIPHER(TLS_ECDH_ECDSA_WITH_3DES_EDE_CBC_SHA);
    STRINGIFY_CIPHER(TLS_ECDH_ECDSA_WITH_AES_128_CBC_SHA);
    STRINGIFY_CIPHER(TLS_ECDH_ECDSA_WITH_AES_256_CBC_SHA);
    STRINGIFY_CIPHER(TLS_ECDHE_ECDSA_WITH_NULL_SHA);
    STRINGIFY_CIPHER(TLS_ECDHE_ECDSA_WITH_RC4_128_SHA);
    STRINGIFY_CIPHER(TLS_ECDHE_ECDSA_WITH_3DES_EDE_CBC_SHA);
    STRINGIFY_CIPHER(TLS_ECDHE_ECDSA_WITH_AES_128_CBC_SHA);
    STRINGIFY_CIPHER(TLS_ECDHE_ECDSA_WITH_AES_256_CBC_SHA);
    STRINGIFY_CIPHER(TLS_ECDH_RSA_WITH_NULL_SHA);
    STRINGIFY_CIPHER(TLS_ECDH_RSA_WITH_RC4_128_SHA);
    STRINGIFY_CIPHER(TLS_ECDH_RSA_WITH_3DES_EDE_CBC_SHA);
    STRINGIFY_CIPHER(TLS_ECDH_RSA_WITH_AES_128_CBC_SHA);
    STRINGIFY_CIPHER(TLS_ECDH_RSA_WITH_AES_256_CBC_SHA);
    STRINGIFY_CIPHER(TLS_ECDHE_RSA_WITH_NULL_SHA);
    STRINGIFY_CIPHER(TLS_ECDHE_RSA_WITH_RC4_128_SHA);
    STRINGIFY_CIPHER(TLS_ECDHE_RSA_WITH_3DES_EDE_CBC_SHA);
    STRINGIFY_CIPHER(TLS_ECDHE_RSA_WITH_AES_128_CBC_SHA);
    STRINGIFY_CIPHER(TLS_ECDHE_RSA_WITH_AES_256_CBC_SHA);
    STRINGIFY_CIPHER(TLS_ECDH_anon_WITH_NULL_SHA);
    STRINGIFY_CIPHER(TLS_ECDH_anon_WITH_RC4_128_SHA);
    STRINGIFY_CIPHER(TLS_ECDH_anon_WITH_3DES_EDE_CBC_SHA);
    STRINGIFY_CIPHER(TLS_ECDH_anon_WITH_AES_128_CBC_SHA);
    STRINGIFY_CIPHER(TLS_ECDH_anon_WITH_AES_256_CBC_SHA);
    // STRINGIFY_CIPHER(SSL_NULL_WITH_NULL_NULL);
    STRINGIFY_CIPHER(TLS_NULL_WITH_NULL_NULL);
    // STRINGIFY_CIPHER(SSL_RSA_WITH_NULL_MD5);
    STRINGIFY_CIPHER(TLS_RSA_WITH_NULL_MD5);
    // STRINGIFY_CIPHER(SSL_RSA_WITH_NULL_SHA);
    STRINGIFY_CIPHER(TLS_RSA_WITH_NULL_SHA);
    // STRINGIFY_CIPHER(SSL_RSA_WITH_RC4_128_MD5);
    STRINGIFY_CIPHER(TLS_RSA_WITH_RC4_128_MD5);
    // STRINGIFY_CIPHER(SSL_RSA_WITH_RC4_128_SHA);
    STRINGIFY_CIPHER(TLS_RSA_WITH_RC4_128_SHA);
    // STRINGIFY_CIPHER(SSL_RSA_WITH_3DES_EDE_CBC_SHA);
    STRINGIFY_CIPHER(TLS_RSA_WITH_3DES_EDE_CBC_SHA);
    STRINGIFY_CIPHER(TLS_RSA_WITH_NULL_SHA256);
    STRINGIFY_CIPHER(TLS_RSA_WITH_AES_128_CBC_SHA256);
    STRINGIFY_CIPHER(TLS_RSA_WITH_AES_256_CBC_SHA256);
    // STRINGIFY_CIPHER(SSL_DH_DSS_WITH_3DES_EDE_CBC_SHA);
    STRINGIFY_CIPHER(TLS_DH_DSS_WITH_3DES_EDE_CBC_SHA);
    // STRINGIFY_CIPHER(SSL_DH_RSA_WITH_3DES_EDE_CBC_SHA);
    STRINGIFY_CIPHER(TLS_DH_RSA_WITH_3DES_EDE_CBC_SHA);
    // STRINGIFY_CIPHER(SSL_DHE_DSS_WITH_3DES_EDE_CBC_SHA);
    STRINGIFY_CIPHER(TLS_DHE_DSS_WITH_3DES_EDE_CBC_SHA);
    // STRINGIFY_CIPHER(SSL_DHE_RSA_WITH_3DES_EDE_CBC_SHA);
    STRINGIFY_CIPHER(TLS_DHE_RSA_WITH_3DES_EDE_CBC_SHA);
    STRINGIFY_CIPHER(TLS_DH_DSS_WITH_AES_128_CBC_SHA256);
    STRINGIFY_CIPHER(TLS_DH_RSA_WITH_AES_128_CBC_SHA256);
    STRINGIFY_CIPHER(TLS_DHE_DSS_WITH_AES_128_CBC_SHA256);
    STRINGIFY_CIPHER(TLS_DHE_RSA_WITH_AES_128_CBC_SHA256);
    STRINGIFY_CIPHER(TLS_DH_DSS_WITH_AES_256_CBC_SHA256);
    STRINGIFY_CIPHER(TLS_DH_RSA_WITH_AES_256_CBC_SHA256);
    STRINGIFY_CIPHER(TLS_DHE_DSS_WITH_AES_256_CBC_SHA256);
    STRINGIFY_CIPHER(TLS_DHE_RSA_WITH_AES_256_CBC_SHA256);
    // STRINGIFY_CIPHER(SSL_DH_anon_WITH_RC4_128_MD5);
    STRINGIFY_CIPHER(TLS_DH_anon_WITH_RC4_128_MD5);
    // STRINGIFY_CIPHER(SSL_DH_anon_WITH_3DES_EDE_CBC_SHA);
    STRINGIFY_CIPHER(TLS_DH_anon_WITH_3DES_EDE_CBC_SHA);
    STRINGIFY_CIPHER(TLS_DH_anon_WITH_AES_128_CBC_SHA256);
    STRINGIFY_CIPHER(TLS_DH_anon_WITH_AES_256_CBC_SHA256);
    STRINGIFY_CIPHER(TLS_PSK_WITH_RC4_128_SHA);
    STRINGIFY_CIPHER(TLS_PSK_WITH_3DES_EDE_CBC_SHA);
    STRINGIFY_CIPHER(TLS_PSK_WITH_AES_128_CBC_SHA);
    STRINGIFY_CIPHER(TLS_PSK_WITH_AES_256_CBC_SHA);
    STRINGIFY_CIPHER(TLS_DHE_PSK_WITH_RC4_128_SHA);
    STRINGIFY_CIPHER(TLS_DHE_PSK_WITH_3DES_EDE_CBC_SHA);
    STRINGIFY_CIPHER(TLS_DHE_PSK_WITH_AES_128_CBC_SHA);
    STRINGIFY_CIPHER(TLS_DHE_PSK_WITH_AES_256_CBC_SHA);
    STRINGIFY_CIPHER(TLS_RSA_PSK_WITH_RC4_128_SHA);
    STRINGIFY_CIPHER(TLS_RSA_PSK_WITH_3DES_EDE_CBC_SHA);
    STRINGIFY_CIPHER(TLS_RSA_PSK_WITH_AES_128_CBC_SHA);
    STRINGIFY_CIPHER(TLS_RSA_PSK_WITH_AES_256_CBC_SHA);
    STRINGIFY_CIPHER(TLS_PSK_WITH_NULL_SHA);
    STRINGIFY_CIPHER(TLS_DHE_PSK_WITH_NULL_SHA);
    STRINGIFY_CIPHER(TLS_RSA_PSK_WITH_NULL_SHA);
    STRINGIFY_CIPHER(TLS_RSA_WITH_AES_128_GCM_SHA256);
    STRINGIFY_CIPHER(TLS_RSA_WITH_AES_256_GCM_SHA384);
    STRINGIFY_CIPHER(TLS_DHE_RSA_WITH_AES_128_GCM_SHA256);
    STRINGIFY_CIPHER(TLS_DHE_RSA_WITH_AES_256_GCM_SHA384);
    STRINGIFY_CIPHER(TLS_DH_RSA_WITH_AES_128_GCM_SHA256);
    STRINGIFY_CIPHER(TLS_DH_RSA_WITH_AES_256_GCM_SHA384);
    STRINGIFY_CIPHER(TLS_DHE_DSS_WITH_AES_128_GCM_SHA256);
    STRINGIFY_CIPHER(TLS_DHE_DSS_WITH_AES_256_GCM_SHA384);
    STRINGIFY_CIPHER(TLS_DH_DSS_WITH_AES_128_GCM_SHA256);
    STRINGIFY_CIPHER(TLS_DH_DSS_WITH_AES_256_GCM_SHA384);
    STRINGIFY_CIPHER(TLS_DH_anon_WITH_AES_128_GCM_SHA256);
    STRINGIFY_CIPHER(TLS_DH_anon_WITH_AES_256_GCM_SHA384);
    STRINGIFY_CIPHER(TLS_PSK_WITH_AES_128_GCM_SHA256);
    STRINGIFY_CIPHER(TLS_PSK_WITH_AES_256_GCM_SHA384);
    STRINGIFY_CIPHER(TLS_DHE_PSK_WITH_AES_128_GCM_SHA256);
    STRINGIFY_CIPHER(TLS_DHE_PSK_WITH_AES_256_GCM_SHA384);
    STRINGIFY_CIPHER(TLS_RSA_PSK_WITH_AES_128_GCM_SHA256);
    STRINGIFY_CIPHER(TLS_RSA_PSK_WITH_AES_256_GCM_SHA384);
    STRINGIFY_CIPHER(TLS_PSK_WITH_AES_128_CBC_SHA256);
    STRINGIFY_CIPHER(TLS_PSK_WITH_AES_256_CBC_SHA384);
    STRINGIFY_CIPHER(TLS_PSK_WITH_NULL_SHA256);
    STRINGIFY_CIPHER(TLS_PSK_WITH_NULL_SHA384);
    STRINGIFY_CIPHER(TLS_DHE_PSK_WITH_AES_128_CBC_SHA256);
    STRINGIFY_CIPHER(TLS_DHE_PSK_WITH_AES_256_CBC_SHA384);
    STRINGIFY_CIPHER(TLS_DHE_PSK_WITH_NULL_SHA256);
    STRINGIFY_CIPHER(TLS_DHE_PSK_WITH_NULL_SHA384);
    STRINGIFY_CIPHER(TLS_RSA_PSK_WITH_AES_128_CBC_SHA256);
    STRINGIFY_CIPHER(TLS_RSA_PSK_WITH_AES_256_CBC_SHA384);
    STRINGIFY_CIPHER(TLS_RSA_PSK_WITH_NULL_SHA256);
    STRINGIFY_CIPHER(TLS_RSA_PSK_WITH_NULL_SHA384);
    STRINGIFY_CIPHER(TLS_AES_128_GCM_SHA256);
    STRINGIFY_CIPHER(TLS_AES_256_GCM_SHA384);
    STRINGIFY_CIPHER(TLS_CHACHA20_POLY1305_SHA256);
    STRINGIFY_CIPHER(TLS_AES_128_CCM_SHA256);
    STRINGIFY_CIPHER(TLS_AES_128_CCM_8_SHA256);
    STRINGIFY_CIPHER(TLS_ECDHE_ECDSA_WITH_AES_128_CBC_SHA256);
    STRINGIFY_CIPHER(TLS_ECDHE_ECDSA_WITH_AES_256_CBC_SHA384);
    STRINGIFY_CIPHER(TLS_ECDH_ECDSA_WITH_AES_128_CBC_SHA256);
    STRINGIFY_CIPHER(TLS_ECDH_ECDSA_WITH_AES_256_CBC_SHA384);
    STRINGIFY_CIPHER(TLS_ECDHE_RSA_WITH_AES_128_CBC_SHA256);
    STRINGIFY_CIPHER(TLS_ECDHE_RSA_WITH_AES_256_CBC_SHA384);
    STRINGIFY_CIPHER(TLS_ECDH_RSA_WITH_AES_128_CBC_SHA256);
    STRINGIFY_CIPHER(TLS_ECDH_RSA_WITH_AES_256_CBC_SHA384);
    STRINGIFY_CIPHER(TLS_ECDHE_ECDSA_WITH_AES_128_GCM_SHA256);
    STRINGIFY_CIPHER(TLS_ECDHE_ECDSA_WITH_AES_256_GCM_SHA384);
    STRINGIFY_CIPHER(TLS_ECDH_ECDSA_WITH_AES_128_GCM_SHA256);
    STRINGIFY_CIPHER(TLS_ECDH_ECDSA_WITH_AES_256_GCM_SHA384);
    STRINGIFY_CIPHER(TLS_ECDHE_RSA_WITH_AES_128_GCM_SHA256);
    STRINGIFY_CIPHER(TLS_ECDHE_RSA_WITH_AES_256_GCM_SHA384);
    STRINGIFY_CIPHER(TLS_ECDH_RSA_WITH_AES_128_GCM_SHA256);
    STRINGIFY_CIPHER(TLS_ECDH_RSA_WITH_AES_256_GCM_SHA384);
    STRINGIFY_CIPHER(TLS_ECDHE_RSA_WITH_CHACHA20_POLY1305_SHA256);
    STRINGIFY_CIPHER(TLS_ECDHE_ECDSA_WITH_CHACHA20_POLY1305_SHA256);
    STRINGIFY_CIPHER(TLS_EMPTY_RENEGOTIATION_INFO_SCSV);
    STRINGIFY_CIPHER(SSL_RSA_WITH_RC2_CBC_MD5);
    STRINGIFY_CIPHER(SSL_RSA_WITH_IDEA_CBC_MD5);
    STRINGIFY_CIPHER(SSL_RSA_WITH_DES_CBC_MD5);
    STRINGIFY_CIPHER(SSL_RSA_WITH_3DES_EDE_CBC_MD5);
    STRINGIFY_CIPHER(SSL_NO_SUCH_CIPHERSUITE);
    default:
        ASSERT_NOT_REACHED();
        return emptyString();
    }
ALLOW_DEPRECATED_DECLARATIONS_END

#undef STRINGIFY_CIPHER
}
#endif // HAVE(CFNETWORK_METRICS_APIS_V4)

@interface WKNetworkSessionDelegate : NSObject <NSURLSessionDataDelegate, NSURLSessionWebSocketDelegate> {
    WeakPtr<WebKit::NetworkSessionCocoa> _session;
    WeakPtr<WebKit::SessionWrapper> _sessionWrapper;
@public
    bool _withCredentials;
}

- (id)initWithNetworkSession:(std::reference_wrapper<WebKit::NetworkSessionCocoa>)session wrapper:(WebKit::SessionWrapper&)sessionWrapper withCredentials:(bool)withCredentials;
- (void)sessionInvalidated;

@end

@implementation WKNetworkSessionDelegate

- (id)initWithNetworkSession:(std::reference_wrapper<WebKit::NetworkSessionCocoa>)session wrapper:(WebKit::SessionWrapper&)sessionWrapper withCredentials:(bool)withCredentials
{
    self = [super init];
    if (!self)
        return nil;

    _session = session.get();
    _sessionWrapper = sessionWrapper;
    _withCredentials = withCredentials;

    return self;
}

- (void)sessionInvalidated
{
    _sessionWrapper = nullptr;
}

- (RefPtr<NetworkDataTaskCocoa>)existingTask:(NSURLSessionTask *)task
{
    if (!_sessionWrapper)
        return nullptr;

    if (!task)
        return nullptr;

    return _sessionWrapper->dataTaskMap.get(task.taskIdentifier).get();
}

- (void)URLSession:(NSURLSession *)session didBecomeInvalidWithError:(NSError *)error
{
    ASSERT(!_sessionWrapper);
}

- (void)URLSession:(NSURLSession *)session task:(NSURLSessionTask *)task didSendBodyData:(int64_t)bytesSent totalBytesSent:(int64_t)totalBytesSent totalBytesExpectedToSend:(int64_t)totalBytesExpectedToSend
{
    if (auto networkDataTask = [self existingTask:task])
        networkDataTask->didSendData(totalBytesSent, totalBytesExpectedToSend);
}

- (void)URLSession:(NSURLSession *)session task:(NSURLSessionTask *)task needNewBodyStream:(void (^)(NSInputStream *bodyStream))completionHandler
{
    auto networkDataTask = [self existingTask:task];
    if (!networkDataTask) {
        completionHandler(nil);
        return;
    }

    auto body = networkDataTask->firstRequest().httpBody();
    if (!body) {
        completionHandler(nil);
        return;
    }

    completionHandler(WebCore::createHTTPBodyNSInputStream(body.releaseNonNull()).get());
}

static RetainPtr<NSURLRequest> downgradeRequest(NSURLRequest *request)
{
    auto nsMutableRequest = adoptNS([request mutableCopy]);
    if ([[nsMutableRequest URL].scheme isEqualToString:@"https"]) {
        RetainPtr components = [NSURLComponents componentsWithURL:[nsMutableRequest URL] resolvingAgainstBaseURL:NO];
        components.get().scheme = @"http";
        [nsMutableRequest setURL:components.get().URL];
        ASSERT([[nsMutableRequest URL].scheme isEqualToString:@"http"]);
        return nsMutableRequest;
    }

    ASSERT_NOT_REACHED();
    return request;
}

static bool schemeWasUpgradedDueToDynamicHSTS(NSURLRequest *request)
{
    return [request respondsToSelector:@selector(_schemeWasUpgradedDueToDynamicHSTS)]
        && [request _schemeWasUpgradedDueToDynamicHSTS];
}

static void setIgnoreHSTS(NSMutableURLRequest *request, bool ignoreHSTS)
{
    if ([request respondsToSelector:@selector(_setIgnoreHSTS:)])
        [request _setIgnoreHSTS:ignoreHSTS];
}

static void setIgnoreHSTS(RetainPtr<NSURLRequest>& request, bool shouldIgnoreHSTS)
{
    auto mutableRequest = adoptNS([request mutableCopy]);
    setIgnoreHSTS(mutableRequest.get(), shouldIgnoreHSTS);
    request = mutableRequest;
}

static bool ignoreHSTS(NSURLRequest *request)
{
    return [request respondsToSelector:@selector(_ignoreHSTS)]
        && [request _ignoreHSTS];
}

static void updateIgnoreStrictTransportSecuritySetting(RetainPtr<NSURLRequest>& request, bool shouldIgnoreHSTS)
{
    RetainPtr<NSString> scheme = request.get().URL.scheme;
    if ([scheme isEqualToString:@"https"]) {
        if (shouldIgnoreHSTS && ignoreHSTS(request.get())) {
            // The request was upgraded for some other reason than HSTS.
            // Don't ignore HSTS to avoid the risk of another downgrade.
            setIgnoreHSTS(request, false);
        }
    } else if ([scheme isEqualToString:@"http"]) {
        if (ignoreHSTS(request.get()) != shouldIgnoreHSTS)
            setIgnoreHSTS(request, shouldIgnoreHSTS);
    }
}

- (void)URLSession:(NSURLSession *)session task:(NSURLSessionTask *)task willPerformHTTPRedirection:(NSHTTPURLResponse *)response newRequest:(NSURLRequest *)request completionHandler:(void (^)(NSURLRequest *))completionHandler
{
    auto taskIdentifier = task.taskIdentifier;
    LOG(NetworkSession, "%zu willPerformHTTPRedirection from %s to %s", taskIdentifier, response.URL.absoluteString.UTF8String, request.URL.absoluteString.UTF8String);

    if (RefPtr networkDataTask = [self existingTask:task]) {
        bool shouldIgnoreHSTS = false;
        if (CheckedPtr sessionCocoa = networkDataTask->networkSession()) {
            CheckedPtr storageSession = sessionCocoa->networkProcess().storageSession(sessionCocoa->sessionID());
            RetainPtr firstPartyForCookies = networkDataTask->isTopLevelNavigation() ? request.URL : request.mainDocumentURL;
            shouldIgnoreHSTS = schemeWasUpgradedDueToDynamicHSTS(request)
                && storageSession->shouldBlockCookies(firstPartyForCookies.get(), request.URL, networkDataTask->frameID(), networkDataTask->pageID(), networkDataTask->shouldRelaxThirdPartyCookieBlocking(), NetworkSession::isRequestToKnownCrossSiteTracker(request));
            if (shouldIgnoreHSTS) {
                RetainPtr newRequest = downgradeRequest(request);
                ASSERT([newRequest.get().URL.scheme isEqualToString:@"http"]);
                LOG(NetworkSession, "%zu Downgraded %s from https to http", taskIdentifier, newRequest.get().URL.absoluteString.UTF8String);

                updateIgnoreStrictTransportSecuritySetting(newRequest, shouldIgnoreHSTS);
                completionHandler(newRequest.get());
                return;
            }
        } else
            ASSERT_NOT_REACHED();

        WebCore::ResourceResponse resourceResponse(response);

        networkDataTask->willPerformHTTPRedirection(WTFMove(resourceResponse), request, [completionHandler = makeBlockPtr(completionHandler), taskIdentifier, shouldIgnoreHSTS](auto&& request) {
#if !LOG_DISABLED
            LOG(NetworkSession, "%zu willPerformHTTPRedirection completionHandler (%s)", taskIdentifier, request.url().string().utf8().data());
#else
            UNUSED_PARAM(taskIdentifier);
#endif
            RetainPtr nsRequest = request.nsURLRequest(WebCore::HTTPBodyUpdatePolicy::UpdateHTTPBody);
            updateIgnoreStrictTransportSecuritySetting(nsRequest, shouldIgnoreHSTS);
            completionHandler(nsRequest.get());
        });
    } else if (RefPtr webSocketTask = [self existingWebSocketTask:task]) {
        WebCore::ResourceResponse resourceResponse(response);
        webSocketTask->willPerformHTTPRedirection(WTFMove(resourceResponse), request, [completionHandler = makeBlockPtr(completionHandler), taskIdentifier](auto&& request) {
#if !LOG_DISABLED
            LOG(NetworkSession, "%zu willPerformHTTPRedirection completionHandler (%s)", taskIdentifier, request.url().string().utf8().data());
#else
            UNUSED_PARAM(taskIdentifier);
#endif
            RetainPtr nsRequest = request.nsURLRequest(WebCore::HTTPBodyUpdatePolicy::UpdateHTTPBody);
            completionHandler(nsRequest.get());
        });
    } else {
        LOG(NetworkSession, "%zu willPerformHTTPRedirection completionHandler (nil)", taskIdentifier);
        completionHandler(nil);
    }
}

- (void)URLSession:(NSURLSession *)session task:(NSURLSessionTask*)task _schemeUpgraded:(NSURLRequest*)request completionHandler:(void (^)(NSURLRequest*))completionHandler
{
    auto taskIdentifier = task.taskIdentifier;
    LOG(NetworkSession, "%zu _schemeUpgraded %s", taskIdentifier, request.URL.absoluteString.UTF8String);

    if (RefPtr networkDataTask = [self existingTask:task]) {
        bool shouldIgnoreHSTS = false;
        if (CheckedPtr sessionCocoa = networkDataTask->networkSession()) {
            CheckedPtr storageSession = sessionCocoa->networkProcess().storageSession(sessionCocoa->sessionID());
            shouldIgnoreHSTS = schemeWasUpgradedDueToDynamicHSTS(request)
                && storageSession->shouldBlockCookies(request, networkDataTask->frameID(), networkDataTask->pageID(), networkDataTask->shouldRelaxThirdPartyCookieBlocking(), NetworkSession::isRequestToKnownCrossSiteTracker(request));
            if (shouldIgnoreHSTS) {
                RetainPtr newRequest = downgradeRequest(request);
                ASSERT([newRequest.get().URL.scheme isEqualToString:@"http"]);
                LOG(NetworkSession, "%zu Downgraded %s from https to http", taskIdentifier, newRequest.get().URL.absoluteString.UTF8String);

                updateIgnoreStrictTransportSecuritySetting(newRequest, shouldIgnoreHSTS);
                completionHandler(newRequest.get());
                return;
            }
        } else
            ASSERT_NOT_REACHED();

        WebCore::ResourceResponse synthesizedResponse = WebCore::synthesizeRedirectResponseIfNecessary([task currentRequest], request, nil);
        RetainPtr origin = [request valueForHTTPHeaderField:@"Origin"] ?: @"*";
        synthesizedResponse.setHTTPHeaderField(WebCore::HTTPHeaderName::AccessControlAllowOrigin, origin.get());
        networkDataTask->willPerformHTTPRedirection(WTFMove(synthesizedResponse), request, [completionHandler = makeBlockPtr(completionHandler), taskIdentifier, shouldIgnoreHSTS](auto&& request) {
#if !LOG_DISABLED
            LOG(NetworkSession, "%zu _schemeUpgraded completionHandler (%s)", taskIdentifier, request.url().string().utf8().data());
#else
            UNUSED_PARAM(taskIdentifier);
#endif
            RetainPtr nsRequest = request.nsURLRequest(WebCore::HTTPBodyUpdatePolicy::UpdateHTTPBody);
            updateIgnoreStrictTransportSecuritySetting(nsRequest, shouldIgnoreHSTS);
            completionHandler(nsRequest.get());
        });
    } else {
        LOG(NetworkSession, "%zu _schemeUpgraded completionHandler (nil)", taskIdentifier);
        completionHandler(nil);
    }
}

static inline void processServerTrustEvaluation(NetworkSessionCocoa& session, SessionWrapper& sessionWrapper, NSURLAuthenticationChallenge *challenge, NegotiatedLegacyTLS negotiatedLegacyTLS, NetworkDataTaskCocoa::TaskIdentifier taskIdentifier, NetworkDataTaskCocoa* networkDataTask, CompletionHandler<void(NSURLSessionAuthChallengeDisposition disposition, NSURLCredential *credential)>&& completionHandler)
{
    session.continueDidReceiveChallenge(sessionWrapper, challenge, negotiatedLegacyTLS, taskIdentifier, networkDataTask, [completionHandler = WTFMove(completionHandler), secTrust = retainPtr(challenge.protectionSpace.serverTrust)] (WebKit::AuthenticationChallengeDisposition disposition, const WebCore::Credential& credential) mutable {
        // FIXME: UIProcess should send us back non nil credentials but the credential IPC encoder currently only serializes ns credentials for username/password.
        if (disposition == WebKit::AuthenticationChallengeDisposition::UseCredential && !credential.nsCredential()) {
            completionHandler(NSURLSessionAuthChallengeUseCredential, [NSURLCredential credentialForTrust: secTrust.get()]);
            return;
        }
        completionHandler(toNSURLSessionAuthChallengeDisposition(disposition), RetainPtr { credential.nsCredential() }.get());
    });
}

- (NetworkSessionCocoa*)sessionFromTask:(NSURLSessionTask *)task {
    if (RefPtr networkDataTask = [self existingTask:task])
        return downcast<NetworkSessionCocoa>(networkDataTask->networkSession());

    if (!_sessionWrapper)
        return nullptr;

    if (auto downloadID = _sessionWrapper->downloadMap.getOptional(task.taskIdentifier)) {
        if (CheckedPtr session = _session.get()) {
            if (RefPtr download = session->networkProcess().checkedDownloadManager()->download(*downloadID))
                return downcast<NetworkSessionCocoa>(session->networkProcess().networkSession(download->sessionID()));
        }
        return nullptr;
    }

    if (RefPtr webSocketTask = _sessionWrapper->webSocketDataTaskMap.get(task.taskIdentifier).get())
        return webSocketTask->networkSession();

    return nullptr;
}

void NetworkSessionCocoa::setClientAuditToken(const WebCore::AuthenticationChallenge& challenge)
{
    if (auto auditData = networkProcess().sourceApplicationAuditData())
        SecTrustSetClientAuditToken(RetainPtr { challenge.protectedNSURLAuthenticationChallenge().get().protectionSpace.serverTrust }.get(), auditData.get());

}

static NegotiatedLegacyTLS checkForLegacyTLS(NSURLSessionTaskTransactionMetrics *metrics)
{
    auto tlsVersion = (tls_protocol_version_t)metrics.negotiatedTLSProtocolVersion.unsignedShortValue;
ALLOW_DEPRECATED_DECLARATIONS_BEGIN
    if (tlsVersion == tls_protocol_version_TLSv10 || tlsVersion == tls_protocol_version_TLSv11)
        return NegotiatedLegacyTLS::Yes;
ALLOW_DEPRECATED_DECLARATIONS_END

    switch (metrics.negotiatedTLSCipherSuite.unsignedShortValue) {
    case TLS_ECDH_ECDSA_WITH_3DES_EDE_CBC_SHA:
    case TLS_ECDHE_ECDSA_WITH_3DES_EDE_CBC_SHA:
    case TLS_ECDH_RSA_WITH_3DES_EDE_CBC_SHA:
    case TLS_ECDHE_RSA_WITH_3DES_EDE_CBC_SHA:
    case TLS_ECDH_anon_WITH_3DES_EDE_CBC_SHA:
    case TLS_RSA_WITH_3DES_EDE_CBC_SHA:
    case TLS_DH_DSS_WITH_3DES_EDE_CBC_SHA:
    case TLS_DH_RSA_WITH_3DES_EDE_CBC_SHA:
    case TLS_DHE_DSS_WITH_3DES_EDE_CBC_SHA:
    case TLS_DHE_RSA_WITH_3DES_EDE_CBC_SHA:
    case TLS_DH_anon_WITH_3DES_EDE_CBC_SHA:
    case TLS_PSK_WITH_3DES_EDE_CBC_SHA:
    case TLS_DHE_PSK_WITH_3DES_EDE_CBC_SHA:
    case TLS_RSA_PSK_WITH_3DES_EDE_CBC_SHA:
    case SSL_RSA_WITH_3DES_EDE_CBC_MD5:
        return NegotiatedLegacyTLS::Yes;
    default:
        return NegotiatedLegacyTLS::No;
    }
}

- (void)URLSession:(NSURLSession *)session task:(NSURLSessionTask *)task didReceiveChallenge:(NSURLAuthenticationChallenge *)challenge completionHandler:(void (^)(NSURLSessionAuthChallengeDisposition disposition, NSURLCredential *credential))completionHandler
{
    CheckedPtr sessionCocoa = [self sessionFromTask:task];
    if (!sessionCocoa || [task state] == NSURLSessionTaskStateCanceling) {
        completionHandler(NSURLSessionAuthChallengeCancelAuthenticationChallenge, nil);
        return;
    }

    auto taskIdentifier = task.taskIdentifier;
    LOG(NetworkSession, "%zu didReceiveChallenge", taskIdentifier);
    
    // Proxy authentication is handled by CFNetwork internally. We can get here if the user cancels
    // CFNetwork authentication dialog, and we shouldn't ask the client to display another one in that case.
    if (challenge.protectionSpace.isProxy
#if HAVE(NW_PROXY_CONFIG)
        && sessionCocoa->proxyConfigs().isEmpty()
#endif
        && !sessionCocoa->preventsSystemHTTPProxyAuthentication())
        return completionHandler(NSURLSessionAuthChallengeUseCredential, nil);

    NegotiatedLegacyTLS negotiatedLegacyTLS = NegotiatedLegacyTLS::No;

    if ([challenge.protectionSpace.authenticationMethod isEqualToString:NSURLAuthenticationMethodServerTrust]) {
        sessionCocoa->setClientAuditToken(challenge);

        negotiatedLegacyTLS = checkForLegacyTLS(task._incompleteTaskMetrics.transactionMetrics.lastObject);
        if (negotiatedLegacyTLS == NegotiatedLegacyTLS::Yes && task._preconnect)
            return completionHandler(NSURLSessionAuthChallengeCancelAuthenticationChallenge, nil);

        // Handle server trust evaluation at platform-level if requested, for performance reasons and to use ATS defaults.
        if (sessionCocoa->fastServerTrustEvaluationEnabled() && negotiatedLegacyTLS == NegotiatedLegacyTLS::No) {
            auto networkDataTask = [self existingTask:task];
            if (networkDataTask) {
                RetainPtr<NSURLProtectionSpace> protectionSpace = challenge.protectionSpace;
                networkDataTask->didNegotiateModernTLS(URL { makeString(String(protectionSpace.get().protocol), "://"_s, String(protectionSpace.get().host), ':', protectionSpace.get().port) });
            }
            auto decisionHandler = makeBlockPtr([weakSelf = WeakObjCPtr<WKNetworkSessionDelegate>(self), sessionCocoa = WeakPtr { sessionCocoa.get() }, completionHandler = makeBlockPtr(completionHandler), taskIdentifier, networkDataTask = RefPtr { networkDataTask }, negotiatedLegacyTLS](NSURLAuthenticationChallenge *challenge, OSStatus trustResult) mutable {
                auto strongSelf = weakSelf.get();
                if (!strongSelf || !strongSelf->_sessionWrapper)
                    return completionHandler(NSURLSessionAuthChallengeCancelAuthenticationChallenge, nil);
                auto task = WTFMove(networkDataTask);
                CheckedPtr session = sessionCocoa.get();
                if (trustResult == noErr || !session) {
                    completionHandler(NSURLSessionAuthChallengePerformDefaultHandling, nil);
                    return;
                }
                processServerTrustEvaluation(*session, CheckedRef { *strongSelf->_sessionWrapper }, challenge, negotiatedLegacyTLS, taskIdentifier, task.get(), WTFMove(completionHandler));
            });
            [NSURLSession _strictTrustEvaluate:challenge queue:[NSOperationQueue mainQueue].underlyingQueue completionHandler:decisionHandler.get()];
            return;
        }
    }

    sessionCocoa->continueDidReceiveChallenge(CheckedRef { *_sessionWrapper }, challenge, negotiatedLegacyTLS, taskIdentifier, [self existingTask:task], [completionHandler = makeBlockPtr(completionHandler)] (WebKit::AuthenticationChallengeDisposition disposition, const WebCore::Credential& credential) mutable {
        completionHandler(toNSURLSessionAuthChallengeDisposition(disposition), RetainPtr { credential.nsCredential() }.get());
    });
}

#if HAVE(NETWORK_RESOLUTION_FAILURE_REPORT) && defined(NW_CONNECTION_HAS_FAILED_RESOLUTION_REPORT)

static NSString *description(nw_interface_type_t type)
{
    switch (type) {
    case nw_interface_type_other:
        return @"other";
    case nw_interface_type_wifi:
        return @"wifi";
    case nw_interface_type_cellular:
        return @"cellular";
    case nw_interface_type_wired:
        return @"wired";
    case nw_interface_type_loopback:
        return @"loopback";
    }
    return @"";
}

static NSString *description(nw_dns_failure_reason_t reason)
{
    switch (reason) {
    case nw_dns_failure_reason_unknown:
        return @"unknown";
    case nw_dns_failure_reason_blocked:
        return @"blocked";
    case nw_dns_failure_reason_censored:
        return @"censored";
    case nw_dns_failure_reason_filtered:
        return @"filtered";
    }
    return @"";
}


static NSDictionary<NSString *, id> *extractResolutionReport(NSError *error)
{
    RetainPtr reportValue = (__bridge CFTypeRef)error.userInfo[@"_NSURLErrorNWResolutionReportKey"];
    if (!reportValue)
        return nil;

    RetainPtr pathValue = (__bridge CFTypeRef)error.userInfo[@"_NSURLErrorNWPathKey"];
    if (!pathValue)
        return nil;

    auto interfaces = adoptNS([[NSMutableArray alloc] initWithCapacity:1]);
    if (!interfaces.get())
        return nil;
    nw_path_enumerate_interfaces(static_cast<nw_path_t>(pathValue.get()), ^bool(nw_interface_t interface) {
        String name = String::fromUTF8(nw_interface_get_name(interface));
        [interfaces addObject:@{
            @"type" : description(nw_interface_get_type(interface)),
            @"name" : name.createNSString().get() ?: @"",
        }];
        return true;
    });

    auto report = static_cast<nw_resolution_report_t>(reportValue.get());
    String provider = String::fromUTF8(nw_resolution_report_get_provider_name(report));
    String extraText = String::fromUTF8(nw_resolution_report_get_extended_dns_error_extra_text(report));
    return @{
        @"provider" : provider.createNSString().get() ?: @"",
        @"dnsFailureReason" : description(nw_resolution_report_get_dns_failure_reason(report)),
        @"extendedDNSErrorExtraText" : extraText.createNSString().get() ?: @"",
        @"interfaces" : interfaces.get(),
    };
}

#endif // HAVE(NETWORK_RESOLUTION_FAILURE_REPORT) && defined(NW_CONNECTION_HAS_FAILED_RESOLUTION_REPORT)

- (void)URLSession:(NSURLSession *)session task:(NSURLSessionTask *)task didCompleteWithError:(NSError *)error
{
    LOG(NetworkSession, "%zu didCompleteWithError %@", task.taskIdentifier, error);

    RetainPtr updatedError = error;
    if (updatedError) {
        RetainPtr oldUserInfo = [updatedError userInfo];
        RetainPtr newUserInfo = oldUserInfo ? [NSMutableDictionary dictionaryWithDictionary:oldUserInfo.get()] : [NSMutableDictionary dictionary];
        newUserInfo.get()[@"networkTaskDescription"] = [task description];
        if (auto networkDataTask = [self existingTask:task]) {
            newUserInfo.get()[@"networkTaskMetricsPrivacyStance"] = RetainPtr { privacyStanceToString(networkDataTask->networkLoadMetrics().privacyStance) }.get();
#if HAVE(NETWORK_RESOLUTION_FAILURE_REPORT) && defined(NW_CONNECTION_HAS_FAILED_RESOLUTION_REPORT)
            if (auto report = extractResolutionReport(updatedError.get()))
                newUserInfo.get()[@"networkResolutionReport"] = report;
            else {
                // This can be removed when the CFNetwork loader is no longer in use
                for (NSError *underlyingError in updatedError.get().underlyingErrors) {
                    if (auto report = extractResolutionReport(underlyingError)) {
                        newUserInfo.get()[@"networkResolutionReport"] = report;
                        break;
                    }
                }
            }
#endif
        }
        updatedError = [NSError errorWithDomain:[updatedError domain] code:[updatedError code] userInfo:newUserInfo.get()];
    }

    if (auto networkDataTask = [self existingTask:task])
        networkDataTask->didCompleteWithError(updatedError.get(), networkDataTask->networkLoadMetrics());
    else if (RefPtr webSocketTask = [self existingWebSocketTask:task]) {
        // Even if error is null here, it's an error to close a WebSocket without a close frame which should be received
        // in URLSession:webSocketTask:didCloseWithCode:reason: but this can be hit in some TCP error cases like rdar://110487778
        webSocketTask->didClose(WebCore::ThreadableWebSocketChannel::CloseEventCodeAbnormalClosure, emptyString());
    } else if (updatedError) {
        if (!_sessionWrapper)
            return;
        auto downloadID = _sessionWrapper->downloadMap.takeOptional(task.taskIdentifier);
        if (!downloadID)
            return;
        if (!_session)
            return;
        RefPtr download = CheckedPtr { _session.get() }->networkProcess().checkedDownloadManager()->download(*downloadID);
        if (!download)
            return;

        RetainPtr<NSData> resumeData;
        if (RetainPtr userInfo = dynamic_objc_cast<NSDictionary>(updatedError.get().userInfo)) {
            resumeData = userInfo.get()[@"NSURLSessionDownloadTaskResumeData"];
            if (resumeData && ![resumeData isKindOfClass:[NSData class]]) {
                RELEASE_LOG(NetworkSession, "Download task %llu finished with resume data of wrong class: %s", (unsigned long long)task.taskIdentifier, NSStringFromClass([resumeData class]).UTF8String);
                ASSERT_NOT_REACHED();
                resumeData = nil;
            }
        }

        auto resumeDataReference = span(resumeData.get());
        download->didFail(updatedError.get(), resumeDataReference);
    }
}

- (void)URLSession:(NSURLSession *)session task:(NSURLSessionTask *)task didFinishCollectingMetrics:(NSURLSessionTaskMetrics *)metrics
{
    LOG(NetworkSession, "%zu didFinishCollectingMetrics", task.taskIdentifier);
    if (auto networkDataTask = [self existingTask:task]) {
        RetainPtr<NSArray<NSURLSessionTaskTransactionMetrics *>> transactionMetrics = metrics.transactionMetrics;
        RetainPtr<NSURLSessionTaskTransactionMetrics> m = transactionMetrics.get().lastObject;

        auto dateToMonotonicTime = [] (NSDate *date) {
            if (auto interval = date.timeIntervalSince1970)
                return WallTime::fromRawSeconds(interval).approximateMonotonicTime();
            return MonotonicTime { };
        };

        auto& networkLoadMetrics = networkDataTask->networkLoadMetrics();
        networkLoadMetrics.redirectStart = dateToMonotonicTime(transactionMetrics.get().firstObject.fetchStartDate);
        networkLoadMetrics.fetchStart = dateToMonotonicTime(m.get().fetchStartDate);
        networkLoadMetrics.domainLookupStart = dateToMonotonicTime(m.get().domainLookupStartDate);
        networkLoadMetrics.domainLookupEnd = dateToMonotonicTime(m.get().domainLookupEndDate);
        networkLoadMetrics.connectStart = dateToMonotonicTime(m.get().connectStartDate);
        if (m.get().reusedConnection && [m.get().response.URL.scheme isEqualToString:@"https"])
            networkLoadMetrics.secureConnectionStart = WebCore::reusedTLSConnectionSentinel;
        else
            networkLoadMetrics.secureConnectionStart = dateToMonotonicTime(m.get().secureConnectionStartDate);
        networkLoadMetrics.connectEnd = dateToMonotonicTime(m.get().connectEndDate);
        networkLoadMetrics.requestStart = dateToMonotonicTime(m.get().requestStartDate);
        // Sometimes, likely because of <rdar://90997689>, responseStart is before requestStart. If this happens, use the later of the two.
        networkLoadMetrics.responseStart = std::max(networkLoadMetrics.requestStart, dateToMonotonicTime(m.get().responseStartDate));
        networkLoadMetrics.responseEnd = dateToMonotonicTime(m.get().responseEndDate);
        networkLoadMetrics.markComplete();
        networkLoadMetrics.redirectCount = metrics.redirectCount;
        networkLoadMetrics.protocol = String(m.get().networkProtocolName);
        networkLoadMetrics.cellular = m.get().cellular;
        networkLoadMetrics.expensive = m.get().expensive;
        networkLoadMetrics.constrained = m.get().constrained;
        networkLoadMetrics.multipath = m.get().multipath;
        networkLoadMetrics.isReusedConnection = m.get().isReusedConnection;
        networkLoadMetrics.privacyStance = toPrivacyStance(m.get()._privacyStance);

        if (networkDataTask->shouldCaptureExtraNetworkLoadMetrics()) {
            auto additionalMetrics = WebCore::AdditionalNetworkLoadMetricsForWebInspector::create();
            additionalMetrics->priority = toNetworkLoadPriority(task.priority);

#if HAVE(CFNETWORK_METRICS_APIS_V4)
            if (auto port = [m.get().remotePort unsignedIntValue])
                additionalMetrics->remoteAddress = makeString(String(m.get().remoteAddress), ':', port);
            else
                additionalMetrics->remoteAddress = m.get().remoteAddress;
#else
ALLOW_DEPRECATED_DECLARATIONS_BEGIN
            additionalMetrics->remoteAddress = String(m.get()._remoteAddressAndPort);
ALLOW_DEPRECATED_DECLARATIONS_END
#endif
            additionalMetrics->connectionIdentifier = String([m.get()._connectionIdentifier UUIDString]);

#if HAVE(CFNETWORK_METRICS_APIS_V4)
            additionalMetrics->tlsProtocol = stringForTLSProtocolVersion((tls_protocol_version_t)[m.get().negotiatedTLSProtocolVersion unsignedShortValue]);
            additionalMetrics->tlsCipher = stringForTLSCipherSuite((tls_ciphersuite_t)[m.get().negotiatedTLSCipherSuite unsignedShortValue]);
#else
ALLOW_DEPRECATED_DECLARATIONS_BEGIN
            additionalMetrics->tlsProtocol = stringForSSLProtocol(m.get()._negotiatedTLSProtocol);
            additionalMetrics->tlsCipher = stringForSSLCipher(m.get()._negotiatedTLSCipher);
ALLOW_DEPRECATED_DECLARATIONS_END
#endif

            __block WebCore::HTTPHeaderMap requestHeaders;
            [m.get().request.allHTTPHeaderFields enumerateKeysAndObjectsUsingBlock:^(NSString *name, NSString *value, BOOL *) {
                requestHeaders.set(String(name), String(value));
            }];
            additionalMetrics->requestHeaders = WTFMove(requestHeaders);

            uint64_t requestHeaderBytesSent = 0;
            uint64_t responseHeaderBytesReceived = 0;

            for (NSURLSessionTaskTransactionMetrics *transactionMetrics in metrics.transactionMetrics) {
#if HAVE(CFNETWORK_METRICS_APIS_V4)
                requestHeaderBytesSent += transactionMetrics.countOfRequestHeaderBytesSent;
                responseHeaderBytesReceived += transactionMetrics.countOfResponseHeaderBytesReceived;
#else
ALLOW_DEPRECATED_DECLARATIONS_BEGIN
                requestHeaderBytesSent += transactionMetrics._requestHeaderBytesSent;
                responseHeaderBytesReceived += transactionMetrics._responseHeaderBytesReceived;
ALLOW_DEPRECATED_DECLARATIONS_END
#endif
            }

            additionalMetrics->requestHeaderBytesSent = requestHeaderBytesSent;
            additionalMetrics->requestBodyBytesSent = task.countOfBytesSent;
            additionalMetrics->responseHeaderBytesReceived = responseHeaderBytesReceived;

            additionalMetrics->isProxyConnection = m.get().proxyConnection;

            networkLoadMetrics.additionalNetworkLoadMetricsForWebInspector = WTFMove(additionalMetrics);
        }
#if HAVE(CFNETWORK_METRICS_APIS_V4)
        networkLoadMetrics.responseBodyBytesReceived = m.get().countOfResponseBodyBytesReceived;
        networkLoadMetrics.responseBodyDecodedSize = m.get().countOfResponseBodyBytesAfterDecoding;
#else
ALLOW_DEPRECATED_DECLARATIONS_BEGIN
        networkLoadMetrics.responseBodyBytesReceived = m.get()._responseBodyBytesReceived;
        networkLoadMetrics.responseBodyDecodedSize = m.get()._responseBodyBytesDecoded;
ALLOW_DEPRECATED_DECLARATIONS_END
#endif
        // Sometimes the encoded body bytes received contains a few (3 or so) bytes from the header when there is no body.
        // When this happens, trim our metrics to make more sense.
        if (!networkLoadMetrics.responseBodyDecodedSize)
            networkLoadMetrics.responseBodyBytesReceived = 0;
    }
}

- (void)URLSession:(NSURLSession *)session task:(NSURLSessionTask *)task didReceiveInformationalResponse:(NSHTTPURLResponse *)response
{
    if (auto networkDataTask = [self existingTask:task]) {
        ASSERT(RunLoop::isMain());

        WebCore::ResourceResponse resourceResponse(response);
        networkDataTask->didReceiveInformationalResponse(WTFMove(resourceResponse));
    }
}

// FIXME: Remove when rdar://108002223 can be resolved.
- (void)URLSession:(NSURLSession *)session task:(NSURLSessionTask *)task _didReceiveInformationalResponse:(NSURLResponse *)response
{
    if (RetainPtr httpResponse = dynamic_objc_cast<NSHTTPURLResponse>(response))
        [self URLSession:session task:task didReceiveInformationalResponse:httpResponse.get()];
}

- (void)URLSession:(NSURLSession *)session dataTask:(NSURLSessionDataTask *)dataTask didReceiveResponse:(NSURLResponse *)response completionHandler:(void (^)(NSURLSessionResponseDisposition disposition))completionHandler
{
    auto taskIdentifier = dataTask.taskIdentifier;
    LOG(NetworkSession, "%zu didReceiveResponse", taskIdentifier);
    if (auto networkDataTask = [self existingTask:dataTask]) {
        ASSERT(RunLoop::isMain());

        NegotiatedLegacyTLS negotiatedLegacyTLS = NegotiatedLegacyTLS::No;
        RetainPtr<NSURLSessionTaskMetrics> taskMetrics = dataTask._incompleteTaskMetrics;

        RetainPtr<NSURLSessionTaskTransactionMetrics> metrics = taskMetrics.get().transactionMetrics.lastObject;
        auto privateRelayed = metrics.get()._privacyStance == nw_connection_privacy_stance_direct
            || metrics.get()._privacyStance == nw_connection_privacy_stance_not_eligible
            ? PrivateRelayed::No : PrivateRelayed::Yes;
        String proxyName;
        if (metrics.get()._establishmentReport) {
            if (RetainPtr endpoint = adoptNS(nw_establishment_report_copy_proxy_endpoint(metrics.get()._establishmentReport))) {
                if (const char *hostname = nw_endpoint_get_hostname(endpoint.get()))
                    proxyName = String::fromUTF8(unsafeSpan(hostname));
            }
        }

        negotiatedLegacyTLS = checkForLegacyTLS(metrics.get());

        // Avoid MIME type sniffing if the response comes back as 304 Not Modified.
        RetainPtr httpResponse = dynamic_objc_cast<NSHTTPURLResponse>(response);
        int statusCode = httpResponse ? [httpResponse statusCode] : 0;
        RetainPtr xContentTypeOptions = httpResponse ? [httpResponse valueForHTTPHeaderField:@"X-Content-Type-Options"] : nil;
        bool isNoSniff = xContentTypeOptions && [xContentTypeOptions caseInsensitiveCompare:@"nosniff"] == NSOrderedSame;
        if (statusCode != httpStatus304NotModified) {
            bool isMainResourceLoad = networkDataTask->firstRequest().requester() == WebCore::ResourceRequestRequester::Main;
            WebCore::adjustMIMETypeIfNecessary(response._CFURLResponse, isMainResourceLoad ? WebCore::IsMainResourceLoad::Yes : WebCore::IsMainResourceLoad::No, isNoSniff ? WebCore::IsNoSniffSet::Yes : WebCore::IsNoSniffSet::No);
        }

        WebCore::ResourceResponse resourceResponse(response);
        if (!resourceResponse.url().hasSpecialScheme())
            privateRelayed = PrivateRelayed::No;
        // Lazy initialization is not helpful in the WebKit2 case because we always end up initializing
        // all the fields when sending the response to the WebContent process over IPC.
        resourceResponse.disableLazyInitialization();

        resourceResponse.setDeprecatedNetworkLoadMetrics(WebCore::copyTimingData(taskMetrics.get(), networkDataTask->networkLoadMetrics()));
        resourceResponse.setProxyName(WTFMove(proxyName));
        networkDataTask->didReceiveResponse(WTFMove(resourceResponse), negotiatedLegacyTLS, privateRelayed, [completionHandler = makeBlockPtr(completionHandler), taskIdentifier](WebCore::PolicyAction policyAction) {
#if !LOG_DISABLED
            LOG(NetworkSession, "%zu didReceiveResponse completionHandler (%s)", taskIdentifier, toString(policyAction).characters());
#else
            UNUSED_PARAM(taskIdentifier);
#endif
            completionHandler(toNSURLSessionResponseDisposition(policyAction));
        });
    } else {
        LOG(NetworkSession, "%zu didReceiveResponse completionHandler (cancel)", taskIdentifier);
        completionHandler(NSURLSessionResponseCancel);
    }
}

- (void)URLSession:(NSURLSession *)session dataTask:(NSURLSessionDataTask *)dataTask didReceiveData:(NSData *)data
{
    if (auto networkDataTask = [self existingTask:dataTask])
        networkDataTask->didReceiveData(WebCore::SharedBuffer::create(data));
}

- (void)URLSession:(NSURLSession *)nsSession downloadTask:(NSURLSessionDownloadTask *)downloadTask didFinishDownloadingToURL:(NSURL *)location
{
    if (!_sessionWrapper)
        return;
    auto downloadID = _sessionWrapper->downloadMap.takeOptional([downloadTask taskIdentifier]);
    if (!downloadID)
        return;
    if (CheckedPtr session = _session.get()) {
        if (RefPtr download = session->networkProcess().checkedDownloadManager()->download(*downloadID))
            download->didFinish();
    }
}

- (void)URLSession:(NSURLSession *)nsSession downloadTask:(NSURLSessionDownloadTask *)downloadTask didWriteData:(int64_t)bytesWritten totalBytesWritten:(int64_t)totalBytesWritten totalBytesExpectedToWrite:(int64_t)totalBytesExpectedToWrite
{
    ASSERT_WITH_MESSAGE(![self existingTask:downloadTask], "The NetworkDataTask should be destroyed immediately after didBecomeDownloadTask returns");

    if (!_sessionWrapper)
        return;
    auto downloadID = _sessionWrapper->downloadMap.getOptional([downloadTask taskIdentifier]);
    if (!downloadID)
        return;
    if (CheckedPtr session = _session.get()) {
        if (RefPtr download = session->networkProcess().checkedDownloadManager()->download(*downloadID))
            download->didReceiveData(bytesWritten, totalBytesWritten, totalBytesExpectedToWrite);
    }
}

- (void)URLSession:(NSURLSession *)session dataTask:(NSURLSessionDataTask *)dataTask didBecomeDownloadTask:(NSURLSessionDownloadTask *)downloadTask
{
    auto networkDataTask = [self existingTask:dataTask];
    if (!networkDataTask)
        return;
    CheckedPtr sessionCocoa = networkDataTask->networkSession();
    if (!sessionCocoa)
        return;

    Ref<NetworkDataTaskCocoa> protectedNetworkDataTask(*networkDataTask);
    auto downloadID = *networkDataTask->pendingDownloadID();
    CheckedRef downloadManager = sessionCocoa->networkProcess().downloadManager();
    Ref download = WebKit::Download::create(downloadManager, downloadID, downloadTask, *sessionCocoa, networkDataTask->suggestedFilename());
    networkDataTask->transferSandboxExtensionToDownload(download);
    ASSERT(FileSystem::fileExists(networkDataTask->pendingDownloadLocation()));
    download->didCreateDestination(networkDataTask->pendingDownloadLocation());
    downloadManager->dataTaskBecameDownloadTask(downloadID, WTFMove(download));

    RELEASE_ASSERT(!_sessionWrapper->downloadMap.contains(downloadTask.taskIdentifier));
    _sessionWrapper->downloadMap.add(downloadTask.taskIdentifier, downloadID);
}

- (RefPtr<WebSocketTask>)existingWebSocketTask:(NSURLSessionTask *)task
{
    if (!_sessionWrapper)
        return nullptr;

    if (!task)
        return nullptr;

    return _sessionWrapper->webSocketDataTaskMap.get(task.taskIdentifier).get();
}


- (void)URLSession:(NSURLSession *)session webSocketTask:(NSURLSessionWebSocketTask *)task didOpenWithProtocol:(NSString *) protocol
{
    if (RefPtr webSocketTask = [self existingWebSocketTask:task])
        webSocketTask->didConnect(protocol);
}

- (void)URLSession:(NSURLSession *)session webSocketTask:(NSURLSessionWebSocketTask *)task didCloseWithCode:(NSURLSessionWebSocketCloseCode)closeCode reason:(NSData *)reason
{
    if (RefPtr webSocketTask = [self existingWebSocketTask:task]) {
        // FIXME: We can re-enable ASSERT below once NSURLSession bug rdar://problem/72383646 is fixed.
        // ASSERT([reason isEqualToData:task.closeReason]);
        ASSERT(closeCode == [task closeCode]);
        auto closeReason = adoptNS([[NSString alloc] initWithData:reason encoding:NSUTF8StringEncoding]);
        webSocketTask->didClose(closeCode, closeReason.get());
    }
}

@end

namespace WebKit {

#if ASSERT_ENABLED
static bool sessionsCreated = false;
#endif

static RetainPtr<NSURLSessionConfiguration> configurationForSessionID(PAL::SessionID session, bool isFullWebBrowser)
{
#if HAVE(LOGGING_PRIVACY_LEVEL)
    auto loggingPrivacyLevel = nw_context_privacy_level_sensitive;
#endif

    RetainPtr<NSURLSessionConfiguration> configuration;
    if (session.isEphemeral()) {
        configuration = [NSURLSessionConfiguration ephemeralSessionConfiguration];
#if HAVE(LOGGING_PRIVACY_LEVEL) && defined(NW_CONTEXT_HAS_PRIVACY_LEVEL_SILENT)
        if (isFullWebBrowser)
            loggingPrivacyLevel = nw_context_privacy_level_silent;
#endif
    } else
        configuration = [NSURLSessionConfiguration defaultSessionConfiguration];

#if PLATFORM(MAC)
    bool preventCFNetworkClientCertificateLookup = linkedOnOrAfterSDKWithBehavior(SDKAlignedBehavior::NoClientCertificateLookup) || session.isEphemeral();
#else
    bool preventCFNetworkClientCertificateLookup = true;
#endif
    configuration.get()._shouldSkipPreferredClientCertificateLookup = preventCFNetworkClientCertificateLookup;

#if HAVE(LOGGING_PRIVACY_LEVEL)
    auto setLoggingPrivacyLevel = NSSelectorFromString(@"set_loggingPrivacyLevel:");
    if ([configuration respondsToSelector:setLoggingPrivacyLevel]) {
        wtfObjCMsgSend<void>(configuration.get(), setLoggingPrivacyLevel, loggingPrivacyLevel);
        RELEASE_LOG(NetworkSession, "Setting logging level for %{public}s session %" PRIu64 " to %{public}s", session.isEphemeral() ? "Ephemeral" : "Regular", session.toUInt64(), loggingPrivacyLevel == nw_context_privacy_level_silent ? "silent" : "sensitive");
    }
#elif HAVE(ALLOWS_SENSITIVE_LOGGING)
ALLOW_DEPRECATED_DECLARATIONS_BEGIN
    configuration.get()._allowsSensitiveLogging = NO;
ALLOW_DEPRECATED_DECLARATIONS_END
#endif

#if HAVE(CFNETWORK_NSURLSESSION_CONNECTION_CACHE_LIMITS)
    if (WebCore::ResourceRequest::resourcePrioritiesEnabled()) {
        configuration.get()._connectionCacheNumPriorityLevels = WebCore::resourceLoadPriorityCount;
        configuration.get()._connectionCacheMinimumFastLanePriority = toPlatformRequestPriority(WebCore::ResourceLoadPriority::Medium);
        configuration.get()._connectionCacheNumFastLanes = 1;
    }
#endif

#if ENABLE(NETWORK_ISSUE_REPORTING)
    if ([configuration respondsToSelector:@selector(set_skipsStackTraceCapture:)])
        configuration.get()._skipsStackTraceCapture = YES;
#endif

    return configuration;
}

_NSHSTSStorage *NetworkSessionCocoa::hstsStorage() const
{
    return m_defaultSessionSet->sessionWithCredentialStorage.session.get().configuration._hstsStorage;
}

RetainPtr<_NSHSTSStorage> NetworkSessionCocoa::protectedHSTSStorage() const
{
    return hstsStorage();
}

NSURLCredentialStorage *NetworkSessionCocoa::nsCredentialStorage() const
{
    return m_defaultSessionSet->sessionWithCredentialStorage.session.get().configuration.URLCredentialStorage;
}
    
const String& NetworkSessionCocoa::boundInterfaceIdentifier() const
{
    return m_boundInterfaceIdentifier;
}

const String& NetworkSessionCocoa::sourceApplicationBundleIdentifier() const
{
    return m_sourceApplicationBundleIdentifier;
}

const String& NetworkSessionCocoa::sourceApplicationSecondaryIdentifier() const
{
    return m_sourceApplicationSecondaryIdentifier;
}

#if PLATFORM(IOS_FAMILY)
const String& NetworkSessionCocoa::dataConnectionServiceType() const
{
    return m_dataConnectionServiceType;
}
#endif

std::unique_ptr<NetworkSession> NetworkSessionCocoa::create(NetworkProcess& networkProcess, const NetworkSessionCreationParameters& parameters)
{
    return makeUnique<NetworkSessionCocoa>(networkProcess, parameters);
}

static RetainPtr<NSDictionary> proxyDictionary(const URL& httpProxy, const URL& httpsProxy)
{
    if (!httpProxy.isValid() && !httpsProxy.isValid())
        return nil;

ALLOW_DEPRECATED_DECLARATIONS_BEGIN

    auto dictionary = adoptNS([[NSMutableDictionary alloc] init]);
    if (httpProxy.isValid()) {
        [dictionary setObject:httpProxy.host().createNSString().get() forKey:bridge_cast(kCFStreamPropertyHTTPProxyHost)];
        if (auto port = httpProxy.port())
            [dictionary setObject: RetainPtr { @(*port) }.get() forKey:bridge_cast(kCFStreamPropertyHTTPProxyPort)];
    }
    if (httpsProxy.isValid()) {
        [dictionary setObject:httpsProxy.host().createNSString().get() forKey:bridge_cast(kCFStreamPropertyHTTPSProxyHost)];
        if (auto port = httpsProxy.port())
            [dictionary setObject: RetainPtr { @(*port) }.get() forKey:bridge_cast(kCFStreamPropertyHTTPSProxyPort)];
    }
    return dictionary;

ALLOW_DEPRECATED_DECLARATIONS_END
}

void SessionWrapper::recreateSessionWithUpdatedProxyConfigurations(NetworkSessionCocoa& networkSession)
{
    RELEASE_ASSERT(session);
    RELEASE_ASSERT(delegate);

    auto withCredentials = delegate->_withCredentials;
    RetainPtr<NSURLSessionConfiguration> configuration = session.get().configuration;

#if HAVE(NW_PROXY_CONFIG)
    networkSession.applyProxyConfigurationToSessionConfiguration(configuration.get());
#endif

    [session invalidateAndCancel];
    [delegate sessionInvalidated];
    delegate = adoptNS([[WKNetworkSessionDelegate alloc] initWithNetworkSession:networkSession wrapper:*this withCredentials:withCredentials]);
    session = [NSURLSession sessionWithConfiguration:configuration.get() delegate:delegate.get() delegateQueue:[NSOperationQueue mainQueue]];

    dataTaskMap.clear();
    downloadMap.clear();
    webSocketDataTaskMap.clear();
}

void SessionWrapper::initialize(NSURLSessionConfiguration *configuration, NetworkSessionCocoa& networkSession, WebCore::StoredCredentialsPolicy storedCredentialsPolicy, NavigatingToAppBoundDomain isNavigatingToAppBoundDomain)
{
    UNUSED_PARAM(isNavigatingToAppBoundDomain);

    // FIXME: The following `isParentProcessAFullWebBrowser` check is inaccurate in Safari on macOS.
    auto isFullBrowser = isParentProcessAFullWebBrowser(networkSession.networkProcess());
#if PLATFORM(MAC)
    isFullBrowser = WTF::MacApplication::isSafari();
#endif
    if (!configuration._sourceApplicationSecondaryIdentifier && isFullBrowser)
        configuration._sourceApplicationSecondaryIdentifier = @"com.apple.WebKit.InAppBrowser";

#if HAVE(NW_PROXY_CONFIG)
    networkSession.applyProxyConfigurationToSessionConfiguration(configuration);
#endif

    delegate = adoptNS([[WKNetworkSessionDelegate alloc] initWithNetworkSession:networkSession wrapper:*this withCredentials:storedCredentialsPolicy == WebCore::StoredCredentialsPolicy::Use]);
    session = [NSURLSession sessionWithConfiguration:configuration delegate:delegate.get() delegateQueue:[NSOperationQueue mainQueue]];
}

NetworkSessionCocoa::NetworkSessionCocoa(NetworkProcess& networkProcess, const NetworkSessionCreationParameters& parameters)
    : NetworkSession(networkProcess, parameters)
    , m_defaultSessionSet(SessionSet::create())
    , m_boundInterfaceIdentifier(parameters.boundInterfaceIdentifier)
    , m_sourceApplicationBundleIdentifier(parameters.sourceApplicationBundleIdentifier)
    , m_sourceApplicationSecondaryIdentifier(parameters.sourceApplicationSecondaryIdentifier)
    , m_proxyConfiguration(parameters.proxyConfiguration)
    , m_shouldLogCookieInformation(parameters.shouldLogCookieInformation)
    , m_fastServerTrustEvaluationEnabled(parameters.fastServerTrustEvaluationEnabled)
    , m_dataConnectionServiceType(parameters.dataConnectionServiceType)
    , m_preventsSystemHTTPProxyAuthentication(parameters.preventsSystemHTTPProxyAuthentication)
    , m_isLegacyTLSAllowed(parameters.isLegacyTLSAllowed)
{
    ASSERT(hasProcessPrivilege(ProcessPrivilege::CanAccessRawCookies));

#if ASSERT_ENABLED
    sessionsCreated = true;
#endif

    RetainPtr configuration = configurationForSessionID(m_sessionID, networkProcess.isParentProcessFullWebBrowserOrRunningTest());

    if (!m_sessionID.isEphemeral())
        m_blobRegistry.setFileDirectory(FileSystem::createTemporaryDirectory(@"BlobRegistryFiles"));

    if (!!parameters.hstsStorageDirectory && !m_sessionID.isEphemeral()) {
        SandboxExtension::consumePermanently(parameters.hstsStorageDirectoryExtensionHandle);
        configuration.get()._hstsStorage = adoptNS([[_NSHSTSStorage alloc] initPersistentStoreWithURL:adoptNS([[NSURL alloc] initFileURLWithPath:parameters.hstsStorageDirectory.createNSString().get() isDirectory:YES]).get()]).get();
    }

#if HAVE(CFNETWORK_SEPARATE_CREDENTIAL_STORAGE)
    if (parameters.dataStoreIdentifier && !m_sessionID.isEphemeral())
        configuration.get().URLCredentialStorage = adoptNS([[NSURLCredentialStorage alloc] _initWithIdentifier:parameters.dataStoreIdentifier->toString().createNSString().get() private:NO]).get();
#endif

#if HAVE(NETWORK_LOADER)
    if (parameters.useNetworkLoader) {
        RELEASE_LOG_IF(*parameters.useNetworkLoader, NetworkSession, "Using experimental network loader.");
// FIXME: rdar://152673570 Stop using `_usesNWLoader` as it is deprecated
ALLOW_DEPRECATED_DECLARATIONS_BEGIN
        configuration.get()._usesNWLoader = *parameters.useNetworkLoader;
ALLOW_DEPRECATED_DECLARATIONS_END
    }
#endif

    if (parameters.allowsHSTSWithUntrustedRootCertificate && [configuration respondsToSelector:@selector(_allowsHSTSWithUntrustedRootCertificate)])
        configuration.get()._allowsHSTSWithUntrustedRootCertificate = YES;
    
#if HAVE(APP_SSO) || PLATFORM(MACCATALYST)
    configuration.get()._preventsAppSSO = true;
#endif

    // Without this, CFNetwork would sometimes add a Content-Type header to our requests (rdar://problem/34748470).
    configuration.get()._suppressedAutoAddedHTTPHeaders = [NSSet setWithObject:@"Content-Type"];

    if (parameters.allowsCellularAccess == AllowsCellularAccess::No)
        configuration.get().allowsCellularAccess = NO;

    // The WebKit network cache was already queried.
    configuration.get().URLCache = nil;

    if (auto data = networkProcess.sourceApplicationAuditData())
        configuration.get()._sourceApplicationAuditTokenData = (__bridge NSData *)data.get();

    if (!m_sourceApplicationBundleIdentifier.isEmpty()) {
        configuration.get()._sourceApplicationBundleIdentifier = m_sourceApplicationBundleIdentifier.createNSString().get();
        configuration.get()._sourceApplicationAuditTokenData = nil;
    }

    if (!m_sourceApplicationSecondaryIdentifier.isEmpty())
        configuration.get()._sourceApplicationSecondaryIdentifier = m_sourceApplicationSecondaryIdentifier.createNSString().get();

#if HAVE(ALTERNATIVE_SERVICE)
    if (!parameters.alternativeServiceDirectory.isEmpty()) {
        SandboxExtension::consumePermanently(parameters.alternativeServiceDirectoryExtensionHandle);
        configuration.get()._alternativeServicesStorage = adoptNS([[_NSHTTPAlternativeServicesStorage alloc] initPersistentStoreWithURL:[adoptNS([[NSURL alloc] initFileURLWithPath:parameters.alternativeServiceDirectory.createNSString().get() isDirectory:YES]) URLByAppendingPathComponent:@"AlternativeService.sqlite" isDirectory:NO]]).get();
        if ([configuration.get()._alternativeServicesStorage respondsToSelector:@selector(setCanSuspendLocked:)])
            [configuration.get()._alternativeServicesStorage setCanSuspendLocked:YES];
    }
#endif

    configuration.get()._preventsSystemHTTPProxyAuthentication = parameters.preventsSystemHTTPProxyAuthentication;
    configuration.get()._requiresSecureHTTPSProxyConnection = parameters.requiresSecureHTTPSProxyConnection;
    configuration.get().connectionProxyDictionary = RetainPtr { (NSDictionary *)parameters.proxyConfiguration.get()?: proxyDictionary(parameters.httpProxy, parameters.httpsProxy) }.get();

#if PLATFORM(IOS_FAMILY)
    if (!m_dataConnectionServiceType.isEmpty())
        configuration.get()._CTDataConnectionServiceType = m_dataConnectionServiceType.createNSString().get();
#endif

#if ENABLE(LEGACY_CUSTOM_PROTOCOL_MANAGER)
    networkProcess.protectedSupplement<LegacyCustomProtocolManager>()->registerProtocolClass(configuration.get());
#endif

    configuration.get()._timingDataOptions = _TimingDataOptionsEnableW3CNavigationTiming;

    // FIXME: Replace @"kCFStreamPropertyAutoErrorOnSystemChange" with a constant from the SDK once rdar://problem/40650244 is in a build.
    if (parameters.suppressesConnectionTerminationOnSystemChange)
        configuration.get()._socketStreamProperties = @{ @"kCFStreamPropertyAutoErrorOnSystemChange" : @NO };

#if PLATFORM(WATCHOS)
    configuration.get()._companionProxyPreference = NSURLSessionCompanionProxyPreferencePreferDirectToCloud;
#endif

    CheckedPtr storageSession = networkProcess.storageSession(parameters.sessionID);
    RELEASE_ASSERT(storageSession);

    RetainPtr<NSHTTPCookieStorage> cookieStorage;
    if (CFHTTPCookieStorageRef storage = storageSession->cookieStorage().get()) {
        cookieStorage = adoptNS([[NSHTTPCookieStorage alloc] _initWithCFHTTPCookieStorage:storage]);
        configuration.get().HTTPCookieStorage = cookieStorage.get();
    } else
        cookieStorage = storageSession->nsCookieStorage();

    cookieStorage.get()._overrideSessionCookieAcceptPolicy = YES;

#if ENABLE(TLS_1_2_DEFAULT_MINIMUM)
    if (m_isLegacyTLSAllowed) {
ALLOW_DEPRECATED_DECLARATIONS_BEGIN
        configuration.get().TLSMinimumSupportedProtocolVersion = tls_protocol_version_TLSv10;
ALLOW_DEPRECATED_DECLARATIONS_END
    } else
        configuration.get().TLSMinimumSupportedProtocolVersion = tls_protocol_version_TLSv12;
#endif

    initializeNSURLSessionsInSet(m_defaultSessionSet.get(), configuration.get());

    m_deviceManagementRestrictionsEnabled = parameters.deviceManagementRestrictionsEnabled;
    m_allLoadsBlockedByDeviceManagementRestrictionsForTesting = parameters.allLoadsBlockedByDeviceManagementRestrictionsForTesting;

#if ENABLE(APP_BOUND_DOMAINS)
    if (RefPtr resourceLoadStatistics = m_resourceLoadStatistics; resourceLoadStatistics && !parameters.resourceLoadStatisticsParameters.appBoundDomains.isEmpty())
        resourceLoadStatistics->setAppBoundDomains(HashSet<WebCore::RegistrableDomain> { parameters.resourceLoadStatisticsParameters.appBoundDomains }, [] { });
#endif

#if ENABLE(MANAGED_DOMAINS)
    if (RefPtr resourceLoadStatistics = m_resourceLoadStatistics; resourceLoadStatistics && !parameters.resourceLoadStatisticsParameters.managedDomains.isEmpty())
        resourceLoadStatistics->setManagedDomains(HashSet<WebCore::RegistrableDomain> { parameters.resourceLoadStatisticsParameters.managedDomains }, [] { });
#endif

#if HAVE(NW_PROXY_CONFIG)
    if (parameters.proxyConfigData)
        setProxyConfigData(WTFMove(*parameters.proxyConfigData));
#endif
}

NetworkSessionCocoa::~NetworkSessionCocoa()
{
    notifyAdAttributionKitOfSessionTermination();
}

void NetworkSessionCocoa::notifyAdAttributionKitOfSessionTermination()
{
    // FIXME: Ad attribution stuff should be pulled to a separate object that has the same lifetime as
    // NetworkSessionCocoa, perhaps owned by NetworkSessionCocoa. It is separate from the network responsibilities.
#if HAVE(AD_ATTRIBUTION_KIT_PRIVATE_BROWSING)
    if (m_donatedEphemeralImpressionSessionID) {
        // FIXME: Remove this respondsToSelector check in 2026 or so.
        if ([ASDInstallWebAttributionService.sharedInstance respondsToSelector:@selector(removeInstallWebAttributionParamsFromPrivateBrowsingSessionID:completionHandler:)])
            [ASDInstallWebAttributionService.sharedInstance removeInstallWebAttributionParamsFromPrivateBrowsingSessionID:m_donatedEphemeralImpressionSessionID->createNSUUID().get() completionHandler:^(NSError *) { }];
    }
#endif
}

void NetworkSessionCocoa::initializeNSURLSessionsInSet(SessionSet& sessionSet, NSURLSessionConfiguration *configuration)
{
    sessionSet.checkedSessionWithCredentialStorage()->initialize(configuration, *this, WebCore::StoredCredentialsPolicy::Use, NavigatingToAppBoundDomain::No);
    auto cookieAcceptPolicy = configuration.HTTPCookieStorage.cookieAcceptPolicy;
    LOG(NetworkSession, "Created NetworkSession with cookieAcceptPolicy %lu", cookieAcceptPolicy);
    RELEASE_LOG_IF(cookieAcceptPolicy == NSHTTPCookieAcceptPolicyNever, NetworkSession, "Creating network session with ID %" PRIu64 " that will not accept cookies.", m_sessionID.toUInt64());
}

SessionSet& NetworkSessionCocoa::sessionSetForPage(std::optional<WebPageProxyIdentifier> webPageProxyID)
{
    RefPtr sessionSet = webPageProxyID ? m_perPageSessionSets.get(*webPageProxyID) : nullptr;
    return sessionSet ? *sessionSet : m_defaultSessionSet.get();
}

const SessionSet& NetworkSessionCocoa::sessionSetForPage(std::optional<WebPageProxyIdentifier> webPageProxyID) const
{
    RefPtr sessionSet = webPageProxyID ? m_perPageSessionSets.get(*webPageProxyID) : nullptr;
    return sessionSet ? *sessionSet : m_defaultSessionSet.get();
}

SessionWrapper& NetworkSessionCocoa::initializeEphemeralStatelessSessionIfNeeded(std::optional<WebPageProxyIdentifier> webPageProxyID, NavigatingToAppBoundDomain isNavigatingToAppBoundDomain)
{
    return protectedSessionSetForPage(webPageProxyID)->initializeEphemeralStatelessSessionIfNeeded(isNavigatingToAppBoundDomain, *this);
}

SessionWrapper& SessionSet::initializeEphemeralStatelessSessionIfNeeded(NavigatingToAppBoundDomain isNavigatingToAppBoundDomain, NetworkSessionCocoa& session)
{
    if (ephemeralStatelessSession.session)
        return ephemeralStatelessSession;

    RetainPtr configuration = [NSURLSessionConfiguration ephemeralSessionConfiguration];
    RetainPtr<NSURLSessionConfiguration> existingConfiguration = sessionWithCredentialStorage.session.get().configuration;

    configuration.get().HTTPCookieAcceptPolicy = NSHTTPCookieAcceptPolicyNever;
    configuration.get().URLCredentialStorage = nil;
    configuration.get().URLCache = nil;
    configuration.get().allowsCellularAccess = existingConfiguration.get().allowsCellularAccess;
    configuration.get().connectionProxyDictionary = existingConfiguration.get().connectionProxyDictionary;

#if ENABLE(TLS_1_2_DEFAULT_MINIMUM)
    if (session.isLegacyTLSAllowed())
        configuration.get().TLSMinimumSupportedProtocolVersion = existingConfiguration.get().TLSMinimumSupportedProtocolVersion;
    else
        configuration.get().TLSMinimumSupportedProtocolVersion = tls_protocol_version_TLSv12;
#endif

    configuration.get()._shouldSkipPreferredClientCertificateLookup = YES;
    configuration.get()._sourceApplicationAuditTokenData = existingConfiguration.get()._sourceApplicationAuditTokenData;
    configuration.get()._sourceApplicationSecondaryIdentifier = existingConfiguration.get()._sourceApplicationSecondaryIdentifier;
#if PLATFORM(IOS_FAMILY)
    configuration.get()._CTDataConnectionServiceType = existingConfiguration.get()._CTDataConnectionServiceType;
#endif

    checkedEphemeralStatelessSession()->initialize(configuration.get(), session, WebCore::StoredCredentialsPolicy::EphemeralStateless, isNavigatingToAppBoundDomain);

    return ephemeralStatelessSession;
}

SessionWrapper& NetworkSessionCocoa::sessionWrapperForTask(std::optional<WebPageProxyIdentifier> webPageProxyID, const WebCore::ResourceRequest& request, WebCore::StoredCredentialsPolicy storedCredentialsPolicy, std::optional<NavigatingToAppBoundDomain> isNavigatingToAppBoundDomain)
{
    auto shouldBeConsideredAppBound = isNavigatingToAppBoundDomain ? *isNavigatingToAppBoundDomain : NavigatingToAppBoundDomain::Yes;
    // FIXME: The following `isParentProcessAFullWebBrowser` check is inaccurate in Safari on macOS.
    if (isParentProcessAFullWebBrowser(networkProcess()))
        shouldBeConsideredAppBound = NavigatingToAppBoundDomain::No;

    if (CheckedPtr storageSession = networkStorageSession()) {
        auto firstParty = WebCore::RegistrableDomain(request.firstPartyForCookies());
        if (storageSession->shouldBlockThirdPartyCookiesButKeepFirstPartyCookiesFor(firstParty))
            return protectedSessionSetForPage(webPageProxyID)->isolatedSession(storedCredentialsPolicy, firstParty, shouldBeConsideredAppBound, *this);
    } else
        ASSERT_NOT_REACHED();

#if ENABLE(APP_BOUND_DOMAINS)
    if (shouldBeConsideredAppBound == NavigatingToAppBoundDomain::Yes)
        return appBoundSession(webPageProxyID, storedCredentialsPolicy);
#endif

    switch (storedCredentialsPolicy) {
    case WebCore::StoredCredentialsPolicy::Use:
    case WebCore::StoredCredentialsPolicy::DoNotUse:
        return sessionSetForPage(webPageProxyID).sessionWithCredentialStorage;
    case WebCore::StoredCredentialsPolicy::EphemeralStateless:
        return initializeEphemeralStatelessSessionIfNeeded(webPageProxyID, NavigatingToAppBoundDomain::No);
    }
}

#if ENABLE(APP_BOUND_DOMAINS)
SessionWrapper& NetworkSessionCocoa::appBoundSession(std::optional<WebPageProxyIdentifier> webPageProxyID, WebCore::StoredCredentialsPolicy storedCredentialsPolicy)
{
    auto& sessionSet = sessionSetForPage(webPageProxyID);
    
    if (!sessionSet.appBoundSession) {
        sessionSet.appBoundSession = makeUnique<IsolatedSession>();
        sessionSet.appBoundSession->checkedSessionWithCredentialStorage()->initialize(sessionSet.sessionWithCredentialStorage.session.get().configuration, *this, WebCore::StoredCredentialsPolicy::Use, NavigatingToAppBoundDomain::Yes);
    }

    auto& sessionWrapper = [&](auto storedCredentialsPolicy) -> SessionWrapper& {
        switch (storedCredentialsPolicy) {
        case WebCore::StoredCredentialsPolicy::Use:
        case WebCore::StoredCredentialsPolicy::DoNotUse:
            LOG(NetworkSession, "Using app-bound NSURLSession.");
            return sessionSet.appBoundSession->sessionWithCredentialStorage;
        case WebCore::StoredCredentialsPolicy::EphemeralStateless:
            return initializeEphemeralStatelessSessionIfNeeded(webPageProxyID, NavigatingToAppBoundDomain::Yes);
        }
    } (storedCredentialsPolicy);

    return sessionWrapper;
}

bool NetworkSessionCocoa::hasAppBoundSession() const
{
    if (!!m_defaultSessionSet->appBoundSession)
        return true;
    for (auto& sessionSet : m_perPageSessionSets.values()) {
        if (!!sessionSet->appBoundSession)
            return true;
    }
    
    return false;
}

void NetworkSessionCocoa::clearAppBoundSession()
{
    m_defaultSessionSet->appBoundSession = nullptr;
    for (auto& sessionSet : m_perPageSessionSets.values())
        sessionSet->appBoundSession = nullptr;
}
#endif

SessionWrapper& NetworkSessionCocoa::isolatedSession(WebPageProxyIdentifier webPageProxyID, WebCore::StoredCredentialsPolicy storedCredentialsPolicy, const WebCore::RegistrableDomain& firstPartyDomain, NavigatingToAppBoundDomain isNavigatingToAppBoundDomain)
{
    return protectedSessionSetForPage(webPageProxyID)->isolatedSession(storedCredentialsPolicy, firstPartyDomain, isNavigatingToAppBoundDomain, *this);
}

SessionWrapper& SessionSet::isolatedSession(WebCore::StoredCredentialsPolicy storedCredentialsPolicy, const WebCore::RegistrableDomain& firstPartyDomain, NavigatingToAppBoundDomain isNavigatingToAppBoundDomain, NetworkSessionCocoa& session)
{
    auto& entry = isolatedSessions.ensure(firstPartyDomain, [this, &session, isNavigatingToAppBoundDomain] {
        auto newEntry = makeUnique<IsolatedSession>();
        newEntry->checkedSessionWithCredentialStorage()->initialize(sessionWithCredentialStorage.session.get().configuration, session, WebCore::StoredCredentialsPolicy::Use, isNavigatingToAppBoundDomain);
        return newEntry;
    }).iterator->value;

    entry->lastUsed = WallTime::now();

    CheckedRef sessionWrapper = [this, protectedThis = Ref { *this }, &entry, isNavigatingToAppBoundDomain, session = CheckedRef { session }] (auto storedCredentialsPolicy) -> SessionWrapper& {
        switch (storedCredentialsPolicy) {
        case WebCore::StoredCredentialsPolicy::Use:
        case WebCore::StoredCredentialsPolicy::DoNotUse:
            LOG(NetworkSession, "Using isolated NSURLSession.");
            return entry->sessionWithCredentialStorage;
        case WebCore::StoredCredentialsPolicy::EphemeralStateless:
            return initializeEphemeralStatelessSessionIfNeeded(isNavigatingToAppBoundDomain, session);
        }
    } (storedCredentialsPolicy);

    if (isolatedSessions.size() > maxNumberOfIsolatedSessions) {
        WebCore::RegistrableDomain keyToRemove;
        auto oldestTimestamp = WallTime::now();
        for (auto& key : isolatedSessions.keys()) {
            auto timestamp = isolatedSessions.get(key)->lastUsed;
            if (timestamp < oldestTimestamp) {
                oldestTimestamp = timestamp;
                keyToRemove = key;
            }
        }
        LOG(NetworkSession, "About to remove isolated NSURLSession.");
        isolatedSessions.remove(keyToRemove);
    }

    RELEASE_ASSERT(isolatedSessions.size() <= maxNumberOfIsolatedSessions);

    return sessionWrapper;
}

bool NetworkSessionCocoa::hasIsolatedSession(const WebCore::RegistrableDomain& domain) const
{
    if (m_defaultSessionSet->isolatedSessions.contains(domain))
        return true;
    for (auto& sessionSet : m_perPageSessionSets.values()) {
        if (sessionSet->isolatedSessions.contains(domain))
            return true;
    }
    
    return false;
}

void NetworkSessionCocoa::clearIsolatedSessions()
{
    m_defaultSessionSet->isolatedSessions.clear();
    for (auto& sessionSet : m_perPageSessionSets.values())
        sessionSet->isolatedSessions.clear();
}

void NetworkSessionCocoa::invalidateAndCancelSessionSet(SessionSet& sessionSet)
{
    [sessionSet.sessionWithCredentialStorage.session invalidateAndCancel];
    [sessionSet.ephemeralStatelessSession.session invalidateAndCancel];
    [sessionSet.sessionWithCredentialStorage.delegate sessionInvalidated];
    [sessionSet.ephemeralStatelessSession.delegate sessionInvalidated];

    for (auto& session : sessionSet.isolatedSessions.values()) {
        [session->sessionWithCredentialStorage.session invalidateAndCancel];
        [session->sessionWithCredentialStorage.delegate sessionInvalidated];
    }
    sessionSet.isolatedSessions.clear();

    if (sessionSet.appBoundSession) {
        [sessionSet.appBoundSession->sessionWithCredentialStorage.session invalidateAndCancel];
        [sessionSet.appBoundSession->sessionWithCredentialStorage.delegate sessionInvalidated];
    }
}

void NetworkSessionCocoa::invalidateAndCancel()
{
    NetworkSession::invalidateAndCancel();

    invalidateAndCancelSessionSet(m_defaultSessionSet.get());
    for (auto& sessionSet : m_perPageSessionSets.values())
        invalidateAndCancelSessionSet(sessionSet.get());
}

HashSet<WebCore::SecurityOriginData> NetworkSessionCocoa::originsWithCredentials()
{
    RetainPtr credentialStorage = nsCredentialStorage();
    if (!credentialStorage)
        return { };

    bool shouldHandleSessionCredentialsOnly = credentialStorage.get() == [NSURLCredentialStorage sharedCredentialStorage];
    HashSet<WebCore::SecurityOriginData> origins;
    RetainPtr credentials = [credentialStorage allCredentials];
    for (NSURLProtectionSpace *space in credentials.get()) {
        for (NSURLCredential *credential in [credentials.get()[space] allValues]) {
            if (!shouldHandleSessionCredentialsOnly || credential.persistence == NSURLCredentialPersistenceForSession) {
                origins.add(WebCore::SecurityOriginData { String(space.protocol), String(space.host), space.port });
                break;
            }
        }
    }
    return origins;
}

void NetworkSessionCocoa::removeCredentialsForOrigins(const Vector<WebCore::SecurityOriginData>& origins)
{
    RetainPtr credentialStorage = nsCredentialStorage();
    if (!credentialStorage)
        return;

    HashSet<WebCore::SecurityOriginData> originSet;
    for (auto& origin : origins) {
        if (!origin.isNull() && !origin.isOpaque())
            originSet.add(origin);
    }

    bool shouldHandleSessionCredentialsOnly = credentialStorage.get() == [NSURLCredentialStorage sharedCredentialStorage];
    RetainPtr credentials = [credentialStorage allCredentials];
    for (NSURLProtectionSpace *space in credentials.get()) {
        for (NSURLCredential *credential in [credentials.get()[space] allValues]) {
            if (shouldHandleSessionCredentialsOnly && credential.persistence != NSURLCredentialPersistenceForSession)
                continue;
            auto origin = WebCore::SecurityOriginData { String(space.protocol), String(space.host), space.port };
            if (originSet.contains(origin))
                [credentialStorage removeCredential:credential forProtectionSpace:space];
        }
    }
}

void NetworkSessionCocoa::clearCredentials(WallTime modifiedSince)
{
    RetainPtr credentialStorage = nsCredentialStorage();
    if (!credentialStorage)
        return;

    bool useSharedCredentialStorage = credentialStorage.get() == [NSURLCredentialStorage sharedCredentialStorage];
    bool shouldHandleSessionCredentialsOnly = useSharedCredentialStorage || (modifiedSince.secondsSinceEpoch().value() > 0.0);
    RetainPtr credentials = [credentialStorage allCredentials];
    for (NSURLProtectionSpace *space in credentials.get()) {
        for (NSURLCredential *credential in [credentials.get()[space] allValues]) {
            if (shouldHandleSessionCredentialsOnly && credential.persistence != NSURLCredentialPersistenceForSession)
                continue;
            [credentialStorage removeCredential:credential forProtectionSpace:space];
        }
    }
}

static CompletionHandler<void(WebKit::AuthenticationChallengeDisposition disposition, const WebCore::Credential& credential)> createChallengeCompletionHandler(Ref<NetworkProcess>&& networkProcess, PAL::SessionID sessionID,  const WebCore::AuthenticationChallenge& challenge, const String& partition, uint64_t taskIdentifier, CompletionHandler<void(WebKit::AuthenticationChallengeDisposition, const WebCore::Credential&)>&& completionHandler)
 {
    WebCore::AuthenticationChallenge authenticationChallenge { challenge };
    return [completionHandler = WTFMove(completionHandler), networkProcess = WTFMove(networkProcess), sessionID, authenticationChallenge, taskIdentifier, partition](WebKit::AuthenticationChallengeDisposition disposition, const WebCore::Credential& credential) mutable {
#if !LOG_DISABLED
        LOG(NetworkSession, "%llu didReceiveChallenge completionHandler %hhu", taskIdentifier, disposition);
#else
        UNUSED_PARAM(taskIdentifier);
#endif
        if (credential.persistence() == WebCore::CredentialPersistence::ForSession && authenticationChallenge.protectionSpace().isPasswordBased()) {
            WebCore::Credential nonPersistentCredential(credential.user(), credential.password(), WebCore::CredentialPersistence::None);
            URL urlToStore;
            if (authenticationChallenge.failureResponse().httpStatusCode() == httpStatus401Unauthorized)
                urlToStore = authenticationChallenge.failureResponse().url();
            if (CheckedPtr storageSession = networkProcess->storageSession(sessionID))
                storageSession->credentialStorage().set(partition, nonPersistentCredential, authenticationChallenge.protectionSpace(), urlToStore);
            else
                ASSERT_NOT_REACHED();

            completionHandler(disposition, nonPersistentCredential);
            return;
        }
        completionHandler(disposition, credential);
    };
}

void NetworkSessionCocoa::continueDidReceiveChallenge(SessionWrapper& sessionWrapper, const WebCore::AuthenticationChallenge& challenge, NegotiatedLegacyTLS negotiatedLegacyTLS, NetworkDataTaskCocoa::TaskIdentifier taskIdentifier, RefPtr<NetworkDataTaskCocoa> networkDataTask, CompletionHandler<void(WebKit::AuthenticationChallengeDisposition, const WebCore::Credential&)>&& completionHandler)
{
    if (!networkDataTask) {
        if (RefPtr webSocketTask = sessionWrapper.webSocketDataTaskMap.get(taskIdentifier).get()) {
            auto challengeCompletionHandler = createChallengeCompletionHandler(networkProcess(), sessionID(), challenge, webSocketTask->partition(), 0, WTFMove(completionHandler));
            networkProcess().protectedAuthenticationManager()->didReceiveAuthenticationChallenge(sessionID(), webSocketTask->webPageProxyID(), !webSocketTask->topOrigin().isNull() ? &webSocketTask->topOrigin() : nullptr, challenge, negotiatedLegacyTLS, WTFMove(challengeCompletionHandler));
            return;
        }
        if (auto downloadID = sessionWrapper.downloadMap.getOptional(taskIdentifier)) {
            if (RefPtr download = networkProcess().checkedDownloadManager()->download(*downloadID)) {
                WebCore::AuthenticationChallenge authenticationChallenge { challenge };
                // Received an authentication challenge for a download being resumed.
                download->didReceiveChallenge(authenticationChallenge, WTFMove(completionHandler));
                return;
            }
        }
        LOG(NetworkSession, "%llu didReceiveChallenge completionHandler (cancel)", taskIdentifier);
        completionHandler(AuthenticationChallengeDisposition::Cancel, { });
        return;
    }

    auto challengeCompletionHandler = createChallengeCompletionHandler(networkProcess(), sessionID(), challenge, networkDataTask->partition(), taskIdentifier, WTFMove(completionHandler));
    if (negotiatedLegacyTLS == NegotiatedLegacyTLS::Yes
        && fastServerTrustEvaluationEnabled()
        && !networkDataTask->isTopLevelNavigation())
        return challengeCompletionHandler(AuthenticationChallengeDisposition::Cancel, { });

    networkDataTask->didReceiveChallenge(WebCore::AuthenticationChallenge { challenge }, negotiatedLegacyTLS, WTFMove(challengeCompletionHandler));
}

DMFWebsitePolicyMonitor *NetworkSessionCocoa::deviceManagementPolicyMonitor()
{
#if HAVE(DEVICE_MANAGEMENT)
    ASSERT(m_deviceManagementRestrictionsEnabled);
    if (!m_deviceManagementPolicyMonitor)
        m_deviceManagementPolicyMonitor = adoptNS([allocDMFWebsitePolicyMonitorInstance() initWithPolicyChangeHandler:nil]);
    return m_deviceManagementPolicyMonitor.get();
#else
    RELEASE_ASSERT_NOT_REACHED();
    return nil;
#endif
}

RefPtr<WebSocketTask> NetworkSessionCocoa::createWebSocketTask(WebPageProxyIdentifier webPageProxyID, std::optional<WebCore::FrameIdentifier> frameID, std::optional<WebCore::PageIdentifier> pageID, NetworkSocketChannel& channel, const WebCore::ResourceRequest& request, const String& protocol, const WebCore::ClientOrigin& clientOrigin, bool hadMainFrameMainResourcePrivateRelayed, bool allowPrivacyProxy, OptionSet<WebCore::AdvancedPrivacyProtections> advancedPrivacyProtections, WebCore::StoredCredentialsPolicy storedCredentialsPolicy)
{
    ASSERT(!request.hasHTTPHeaderField(WebCore::HTTPHeaderName::SecWebSocketProtocol));
    RetainPtr nsRequest = request.nsURLRequest(WebCore::HTTPBodyUpdatePolicy::DoNotUpdateHTTPBody);
    RetainPtr<NSMutableURLRequest> mutableRequest;

    auto ensureMutableRequest = [&] {
        if (!mutableRequest) {
            mutableRequest = adoptNS([nsRequest mutableCopy]);
            nsRequest = mutableRequest.get();
        }
        return mutableRequest;
    };

    if (!protocol.isNull())
        [ensureMutableRequest() addValue:StringView(protocol).createNSString().get() forHTTPHeaderField:@"Sec-WebSocket-Protocol"];

    // rdar://problem/68057031: explicitly disable sniffing for WebSocket handshakes.
    // FIXME: This is a static analysis false positive.
    SUPPRESS_UNRETAINED_ARG [nsRequest _setProperty:@NO forKey:bridge_cast(_kCFURLConnectionPropertyShouldSniff)];

#if ENABLE(APP_PRIVACY_REPORT)
    if (!request.isAppInitiated())
        ensureMutableRequest().get().attribution = NSURLRequestAttributionUser;

    appPrivacyReportTestingData().didLoadAppInitiatedRequest(nsRequest.get().attribution == NSURLRequestAttributionDeveloper);
#endif

    if (!allowPrivacyProxy)
        ensureMutableRequest().get()._prohibitPrivacyProxy = YES;

    if (hadMainFrameMainResourcePrivateRelayed || request.url().host() == clientOrigin.topOrigin.host())
        ensureMutableRequest().get()._privacyProxyFailClosedForUnreachableNonMainHosts = YES;

#if ENABLE(OPT_IN_PARTITIONED_COOKIES) && defined(CFN_COOKIE_ACCEPTS_POLICY_PARTITION) && CFN_COOKIE_ACCEPTS_POLICY_PARTITION
    if ([mutableRequest respondsToSelector:@selector(_setAllowOnlyPartitionedCookies:)]) {
        if (CheckedPtr storageSession = networkStorageSession(); storageSession && storageSession->isOptInCookiePartitioningEnabled()) {
            bool shouldAllowOnlyPartitioned = storageSession->thirdPartyCookieBlockingDecisionForRequest(request, frameID, pageID, networkProcess().shouldRelaxThirdPartyCookieBlockingForPage(webPageProxyID), isRequestToKnownCrossSiteTracker(request)) == WebCore::ThirdPartyCookieBlockingDecision::AllExceptPartitioned;
            [mutableRequest _setAllowOnlyPartitionedCookies:shouldAllowOnlyPartitioned];
        }
    }
#endif

    enableAdvancedPrivacyProtections(ensureMutableRequest().get(), advancedPrivacyProtections);

    Ref sessionSet = sessionSetForPage(webPageProxyID);
    RetainPtr task = [sessionSet->sessionWithCredentialStorage.session webSocketTaskWithRequest:nsRequest.get()];
    
    // Although the WebSocket protocol allows full 64-bit lengths, Chrome and Firefox limit the length to 2^63 - 1.
    // Use NSIntegerMax instead of 2^63 - 1 for 32-bit systems.
    task.get().maximumMessageSize = NSIntegerMax;

    return WebSocketTask::create(channel, webPageProxyID, frameID, pageID, sessionSet, request, clientOrigin, WTFMove(task), storedCredentialsPolicy);
}

void NetworkSessionCocoa::addWebSocketTask(WebPageProxyIdentifier webPageProxyID, WebSocketTask& task)
{
    auto& webSocketDataTaskMap = sessionSetForPage(webPageProxyID).sessionWithCredentialStorage.webSocketDataTaskMap;
    auto addResult = webSocketDataTaskMap.add(task.identifier(), &task);
    RELEASE_ASSERT(addResult.isNewEntry);
    RELEASE_LOG(NetworkSession, "NetworkSessionCocoa::addWebSocketTask, web socket count is %u", webSocketDataTaskMap.size());
}

void NetworkSessionCocoa::removeWebSocketTask(SessionSet& sessionSet, WebSocketTask& task)
{
    auto& webSocketDataTaskMap = sessionSet.sessionWithCredentialStorage.webSocketDataTaskMap;
    bool contained = webSocketDataTaskMap.remove(task.identifier());
    RELEASE_ASSERT(contained);
    RELEASE_LOG(NetworkSession, "NetworkSessionCocoa::removeWebSocketTask, web socket count is %u", webSocketDataTaskMap.size());
}

void NetworkSessionCocoa::addWebPageNetworkParameters(WebPageProxyIdentifier pageID, WebPageNetworkParameters&& parameters)
{
    auto addResult1 = m_perParametersSessionSets.add(parameters, nullptr);
    if (auto set = addResult1.iterator->value) {
        m_perPageSessionSets.add(pageID, *set);
        return;
    }

    auto addResult2 = m_perPageSessionSets.add(pageID, SessionSet::create());
    ASSERT(addResult2.isNewEntry);
    RetainPtr<NSURLSessionConfiguration> configuration = adoptNS([m_defaultSessionSet->sessionWithCredentialStorage.session.get().configuration copy]);
#if USE(APPLE_INTERNAL_SDK)
    configuration.get()._attributedBundleIdentifier = parameters.attributedBundleIdentifier().createNSString().get();
#endif
    initializeNSURLSessionsInSet(addResult2.iterator->value.get(), configuration.get());
    addResult1.iterator->value = addResult2.iterator->value.get();

    m_attributedBundleIdentifierFromPageIdentifiers.add(pageID, parameters.attributedBundleIdentifier());
}

// FIXME: This and WKURLSessionTaskDelegate are kind of duplicate code. Remove this.
// NetworkSessionCocoa::dataTaskWithRequest and NetworkLoad's constructor are also kind of duplicate code.
// Make NetworkLoad's redirection and challenge handling code pass everything to the NetworkLoadClient
// and use NetworkLoad and a new NetworkLoadClient instead of BlobDataTaskClient and WKURLSessionTaskDelegate.
class NetworkSessionCocoa::BlobDataTaskClient final : public RefCounted<NetworkSessionCocoa::BlobDataTaskClient>, public NetworkDataTaskClient {
    WTF_MAKE_TZONE_ALLOCATED(NetworkSessionCocoa::BlobDataTaskClient);
public:
    void ref() const final { RefCounted::ref(); }
    void deref() const final { RefCounted::deref(); }

    static Ref<BlobDataTaskClient> create(WebCore::ResourceRequest&& request, const std::optional<WebCore::SecurityOriginData>& topOrigin, NetworkSessionCocoa& session, IPC::Connection* connection, DataTaskIdentifier identifier)
    {
        return adoptRef(*new BlobDataTaskClient(WTFMove(request), topOrigin, session, connection, identifier));
    }

    void cancel() { m_task->cancel(); }

private:
    BlobDataTaskClient(WebCore::ResourceRequest&& request, const std::optional<WebCore::SecurityOriginData>& topOrigin, NetworkSessionCocoa& session, IPC::Connection* connection, DataTaskIdentifier identifier)
        : m_task(NetworkDataTaskBlob::create(session, *this, request, session.blobRegistry().filesInBlob(request.url(), topOrigin), topOrigin ? topOrigin->securityOrigin().ptr() : nullptr))
        , m_connection(connection)
        , m_identifier(identifier)
    {
        m_task->resume();
    }

    void willPerformHTTPRedirection(WebCore::ResourceResponse&&, WebCore::ResourceRequest&&, RedirectCompletionHandler&&) final { ASSERT_NOT_REACHED(); }
    void didReceiveChallenge(WebCore::AuthenticationChallenge&&, NegotiatedLegacyTLS, ChallengeCompletionHandler&&) final { ASSERT_NOT_REACHED(); }
    void didSendData(uint64_t totalBytesSent, uint64_t totalBytesExpectedToSend) final { ASSERT_NOT_REACHED(); }
    void wasBlocked() final { ASSERT_NOT_REACHED(); }
    void cannotShowURL() final { ASSERT_NOT_REACHED(); }
    void wasBlockedByRestrictions() final { ASSERT_NOT_REACHED(); }
    void wasBlockedByDisabledFTP() final { ASSERT_NOT_REACHED(); }

    void didReceiveResponse(WebCore::ResourceResponse&& response, NegotiatedLegacyTLS, PrivateRelayed, ResponseCompletionHandler&& completionHandler)
    {
        if (!m_connection)
            return completionHandler(WebCore::PolicyAction::Ignore);
        m_connection->sendWithAsyncReply(Messages::NetworkProcessProxy::DataTaskDidReceiveResponse(m_identifier, response), [completionHandler = WTFMove(completionHandler)] (bool allowed) mutable {
            completionHandler(allowed ? WebCore::PolicyAction::Use : WebCore::PolicyAction::Ignore);
        });
    }

    void didReceiveData(const WebCore::SharedBuffer& buffer)
    {
        if (!m_connection)
            return;
        buffer.forEachSegment([&](auto segment) {
            m_connection->send(Messages::NetworkProcessProxy::DataTaskDidReceiveData(m_identifier, segment), 0);
        });
    }

    void didCompleteWithError(const WebCore::ResourceError& error, const WebCore::NetworkLoadMetrics&)
    {
        if (!m_connection)
            return;
        m_connection->send(Messages::NetworkProcessProxy::DataTaskDidCompleteWithError(m_identifier, error), 0);
        if (CheckedPtr session = m_session.get())
            session->removeBlobDataTask(m_identifier);
    }

    const Ref<NetworkDataTask> m_task;
    const RefPtr<IPC::Connection> m_connection;
    WeakPtr<NetworkSessionCocoa> m_session;
    const DataTaskIdentifier m_identifier;
};

WTF_MAKE_TZONE_ALLOCATED_IMPL(NetworkSessionCocoa::BlobDataTaskClient);

void NetworkSessionCocoa::loadImageForDecoding(WebCore::ResourceRequest&& request, WebPageProxyIdentifier pageID, size_t maximumBytesFromNetwork, CompletionHandler<void(Expected<Ref<WebCore::FragmentedSharedBuffer>, WebCore::ResourceError>&&)>&& completionHandler)
{
    class Client : public RefCounted<Client>, public NetworkDataTaskClient {
    public:
        void ref() const final { RefCounted::ref(); }
        void deref() const final { RefCounted::deref(); }

        static void create(NetworkSession& networkSession, Ref<NetworkProcess>&& networkProcess, WebPageProxyIdentifier pageID, const NetworkLoadParameters& loadParameters, size_t maximumBytesFromNetwork, CompletionHandler<void(Expected<Ref<WebCore::FragmentedSharedBuffer>, WebCore::ResourceError>&&)>&& completionHandler)
        {
            Ref client = adoptRef(*new Client(networkSession, WTFMove(networkProcess), pageID, loadParameters, maximumBytesFromNetwork, WTFMove(completionHandler)));

            // Keep the load alive until didCompleteWithError.
            client->m_selfReference = WTFMove(client);
        }

    private:
        Client(NetworkSession& networkSession, Ref<NetworkProcess>&& networkProcess, WebPageProxyIdentifier pageID, const NetworkLoadParameters& loadParameters, size_t maximumBytesFromNetwork, CompletionHandler<void(Expected<Ref<WebCore::FragmentedSharedBuffer>, WebCore::ResourceError>&&)>&& completionHandler)
            : m_networkProcess(WTFMove(networkProcess))
            , m_url(loadParameters.request.url())
            , m_sessionID(networkSession.sessionID())
            , m_pageID(pageID)
            , m_maximumBytesFromNetwork(maximumBytesFromNetwork)
            , m_dataTask(NetworkDataTask::create(networkSession, *this, loadParameters))
            , m_completionHandler(WTFMove(completionHandler))
        {
            m_dataTask->resume();
        }

        void willPerformHTTPRedirection(WebCore::ResourceResponse&&, WebCore::ResourceRequest&& request, RedirectCompletionHandler&& completionHandler) final { completionHandler(WTFMove(request)); }
        void didReceiveChallenge(WebCore::AuthenticationChallenge&& challenge, NegotiatedLegacyTLS negotiatedLegacyTLS, ChallengeCompletionHandler&& completionHandler) final
        {
            m_networkProcess->protectedAuthenticationManager()->didReceiveAuthenticationChallenge(m_sessionID, m_pageID, nullptr, challenge, negotiatedLegacyTLS, WTFMove(completionHandler));
        }
        void didReceiveResponse(WebCore::ResourceResponse&&, NegotiatedLegacyTLS, PrivateRelayed, ResponseCompletionHandler&& completionHandler) final
        {
            completionHandler(WebCore::PolicyAction::Use);
        }
        void didReceiveData(const WebCore::SharedBuffer& buffer) final
        {
            m_buffer.append(buffer);
            if (m_buffer.size() > m_maximumBytesFromNetwork)
                didCompleteWithError(WebCore::ResourceError([NSError errorWithDomain:NSURLErrorDomain code:NSURLErrorDataLengthExceedsMaximum userInfo:nil]), WebCore::NetworkLoadMetrics { });
        }
        void didCompleteWithError(const WebCore::ResourceError& error, const WebCore::NetworkLoadMetrics&) final
        {
            if (error.isNull())
                m_completionHandler(m_buffer.take());
            else
                m_completionHandler(makeUnexpected(error));
            m_selfReference = nullptr;
        }
        void didSendData(uint64_t, uint64_t) final { }
        void wasBlocked() final { completeWithBlockedError(); }
        void cannotShowURL() final { completeWithBlockedError(); }
        void wasBlockedByRestrictions() final { completeWithBlockedError(); }
        void wasBlockedByDisabledFTP() final { completeWithBlockedError(); }
        void completeWithBlockedError()
        {
            didCompleteWithError(blockedError({ URL { m_url } }), WebCore::NetworkLoadMetrics { });
        }

        RefPtr<Client> m_selfReference;
        const Ref<NetworkProcess> m_networkProcess;
        const URL m_url;
        const PAL::SessionID m_sessionID;
        const WebPageProxyIdentifier m_pageID;
        const size_t m_maximumBytesFromNetwork;
        const Ref<NetworkDataTask> m_dataTask;
        CompletionHandler<void(Expected<Ref<WebCore::FragmentedSharedBuffer>, WebCore::ResourceError>&&)> m_completionHandler;
        WebCore::SharedBufferBuilder m_buffer;
    };

    NetworkLoadParameters loadParameters;
    loadParameters.request = WTFMove(request);
    // Client manages its own lifetime, derefing itself when its purpose has been fulfilled.
    Client::create(*this, networkProcess(), pageID, loadParameters, maximumBytesFromNetwork, WTFMove(completionHandler));
}

void NetworkSessionCocoa::dataTaskWithRequest(WebPageProxyIdentifier pageID, WebCore::ResourceRequest&& request, const std::optional<WebCore::SecurityOriginData>& topOrigin, CompletionHandler<void(DataTaskIdentifier)>&& completionHandler)
{
    auto identifier = DataTaskIdentifier::generate();
    if (request.url().protocolIsBlob()) {
        m_blobDataTasksForAPI.add(identifier, BlobDataTaskClient::create(WTFMove(request), topOrigin, *this, networkProcess().protectedParentProcessConnection().get(), identifier));
        return completionHandler(identifier);
    }

    RetainPtr nsRequest = request.nsURLRequest(WebCore::HTTPBodyUpdatePolicy::UpdateHTTPBody);
    if (![nsRequest URL]) {
        completionHandler(identifier);
        networkProcess().protectedParentProcessConnection()->send(Messages::NetworkProcessProxy::DataTaskDidCompleteWithError(identifier, cannotShowURLError(request)), 0);
        return;
    }

    auto session = sessionWrapperForTask(pageID, request, WebCore::StoredCredentialsPolicy::Use, std::nullopt).session;
    RetainPtr task = [session dataTaskWithRequest:nsRequest.get()];
    auto delegate = adoptNS([[WKURLSessionTaskDelegate alloc] initWithTask:task.get() identifier:identifier session:*this]);
#if HAVE(NSURLSESSION_TASK_DELEGATE)
    task.get().delegate = delegate.get();
#endif
    auto addResult = m_dataTasksForAPI.add(identifier, task.get());
    RELEASE_ASSERT(addResult.isNewEntry);
    [task resume];
    completionHandler(identifier);
}

void NetworkSessionCocoa::cancelDataTask(DataTaskIdentifier identifier)
{
    if (auto dataTask = m_dataTasksForAPI.take(identifier))
        [dataTask cancel];
    else if (auto blobDataTask = m_blobDataTasksForAPI.take(identifier))
        blobDataTask->cancel();
    else
        ASSERT_NOT_REACHED();
}

void NetworkSessionCocoa::removeDataTask(DataTaskIdentifier identifier)
{
    m_dataTasksForAPI.remove(identifier);
}

void NetworkSessionCocoa::removeBlobDataTask(DataTaskIdentifier identifier)
{
    m_blobDataTasksForAPI.remove(identifier);
}

void NetworkSessionCocoa::removeWebPageNetworkParameters(WebPageProxyIdentifier pageID)
{
    m_perPageSessionSets.remove(pageID);
    m_attributedBundleIdentifierFromPageIdentifiers.remove(pageID);
}

size_t NetworkSessionCocoa::countNonDefaultSessionSets() const
{
    HashSet<Ref<SessionSet>> uniqueSets;
    for (auto& set : m_perPageSessionSets.values())
        uniqueSets.add(set);
    return uniqueSets.size();
}

Vector<WebCore::SecurityOriginData> NetworkSessionCocoa::hostNamesWithAlternativeServices() const
{
#if HAVE(ALTERNATIVE_SERVICE)
    Vector<WebCore::SecurityOriginData> origins;
    RetainPtr<_NSHTTPAlternativeServicesStorage> storage = m_defaultSessionSet->sessionWithCredentialStorage.session.get().configuration._alternativeServicesStorage;
    RetainPtr<NSArray<_NSHTTPAlternativeServiceEntry *>> entries = [storage HTTPServiceEntriesWithFilter:_NSHTTPAlternativeServicesFilter.emptyFilter];

    for (_NSHTTPAlternativeServiceEntry* entry in entries.get()) {
        WebCore::SecurityOriginData origin = { "https"_s, entry.host, entry.port };
        origins.append(origin);
    }
    return origins;
#else
    return { };
#endif
}

void NetworkSessionCocoa::donateToSKAdNetwork(WebCore::PrivateClickMeasurement&& pcm)
{
#if HAVE(SKADNETWORK_v4)
    auto config = adoptNS([ASDInstallWebAttributionParamsConfig new]);
    config.get().appAdamId = @(*pcm.adamID());
    config.get().adNetworkRegistrableDomain = pcm.destinationSite().registrableDomain.string().createNSString().get();
    config.get().impressionId = pcm.ephemeralSourceNonce()->nonce.createNSString().get();
    config.get().sourceWebRegistrableDomain = pcm.sourceSite().registrableDomain.string().createNSString().get();
    config.get().version = @"4.0";
    config.get().attributionContext = AttributionTypeDefault;
#if HAVE(AD_ATTRIBUTION_KIT_PRIVATE_BROWSING)
    if (m_sessionID.isEphemeral()) {
        m_donatedEphemeralImpressionSessionID = WTF::UUID::createVersion4();

        // FIXME: Remove this respondsToSelector check in 2026 or so.
        if ([config respondsToSelector:@selector(privateBrowsingSessionId)])
            config.get().privateBrowsingSessionId = m_donatedEphemeralImpressionSessionID->createNSUUID().get();
    }
#endif // HAVE(AD_ATTRIBUTION_KIT_PRIVATE_BROWSING)
#if HAVE(ASD_INSTALL_WEB_ATTRIBUTION_SERVICE)
    [[ASDInstallWebAttributionService sharedInstance] addInstallWebAttributionParamsWithConfig:config.get() completionHandler:^(NSError *) { }];
#endif // HAVE(ASD_INSTALL_WEB_ATTRIBUTION_SERVICE)
#endif // HAVE(SKADNETWORK_v4)

    if (!m_privateClickMeasurementDebugModeEnabled)
        return;

    auto debugString = makeString("Submitting potential install attribution for AdamId: "_s,
        *pcm.adamID(),
        ", adNetworkRegistrableDomain: "_s,
        pcm.destinationSite().registrableDomain.string(),
        ", impressionId: "_s,
        pcm.ephemeralSourceNonce()->nonce,
        ", sourceWebRegistrableDomain: "_s,
        pcm.sourceSite().registrableDomain.string(),
        ", version: 3"_s);
    networkProcess().broadcastConsoleMessage(sessionID(), MessageSource::PrivateClickMeasurement, MessageLevel::Debug, debugString);
}

void NetworkSessionCocoa::deleteAlternativeServicesForHostNames(const Vector<String>& hosts)
{
#if HAVE(ALTERNATIVE_SERVICE)
    RetainPtr<_NSHTTPAlternativeServicesStorage> storage = m_defaultSessionSet->sessionWithCredentialStorage.session.get().configuration._alternativeServicesStorage;
    for (auto& host : hosts)
        [storage removeHTTPAlternativeServiceEntriesWithRegistrableDomain:host.createNSString().get()];
#else
    UNUSED_PARAM(hosts);
#endif
}

void NetworkSessionCocoa::clearAlternativeServices(WallTime modifiedSince)
{
#if HAVE(ALTERNATIVE_SERVICE)
    RetainPtr<_NSHTTPAlternativeServicesStorage> storage = m_defaultSessionSet->sessionWithCredentialStorage.session.get().configuration._alternativeServicesStorage;
    NSTimeInterval timeInterval = modifiedSince.secondsSinceEpoch().seconds();
    RetainPtr date = [NSDate dateWithTimeIntervalSince1970:timeInterval];
    [storage removeHTTPAlternativeServiceEntriesCreatedAfterDate:date.get()];
#else
    UNUSED_PARAM(modifiedSince);
#endif
}

void NetworkSessionCocoa::forEachSessionWrapper(NOESCAPE const Function<void(SessionWrapper&)>& function)
{
    auto sessionSetFunction = [&](SessionSet& sessionSet) {
        function(sessionSet.checkedSessionWithCredentialStorage().get());
        function(sessionSet.checkedEphemeralStatelessSession().get());
        if (sessionSet.appBoundSession)
            function(sessionSet.appBoundSession->checkedSessionWithCredentialStorage());

        for (auto& isolatedSession : sessionSet.isolatedSessions.values()) {
            if (isolatedSession)
                function(isolatedSession->checkedSessionWithCredentialStorage());
        }
    };
    
    sessionSetFunction(m_defaultSessionSet.get());

    for (auto& set : m_perPageSessionSets.values())
        sessionSetFunction(set);

    for (auto& set : m_perParametersSessionSets.values()) {
        if (set)
            sessionSetFunction(*set);
    }
}

#if HAVE(NW_PROXY_CONFIG)
void NetworkSessionCocoa::clearProxyConfigData()
{
    auto* clearProxies = nw_context_clear_proxiesPtr();
    if (!clearProxies)
        return;

    m_nwProxyConfigs.clear();

    RetainPtr<NSMutableSet> contexts = adoptNS([[NSMutableSet alloc] init]);
    forEachSessionWrapper([&contexts](SessionWrapper& sessionWrapper) {
        if (!sessionWrapper.session)
            return;
        [contexts.get() addObject:sessionWrapper.session.get()._networkContext];
    });

    for (nw_context_t context in contexts.get())
        clearProxies(context);
}

void NetworkSessionCocoa::setProxyConfigData(const Vector<std::pair<Vector<uint8_t>, std::optional<WTF::UUID>>>& proxyConfigurations)
{
    auto* clearProxies = nw_context_clear_proxiesPtr();
    auto* addProxy = nw_context_add_proxyPtr();
    auto* createProxyConfig = nw_proxy_config_create_with_agent_dataPtr();
    auto* requiresHTTPProtocols = nw_proxy_config_stack_requires_http_protocolsPtr();
    if (!clearProxies || !addProxy || !createProxyConfig || !requiresHTTPProtocols)
        return;

    m_nwProxyConfigs.clear();

    // If any of the proxies pass the `nw_proxy_config_stack_requires_http_protocols` check,
    // then we cannot set the proxy on the live nw_context_t and instead must destroy and recreate the NSURLSession
    bool recreateSessions = false;
    for (auto& config : proxyConfigurations) {
        uuid_t identifier;
        if (config.second)
            memcpySpan(asMutableByteSpan(identifier), config.second->span().first(sizeof(uuid_t)));
        else
            zeroBytes(identifier);

        // This is correct but static analysis does not seem to recognize that `createProxyConfig` returns
        // a +1 value.
        SUPPRESS_RETAINPTR_CTOR_ADOPT auto nwProxyConfig = adoptNS(createProxyConfig(config.first.span().data(), config.first.size(), identifier));

        if (requiresHTTPProtocols(nwProxyConfig.get()))
            recreateSessions = true;

        m_nwProxyConfigs.append(WTFMove(nwProxyConfig));
    }

    if (recreateSessions) {
        forEachSessionWrapper([this](SessionWrapper& sessionWrapper) {
            if (sessionWrapper.session)
                sessionWrapper.recreateSessionWithUpdatedProxyConfigurations(*this);
        });
        return;
    }

    RetainPtr<NSMutableSet> contexts = adoptNS([[NSMutableSet alloc] init]);
    forEachSessionWrapper([&contexts](SessionWrapper& sessionWrapper) {
        if (!sessionWrapper.session)
            return;
        [contexts.get() addObject:sessionWrapper.session.get()._networkContext];
    });

    for (nw_context_t context in contexts.get()) {
        clearProxies(context);

        for (auto& proxyConfig : m_nwProxyConfigs)
            addProxy(context, proxyConfig.get());
    }
}

void NetworkSessionCocoa::applyProxyConfigurationToSessionConfiguration(NSURLSessionConfiguration *configuration)
{
    if (!m_nwProxyConfigs.isEmpty()) {
        RetainPtr nwProxyConfigurations = adoptNS([[NSMutableArray alloc] initWithCapacity:m_nwProxyConfigs.size()]);
        for (auto& proxyConfig : m_nwProxyConfigs)
            [nwProxyConfigurations addObject:proxyConfig.get()];

        configuration.proxyConfigurations = nwProxyConfigurations.get();
    } else
        configuration.proxyConfigurations = @[ ];
}
#endif // HAVE(NW_PROXY_CONFIG)

#if USE(APPLE_INTERNAL_SDK)

#if ENABLE(APP_PRIVACY_REPORT) && HAVE(SYMPTOMS_FRAMEWORK)
static bool isActingOnBehalfOfAFullWebBrowser(const String& bundleID)
{
    return bundleID == "com.apple.webbookmarksd"_s;
}
#endif

void NetworkSessionCocoa::removeNetworkWebsiteData(std::optional<WallTime> modifiedSince, std::optional<HashSet<WebCore::RegistrableDomain>>&& domains, CompletionHandler<void()>&& completionHandler)
{
#if ENABLE(APP_PRIVACY_REPORT) && HAVE(SYMPTOMS_FRAMEWORK)
    // FIXME: Add automated test for this once rdar://81420753 is resolved.
    if (sessionID().isEphemeral()) {
        completionHandler();
        return;
    }

    auto bundleID = applicationBundleIdentifier();
    // FIXME: The following `isParentProcessAFullWebBrowser` check is inaccurate in Safari on macOS.
    if (!isParentProcessAFullWebBrowser(networkProcess()) && !isActingOnBehalfOfAFullWebBrowser(bundleID))
        return completionHandler();

    if (!SymptomAnalyticsLibrary()
        || !SymptomPresentationFeedLibrary()
        || !SymptomPresentationLiteLibrary()
        || !getAnalyticsWorkspaceClassSingleton()
        || !getUsageFeedClassSingleton()
        || !canLoadkSymptomAnalyticsServiceEndpoint()) {
        completionHandler();
        return;
    }

    RetainPtr<AnalyticsWorkspace> workspace = adoptNS([allocAnalyticsWorkspaceInstance() initWorkspaceWithService:getkSymptomAnalyticsServiceEndpoint()]);
    RetainPtr<UsageFeed> usageFeed = adoptNS([allocUsageFeedInstance() initWithWorkspace:workspace.get()]);

    if (![usageFeed.get() respondsToSelector:@selector(performNetworkDomainsActionWithOptions:reply:)]
        || !canLoadkSymptomAnalyticsServiceDomainTrackingClearHistoryKey()
        || !canLoadkSymptomAnalyticsServiceDomainTrackingClearHistoryBundleIDs()
        || !canLoadkSymptomAnalyticsServiceDomainTrackingClearHistoryStartDate()
        || !canLoadkSymptomAnalyticsServiceDomainTrackingClearHistoryEndDate()) {
        completionHandler();
        return;
    }

    auto *startDate = [NSDate distantPast];
    if (modifiedSince) {
        NSTimeInterval timeInterval = modifiedSince.value().secondsSinceEpoch().seconds();
        startDate = [NSDate dateWithTimeIntervalSince1970:timeInterval];
    }

    auto contextArray = adoptNS([[NSMutableArray alloc] init]);
    if (domains) {
        contextArray = createNSArray(*domains, [&] (auto domain) {
            return [NSString stringWithUTF8String:domain.string().utf8().data()];
        });
    }

    if (isActingOnBehalfOfAFullWebBrowser(bundleID))
        bundleID = "com.apple.mobilesafari"_s;

    NSDictionary *options = @{
        (id)getkSymptomAnalyticsServiceDomainTrackingClearHistoryKey(): @{
            (id)getkSymptomAnalyticsServiceDomainTrackingClearHistoryBundleIDs(): @{
                bundleID.createNSString().get() : contextArray.get(),
            },
            (id)getkSymptomAnalyticsServiceDomainTrackingClearHistoryStartDate(): startDate,
            (id)getkSymptomAnalyticsServiceDomainTrackingClearHistoryEndDate(): [NSDate distantFuture]
        }
    };

    bool result = [usageFeed performNetworkDomainsActionWithOptions:options reply:makeBlockPtr([completionHandler = WTFMove(completionHandler)](NSDictionary *reply, NSError *error) mutable {
        if (error)
            RELEASE_LOG_DEBUG(NetworkSession, "Error deleting network domain data %" PUBLIC_LOG_STRING, error.localizedDescription.UTF8String);

        completionHandler();
    }).get()];

    if (!result)
        LOG(NetworkSession, "Error deleting network domain data: invalid parameter or failure to contact the service");
#else
    UNUSED_PARAM(modifiedSince);
    completionHandler();
#endif
}

#endif // USE(APPLE_INTERNAL_SDK)

} // namespace WebKit
