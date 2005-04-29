/* $XFree86: xc/programs/Xserver/hw/xfree86/drivers/ati/ativgaio.h,v 1.6 2004/01/05 16:42:05 tsi Exp $ */
/*
 * Copyright 2000 through 2004 by Marc Aurele La France (TSI @ UQV), tsi@xfree86.org
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

#ifndef ___ATIVGAIO_H___

#if !defined(___ATI_H___) && defined(XFree86Module)
# error missing #include "ati.h" before #include "ativgaio.h"
# undef XFree86Module
#endif

#define ___ATIVGAIO_H___ 1

#include "atiio.h"
#include "atipriv.h"
#include "atiproto.h"

#ifndef AVOID_CPIO

extern void ATISetVGAIOBase FunctionPrototype((ATIPtr, const CARD8));

/* Odds and ends to ease reading and writting of indexed registers */
#define GetReg(_Register, _Index) \
    (                             \
        outb(_Register, _Index),  \
        inb((_Register) + 1)      \
    )
#define PutReg(_Register, _Index, _Value) \
    do                                    \
    {                                     \
        outb(_Register, _Index);          \
        outb((_Register) + 1, _Value);    \
    } while (0)

#endif /* AVOID_CPIO */

#endif /* ___ATIVGAIO_H___ */
