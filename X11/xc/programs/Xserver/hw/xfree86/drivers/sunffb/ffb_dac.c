/*
 * Acceleration for the Creator and Creator3D framebuffer - DAC programming.
 *
 * Copyright (C) 2000 David S. Miller (davem@redhat.com)
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
 * DAVID MILLER BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER
 * IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
 * CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 *
 */
/* $XFree86: xc/programs/Xserver/hw/xfree86/drivers/sunffb/ffb_dac.c,v 1.4 2002/12/06 02:44:03 tsi Exp $ */

#include "ffb.h"
#include "ffb_rcache.h"
#include "ffb_fifo.h"

#include "xf86.h"
#include "xf86_OSproc.h"
#include "xf86_ansic.h"

#include "xf86DDC.h"

/*
 * Used for stabilize time after playing with power management on the display
 */

#ifndef DPMS_SPIN_COUNT
#define DPMS_SPIN_COUNT 100
#endif  /* DPMS_SPIN_COUNT */

/* Cursor programming */

void
FFBDacLoadCursorPos(FFBPtr pFfb, int x, int y)
{
	ffb_dacPtr dac = pFfb->dac;
	int posval;

	posval = ((y & 0xffff) << 16) | (x & 0xffff);
	posval &= (FFBDAC_CUR_POS_Y_SIGN |
		   FFBDAC_CUR_POS_Y |
		   FFBDAC_CUR_POS_X_SIGN |
		   FFBDAC_CUR_POS_X);

	DACCUR_WRITE(dac, FFBDAC_CUR_POS, posval);
}

void
FFBDacLoadCursorColor(FFBPtr pFfb, int fg, int bg)
{
	ffb_dacPtr dac = pFfb->dac;

	dac->cur = FFBDAC_CUR_COLOR1;
	dac->curdata = bg;
	dac->curdata = fg;
}

void
FFBDacCursorEnableDisable(FFBPtr pFfb, int enable)
{
	ffb_dac_info_t *p = &pFfb->dac_info;
	ffb_dacPtr dac = pFfb->dac;
	int val;

	val = 0;
	if (!enable)
		val = (FFBDAC_CUR_CTRL_P0 | FFBDAC_CUR_CTRL_P1);

	/* PAC1 ramdacs with manufacturing revision less than
	 * '3' invert these control bits, wheee...
	 */
	if (p->flags & FFB_DAC_ICURCTL)
		val ^= (FFBDAC_CUR_CTRL_P0 | FFBDAC_CUR_CTRL_P1);

	DACCUR_WRITE(dac, FFBDAC_CUR_CTRL, val);
}

void
FFBDacCursorLoadBitmap(FFBPtr pFfb, int xshift, int yshift, unsigned int *bitmap)
{
	ffb_dacPtr dac = pFfb->dac;
	int i, j;

	dac->cur = FFBDAC_CUR_BITMAP_P0;
	for (j = 0; j < 2; j++) {
		bitmap += yshift * 2;
		if (!xshift) {
			for (i = yshift * 2; i < 128; i++)
				dac->curdata = *bitmap++;
		} else if (xshift < 32) {
			for (i = yshift; i < 64; i++, bitmap += 2) {
				dac->curdata = (bitmap[0] << xshift) |
					(bitmap[1] >> (32 - xshift));
				dac->curdata = bitmap[1] << xshift;
			}
		} else {
			for (i = yshift; i < 64; i++, bitmap += 2) {
				dac->curdata = bitmap[1] << (xshift - 32);
				dac->curdata = 0;
			}
		}

		for (i = 0; i < yshift * 2; i++)
			dac->curdata = 0;
	}
}

/* Config space programming */

/* XF86 LoadPalette callback. */

