/*
 * Acceleration for the Creator and Creator3D framebuffer - WID pool management.
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
/* $XFree86: xc/programs/Xserver/hw/xfree86/drivers/sunffb/ffb_wid.c,v 1.3 2000/12/01 00:24:34 dawes Exp $ */

#include "ffb.h"

static void
determine_numwids(FFBPtr pFfb)
{
	ffb_dac_info_t *p = &pFfb->dac_info;

	if (p->flags & FFB_DAC_PAC1)
		p->wid_table.num_wids = 32;
	else
		p->wid_table.num_wids = 64;
}

static void
make_wlut_regval(ffb_dac_info_t *p, ffb_wid_info_t *wid)
{
	wid->wlut_regval = 0;

	if (p->flags & FFB_DAC_PAC1) {
		unsigned int color_model_bits;

		/* Pacifica1 format */
		if (wid->buffer != 0)
			wid->wlut_regval |= FFBDAC_PAC1_WLUT_DB;

		if (wid->depth == 8) {
			if (wid->greyscale) {
				if (wid->linear)
					color_model_bits = FFBDAC_PAC1_WLUT_C_8LG;
				else
					color_model_bits = FFBDAC_PAC1_WLUT_C_8NG;
			} else {
				color_model_bits = FFBDAC_PAC1_WLUT_C_8P;
			}
		} else {
			if (wid->direct) {
				color_model_bits = FFBDAC_PAC1_WLUT_C_24D;
			} else {
				if (wid->linear)
					color_model_bits = FFBDAC_PAC1_WLUT_C_24LT;
				else
					color_model_bits = FFBDAC_PAC1_WLUT_C_24NT;
			}
		}

		wid->wlut_regval |= color_model_bits;

		switch (wid->channel) {
		default:
		case 0:
			wid->wlut_regval |= FFBDAC_PAC1_WLUT_P_XO;
			break;
		case 1:
			wid->wlut_regval |= FFBDAC_PAC1_WLUT_P_R;
			break;
		case 2:
			wid->wlut_regval |= FFBDAC_PAC1_WLUT_P_G;
			break;
		case 3:
			wid->wlut_regval |= FFBDAC_PAC1_WLUT_P_B;
			break;
		};
	} else {
		/* Pacifica2 format */
		if (wid->buffer != 0)
			wid->wlut_regval |= FFBDAC_PAC2_WLUT_DB;

		if (wid->depth == 24)
			wid->wlut_regval |= FFBDAC_PAC2_WLUT_DEPTH;

		switch (wid->channel) {
		default:
		case 0:
			wid->wlut_regval |= FFBDAC_PAC2_WLUT_P_XO;
			break;
		case 1:
			wid->wlut_regval |= FFBDAC_PAC2_WLUT_P_R;
			break;
		case 2:
			wid->wlut_regval |= FFBDAC_PAC2_WLUT_P_G;
			break;
		case 3:
			wid->wlut_regval |= FFBDAC_PAC2_WLUT_P_B;
			break;
		};

		if ((wid->depth == 8 && wid->greyscale == 0) ||
		    (wid->depth == 24 && wid->direct != 0))
			wid->wlut_regval |= FFBDAC_PAC2_WLUT_LKUP;

		if (wid->palette != -1)
			wid->wlut_regval |=
				((wid->palette << 4) & FFBDAC_PAC2_WLUT_PTBL);
	}
}

static void
init_wid_table(FFBPtr pFfb)
{
	ffb_dac_info_t *p = &pFfb->dac_info;
	ffb_wid_pool_t *table = &p->wid_table;
	int i;

	for (i = 0; i < table->num_wids; i++) {
		table->wid_pool[i].InUse = FALSE;
		table->wid_pool[i].buffer = 0;
		table->wid_pool[i].depth = 24;
		table->wid_pool[i].greyscale = 0;
		table->wid_pool[i].linear = 0;
		table->wid_pool[i].direct = 0;
		table->wid_pool[i].channel = 0;
		table->wid_pool[i].palette = -1;
		make_wlut_regval(p, &table->wid_pool[i]);
	}

	table->wid_pool[table->num_wids - 1].InUse = TRUE;
	table->wid_pool[table->num_wids - 1].canshare = FALSE;
}

static void
init_hw_wids(FFBPtr pFfb)
{
	ffb_dac_info_t *p = &pFfb->dac_info;
	ffb_dacPtr dac = pFfb->dac;
	ffb_wid_pool_t *table = &p->wid_table;
	int i;

	if (p->flags & FFB_DAC_PAC1)
		dac->cfg = FFBDAC_PAC1_APWLUT_BASE;
	else
		dac->cfg = FFBDAC_PAC2_APWLUT_BASE;
	for (i = 0; i < table->num_wids; i++)
		dac->cfgdata = table->wid_pool[i].wlut_regval;

	if (p->flags & FFB_DAC_PAC1)
		dac->cfg = FFBDAC_PAC1_SPWLUT_BASE;
	else
		dac->cfg = FFBDAC_PAC2_SPWLUT_BASE;
	for (i = 0; i < table->num_wids; i++)
		dac->cfgdata = table->wid_pool[i].wlut_regval;
}

