/* $Xorg: sunLyUtil.c,v 1.3 2000/08/17 19:48:37 cpqbld Exp $ */
/*
 * CG3 and CG6 utility functions for LynxOS, derived from NetBSD
 * Copyright 1996 by Thomas Mueller
 *
 * Permission to use, copy, modify, distribute, and sell this software and its
 * documentation for any purpose is hereby granted without fee, provided that
 * the above copyright notice appear in all copies and that both that
 * copyright notice and this permission notice appear in supporting
 * documentation, and that the name of Thomas Mueller not be used in
 * advertising or publicity pertaining to distribution of the software without
 * specific, written prior permission.  Thomas Mueller makes no representations
 * about the suitability of this software for any purpose.  It is provided
 * "as is" without express or implied warranty.
 *
 * THOMAS MUELLER DISCLAIMS ALL WARRANTIES WITH REGARD TO THIS SOFTWARE,
 * INCLUDING ALL IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS, IN NO
 * EVENT SHALL THOMAS MUELLER BE LIABLE FOR ANY SPECIAL, INDIRECT OR
 * CONSEQUENTIAL DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM LOSS OF USE,
 * DATA OR PROFITS, WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE OR OTHER
 * TORTIOUS ACTION, ARISING OUT OF OR IN CONNECTION WITH THE USE OR
 * PERFORMANCE OF THIS SOFTWARE.
 *
 */
/* $XFree86: xc/programs/Xserver/hw/sunLynx/sunLyUtil.c,v 3.2 2001/01/17 22:36:53 dawes Exp $ */

/*      $NetBSD: bt_subr.c,v 1.4 1994/11/20 20:51:54 deraadt Exp $ */

/*
 * Copyright (c) 1993
 *      The Regents of the University of California.  All rights reserved.
 *
 * This software was developed by the Computer Systems Engineering group
 * at Lawrence Berkeley Laboratory under DARPA contract BG 91-66 and
 * contributed to Berkeley.
 *
 * All advertising materials mentioning features or use of this software
 * must display the following acknowledgement:
 *      This product includes software developed by the University of
 *      California, Lawrence Berkeley Laboratory.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *      This product includes software developed by the University of
 *      California, Berkeley and its contributors.
 * 4. Neither the name of the University nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 *      @(#)bt_subr.c   8.2 (Berkeley) 1/21/94
 */

/* adaption for LynxOS microSPARC 2.4.0 and X11R6[.1]
 * Copyright 1996 by Thomas Mueller <tm@systrix.de>
 *
 * Permission to use, copy, modify, distribute, and sell this software and its
 * documentation for any purpose is hereby granted without fee, provided that
 * the above copyright notice appear in all copies and that both that
 * copyright notice and this permission notice appear in supporting
 * documentation, and that the name of Thomas Mueller not be used in
 * advertising or publicity pertaining to distribution of the software without
 * specific, written prior permission.  Thomas Mueller makes no representations
 * about the suitability of this software for any purpose.  It is provided
 * "as is" without express or implied warranty.
 *
 * THOMAS MUELLER DISCLAIMS ALL WARRANTIES WITH REGARD TO THIS SOFTWARE,
 * INCLUDING ALL IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS, IN NO
 * EVENT SHALL THOMAS MUELLER BE LIABLE FOR ANY SPECIAL, INDIRECT OR
 * CONSEQUENTIAL DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM LOSS OF USE,
 * DATA OR PROFITS, WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE OR OTHER
 * TORTIOUS ACTION, ARISING OUT OF OR IN CONNECTION WITH THE USE OR
 * PERFORMANCE OF THIS SOFTWARE.
 *
 */

#include "sun.h"
#include "btreg.h"
#include "btvar.h"
#include "cgsixreg.h"
#define useracc(a,b,c)  (1)

/*
 * Common code for dealing with Brooktree video DACs.
 * (Contains some software-only code as well, since the colormap
 * ioctls are shared between the cgthree and cgsix drivers.)
 */

/*
 * Implement an FBIOGETCMAP-like ioctl.
 */
int
bt_getcmap(struct fbcmap *p, union bt_cmap *cm, int cmsize)
{
    u_int i;
    u_int start;
    u_int count;
    u_char *cp;

    start = p->index;
    count = p->count;
    if (start >= cmsize || start + count > cmsize)
	return (EINVAL);
    if (!useracc(p->red, count, B_WRITE) ||
	!useracc(p->green, count, B_WRITE) ||
	!useracc(p->blue, count, B_WRITE))
	return (EFAULT);
    for (cp = &cm->cm_map[start][0], i = 0; i < count; cp += 3, i++) {
	p->red[i] = cp[0];
	p->green[i] = cp[1];
	p->blue[i] = cp[2];
    }
    return (0);
}

