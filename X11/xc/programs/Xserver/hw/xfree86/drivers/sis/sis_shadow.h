/* $XFree86: xc/programs/Xserver/hw/xfree86/drivers/sis/sis_shadow.h,v 1.8 2004/01/04 18:08:00 twini Exp $ */
/*
 * Copyright (C) 1999-2004 by The XFree86 Project, Inc.
 *
 * Licensed under the following terms:
 *
 * Permission to use, copy, modify, distribute, and sell this software and its
 * documentation for any purpose is hereby granted without fee, provided that
 * the above copyright notice appears in all copies and that both that copyright
 * notice and this permission notice appear in supporting documentation, and
 * and that the name of the copyright holder not be used in advertising
 * or publicity pertaining to distribution of the software without specific,
 * written prior permission. The copyright holder makes no representations
 * about the suitability of this software for any purpose.  It is provided
 * "as is" without expressed or implied warranty.
 *
 * THE COPYRIGHT HOLDER DISCLAIMS ALL WARRANTIES WITH REGARD TO THIS SOFTWARE,
 * INCLUDING ALL IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS. IN NO
 * EVENT SHALL THE COPYRIGHT HOLDER BE LIABLE FOR ANY SPECIAL, INDIRECT OR
 * CONSEQUENTIAL DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM LOSS OF USE,
 * DATA OR PROFITS, WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE OR OTHER
 * TORTIOUS ACTION, ARISING OUT OF OR IN CONNECTION WITH THE USE OR
 * PERFORMANCE OF THIS SOFTWARE.
 *
 */

void SISPointerMoved(int index, int x, int y);
void SISRefreshArea(ScrnInfoPtr pScrn, int num, BoxPtr pbox);
void SISRefreshArea8(ScrnInfoPtr pScrn, int num, BoxPtr pbox);
void SISRefreshArea16(ScrnInfoPtr pScrn, int num, BoxPtr pbox);
void SISRefreshArea24(ScrnInfoPtr pScrn, int num, BoxPtr pbox);
void SISRefreshArea32(ScrnInfoPtr pScrn, int num, BoxPtr pbox);
