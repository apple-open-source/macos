/*
 * ffb_asm.s: Fast Creator raster op inner loops.
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
/* $XFree86: xc/programs/Xserver/hw/xfree86/drivers/sunffb/ffb_asm.s,v 1.1 2000/05/18 23:21:35 dawes Exp $ */

/* Hardware register offsets */
#define FFB_BY		0x0060
#define FFB_DY		0x0068
#define FFB_BH		0x0070
#define FFB_PPC		0x0200
#define FFB_DRAWOP	0x0300
#define FFB_UCSR	0x0900

#define FFB_DRAWOP_VSCROLL	0x0b

#define FIFO_CACHE	0x00

#define BOX_X1		0x00
#define BOX_Y1		0x02
#define BOX_X2		0x04
#define BOX_Y2		0x06

#define POINT_X		0x00
#define POINT_Y		0x02

#define RECT_X		0x00
#define RECT_Y		0x02
#define RECT_W		0x04
#define RECT_H		0x06

#define SEG_X1		0x00
#define SEG_Y1		0x02
#define SEG_X2		0x04
#define SEG_Y2		0x06

	.text

	.align	32
	.globl	FFB_STIPPLE_LOAD
	/* %o0 = &ffbregs->pattern[0]
	 * %o1 = &stipple->bits[0]
	 */
FFB_STIPPLE_LOAD:
	cmp	%g0, 0
1:	ldx	[%o1 + 0x00], %g1
	ldx	[%o1 + 0x08], %g2
	ldx	[%o1 + 0x10], %g3

	ldx	[%o1 + 0x18], %g4
	add	%o0, 0x40, %o0
	ldx	[%o1 + 0x20], %g5
	ldx	[%o1 + 0x28], %o2

	ldx	[%o1 + 0x30], %o3
	ldx	[%o1 + 0x38], %o4
	stx	%g1, [%o0 - 0x40]
	stx	%g2, [%o0 - 0x38]

	stx	%g3, [%o0 - 0x30]
	stx	%g4, [%o0 - 0x28]
	add	%o1, 0x40, %o1
	stx	%g5, [%o0 - 0x20]

	stx	%o2, [%o0 - 0x18]
	stx	%o3, [%o0 - 0x10]
	stx	%o4, [%o0 - 0x08]
	be,pt	%icc, 1b

	 cmp	%g0, 1
	retl
	 nop

#define	FIFO_WAIT(ffbregs, goal, this_label, done_label) \
this_label: \
	lduw	[ffbregs + FFB_UCSR], %g1; \
	and	%g1, 0xfff, %g1; \
	subcc	%g1, (4 + goal), %g1; \
	bge,pt	%icc, done_label; \
	 nop; \
	ba,a,pt	%xcc, this_label

	.align	32
	.globl	FFB_PPT_BOX_LOOP
	/* This is only used (currently) by vscroll, so we put
	 * the creator hwbug workaround in here (writing
	 * the drawop each iteration).
	 *
	 * %o0 = ffbpriv, %o1 = ffbregs,
	 * %o2 = pbox, %o3 = pbox_last, %o4 = ppt
	 */
FFB_PPT_BOX_LOOP:
	lduh	[%o0 + FIFO_CACHE], %g1	/* Load		Group		*/
	sethi	%hi(FFB_DRAWOP), %g2	/* IEU0				*/
	cmp	%o2, %o3		/* IEU1				*/
	bgu,pn	%icc, 2f		/* CTI				*/

	 or	%g2,%lo(FFB_DRAWOP),%g2	/* IEU0		Group		*/
1:	lduh	[%o4 + POINT_X], %g3	/* Load		Group		*/
	lduh	[%o4 + POINT_Y], %g4	/* Load		Group		*/
	lduh	[%o2 + BOX_Y2], %g5	/* Load		Group		*/

	lduh	[%o2 + BOX_Y1], %o5	/* Load		Group		*/
	sllx	%g4, 32, %g4		/* IEU0				*/
	or	%g3, %g4, %g3		/* IEU0		Group		*/
	sub	%g5, %o5, %g5		/* IEU1				*/

	sllx	%o5, 32, %g4		/* IEU0		Group		*/
	sllx	%g5, 32, %g5		/* IEU0		Group		*/
	lduh	[%o2 + BOX_X2], %o5	/* Load				*/
	add	%o4, 0x4, %o4		/* IEU1				*/

	lduh	[%o2 + BOX_X1], %g7	/* Load		Group		*/
	add	%o2, 0x8, %o2		/* IEU0				*/
	sub	%o5, %g7, %o5		/* IEU0		Group		*/
	or	%g4, %g7, %g4		/* IEU1				*/

	or	%g5, %o5, %g5		/* IEU0		Group		*/
	subcc	%g1, 7, %g1		/* IEU1				*/
	bl,pn	%icc, FFB_PPT_BOX_WAIT	/* CTI				*/
