/*
 * Copyright (C) 2012 Apple Inc. All rights reserved.
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
#include "NetworkResourceLoadParameters.h"


namespace WebKit {
using namespace WebCore;

NetworkResourceLoadParameters::NetworkResourceLoadParameters(
    WebPageProxyIdentifier webPageProxyID
    , WebCore::PageIdentifier webPageID
    , WebCore::FrameIdentifier webFrameID
    , RefPtr<WebCore::SecurityOrigin>&& topOrigin
    , RefPtr<WebCore::SecurityOrigin>&& sourceOrigin
    , WTF::ProcessID parentPID
    , WebCore::ResourceRequest&& request
    , WebCore::ContentSniffingPolicy contentSniffingPolicy
    , WebCore::ContentEncodingSniffingPolicy contentEncodingSniffingPolicy
    , WebCore::StoredCredentialsPolicy storedCredentialsPolicy
    , WebCore::ClientCredentialPolicy clientCredentialPolicy
    , bool shouldClearReferrerOnHTTPSToHTTPRedirect
    , bool needsCertificateInfo
    , bool isMainFrameNavigation
    , std::optional<NavigationActionData>&& mainResourceNavigationDataForAnyFrame
    , PreconnectOnly shouldPreconnectOnly
    , std::optional<NavigatingToAppBoundDomain> isNavigatingToAppBoundDomain
    , bool hadMainFrameMainResourcePrivateRelayed
    , bool allowPrivacyProxy
    , OptionSet<WebCore::AdvancedPrivacyProtections> advancedPrivacyProtections
    , std::optional<WebCore::ResourceLoaderIdentifier> identifier
    , RefPtr<WebCore::FormData>&& httpBody
    , std::optional<Vector<SandboxExtension::Handle>>&& sandboxExtensionIfHttpBody
    , std::optional<SandboxExtension::Handle>&& sandboxExtensionIflocalFile
    , Seconds maximumBufferingTime
    , WebCore::FetchOptions&& options
    , std::optional<WebCore::ContentSecurityPolicyResponseHeaders>&& cspResponseHeaders
    , URL&& parentFrameURL
    , URL&& frameURL
    , WebCore::CrossOriginEmbedderPolicy parentCrossOriginEmbedderPolicy
    , WebCore::CrossOriginEmbedderPolicy crossOriginEmbedderPolicy
    , WebCore::HTTPHeaderMap&& originalRequestHeaders
    , bool shouldRestrictHTTPResponseAccess
    , WebCore::PreflightPolicy preflightPolicy
    , bool shouldEnableCrossOriginResourcePolicy
    , Vector<Ref<WebCore::SecurityOrigin>>&& frameAncestorOrigins
    , bool pageHasResourceLoadClient
    , std::optional<WebCore::FrameIdentifier> parentFrameID
    , bool crossOriginAccessControlCheckEnabled
    , URL&& documentURL
    , bool isCrossOriginOpenerPolicyEnabled
    , bool isClearSiteDataHeaderEnabled
    , bool isClearSiteDataExecutionContextEnabled
    , bool isDisplayingInitialEmptyDocument
    , WebCore::SandboxFlags effectiveSandboxFlags
    , URL&& openerURL
    , WebCore::CrossOriginOpenerPolicy&& sourceCrossOriginOpenerPolicy
    , std::optional<WebCore::NavigationIdentifier> navigationID
    , std::optional<WebCore::NavigationRequester>&& navigationRequester
    , WebCore::ServiceWorkersMode serviceWorkersMode
    , std::optional<WebCore::ServiceWorkerRegistrationIdentifier> serviceWorkerRegistrationIdentifier
    , OptionSet<WebCore::HTTPHeadersToKeepFromCleaning> httpHeadersToKeep
    , std::optional<WebCore::FetchIdentifier> navigationPreloadIdentifier
#if ENABLE(CONTENT_EXTENSIONS)
    , URL&& mainDocumentURL
    , std::optional<UserContentControllerIdentifier> userContentControllerIdentifier
#endif
#if ENABLE(WK_WEB_EXTENSIONS)
    , bool pageHasLoadedWebExtensions
#endif
    , bool linkPreconnectEarlyHintsEnabled
    , bool shouldRecordFrameLoadForStorageAccess
    ) : webPageProxyID(webPageProxyID)
    , webPageID(webPageID)
    , webFrameID(webFrameID)
    , topOrigin(WTFMove(topOrigin))
    , sourceOrigin(WTFMove(sourceOrigin))
    , parentPID(parentPID)
    , request(WTFMove(request))
    , contentSniffingPolicy(contentSniffingPolicy)
    , contentEncodingSniffingPolicy(contentEncodingSniffingPolicy)
    , storedCredentialsPolicy(storedCredentialsPolicy)
    , clientCredentialPolicy(clientCredentialPolicy)
    , shouldClearReferrerOnHTTPSToHTTPRedirect(shouldClearReferrerOnHTTPSToHTTPRedirect)
    , needsCertificateInfo(needsCertificateInfo)
    , isMainFrameNavigation(isMainFrameNavigation)
    , mainResourceNavigationDataForAnyFrame(mainResourceNavigationDataForAnyFrame)
    , shouldPreconnectOnly(shouldPreconnectOnly)
    , isNavigatingToAppBoundDomain(isNavigatingToAppBoundDomain)
    , hadMainFrameMainResourcePrivateRelayed(hadMainFrameMainResourcePrivateRelayed)
    , allowPrivacyProxy(allowPrivacyProxy)
    , advancedPrivacyProtections(advancedPrivacyProtections)
    , identifier(identifier)
    , maximumBufferingTime(maximumBufferingTime)
    , options(WTFMove(options))
    , cspResponseHeaders(WTFMove(cspResponseHeaders))
    , parentFrameURL(WTFMove(parentFrameURL))
    , frameURL(WTFMove(frameURL))
    , parentCrossOriginEmbedderPolicy(parentCrossOriginEmbedderPolicy)
    , crossOriginEmbedderPolicy(crossOriginEmbedderPolicy)
    , originalRequestHeaders(WTFMove(originalRequestHeaders))
    , shouldRestrictHTTPResponseAccess(shouldRestrictHTTPResponseAccess)
    , preflightPolicy(preflightPolicy)
    , shouldEnableCrossOriginResourcePolicy(shouldEnableCrossOriginResourcePolicy)
    , frameAncestorOrigins(WTFMove(frameAncestorOrigins))
    , pageHasResourceLoadClient(pageHasResourceLoadClient)
    , parentFrameID(parentFrameID)
    , crossOriginAccessControlCheckEnabled(crossOriginAccessControlCheckEnabled)
    , documentURL(WTFMove(documentURL))
    , isCrossOriginOpenerPolicyEnabled(isCrossOriginOpenerPolicyEnabled)
    , isClearSiteDataHeaderEnabled(isClearSiteDataHeaderEnabled)
    , isClearSiteDataExecutionContextEnabled(isClearSiteDataExecutionContextEnabled)
    , isDisplayingInitialEmptyDocument(isDisplayingInitialEmptyDocument)
    , effectiveSandboxFlags(effectiveSandboxFlags)
    , openerURL(WTFMove(openerURL))
    , sourceCrossOriginOpenerPolicy(WTFMove(sourceCrossOriginOpenerPolicy))
    , navigationID(navigationID)
    , navigationRequester(WTFMove(navigationRequester))
    , serviceWorkersMode(serviceWorkersMode)
    , serviceWorkerRegistrationIdentifier(serviceWorkerRegistrationIdentifier)
    , httpHeadersToKeep(httpHeadersToKeep)
    , navigationPreloadIdentifier(navigationPreloadIdentifier)
#if ENABLE(CONTENT_EXTENSIONS)
    , mainDocumentURL(WTFMove(mainDocumentURL))
    , userContentControllerIdentifier(userContentControllerIdentifier)
#endif
#if ENABLE(WK_WEB_EXTENSIONS)
    , pageHasLoadedWebExtensions(pageHasLoadedWebExtensions)
#endif
    , linkPreconnectEarlyHintsEnabled(linkPreconnectEarlyHintsEnabled)
    , shouldRecordFrameLoadForStorageAccess(shouldRecordFrameLoadForStorageAccess)
{
    if (httpBody) {
        // FIXME: Use EncodeRequestBody instead of this.
        this->request.setHTTPBody(WTFMove(httpBody));

        if (!sandboxExtensionIfHttpBody)
            RELEASE_ASSERT_NOT_REACHED_WITH_MESSAGE("NetworkResourceLoadParameters which specify a httpBody should have sandboxExtensionIfHttpBody");
        for (auto& handle : WTFMove(*sandboxExtensionIfHttpBody)) {
            if (auto extension = SandboxExtension::create(WTFMove(handle)))
                requestBodySandboxExtensions.append(WTFMove(extension));
        }
    }

    if (this->request.url().protocolIsFile()) {
        if (!sandboxExtensionIflocalFile)
            RELEASE_ASSERT_NOT_REACHED_WITH_MESSAGE("NetworkResourceLoadParameters which specify a URL of a local file should have sandboxExtensionIflocalFile");
        resourceSandboxExtension = SandboxExtension::create(WTFMove(*sandboxExtensionIflocalFile));
    }
}

RefPtr<SecurityOrigin> NetworkResourceLoadParameters::parentOrigin() const
{
    if (frameAncestorOrigins.isEmpty())
        return nullptr;
    return frameAncestorOrigins.first().ptr();
}

std::optional<Vector<SandboxExtension::Handle>> NetworkResourceLoadParameters::sandboxExtensionsIfHttpBody() const
{
    if (!request.httpBody())
        return std::nullopt;
    
    Vector<SandboxExtension::Handle> requestBodySandboxExtensions;
    for (const FormDataElement& element : request.httpBody()->elements()) {
        if (auto* fileData = std::get_if<FormDataElement::EncodedFileData>(&element.data)) {
            const String& path = fileData->filename;
            if (auto handle = SandboxExtension::createHandle(path, SandboxExtension::Type::ReadOnly))
                requestBodySandboxExtensions.append(WTFMove(*handle));
        }
    }
    return requestBodySandboxExtensions;
    
}

std::optional<SandboxExtension::Handle> NetworkResourceLoadParameters::sandboxExtensionIflocalFile() const
{
    if (!request.url().protocolIsFile())
        return std::nullopt;
    
    SandboxExtension::Handle requestSandboxExtension;
#if HAVE(AUDIT_TOKEN)
    if (networkProcessAuditToken) {
        if (auto handle = SandboxExtension::createHandleForReadByAuditToken(request.url().fileSystemPath(), *networkProcessAuditToken))
            requestSandboxExtension = WTFMove(*handle);
    } else
#endif
    {
        if (auto handle = SandboxExtension::createHandle(request.url().fileSystemPath(), SandboxExtension::Type::ReadOnly))
            requestSandboxExtension = WTFMove(*handle);
    }

    return requestSandboxExtension;
}

NetworkLoadParameters NetworkResourceLoadParameters::networkLoadParameters() const
{
    return {
        webPageProxyID,
        webPageID,
        webFrameID,
        topOrigin,
        sourceOrigin,
        parentPID,
#if HAVE(AUDIT_TOKEN)
        networkProcessAuditToken,
#endif
        request,
        contentSniffingPolicy,
        contentEncodingSniffingPolicy,
        storedCredentialsPolicy,
        clientCredentialPolicy,
        shouldClearReferrerOnHTTPSToHTTPRedirect,
        needsCertificateInfo,
        isMainFrameNavigation,
        mainResourceNavigationDataForAnyFrame,
        blobFileReferences,
        shouldPreconnectOnly,
        networkActivityTracker,
        isNavigatingToAppBoundDomain,
        hadMainFrameMainResourcePrivateRelayed,
        allowPrivacyProxy,
        advancedPrivacyProtections
    };
}

} // namespace WebKit
