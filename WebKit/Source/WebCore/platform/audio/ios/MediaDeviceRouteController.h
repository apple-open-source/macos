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

#pragma once

#if HAVE(AVROUTING_FRAMEWORK)

#include <WebKitAdditions/MediaDeviceRouteAdditions.h>
#include <WebKitAdditions/MediaDeviceRouteControllerAdditions.h>
#include <wtf/AbstractThreadSafeRefCountedAndCanMakeWeakPtr.h>
#include <wtf/RetainPtr.h>
#include <wtf/ThreadSafeWeakPtr.h>
#include <wtf/Vector.h>

OBJC_CLASS WebMediaDeviceRouteController;

namespace WebCore {

class MediaDeviceRoute;
class MediaDeviceRouteController;

class MediaDeviceRouteControllerClient : public AbstractThreadSafeRefCountedAndCanMakeWeakPtr {
public:
    virtual ~MediaDeviceRouteControllerClient() = default;

    virtual void activeRoutesDidChange(MediaDeviceRouteController&) = 0;
};

class MediaDeviceRouteController {
    friend class NeverDestroyed<MediaDeviceRouteController>;

public:
    WEBCORE_EXPORT static MediaDeviceRouteController& singleton();

    RefPtr<MediaDeviceRouteControllerClient> client() const { return m_client.get(); }
    void setClient(MediaDeviceRouteControllerClient* client) { m_client = client; }

    RefPtr<MediaDeviceRoute> mostRecentActiveRoute() const;
    RefPtr<MediaDeviceRoute> routeForIdentifier(const std::optional<WTF::UUID>&) const;

    WEBCORE_EXPORT bool activateRoute(WebMediaDevicePlatformRoute *);
    WEBCORE_EXPORT bool deactivateRoute(WebMediaDevicePlatformRoute *);

private:
    MediaDeviceRouteController();

    RetainPtr<WebMediaDeviceRouteController> m_controller;
    RetainPtr<WebMediaDevicePlatformRouteController> m_platformController;
    ThreadSafeWeakPtr<MediaDeviceRouteControllerClient> m_client;
    Vector<Ref<MediaDeviceRoute>> m_activeRoutes;
};

} // namespace WebCore

#endif // HAVE(AVROUTING_FRAMEWORK)

namespace WebCore {
WEBCORE_EXPORT void setMockMediaDeviceRouteControllerEnabled(bool);
WEBCORE_EXPORT bool mockMediaDeviceRouteControllerEnabled();
}