/*
 * Implement the software portion of an FBIOPUTCMAP-like ioctl.
 */
int
bt_putcmap(struct fbcmap *p, union bt_cmap *cm, int cmsize)
{
    u_int i;
    int start;
    int count;
    u_char *cp;

    start = p->index;
    count = p->count;
    if (start >= cmsize || start + count > cmsize)
	return (EINVAL);
    if (!useracc(p->red, count, B_READ) ||
	!useracc(p->green, count, B_READ) ||
	!useracc(p->blue, count, B_READ))
	return (EFAULT);
    for (cp = &cm->cm_map[start][0], i = 0; i < count; cp += 3, i++) {
	cp[0] = p->red[i];
	cp[1] = p->green[i];
	cp[2] = p->blue[i];
    }
    return (0);
}

union cursor_cmap {		/* colormap, like bt_cmap, but tiny */
    u_char cm_map[2][3];	/* 2 R/G/B entries */
    u_int cm_chip[2];		/* 2 chip equivalents */
};

struct cg6_cursor {		/* cg6 hardware cursor status */
    short cc_enable;		/* cursor is enabled */
    struct fbcurpos cc_pos;	/* position */
    struct fbcurpos cc_hot;	/* hot-spot */
    struct fbcurpos cc_size;	/* size of mask & image fields */
    u_int cc_bits[2][32];	/* space for mask & image bits */
    union cursor_cmap cc_color;	/* cursor colormap */
};

static union bt_cmap sc_cmap;	/* local copy of LUT    */
static int blanked = 1;		/* to jump-start it ... */
static struct cg6_cursor sc_cursor;	/* cursor info		*/

/*
 * Clean up hardware state (e.g., after bootup or after X crashes).
 */
static void
cg6_reset(fbFd * fb)
{
    volatile struct cg6_tec_xxx *tec;
    int fhc;
    short fhcrev;
    volatile struct bt_regs *bt;

    /* hide the cursor, just in case */
    ((volatile struct cg6_thc *) (fb->thc))->thc_cursxy =
	(THC_CURSOFF << 16) | THC_CURSOFF;

    /* turn off frobs in transform engine (makes X11 work) */
    tec = fb->tec;
    tec->tec_mv = 0;
    tec->tec_clip = 0;
    tec->tec_vdc = 0;

    /* take care of hardware bugs in old revisions */
    fhcrev = (*(int *) fb->fhc >> FHC_REV_SHIFT) &
	(FHC_REV_MASK >> FHC_REV_SHIFT);
    if (fhcrev < 5) {
	/*
		 * Keep current resolution; set cpu to 68020, set test
		 * window (size 1Kx1K), and for rev 1, disable dest cache.
		 */
	fhc = (*(int *) fb->fhc & FHC_RES_MASK) | FHC_CPU_68020 |
	    FHC_TEST |
	    (11 << FHC_TESTX_SHIFT) | (11 << FHC_TESTY_SHIFT);
	if (fhcrev < 2)
	    fhc |= FHC_DST_DISABLE;
	*(int *) fb->fhc = fhc;
    }
    /* Enable cursor in Brooktree DAC. */
    bt = fb->ramdac;
    bt->bt_addr = 0x06 << 24;
    bt->bt_ctrl |= 0x03 << 24;
}

static void
cg6_loadcursor(fbFd * fb)
{
    volatile struct cg6_thc *thc;
    u_int edgemask;
    u_int m;
    int i;

    /*
     * Keep the top size.x bits.  Here we *throw out* the top
     * size.x bits from an all-one-bits word, introducing zeros in
     * the top size.x bits, then invert all the bits to get what
     * we really wanted as our mask.  But this fails if size.x is
     * 32---a sparc uses only the low 5 bits of the shift count---
     * so we have to special case that.
     */
    edgemask = ~0;
    if (sc_cursor.cc_size.x < 32)
	edgemask = ~(edgemask >> sc_cursor.cc_size.x);
    thc = (volatile struct cg6_thc *) fb->thc;
    for (i = 0; i < 32; i++) {
	m = sc_cursor.cc_bits[0][i] & edgemask;
	thc->thc_cursmask[i] = m;
	thc->thc_cursbits[i] = m & sc_cursor.cc_bits[1][i];
    }
}