void
FFBDacLoadPalette(ScrnInfoPtr pScrn, int ncolors, int *indices, LOCO *colors, VisualPtr pVisual)
{
	FFBPtr pFfb = GET_FFB_FROM_SCRN(pScrn);
	ffb_dac_info_t *p = &pFfb->dac_info;
	ffb_dacPtr dac = pFfb->dac;
	unsigned int *cluts;
	int i, index, palette;

	if ((pVisual->nplanes != 8 && pVisual->class != DirectColor) ||
	    (pVisual->nplanes == 8 && pVisual->class == StaticGray))
		return;

	palette = 0;
	if (p->flags & FFB_DAC_PAC2) {
		if (pVisual->class == PseudoColor)
			palette = 0;
		if (pVisual->class == GrayScale)
			palette = 1;
		if (pVisual->class == DirectColor)
			palette = 2;
	}

	cluts = &p->x_dac_state.clut[256 * palette];
	for (i = 0; i < ncolors; i++) {
		unsigned int regval;

		index = indices[i];
		if (pVisual->class == GrayScale) {
			regval = cluts[index] =
				((colors[index].red << FFBDAC_COLOR_RED_SHFT) |
				 (colors[index].red << FFBDAC_COLOR_GREEN_SHFT) |
				 (colors[index].red << FFBDAC_COLOR_BLUE_SHFT));
		} else {
			regval = cluts[index] =
				((colors[index].red   << FFBDAC_COLOR_RED_SHFT) |
				 (colors[index].green << FFBDAC_COLOR_GREEN_SHFT) |
				 (colors[index].blue  << FFBDAC_COLOR_BLUE_SHFT));
		}

		FFBLOG(("FFBDacLoadPalette: visclass(%d) index(%d) val[%08x]\n",
			pVisual->class, index, regval));

		/* Now update the hardware copy. */
		dac->cfg = FFBDAC_CFG_CLUP(palette) + index;
		dac->cfgdata = regval;
	}
}

/* WARNING: Very dangerous function, use with extreme care. */
static void
dac_stop(FFBPtr pFfb)
{
	ffb_dacPtr dac = pFfb->dac;
	unsigned int tgctrl;

	tgctrl = DACCFG_READ(dac, FFBDAC_CFG_TGEN);
	if (tgctrl & FFBDAC_CFG_TGEN_TGE) {
		long limit = 1000000;

		/* We try to shut off the timing generation
		 * precisely at the beginning of a vertical
		 * retrace.  This is really just to make it
		 * look nice, it's not a functional necessity.
		 *
		 * The limit is so that malfunctioning hardware
		 * does not end up hanging the server.
		 */
		while (limit--) {
			unsigned int vctr = DACCFG_READ(dac, FFBDAC_CFG_TGVC);

			if (vctr == 0)
				break;
		}		

		DACCFG_WRITE(dac, FFBDAC_CFG_TGEN, 0);
	}
}

/* This is made slightly complex because the ordering matters
 * between several operations.  We have to stop the DAC while
 * restoring the timing registers so that some intermediate
 * state does not emit wild retrace signals to the monitor.
 *
 * Another further complication is that we need to mess with
 * some portions of the FFB framebuffer config registers to
 * do this all properly.
 */
