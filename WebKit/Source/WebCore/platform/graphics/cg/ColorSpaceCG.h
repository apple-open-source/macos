/*
 * Copyright (C) 2020-2025 Apple Inc. All rights reserved.
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

#include "ColorSpace.h"
#include <CoreGraphics/CoreGraphics.h>
#include <optional>
#include <wtf/cf/TypeCastsCF.h>

WTF_DECLARE_CF_TYPE_TRAIT(CGColorSpace);

namespace WebCore {

template<ColorSpace> struct CGColorSpaceMapping;

WEBCORE_EXPORT CGColorSpaceRef sRGBColorSpaceSingleton();
template<> struct CGColorSpaceMapping<ColorSpace::SRGB> {
    static CGColorSpaceRef colorSpace()
    {
        return sRGBColorSpaceSingleton();
    }
};

WEBCORE_EXPORT CGColorSpaceRef adobeRGB1998ColorSpaceSingleton();
template<> struct CGColorSpaceMapping<ColorSpace::A98RGB> {
    static CGColorSpaceRef colorSpace()
    {
        return adobeRGB1998ColorSpaceSingleton();
    }
};

WEBCORE_EXPORT CGColorSpaceRef displayP3ColorSpaceSingleton();
template<> struct CGColorSpaceMapping<ColorSpace::DisplayP3> {
    static CGColorSpaceRef colorSpace()
    {
        return displayP3ColorSpaceSingleton();
    }
};

WEBCORE_EXPORT CGColorSpaceRef extendedAdobeRGB1998ColorSpaceSingleton();
template<> struct CGColorSpaceMapping<ColorSpace::ExtendedA98RGB> {
    static CGColorSpaceRef colorSpace()
    {
        return extendedAdobeRGB1998ColorSpaceSingleton();
    }
};

WEBCORE_EXPORT CGColorSpaceRef extendedDisplayP3ColorSpaceSingleton();
template<> struct CGColorSpaceMapping<ColorSpace::ExtendedDisplayP3> {
    static CGColorSpaceRef colorSpace()
    {
        return extendedDisplayP3ColorSpaceSingleton();
    }
};

WEBCORE_EXPORT CGColorSpaceRef extendedITUR_2020ColorSpaceSingleton();
template<> struct CGColorSpaceMapping<ColorSpace::ExtendedRec2020> {
    static CGColorSpaceRef colorSpace()
    {
        return extendedITUR_2020ColorSpaceSingleton();
    }
};

WEBCORE_EXPORT CGColorSpaceRef extendedLinearSRGBColorSpaceSingleton();
template<> struct CGColorSpaceMapping<ColorSpace::ExtendedLinearSRGB> {
    static CGColorSpaceRef colorSpace()
    {
        return extendedLinearSRGBColorSpaceSingleton();
    }
};

WEBCORE_EXPORT CGColorSpaceRef extendedROMMRGBColorSpaceSingleton();
template<> struct CGColorSpaceMapping<ColorSpace::ExtendedProPhotoRGB> {
    static CGColorSpaceRef colorSpace()
    {
        return extendedROMMRGBColorSpaceSingleton();
    }
};

WEBCORE_EXPORT CGColorSpaceRef extendedSRGBColorSpaceSingleton();
template<> struct CGColorSpaceMapping<ColorSpace::ExtendedSRGB> {
    static CGColorSpaceRef colorSpace()
    {
        return extendedSRGBColorSpaceSingleton();
    }
};

WEBCORE_EXPORT CGColorSpaceRef ITUR_2020ColorSpaceSingleton();
template<> struct CGColorSpaceMapping<ColorSpace::Rec2020> {
    static CGColorSpaceRef colorSpace()
    {
        return ITUR_2020ColorSpaceSingleton();
    }
};

WEBCORE_EXPORT CGColorSpaceRef linearSRGBColorSpaceSingleton();
template<> struct CGColorSpaceMapping<ColorSpace::LinearSRGB> {
    static CGColorSpaceRef colorSpace()
    {
        return linearSRGBColorSpaceSingleton();
    }
};

WEBCORE_EXPORT CGColorSpaceRef ROMMRGBColorSpaceSingleton();
template<> struct CGColorSpaceMapping<ColorSpace::ProPhotoRGB> {
    static CGColorSpaceRef colorSpace()
    {
        return ROMMRGBColorSpaceSingleton();
    }
};

WEBCORE_EXPORT CGColorSpaceRef xyzD50ColorSpaceSingleton();
template<> struct CGColorSpaceMapping<ColorSpace::XYZ_D50> {
    static CGColorSpaceRef colorSpace()
    {
        return xyzD50ColorSpaceSingleton();
    }
};

// FIXME: Add support for these once/if CoreGraphics adds support for them.
template<> struct CGColorSpaceMapping<ColorSpace::HSL> { };
template<> struct CGColorSpaceMapping<ColorSpace::HWB> { };
template<> struct CGColorSpaceMapping<ColorSpace::LCH> { };
template<> struct CGColorSpaceMapping<ColorSpace::Lab> { };
template<> struct CGColorSpaceMapping<ColorSpace::OKLab> { };
template<> struct CGColorSpaceMapping<ColorSpace::OKLCH> { };
template<> struct CGColorSpaceMapping<ColorSpace::XYZ_D65> { };


WEBCORE_EXPORT std::optional<ColorSpace> colorSpaceForCGColorSpace(CGColorSpaceRef);


template<ColorSpace, typename = void> inline constexpr bool HasCGColorSpaceMapping = false;
template<ColorSpace space> inline constexpr bool HasCGColorSpaceMapping<space, std::void_t<decltype(CGColorSpaceMapping<space>::colorSpace())>> = true;
static_assert(HasCGColorSpaceMapping<ColorSpace::SRGB>, "An SRGB color space mapping must be supported on all platforms.");

template<ColorSpace space, bool = HasCGColorSpaceMapping<space>> struct CGColorSpaceMappingOrNullGetter { static CGColorSpaceRef colorSpace() { return nullptr; } };
template<ColorSpace space> struct CGColorSpaceMappingOrNullGetter<space, true> { static CGColorSpaceRef colorSpace() { return CGColorSpaceMapping<space>::colorSpace(); } };

template<ColorSpace space> CGColorSpaceRef cachedCGColorSpace()
{
    return CGColorSpaceMapping<space>::colorSpace();
}

template<ColorSpace space> CGColorSpaceRef cachedNullableCGColorSpace()
{
    return CGColorSpaceMappingOrNullGetter<space>::colorSpace();
}

inline CGColorSpaceRef cachedNullableCGColorSpace(ColorSpace colorSpace)
{
    switch (colorSpace) {
    case ColorSpace::A98RGB:
        return cachedNullableCGColorSpace<ColorSpace::A98RGB>();
    case ColorSpace::DisplayP3:
        return cachedNullableCGColorSpace<ColorSpace::DisplayP3>();
    case ColorSpace::ExtendedA98RGB:
        return cachedNullableCGColorSpace<ColorSpace::ExtendedA98RGB>();
    case ColorSpace::ExtendedDisplayP3:
        return cachedNullableCGColorSpace<ColorSpace::ExtendedDisplayP3>();
    case ColorSpace::ExtendedLinearSRGB:
        return cachedNullableCGColorSpace<ColorSpace::ExtendedLinearSRGB>();
    case ColorSpace::ExtendedProPhotoRGB:
        return cachedNullableCGColorSpace<ColorSpace::ExtendedProPhotoRGB>();
    case ColorSpace::ExtendedRec2020:
        return cachedNullableCGColorSpace<ColorSpace::ExtendedRec2020>();
    case ColorSpace::ExtendedSRGB:
        return cachedNullableCGColorSpace<ColorSpace::ExtendedSRGB>();
    case ColorSpace::HSL:
        return cachedNullableCGColorSpace<ColorSpace::HSL>();
    case ColorSpace::HWB:
        return cachedNullableCGColorSpace<ColorSpace::HWB>();
    case ColorSpace::LCH:
        return cachedNullableCGColorSpace<ColorSpace::LCH>();
    case ColorSpace::Lab:
        return cachedNullableCGColorSpace<ColorSpace::Lab>();
    case ColorSpace::LinearSRGB:
        return cachedNullableCGColorSpace<ColorSpace::LinearSRGB>();
    case ColorSpace::OKLCH:
        return cachedNullableCGColorSpace<ColorSpace::OKLCH>();
    case ColorSpace::OKLab:
        return cachedNullableCGColorSpace<ColorSpace::OKLab>();
    case ColorSpace::ProPhotoRGB:
        return cachedNullableCGColorSpace<ColorSpace::ProPhotoRGB>();
    case ColorSpace::Rec2020:
        return cachedNullableCGColorSpace<ColorSpace::Rec2020>();
    case ColorSpace::SRGB:
        return cachedNullableCGColorSpace<ColorSpace::SRGB>();
    case ColorSpace::XYZ_D50:
        return cachedNullableCGColorSpace<ColorSpace::XYZ_D50>();
    case ColorSpace::XYZ_D65:
        return cachedNullableCGColorSpace<ColorSpace::XYZ_D65>();
    }

    ASSERT_NOT_REACHED();
    return nullptr;
}

}
