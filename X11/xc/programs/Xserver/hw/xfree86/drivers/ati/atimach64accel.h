/* $XFree86: xc/programs/Xserver/hw/xfree86/drivers/ati/atimach64accel.h,v 1.2 2004/01/05 16:42:02 tsi Exp $ */
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

#ifndef ___ATIMACH64ACCEL_H___
#define ___ATIMACH64ACCEL_H___ 1

#include "atipriv.h"
#include "atiproto.h"

#include "xaa.h"

#define ATIMach64MaxX  8191
#define ATIMach64MaxY 32767

extern int  ATIMach64AccelInit FunctionPrototype((ATIPtr, XAAInfoRecPtr));
extern void ATIMach64Sync      FunctionPrototype((ScrnInfoPtr));

#endif /* ___ATIMACH64ACCEL_H___ */
