/*
 * Attribute setting for the Creator and Creator3D framebuffer.
 *
 * Copyright (C) 1999 David S. Miller (davem@redhat.com)
 * Copyright (C) 1999 Jakub Jelinek (jakub@redhat.com)
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
/* $XFree86: xc/programs/Xserver/hw/xfree86/drivers/sunffb/ffb_attr.c,v 1.2 2000/05/23 04:47:44 dawes Exp $ */

#include "ffb.h"
#include "ffb_fifo.h"
#include "ffb_rcache.h"

/* If we're going to write any attributes, write them all. */
void __FFB_Attr_Raw(FFBPtr pFfb, unsigned int ppc, unsigned int ppc_mask,
		    unsigned int pmask, unsigned int rop,
		    int drawop, int fg, unsigned int fbc, unsigned int wid)
{
	ffb_fbcPtr ffb = pFfb->regs;

	FFBLOG(("WRATTRS_RAW: PPC[%08x:%08x] PMSK[%08x] ROP[%08x] DOP[%08x] FG[%08x] FBC[%08x] WID[%02x]\n",
		ppc, ppc_mask, pmask, rop, drawop, fg, fbc, wid));
	pFfb->ppc_cache &= ~ppc_mask;
	pFfb->ppc_cache |= ppc;
	pFfb->fg_cache = fg;
	pFfb->fbc_cache = fbc;
	pFfb->wid_cache = wid;
	pFfb->rop_cache = rop;
	pFfb->pmask_cache = pmask;
	pFfb->drawop_cache = drawop;
	pFfb->rp_active = 1;
	FFBFifo(pFfb, 7);
	ffb->ppc = ppc;
	ffb->fg = fg;
	ffb->fbc = fbc;
	ffb->wid = wid;
	ffb->rop = rop;
	ffb->pmask = pmask;
	ffb->drawop = drawop;
}

void __FFB_Attr_SFB_VAR(FFBPtr pFfb, unsigned int ppc, unsigned int ppc_mask, unsigned int fbc,
			unsigned int wid, unsigned int rop, unsigned int pmask)
{
	ffb_fbcPtr ffb = pFfb->regs;

	FFBLOG(("WRATTRS_SFBVAR: PPC[%08x:%08x] PMSK[%08x] ROP[%08x] FBC[%08x] WID[%02x]\n",
		ppc, ppc_mask, pmask, rop, fbc, wid));
	pFfb->ppc_cache &= ~ppc_mask;
	pFfb->ppc_cache |= ppc;
	pFfb->fbc_cache = fbc;
	pFfb->wid_cache = wid;
	pFfb->rop_cache = rop;
	pFfb->pmask_cache = pmask;
	pFfb->rp_active = 1;
	FFBFifo(pFfb, 5);
	ffb->ppc = ppc;
	ffb->fbc = fbc;
	ffb->wid = wid;
	ffb->rop = rop;
	ffb->pmask = pmask;
}

#define NEED_PPC	0x00000001
#define NEED_PMASK	0x00000002
#define NEED_ROP	0x00000004
#define NEED_DRAWOP	0x00000008
#define NEED_FG		0x00000010
#define NEED_BG		0x00000020
#define NEED_FBC	0x00000040
#define NEED_WID	0x00000080

void __FFB_Attr_GC(FFBPtr pFfb, GCPtr pGC, WindowPtr pWin, unsigned int ppc, int drawop)
{
	ffb_fbcPtr ffb = pFfb->regs;
	unsigned int rop, need_mask, need_count;

	need_mask = need_count = 0;
	if ((pFfb->ppc_cache & FFB_PPC_GCMASK) != ppc) {
		unsigned int newppc = pFfb->ppc_cache & ~FFB_PPC_GCMASK;

		newppc |= (ppc & FFB_PPC_GCMASK);
		pFfb->ppc_cache = newppc;
		need_mask |= NEED_PPC;
		need_count++;
	}

	if (pFfb->pmask_cache != pGC->planemask) {
		pFfb->pmask_cache = pGC->planemask;
		need_mask |= NEED_PMASK;
		need_count++;
	}

	rop = (pGC->alu | FFB_ROP_EDIT_BIT)|(FFB_ROP_NEW<<8);
	if (pFfb->rop_cache != rop) {
		pFfb->rop_cache = rop;
		need_mask |= NEED_ROP;
		need_count++;
	}

	if (pFfb->drawop_cache != drawop) {
		pFfb->drawop_cache = drawop;
		need_mask |= NEED_DRAWOP;
		need_count++;
	}

	if (pFfb->fg_cache != pGC->fgPixel) {
		pFfb->fg_cache = pGC->fgPixel;
		need_mask |= NEED_FG;
		need_count++;
	}

	{
		CreatorPrivWinPtr WinPriv = CreatorGetWindowPrivate(pWin);
		unsigned int fbc = WinPriv->fbc_base;

		fbc &= ~FFB_FBC_XE_MASK;
		fbc |= FFB_FBC_XE_OFF;

		if (pFfb->fbc_cache != fbc) {
			pFfb->fbc_cache = fbc;
			need_mask |= NEED_FBC;
			need_count++;
		}

	}
	pFfb->rp_active = 1;

	FFBLOG(("WRATTRS_GC: PPC[%08x:%08x] PMSK[%08x] ROP[%08x] "
		"DOP[%08x] FG[%08x] FBC[%08x]\n",
		pFfb->ppc_cache & FFB_PPC_GCMASK, FFB_PPC_GCMASK,
		pFfb->pmask_cache, pFfb->rop_cache,
		pFfb->drawop_cache, pFfb->fg_cache, pFfb->fbc_cache));

	FFBFifo(pFfb, need_count);
	if (need_mask & NEED_PPC)
		ffb->ppc = (pFfb->ppc_cache & FFB_PPC_GCMASK);
	if (need_mask & NEED_PMASK)
		ffb->pmask = pFfb->pmask_cache;
	if (need_mask & NEED_ROP)
		ffb->rop = pFfb->rop_cache;
	if (need_mask & NEED_DRAWOP)
		ffb->drawop = pFfb->drawop_cache;
	if (need_mask & NEED_FG)
		ffb->fg = pFfb->fg_cache;
	if (need_mask & NEED_FBC)
		ffb->fbc = pFfb->fbc_cache;
}

