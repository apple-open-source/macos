/* $XFree86: xc/programs/Xserver/hw/xfree86/drivers/ati/ativersion.h,v 1.58 2003/01/10 20:57:58 tsi Exp $ */
/*
 * Copyright 1997 through 2003 by Marc Aurele La France (TSI @ UQV), tsi@xfree86.org
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

#ifndef ___ATIVERSION_H___
#define ___ATIVERSION_H___ 1

#undef  ATI_NAME
#undef  ATI_DRIVER_NAME
#undef  ATI_VERSION_MAJOR
#undef  ATI_VERSION_MINOR
#undef  ATI_VERSION_PATCH
#undef  ATI_VERSION_CURRENT
#undef  ATI_VERSION_EVALUATE
#undef  ATI_VERSION_STRINGIFY
#undef  ATI_VERSION_NAME

#define ATI_NAME          "ATI"
#define ATI_DRIVER_NAME   "ati"

#define ATI_VERSION_MAJOR 6
#define ATI_VERSION_MINOR 4
#define ATI_VERSION_PATCH 18

#ifndef ATI_VERSION_EXTRA
#define ATI_VERSION_EXTRA ""
#endif

#define ATI_VERSION_CURRENT \
    ((ATI_VERSION_MAJOR << 20) | (ATI_VERSION_MINOR << 10) | ATI_VERSION_PATCH)

#define ATI_VERSION_EVALUATE(__x) #__x
#define ATI_VERSION_STRINGIFY(_x) ATI_VERSION_EVALUATE(_x)
#define ATI_VERSION_NAME                                        \
    ATI_VERSION_STRINGIFY(ATI_VERSION_MAJOR) "."                \
    ATI_VERSION_STRINGIFY(ATI_VERSION_MINOR) "."                \
    ATI_VERSION_STRINGIFY(ATI_VERSION_PATCH) ATI_VERSION_EXTRA

#endif /* ___ATIVERSION_H___ */
