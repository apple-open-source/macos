/* $XFree86: xc/programs/Xserver/hw/xfree86/drivers/ati/atituner.h,v 1.2 2004/01/05 16:42:05 tsi Exp $ */
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

#ifndef ___ATITUNER_H___
#define ___ATITUNER_H___ 1

#include "xf86str.h"

/*
 * TV Tuner definitions.  Most of these are from Philips.
 */
typedef enum
{
    ATI_TUNER_NONE,
    ATI_TUNER_FI1236MK1NA,
    ATI_TUNER_FI1236MK2J,
    ATI_TUNER_FI1216MK2BG,
    ATI_TUNER_FI1246MK2I,
    ATI_TUNER_FI1216MFMK2,
    ATI_TUNER_FI1236MK2NA,
    ATI_TUNER_FI1256MK2DK,
    ATI_TUNER_FM1236MK2NA,
    ATI_TUNER_FI1216MK2BGEXT,
    ATI_TUNER_FI1246MK2IEXT,
    ATI_TUNER_FI1216MFMK2EXT,
    ATI_TUNER_FI1236MK2NAEXT,
    ATI_TUNER_TEMIC_FN5AL,
    ATI_TUNER_FQ1216MEP,
    ATI_TUNER_15,
    ATI_TUNER_ALPS_TSBH5,
    ATI_TUNER_ALPS_TSCXX,
    ATI_TUNER_ALPS_TSCH5,
    ATI_TUNER_19,
    ATI_TUNER_20,
    ATI_TUNER_21,
    ATI_TUNER_22,
    ATI_TUNER_23,
    ATI_TUNER_24,
    ATI_TUNER_25,
    ATI_TUNER_26,
    ATI_TUNER_27,
    ATI_TUNER_28,
    ATI_TUNER_MT2032,
    ATI_TUNER_30,
    ATI_TUNER_31
} ATITunerType;

extern const SymTabRec ATITuners[];

#endif /* ___ATITUNER_H___ */
