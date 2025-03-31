/*
 *  Copyright (C) 2025 Igalia, S.L
 *
 *  This library is free software; you can redistribute it and/or
 *  modify it under the terms of the GNU Lesser General Public
 *  License as published by the Free Software Foundation; either
 *  version 2 of the License, or (at your option) any later version.
 *
 *  This library is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 *  Lesser General Public License for more details.
 *
 *  You should have received a copy of the GNU Lesser General Public
 *  License along with this library; if not, write to the Free Software
 *  Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
 */

#pragma once

#if ENABLE(VIDEO) && USE(GSTREAMER)

#include "GRefPtrGStreamer.h"
#include <wtf/Forward.h>

namespace WebCore {

class GStreamerVideoFrameConverter {
    friend NeverDestroyed<GStreamerVideoFrameConverter>;

public:
    static GStreamerVideoFrameConverter& singleton();

    WARN_UNUSED_RETURN GRefPtr<GstSample> convert(const GRefPtr<GstSample>&, const GRefPtr<GstCaps>&);

private:
    GStreamerVideoFrameConverter();

    GRefPtr<GstElement> m_pipeline;
    GRefPtr<GstElement> m_src;
    GRefPtr<GstElement> m_sink;
};

} // namespace WebCore

#endif // ENABLE(VIDEO) && USE(GSTREAMER)