static void
init_hw_widmode(FFBPtr pFfb)
{
	ffb_dacPtr dac = pFfb->dac;
	ffb_dac_info_t *p = &pFfb->dac_info;
	unsigned int uctrl;

	/* For now we use the Combined WID mode until I figure
	 * out exactly how Seperate4 and Seperate8 work.  We
	 * also disable overlays for the time being.
	 */
	p->wid_table.wid_shift = 0;

	dac->cfg = FFBDAC_CFG_UCTRL;
	uctrl = dac->cfgdata;
	uctrl &= ~FFBDAC_UCTRL_WMODE;
	uctrl |= FFBDAC_UCTRL_WM_COMB;
	uctrl &= ~FFBDAC_UCTRL_OVENAB;
	dac->cfg = FFBDAC_CFG_UCTRL;
	dac->cfgdata = uctrl;
}

void
FFBWidPoolInit(FFBPtr pFfb)
{
	determine_numwids(pFfb);
	init_wid_table(pFfb);
	init_hw_wids(pFfb);
	init_hw_widmode(pFfb);
}

static void
update_wids(FFBPtr pFfb, int index)
{
	ffb_dac_info_t *p = &pFfb->dac_info;
	ffb_dacPtr dac = pFfb->dac;
	unsigned int base;
	int limit;

	if (pFfb->vtSema)
		return;

	if (p->flags & FFB_DAC_PAC1)
		base = FFBDAC_PAC1_SPWLUT(index);
	else
		base = FFBDAC_PAC2_SPWLUT(index);
	DACCFG_WRITE(dac, base, p->wid_table.wid_pool[index].wlut_regval);

	/* Schedule the window transfer. */
	DACCFG_WRITE(dac, FFBDAC_CFG_WTCTRL, 
		     (FFBDAC_CFG_WTCTRL_TCMD | FFBDAC_CFG_WTCTRL_TE));

	limit = 1000000;
	while (limit--) {
		unsigned int wtctrl = DACCFG_READ(dac, FFBDAC_CFG_WTCTRL);

		if ((wtctrl & FFBDAC_CFG_WTCTRL_DS) == 0)
			break;
	}
}

unsigned int
FFBWidAlloc(FFBPtr pFfb, int visclass, int cmap, Bool canshare)
{
	ffb_dac_info_t *p = &pFfb->dac_info;
	ffb_wid_pool_t *table = &p->wid_table;
	int i, depth, direct, static_greyscale, palette, channel;

	direct = 0;
	static_greyscale = 0;
	switch (visclass) {
	case StaticGray:
		static_greyscale = 1;
		/* Fallthrough... */
	case StaticColor:
	case GrayScale:
	case PseudoColor:
		depth = 8;
		channel = 1;
		break;

	case DirectColor:
		direct = 1;
		/* Fallthrough... */
	case TrueColor:
		depth = 24;
		channel = 0;
		break;

	default:
		return (unsigned int) -1;
	};

	palette = -1;
	if (p->flags & FFB_DAC_PAC1) {
		if (visclass == PseudoColor ||
		    visclass == GrayScale ||
		    visclass == DirectColor)
			palette = 0;
	} else {
		if (visclass == PseudoColor)
			palette = 0;
		if (visclass == GrayScale)
			palette = 1;
		if (visclass == DirectColor)
			palette = 2;
	}

	if (canshare) {
		for (i = 0; i < table->num_wids; i++) {
			if (table->wid_pool[i].InUse == TRUE &&
			    table->wid_pool[i].canshare == TRUE &&
			    table->wid_pool[i].palette == palette &&
			    table->wid_pool[i].direct == direct &&
			    table->wid_pool[i].greyscale == static_greyscale &&
			    table->wid_pool[i].channel == channel &&
			    table->wid_pool[i].depth == depth) {
				table->wid_pool[i].refcount++;
				return i << table->wid_shift;
			}
		}
	}

	for (i = 0; i < table->num_wids; i++) {
		if (table->wid_pool[i].InUse == FALSE)
			break;
	}

	if (i != table->num_wids) {
		table->wid_pool[i].InUse = TRUE;
		table->wid_pool[i].buffer = 0;
		table->wid_pool[i].depth = depth;
		table->wid_pool[i].palette = palette;
		table->wid_pool[i].direct = direct;
		table->wid_pool[i].greyscale = static_greyscale;
		if (depth == 8)
			table->wid_pool[i].channel = 1;
		else
			table->wid_pool[i].channel = 0;
		table->wid_pool[i].refcount = 1;
		table->wid_pool[i].canshare = canshare;
		make_wlut_regval(p, &table->wid_pool[i]);
		update_wids(pFfb, i);
		return i << table->wid_shift;
	}

	return (unsigned int) -1;
}

