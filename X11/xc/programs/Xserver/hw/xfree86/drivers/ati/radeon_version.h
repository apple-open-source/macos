/* $XFree86: xc/programs/Xserver/hw/xfree86/drivers/ati/radeon_version.h,v 1.8 2003/01/01 19:16:35 tsi Exp $ */
/*
 * Copyright 2000 through 2003 by Marc Aurele La France (TSI @ UQV), tsi@xfree86.org
 *
 * Permission to use, copy, modify, distribute, and sell this software and its
 * documentation for any purpose is hereby granted without fee, provided that
 * the above copyright notice appear in all copies and that both that copyright
 * notice and this permission notice appear in supporting documentation, and
 * that the name of Marc Aurele La France not be used in advertising or
 * publicity pertaining to distribution of the software without specific,
 * written prior permission.  Marc Aurele La France makes no representations
 * about the suitability of this software for any purpose.  It is provided
 * "as-is" without express or implied warranty.
 *
 * MARC AURELE LA FRANCE DISCLAIMS ALL WARRANTIES WITH REGARD TO THIS SOFTWARE,
 * INCLUDING ALL IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS.  IN NO
 * EVENT SHALL MARC AURELE LA FRANCE BE LIABLE FOR ANY SPECIAL, INDIRECT OR
 * CONSEQUENTIAL DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM LOSS OF USE,
 * DATA OR PROFITS, WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE OR OTHER
 * TORTIOUS ACTION, ARISING OUT OF OR IN CONNECTION WITH THE USE OR
 * PERFORMANCE OF THIS SOFTWARE.
 */

#ifndef _RADEON_VERSION_H_
#define _RADEON_VERSION_H_ 1

#undef  RADEON_NAME
#undef  RADEON_DRIVER_NAME
#undef  R200_DRIVER_NAME
#undef  RADEON_VERSION_MAJOR
#undef  RADEON_VERSION_MINOR
#undef  RADEON_VERSION_PATCH
#undef  RADEON_VERSION_CURRENT
#undef  RADEON_VERSION_EVALUATE
#undef  RADEON_VERSION_STRINGIFY
#undef  RADEON_VERSION_NAME

#define RADEON_NAME          "RADEON"
#define RADEON_DRIVER_NAME   "radeon"
#define R200_DRIVER_NAME     "r200"
#define RV250_DRIVER_NAME    "r200"

#define RADEON_VERSION_MAJOR 4
#define RADEON_VERSION_MINOR 0
#define RADEON_VERSION_PATCH 1

#ifndef RADEON_VERSION_EXTRA
#define RADEON_VERSION_EXTRA ""
#endif

#define RADEON_VERSION_CURRENT \
    ((RADEON_VERSION_MAJOR << 20) | \
     (RADEON_VERSION_MINOR << 10) | \
     (RADEON_VERSION_PATCH))

#define RADEON_VERSION_EVALUATE(__x) #__x
#define RADEON_VERSION_STRINGIFY(_x) RADEON_VERSION_EVALUATE(_x)
#define RADEON_VERSION_NAME                                             \
    RADEON_VERSION_STRINGIFY(RADEON_VERSION_MAJOR) "."                  \
    RADEON_VERSION_STRINGIFY(RADEON_VERSION_MINOR) "."                  \
    RADEON_VERSION_STRINGIFY(RADEON_VERSION_PATCH) RADEON_VERSION_EXTRA

#endif /* _RADEON_VERSION_H_ */
