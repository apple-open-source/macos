/*
 * Acceleration for the Creator and Creator3D framebuffer - register caching.
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
 *
 */
/* $XFree86: xc/programs/Xserver/hw/xfree86/drivers/sunffb/ffb_rcache.h,v 1.2 2000/05/23 04:47:45 dawes Exp $ */

#ifndef FFBRCACHE_H
#define FFBRCACHE_H

/* We only need to write the bits which actually change,
 * writing unnessary bits causes the operation to go more
 * slowly. -DaveM
 *
 * We used to have some hairy code here which tried to
 * avoid writing attribute bits unnecessarily.  It has been
 * removed because various two bit fields have different
 * semantics.  For example, for some the higher bit is the
 * edit bit, for others there are three state patterns
 * and zero is a special "no edit" value.  Ho hum, it was
 * a nice idea...
 */
#define FFB_WRITE_PPC(__fpriv, __ffb, __val, __chg_mask) \
do {	unsigned int oldval = (__fpriv)->ppc_cache; \
	unsigned int __t; \
	__t = (oldval & (__chg_mask)) ^ (__val); \
	if (__t) { \
		unsigned int newval = oldval & ~(__chg_mask); \
		newval |= (__val); \
		(__fpriv)->ppc_cache = newval; \
		FFBFifo((__fpriv), 1); \
		(__ffb)->ppc = (__val); \
	} \
} while(0)

#define FFB_WRITE_PMASK(__fpriv, __ffb, __val) \
do {	if((__fpriv)->pmask_cache != (__val)) { \
		(__fpriv)->pmask_cache = (__val); \
		FFBFifo((__fpriv), 1); \
		(__ffb)->pmask = (__val); \
	} \
} while(0)

#define FFB_WRITE_ROP(__fpriv, __ffb, __val) \
do {	if((__fpriv)->rop_cache != (__val)) { \
		(__fpriv)->rop_cache = (__val); \
		FFBFifo((__fpriv), 1); \
		(__ffb)->rop = (__val); \
	} \
} while(0)

#define FFB_WRITE_DRAWOP(__fpriv, __ffb, __val) \
do {	if((__fpriv)->drawop_cache != (__val)) { \
		(__fpriv)->drawop_cache = (__val); \
		FFBFifo((__fpriv), 1); \
		(__ffb)->drawop = (__val); \
	} \
} while(0)

#define FFB_WRITE_FG(__fpriv, __ffb, __val) \
do {	if((__fpriv)->fg_cache != (__val)) { \
		(__fpriv)->fg_cache = (__val); \
		FFBFifo((__fpriv), 1); \
		(__ffb)->fg = (__val); \
	} \
} while(0)

#define FFB_WRITE_BG(__fpriv, __ffb, __val) \
do {	if((__fpriv)->bg_cache != (__val)) { \
		(__fpriv)->bg_cache = (__val); \
		FFBFifo((__fpriv), 1); \
		(__ffb)->bg = (__val); \
	} \
} while(0)

#define FFB_WRITE_FONTW(__fpriv, __ffb, __val) \
do {	if((__fpriv)->fontw_cache != (__val)) { \
		(__fpriv)->fontw_cache = (__val); \
		FFBFifo((__fpriv), 1); \
		(__ffb)->fontw = (__val); \
	} \
} while(0)

#define FFB_WRITE_FONTINC(__fpriv, __ffb, __val) \
do {	if((__fpriv)->fontinc_cache != (__val)) { \
		(__fpriv)->fontinc_cache = (__val); \
		FFBFifo((__fpriv), 1); \
		(__ffb)->fontinc = (__val); \
	} \
} while(0)

#define FFB_WRITE_FBC(__fpriv, __ffb, __val) \
do {	if((__fpriv)->fbc_cache != (__val)) { \
		(__fpriv)->fbc_cache = (__val); \
		FFBFifo((__fpriv), 1); \
		(__ffb)->fbc = (__val); \
	} \
} while(0)

#define FFB_WRITE_WID(__fpriv, __ffb, __val) \
do {	if((__fpriv)->wid_cache != (__val)) { \
		(__fpriv)->wid_cache = (__val); \
		FFBFifo((__fpriv), 1); \
		(__ffb)->wid = (__val); \
	} \
} while(0)