void
FFBWidFree(FFBPtr pFfb, unsigned int wid)
{
	ffb_dac_info_t *p = &pFfb->dac_info;
	ffb_wid_pool_t *table = &p->wid_table;
	int index = wid >> table->wid_shift;

	if (index < 0 || index >= table->num_wids) {
		return;
	}

	if (--table->wid_pool[index].refcount == 0) {
		table->wid_pool[index].InUse = FALSE;
	}
}

/* Double Buffering support. */

unsigned int
FFBWidUnshare(FFBPtr pFfb, unsigned int wid)
{
	ffb_dac_info_t *p = &pFfb->dac_info;
	ffb_wid_pool_t *table = &p->wid_table;
	int index = wid >> table->wid_shift;
	int i;

	if (index < 0 || index >= table->num_wids) {
		return (unsigned int) -1;
	}

	for (i = 0; i < table->num_wids; i++) {
		if (table->wid_pool[i].InUse == FALSE)
			break;
	}

	if (i == table->num_wids) {
		return (unsigned int) -1;
	}

	table->wid_pool[i].InUse = TRUE;
	table->wid_pool[i].buffer = 0;
	table->wid_pool[i].depth = table->wid_pool[index].depth;
	table->wid_pool[i].palette = table->wid_pool[index].palette;
	table->wid_pool[i].direct = table->wid_pool[index].direct;
	table->wid_pool[i].greyscale = table->wid_pool[index].greyscale;
	table->wid_pool[i].channel = table->wid_pool[index].channel;
	table->wid_pool[i].refcount = 1;
	table->wid_pool[i].canshare = FALSE;
	make_wlut_regval(p, &table->wid_pool[i]);
	update_wids(pFfb, i);

	/* Now free the original WID. */
	if (--table->wid_pool[index].refcount == 0) {
		table->wid_pool[index].InUse = FALSE;
	}

	return i << table->wid_shift;
}

unsigned int
FFBWidReshare(FFBPtr pFfb, unsigned int wid)
{
	ffb_dac_info_t *p = &pFfb->dac_info;
	ffb_wid_pool_t *table = &p->wid_table;
	int index = wid >> table->wid_shift;
	int i;

	if (index < 0 || index >= table->num_wids) {
		return wid;
	}

	for (i = 0; i < table->num_wids; i++) {
		if (table->wid_pool[i].InUse == TRUE &&
		    table->wid_pool[i].canshare == TRUE &&
		    table->wid_pool[i].depth == table->wid_pool[index].depth &&
		    table->wid_pool[i].palette == table->wid_pool[index].palette &&
		    table->wid_pool[i].direct == table->wid_pool[index].direct &&
		    table->wid_pool[i].greyscale == table->wid_pool[index].greyscale &&
		    table->wid_pool[i].channel == table->wid_pool[index].channel)
			break;
	}

	if (i == table->num_wids) {
		/* OK, very simple, just make the old one shared. */
		table->wid_pool[index].canshare = TRUE;
		table->wid_pool[index].buffer = 0;
		make_wlut_regval(p, &table->wid_pool[index]);
		update_wids(pFfb, index);
		return wid;
	}

	/* Ok, free the original WID. */
	if (--table->wid_pool[index].refcount == 0) {
		table->wid_pool[index].InUse = FALSE;
	}

	/* And grab a reference to the new one. */
	table->wid_pool[i].refcount++;

	/* And return the shared one. */
	return i << table->wid_shift;
}

void
FFBWidChangeBuffer(FFBPtr pFfb, unsigned int wid, int visible)
{
	ffb_dac_info_t *p = &pFfb->dac_info;
	ffb_wid_pool_t *table = &p->wid_table;
	int index = wid >> table->wid_shift;
	int buffer;

	if (index < 0 || index >= table->num_wids)
		return;

	buffer = (table->wid_pool[index].buffer ^= 1);
	if (visible) {
		unsigned int bit;

		if (p->flags & FFB_DAC_PAC1)
			bit = FFBDAC_PAC1_WLUT_DB;
		else
			bit = FFBDAC_PAC2_WLUT_DB;

		if (buffer)
			table->wid_pool[index].wlut_regval |= bit;
		else
			table->wid_pool[index].wlut_regval &= ~bit;

		update_wids(pFfb, index);
	}
}

/* Used by DRI part of driver. */
Bool
FFBWidIsShared(FFBPtr pFfb, unsigned int wid)
{
	ffb_dac_info_t *p = &pFfb->dac_info;
	ffb_wid_pool_t *table = &p->wid_table;
	int index = wid >> table->wid_shift;

	if (index < 0 || index >= table->num_wids)
		return TRUE;

	if (table->wid_pool[index].canshare == TRUE)
		return TRUE;

	return FALSE;
}
