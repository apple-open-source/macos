/* $XFree86: xc/programs/Xserver/hw/xfree86/drivers/ati/atiio.h,v 1.14 2003/01/01 19:16:32 tsi Exp $ */
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

#ifndef ___ATIIO_H___

#if !defined(___ATI_H___) && defined(XFree86Module)
# error missing #include "ati.h" before #include "atiio.h"
# undef XFree86Module
#endif

#define ___ATIIO_H___ 1

#include "atiregs.h"

#include "compiler.h"

/* I/O decoding definitions */
typedef enum
{
    SPARSE_IO,
    BLOCK_IO
} ATIIODecodingType;

#ifndef AVOID_CPIO

/* Wait until "n" queue entries are free */
#define ibm8514WaitQueue(_n)                      \
    {                                             \
        while (inw(GP_STAT) & (0x0100U >> (_n))); \
    }
#define ATIWaitQueue(_n)                                    \
    {                                                       \
        while (inw(EXT_FIFO_STATUS) & (0x010000U >> (_n))); \
    }

/* Wait until GP is idle and queue is empty */
#define WaitIdleEmpty()                      \
    {                                        \
        while (inw(GP_STAT) & (GPBUSY | 1)); \
    }
#define ProbeWaitIdleEmpty()              \
    {                                     \
        int _i;                           \
        CARD16 _value;                    \
        for (_i = 0;  _i < 100000;  _i++) \
        {                                 \
            _value = inw(GP_STAT);        \
            if (_value == (CARD16)(-1))   \
                break;                    \
            if (!(_value & (GPBUSY | 1))) \
                break;                    \
        }                                 \
    }

/* Wait until GP has data available */
#define WaitDataReady()                    \
    {                                      \
        while (!(inw(GP_STAT) & DATARDY)); \
    }

#endif /* AVOID_CPIO */

#endif /* ___ATIIO_H___ */