void __FFB_Attr_FastfillWin(FFBPtr pFfb, WindowPtr pWin,
			    unsigned int ppc, unsigned int pixel)
{
	ffb_fbcPtr ffb = pFfb->regs;
	unsigned int rop, need_mask, need_count;

	need_mask = need_count = 0;
	if ((pFfb->ppc_cache & FFB_PPC_WINMASK) != ppc) {
		unsigned int newppc = pFfb->ppc_cache & ~FFB_PPC_WINMASK;

		newppc |= (ppc & FFB_PPC_WINMASK);
		pFfb->ppc_cache = newppc;
		need_mask |= NEED_PPC;
		need_count++;
	}

	if (pFfb->pmask_cache != 0x00ffffff) {
		pFfb->pmask_cache = 0x00ffffff;
		need_mask |= NEED_PMASK;
		need_count++;
	}

	rop = FFB_ROP_NEW | (FFB_ROP_NEW<<8);
	if (pFfb->rop_cache != rop) {
		pFfb->rop_cache = rop;
		need_mask |= NEED_ROP;
		need_count++;
	}

	if (pFfb->drawop_cache != FFB_DRAWOP_FASTFILL) {
		pFfb->drawop_cache = FFB_DRAWOP_FASTFILL;
		need_mask |= NEED_DRAWOP;
		need_count++;
	}

	if (pFfb->fg_cache != pixel) {
		pFfb->fg_cache = pixel;
		need_mask |= NEED_FG;
		need_count++;
	}

	{
		CreatorPrivWinPtr pWinPriv = CreatorGetWindowPrivate(pWin);
		unsigned int fbc = pWinPriv->fbc_base;

		if (pFfb->has_double_buffer) {
			fbc &= ~FFB_FBC_WB_MASK;
			fbc |= FFB_FBC_WB_AB;
		}
		fbc &= ~(FFB_FBC_XE_MASK | FFB_FBC_RGBE_MASK);
		fbc |= FFB_FBC_XE_ON | FFB_FBC_RGBE_ON;
		if (pFfb->ffb_res == ffb_res_high)
			fbc |= FFB_FBC_WB_B;

		if (pFfb->fbc_cache != fbc) {
			pFfb->fbc_cache = fbc;
			need_mask |= NEED_FBC;
			need_count++;
		}

		if (pFfb->wid_cache != pWinPriv->wid) {
			pFfb->wid_cache = pWinPriv->wid;
			need_mask |= NEED_WID;
			need_count++;
		}
	}

	pFfb->rp_active = 1;

	FFBLOG(("WRATTRS_GC: PPC[%08x:%08x] PMSK[%08x] ROP[%08x] DOP[%08x] FG[%08x] FBC[%08x] WID[%02x]\n",
		pFfb->ppc_cache & FFB_PPC_WINMASK, FFB_PPC_WINMASK,
		pFfb->pmask_cache, pFfb->rop_cache,
		pFfb->drawop_cache, pFfb->fg_cache, pFfb->fbc_cache, pFfb->wid_cache));

	FFBFifo(pFfb, need_count);
	if (need_mask & NEED_PPC)
		ffb->ppc = (pFfb->ppc_cache & FFB_PPC_WINMASK);
	if (need_mask & NEED_PMASK)
		ffb->pmask = pFfb->pmask_cache;
	if (need_mask & NEED_ROP)
		ffb->rop = pFfb->rop_cache;
	if (need_mask & NEED_DRAWOP)
		ffb->drawop = pFfb->drawop_cache;
	if (need_mask & NEED_FG)
		ffb->fg = pFfb->fg_cache;
	if (need_mask & NEED_FBC)
		ffb->fbc = pFfb->fbc_cache;
	if (need_mask & NEED_WID)
		ffb->wid = pFfb->wid_cache;
}
