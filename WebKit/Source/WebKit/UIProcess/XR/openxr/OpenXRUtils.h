/*
 * Copyright (C) 2025 Igalia, S.L.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public License
 * aint with this library; see the file COPYING.LIB.  If not, write to
 * the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 */

#pragma once

#if ENABLE(WEBXR) && USE(OPENXR)

#include "Logging.h"
#include <openxr/openxr.h>
#include <openxr/openxr_reflection.h>
#include <wtf/text/MakeString.h>
#include <wtf/text/WTFString.h>

namespace WebKit {

template<typename T, XrStructureType StructureType>
T createOpenXRStruct()
{
    T object;
    zeroBytes(object);
    object.type = StructureType;
    object.next = nullptr;
    return object;
}

#define STRINGIFY(x) #x
#define TOSTRING(x) STRINGIFY(x)
#define FILE_AND_LINE __FILE__ ":" TOSTRING(__LINE__)

// Macro to generate stringify functions for OpenXR enumerations based data provided in openxr_reflection.h
#define ENUM_CASE_STR(name, val) case name: return #name;
#define MAKE_TO_STRING_FUNC(enumType) \
    inline const char* toString(enumType e) { \
        switch (e) { \
            XR_LIST_ENUM_##enumType(ENUM_CASE_STR) \
            default: return "Unknown " #enumType; \
        } \
    }

MAKE_TO_STRING_FUNC(XrReferenceSpaceType);
MAKE_TO_STRING_FUNC(XrViewConfigurationType);
MAKE_TO_STRING_FUNC(XrEnvironmentBlendMode);
MAKE_TO_STRING_FUNC(XrSessionState);
MAKE_TO_STRING_FUNC(XrResult);
MAKE_TO_STRING_FUNC(XrFormFactor);

inline XrResult checkXrResult(XrResult res, const char* originator = nullptr, const char* sourceLocation = nullptr)
{
    if (XR_FAILED(res))
        LOG(XR, "OpenXR error: %s (%s) at %s", toString(res), originator ? originator : "unknown", sourceLocation ? sourceLocation : "unknown location");

    return res;
}

#define CHECK_XRCMD(cmd) checkXrResult(cmd, #cmd, FILE_AND_LINE);

} // namespace WebKit

#endif // ENABLE(WEBXR) && USE(OPENXR)
