/*
 * Acceleration for the Creator and Creator3D framebuffer - DDC support.
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
/* $XFree86: xc/programs/Xserver/hw/xfree86/drivers/sunffb/ffb_ddc.c,v 1.1 2000/05/23 04:47:44 dawes Exp $ */

#include "ffb.h"

#include "ffb_dac.h"

#include "xf86.h"
#include "xf86_OSproc.h"
#include "xf86_ansic.h"

#include "xf86DDC.h"

/* XXX This needs a lot more work.  Only an attempt at the PAC2 version
 * XXX is below, and that is untested.  The BT498 manual is unclear about
 * XXX several details and I must figure them out by trial and error.
 */

/* Wait for the next VSYNC. */
static void
WaitForVSYNC(ffb_dacPtr dac)
{
	unsigned int vsap = DACCFG_READ(dac, FFBDAC_CFG_VSAP);
	unsigned int vcnt;

	vcnt = DACCFG_READ(dac, FFBDAC_CFG_TGVC);
	while (vcnt > vsap)
		vcnt = DACCFG_READ(dac, FFBDAC_CFG_TGVC);
	while (vcnt <= vsap)
		vcnt = DACCFG_READ(dac, FFBDAC_CFG_TGVC);

}

/* The manual seems to imply this is needed, but it's really clumsy
 * so we can test if it really is a requirement with this.
 */
#define MDATA_NEEDS_BLANK

/* DDC1/DDC2 support */
static unsigned int
FFBDacDdc1Read(ScrnInfoPtr pScrn)
{
	FFBPtr pFfb = GET_FFB_FROM_SCRN(pScrn);
	ffb_dacPtr dac = pFfb->dac;
	unsigned int val;
#ifdef MDATA_NEEDS_BLANK
	unsigned int uctrl;
#endif

#ifdef MDATA_NEEDS_BLANK
	/* Force a blank of the screen. */
	uctrl = DACCFG_READ(dac, FFBDAC_CFG_UCTRL);
	DACCFG_WRITE(dac, FFBDAC_CFG_UCTRL,
			  (uctrl | FFBDAC_UCTRL_ABLANK));
#endif

	/* Tristate SCL pin. */
	DACCFG_WRITE(dac, FFBDAC_CFG_MPDATA,
			  FFBDAC_CFG_MPDATA_SCL);

	/* Pause until VSYNC is hit. */
	WaitForVSYNC(dac);

	/* Read the sense line to see what the monitor is driving
	 * it at.
	 */
	val = DACCFG_READ(dac, FFBDAC_CFG_MPSENSE);
	val = (val & FFBDAC_CFG_MPSENSE_SCL) ? 1 : 0;

	/* Stop tristating the SCL pin. */
	DACCFG_WRITE(dac, FFBDAC_CFG_MPDATA, 0);

#ifdef MDATA_NEEDS_BLANK
	/* Restore UCTRL to unblank the screen. */
	DACCFG_WRITE(dac, FFBDAC_CFG_UCTRL, uctrl);
#endif

	/* Return the result and we're done. */
	return val;
}

static void
FFBI2CGetBits(I2CBusPtr b, int *clock, int *data)
{
	FFBPtr pFfb = GET_FFB_FROM_SCRN(xf86Screens[b->scrnIndex]);
	ffb_dacPtr dac = pFfb->dac;
	unsigned int val;
#ifdef MDATA_NEEDS_BLANK
	unsigned int uctrl;
#endif

#ifdef MDATA_NEEDS_BLANK
	/* Force a blank of the screen. */
	uctrl = DACCFG_READ(dac, FFBDAC_CFG_UCTRL);
	DACCFG_WRITE(dac, FFBDAC_CFG_UCTRL,
			  (uctrl | FFBDAC_UCTRL_ABLANK));
#endif

	/* Tristate SCL+SDA pins. */
	DACCFG_WRITE(dac, FFBDAC_CFG_MPDATA,
		     (FFBDAC_CFG_MPDATA_SCL | FFBDAC_CFG_MPDATA_SDA));

	/* Read the sense line to see what the monitor is driving
	 * them at.
	 */
	val = DACCFG_READ(dac, FFBDAC_CFG_MPSENSE);
	*clock = (val & FFBDAC_CFG_MPSENSE_SCL) ? 1 : 0;
	*data  = (val & FFBDAC_CFG_MPSENSE_SDA) ? 1 : 0;

	/* Stop tristating the SCL pin. */
	DACCFG_WRITE(dac, FFBDAC_CFG_MPDATA, 0);

#ifdef MDATA_NEEDS_BLANK
	/* Restore UCTRL to unblank the screen. */
	DACCFG_WRITE(dac, FFBDAC_CFG_UCTRL, uctrl);
#endif
}

static void
FFBI2CPutBits(I2CBusPtr b, int clock, int data)
{
	FFBPtr pFfb = GET_FFB_FROM_SCRN(xf86Screens[b->scrnIndex]);
	ffb_dacPtr dac = pFfb->dac;
	unsigned int val;
#ifdef MDATA_NEEDS_BLANK
	unsigned int uctrl;
#endif

	val = 0;
	if (clock)
		val |= FFBDAC_CFG_MPDATA_SCL;
	if (data)
		val |= FFBDAC_CFG_MPDATA_SDA;

#ifdef MDATA_NEEDS_BLANK
	/* Force a blank of the screen. */
	uctrl = DACCFG_READ(dac, FFBDAC_CFG_UCTRL);
	DACCFG_WRITE(dac, FFBDAC_CFG_UCTRL,
			  (uctrl | FFBDAC_UCTRL_ABLANK));
#endif

	/* Tristate requested pins. */
	DACCFG_WRITE(dac, FFBDAC_CFG_MPDATA, val);

#ifdef MDATA_NEEDS_BLANK
	/* Restore UCTRL to unblank the screen. */
	DACCFG_WRITE(dac, FFBDAC_CFG_UCTRL, uctrl);
#endif
}

Bool
FFBi2cInit(ScrnInfoPtr pScrn)
{
	FFBPtr pFfb = GET_FFB_FROM_SCRN(pScrn);
	I2CBusPtr I2CPtr;

	I2CPtr = xf86CreateI2CBusRec();
	if (!I2CPtr)
		return FALSE;

	pFfb->I2C = I2CPtr;

	I2CPtr->BusName		= "DDC";
	I2CPtr->scrnIndex	= pScrn->scrnIndex;
	I2CPtr->I2CPutBits	= FFBI2CPutBits;
	I2CPtr->I2CGetBits	= FFBI2CGetBits;
	I2CPtr->AcknTimeout	= 5;

	if (!xf86I2CBusInit(I2CPtr))
		return FALSE;

	return TRUE;
}
