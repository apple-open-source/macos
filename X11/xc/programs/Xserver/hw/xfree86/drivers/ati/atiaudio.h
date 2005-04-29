/* $XFree86: xc/programs/Xserver/hw/xfree86/drivers/ati/atiaudio.h,v 1.2 2004/01/05 16:42:00 tsi Exp $ */
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

#ifndef ___ATIAUDIO_H___
#define ___ATIAUDIO_H___ 1

/*
 * Audio chip definitions.
 */
typedef enum
{
    ATI_AUDIO_TEA5582,
    ATI_AUDIO_MONO,
    ATI_AUDIO_TDA9850,
    ATI_AUDIO_CXA2020S,
    ATI_AUDIO_MSP3410D,
    ATI_AUDIO_CS4236B,
    ATI_AUDIO_TDA9851,
    ATI_AUDIO_MSP3415,
    ATI_AUDIO_MSP3430,
    ATI_AUDIO_9,
    ATI_AUDIO_10,
    ATI_AUDIO_11,
    ATI_AUDIO_12,
    ATI_AUDIO_13,
    ATI_AUDIO_14,
    ATI_AUDIO_NONE
} ATIAudioType;

extern const char *ATIAudioNames[];

#endif /* ___ATIAUDIO_H___ */
