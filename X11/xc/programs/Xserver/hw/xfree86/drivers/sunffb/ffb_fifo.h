/*
 * Acceleration for the Creator and Creator3D framebuffer - fifo macros.
 *
 * Copyright (C) 1998,1999 Jakub Jelinek (jakub@redhat.com)
 * Copyright (C) 1999 David S. Miller (davem@redhat.com)
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL
 * JAKUB JELINEK OR DAVID MILLER BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS
 * IN THE SOFTWARE.
 */
/* $XFree86: xc/programs/Xserver/hw/xfree86/drivers/sunffb/ffb_fifo.h,v 1.2 2000/05/23 04:47:44 dawes Exp $ */

#ifndef FFBFIFO_H
#define FFBFIFO_H

#include "ffb.h"

/* This is the smallest FFB fifo size I know of. -DaveM */
#define FFB_FIFO_MIN	124

#define FFBFifo(__fpriv, __n) \
do { 	int __cur_slots = (__fpriv)->fifo_cache; \
	if((__cur_slots - (__n)) < 0) { \
		ffb_fbcPtr __ffb = pFfb->regs; \
		do {	__cur_slots = (((int)__ffb->ucsr & FFB_UCSR_FIFO_MASK) - 4); \
		} while((__cur_slots - (__n)) < 0); \
	} (__fpriv)->fifo_cache = (__cur_slots - (__n)); \
} while(0)

/* While we are polling for the raster processor to idle, cache the
 * fifo count as well.
 */
#define FFBWait(__fpriv, __ffb) \
if ((__fpriv)->rp_active != 0) { \
	unsigned int __regval = (__ffb)->ucsr; \
	while((__regval & FFB_UCSR_RP_BUSY) != 0) { \
		__regval = (__ffb)->ucsr; \
	} \
	(__fpriv)->fifo_cache = ((int)(__regval & FFB_UCSR_FIFO_MASK)) - 4; \
	(__fpriv)->rp_active = 0; \
} while(0)

/* DEBUGGING:  You can use this if you suspect corruption is occuring
 *             because someone is touching the framebuffer while the
 *	       raster processor is active.  If you enable this and the
 *	       problem goes away, odds are your suspicions are correct.
 */
#undef FORCE_WAIT_EVERY_ROP
#ifdef FORCE_WAIT_EVERY_ROP
#define FFBSync(__fpriv, __ffb)	FFBWait(__fpriv, __ffb)
#else
#define FFBSync(__fpriv, __ffb)	do { } while(0)
#endif

#endif /* FFBFIFO_H */