extern void __FFB_Attr_Raw(FFBPtr pFfb,
			   unsigned int ppc,
			   unsigned int ppc_mask,
			   unsigned int pmask,
			   unsigned int rop,
			   int drawop, int fg,
			   unsigned int fbc,
			   unsigned int wid);

#define FFB_ATTR_RAW(__fpriv, __ppc, __ppc_mask, __pmask, __rop, __drawop, __fg, __fbc, __wid) \
	if((((__fpriv)->ppc_cache & (__ppc_mask)) != (__ppc))			|| \
	   ((__fpriv)->pmask_cache != (__pmask))				|| \
	   ((__fpriv)->rop_cache != (__rop))					|| \
	   (((__drawop) != -1) && ((__fpriv)->drawop_cache != (__drawop)))	|| \
	   ((__fpriv)->fg_cache != (__fg))					|| \
	   ((__fpriv)->fbc_cache != (__fbc))					|| \
	   ((__fpriv)->wid_cache != (__wid))) \
		__FFB_Attr_Raw((__fpriv), (__ppc), (__ppc_mask), (__pmask), \
			       (__rop), (__drawop), (__fg), (__fbc), (__wid))

#define FFB_PPC_GCMASK	(FFB_PPC_APE_MASK | FFB_PPC_CS_MASK)

/* This is for loading the FFB attributes for the case where
 * where most of the values come directly from the graphics
 * context and only the PPC and DRAWOP are variable.
 */
extern void __FFB_Attr_GC(FFBPtr pFfb, GCPtr pGC, WindowPtr pWin,
			  unsigned int ppc, int drawop);

#define FFB_ATTR_GC(__fpriv, __pgc, __pwin, __ppc, __drawop)		\
do {	CreatorPrivWinPtr __winpriv = CreatorGetWindowPrivate(__pwin);	\
	unsigned int __rop = ((__pgc)->alu | FFB_ROP_EDIT_BIT);		\
	unsigned int __fbc = ((__winpriv)->fbc_base);			\
	__fbc &= ~FFB_FBC_XE_MASK;					\
	__fbc |= FFB_FBC_XE_OFF;					\
	__rop |= (FFB_ROP_NEW << 8);					\
	if ((((__fpriv)->ppc_cache & FFB_PPC_GCMASK) != (__ppc))||	\
	    ((__fpriv)->pmask_cache != ((__pgc)->planemask))	||	\
	    ((__fpriv)->rop_cache != (__rop))			||	\
	    ((__fpriv)->drawop_cache != (__drawop))		||	\
	    ((__fpriv)->fg_cache != ((__pgc)->fgPixel))		||	\
	    ((__fpriv)->fbc_cache != __fbc))				\
		__FFB_Attr_GC(__fpriv, __pgc, __pwin, __ppc, __drawop);	\
} while(0)

#define FFB_PPC_WINMASK	(FFB_PPC_APE_MASK | FFB_PPC_CS_MASK | FFB_PPC_XS_MASK)

extern void __FFB_Attr_FastfillWin(FFBPtr pFfb, WindowPtr pWin,
				   unsigned int ppc, unsigned int pixel);

#define FFB_ATTR_FFWIN(__fpriv, __pwin, __ppc, __pixel) \
do {	CreatorPrivWinPtr __winpriv = CreatorGetWindowPrivate(__pwin);	\
	unsigned int ___ppc = (__ppc) | FFB_PPC_XS_WID;			\
	unsigned int fbc = (__winpriv)->fbc_base; \
	unsigned int rop = (FFB_ROP_NEW|(FFB_ROP_NEW<<8)); \
	if((__fpriv)->has_double_buffer) { \
		fbc &= ~FFB_FBC_WB_MASK; \
		fbc |= FFB_FBC_WB_AB; \
	} \
	fbc &= ~(FFB_FBC_XE_MASK | FFB_FBC_RGBE_MASK); \
	fbc |= FFB_FBC_XE_ON | FFB_FBC_RGBE_ON; \
	if (pFfb->ffb_res == ffb_res_high) \
		fbc |= FFB_FBC_WB_B; \
	if ((((__fpriv)->ppc_cache & FFB_PPC_WINMASK) != (___ppc))||	\
	    ((__fpriv)->pmask_cache != 0x00ffffff)		||	\
	    ((__fpriv)->rop_cache!= rop)			||	\
	    ((__fpriv)->drawop_cache != FFB_DRAWOP_FASTFILL)	||	\
	    ((__fpriv)->fg_cache != (__pixel))			||	\
	    ((__fpriv)->fbc_cache != fbc)			||	\
	    ((__fpriv)->wid_cache != ((__winpriv)->wid)))		\
		__FFB_Attr_FastfillWin(__fpriv, __pwin, ___ppc, __pixel);\
} while (0)