static void
cg6_setcursor(fbFd * fb)
{
    /* we need to subtract the hot-spot value here */
#define COORD(f) (sc_cursor.cc_pos.f - sc_cursor.cc_hot.f)
    ((volatile struct cg6_thc *) (fb->thc))->thc_cursxy = sc_cursor.cc_enable ?
    ((COORD(x) << 16) | (COORD(y) & 0xffff)) :
    (THC_CURSOFF << 16) | THC_CURSOFF;
#undef COORD
}

/*
 * Load the cursor (overlay `foreground' and `background') colors.
 */
static void
cg6_loadomap(fbFd * fb)
{
    volatile struct bt_regs *bt;
    u_int i;

    bt = (volatile struct bt_regs *) fb->ramdac;
    bt->bt_addr = 0x01 << 24;	/* set background color */
    i = sc_cursor.cc_color.cm_chip[0];
    bt->bt_omap = i;		/* R */
    bt->bt_omap = i << 8;	/* G */
    bt->bt_omap = i << 16;	/* B */

    bt->bt_addr = 0x03 << 24;	/* set foreground color */
    bt->bt_omap = i << 24;	/* R */
    i = sc_cursor.cc_color.cm_chip[1];
    bt->bt_omap = i;		/* G */
    bt->bt_omap = i << 8;	/* B */
}

/*
 * Load a subset of the current (new) colormap into the color DAC.
 */
static void
cg6_loadcmap(fbFd * fb, union bt_cmap *cm, int start, int ncolors)
{
    volatile struct bt_regs *bt;
    u_int *ip;
    u_int i;
    int count;

    ip = &cm->cm_chip[BT_D4M3(start)];	/* start/4 * 3 */
    count = BT_D4M3(start + ncolors - 1) - BT_D4M3(start) + 3;
    bt = (struct bt_regs *) fb->ramdac;
    bt->bt_addr = BT_D4M4(start) << 24;
    while (--count >= 0) {
	i = *ip++;
	/* hardware that makes one want to pound boards with hammers */
	bt->bt_cmap = i;
	bt->bt_cmap = i << 8;
	bt->bt_cmap = i << 16;
	bt->bt_cmap = i << 24;
    }
}

/*
 * Undo the effect of an FBIOSVIDEO that turns the video off on a CG6
 */
static void
cg6_unblank(fbFd * dev)
{
    volatile struct cg6_thc *thc = (volatile struct cg6_thc *) dev->thc;
    unsigned long x;
    unsigned long y;

    if (blanked) {
	thc->thc_misc = (thc->thc_misc & ~THC_MISC_VIDEN) | THC_MISC_VIDEN;
    }
    blanked = 0;
}

/*
 * Load a subset of the current (new) colormap into the Brooktree DAC.
 */
static void
cg3_loadcmap(fbFd * fb, union bt_cmap *cm, int start, int ncolors)
{
    volatile struct bt_regs *bt;
    u_int *ip;
    int count;

    ip = &cm->cm_chip[BT_D4M3(start)];	/* start/4 * 3 */
    count = BT_D4M3(start + ncolors - 1) - BT_D4M3(start) + 3;
    bt = (struct bt_regs *) fb->ramdac;
    bt->bt_addr = BT_D4M4(start);
    while (--count >= 0)
	bt->bt_cmap = *ip++;
}

/*
 * Undo the effect of an FBIOSVIDEO that turns the video off on a CG3
 */
static void
cg3_unblank(fbFd * dev)
{
    volatile struct bt_regs *bt;

    if (blanked) {
	bt = (struct bt_regs *) dev->ramdac;
	/* restore color 0 (and R of color 1) */
	bt->bt_addr = 0;
	bt->bt_cmap = sc_cmap.cm_chip[0];

	/* restore read mask */
	bt->bt_addr = 0x06;	/* command reg */
	bt->bt_ctrl = 0x73;	/* overlay plane */
	bt->bt_addr = 0x04;	/* read mask */
	bt->bt_ctrl = 0xff;	/* color planes */
    }
    blanked = 0;
}

