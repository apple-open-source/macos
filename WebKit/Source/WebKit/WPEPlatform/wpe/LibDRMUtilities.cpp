/*
 * Copyright (C) 2025 Igalia S.L.
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
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A
 * PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "LibDRMUtilities.h"

#if USE(LIBDRM)

#include <glib.h>
#include <wtf/StdLibExtras.h>
#include <xf86drm.h>

std::pair<CString, CString> lookupNodesWithLibDRM()
{
    std::array<drmDevicePtr, 64> devices { };

    int numDevices = drmGetDevices2(0, devices.data(), std::size(devices));
    if (numDevices <= 0)
        return { };

    CString devicePath;
    CString renderNodePath;
    for (int i = 0; i < numDevices; ++i) {
        drmDevice* device = devices[i];
        if (!(device->available_nodes & (1 << DRM_NODE_PRIMARY | 1 << DRM_NODE_RENDER)))
            continue;

        if (!devicePath.isNull()) {
            g_warning("Infered DRM device (%s) using libdrm but multiple were found, you can override this with WPE_DRM_DEVICE and WPE_DRM_RENDER_NODE", devicePath.data());
            break;
        }

        if (device->available_nodes & (1 << DRM_NODE_RENDER))
            renderNodePath = CString(device->nodes[DRM_NODE_RENDER]);
        if (device->available_nodes & (1 << DRM_NODE_PRIMARY))
            devicePath = CString(device->nodes[DRM_NODE_PRIMARY]);
    }
    drmFreeDevices(devices.data(), numDevices);

    return { WTFMove(devicePath), WTFMove(renderNodePath) };
}

#endif /* USE(LIBDRM) */
