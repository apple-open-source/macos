/* $XFree86: xc/programs/Xserver/hw/xfree86/drivers/ati/atiaudio.c,v 1.2 2004/01/05 16:42:00 tsi Exp $ */
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

#include "atiaudio.h"

/*
 * Audio chip definitions.
 */
const char *ATIAudioNames[] =
{
    "Philips TEA5582",
    "Mono with audio mux",
    "Philips TDA9850",
    "Sony CXA2020S",
    "ITT MSP3410D",
    "Crystal CS4236B",
    "Philips TDA9851",
    "ITT MSP3415",
    "ITT MSP3430",
    "Unknown type (9)",
    "Unknown type (10)",
    "Unknown type (11)",
    "Unknown type (12)",
    "Unknown type (13)",
    "Unknown type (14)",
    "No audio"
};