/* We have to be careful when copying windows around.  For that
 * case we will use either VIS copies or hw accelerated VSCROLL.
 * All of the planes needs to be copied in the framebuffer from
 * src to dst in order to handle child windows using different WIDs
 * than the parent window being copied.
 */

/* Setup to access the smart frame buffer (SFB) directly where the
 * pixel color comes from the cpu on writes.
 */
#define FFB_PPC_WINCOPY		(FFB_PPC_APE_DISABLE | FFB_PPC_XS_VAR | FFB_PPC_CS_VAR)
#define FFB_PPC_WINCOPY_MASK	(FFB_PPC_APE_MASK | FFB_PPC_XS_MASK | FFB_PPC_CS_MASK)
#define FFB_FBC_WINCOPY		(FFB_FBC_WB_A | FFB_FBC_WM_COMBINED | FFB_FBC_WE_FORCEON | \
				 FFB_FBC_RB_A | FFB_FBC_SB_BOTH | FFB_FBC_XE_ON | \
				 FFB_FBC_ZE_OFF | FFB_FBC_YE_OFF | FFB_FBC_RGBE_ON)

#define FFB_ATTR_SFB_VAR_WINCOPY(__fpriv) \
do {	unsigned int ppc = FFB_PPC_WINCOPY; \
	unsigned int ppc_mask = FFB_PPC_WINCOPY_MASK; \
	unsigned int rop = FFB_ROP_NEW|(FFB_ROP_NEW<<8); \
	unsigned int fbc = FFB_FBC_WINCOPY; \
	if((__fpriv)->has_double_buffer) { \
		fbc &= ~FFB_FBC_WB_MASK; \
		fbc |= FFB_FBC_WB_AB; \
	} \
	if (((__fpriv)->ppc_cache & ppc_mask) != ppc || \
	    (__fpriv)->fbc_cache != fbc || \
	    (__fpriv)->rop_cache != rop || \
	    (__fpriv)->pmask_cache != 0xffffffff) { \
		ffb_fbcPtr __ffb = (__fpriv)->regs; \
		(__fpriv)->ppc_cache &= ~ppc_mask; \
		(__fpriv)->ppc_cache |= ppc; \
		(__fpriv)->fbc_cache = fbc; \
		(__fpriv)->rop_cache = rop; \
		(__fpriv)->pmask_cache = 0xffffffff; \
		(__fpriv)->rp_active = 1; \
		FFBFifo(__fpriv, 4); \
		(__ffb)->ppc = ppc; \
		(__ffb)->fbc = fbc; \
		(__ffb)->rop = rop; \
		(__ffb)->pmask = 0xffffffff; \
		(__fpriv)->rp_active = 1; \
	} \
} while(0)

extern void __FFB_Attr_SFB_VAR(FFBPtr pFfb, unsigned int ppc, unsigned int ppc_mask, unsigned int fbc,
			       unsigned int wid, unsigned int rop, unsigned int pmask);

#define FFB_ATTR_SFB_VAR_WIN(__fpriv, __pmask, __alu, __pwin) \
do {	unsigned int ppc = FFB_PPC_APE_DISABLE | FFB_PPC_CS_VAR | FFB_PPC_XS_WID; \
	unsigned int ppc_mask = FFB_PPC_APE_MASK | FFB_PPC_CS_MASK | FFB_PPC_XS_MASK; \
	unsigned int rop = (FFB_ROP_EDIT_BIT | (__alu))|(FFB_ROP_NEW<<8); \
	unsigned int fbc = FFB_FBC_WIN(__pwin); \
	if((__fpriv)->has_double_buffer) { \
		fbc &= ~FFB_FBC_WB_MASK; \
		fbc |= FFB_FBC_WB_AB; \
	} \
	if(((__fpriv)->ppc_cache & ppc_mask) != ppc || \
	   (__fpriv)->fbc_cache != fbc || \
	   (__fpriv)->wid_cache != FFB_WID_WIN(__pwin) || \
	   (__fpriv)->rop_cache != rop || \
	   (__fpriv)->pmask_cache != (__pmask)) \
		__FFB_Attr_SFB_VAR(__fpriv, ppc, ppc_mask, fbc, \
				   FFB_WID_WIN(__pwin), rop, (__pmask)); \
} while(0)

