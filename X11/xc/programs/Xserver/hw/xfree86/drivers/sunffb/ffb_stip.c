/*
 * Acceleration for the Creator and Creator3D framebuffer - stipple setting.
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
 *
 */
/* $XFree86: xc/programs/Xserver/hw/xfree86/drivers/sunffb/ffb_stip.c,v 1.2 2000/05/23 04:47:45 dawes Exp $ */

#include "ffb.h"
#include "ffb_regs.h"
#include "ffb_rcache.h"
#include "ffb_fifo.h"
#include "ffb_stip.h"
#include "ffb_loops.h"

void
FFBSetStipple(FFBPtr pFfb, ffb_fbcPtr ffb,
	      CreatorStipplePtr stipple,
	      unsigned int ppc, unsigned int ppc_mask)
{
	int transparent = ((stipple->alu & FFB_ROP_EDIT_BIT) != 0);

	if(transparent)
		ppc |= FFB_PPC_APE_ENABLE|FFB_PPC_TBE_TRANSPARENT;
	else
		ppc |= FFB_PPC_APE_ENABLE|FFB_PPC_TBE_OPAQUE;
	ppc_mask |= FFB_PPC_APE_MASK|FFB_PPC_TBE_MASK;
	FFB_WRITE_PPC(pFfb, ffb, ppc, ppc_mask);
	FFB_WRITE_ROP(pFfb, ffb,
		      (FFB_ROP_EDIT_BIT|stipple->alu)| (FFB_ROP_NEW<<8));

	if (stipple->inhw && pFfb->laststipple == stipple) {
		FFB_WRITE_FG(pFfb, ffb, stipple->fg);
		if(!transparent)
			FFB_WRITE_BG(pFfb, ffb, stipple->bg);
		return;
	}
	FFBFifo(pFfb, 32);
	FFB_STIPPLE_LOAD(&ffb->pattern[0], &stipple->bits[0]);
	FFB_WRITE_FG(pFfb, ffb, stipple->fg);
	if(!transparent)
		FFB_WRITE_BG(pFfb, ffb, stipple->bg);
	stipple->inhw = 1;
	pFfb->laststipple = stipple;
}