9:	 cmp	%o2, %o3		/* IEU1		Group		*/

	/* This works around BUG ID 1189858 -DaveM */
	mov	FFB_DRAWOP_VSCROLL, %o5	/* IEU0				*/
	stw	%o5, [%o1 + %g2]	/* STORE			*/
	stx	%g3, [%o1 + FFB_BY]	/* STORE	Group		*/
	stx	%g4, [%o1 + FFB_DY]	/* STORE	Group		*/

	bleu,pt	%icc, 1b		/* CTI				*/
	 stx	%g5, [%o1 + FFB_BH]	/* STORE	Group		*/
2:	retl				/* CTI		Group		*/
	 sth	%g1, [%o0 + FIFO_CACHE]	/* STORE			*/

	FIFO_WAIT(%o1, 7, FFB_PPT_BOX_WAIT, 9b)

	.align	32
	.globl	FFB_BOX_LOOP
	/* %o0 = ffbpriv, %o1 = ffbregs,
	 * %o2 = pbox, %o3 = pbox_last
	 */
FFB_BOX_LOOP:
	lduh	[%o0 + FIFO_CACHE], %g1	/* Load		Group		*/
	cmp	%o2, %o3		/* IEU0				*/
	bgu,pn	%icc, 2f		/* CTI				*/
	 nop				/* IEU0		Group		*/

1:	lduw	[%o2 + BOX_X1], %g4	/* Load		Group		*/
	lduw	[%o2 + BOX_X2], %g3	/* Load		Group		*/
	sllx	%g4, 32, %o5		/* IEU0				*/
	srl	%g4, 16, %g7		/* IEU0		Group		*/

	add	%o2, 8, %o2		/* IEU1				*/
	sllx	%g3, 32, %g5		/* IEU0		Group		*/
	subcc	%g1, 4, %g1		/* IEU1				*/
	srl	%g3, 16, %o4		/* IEU0		Group		*/

	or	%o5, %g7, %o5		/* IEU1				*/
	or	%g5, %o4, %g5		/* IEU0		Group		*/
	bl,pn	%icc, FFB_BOX_WAIT	/* CTI				*/
	 sub	%g5, %o5, %g5		/* IEU0		Group		*/

9:	cmp	%o2, %o3		/* IEU1				*/
	stx	%o5, [%o1 + FFB_BY]	/* Store			*/
	bleu,pt	%icc, 1b		/* CTI		Group		*/
	 stx	%g5, [%o1 + FFB_BH]	/* Store			*/
2:	retl				/* CTI		Group		*/
	 sth	%g1, [%o0 + FIFO_CACHE]	/* Store			*/

	FIFO_WAIT(%o1, 4, FFB_BOX_WAIT, 9b)

	.align	32
	.globl	FFB_RECT_LOOP
	/* %o0 = ffbpriv, %o1 = ffbregs,
	 * %o2 = prect, %o3 = prect_last,
	 * %o4 = xOrg, %o5 = yOrg
	 */
FFB_RECT_LOOP:
	lduh	[%o0 + FIFO_CACHE], %g1	/* Load		Group		*/
	sllx	%o5, 32, %o5		/* IEU0				*/
	cmp	%o2, %o3		/* IEU1				*/
	bgu,pn	%icc, 2f		/* CTI				*/

	 or	%o4, %o5, %o4		/* IEU0		Group		*/
1:	lduh	[%o2 + RECT_X], %g4	/* Load		Group		*/
	lduh	[%o2 + RECT_Y], %o5	/* Load		Group		*/
	lduh	[%o2 + RECT_W], %g3	/* Load		Group		*/

	lduh	[%o2 + RECT_H], %g2	/* Load		Group		*/
	sllx	%o5, 32, %g7		/* IEU0				*/
	add	%o2, 8, %o2		/* IEU1				*/
	orcc	%g7, %g4, %g7		/* IEU1		Group		*/

	sllx	%g2, 32, %g5		/* IEU0				*/
	or	%g5, %g3, %g5		/* IEU0		Group		*/
	add	%g7, %o4, %g7		/* IEU1				*/
	subcc	%g1, 4, %g1		/* IEU1		Group		*/

	bl,pn	%icc, FFB_RECT_WAIT	/* CTI				*/
9:	 cmp	%o2, %o3		/* IEU1		Group		*/
	stx	%g7, [%o1 + FFB_BY]	/* Store			*/
	bleu,pt	%icc, 1b		/* CTI				*/

	 stx	%g5, [%o1 + FFB_BH]	/* Store	Group		*/
2:	retl				/* CTI		Group		*/
	 sth	%g1, [%o0 + FIFO_CACHE]	/* Store			*/

	FIFO_WAIT(%o1, 4, FFB_RECT_WAIT, 9b)

	.align	32
	.globl	FFB_PPT_WIDTH_LOOP
	/* %o0 = ffbpriv, %o1 = ffbregs,
	 * %o2 = ppt, %o3 = ppt_last,
	 * %o4 = pwidth
	 */