static void
dac_state_restore(FFBPtr pFfb, ffb_dac_hwstate_t *state)
{
	ffb_dac_info_t *p = &pFfb->dac_info;
	ffb_dacPtr dac = pFfb->dac;
	ffb_fbcPtr ffb = pFfb->regs;
	int i, nluts;

	/* Step 1: Shut off all pixel timing generation. */
	dac_stop(pFfb);
	ffb->fbcfg0 = 0;

	/* Step 2: Restore timing settings. */
	DACCFG_WRITE(dac, FFBDAC_CFG_VBNP, state->vbnp);
	DACCFG_WRITE(dac, FFBDAC_CFG_VBAP, state->vbap);
	DACCFG_WRITE(dac, FFBDAC_CFG_VSNP, state->vsnp);
	DACCFG_WRITE(dac, FFBDAC_CFG_VSAP, state->vsap);
	DACCFG_WRITE(dac, FFBDAC_CFG_HSNP, state->hsnp);
	DACCFG_WRITE(dac, FFBDAC_CFG_HBNP, state->hbnp);
	DACCFG_WRITE(dac, FFBDAC_CFG_HBAP, state->hbap);
	DACCFG_WRITE(dac, FFBDAC_CFG_HSYNCNP, state->hsyncnp);
	DACCFG_WRITE(dac, FFBDAC_CFG_HSYNCAP, state->hsyncap);
	DACCFG_WRITE(dac, FFBDAC_CFG_HSCENNP, state->hscennp);
	DACCFG_WRITE(dac, FFBDAC_CFG_HSCENAP, state->hscenap);
	DACCFG_WRITE(dac, FFBDAC_CFG_EPNP, state->epnp);
	DACCFG_WRITE(dac, FFBDAC_CFG_EINP, state->einp);
	DACCFG_WRITE(dac, FFBDAC_CFG_EIAP, state->eiap);

	/* Step 3: Restore rest of DAC hw state. */
	DACCFG_WRITE(dac, FFBDAC_CFG_PPLLCTRL, state->ppllctrl);
	DACCFG_WRITE(dac, FFBDAC_CFG_GPLLCTRL, state->gpllctrl);
	DACCFG_WRITE(dac, FFBDAC_CFG_PFCTRL, state->pfctrl);
	DACCFG_WRITE(dac, FFBDAC_CFG_UCTRL, state->uctrl);

	nluts = (p->flags & FFB_DAC_PAC1) ? 256 : (4 * 256);
	dac->cfg = FFBDAC_CFG_CLUP_BASE;
	for (i = 0; i < nluts; i++)
		dac->cfgdata = state->clut[i];

	if (p->flags & FFB_DAC_PAC2) {
		dac->cfg = FFBDAC_PAC2_AOVWLUT0;
		for (i = 0; i < 4; i++)
			dac->cfgdata = state->ovluts[i];
	}

	DACCFG_WRITE(dac, FFBDAC_CFG_WTCTRL, state->wtctrl);
	DACCFG_WRITE(dac, FFBDAC_CFG_TMCTRL, state->tmctrl);
	DACCFG_WRITE(dac, FFBDAC_CFG_TCOLORKEY, state->tcolorkey);
	if (p->flags & FFB_DAC_PAC2)
		DACCFG_WRITE(dac, FFBDAC_CFG_WAMASK, state->wamask);

	if (p->flags & FFB_DAC_PAC1) {
		dac->cfg = FFBDAC_PAC1_APWLUT_BASE;
		for (i = 0; i < 32; i++)
			dac->cfgdata = state->pwluts[i];
	} else {
		dac->cfg = FFBDAC_PAC2_APWLUT_BASE;
		for (i = 0; i < 64; i++)
			dac->cfgdata = state->pwluts[i];
	}

	DACCFG_WRITE(dac, FFBDAC_CFG_DACCTRL, state->dacctrl);

	/* Step 4: Restore FFB framebuffer config state. */
	if (pFfb->ffb_type == ffb2_vertical_plus ||
	    pFfb->ffb_type == ffb2_horizontal_plus ||
	    pFfb->ffb_type == afb_m3 ||
	    pFfb->ffb_type == afb_m6)
		ffb->passin = p->ffb_passin_ctrl;
	ffb->fbcfg0 = p->ffbcfg0;
	ffb->fbcfg2 = p->ffbcfg2;

	/* Step 5: Restore the timing generator control reg. */
	DACCFG_WRITE(dac, FFBDAC_CFG_TGEN, state->tgen);

	/* Step 6: Pause for a bit. */
	for (i = 0; i < 100; i++)
		(void) DACCFG_READ(dac, FFBDAC_CFG_TGVC);
}