int
sunIoctl(fbFd * fb, int cmd, void *arg)
{
    int error;
    int v;
    int i;
    u_int count;
    union cursor_cmap tcm;

    switch (cmd) {
    default:
	errno = EINVAL;
	return -1;
    case FBIOPUTCMAP:
#define q ((struct fbcmap *)arg)
	if (error = bt_putcmap(q, &sc_cmap, 256))
	    return error;
	if (fb->thc) {		/* CG6 */
	    cg6_loadcmap(fb, &sc_cmap, q->index, q->count);
	} else {		/* CG3 */
	    cg3_loadcmap(fb, &sc_cmap, q->index, q->count);
	}
	break;
    case FBIOSVIDEO:
	if (fb->thc) {		/* CG6 */
	    if (*(int *) arg)
		cg6_unblank(fb);
	    else {
		if (!blanked) {
		    volatile struct cg6_thc *thc =
		    (volatile struct cg6_thc *) fb->thc;

		    thc->thc_misc = (thc->thc_misc & ~THC_MISC_VIDEN);
		}
		blanked = 1;
	    }
	} else {		/* CG3 */
	    if (*(int *) arg)
		cg3_unblank(fb);
	    else {
		if (!blanked) {
		    volatile struct bt_regs *bt;

		    bt = (struct bt_regs *) fb->ramdac;
		    bt->bt_addr = 0x06;	/* command reg */
		    bt->bt_ctrl = 0x70;	/* overlay plane */
		    bt->bt_addr = 0x04;	/* read mask */
		    bt->bt_ctrl = 0x00;	/* color planes */

		    /*
                     * Set color 0 to black -- note that this overwrites R of
                     * color 1.
                     */
		    bt->bt_addr = 0;
		    bt->bt_cmap = 0;

		}
		blanked = 1;
	    }
	}
	break;
/* these are for both FBIOSCURSOR and FBIOGCURSOR */
#define p ((struct fbcursor *)arg)
#define cc (&sc_cursor)

    case FBIOSCURSOR:
	if (!fb->thc) {		/* reject non CG6 */
	    errno = EINVAL;
	    return -1;
	}
	/*
         * For setcmap and setshape, verify parameters, so that
         * we do not get halfway through an update and then crap
         * out with the software state screwed up.
         */
	v = p->set;
	if (v & FB_CUR_SETCMAP) {
	    /*
            * This use of a temporary copy of the cursor
            * colormap is not terribly efficient, but these
            * copies are small (8 bytes)...
            */
	    tcm = cc->cc_color;
	    error = bt_putcmap(&p->cmap, (union bt_cmap *) &tcm, 2);
	    if (error)
		return (error);
	}
	if (v & FB_CUR_SETSHAPE) {
	    if ((u_int) p->size.x > 32 || (u_int) p->size.y > 32)
		return (EINVAL);
	    count = p->size.y * 32 / NBBY;
	    if (!useracc(p->image, count, B_READ) ||
		!useracc(p->mask, count, B_READ))
		return (EFAULT);
	}
	/* parameters are OK; do it */
	if (v & (FB_CUR_SETCUR | FB_CUR_SETPOS | FB_CUR_SETHOT)) {
	    if (v & FB_CUR_SETCUR)
		cc->cc_enable = p->enable;
	    if (v & FB_CUR_SETPOS)
		cc->cc_pos = p->pos;
	    if (v & FB_CUR_SETHOT)
		cc->cc_hot = p->hot;
	    cg6_setcursor(fb);
	}
	if (v & FB_CUR_SETCMAP) {
	    cc->cc_color = tcm;
	    cg6_loadomap(fb);	/* XXX defer to vertical retrace */
	}
	if (v & FB_CUR_SETSHAPE) {
	    cc->cc_size = p->size;
	    count = p->size.y * 32 / NBBY;
	    bzero((caddr_t) cc->cc_bits, sizeof cc->cc_bits);
	    bcopy(p->mask, (caddr_t) cc->cc_bits[0], count);
	    bcopy(p->image, (caddr_t) cc->cc_bits[1], count);
	    cg6_loadcursor(fb);
	}
	break;
    case FBIOSCURPOS:
	if (!fb->thc) {		/* reject non CG6 */
	    errno = EINVAL;
	    return -1;
	}
	sc_cursor.cc_pos = *(struct fbcurpos *) arg;
	cg6_setcursor(fb);
	break;

    case FBIOGCURMAX:
	if (!fb->thc) {		/* reject non CG6 */
	    errno = EINVAL;
	    return -1;
	}
	/* max cursor size is 32x32 */
	((struct fbcurpos *) arg)->x = 32;
	((struct fbcurpos *) arg)->y = 32;
	break;
    case FBIORESET:
	if (!fb->thc) {		/* reject non CG6 */
	    errno = EINVAL;
	    return -1;
	}
	cg6_reset(fb);
    }
    return 0;
}
