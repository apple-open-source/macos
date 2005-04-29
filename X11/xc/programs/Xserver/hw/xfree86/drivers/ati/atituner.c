/* $XFree86: xc/programs/Xserver/hw/xfree86/drivers/ati/atituner.c,v 1.2 2004/01/05 16:42:04 tsi Exp $ */
/*
 * Copyright 2003 through 2004 by Marc Aurele La France (TSI @ UQV), tsi@xfree86.org
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

#include "atituner.h"

/* Temporary interface glitch */
#if 0
# include "fi12x6.h"
#else
    typedef enum {
        FI12x6_TYPE_UNKNOWN = -1,
        FI12x6_TYPE_FI1236 = 0,
        FI12x6_TYPE_FI1216,
        FI12x6_TYPE_FI1216MF,
        FI12x6_TYPE_TEMIC_FN5AL,
        FI12x6_TYPE_MT2032,
        FI12x6_TYPE_MAX             /* Must be last */
    } FI12x6TunerType;
#endif

/*
 * TV tuner definitions.
 */
const SymTabRec ATITuners[] =
{
    {
        FI12x6_TYPE_UNKNOWN,
        "No tuner"
    },
    {
        FI12x6_TYPE_FI1236,
        "Philips FI1236 MK1 NTSC M/N North America"
    },
    {
        FI12x6_TYPE_FI1236,
        "Philips FI1236 MK2 NTSC M/N Japan"
    },
    {
        FI12x6_TYPE_FI1216,
        "Philips FI1216 MK2 PAL B/G"
    },
    {
        FI12x6_TYPE_UNKNOWN,
        "Philips FI1246 MK2 PAL I"
    },
    {
        FI12x6_TYPE_FI1216MF,
        "Philips FI1216 MF MK2 PAL B/G, SECAM L/L"
    },
    {
        FI12x6_TYPE_FI1236,
        "Philips FI1236 MK2 NTSC M/N North America"
    },
    {
        FI12x6_TYPE_UNKNOWN,
        "Philips FI1256 MK2 SECAM D/K"
    },
    {
        FI12x6_TYPE_FI1236,
        "Philips FM1236 MK2 NTSC M/N North America"
    },
    {
        FI12x6_TYPE_FI1216,
        "Philips FI1216 MK2 PAL B/G - External Tuner POD"
    },
    {
        FI12x6_TYPE_UNKNOWN,
        "Philips FI1246 MK2 PAL I - External Tuner POD"
    },
    {
        FI12x6_TYPE_FI1216MF,
        "Philips FI1216 MF MK2 PAL B/G, SECAM L/L - External Tuner POD"
    },
    {
        FI12x6_TYPE_FI1236,
        "Philips FI1236 MK2 NTSC M/N North America - External Tuner POD"
    },
    {
        FI12x6_TYPE_TEMIC_FN5AL,
        "Temic FN5AL.RF3X7595 PAL I/B/G/DK & SECAM DK"
    },
    {
        FI12x6_TYPE_FI1216MF,
        "Philips FQ1216 ME/P"
    },
    {
        FI12x6_TYPE_UNKNOWN,
        "Unknown type (15)"
    },
    {
        FI12x6_TYPE_UNKNOWN,
        "Alps TSBH5 NTSC M/N North America"
    },
    {
        FI12x6_TYPE_UNKNOWN,
        "Alps TSC?? NTSC M/N North America"
    },
    {
        FI12x6_TYPE_UNKNOWN,
        "Alps TSCH5 NTSC M/N North America with FM"
    },
    {
        FI12x6_TYPE_UNKNOWN,
        "Unknown type (19)"
    },
    {
        FI12x6_TYPE_UNKNOWN,
        "Unknown type (20)"
    },
    {
        FI12x6_TYPE_UNKNOWN,
        "Unknown type (21)"
    },
    {
        FI12x6_TYPE_UNKNOWN,
        "Unknown type (22)"
    },
    {
        FI12x6_TYPE_UNKNOWN,
        "Unknown type (23)"
    },
    {
        FI12x6_TYPE_UNKNOWN,
        "Unknown type (24)"
    },
    {
        FI12x6_TYPE_UNKNOWN,
        "Unknown type (25)"
    },
    {
        FI12x6_TYPE_UNKNOWN,
        "Unknown type (26)"
    },
    {
        FI12x6_TYPE_UNKNOWN,
        "Unknown type (27)"
    },
    {
        FI12x6_TYPE_UNKNOWN,
        "Unknown type (28)"
    },
    {
        FI12x6_TYPE_MT2032,
        "Microtune MT2032"
    },
    {
        FI12x6_TYPE_UNKNOWN,
        "Unknown type (30)"
    },
    {
        FI12x6_TYPE_UNKNOWN,
        "Unknown type (31)"
    }
};