static void
dac_state_save(FFBPtr pFfb, ffb_dac_hwstate_t *state)
{
	ffb_dac_info_t *p = &pFfb->dac_info;
	ffb_dacPtr dac = pFfb->dac;
	int i, nluts;

	state->ppllctrl = DACCFG_READ(dac, FFBDAC_CFG_PPLLCTRL);
	state->gpllctrl = DACCFG_READ(dac, FFBDAC_CFG_GPLLCTRL);
	state->pfctrl   = DACCFG_READ(dac, FFBDAC_CFG_PFCTRL);
	state->uctrl    = DACCFG_READ(dac, FFBDAC_CFG_UCTRL);

	nluts = (p->flags & FFB_DAC_PAC1) ? 256 : (4 * 256);
	dac->cfg = FFBDAC_CFG_CLUP_BASE;
	for (i = 0; i < nluts; i++)
		state->clut[i] = dac->cfgdata;

	if (p->flags & FFB_DAC_PAC2) {
		dac->cfg = FFBDAC_PAC2_AOVWLUT0;
		for (i = 0; i < 4; i++)
			state->ovluts[i] = dac->cfgdata;
	}

	state->wtctrl    = DACCFG_READ(dac, FFBDAC_CFG_WTCTRL);
	state->tmctrl    = DACCFG_READ(dac, FFBDAC_CFG_TMCTRL);
	state->tcolorkey = DACCFG_READ(dac, FFBDAC_CFG_TCOLORKEY);
	if (p->flags & FFB_DAC_PAC2)
		state->wamask = DACCFG_READ(dac, FFBDAC_CFG_WAMASK);

	if (p->flags & FFB_DAC_PAC1) {
		dac->cfg = FFBDAC_PAC1_APWLUT_BASE;
		for (i = 0; i < 32; i++)
			state->pwluts[i] = dac->cfgdata;
	} else {
		dac->cfg = FFBDAC_PAC2_APWLUT_BASE;
		for (i = 0; i < 64; i++)
			state->pwluts[i] = dac->cfgdata;
	}

	state->dacctrl = DACCFG_READ(dac, FFBDAC_CFG_DACCTRL);

	state->tgen = DACCFG_READ(dac, FFBDAC_CFG_TGEN);
	state->vbnp = DACCFG_READ(dac, FFBDAC_CFG_VBNP);
	state->vbap = DACCFG_READ(dac, FFBDAC_CFG_VBAP);
	state->vsnp = DACCFG_READ(dac, FFBDAC_CFG_VSNP);
	state->vsap = DACCFG_READ(dac, FFBDAC_CFG_VSAP);
	state->hsnp = DACCFG_READ(dac, FFBDAC_CFG_HSNP);
	state->hbnp = DACCFG_READ(dac, FFBDAC_CFG_HBNP);
	state->hbap = DACCFG_READ(dac, FFBDAC_CFG_HBAP);
	state->hsyncnp = DACCFG_READ(dac, FFBDAC_CFG_HSYNCNP);
	state->hsyncap = DACCFG_READ(dac, FFBDAC_CFG_HSYNCAP);
	state->hscennp = DACCFG_READ(dac, FFBDAC_CFG_HSCENNP);
	state->hscenap = DACCFG_READ(dac, FFBDAC_CFG_HSCENAP);
	state->epnp = DACCFG_READ(dac, FFBDAC_CFG_EPNP);
	state->einp = DACCFG_READ(dac, FFBDAC_CFG_EINP);
	state->eiap = DACCFG_READ(dac, FFBDAC_CFG_EIAP);
}

static void
init_dac_flags(FFBPtr pFfb)
{
	ffb_dac_info_t *p = &pFfb->dac_info;
	ffb_dacPtr dac = pFfb->dac;
	unsigned int did, manuf_rev, partnum;
	char *device;

	/* Fetch kernel WID. */
	p->kernel_wid = *((volatile unsigned char *)pFfb->dfb8x);

	/* For AFB, assume it is PAC2 which also implies not having
	 * the inverted cursor control attribute.
	 */
	if (pFfb->ffb_type == afb_m3 || pFfb->ffb_type == afb_m6) {
		p->flags = FFB_DAC_PAC2;
		manuf_rev = 4;
	} else {
		p->flags = 0;

		did = DACCFG_READ(dac, FFBDAC_CFG_DID);

		manuf_rev = DACCFG_READ(dac, FFBDAC_CFG_UCTRL);
		manuf_rev = (manuf_rev & FFBDAC_UCTRL_MANREV) >> 8;

		partnum = ((did & FFBDAC_CFG_DID_PNUM) >> 12);
		if (partnum == 0x236e)
			p->flags |= FFB_DAC_PAC2;
		else
			p->flags |= FFB_DAC_PAC1;
	}

	device = pFfb->psdp->device;
	if ((p->flags & FFB_DAC_PAC1) != 0) {
		if (manuf_rev < 3) {
			p->flags |= FFB_DAC_ICURCTL;
			xf86Msg(X_INFO, "%s: BT9068 (PAC1) ramdac detected (with "
				"inverted cursor control)\n", device);
		} else {
			xf86Msg(X_INFO, "%s: BT9068 (PAC1) ramdac detected (with "
				"normal cursor control)\n", device);
		}
	} else {
		xf86Msg(X_INFO, "%s: BT498 (PAC2) ramdac detected\n", device);
	}
}

/* The registers of the chip must be mapped, and the FFB/AFB
 * board type must be probed before this is invoked.
 */