/* VSCROLL Attributes:
 *
 * PPC) VCE_DISABLE must be set, all other attributes are effectively
 *      ignored since this drawop just copies pixels within the ram
 *      chips and bypasses the pixel processor entirely.  So you
 *	end up with a PPC color source behavior of {ZS,YS,XS,CS}_VAR.
 * FBC) all options are allowed, but the SRC/DST buffers are determined
 *      solely by the WB_* setting, that is, the RB_* setting is effectively
 *      ignored since the pixels are copied directly through the write buffer
 * ROP) must be FFB_ROP_OLD (even for the X plane!)
 * PMASK) all options allowed
 */
#define FFB_ATTR_VSCROLL_WINCOPY(__fpriv) \
do {	unsigned int rop = (FFB_ROP_OLD | (FFB_ROP_OLD << 8)); \
	unsigned int fbc = FFB_FBC_WINCOPY; \
	if((__fpriv)->has_double_buffer) { \
		fbc &= ~FFB_FBC_WB_MASK; \
		fbc |= FFB_FBC_WB_AB; \
	} \
	if((__fpriv)->fbc_cache != fbc || \
	   (__fpriv)->rop_cache != rop || \
	   (__fpriv)->pmask_cache != 0xffffffff || \
	   (__fpriv)->drawop_cache != FFB_DRAWOP_VSCROLL) { \
		ffb_fbcPtr __ffb = (__fpriv)->regs; \
		(__fpriv)->fbc_cache = fbc; \
		(__fpriv)->rop_cache = rop; \
		(__fpriv)->pmask_cache = 0xffffffff; \
		(__fpriv)->drawop_cache = FFB_DRAWOP_VSCROLL; \
		(__fpriv)->rp_active = 1; \
		FFBFifo(__fpriv, 4); \
		(__ffb)->fbc = fbc; \
		(__ffb)->rop = rop; \
		(__ffb)->pmask = 0xffffffff; \
		(__ffb)->drawop = FFB_DRAWOP_VSCROLL; \
		(__fpriv)->rp_active = 1; \
	} \
} while(0)

#define FFB_ATTR_VSCROLL_WIN(__fpriv, __pmask, __pwin) \
do {	unsigned int rop = (FFB_ROP_OLD | (FFB_ROP_OLD << 8)); \
	unsigned int fbc = FFB_FBC_WIN(__pwin); \
	if((__fpriv)->has_double_buffer) { \
		fbc &= ~FFB_FBC_WB_MASK; \
		fbc |= FFB_FBC_WB_AB; \
	} \
	if((__fpriv)->fbc_cache != fbc || \
	   (__fpriv)->rop_cache != rop || \
	   (__fpriv)->pmask_cache != (__pmask) || \
	   (__fpriv)->drawop_cache != FFB_DRAWOP_VSCROLL) { \
		ffb_fbcPtr __ffb = (__fpriv)->regs; \
		(__fpriv)->fbc_cache = fbc; \
		(__fpriv)->rop_cache = rop; \
		(__fpriv)->pmask_cache = (__pmask); \
		(__fpriv)->drawop_cache = FFB_DRAWOP_VSCROLL; \
		(__fpriv)->rp_active = 1; \
		FFBFifo(__fpriv, 4); \
		(__ffb)->fbc = fbc; \
		(__ffb)->rop = rop; \
		(__ffb)->pmask = (__pmask); \
		(__ffb)->drawop = FFB_DRAWOP_VSCROLL; \
	} \
} while(0)

#define FFB_FBC_WIN(pWin)	CreatorGetWindowPrivate(pWin)->fbc_base
#define FFB_WID_WIN(pWin)	CreatorGetWindowPrivate(pWin)->wid

#endif /* FFBRCACHE_H */
