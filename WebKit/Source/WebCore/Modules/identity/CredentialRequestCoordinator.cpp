/*
 * Copyright (C) 2024 Apple Inc. All rights reserved.
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
#include "CredentialRequestCoordinator.h"

#if ENABLE(WEB_AUTHN)

#include "AbortSignal.h"
#include "CredentialRequestCoordinatorClient.h"
#include "CredentialRequestOptions.h"
#include "Document.h"
#include "DocumentInlines.h"
#include "FrameDestructionObserverInlines.h"
#include "JSDOMPromiseDeferred.h"
#include "VisibilityState.h"
#include <wtf/Logger.h>
#include <wtf/TZoneMallocInlines.h>

namespace WebCore {

WTF_MAKE_TZONE_ALLOCATED_IMPL(CredentialRequestCoordinator);
WTF_MAKE_TZONE_ALLOCATED_IMPL(CredentialRequestCoordinatorClient);

CredentialRequestCoordinator::CredentialRequestCoordinator(std::unique_ptr<CredentialRequestCoordinatorClient>&& client)
    : m_client(WTFMove(client))
{
}

void CredentialRequestCoordinator::discoverFromExternalSource(const Document& document, CredentialRequestOptions&& requestOptions, CredentialPromise&& promise)
{
    RefPtr window = document.protectedWindow();
    if (!m_client || !window) {
        LOG_ERROR("No client or window found");
        promise.reject(Exception { ExceptionCode::UnknownError, "Unknown internal error."_s });
        return;
    }

    if (!document.hasFocus()) {
        promise.reject(Exception { ExceptionCode::NotAllowedError, "The document is not focused."_s });
        return;
    }

    if (document.visibilityState() != VisibilityState::Visible) {
        promise.reject(Exception { ExceptionCode::NotAllowedError, "The document is not visible."_s });
        return;
    }

    const auto& options = requestOptions.digital.value();

    if (!options.requests.size()) {
        promise.reject(Exception { ExceptionCode::TypeError, "Must make at least one request."_s });
        return;
    }

    if (!window->consumeTransientActivation()) {
        promise.reject(Exception { ExceptionCode::NotAllowedError, "Calling get() needs to be triggered by an activation triggering user event."_s });
        return;
    }

    if (requestOptions.signal) {
        requestOptions.signal->addAlgorithm([this](JSC::JSValue) mutable {
            if (!this->m_client)
                return;

            ASSERT(!this->m_isCancelling);

            this->m_isCancelling = true;
            this->m_client->cancel([this]() mutable {
                this->m_isCancelling = false;
                if (auto queuedRequest = WTFMove(this->m_queuedRequest))
                    queuedRequest();
            });
        });
    }

    auto callback = [promise = WTFMove(promise), abortSignal = WTFMove(requestOptions.signal)](ExceptionData&& exception) mutable {
        if (abortSignal && abortSignal->aborted()) {
            LOG_ERROR("Request aborted by AbortSignal");
            promise.reject(Exception { ExceptionCode::AbortError, "Aborted by AbortSignal."_s });
            return;
        }
        ASSERT(!exception.message.isNull());
        LOG_ERROR("Exception occurred: %s", exception.message.utf8().data());
        promise.reject(exception.toException());
    };

    RefPtr frame = document.frame();
    ASSERT(frame);

    if (m_isCancelling) {
        m_queuedRequest = [this, weakFrame = WeakPtr(*frame), requestOptions = WTFMove(requestOptions), callback = WTFMove(callback)]() mutable {
            if (!this->m_client || !weakFrame) {
                LOG_ERROR("No this, or no frame, or no client found");
                return;
            }
            const auto options = requestOptions.digital.value();
            this->m_client->requestDigitalCredential(*weakFrame, options, WTFMove(callback));
        };
        return;
    }

    m_client->requestDigitalCredential(*frame, options, WTFMove(callback));
}

} // namespace WebCore

#endif // ENABLE(WEB_AUTHN)
