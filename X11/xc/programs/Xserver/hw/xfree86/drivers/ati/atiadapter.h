/* $XFree86: xc/programs/Xserver/hw/xfree86/drivers/ati/atiadapter.h,v 1.11 2004/01/05 16:42:00 tsi Exp $ */
/*
 * Copyright 1997 through 2004 by Marc Aurele La France (TSI @ UQV), tsi@xfree86.org
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

#ifndef ___ATIADAPTER_H___
#define ___ATIADAPTER_H___ 1

/*
 * Adapter-related definitions.
 */
typedef enum
{
    ATI_ADAPTER_NONE = 0,

#ifndef AVOID_CPIO

    ATI_ADAPTER_EGA,
    ATI_ADAPTER_EGA_PLUS,
    ATI_ADAPTER_VGA,
    ATI_ADAPTER_BASIC,
    ATI_ADAPTER_V3,
    ATI_ADAPTER_V4,
    ATI_ADAPTER_V5,
    ATI_ADAPTER_PLUS,
    ATI_ADAPTER_XL,
    ATI_ADAPTER_NONISA,
    ATI_ADAPTER_8514A,
    ATI_ADAPTER_MACH8,
    ATI_ADAPTER_MACH32,

#endif /* AVOID_CPIO */

    ATI_ADAPTER_MACH64,
    ATI_ADAPTER_RAGE128,
    ATI_ADAPTER_RADEON,
    ATI_ADAPTER_MAX     /* Must be last */
} ATIAdapterType;

extern const char *ATIAdapterNames[];

#endif /* ___ATIADAPTER_H___ */