Bool
FFBDacInit(FFBPtr pFfb)
{
	ffb_dac_info_t *p = &pFfb->dac_info;
	ffb_fbcPtr ffb = pFfb->regs;

	init_dac_flags(pFfb);

	p->ffbcfg0 = ffb->fbcfg0;
	p->ffbcfg2 = ffb->fbcfg2;
	if (pFfb->ffb_type == ffb2_vertical_plus ||
	    pFfb->ffb_type == ffb2_horizontal_plus ||
	    pFfb->ffb_type == afb_m3 ||
	    pFfb->ffb_type == afb_m6)
		p->ffb_passin_ctrl = ffb->passin;

	/* Save the kernel DAC state.  We also save to the
	 * X server state here as well even though we have
	 * not modified anything yet.
	 */
	dac_state_save(pFfb, &p->kern_dac_state);
	dac_state_save(pFfb, &p->x_dac_state);

	/* Fire up the WID layer. */
	FFBWidPoolInit(pFfb);

	return TRUE;
}

/* We need to reset the A buffer X planes to the value 0xff
 * when giving the hardware back to the kernel too, thus...
 * Also need to do this for the B buffer X planes when double
 * buffering is available.
 */
static void
restore_kernel_xchannel(FFBPtr pFfb)
{
	ffb_fbcPtr ffb = pFfb->regs;
	unsigned int fbc, ppc, ppc_mask, drawop, wid;

	wid = pFfb->dac_info.kernel_wid;

	if (pFfb->has_double_buffer)
		fbc = FFB_FBC_WB_AB;
	else
		fbc = FFB_FBC_WB_A;

	fbc |= (FFB_FBC_WM_COMBINED | FFB_FBC_RB_A | FFB_FBC_SB_BOTH |
		FFB_FBC_ZE_OFF | FFB_FBC_YE_OFF |
		FFB_FBC_XE_ON | FFB_FBC_RGBE_MASK);

	ppc      = (FFB_PPC_APE_DISABLE | FFB_PPC_CS_CONST | FFB_PPC_XS_WID);
	ppc_mask = (FFB_PPC_APE_MASK | FFB_PPC_CS_MASK | FFB_PPC_XS_MASK);

	drawop = FFB_DRAWOP_RECTANGLE;

	FFB_ATTR_RAW(pFfb, ppc, ppc_mask, ~0,
		     (FFB_ROP_EDIT_BIT | GXcopy)|(FFB_ROP_NEW<<8),
		     drawop, 0x0, fbc, wid);

	FFBFifo(pFfb, 4);
	FFB_WRITE64(&ffb->by, 0, 0);
	FFB_WRITE64_2(&ffb->bh, pFfb->psdp->height, pFfb->psdp->width);
	pFfb->rp_active = 1;
	FFBWait(pFfb, ffb);
}

void
FFBDacFini(FFBPtr pFfb)
{
	ffb_dac_info_t *p = &pFfb->dac_info;

	/* Just restore the kernel ramdac/x-channel state. */
	dac_state_restore(pFfb, &p->kern_dac_state);
	restore_kernel_xchannel(pFfb);
}


/* Restore X server DAC state. */
void
FFBDacEnterVT(FFBPtr pFfb)
{
	ffb_dac_info_t *p = &pFfb->dac_info;

	/* Save kernel DAC state. */
	dac_state_save(pFfb, &p->kern_dac_state);

	/* Restore X DAC state. */
	dac_state_restore(pFfb, &p->x_dac_state);
}

/* Restore kernel DAC state. */
void
FFBDacLeaveVT(FFBPtr pFfb)
{
	ffb_dac_info_t *p = &pFfb->dac_info;

	/* Save X DAC state. */
	dac_state_save(pFfb, &p->x_dac_state);

	/* Restore kernel DAC and x-channel state. */
	dac_state_restore(pFfb, &p->kern_dac_state);
	restore_kernel_xchannel(pFfb);
}

/*  DPMS stuff, courtesy of a hint from David S. Miller.
 *  05.xii.01, FEM
 */

/*
 * I don't know why, if at all, this is needed, but JJ or DSM do it
 * on restore. I observe that when just blanking/unblanking, everything
 * works fine without it, but that sometimes DPMS -> Standby actually
 * results in Off.  Maybe related?
 */