FFB_PPT_WIDTH_LOOP:
	lduh	[%o0 + FIFO_CACHE], %g1	/* Load		Group		*/
	cmp	%o2, %o3		/* IEU1				*/
	bgu,pn	%icc, 2f		/* CTI				*/
	 nop

8:	lduw	[%o2 + POINT_X], %g4	/* Load		Group		*/
	lduw	[%o4], %g2		/* Load		Group		*/
	sllx	%g4, 32, %g7
1:	srl	%g4, 16, %g5		/* IEU0		Group		*/

	add	%o2, 4, %o2		/* IEU1				*/
	brz,pn	%g2, 8b			/* CTI+IEU1	Group		*/
	 add	%o4, 4, %o4		/* IEU0				*/
	or	%g5, %g7, %g7		/* IEU0		Group		*/

	subcc	%g1, 5, %g1		/* IEU1				*/
	bl,pn	%icc, FFB_PPT_WIDTH_WAIT/* CTI				*/
	 add	%g7, %g2, %o5		/* IEU0		Group		*/
9:	stw	%g0, [%o1 + FFB_PPC]	/* Store	Group		*/

	cmp	%o2, %o3		/* IEU1				*/
	stx	%g7, [%o1 + FFB_BY]	/* Store	Group		*/
	bleu,pt	%icc, 8b		/* CTI				*/
	 stx	%o5, [%o1 + FFB_BH]	/* Store	Group		*/

2:	retl				/* CTI		Group		*/
	 sth	%g1, [%o0 + FIFO_CACHE]	/* Store			*/

	FIFO_WAIT(%o1, 5, FFB_PPT_WIDTH_WAIT, 9b)

	.align	32
	.globl	FFB_LINE_LOOP1
	/* %o0 = ffbpriv, %o1 = ffbregs,
	 * %o2 = ppt, %o3 = ppt_last,
	 * %o4 = xOrg, %o5 = yOrg
	 *
	 * We return ppt_last + 1.
	 */
FFB_LINE_LOOP1:
	lduh	[%o0 + FIFO_CACHE], %g1
	sllx	%o5, 32, %o5
	and	%o4, 0x7ff, %o4
	cmp	%o2, %o3

	bgu,pn	%icc, 2f
	 or	%o4, %o5, %o4
1:	lduw	[%o2 + POINT_X], %g4
	sllx	%g4, 32, %g7

	srl	%g4, 16, %g3
	add	%o2, 4, %o2
	or	%g7, %g3, %g7
	subcc	%g1, 3, %g1
 
	bl,pn	%icc, FFB_LINE1_WAIT
	 add	%g7, %o4, %g7
9:	cmp	%o2, %o3
	stw	%g0, [%o1 + FFB_PPC]

	bleu,pt	%icc, 1b
	 stx	%g7, [%o1 + FFB_BH]
2:	nop
	sth	%g1, [%o0 + FIFO_CACHE]

	retl
	 mov	%o2, %o0

	FIFO_WAIT(%o1, 3, FFB_LINE1_WAIT, 9b)

	.align	32
	.globl	FFB_LINE_LOOP2
	/* %o0 = ffbpriv, %o1 = ffbregs,
	 * %o2 = ppt, %o3 = ppt_last,
	 * %o4 = &xOrg, %o5 = &yOrg
	 *
	 * We return ppt_last + 1.
	 * The only difference between the previous routine
	 * is that here we accumulate the origin values.
	 */
FFB_LINE_LOOP2:
	lduw	[%o5], %g5
	lduw	[%o4], %g2
	lduh	[%o0 + FIFO_CACHE], %g1
	sllx	%g5, 32, %g5

	and	%g2, 0x7ff, %g2
	cmp	%o2, %o3
	bgu,pn	%icc, 2f
	 or	%g5, %g2, %g5

1:	lduw	[%o2 + POINT_X], %g4
	sllx	%g4, 32, %g7
	srl	%g4, 16, %g3
	add	%o2, 4, %o2
 
	or	%g7, %g3, %g7
	subcc	%g1, 3, %g1
	bl,pn	%icc, FFB_LINE2_WAIT
	 add	%g7, %g5, %g5

9:	cmp	%o2, %o3
	stw	%g0, [%o1 + FFB_PPC]
	bleu,pt	%icc, 1b
	 stx	%g5, [%o1 + FFB_BH]

2:	sth	%g1, [%o0 + FIFO_CACHE]
	srlx	%g5, 32, %g4
	stw	%g5, [%o4]
	stw	%g4, [%o5]

	retl
	 mov	%o2, %o0

	FIFO_WAIT(%o1, 3, FFB_LINE2_WAIT, 9b)