static void
SPIN(ffb_dacPtr d, int count) {
  while(count-- > 0) {
    (void) DACCFG_READ(d, FFBDAC_CFG_TGVC);
  }
  return;
}

/*  Screen save (blank) restore */
Bool
FFBDacSaveScreen(FFBPtr pFfb, int mode) {
  int tmp;
  ffb_dacPtr dac;
  if(!pFfb) return FALSE;   /* Is there any way at all this could happen? */
  else dac = pFfb -> dac;

  tmp = DACCFG_READ(dac, FFBDAC_CFG_TGEN);  /* Get the timing information */

  switch(mode) {
    case SCREEN_SAVER_ON:
    case SCREEN_SAVER_CYCLE:
      tmp &= ~FFBDAC_CFG_TGEN_VIDE;  /* Kill the video */
      break;

    case SCREEN_SAVER_OFF:
    case SCREEN_SAVER_FORCER:
      tmp |= FFBDAC_CFG_TGEN_VIDE;  /* Turn the video on */
      break;

    default:
      return FALSE;  /* Don't know what to do; gently fail. */
  }
  DACCFG_WRITE(dac, FFBDAC_CFG_TGEN, tmp);  /* Restore timing register, video set as asked */
  SPIN(dac, DPMS_SPIN_COUNT/10);
  return TRUE;
}

/*  DPMS Control, also hinted at by David Miller.

    The rule seems to be:
    
    StandBy  =  -HSYNC +VSYNC -VIDEO
    Suspend  =  +HSYNC -VSYNC -VIDEO
    Off      =  -HSYNC -VSYNC -VIDEO
    On       =  +HSYNC +VSINC +VIDEO

    If you don't force video off, someone periodically tries to turn the
    monitor on for some reason.  I don't know who or why, so I kill the video
    when trying to go into some sort of energy saving mode.  (In real life,
    'xset s blank s xx' could well have taken care of this.)

    Also, on MY monitor, StandBy as above defined (-H+V-Vid) in fact
    gives the same as Off, which I don't want.  Hence, I just do (-Vid)

    05.xii.01, FEM
    08.xii.01, FEM
*/
void
FFBDacDPMSMode(FFBPtr pFfb, int DPMSMode, int flags) {
  int tmp;
  ffb_dacPtr dac = pFfb -> dac;

  tmp = DACCFG_READ(dac, FFBDAC_CFG_TGEN);  /* Get timing control */

  switch(DPMSMode) {

    case DPMSModeOn:
      tmp &= ~(FFBDAC_CFG_TGEN_VSD | FFBDAC_CFG_TGEN_HSD); /* Turn off VSYNC, HSYNC
							      disable bits */
      tmp |= FFBDAC_CFG_TGEN_VIDE;  /* Turn the video on */
       break;

    case DPMSModeStandby:
#ifdef  DPMS_TRUE_STANDBY
      tmp |=  FFBDAC_CFG_TGEN_HSD;  /* HSYNC = OFF    */
#endif  /* DPMS_TRUE_STANDBY */
      tmp &= ~FFBDAC_CFG_TGEN_VSD;  /* VSYNC = ON     */
      tmp &= ~FFBDAC_CFG_TGEN_VIDE; /* Kill the video */
      break;

    case DPMSModeSuspend:
      tmp |=  FFBDAC_CFG_TGEN_VSD;  /* VSYNC = OFF    */
      tmp &= ~FFBDAC_CFG_TGEN_HSD;  /* HSYNC = ON     */
      tmp &= ~FFBDAC_CFG_TGEN_VIDE; /* Kill the video */
      break;

    case DPMSModeOff:
      tmp |= (FFBDAC_CFG_TGEN_VSD | FFBDAC_CFG_TGEN_HSD);  /* Kill HSYNC, VSYNC both */
      tmp &= ~FFBDAC_CFG_TGEN_VIDE;                        /* Kill the video         */
      break;
      
    default:
      return;     /* If we get here, we really should log an error */
  }
  DACCFG_WRITE(dac, FFBDAC_CFG_TGEN,tmp);  /* Restore timing register, video set as asked */
  SPIN(dac, DPMS_SPIN_COUNT);  /* Is this necessary?  Why?  */
}
