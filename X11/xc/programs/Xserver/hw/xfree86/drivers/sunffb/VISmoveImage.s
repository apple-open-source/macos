/*
 * VISmoveImage.S: High speed moveImage operations utilizing the UltraSPARC
 *                 Visual Instruction Set.
 *
 * Copyright (C) 1998,1999 Jakub Jelinek (jakub@redhat.com)
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
 * JAKUB JELINEK BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER
 * IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
 * CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 */
/* $XFree86: xc/programs/Xserver/hw/xfree86/drivers/sunffb/VISmoveImage.s,v 1.2 2000/05/23 04:47:43 dawes Exp $ */


/* NOTE NOTE NOTE: All loads in these routines _MUST_ be 64-byte block
 *                 loads.  If any non-block loads are done to the frame-
 *                 buffer when prefetching is enabled (which we use, when
 *		   available) this will cause lockups on FFB2 due to a
 *		   hardware bug. -DaveM
 */

/* 
   void
   VISmoveImageLR(unsigned char *src, unsigned char *dst, long w, long h, long skind, long dkind)
   {
     int i;

     // Handles copying non-overlapping images and for overlapping images
     // copying where dstx <= srcx.
     // To copy overlapping images where dsty > srcy, set src and dst
     // to start of the last scanline and negate both skind and dkind arguments.

     assert(abs(skind) >= w && abs(dkind) >= w);

     while (h--) {
       for (i = 0; i < w; i++)
         *d++ = *s++;
       s += skind - w;
       d += dkind - w;
     }
   }

   void
   VISmoveImageRL(unsigned char *src, unsigned char *dst, long w, long h, long skind, long dkind)
   {
     int i;

     // Handles copying non-overlapping images and for overlapping images
     // copying where dstx >= srcx.
     // To copy overlapping images where dsty > srcy, set src and dst
     // to start of the last scanline and negate both skind and dkind arguments.

     assert(abs(skind) >= w && abs(dkind) >= w);

     s += w;
     d += w;
     while (h--) {
       for (i = 0; i < w; i++)
         *--d = *--s;
       s += skind + w;
       d += dkind + w;
     }
   }

 */

#if defined(__sparc_v9__) || defined(__sparcv9) || defined(__arch64__)
#define SMUL			mulx
#define ICC			xcc
#else
#define SMUL			smul
#define ICC			icc
#endif

#define ASI_PST8_P		0xc0
#define ASI_BLK_P		0xf0
#define SYNC			0x40

/* Register usage:								*/
/*   integer registers:								*/
#define src			i0
#define dst			i1
#define w			i2
#define h			i3
#define skind			i4
#define dkind			i5
#define branchbase		l0
#define tmp1			l1
#define tmp4			l2
#define tmp5			l3
#define leftw			l4
#define rightw			l5
#define srcstart		l6
#define mode			l7
#define fregset			o0		/* Must be [og]? for v8plusa */
#define srcend			o1
#define srcn			o2
#define srcnext			o3
#define rightw2			o4
#define tmp2			o5		/* Must be [og]? for v8plusa */
#define narrow			o7
#define prepw			g1		/* Must be [og]? for v8plusa */
#define srcstop			g2
#define srcnotdone		g3
#define curw			g4		/* Must be [og]? for v8plusa */
#define leftw2			rightw2
#define tmp3			g5		/* Must be [og]? for v8plusa */
/*   floating point registers:							*/
#define ftmp1			f0
#define ftmp2			f2
#define ftmp3			f4
#define ftmp4			f6
#define ftmp5			f8
#define ftmp6			f10
#define ftmp7			f12
#define ftmp8			f14
#define store_regs		f16-f31
#define load_bank1		f32-f46
#define load_bank2		f48-f62
#define fnop			fmovd	%ftmp2, %ftmp2

#define LOAD(f32,f46,tgt,tgtr)							\
	subcc			%curw, 64, %curw;				\
	bleu,pn			%ICC, tgt;					\
	 fmovd			%f46, %ftmp1;					\
	ldda			[%src] ASI_BLK_P, %f32;				\
	add			%src, 64, %src;					\
tgtr:

#define PREPLOAD(f32,f46,tgt,tgtr)						\
	brlez,pn		%prepw, tgt;					\
	 fmovd			%f46, %ftmp1;					\
	ldda			[%src] ASI_BLK_P, %f32;				\
	add			%src, 64, %src;					\
tgtr:

#define STORE									\
	stda			%f16, [%dst] ASI_BLK_P;

#define FREG_FROB(f0,A0,F2,f2,A1,F4,f4,A2,F6,f6,A3,F8,f8,A4,F10,f10,A5,F12,f12,A6,F14,f14,A7,F16) 	\
	A0									\
	faligndata		%f0,%F2,%f16;					\
	A1									\
	faligndata		%f2,%F4,%f18;					\
	A2									\
	faligndata		%f4,%F6,%f20;					\
	A3									\
	faligndata		%f6,%F8,%f22;					\
	A4									\
	faligndata		%f8,%F10,%f24;					\
	A5									\
	faligndata		%f10,%F12,%f26;					\
	A6									\
	faligndata		%f12,%F14,%f28;					\
	A7									\
	faligndata		%f14,%F16,%f30;

	.section		".rodata"
	.asciz			"VISmoveImage (C) 1998,1999 Jakub Jelinek"

/* The code might not be self-explanatory, but it was written to be processed
 * by machines, not humans.  Comments are deliberately left as an exercise
 * to the occasional reader.  */

	.text
	.globl			VISmoveImageLR
	.align			32
VISmoveImageLR:
	save			%sp, -160, %sp				! Group 0
0:	rd			%pc, %tmp3				! Group 1
	sub			%src, %dst, %mode			! Group 7
	brz,pn			%h, return
	 neg			%dst, %leftw				! Group 8
	mov			%src, %srcstart
	andn			%src, 63, %src				! Group 9
	cmp			%w, 128
	blu,pn			%ICC, prepare_narrow
	 and			%mode, 63, %mode			! Group 10
	add			%dst, %w, %rightw
	ldda			[%src] ASI_BLK_P, %f32			! Group 11
	add			%src, 64, %src				! Group 12
	clr			%narrow
	ldda			[%src] ASI_BLK_P, %f48			! Group 13
	and			%leftw, 63, %leftw			! Group 14
	sub			%rightw, 1, %rightw
	andn			%dst, 63, %dst				! Group 15
	and			%rightw, 63, %rightw
	alignaddr		%mode, %g0, %g0				! Group 16
	add			%rightw, 1, %rightw			! Group 17
	clr			%fregset
	add			%src, 64, %src				! Group 18
	add			%tmp3, (BranchBase - 0b), %branchbase
	mov			64, %prepw				! Group 19
	ba,pt			%xcc, roll_wide
	 sub			%h, 1, %srcnotdone
prepare_narrow:
#if defined(__sparc_v9__) || defined(__sparcv9) || defined(__arch64__)
	brlez,pn		%w, return
#else
	tst			%w
	ble,pn			%icc, return
#endif
	 add			%dst, %w, %rightw			! Group 11
	and			%leftw, 63, %leftw
	ldda			[%src] ASI_BLK_P, %f32			! Group 12
	sub			%rightw, 1, %rightw			! Group 13
	andn			%dst, 63, %dst
	and			%rightw, 63, %rightw			! Group 14
	sub			%w, %leftw, %tmp2
	add			%rightw, 1, %rightw			! Group 15
	clr			%fregset
	subcc			%tmp2, %rightw, %curw			! Group 16
	and			%srcstart, 63, %tmp1
	mov			%curw, %prepw				! Group 17
	add			%tmp1, %w, %tmp1
	bg,pt			%ICC, 1f
	 add			%src, 64, %src				! Group 18
	cmp			%tmp1, 128
	movg			%icc, 64, %prepw			! Group 19
1:	add			%srcstart, %w, %srcend
	sub			%h, 1, %srcnotdone			! Group 20
	add			%srcstart, %skind, %srcnext
	add			%srcend, 63, %tmp4			! Group 21
	andn			%srcnext, 63, %srcnext
	add			%w, 64, %tmp2				! Group 22
	cmp			%skind, 0
	bl,pn			%ICC, 1f
	 clr			%srcstop				! Group 23
	cmp			%skind, %tmp2
	bgeu,pt			%ICC, 2f
	 andn			%tmp4, 63, %tmp2			! Group 24
	SMUL			%skind, %srcnotdone, %srcstop
	ba,pt			%xcc, 2f
	 add			%tmp4, %srcstop, %srcstop
1:	addcc			%tmp2, %skind, %g0
	ble,pt			%ICC, 2f
	 sub			%srcnext, 64, %tmp2
	SMUL			%skind, %srcnotdone, %srcstop
	sub			%srcstart, 64, %narrow
	add			%srcstop, %narrow, %srcstop
2:	add			%srcnext, 64, %narrow
	andn			%tmp4, 63, %srcend			! Group 25
	mov			%srcnext, %srcn
	cmp			%narrow, %srcend			! Group 26
	andn			%srcstop, 63, %srcstop
	move			%ICC, %tmp2, %srcnext			! Group 27
	alignaddr		%mode, %g0, %g0				! Group 28
	add			%tmp3, (BranchBase - 0b), %branchbase
	mov			1, %narrow				! Group 29
	cmp			%srcnext, %srcstop
	move			%ICC, 0, %srcnotdone			! Group 30
	cmp			%tmp1, 64
	bleu,pn			%ICC, 1f
	 dec			%h					! Group 31
	ldda			[%src] ASI_BLK_P, %f48			! Group 32
	ba,pt			%xcc, roll_narrow			! Group 33
	 addcc			%src, 64, %src
1:	brz,a,pn		%srcnotdone, roll_narrow
	 membar			#Sync
	ldda			[%srcnext] ASI_BLK_P, %f48
	ba,pt			%xcc, roll_narrow
	 addcc			%srcnext, 64, %src

	.align			32
prepr0:	faligndata		%f32, %f34, %f16
	faligndata		%f34, %f36, %f18
	faligndata		%f36, %f38, %f20
	faligndata		%f38, %f40, %f22
	faligndata		%f40, %f42, %f24
	faligndata		%f42, %f44, %f26
	ba,pt			%xcc, 4f
	 faligndata		%f44, %f46, %f28
	.align			32
prepr1:	faligndata		%f48, %f50, %f16
	faligndata		%f50, %f52, %f18
	faligndata		%f52, %f54, %f20
	faligndata		%f54, %f56, %f22
	faligndata		%f56, %f58, %f24
	faligndata		%f58, %f60, %f26
	ba,pt			%xcc, 5f
	 faligndata		%f60, %f62, %f28
	.align			32
prepr2:	faligndata		%f34, %f36, %f16
	faligndata		%f36, %f38, %f18
	faligndata		%f38, %f40, %f20
	faligndata		%f40, %f42, %f22
	faligndata		%f42, %f44, %f24
	faligndata		%f44, %f46, %f26
	ba,pt			%xcc, narrowst
	 faligndata		%f46, %f0, %f28
	.align			32
prepr3:	faligndata		%f50, %f52, %f16
	faligndata		%f52, %f54, %f18
	faligndata		%f54, %f56, %f20
	faligndata		%f56, %f58, %f22
	faligndata		%f58, %f60, %f24
	faligndata		%f60, %f62, %f26
	ba,pt			%xcc, narrowst
	 faligndata		%f62, %f0, %f28
	.align			32
prepr4:	faligndata		%f36, %f38, %f16
	faligndata		%f38, %f40, %f18
	faligndata		%f40, %f42, %f20
	faligndata		%f42, %f44, %f22
	faligndata		%f44, %f46, %f24
	ba,pt			%xcc, narrowst
	 faligndata		%f46, %f0, %f26
	.align			32
prepr5:	faligndata		%f52, %f54, %f16
	faligndata		%f54, %f56, %f18
	faligndata		%f56, %f58, %f20
	faligndata		%f58, %f60, %f22
	faligndata		%f60, %f62, %f24
	ba,pt			%xcc, narrowst
	 faligndata		%f62, %f0, %f26
	.align			32
prepr6:	faligndata		%f38, %f40, %f16
	faligndata		%f40, %f42, %f18
	faligndata		%f42, %f44, %f20
	faligndata		%f44, %f46, %f22
	ba,pt			%xcc, narrowst
	 faligndata		%f46, %f0, %f24
	.align			32
prepr7:	faligndata		%f54, %f56, %f16
	faligndata		%f56, %f58, %f18
	faligndata		%f58, %f60, %f20
	faligndata		%f60, %f62, %f22
	ba,pt			%xcc, narrowst
	 faligndata		%f62, %f0, %f24
	.align			32
prepr8:	faligndata		%f40, %f42, %f16
	faligndata		%f42, %f44, %f18
	faligndata		%f44, %f46, %f20
	ba,pt			%xcc, narrowst
	 faligndata		%f46, %f0, %f22
	.align			32
prepr9:	faligndata		%f56, %f58, %f16
	faligndata		%f58, %f60, %f18
	faligndata		%f60, %f62, %f20
	ba,pt			%xcc, narrowst
	 faligndata		%f62, %f0, %f22
	.align			32
prepr10:faligndata		%f42, %f44, %f16
	faligndata		%f44, %f46, %f18
	ba,pt			%xcc, narrowst
	 faligndata		%f46, %f0, %f20
	.align			32
prepr11:faligndata		%f58, %f60, %f16
	faligndata		%f60, %f62, %f18
	ba,pt			%xcc, narrowst
	 faligndata		%f62, %f0, %f20
	.align			32
prepr12:faligndata		%f44, %f46, %f16
	ba,pt			%xcc, narrowst
	 faligndata		%f46, %f0, %f18
	.align			32
prepr13:faligndata		%f60, %f62, %f16
	ba,pt			%xcc, narrowst
	 faligndata		%f62, %f0, %f18
	.align			32
prepr14:ba,pt			%xcc, narrowst
	 faligndata		%f46, %f0, %f16
	nop
	nop
4:	ba,pt			%xcc, narrowst
	 faligndata		%f46, %f0, %f30
	.align			32
prepr15:ba,pt			%xcc, narrowst
	 faligndata		%f62, %f0, %f16
	nop
	nop
5:	ba,pt			%xcc, narrowst
	 faligndata		%f62, %f0, %f30

	.align			32
prepn0:	faligndata		%ftmp1, %f32, %f30
	ba,pt			%xcc, leftst
	 mov			(vis0 - BranchBase), %tmp3
	nop
1:	ba,pt			%xcc, leftst
	 mov			(vis12 - BranchBase), %tmp3
	.align			32
prepn1:	faligndata		%ftmp1, %f48, %f30
	ba,pt			%xcc, leftst
	 mov			(vis1 - BranchBase), %tmp3
	nop
2:	ba,pt			%xcc, leftst
	 mov			(vis13 - BranchBase), %tmp3
	.align			32
prepn2: faligndata		%ftmp1, %f32, %f28
	faligndata		%f32, %f34, %f30
	ba,pt			%xcc, leftst
	 mov			(vis2 - BranchBase), %tmp3
3:	faligndata		%f44, %f46, %f30
	ba,pt			%xcc, leftst
	 mov			(vis14 - BranchBase), %tmp3
	.align			32
prepn3: faligndata		%ftmp1, %f48, %f28
	faligndata		%f48, %f50, %f30
	ba,pt			%xcc, leftst
	 mov			(vis3 - BranchBase), %tmp3
	.align			32
prepn4: faligndata		%ftmp1, %f32, %f26
	faligndata		%f32, %f34, %f28
	faligndata		%f34, %f36, %f30
	ba,pt			%xcc, leftst
	 mov			(vis4 - BranchBase), %tmp3
	.align			32
prepn5:	faligndata		%ftmp1, %f48, %f26
	faligndata		%f48, %f50, %f28
	faligndata		%f50, %f52, %f30
	ba,pt			%xcc, leftst
	 mov			(vis5 - BranchBase), %tmp3
	.align			32
prepn6: faligndata		%ftmp1, %f32, %f24
	faligndata		%f32, %f34, %f26
	faligndata		%f34, %f36, %f28
	faligndata		%f36, %f38, %f30
	ba,pt			%xcc, leftst
	 mov			(vis6 - BranchBase), %tmp3
	.align			32
prepn7:	faligndata		%ftmp1, %f48, %f24
	faligndata		%f48, %f50, %f26
	faligndata		%f50, %f52, %f28
	faligndata		%f52, %f54, %f30
	ba,pt			%xcc, leftst
	 mov			(vis7 - BranchBase), %tmp3
	.align			32
prepn8:	faligndata		%ftmp1, %f32, %f22
	faligndata		%f32, %f34, %f24
	faligndata		%f34, %f36, %f26
	faligndata		%f36, %f38, %f28
	faligndata		%f38, %f40, %f30
	ba,pt			%xcc, leftst
	 mov			(vis8 - BranchBase), %tmp3
	.align			32
prepn9:	faligndata		%ftmp1, %f48, %f22
	faligndata		%f48, %f50, %f24
	faligndata		%f50, %f52, %f26
	faligndata		%f52, %f54, %f28
	faligndata		%f54, %f56, %f30
	ba,pt			%xcc, leftst
	 mov			(vis9 - BranchBase), %tmp3
	.align			32
prepn10:faligndata		%ftmp1, %f32, %f20
	faligndata		%f32, %f34, %f22
	faligndata		%f34, %f36, %f24
	faligndata		%f36, %f38, %f26
	faligndata		%f38, %f40, %f28
	faligndata		%f40, %f42, %f30
	ba,pt			%xcc, leftst
	 mov			(vis10 - BranchBase), %tmp3
	.align			32
prepn11:faligndata		%ftmp1, %f48, %f20
	faligndata		%f48, %f50, %f22
	faligndata		%f50, %f52, %f24
	faligndata		%f52, %f54, %f26
	faligndata		%f54, %f56, %f28
	faligndata		%f56, %f58, %f30
	ba,pt			%xcc, leftst
	 mov			(vis11 - BranchBase), %tmp3
	.align			32
prepn12:faligndata		%ftmp1, %f32, %f18
	faligndata		%f32, %f34, %f20
	faligndata		%f34, %f36, %f22
	faligndata		%f36, %f38, %f24
	faligndata		%f38, %f40, %f26
	faligndata		%f40, %f42, %f28
	ba,pt			%xcc, 1b
	 faligndata		%f42, %f44, %f30
	.align			32
prepn13:faligndata		%ftmp1, %f48, %f18
	faligndata		%f48, %f50, %f20
	faligndata		%f50, %f52, %f22
	faligndata		%f52, %f54, %f24
	faligndata		%f54, %f56, %f26
	faligndata		%f56, %f58, %f28
	ba,pt			%xcc, 2b
	 faligndata		%f58, %f60, %f30
	.align			32
prepn14:faligndata		%ftmp1, %f32, %f16
	faligndata		%f32, %f34, %f18
	faligndata		%f34, %f36, %f20
	faligndata		%f36, %f38, %f22
	faligndata		%f38, %f40, %f24
	faligndata		%f40, %f42, %f26
	ba,pt			%xcc, 3b
	 faligndata		%f42, %f44, %f28
	.align			32
prepn15:faligndata		%ftmp1, %f48, %f16
	faligndata		%f48, %f50, %f18
	faligndata		%f50, %f52, %f20
	faligndata		%f52, %f54, %f22
	faligndata		%f54, %f56, %f24
	faligndata		%f56, %f58, %f26
	faligndata		%f58, %f60, %f28
	faligndata		%f60, %f62, %f30
	ba,pt			%xcc, leftst
	 mov			(vis15 - BranchBase), %tmp3

	.align			64
BranchBase:
prepl0:	FREG_FROB(f32,,f34,f34,,f36,f36,,f38,f38,,f40,f40,,f42,f42,,f44,f44,,f46,ftmp1,PREPLOAD(f32,f46,prep0e,prep0r),f48)
	ba,pt			%xcc, leftst
	 mov			(vis1 - BranchBase), %tmp3
	.align			64
prepl1:	FREG_FROB(f48,,f50,f50,,f52,f52,,f54,f54,,f56,f56,,f58,f58,,f60,f60,,f62,ftmp1,PREPLOAD(f48,f62,prep1e,prep1r),f32)
	ba,pt			%xcc, leftst
	 mov			(vis0 - BranchBase), %tmp3
	.align			64
prepl2:	FREG_FROB(f34,,f36,f36,,f38,f38,,f40,f40,,f42,f42,,f44,f44,,f46,ftmp1,PREPLOAD(f32,f46,prep2e,prep2r),f48,f48,,f50)
	ba,pt			%xcc, leftst
	 mov			(vis3 - BranchBase), %tmp3
	.align			64
prepl3:	FREG_FROB(f50,,f52,f52,,f54,f54,,f56,f56,,f58,f58,,f60,f60,,f62,ftmp1,PREPLOAD(f48,f62,prep3e,prep3r),f32,f32,,f34)
	ba,pt			%xcc, leftst
	 mov			(vis2 - BranchBase), %tmp3
	.align			64
prepl4:	FREG_FROB(f36,,f38,f38,,f40,f40,,f42,f42,,f44,f44,,f46,ftmp1,PREPLOAD(f32,f46,prep4e,prep4r),f48,f48,,f50,f50,,f52)
	ba,pt			%xcc, leftst
	 mov			(vis5 - BranchBase), %tmp3
	.align			64
prepl5:	FREG_FROB(f52,,f54,f54,,f56,f56,,f58,f58,,f60,f60,,f62,ftmp1,PREPLOAD(f48,f62,prep5e,prep5r),f32,f32,,f34,f34,,f36)
	ba,pt			%xcc, leftst
	 mov			(vis4 - BranchBase), %tmp3
	.align			64
prepl6:	FREG_FROB(f38,,f40,f40,,f42,f42,,f44,f44,,f46,ftmp1,PREPLOAD(f32,f46,prep6e,prep6r),f48,f48,,f50,f50,,f52,f52,,f54)
	ba,pt			%xcc, leftst
	 mov			(vis7 - BranchBase), %tmp3
	.align			64
prepl7:	FREG_FROB(f54,,f56,f56,,f58,f58,,f60,f60,,f62,ftmp1,PREPLOAD(f48,f62,prep7e,prep7r),f32,f32,,f34,f34,,f36,f36,,f38)
	ba,pt			%xcc, leftst
	 mov			(vis6 - BranchBase), %tmp3
	.align			64
prepl8:	FREG_FROB(f40,,f42,f42,,f44,f44,,f46,ftmp1,PREPLOAD(f32,f46,prep8e,prep8r),f48,f48,,f50,f50,,f52,f52,,f54,f54,,f56)
	ba,pt			%xcc, leftst
	 mov			(vis9 - BranchBase), %tmp3
	.align			64
prepl9:	FREG_FROB(f56,,f58,f58,,f60,f60,,f62,ftmp1,PREPLOAD(f48,f62,prep9e,prep9r),f32,f32,,f34,f34,,f36,f36,,f38,f38,,f40)
	ba,pt			%xcc, leftst
	 mov			(vis8 - BranchBase), %tmp3
	.align			64
prepl10:FREG_FROB(f42,,f44,f44,,f46,ftmp1,PREPLOAD(f32,f46,prep10e,prep10r),f48,f48,,f50,f50,,f52,f52,,f54,f54,,f56,f56,,f58)
	ba,pt			%xcc, leftst
	 mov			(vis11 - BranchBase), %tmp3
	.align			64
prepl11:FREG_FROB(f58,,f60,f60,,f62,ftmp1,PREPLOAD(f48,f62,prep11e,prep11r),f32,f32,,f34,f34,,f36,f36,,f38,f38,,f40,f40,,f42)
	ba,pt			%xcc, leftst
	 mov			(vis10 - BranchBase), %tmp3
	.align			64
prepl12:FREG_FROB(f44,,f46,ftmp1,PREPLOAD(f32,f46,prep12e,prep12r),f48,f48,,f50,f50,,f52,f52,,f54,f54,,f56,f56,,f58,f58,,f60)
	ba,pt			%xcc, leftst
	 mov			(vis13 - BranchBase), %tmp3
	.align			64
prepl13:FREG_FROB(f60,,f62,ftmp1,PREPLOAD(f48,f62,prep13e,prep13r),f32,f32,,f34,f34,,f36,f36,,f38,f38,,f40,f40,,f42,f42,,f44)
	ba,pt			%xcc, leftst
	 mov			(vis12 - BranchBase), %tmp3
	.align			64
prepl14:FREG_FROB(ftmp1,PREPLOAD(f32,f46,prep14e,prep14r),f48,f48,,f50,f50,,f52,f52,,f54,f54,,f56,f56,,f58,f58,,f60,f60,,f62)
	ba,pt			%xcc, leftst
	 mov			(vis15 - BranchBase), %tmp3
	.align			64
prepl15:FREG_FROB(ftmp1,PREPLOAD(f48,f62,prep15e,prep15r),f32,f32,,f34,f34,,f36,f36,,f38,f38,,f40,f40,,f42,f42,,f44,f44,,f46)
	ba,pt			%xcc, leftst
	 mov			(vis14 - BranchBase), %tmp3

	.align			128
vis0:	FREG_FROB(f32,,f34,f34,,f36,f36,,f38,f38,,f40,f40,,f42,f42,,f44,f44,,f46,ftmp1,LOAD(f32,f46,vis0e,vis0r),f48)
	STORE
	add			%dst, 64, %dst
vis1:	FREG_FROB(f48,,f50,f50,,f52,f52,,f54,f54,,f56,f56,,f58,f58,,f60,f60,,f62,ftmp1,LOAD(f48,f62,vis1e,vis1r),f32)
	STORE
	ba,pt			%xcc, vis0
	 add			%dst, 64, %dst
	.align			128
vis2:	FREG_FROB(f34,,f36,f36,,f38,f38,,f40,f40,,f42,f42,,f44,f44,,f46,ftmp1,LOAD(f32,f46,vis2e,vis2r),f48,f48,,f50)
	STORE
	add			%dst, 64, %dst
vis3:	FREG_FROB(f50,,f52,f52,,f54,f54,,f56,f56,,f58,f58,,f60,f60,,f62,ftmp1,LOAD(f48,f62,vis3e,vis3r),f32,f32,,f34)
	STORE
	ba,pt			%xcc, vis2
	 add			%dst, 64, %dst
	.align			128
vis4:	FREG_FROB(f36,,f38,f38,,f40,f40,,f42,f42,,f44,f44,,f46,ftmp1,LOAD(f32,f46,vis4e,vis4r),f48,f48,,f50,f50,,f52)
	STORE
	add			%dst, 64, %dst
vis5:	FREG_FROB(f52,,f54,f54,,f56,f56,,f58,f58,,f60,f60,,f62,ftmp1,LOAD(f48,f62,vis5e,vis5r),f32,f32,,f34,f34,,f36)
	STORE
	ba,pt			%xcc, vis4
	 add			%dst, 64, %dst
	.align			128
vis6:	FREG_FROB(f38,,f40,f40,,f42,f42,,f44,f44,,f46,ftmp1,LOAD(f32,f46,vis6e,vis6r),f48,f48,,f50,f50,,f52,f52,,f54)
	STORE
	add			%dst, 64, %dst
vis7:	FREG_FROB(f54,,f56,f56,,f58,f58,,f60,f60,,f62,ftmp1,LOAD(f48,f62,vis7e,vis7r),f32,f32,,f34,f34,,f36,f36,,f38)
	STORE
	ba,pt			%xcc, vis6
	 add			%dst, 64, %dst
	.align			128
vis8:	FREG_FROB(f40,,f42,f42,,f44,f44,,f46,ftmp1,LOAD(f32,f46,vis8e,vis8r),f48,f48,,f50,f50,,f52,f52,,f54,f54,,f56)
	STORE
	add			%dst, 64, %dst
vis9:	FREG_FROB(f56,,f58,f58,,f60,f60,,f62,ftmp1,LOAD(f48,f62,vis9e,vis9r),f32,f32,,f34,f34,,f36,f36,,f38,f38,,f40)
	STORE
	ba,pt			%xcc, vis8
	 add			%dst, 64, %dst
	.align			128
vis10:	FREG_FROB(f42,,f44,f44,,f46,ftmp1,LOAD(f32,f46,vis10e,vis10r),f48,f48,,f50,f50,,f52,f52,,f54,f54,,f56,f56,,f58)
	STORE
	add			%dst, 64, %dst
vis11:	FREG_FROB(f58,,f60,f60,,f62,ftmp1,LOAD(f48,f62,vis11e,vis11r),f32,f32,,f34,f34,,f36,f36,,f38,f38,,f40,f40,,f42)
	STORE
	ba,pt			%xcc, vis10
	 add			%dst, 64, %dst
	.align			128
vis12:	FREG_FROB(f44,,f46,ftmp1,LOAD(f32,f46,vis12e,vis12r),f48,f48,,f50,f50,,f52,f52,,f54,f54,,f56,f56,,f58,f58,,f60)
	STORE
	add			%dst, 64, %dst
vis13:	FREG_FROB(f60,,f62,ftmp1,LOAD(f48,f62,vis13e,vis13r),f32,f32,,f34,f34,,f36,f36,,f38,f38,,f40,f40,,f42,f42,,f44)
	STORE
	ba,pt			%xcc, vis12
	 add			%dst, 64, %dst
	.align			128
vis14:	FREG_FROB(ftmp1,LOAD(f32,f46,vis14e,vis14r),f48,f48,,f50,f50,,f52,f52,,f54,f54,,f56,f56,,f58,f58,,f60,f60,,f62)
	STORE
	add			%dst, 64, %dst
vis15:	FREG_FROB(ftmp1,LOAD(f48,f62,vis15e,vis15r),f32,f32,,f34,f34,,f36,f36,,f38,f38,,f40,f40,,f42,f42,,f44,f44,,f46)
	STORE
	ba,pt			%xcc, vis14
	 add			%dst, 64, %dst

leftst:	brlz,pn			%curw, narrowst + 4
	 and			%leftw, 0x38, %tmp1
	andcc			%leftw, 7, %g0
	be,pn			%icc, 1f
	 neg			%tmp1
	sub			%dst, %leftw, %tmp4
	add			%branchbase, (2f - BranchBase - 8), %tmp2
	edge8			%tmp4, %g0, %tmp5
	jmpl			%tmp2 + %tmp1, %g0
	 andn			%tmp4, 7, %tmp4
1:	addcc			%branchbase, (3f - BranchBase), %tmp2
	sra			%tmp1, 1, %tmp1
	jmpl			%tmp2 + %tmp1, %g0
	 nop
	ba,pt			%xcc, 2f+0x00
	 stda			%f16, [%tmp4 + %tmp5] ASI_PST8_P
	ba,pt			%xcc, 2f+0x04
	 stda			%f18, [%tmp4 + %tmp5] ASI_PST8_P
	ba,pt			%xcc, 2f+0x08
	 stda			%f20, [%tmp4 + %tmp5] ASI_PST8_P
	ba,pt			%xcc, 2f+0x0c
	 stda			%f22, [%tmp4 + %tmp5] ASI_PST8_P
	ba,pt			%xcc, 2f+0x10
	 stda			%f24, [%tmp4 + %tmp5] ASI_PST8_P
	ba,pt			%xcc, 2f+0x14
	 stda			%f26, [%tmp4 + %tmp5] ASI_PST8_P
	ba,pt			%xcc, 2f+0x18
	 stda			%f28, [%tmp4 + %tmp5] ASI_PST8_P
	jmpl			%branchbase + %tmp3, %g0
	 stda			%f30, [%tmp4 + %tmp5] ASI_PST8_P
2:	std			%f18, [%dst - 0x38]
	std			%f20, [%dst - 0x30]
	std			%f22, [%dst - 0x28]
	std			%f24, [%dst - 0x20]
	std			%f26, [%dst - 0x18]
	std			%f28, [%dst - 0x10]
	std			%f30, [%dst - 0x08]
3:	jmpl			%branchbase + %tmp3, %g0
	 nop

rightst:cmp			%rightw, 64
	be,pn			%icc, 2f
	 and			%rightw, 0x38, %tmp1
	andcc			%rightw, 7, %g0
	be,pn			%icc, 1f
	 neg			%tmp1
	sub			%g0, %rightw, %tmp5
	add			%branchbase, (3f - BranchBase - 8), %tmp2
	edge8l			%tmp5, %g0, %tmp5
	add			%mode, %skind, %mode
	jmpl			%tmp1 + %tmp2, %g0
	 sub			%dst, %tmp1, %tmp4
1:	addcc			%branchbase, (4f - BranchBase), %tmp2
	sra			%tmp1, 1, %tmp1
	jmpl			%tmp1 + %tmp2, %g0
2:	 add			%mode, %skind, %mode
	ba,pt			%xcc, 4f
	 stda			%f16, [%dst] ASI_BLK_P
	ba,pt			%xcc, 3f+0x00
	 stda			%f30, [%tmp4 + %tmp5] ASI_PST8_P
	ba,pt			%xcc, 3f+0x04
	 stda			%f28, [%tmp4 + %tmp5] ASI_PST8_P
	ba,pt			%xcc, 3f+0x08
	 stda			%f26, [%tmp4 + %tmp5] ASI_PST8_P
	ba,pt			%xcc, 3f+0x0c
	 stda			%f24, [%tmp4 + %tmp5] ASI_PST8_P
	ba,pt			%xcc, 3f+0x10
	 stda			%f22, [%tmp4 + %tmp5] ASI_PST8_P
	ba,pt			%xcc, 3f+0x14
	 stda			%f20, [%tmp4 + %tmp5] ASI_PST8_P
	ba,pt			%xcc, 3f+0x18
	 stda			%f18, [%tmp4 + %tmp5] ASI_PST8_P
	ba,pt			%xcc, 3f+0x1c
	 stda			%f16, [%tmp4 + %tmp5] ASI_PST8_P
3:	std			%f28, [%dst + 0x30]
	std			%f26, [%dst + 0x28]
	std			%f24, [%dst + 0x20]
	std			%f22, [%dst + 0x18]
	std			%f20, [%dst + 0x10]
	std			%f18, [%dst + 0x08]
	std			%f16, [%dst + 0x00]
4:	brnz,pn			%narrow, narrow_cont
	 deccc			%srcnotdone
	blu,pn			%ICC, return
	 sub			%mode, %dkind, %mode
	alignaddr		%mode, %g0, %g0
	and			%mode, 63, %mode
	cmp			%srcnext, %srcn
	bne,a,pn		%ICC, 6f
	 xor			%fregset, 64, %fregset
	brnz,a,pn		%fregset, 5f
	 ldda			[%src] ASI_BLK_P, %f32
	ldda			[%src] ASI_BLK_P, %f48
5:	add			%src, 64, %src
6:	add			%dst, %dkind, %dst
	sub			%w, %rightw, %tmp1
	add			%srcstart, %skind, %srcstart
	sub			%dst, %tmp1, %dst
	add			%rightw, %dkind, %rightw
	andn			%dst, 63, %dst
	sub			%rightw, 1, %rightw
	sub			%leftw, %dkind, %leftw
	and			%rightw, 63, %rightw
	and			%leftw, 63, %leftw
	add			%rightw, 1, %rightw
roll_wide:
	add			%srcstart, %w, %srcend
	add			%srcstart, %skind, %srcnext
	fnop
	add			%srcend, 63, %srcend
	andncc			%srcnext, 63, %srcnext
	fnop
	sll			%mode, 4, %tmp1
	andn			%srcend, 63, %srcend
	fnop
	mov			%srcnext, %srcn
	and			%mode, 7, %rightw2
	fnop
	andn			%tmp1, 127, %tmp1
	add			%rightw2, %rightw, %rightw2
	fnop
	add			%tmp1, %fregset, %tmp1
	subcc			%w, %leftw, %tmp3
	fnop
	srl			%tmp1, 1, %tmp2
	add			%srcnext, 64, %tmp4
	fnop
	add			%tmp2, (prepn0 - BranchBase), %tmp2
	cmp			%mode, %leftw
	movgeu			%icc, %tmp2, %tmp1
	cmp			%tmp4, %srcend
	add			%dst, 64, %tmp4
	move			%ICC, %srcend, %srcnext
	movrnz			%leftw, %tmp4, %dst
	jmpl			%branchbase + %tmp1, %g0
	 sub			%tmp3, %rightw, %curw

return: return			%i7+8
	 wr			%g0, 0, %fprs

narrowst:
	and			%leftw, 0x38, %tmp1
	sub			%dst, %leftw, %tmp5
	sub			%w, 1, %fregset
	andcc			%leftw, 7, %tmp4
	add			%fregset, %tmp5, %fregset
	neg			%tmp1
	and			%fregset, 7, %srcend
	be,pn			%icc, 1f
	 edge8			%g0, %srcend, %srcend
	add			%tmp1, (2f - BranchBase - 8), %tmp1
	edge8			%tmp5, %fregset, %tmp2
	andn			%tmp5, 7, %tmp5
	andn			%fregset, 7, %fregset
	jmpl			%branchbase + %tmp1, %g0
	 sub			%w, %tmp4, %tmp4
1:	addcc			%branchbase, (3f - BranchBase - 8), %tmp2
	sll			%tmp1, 1, %tmp1
	andn			%fregset, 7, %fregset
	jmpl			%tmp2 + %tmp1, %g0
	 mov			%w, %tmp4
	ba,pt			%xcc, 2f+0x00
	 stda			%f16, [%tmp5 + %tmp2] ASI_PST8_P
	ba,pt			%xcc, 2f+0x10
	 stda			%f18, [%tmp5 + %tmp2] ASI_PST8_P
	ba,pt			%xcc, 2f+0x20
	 stda			%f20, [%tmp5 + %tmp2] ASI_PST8_P
	ba,pt			%xcc, 2f+0x30
	 stda			%f22, [%tmp5 + %tmp2] ASI_PST8_P
	ba,pt			%xcc, 2f+0x40
	 stda			%f24, [%tmp5 + %tmp2] ASI_PST8_P
	ba,pt			%xcc, 2f+0x50
	 stda			%f26, [%tmp5 + %tmp2] ASI_PST8_P
	ba,pt			%xcc, 2f+0x60
	 stda			%f28, [%tmp5 + %tmp2] ASI_PST8_P
	ba,pt			%xcc, 4f
	 stda			%f30, [%tmp5 + %tmp2] ASI_PST8_P
2:	subcc			%tmp4, 8, %tmp4
	bl,pn			%icc, 3f+0x00
	 fnop
	std			%f18, [%dst - 0x38]
	subcc			%tmp4, 8, %tmp4
	bl,pn			%icc, 3f+0x10
	 fnop
	std			%f20, [%dst - 0x30]
	subcc			%tmp4, 8, %tmp4
	bl,pn			%icc, 3f+0x20
	 fnop
	std			%f22, [%dst - 0x28]
	subcc			%tmp4, 8, %tmp4
	bl,pn			%icc, 3f+0x30
	 fnop
	std			%f24, [%dst - 0x20]
	subcc			%tmp4, 8, %tmp4
	bl,pn			%icc, 3f+0x40
	 fnop
	std			%f26, [%dst - 0x18]
	subcc			%tmp4, 8, %tmp4
	bl,pn			%icc, 3f+0x50
	 fnop
	std			%f28, [%dst - 0x10]
	subcc			%tmp4, 8, %tmp4
	bl,pn			%icc, 3f+0x60
	 fnop
	std			%f30, [%dst - 0x08]
	ba,pt			%xcc, 4f
	 nop
3:	cmp			%tmp4, -8
	bg,a,pn			%ICC, 4f
	 stda			%f18, [%fregset + %srcend] ASI_PST8_P
	ba,a,pt			%xcc, 4f
	cmp			%tmp4, -8
	bg,a,pn			%ICC, 4f
	 stda			%f20, [%fregset + %srcend] ASI_PST8_P
	ba,a,pt			%xcc, 4f
	cmp			%tmp4, -8
	bg,a,pn			%ICC, 4f
	 stda			%f22, [%fregset + %srcend] ASI_PST8_P
	ba,a,pt			%xcc, 4f
	cmp			%tmp4, -8
	bg,a,pn			%ICC, 4f
	 stda			%f24, [%fregset + %srcend] ASI_PST8_P
	ba,a,pt			%xcc, 4f
	cmp			%tmp4, -8
	bg,a,pn			%ICC, 4f
	 stda			%f26, [%fregset + %srcend] ASI_PST8_P
	ba,a,pt			%xcc, 4f
	cmp			%tmp4, -8
	bg,a,pn			%ICC, 4f
	 stda			%f28, [%fregset + %srcend] ASI_PST8_P
	ba,a,pt			%xcc, 4f
	cmp			%tmp4, -8
	bg,a,pn			%ICC, 4f
	 stda			%f30, [%fregset + %srcend] ASI_PST8_P
4:	and			%tmp3, 60, %tmp3
	sub			%dst, 64, %dst
	add			%tmp3, 96, %tmp3
	add			%mode, %skind, %mode
	brz,pn			%h, return
	 and			%tmp3, 64, %fregset
narrow_cont:
	deccc			%h
	blu,pn			%ICC, return
	 sub			%mode, %dkind, %mode
	alignaddr		%mode, %g0, %g0
	and			%mode, 63, %mode
	sub			%srcnext, %srcn, %tmp4
	add			%dst, %dkind, %dst
	sub			%w, %rightw, %tmp1
	add			%srcstart, %skind, %srcstart
	sub			%dst, %tmp1, %dst
	add			%rightw, %dkind, %rightw
	sub			%leftw, %dkind, %leftw
	sub			%rightw, 1, %rightw
	and			%leftw, 63, %leftw
	and			%rightw, 63, %rightw
	add			%srcstart, %w, %srcend
	add			%rightw, 1, %rightw
	add			%srcstart, %skind, %srcnext
	add			%srcend, 63, %srcend
	andn			%srcnext, 63, %srcnext
	andn			%srcend, 63, %srcend
	sub			%w, %leftw, %prepw
	mov			%srcnext, %srcn
	add			%srcnext, 64, %tmp2
	sub			%srcnext, 64, %tmp3
	and			%srcstart, 63, %tmp1
	cmp			%skind, 0
	movg			%ICC, %srcend, %tmp3
	sub			%prepw, %rightw, %prepw
	add			%tmp1, %w, %tmp1
	cmp			%tmp2, %srcend
	mov			%prepw, %curw
	move			%ICC, %tmp3, %srcnext
	mov			%h, %srcnotdone
	cmp			%tmp1, 128
	movg			%ICC, 64, %prepw
	andn			%dst, 63, %dst
	cmp			%srcnext, %srcstop
	move			%ICC, 0, %srcnotdone
	brnz,a,pn		%tmp4, roll_narrow
	 xor			%fregset, 64, %fregset
	cmp			%tmp1, 64
	bg,pt			%ICC, 5f
	 tst			%srcnotdone
	be,a,pn			%ICC, roll_narrow
	 membar			SYNC
	mov			%srcnext, %src
5:	brnz,a,pn		%fregset, 6f
	 ldda			[%src] ASI_BLK_P, %f32
	ldda			[%src] ASI_BLK_P, %f48
6:	addcc			%src, 64, %src
roll_narrow:
	fnop
	sll			%mode, 4, %tmp2
	and			%mode, 7, %rightw2
	fnop
	andn			%tmp2, 127, %tmp2
	add			%dst, 64, %tmp4
	fnop
	add			%tmp2, %fregset, %tmp2
	addcc			%rightw2, %rightw, %rightw2
	fnop
	srl			%tmp2, 1, %tmp3
	movrnz			%leftw, %tmp4, %dst
	fnop
	add			%tmp3, (prepr0 - BranchBase), %tmp4
	cmp			%tmp1, 64
	movleu			%icc, %tmp4, %tmp2
	fnop
	add			%tmp3, (prepn0 - BranchBase), %tmp4
	cmp			%mode, %leftw
	fnop
	movgeu			%icc, %tmp4, %tmp2
	jmpl			%branchbase + %tmp2, %g0
	 and			%tmp3, 32, %tmp3

#define VISEND(i,tgt,tgtr,fi,fset)				\
tgt:								\
	brnz,pt			%curw, 1f;			\
	 cmp			%rightw2, (64 - (i&14)*4);	\
	bgu,pn			%icc, tgtr - 8;			\
	 tst			%srcnotdone;			\
	be,a,pn			%ICC, tgtr;			\
	 membar			SYNC;				\
	ba,pt			%xcc, tgtr - 8;			\
	 mov			%srcnext, %src;			\
1:	mov			((i & 1) ^ 1) * 64, %fregset;	\
	bleu,pn			%icc, rightst;			\
	 faligndata		%ftmp1, %ftmp2, %fi;		\
	brz,a,pn		%srcnotdone, 3f;		\
	 membar			SYNC;				\
	ldda			[%srcnext] ASI_BLK_P, %fset;	\
	add			%srcnext, 64, %src;		\
3:	mov			(i & 1) * 64, %fregset;

VISEND(0,vis0e,vis0r,f30,f32)
	ba,pt			%xcc, rightst
	 faligndata		%ftmp1, %f48, %f30
VISEND(1,vis1e,vis1r,f30,f48)
	ba,pt			%xcc, rightst
	 faligndata		%ftmp1, %f32, %f30
VISEND(2,vis2e,vis2r,f28,f32)
	faligndata		%ftmp1, %f48, %f28
	ba,pt			%xcc, rightst
	 faligndata		%f48, %f50, %f30
VISEND(3,vis3e,vis3r,f28,f48)
	faligndata		%ftmp1, %f32, %f28
	ba,pt			%xcc, rightst
	 faligndata		%f32, %f34, %f30
VISEND(4,vis4e,vis4r,f26,f32)
	faligndata		%ftmp1, %f48, %f26
	faligndata		%f48, %f50, %f28
	ba,pt			%xcc, rightst
	 faligndata		%f50, %f52, %f30
VISEND(5,vis5e,vis5r,f26,f48)
	faligndata		%ftmp1, %f32, %f26
	faligndata		%f32, %f34, %f28
	ba,pt			%xcc, rightst
	 faligndata		%f34, %f36, %f30
VISEND(6,vis6e,vis6r,f24,f32)
	faligndata		%ftmp1, %f48, %f24
	faligndata		%f48, %f50, %f26
	faligndata		%f50, %f52, %f28
	ba,pt			%xcc, rightst
	 faligndata		%f52, %f54, %f30
VISEND(7,vis7e,vis7r,f24,f48)
	faligndata		%ftmp1, %f32, %f24
	faligndata		%f32, %f34, %f26
	faligndata		%f34, %f36, %f28
	ba,pt			%xcc, rightst
	 faligndata		%f36, %f38, %f30
VISEND(8,vis8e,vis8r,f22,f32)
	faligndata		%ftmp1, %f48, %f22
	faligndata		%f48, %f50, %f24
	faligndata		%f50, %f52, %f26
	faligndata		%f52, %f54, %f28
	ba,pt			%xcc, rightst
	 faligndata		%f54, %f56, %f30
VISEND(9,vis9e,vis9r,f22,f48)
	faligndata		%ftmp1, %f32, %f22
	faligndata		%f32, %f34, %f24
	faligndata		%f34, %f36, %f26
	faligndata		%f36, %f38, %f28
	ba,pt			%xcc, rightst
	 faligndata		%f38, %f40, %f30
VISEND(10,vis10e,vis10r,f20,f32)
	faligndata		%ftmp1, %f48, %f20
	faligndata		%f48, %f50, %f22
	faligndata		%f50, %f52, %f24
	faligndata		%f52, %f54, %f26
	faligndata		%f54, %f56, %f28
	ba,pt			%xcc, rightst
	 faligndata		%f56, %f58, %f30
VISEND(11,vis11e,vis11r,f20,f48)
	faligndata		%ftmp1, %f32, %f20
	faligndata		%f32, %f34, %f22
	faligndata		%f34, %f36, %f24
	faligndata		%f36, %f38, %f26
	faligndata		%f38, %f40, %f28
	ba,pt			%xcc, rightst
	 faligndata		%f40, %f42, %f30
VISEND(12,vis12e,vis12r,f18,f32)
	faligndata		%ftmp1, %f48, %f18
	faligndata		%f48, %f50, %f20
	faligndata		%f50, %f52, %f22
	faligndata		%f52, %f54, %f24
	faligndata		%f54, %f56, %f26
	faligndata		%f56, %f58, %f28
	ba,pt			%xcc, rightst
	 faligndata		%f58, %f60, %f30
VISEND(13,vis13e,vis13r,f18,f48)
	faligndata		%ftmp1, %f32, %f18
	faligndata		%f32, %f34, %f20
	faligndata		%f34, %f36, %f22
	faligndata		%f36, %f38, %f24
	faligndata		%f38, %f40, %f26
	faligndata		%f40, %f42, %f28
	ba,pt			%xcc, rightst
	 faligndata		%f42, %f44, %f30
VISEND(14,vis14e,vis14r,f16,f32)
	faligndata		%ftmp1, %f48, %f16
	faligndata		%f48, %f50, %f18
	faligndata		%f50, %f52, %f20
	faligndata		%f52, %f54, %f22
	faligndata		%f54, %f56, %f24
	faligndata		%f56, %f58, %f26
	faligndata		%f58, %f60, %f28
	ba,pt			%xcc, rightst
	 faligndata		%f60, %f62, %f30
VISEND(15,vis15e,vis15r,f16,f48)
	faligndata		%ftmp1, %f32, %f16
	faligndata		%f32, %f34, %f18
	faligndata		%f34, %f36, %f20
	faligndata		%f36, %f38, %f22
	faligndata		%f38, %f40, %f24
	faligndata		%f40, %f42, %f26
	faligndata		%f42, %f44, %f28
	ba,pt			%xcc, rightst
	 faligndata		%f44, %f46, %f30

#define PREPEND(tgt,tgtr)					\
tgt:								\
	brnz,pt			%srcnotdone, tgtr - 8;		\
	 mov			%srcnext, %src;			\
	ba,pt			%xcc, tgtr;			\
	 membar			SYNC;

	.align			16
PREPEND(prep0e,prep0r)
PREPEND(prep1e,prep1r)
PREPEND(prep2e,prep2r)
PREPEND(prep3e,prep3r)
PREPEND(prep4e,prep4r)
PREPEND(prep5e,prep5r)
PREPEND(prep6e,prep6r)
PREPEND(prep7e,prep7r)
PREPEND(prep8e,prep8r)
PREPEND(prep9e,prep9r)
PREPEND(prep10e,prep10r)
PREPEND(prep11e,prep11r)
PREPEND(prep12e,prep12r)
PREPEND(prep13e,prep13r)
PREPEND(prep14e,prep14r)
PREPEND(prep15e,prep15r)

/* The other way is even more complicated, as the hardware prefers
 * going from lower addresses up.  */

#define RLOAD(f32,tgt,tgtr)							\
	subcc			%curw, 64, %curw;				\
	bleu,pn			%ICC, tgt;					\
	 fmovd			%f32, %ftmp1;					\
	ldda			[%src] ASI_BLK_P, %f32;				\
	sub			%src, 64, %src;					\
tgtr:

#if defined(__sparc_v9__) || defined(__sparcv9) || defined(__arch64__) || !defined(SIGNAL_CLOBBERS_OG_REGS)

#define RPREPLOAD(f32,tgt,tgtr)							\
	brlez,pn		%prepw, tgt;					\
	 fmovd			%f32, %ftmp1;					\
	ldda			[%src] ASI_BLK_P, %f32;				\
	sub			%src, 64, %src;					\
tgtr:

#else

#define RPREPLOAD(f32,tgt,tgtr)							\
	tst			%prepw;						\
	ble,pn			%ICC, tgt;					\
	 fmovd			%f32, %ftmp1;					\
	ldda			[%src] ASI_BLK_P, %f32;				\
	sub			%src, 64, %src;					\
tgtr:

#endif

	.globl			VISmoveImageRL
	.align			32
VISmoveImageRL:
	save			%sp, -160, %sp				! Group 0
0:	rd			%pc, %tmp3				! Group 1
	and			%dst, 63, %leftw			! Group 7
	mov			64, %tmp1
	add			%src, %w, %src				! Group 8
	add			%dst, %w, %dst
	sub			%src, %dst, %mode			! Group 9
	brz,pn			%h, rreturn
	 sub			%dst, 1, %rightw			! Group 10
	mov			%src, %srcstart
	dec			%src					! Group 11
	cmp			%w, 128
	blu,pn			%ICC, rprepare_narrow
	 and			%mode, 63, %mode			! Group 12
	andn			%src, 63, %src
	ldda			[%src] ASI_BLK_P, %f32			! Group 13
	sub			%src, 64, %src				! Group 14
	clr			%narrow
	ldda			[%src] ASI_BLK_P, %f48			! Group 15
	sub			%tmp1, %leftw, %leftw			! Group 16
	and			%rightw, 63, %rightw
	dec			%dst					! Group 17
	add			%rightw, 1, %rightw
	alignaddr		%mode, %g0, %g0				! Group 18
	andn			%dst, 63, %dst				! Group 19
	clr			%fregset
	sub			%src, 64, %src				! Group 20
	add			%tmp3, (BranchBaseR - 0b), %branchbase
	mov			64, %prepw				! Group 21
	ba,pt			%xcc, rroll_wide
	 sub			%h, 1, %srcnotdone
rprepare_narrow:
#if defined(__sparc_v9__) || defined(__sparcv9) || defined(__arch64__)
	brlez,pn		%w, rreturn
#else
	tst			%w
	ble,pn			%icc, rreturn
#endif
	 andn			%src, 63, %src				! Group 12
	sub			%tmp1, %leftw, %leftw
	ldda			[%src] ASI_BLK_P, %f32			! Group 13
	and			%rightw, 63, %rightw			! Group 14
	dec			%dst
	add			%rightw, 1, %rightw			! Group 15
	andn			%dst, 63, %dst
	sub			%w, %leftw, %tmp2			! Group 16
	sub			%srcstart, 1, %tmp1
	subcc			%tmp2, %rightw, %curw			! Group 17
	and			%tmp1, 63, %tmp1
	mov			%curw, %prepw				! Group 18
	sub			%tmp1, %w, %tmp1
	bg,pt			%ICC, 1f
	 sub			%src, 64, %src				! Group 19
	cmp			%tmp1, -65
	movl			%ICC, 64, %prepw			! Group 20
1:	add			%srcstart, %skind, %srcnext
	sub			%h, 1, %srcnotdone			! Group 21
	sub			%srcstart, %w, %tmp4
	sub			%srcnext, 1, %srcnext			! Group 22
	andn			%tmp4, 63, %srcend
	andn			%srcnext, 63, %srcnext			! Group 23
	cmp			%skind, 0
	add			%w, 64, %tmp2				! Group 24
	mov			%srcnext, %srcn
	bl,pn			%ICC, 1f
	 clr			%srcstop				! Group 25
	cmp			%skind, %tmp2
	bgeu,pt			%ICC, 3f
	 add			%srcnext, 64, %fregset
	SMUL			%skind, %srcnotdone, %srcstop
	ba,pt			%xcc, 2f
	 add			%srcstart, 63, %tmp4
1:	addcc			%tmp2, %skind, %g0
	ble,pt			%ICC, 3f
	 sub			%srcnext, 64, %fregset
	SMUL			%skind, %srcnotdone, %srcstop
	sub			%tmp4, 64, %tmp4
2:	add			%srcstop, %tmp4, %srcstop
3:	cmp			%srcnext, %srcend
	andn			%srcstop, 63, %srcstop
	move			%ICC, %fregset, %srcnext
	cmp			%srcnext, %srcstop
	clr			%fregset
	move			%ICC, 0, %srcnotdone
	alignaddr		%mode, %g0, %g0
	add			%tmp3, (BranchBaseR - 0b), %branchbase
	mov			1, %narrow
	cmp			%tmp1, -1
	bge,pn			%ICC, 1f
	 dec			%h
	ldda			[%src] ASI_BLK_P, %f48
	ba,pt			%xcc, rroll_narrow
	 subcc			%src, 64, %src
1:	brz,a,pn		%srcnotdone, rroll_narrow
	 membar			#Sync
	ldda			[%srcnext] ASI_BLK_P, %f48
	ba,pt			%xcc, rroll_narrow
	 subcc			%srcnext, 64, %src

	.align			32
rrepr0:	ba,pt			%xcc, narrowstr
	 faligndata		%ftmp1, %f32, %f30
	nop
	nop
1:	ba,pt			%xcc, narrowstr
	 faligndata		%ftmp1, %f32, %f16
	.align			32
rrepr1:	ba,pt			%xcc, narrowstr
	 faligndata		%ftmp1, %f48, %f30
	nop
	nop
2:	ba,pt			%xcc, narrowstr
	 faligndata		%ftmp1, %f48, %f16
	.align			32
rrepr2:	faligndata		%f32, %f34, %f30
	ba,pt			%xcc, narrowstr
	 faligndata		%ftmp1, %f32, %f28
	.align			32
rrepr3:	faligndata		%f48, %f50, %f30
	ba,pt			%xcc, narrowstr
	 faligndata		%ftmp1, %f48, %f28
	.align			32
rrepr4:	faligndata		%f34, %f36, %f30
	faligndata		%f32, %f34, %f28
	ba,pt			%xcc, narrowstr
	 faligndata		%ftmp1, %f32, %f26
	.align			32
rrepr5: faligndata		%f50, %f52, %f30
	faligndata		%f48, %f50, %f28
	ba,pt			%xcc, narrowstr
	 faligndata		%ftmp1, %f48, %f26
	.align			32
rrepr6:	faligndata		%f36, %f38, %f30
	faligndata		%f34, %f36, %f28
	faligndata		%f32, %f34, %f26
	ba,pt			%xcc, narrowstr
	 faligndata		%ftmp1, %f32, %f24
	.align			32
rrepr7: faligndata		%f52, %f54, %f30
	faligndata		%f50, %f52, %f28
	faligndata		%f48, %f50, %f26
	ba,pt			%xcc, narrowstr
	 faligndata		%ftmp1, %f48, %f24
	.align			32
rrepr8:	faligndata		%f38, %f40, %f30
	faligndata		%f36, %f38, %f28
	faligndata		%f34, %f36, %f26
	faligndata		%f32, %f34, %f24
	ba,pt			%xcc, narrowstr
	 faligndata		%ftmp1, %f32, %f22
	.align			32
rrepr9: faligndata		%f54, %f56, %f30
	faligndata		%f52, %f54, %f28
	faligndata		%f50, %f52, %f26
	faligndata		%f48, %f50, %f24
	ba,pt			%xcc, narrowstr
	 faligndata		%ftmp1, %f48, %f22
	.align			32
rrepr10:faligndata		%f40, %f42, %f30
	faligndata		%f38, %f40, %f28
	faligndata		%f36, %f38, %f26
	faligndata		%f34, %f36, %f24
	faligndata		%f32, %f34, %f22
	ba,pt			%xcc, narrowstr
	 faligndata		%ftmp1, %f32, %f20
	.align			32
rrepr11:faligndata		%f56, %f58, %f30
	faligndata		%f54, %f56, %f28
	faligndata		%f52, %f54, %f26
	faligndata		%f50, %f52, %f24
	faligndata		%f48, %f50, %f22
	ba,pt			%xcc, narrowstr
	 faligndata		%ftmp1, %f48, %f20
	.align			32
rrepr12:faligndata		%f42, %f44, %f30
	faligndata		%f40, %f42, %f28
	faligndata		%f38, %f40, %f26
	faligndata		%f36, %f38, %f24
	faligndata		%f34, %f36, %f22
	faligndata		%f32, %f34, %f20
	ba,pt			%xcc, narrowstr
	 faligndata		%ftmp1, %f32, %f18
	.align			32
rrepr13:faligndata		%f58, %f60, %f30
	faligndata		%f56, %f58, %f28
	faligndata		%f54, %f56, %f26
	faligndata		%f52, %f54, %f24
	faligndata		%f50, %f52, %f22
	faligndata		%f48, %f50, %f20
	ba,pt			%xcc, narrowstr
	 faligndata		%ftmp1, %f48, %f18
	.align			32
rrepr14:faligndata		%f44, %f46, %f30
	faligndata		%f42, %f44, %f28
	faligndata		%f40, %f42, %f26
	faligndata		%f38, %f40, %f24
	faligndata		%f36, %f38, %f22
	faligndata		%f34, %f36, %f20
	ba,pt			%xcc, 1b
	 faligndata		%f32, %f34, %f18
	.align			32
rrepr15:faligndata		%f60, %f62, %f30
	faligndata		%f58, %f60, %f28
	faligndata		%f56, %f58, %f26
	faligndata		%f54, %f56, %f24
	faligndata		%f52, %f54, %f22
	faligndata		%f50, %f52, %f20
	ba,pt			%xcc, 2b
	 faligndata		%f48, %f50, %f18

	.align			32
rrepn0:	faligndata		%f46, %ftmp1, %f30
	faligndata		%f44, %f46, %f28
	faligndata		%f42, %f44, %f26
	faligndata		%f40, %f42, %f24
	faligndata		%f38, %f40, %f22
	faligndata		%f36, %f38, %f20
	ba,pt			%xcc, 1f
	 faligndata		%f34, %f36, %f18
	.align			32
rrepn1:	faligndata		%f62, %ftmp1, %f30
	faligndata		%f60, %f62, %f28
	faligndata		%f58, %f60, %f26
	faligndata		%f56, %f58, %f24
	faligndata		%f54, %f56, %f22
	faligndata		%f52, %f54, %f20
	ba,pt			%xcc, 2f
	 faligndata		%f50, %f52, %f18
	.align			32
rrepn2:	faligndata		%f46, %ftmp1, %f28
	faligndata		%f44, %f46, %f26
	faligndata		%f42, %f44, %f24
	faligndata		%f40, %f42, %f22
	faligndata		%f38, %f40, %f20
	faligndata		%f36, %f38, %f18
	ba,pt			%xcc, 3f
	 faligndata		%f34, %f36, %f16
	.align			32
rrepn3:	faligndata		%f62, %ftmp1, %f28
	faligndata		%f60, %f62, %f26
	faligndata		%f58, %f60, %f24
	faligndata		%f56, %f58, %f22
	faligndata		%f54, %f56, %f20
	faligndata		%f52, %f54, %f18
	ba,pt			%xcc, 4f
	 faligndata		%f50, %f52, %f16
	.align			32
rrepn4:	faligndata		%f46, %ftmp1, %f26
	faligndata		%f44, %f46, %f24
	faligndata		%f42, %f44, %f22
	faligndata		%f40, %f42, %f20
	faligndata		%f38, %f40, %f18
	faligndata		%f36, %f38, %f16
	ba,pt			%xcc, rightstr
	 mov			(rvis4 - BranchBaseR), %tmp3
	.align			32
rrepn5:	faligndata		%f62, %ftmp1, %f26
	faligndata		%f60, %f62, %f24
	faligndata		%f58, %f60, %f22
	faligndata		%f56, %f58, %f20
	faligndata		%f54, %f56, %f18
	faligndata		%f52, %f54, %f16
	ba,pt			%xcc, rightstr
	 mov			(rvis5 - BranchBaseR), %tmp3
	.align			32
rrepn6:	faligndata		%f46, %ftmp1, %f24
	faligndata		%f44, %f46, %f22
	faligndata		%f42, %f44, %f20
	faligndata		%f40, %f42, %f18
	faligndata		%f38, %f40, %f16
	ba,pt			%xcc, rightstr
	 mov			(rvis6 - BranchBaseR), %tmp3
	.align			32
rrepn7:	faligndata		%f62, %ftmp1, %f24
	faligndata		%f60, %f62, %f22
	faligndata		%f58, %f60, %f20
	faligndata		%f56, %f58, %f18
	faligndata		%f54, %f56, %f16
	ba,pt			%xcc, rightstr
	 mov			(rvis7 - BranchBaseR), %tmp3
	.align			32
rrepn8:	faligndata		%f46, %ftmp1, %f22
	faligndata		%f44, %f46, %f20
	faligndata		%f42, %f44, %f18
	faligndata		%f40, %f42, %f16
	ba,pt			%xcc, rightstr
	 mov			(rvis8 - BranchBaseR), %tmp3
	.align			32
rrepn9:	faligndata		%f62, %ftmp1, %f22
	faligndata		%f60, %f62, %f20
	faligndata		%f58, %f60, %f18
	faligndata		%f56, %f58, %f16
	ba,pt			%xcc, rightstr
	 mov			(rvis9 - BranchBaseR), %tmp3
	.align			32
rrepn10:faligndata		%f46, %ftmp1, %f20
	faligndata		%f44, %f46, %f18
	faligndata		%f42, %f44, %f16
	ba,pt			%xcc, rightstr
	 mov			(rvis10 - BranchBaseR), %tmp3
	.align			32
rrepn11:faligndata		%f62, %ftmp1, %f20
	faligndata		%f60, %f62, %f18
	faligndata		%f58, %f60, %f16
	ba,pt			%xcc, rightstr
	 mov			(rvis11 - BranchBaseR), %tmp3
	.align			32
rrepn12:faligndata		%f46, %ftmp1, %f18
	faligndata		%f44, %f46, %f16
	ba,pt			%xcc, rightstr
	 mov			(rvis12 - BranchBaseR), %tmp3
	nop
	nop
3:	ba,pt			%xcc, rightstr
	 mov			(rvis2 - BranchBaseR), %tmp3
	.align			32
rrepn13:faligndata		%f62, %ftmp1, %f18
	faligndata		%f60, %f62, %f16
	ba,pt			%xcc, rightstr
	 mov			(rvis13 - BranchBaseR), %tmp3
	nop
	nop
4:	ba,pt			%xcc, rightstr
	 mov			(rvis3 - BranchBaseR), %tmp3
	.align			32
rrepn14:faligndata		%f46, %ftmp1, %f16
	ba,pt			%xcc, rightstr
	 mov			(rvis14 - BranchBaseR), %tmp3
	nop
1:	faligndata		%f32, %f34, %f16
	ba,pt			%xcc, rightstr
	 mov			(rvis0 - BranchBaseR), %tmp3
	.align			32
rrepn15:faligndata		%f62, %ftmp1, %f16
	ba,pt			%xcc, rightstr
	 mov			(rvis15 - BranchBaseR), %tmp3
	nop
2:	faligndata		%f48, %f50, %f16
	ba,pt			%xcc, rightstr
	 mov			(rvis1 - BranchBaseR), %tmp3

	.align			64
BranchBaseR:
rrepl0:	RPREPLOAD(f32,rrep0e,rrep0r)
	fnop
	faligndata		%f48, %f50, %f16
	faligndata		%f50, %f52, %f18
	faligndata		%f52, %f54, %f20
	faligndata		%f54, %f56, %f22
	faligndata		%f56, %f58, %f24
	faligndata		%f58, %f60, %f26
	faligndata		%f60, %f62, %f28
	faligndata		%f62, %ftmp1, %f30
	ba,pt			%xcc, rightstr
	 mov			(rvis1 - BranchBaseR), %tmp3
	nop
	nop
	.align			32
rrepl1:	RPREPLOAD(f48,rrep1e,rrep1r)
	fnop
	faligndata		%f32, %f34, %f16
	faligndata		%f34, %f36, %f18
	faligndata		%f36, %f38, %f20
	faligndata		%f38, %f40, %f22
	faligndata		%f40, %f42, %f24
	faligndata		%f42, %f44, %f26
	faligndata		%f44, %f46, %f28
	faligndata		%f46, %ftmp1, %f30
	ba,pt			%xcc, rightstr
	 mov			(rvis0 - BranchBaseR), %tmp3
	nop
	nop
	.align			32
rrepl2:	faligndata		%f32, %f34, %f30
	RPREPLOAD(f32,rrep2e,rrep2r)
	fnop
	fnop
	faligndata		%f50, %f52, %f16
	faligndata		%f52, %f54, %f18
	faligndata		%f54, %f56, %f20
	faligndata		%f56, %f58, %f22
	faligndata		%f58, %f60, %f24
	faligndata		%f60, %f62, %f26
	faligndata		%f62, %ftmp1, %f28
	ba,pt			%xcc, rightstr
	 mov			(rvis3 - BranchBaseR), %tmp3
	nop
	.align			32
rrepl3:	faligndata		%f48, %f50, %f30
	RPREPLOAD(f48,rrep3e,rrep3r)
	fnop
	fnop
	faligndata		%f34, %f36, %f16
	faligndata		%f36, %f38, %f18
	faligndata		%f38, %f40, %f20
	faligndata		%f40, %f42, %f22
	faligndata		%f42, %f44, %f24
	faligndata		%f44, %f46, %f26
	faligndata		%f46, %ftmp1, %f28
	ba,pt			%xcc, rightstr
	 mov			(rvis2 - BranchBaseR), %tmp3
	nop
	.align			32
rrepl4:	faligndata		%f32, %f34, %f28
	faligndata		%f34, %f36, %f30
	RPREPLOAD(f32,rrep4e,rrep4r)
	fnop
	fnop
	fnop
	faligndata		%f52, %f54, %f16
	faligndata		%f54, %f56, %f18
	faligndata		%f56, %f58, %f20
	faligndata		%f58, %f60, %f22
	faligndata		%f60, %f62, %f24
	faligndata		%f62, %ftmp1, %f26
	ba,pt			%xcc, rightstr
	 mov			(rvis5 - BranchBaseR), %tmp3
	.align			32
rrepl5:	faligndata		%f48, %f50, %f28
	faligndata		%f50, %f52, %f30
	RPREPLOAD(f48,rrep5e,rrep5r)
	fnop
	fnop
	fnop
	faligndata		%f36, %f38, %f16
	faligndata		%f38, %f40, %f18
	faligndata		%f40, %f42, %f20
	faligndata		%f42, %f44, %f22
	faligndata		%f44, %f46, %f24
	faligndata		%f46, %ftmp1, %f26
	ba,pt			%xcc, rightstr
	 mov			(rvis4 - BranchBaseR), %tmp3
	.align			32
rrepl6:	faligndata		%f32, %f34, %f26
	faligndata		%f34, %f36, %f28
	faligndata		%f36, %f38, %f30
	RPREPLOAD(f32,rrep6e,rrep6r)
	fnop
	fnop
	fnop
	fnop
	faligndata		%f54, %f56, %f16
	faligndata		%f56, %f58, %f18
	faligndata		%f58, %f60, %f20
	faligndata		%f60, %f62, %f22
	faligndata		%f62, %ftmp1, %f24
	ba,pt			%xcc, rightstr
	 mov			(rvis7 - BranchBaseR), %tmp3
	.align			32
rrepl7:	faligndata		%f48, %f50, %f26
	faligndata		%f50, %f52, %f28
	faligndata		%f52, %f54, %f30
	RPREPLOAD(f48,rrep7e,rrep7r)
	fnop
	fnop
	fnop
	fnop
	faligndata		%f38, %f40, %f16
	faligndata		%f40, %f42, %f18
	faligndata		%f42, %f44, %f20
	faligndata		%f44, %f46, %f22
	faligndata		%f46, %ftmp1, %f24
	ba,pt			%xcc, rightstr
	 mov			(rvis6 - BranchBaseR), %tmp3
	.align			32
rrepl8:	faligndata		%f32, %f34, %f24
	faligndata		%f34, %f36, %f26
	faligndata		%f36, %f38, %f28
	faligndata		%f38, %f40, %f30
	RPREPLOAD(f32,rrep8e,rrep8r)
	fnop
	fnop
	fnop
	fnop
	fnop
	faligndata		%f56, %f58, %f16
	faligndata		%f58, %f60, %f18
	faligndata		%f60, %f62, %f20
	faligndata		%f62, %ftmp1, %f22
	ba,pt			%xcc, rightstr
	 mov			(rvis9 - BranchBaseR), %tmp3
	.align			32
rrepl9:	faligndata		%f48, %f50, %f24
	faligndata		%f50, %f52, %f26
	faligndata		%f52, %f54, %f28
	faligndata		%f54, %f56, %f30
	RPREPLOAD(f48,rrep9e,rrep9r)
	fnop
	fnop
	fnop
	fnop
	fnop
	faligndata		%f40, %f42, %f16
	faligndata		%f42, %f44, %f18
	faligndata		%f44, %f46, %f20
	faligndata		%f46, %ftmp1, %f22
	ba,pt			%xcc, rightstr
	 mov			(rvis8 - BranchBaseR), %tmp3
	.align			32
rrepl10:faligndata		%f32, %f34, %f22
	faligndata		%f34, %f36, %f24
	faligndata		%f36, %f38, %f26
	faligndata		%f38, %f40, %f28
	faligndata		%f40, %f42, %f30
	RPREPLOAD(f32,rrep10e,rrep10r)
	fnop
	fnop
	fnop
	fnop
	fnop
	fnop
	faligndata		%f58, %f60, %f16
	faligndata		%f60, %f62, %f18
	faligndata		%f62, %ftmp1, %f20
	ba,pt			%xcc, rightstr
	 mov			(rvis11 - BranchBaseR), %tmp3
	.align			32
rrepl11:faligndata		%f48, %f50, %f22
	faligndata		%f50, %f52, %f24
	faligndata		%f52, %f54, %f26
	faligndata		%f54, %f56, %f28
	faligndata		%f56, %f58, %f30
	RPREPLOAD(f48,rrep11e,rrep11r)
	fnop
	fnop
	fnop
	fnop
	fnop
	fnop
	faligndata		%f42, %f44, %f16
	faligndata		%f44, %f46, %f18
	faligndata		%f46, %ftmp1, %f20
	ba,pt			%xcc, rightstr
	 mov			(rvis10 - BranchBaseR), %tmp3
	.align			32
rrepl12:faligndata		%f32, %f34, %f20
	faligndata		%f34, %f36, %f22
	faligndata		%f36, %f38, %f24
	faligndata		%f38, %f40, %f26
	faligndata		%f40, %f42, %f28
	faligndata		%f42, %f44, %f30
	RPREPLOAD(f32,rrep12e,rrep12r)
	fnop
	fnop
	fnop
	fnop
	fnop
	fnop
	fnop
	faligndata		%f60, %f62, %f16
	faligndata		%f62, %ftmp1, %f18
	ba,pt			%xcc, rightstr
	 mov			(rvis13 - BranchBaseR), %tmp3
	.align			32
rrepl13:faligndata		%f48, %f50, %f20
	faligndata		%f50, %f52, %f22
	faligndata		%f52, %f54, %f24
	faligndata		%f54, %f56, %f26
	faligndata		%f56, %f58, %f28
	faligndata		%f58, %f60, %f30
	RPREPLOAD(f48,rrep13e,rrep13r)
	fnop
	fnop
	fnop
	fnop
	fnop
	fnop
	fnop
	faligndata		%f44, %f46, %f16
	faligndata		%f46, %ftmp1, %f18
	ba,pt			%xcc, rightstr
	 mov			(rvis12 - BranchBaseR), %tmp3
	.align			32
rrepl14:faligndata		%f32, %f34, %f18
	faligndata		%f34, %f36, %f20
	faligndata		%f36, %f38, %f22
	faligndata		%f38, %f40, %f24
	faligndata		%f40, %f42, %f26
	faligndata		%f42, %f44, %f28
	faligndata		%f44, %f46, %f30
	RPREPLOAD(f32,rrep14e,rrep14r)
	fnop
	fnop
	fnop
	fnop
	fnop
	fnop
	fnop
	faligndata		%f62, %ftmp1, %f16
	ba,pt			%xcc, rightstr
	 mov			(rvis15 - BranchBaseR), %tmp3
	.align			32
rrepl15:faligndata		%f48, %f50, %f18
	faligndata		%f50, %f52, %f20
	faligndata		%f52, %f54, %f22
	faligndata		%f54, %f56, %f24
	faligndata		%f56, %f58, %f26
	faligndata		%f58, %f60, %f28
	faligndata		%f60, %f62, %f30
	RPREPLOAD(f48,rrep15e,rrep15r)
	fnop
	fnop
	fnop
	fnop
	fnop
	fnop
	fnop
	faligndata		%f46, %ftmp1, %f16
	ba,pt			%xcc, rightstr
	 mov			(rvis14 - BranchBaseR), %tmp3

	.align			64
rvis0:	nop
	RLOAD(f32,rvis0e,rvis0r)
	fnop
	faligndata		%f48, %f50, %f16
	faligndata		%f50, %f52, %f18
	faligndata		%f52, %f54, %f20
	faligndata		%f54, %f56, %f22
	faligndata		%f56, %f58, %f24
	faligndata		%f58, %f60, %f26
	faligndata		%f60, %f62, %f28
	faligndata		%f62, %ftmp1, %f30
	STORE
	sub			%dst, 64, %dst
rvis1:	RLOAD(f48,rvis1e,rvis1r)
	fnop
	faligndata		%f32, %f34, %f16
	faligndata		%f34, %f36, %f18
	faligndata		%f36, %f38, %f20
	faligndata		%f38, %f40, %f22
	faligndata		%f40, %f42, %f24
	faligndata		%f42, %f44, %f26
	faligndata		%f44, %f46, %f28
	faligndata		%f46, %ftmp1, %f30
	STORE
	ba,pt			%xcc, rvis0+0x04
	 sub			%dst, 64, %dst
	.align			64
rvis2:	faligndata		%f32, %f34, %ftmp8
	RLOAD(f32,rvis2e,rvis2r)
	fnop
	fnop
	faligndata		%f50, %f52, %f16
	faligndata		%f52, %f54, %f18
	faligndata		%f54, %f56, %f20
	faligndata		%f56, %f58, %f22
	faligndata		%f58, %f60, %f24
	faligndata		%f60, %f62, %f26
	faligndata		%f62, %ftmp1, %f28
	fmovd			%ftmp8, %f30
	STORE
	sub			%dst, 64, %dst
rvis3:	faligndata		%f48, %f50, %ftmp8
	RLOAD(f48,rvis3e,rvis3r)
	fnop
	fnop
	faligndata		%f34, %f36, %f16
	faligndata		%f36, %f38, %f18
	faligndata		%f38, %f40, %f20
	faligndata		%f40, %f42, %f22
	faligndata		%f42, %f44, %f24
	faligndata		%f44, %f46, %f26
	faligndata		%f46, %ftmp1, %f28
	fmovd			%ftmp8, %f30
	STORE
	ba,pt			%xcc, rvis2
	 sub			%dst, 64, %dst
	.align			64
rvis4:	faligndata		%f32, %f34, %ftmp7
	faligndata		%f34, %f36, %ftmp8
	RLOAD(f32,rvis4e,rvis4r)
	fnop
	fnop
	fnop
	faligndata		%f52, %f54, %f16
	faligndata		%f54, %f56, %f18
	faligndata		%f56, %f58, %f20
	faligndata		%f58, %f60, %f22
	faligndata		%f60, %f62, %f24
	faligndata		%f62, %ftmp1, %f26
	fmovd			%ftmp7, %f28
	fmovd			%ftmp8, %f30
	STORE
	sub			%dst, 64, %dst
rvis5:	faligndata		%f48, %f50, %ftmp7
	faligndata		%f50, %f52, %ftmp8
	RLOAD(f48,rvis5e,rvis5r)
	fnop
	fnop
	fnop
	faligndata		%f36, %f38, %f16
	faligndata		%f38, %f40, %f18
	faligndata		%f40, %f42, %f20
	faligndata		%f42, %f44, %f22
	faligndata		%f44, %f46, %f24
	faligndata		%f46, %ftmp1, %f26
	fmovd			%ftmp7, %f28
	fmovd			%ftmp8, %f30
	STORE
	ba,pt			%xcc, rvis4
	 sub			%dst, 64, %dst
	.align			64
rvis6:	faligndata		%f32, %f34, %ftmp6
	faligndata		%f34, %f36, %ftmp7
	faligndata		%f36, %f38, %ftmp8
	RLOAD(f32,rvis6e,rvis6r)
	fnop
	fmovd			%ftmp6, %f26
	fmovd			%ftmp7, %f28
	fmovd			%ftmp8, %f30
	faligndata		%f54, %f56, %f16
	faligndata		%f56, %f58, %f18
	faligndata		%f58, %f60, %f20
	faligndata		%f60, %f62, %f22
	faligndata		%f62, %ftmp1, %f24
	STORE
	sub			%dst, 64, %dst
rvis7:	faligndata		%f48, %f50, %ftmp6
	faligndata		%f50, %f52, %ftmp7
	faligndata		%f52, %f54, %ftmp8
	RLOAD(f48,rvis7e,rvis7r)
	fnop
	fmovd			%ftmp6, %f26
	fmovd			%ftmp7, %f28
	fmovd			%ftmp8, %f30
	faligndata		%f38, %f40, %f16
	faligndata		%f40, %f42, %f18
	faligndata		%f42, %f44, %f20
	faligndata		%f44, %f46, %f22
	faligndata		%f46, %ftmp1, %f24
	STORE
	ba,pt			%xcc, rvis6
	 sub			%dst, 64, %dst
	.align			64
rvis8:	faligndata		%f32, %f34, %ftmp5
	faligndata		%f34, %f36, %ftmp6
	faligndata		%f36, %f38, %ftmp7
	faligndata		%f38, %f40, %ftmp8
	RLOAD(f32,rvis8e,rvis8r)
	fnop
	fmovd			%ftmp5, %f24
	fmovd			%ftmp6, %f26
	fmovd			%ftmp7, %f28
	fmovd			%ftmp8, %f30
	faligndata		%f56, %f58, %f16
	faligndata		%f58, %f60, %f18
	faligndata		%f60, %f62, %f20
	faligndata		%f62, %ftmp1, %f22
	STORE
	sub			%dst, 64, %dst
rvis9:	faligndata		%f48, %f50, %ftmp5
	faligndata		%f50, %f52, %ftmp6
	faligndata		%f52, %f54, %ftmp7
	faligndata		%f54, %f56, %ftmp8
	RLOAD(f48,rvis9e,rvis9r)
	fnop
	fmovd			%ftmp5, %f24
	fmovd			%ftmp6, %f26
	fmovd			%ftmp7, %f28
	fmovd			%ftmp8, %f30
	faligndata		%f40, %f42, %f16
	faligndata		%f42, %f44, %f18
	faligndata		%f44, %f46, %f20
	faligndata		%f46, %ftmp1, %f22
	STORE
	ba,pt			%xcc, rvis8
	 sub			%dst, 64, %dst
	.align			64
rvis10:	faligndata		%f32, %f34, %ftmp4
	faligndata		%f34, %f36, %ftmp5
	faligndata		%f36, %f38, %ftmp6
	faligndata		%f38, %f40, %ftmp7
	faligndata		%f40, %f42, %ftmp8
	RLOAD(f32,rvis10e,rvis10r)
	fnop
	fmovd			%ftmp4, %f22
	fmovd			%ftmp5, %f24
	fmovd			%ftmp6, %f26
	fmovd			%ftmp7, %f28
	fmovd			%ftmp8, %f30
	faligndata		%f58, %f60, %f16
	faligndata		%f60, %f62, %f18
	faligndata		%f62, %ftmp1, %f20
	STORE
	sub			%dst, 64, %dst
rvis11:	faligndata		%f48, %f50, %ftmp4
	faligndata		%f50, %f52, %ftmp5
	faligndata		%f52, %f54, %ftmp6
	faligndata		%f54, %f56, %ftmp7
	faligndata		%f56, %f58, %ftmp8
	RLOAD(f48,rvis11e,rvis11r)
	fnop
	fmovd			%ftmp4, %f22
	fmovd			%ftmp5, %f24
	fmovd			%ftmp6, %f26
	fmovd			%ftmp7, %f28
	fmovd			%ftmp8, %f30
	faligndata		%f42, %f44, %f16
	faligndata		%f44, %f46, %f18
	faligndata		%f46, %ftmp1, %f20
	STORE
	ba,pt			%xcc, rvis10
	 sub			%dst, 64, %dst
	.align			64
rvis12:	faligndata		%f32, %f34, %ftmp3
	faligndata		%f34, %f36, %ftmp4
	faligndata		%f36, %f38, %ftmp5
	faligndata		%f38, %f40, %ftmp6
	faligndata		%f40, %f42, %ftmp7
	faligndata		%f42, %f44, %ftmp8
	RLOAD(f32,rvis12e,rvis12r)
	fnop
	fmovd			%ftmp3, %f20
	fmovd			%ftmp4, %f22
	fmovd			%ftmp5, %f24
	fmovd			%ftmp6, %f26
	fmovd			%ftmp7, %f28
	fmovd			%ftmp8, %f30
	faligndata		%f60, %f62, %f16
	faligndata		%f62, %ftmp1, %f18
	STORE
	sub			%dst, 64, %dst
rvis13:	faligndata		%f48, %f50, %ftmp3
	faligndata		%f50, %f52, %ftmp4
	faligndata		%f52, %f54, %ftmp5
	faligndata		%f54, %f56, %ftmp6
	faligndata		%f56, %f58, %ftmp7
	faligndata		%f58, %f60, %ftmp8
	RLOAD(f48,rvis13e,rvis13r)
	fnop
	fmovd			%ftmp3, %f20
	fmovd			%ftmp4, %f22
	fmovd			%ftmp5, %f24
	fmovd			%ftmp6, %f26
	fmovd			%ftmp7, %f28
	fmovd			%ftmp8, %f30
	faligndata		%f44, %f46, %f16
	faligndata		%f46, %ftmp1, %f18
	STORE
	ba,pt			%xcc, rvis12
	 sub			%dst, 64, %dst
	.align			64
rvis14:	fnop
	faligndata		%f32, %f34, %f18
	faligndata		%f34, %f36, %f20
	faligndata		%f36, %f38, %f22
	faligndata		%f38, %f40, %f24
	faligndata		%f40, %f42, %f26
	faligndata		%f42, %f44, %f28
	faligndata		%f44, %f46, %f30
	RLOAD(f32,rvis14e,rvis14r)
	fnop
	fnop
	fnop
	fnop
	fnop
	fnop
	fnop
	faligndata		%f62, %ftmp1, %f16
	STORE
	sub			%dst, 64, %dst
rvis15:	fnop
	faligndata		%f48, %f50, %f18
	faligndata		%f50, %f52, %f20
	faligndata		%f52, %f54, %f22
	faligndata		%f54, %f56, %f24
	faligndata		%f56, %f58, %f26
	faligndata		%f58, %f60, %f28
	faligndata		%f60, %f62, %f30
	RLOAD(f48,rvis15e,rvis15r)
	fnop
	fnop
	fnop
	fnop
	fnop
	fnop
	fnop
	faligndata		%f46, %ftmp1, %f16
	STORE
	ba,pt			%xcc, rvis14
	 sub			%dst, 64, %dst

rightstr:
	brlz,pn			%curw, narrowstr + 4
	 cmp			%rightw, 64
	be,pn			%icc, 2f
	 and			%rightw, 0x38, %tmp1
	andcc			%rightw, 7, %g0
	be,pn			%icc, 1f
	 neg			%tmp1
	sub			%g0, %rightw, %tmp5
	add			%branchbase, (3f - BranchBaseR - 8), %tmp2
	edge8l			%tmp5, %g0, %tmp5
	sub			%dst, %tmp1, %tmp4
	jmpl			%tmp1 + %tmp2, %g0
	 add			%tmp4, 0x40, %tmp4
1:	addcc			%branchbase, (4f - BranchBaseR), %tmp2
	sra			%tmp1, 1, %tmp1
	jmpl			%tmp1 + %tmp2, %g0
2:	 add			%dst, 0x40, %tmp1
	jmpl			%branchbase + %tmp3, %g0
	 stda			%f16, [%tmp1] ASI_BLK_P
	ba,pt			%xcc, 3f+0x00
	 stda			%f30, [%tmp4 + %tmp5] ASI_PST8_P
	ba,pt			%xcc, 3f+0x04
	 stda			%f28, [%tmp4 + %tmp5] ASI_PST8_P
	ba,pt			%xcc, 3f+0x08
	 stda			%f26, [%tmp4 + %tmp5] ASI_PST8_P
	ba,pt			%xcc, 3f+0x0c
	 stda			%f24, [%tmp4 + %tmp5] ASI_PST8_P
	ba,pt			%xcc, 3f+0x10
	 stda			%f22, [%tmp4 + %tmp5] ASI_PST8_P
	ba,pt			%xcc, 3f+0x14
	 stda			%f20, [%tmp4 + %tmp5] ASI_PST8_P
	ba,pt			%xcc, 3f+0x18
	 stda			%f18, [%tmp4 + %tmp5] ASI_PST8_P
	jmpl			%branchbase + %tmp3, %g0
	 stda			%f16, [%tmp4 + %tmp5] ASI_PST8_P
3:	std			%f28, [%dst + 0x70]
	std			%f26, [%dst + 0x68]
	std			%f24, [%dst + 0x60]
	std			%f22, [%dst + 0x58]
	std			%f20, [%dst + 0x50]
	std			%f18, [%dst + 0x48]
	std			%f16, [%dst + 0x40]
4:	jmpl			%branchbase + %tmp3, %g0
	 nop

leftstr:cmp			%leftw, 64
	be,pn			%icc, 2f
	 and			%leftw, 0x38, %tmp1
	andcc			%leftw, 7, %g0
	be,pn			%icc, 1f
	 neg			%tmp1
	sub			%dst, %leftw, %tmp4
	add			%branchbase, (3f - BranchBaseR - 8), %tmp2
	edge8			%tmp4, %g0, %tmp5
	andn			%tmp4, 7, %tmp4
	add			%mode, %skind, %mode
	jmpl			%tmp2 + %tmp1, %g0
	 add			%tmp4, 0x40, %tmp4
1:	add			%branchbase, (4f - BranchBaseR), %tmp2
	sra			%tmp1, 1, %tmp1
	jmpl			%tmp2 + %tmp1, %g0
2:	 add			%mode, %skind, %mode
	ba,pt			%xcc, 4f
	 stda			%f16, [%dst] ASI_BLK_P
	ba,pt			%xcc, 3f+0x00
	 stda			%f16, [%tmp4 + %tmp5] ASI_PST8_P
	ba,pt			%xcc, 3f+0x04
	 stda			%f18, [%tmp4 + %tmp5] ASI_PST8_P
	ba,pt			%xcc, 3f+0x08
	 stda			%f20, [%tmp4 + %tmp5] ASI_PST8_P
	ba,pt			%xcc, 3f+0x0c
	 stda			%f22, [%tmp4 + %tmp5] ASI_PST8_P
	ba,pt			%xcc, 3f+0x10
	 stda			%f24, [%tmp4 + %tmp5] ASI_PST8_P
	ba,pt			%xcc, 3f+0x14
	 stda			%f26, [%tmp4 + %tmp5] ASI_PST8_P
	ba,pt			%xcc, 3f+0x18
	 stda			%f28, [%tmp4 + %tmp5] ASI_PST8_P
	ba,pt			%xcc, 3f+0x1c
	 stda			%f30, [%tmp4 + %tmp5] ASI_PST8_P
3:	std			%f18, [%dst + 0x08]
	std			%f20, [%dst + 0x10]
	std			%f22, [%dst + 0x18]
	std			%f24, [%dst + 0x20]
	std			%f26, [%dst + 0x28]
	std			%f28, [%dst + 0x30]
	std			%f30, [%dst + 0x38]
4:	brnz,pn			%narrow, rnarrow_cont
	 deccc			%srcnotdone
	blu,pn			%ICC, rreturn
	 sub			%mode, %dkind, %mode
	alignaddr		%mode, %g0, %g0
	and			%mode, 63, %mode
	cmp			%srcnext, %srcn
	bne,a,pn		%ICC, 6f
	 xor			%fregset, 64, %fregset
	brnz,a,pn		%fregset, 5f
	 ldda			[%src] ASI_BLK_P, %f32
	ldda			[%src] ASI_BLK_P, %f48
5:	sub			%src, 64, %src
6:	add			%dst, %dkind, %dst
	sub			%w, %leftw, %tmp1
	add			%srcstart, %skind, %srcstart
	add			%dst, %tmp1, %dst
	add			%rightw, %dkind, %rightw
	add			%dst, 63, %dst
	sub			%rightw, 1, %rightw
	sub			%dkind, %leftw, %leftw
	and			%rightw, 63, %rightw
	and			%leftw, 63, %leftw
	add			%rightw, 1, %rightw
	mov			64, %tmp1
	andn			%dst, 63, %dst
	sub			%tmp1, %leftw, %leftw
rroll_wide:
	add			%srcstart, %skind, %srcnext
	sub			%srcstart, %w, %srcend
	fnop
	deccc			%srcnext
	sll			%mode, 4, %tmp4
	fnop
	andn			%srcnext, 63, %srcnext
	andn			%srcend, 63, %srcend
	fnop
	mov			%srcnext, %srcn
	and			%mode, 7, %leftw2
	fnop
	andn			%tmp4, 127, %tmp4
	sub			%leftw, %leftw2, %leftw2
	fnop
	add			%tmp4, %fregset, %tmp4
	subcc			%w, %leftw, %curw
	fnop
	srl			%tmp4, 1, %tmp3
	sub			%tmp1, %mode, %tmp1
	fnop
	add			%tmp3, (rrepn0 - BranchBaseR), %tmp2
	cmp			%tmp1, %rightw
	add			%tmp4, %tmp3, %tmp4
	sub			%srcnext, 64, %tmp1
	movgeu			%icc, %tmp2, %tmp4
	cmp			%srcnext, %srcend
	sub			%dst, 64, %dst
	move			%ICC, %tmp1, %srcnext
	jmpl			%branchbase + %tmp4, %g0
	 sub			%curw, %rightw, %curw

rreturn:return			%i7+8
	 wr			%g0, 0, %fprs

narrowstr:
	cmp			%rightw, 64
	add			%dst, 0x80, %fregset
	and			%rightw, 0x38, %tmp1
	sub			%fregset, %leftw, %tmp2
	mov			%w, %tmp4
	andn			%tmp2, 7, %fregset
	be,pn			%icc, 2f
	 edge8			%tmp2, %g0, %srcend
	andcc			%rightw, 7, %g0
	be,pn			%icc, 1f
	 neg			%tmp1
	sub			%g0, %rightw, %tmp4
	add			%tmp1, (2f - BranchBaseR - 8), %tmp1
	add			%dst, 0x40, %tmp5
	sub			%leftw, 0x41, %tmp2
	add			%tmp5, %rightw, %tmp5
	edge8l			%tmp4, %tmp2, %tmp2
	and			%rightw, 7, %tmp4
	andn			%tmp5, 7, %tmp5
	jmpl			%branchbase + %tmp1, %g0
	 sub			%w, %tmp4, %tmp4
1:	addcc			%branchbase, (4f - BranchBaseR - 8), %tmp2
	sll			%tmp1, 1, %tmp1
	jmpl			%tmp2 + %tmp1, %g0
	 nop
	ba,pt			%xcc, 3f+0x00
	 stda			%f30, [%tmp5 + %tmp2] ASI_PST8_P
	ba,pt			%xcc, 3f+0x10
	 stda			%f28, [%tmp5 + %tmp2] ASI_PST8_P
	ba,pt			%xcc, 3f+0x20
	 stda			%f26, [%tmp5 + %tmp2] ASI_PST8_P
	ba,pt			%xcc, 3f+0x30
	 stda			%f24, [%tmp5 + %tmp2] ASI_PST8_P
	ba,pt			%xcc, 3f+0x40
	 stda			%f22, [%tmp5 + %tmp2] ASI_PST8_P
	ba,pt			%xcc, 3f+0x50
	 stda			%f20, [%tmp5 + %tmp2] ASI_PST8_P
	ba,pt			%xcc, 3f+0x60
	 stda			%f18, [%tmp5 + %tmp2] ASI_PST8_P
	ba,pt			%xcc, 3f+0x70
	 stda			%f16, [%tmp5 + %tmp2] ASI_PST8_P
2:	subcc			%tmp4, 8, %tmp4
	bl,pn			%icc, 4f+0x00
	 fnop
	std			%f30, [%dst + 0x78]
3:	subcc			%tmp4, 8, %tmp4
	bl,pn			%icc, 4f+0x10
	 fnop
	std			%f28, [%dst + 0x70]
	subcc			%tmp4, 8, %tmp4
	bl,pn			%icc, 4f+0x20
	 fnop
	std			%f26, [%dst + 0x68]
	subcc			%tmp4, 8, %tmp4
	bl,pn			%icc, 4f+0x30
	 fnop
	std			%f24, [%dst + 0x60]
	subcc			%tmp4, 8, %tmp4
	bl,pn			%icc, 4f+0x40
	 fnop
	std			%f22, [%dst + 0x58]
	subcc			%tmp4, 8, %tmp4
	bl,pn			%icc, 4f+0x50
	 fnop
	std			%f20, [%dst + 0x50]
	subcc			%tmp4, 8, %tmp4
	bl,pn			%icc, 4f+0x60
	 fnop
	std			%f18, [%dst + 0x48]
	subcc			%tmp4, 8, %tmp4
	bl,pn			%icc, 4f+0x70
	 fnop
	std			%f16, [%dst + 0x40]
	ba,pt			%xcc, 5f
	 nop
4:	cmp			%tmp4, -8
	bg,a,pn			%ICC, 5f
	 stda			%f30, [%fregset + %srcend] ASI_PST8_P
	ba,a,pt			%xcc, 5f
	cmp			%tmp4, -8
	bg,a,pn			%ICC, 5f
	 stda			%f28, [%fregset + %srcend] ASI_PST8_P
	ba,a,pt			%xcc, 5f
	cmp			%tmp4, -8
	bg,a,pn			%ICC, 5f
	 stda			%f26, [%fregset + %srcend] ASI_PST8_P
	ba,a,pt			%xcc, 5f
	cmp			%tmp4, -8
	bg,a,pn			%ICC, 5f
	 stda			%f24, [%fregset + %srcend] ASI_PST8_P
	ba,a,pt			%xcc, 5f
	cmp			%tmp4, -8
	bg,a,pn			%ICC, 5f
	 stda			%f22, [%fregset + %srcend] ASI_PST8_P
	ba,a,pt			%xcc, 5f
	cmp			%tmp4, -8
	bg,a,pn			%ICC, 5f
	 stda			%f20, [%fregset + %srcend] ASI_PST8_P
	ba,a,pt			%xcc, 5f
	cmp			%tmp4, -8
	bg,a,pn			%ICC, 5f
	 stda			%f18, [%fregset + %srcend] ASI_PST8_P
	ba,a,pt			%xcc, 5f
	cmp			%tmp4, -8
	bg,a,pn			%ICC, 5f
	 stda			%f16, [%fregset + %srcend] ASI_PST8_P
5:	and			%tmp3, 60, %tmp3
	add			%dst, 64, %dst
	xor			%tmp3, 60, %tmp3
	add			%mode, %skind, %mode
	add			%tmp3, 4, %tmp3
	brz,pn			%h, rreturn
	 and			%tmp3, 64, %fregset
rnarrow_cont:
	deccc			%h
	blu,pn			%ICC, rreturn
	 sub			%mode, %dkind, %mode
	alignaddr		%mode, %g0, %g0
	and			%mode, 63, %mode
	sub			%srcnext, %srcn, %tmp4
	add			%dst, %dkind, %dst
	sub			%w, %leftw, %tmp1
	add			%srcstart, %skind, %srcstart
	add			%dst, %tmp1, %dst
	add			%rightw, %dkind, %rightw
	sub			%dkind, %leftw, %leftw
	sub			%rightw, 1, %rightw
	and			%leftw, 63, %leftw
	add			%dst, 63, %dst
	mov			64, %tmp1
	and			%rightw, 63, %rightw
	sub			%tmp1, %leftw, %leftw
	add			%srcstart, %skind, %srcnext
	add			%rightw, 1, %rightw
	sub			%srcstart, %w, %srcend
	dec			%srcnext
	andn			%srcend, 63, %srcend
	andn			%srcnext, 63, %srcnext
	sub			%w, %leftw, %prepw
	mov			%srcnext, %srcn
	sub			%srcnext, 64, %tmp3
	add			%srcnext, 64, %tmp2
	cmp			%skind, 0
	movg			%ICC, %tmp2, %tmp3
	mov			%h, %srcnotdone
	sub			%prepw, %rightw, %prepw
	sub			%srcstart, 1, %tmp1
	cmp			%srcnext, %srcend
	move			%ICC, %tmp3, %srcnext
	and			%tmp1, 63, %tmp1
	sub			%srcnext, 64, %tmp3
	sub			%tmp1, %w, %tmp1
	cmp			%srcnext, %srcend
	move			%ICC, %tmp3, %srcnext
	mov			%prepw, %curw
	cmp			%tmp1, -65
	movl			%ICC, 64, %prepw
	andn			%dst, 63, %dst
	cmp			%srcnext, %srcstop
	move			%ICC, 0, %srcnotdone
	brnz,a,pn		%tmp4, rroll_narrow
	 xor			%fregset, 64, %fregset
	cmp			%tmp1, -1
	bl,pt			%ICC, 6f
	 tst			%srcnotdone
	be,a,pn			%ICC, rroll_narrow
	 membar			SYNC
	mov			%srcnext, %src
6:	brnz,a,pn		%fregset, 7f
	 ldda			[%src] ASI_BLK_P, %f32
	ldda			[%src] ASI_BLK_P, %f48
7:	subcc			%src, 64, %src
rroll_narrow:
	fnop
	sll			%mode, 4, %tmp2
	and			%mode, 7, %leftw2
	fnop
	andn			%tmp2, 127, %tmp2
	sub			%dst, 64, %dst
	fnop
	add			%tmp2, %fregset, %tmp2
	subcc			%leftw, %leftw2, %leftw2
	fnop
	srl			%tmp2, 1, %tmp3
	add			%tmp3, (rrepr0 - BranchBaseR), %tmp4
	fnop
	add			%tmp2, %tmp3, %tmp2
	cmp			%tmp1, -1
	fnop
	mov			64, %tmp1
	movge			%icc, %tmp4, %tmp2
	sub			%tmp1, %mode, %tmp1
	add			%tmp3, (rrepn0 - BranchBaseR), %tmp4
	fnop
	cmp			%tmp1, %rightw
	movgeu			%icc, %tmp4, %tmp2
	jmpl			%branchbase + %tmp2, %g0
	 and			%tmp3, 32, %tmp3

#define RVISEND1(i,tgt,tgtr)					\
tgt:								\
	brnz,pt			%curw, 1f;			\
	 cmp			%leftw2, ((i&14)*4);		\
	bg,pn			%icc, tgtr - 8;			\
	 tst			%srcnotdone;			\
	be,a,pn			%ICC, tgtr;			\
	 membar			SYNC;				\
	ba,pt			%xcc, tgtr - 8;			\
	 mov			%srcnext, %src;			\
1:	mov			((i & 1) ^ 1) * 64, %fregset;

#define RVISEND2(i,fset)					\
	brz,a,pn		%srcnotdone, 3f;		\
	 membar			SYNC;				\
	ldda			[%srcnext] ASI_BLK_P, %fset;	\
	sub			%srcnext, 64, %src;		\
3:	mov			(i & 1) * 64, %fregset;

RVISEND1(0,rvis0e,rvis0r)
	fnop
	fnop
	fnop
	fnop
	fnop
	fnop
	ble,pn			%icc, leftstr
	 faligndata		%ftmp2, %ftmp1, %f30
RVISEND2(0,f32)
	fnop
	faligndata		%f48, %f50, %f16
	faligndata		%f50, %f52, %f18
	faligndata		%f52, %f54, %f20
	faligndata		%f54, %f56, %f22
	faligndata		%f56, %f58, %f24
	faligndata		%f58, %f60, %f26
	faligndata		%f60, %f62, %f28
	ba,pt			%xcc, leftstr
	 faligndata		%f62, %ftmp1, %f30
RVISEND1(1,rvis1e,rvis1r)
	fnop
	fnop
	fnop
	fnop
	fnop
	fnop
	ble,pn			%icc, leftstr
	 faligndata		%ftmp2, %ftmp1, %f30
RVISEND2(1,f48)
	fnop
	faligndata		%f32, %f34, %f16
	faligndata		%f34, %f36, %f18
	faligndata		%f36, %f38, %f20
	faligndata		%f38, %f40, %f22
	faligndata		%f40, %f42, %f24
	faligndata		%f42, %f44, %f26
	faligndata		%f44, %f46, %f28
	ba,pt			%xcc, leftstr
	 faligndata		%f46, %ftmp1, %f30
RVISEND1(2,rvis2e,rvis2r)
	fnop
	fnop
	fnop
	fnop
	faligndata		%ftmp2, %ftmp1, %f28
	ble,pn			%icc, leftstr
	 fmovd			%ftmp8, %f30
RVISEND2(2,f32)
	fnop
	fnop
	faligndata		%f50, %f52, %f16
	faligndata		%f52, %f54, %f18
	faligndata		%f54, %f56, %f20
	faligndata		%f56, %f58, %f22
	faligndata		%f58, %f60, %f24
	faligndata		%f60, %f62, %f26
	ba,pt			%xcc, leftstr
	 faligndata		%f62, %ftmp1, %f28
RVISEND1(3,rvis3e,rvis3r)
	fnop
	fnop
	fnop
	fnop
	faligndata		%ftmp2, %ftmp1, %f28
	ble,pn			%icc, leftstr
	 fmovd			%ftmp8, %f30
RVISEND2(3,f48)
	fnop
	fnop
	faligndata		%f34, %f36, %f16
	faligndata		%f36, %f38, %f18
	faligndata		%f38, %f40, %f20
	faligndata		%f40, %f42, %f22
	faligndata		%f42, %f44, %f24
	faligndata		%f44, %f46, %f26
	ba,pt			%xcc, leftstr
	 faligndata		%f46, %ftmp1, %f28
RVISEND1(4,rvis4e,rvis4r)
	fnop
	fnop
	faligndata		%ftmp2, %ftmp1, %f26
	fmovd			%ftmp7, %f28
	ble,pn			%icc, leftstr
	 fmovd			%ftmp8, %f30
RVISEND2(4,f32)
	fnop
	fnop
	fnop
	faligndata		%f52, %f54, %f16
	faligndata		%f54, %f56, %f18
	faligndata		%f56, %f58, %f20
	faligndata		%f58, %f60, %f22
	faligndata		%f60, %f62, %f24
	ba,pt			%xcc, leftstr
	 faligndata		%f62, %ftmp1, %f26
RVISEND1(5,rvis5e,rvis5r)
	fnop
	fnop
	faligndata		%ftmp2, %ftmp1, %f26
	fmovd			%ftmp7, %f28
	ble,pn			%icc, leftstr
	 fmovd			%ftmp8, %f30
RVISEND2(5,f48)
	fnop
	fnop
	fnop
	faligndata		%f36, %f38, %f16
	faligndata		%f38, %f40, %f18
	faligndata		%f40, %f42, %f20
	faligndata		%f42, %f44, %f22
	faligndata		%f44, %f46, %f24
	ba,pt			%xcc, leftstr
	 faligndata		%f46, %ftmp1, %f26
RVISEND1(6,rvis6e,rvis6r)
	faligndata		%ftmp2, %ftmp1, %f24
	fmovd			%ftmp6, %f26
	fmovd			%ftmp7, %f28
	ble,pn			%icc, leftstr
	 fmovd			%ftmp8, %f30
RVISEND2(6,f32)
	fnop
	fnop
	fnop
	fnop
	faligndata		%f54, %f56, %f16
	faligndata		%f56, %f58, %f18
	faligndata		%f58, %f60, %f20
	faligndata		%f60, %f62, %f22
	ba,pt			%xcc, leftstr
	 faligndata		%f62, %ftmp1, %f24
RVISEND1(7,rvis7e,rvis7r)
	faligndata		%ftmp2, %ftmp1, %f24
	fmovd			%ftmp6, %f26
	fmovd			%ftmp7, %f28
	ble,pn			%icc, leftstr
	 fmovd			%ftmp8, %f30
RVISEND2(7,f48)
	fnop
	fnop
	fnop
	fnop
	faligndata		%f38, %f40, %f16
	faligndata		%f40, %f42, %f18
	faligndata		%f42, %f44, %f20
	faligndata		%f44, %f46, %f22
	ba,pt			%xcc, leftstr
	 faligndata		%f46, %ftmp1, %f24
RVISEND1(8,rvis8e,rvis8r)
	faligndata		%ftmp2, %ftmp1, %f22
	fmovd			%ftmp5, %f24
	fmovd			%ftmp6, %f26
	fmovd			%ftmp7, %f28
	ble,pn			%icc, leftstr
	 fmovd			%ftmp8, %f30
RVISEND2(8,f32)
	fnop
	fnop
	fnop
	fnop
	fnop
	faligndata		%f56, %f58, %f16
	faligndata		%f58, %f60, %f18
	faligndata		%f60, %f62, %f20
	ba,pt			%xcc, leftstr
	 faligndata		%f62, %ftmp1, %f22
RVISEND1(9,rvis9e,rvis9r)
	faligndata		%ftmp2, %ftmp1, %f22
	fmovd			%ftmp5, %f24
	fmovd			%ftmp6, %f26
	fmovd			%ftmp7, %f28
	ble,pn			%icc, leftstr
	 fmovd			%ftmp8, %f30
RVISEND2(9,f48)
	fnop
	fnop
	fnop
	fnop
	fnop
	faligndata		%f40, %f42, %f16
	faligndata		%f42, %f44, %f18
	faligndata		%f44, %f46, %f20
	ba,pt			%xcc, leftstr
	 faligndata		%f46, %ftmp1, %f22
RVISEND1(10,rvis10e,rvis10r)
	faligndata		%ftmp2, %ftmp1, %f20
	fmovd			%ftmp4, %f22
	fmovd			%ftmp5, %f24
	fmovd			%ftmp6, %f26
	fmovd			%ftmp7, %f28
	ble,pn			%icc, leftstr
	 fmovd			%ftmp8, %f30
RVISEND2(10,f32)
	fnop
	fnop
	fnop
	fnop
	fnop
	fnop
	faligndata		%f58, %f60, %f16
	faligndata		%f60, %f62, %f18
	ba,pt			%xcc, leftstr
	 faligndata		%f62, %ftmp1, %f20
RVISEND1(11,rvis11e,rvis11r)
	faligndata		%ftmp2, %ftmp1, %f20
	fmovd			%ftmp4, %f22
	fmovd			%ftmp5, %f24
	fmovd			%ftmp6, %f26
	fmovd			%ftmp7, %f28
	ble,pn			%icc, leftstr
	 fmovd			%ftmp8, %f30
RVISEND2(11,f48)
	fnop
	fnop
	fnop
	fnop
	fnop
	fnop
	faligndata		%f42, %f44, %f16
	faligndata		%f44, %f46, %f18
	ba,pt			%xcc, leftstr
	 faligndata		%f46, %ftmp1, %f20
RVISEND1(12,rvis12e,rvis12r)
	faligndata		%ftmp2, %ftmp1, %f18
	fmovd			%ftmp3, %f20
	fmovd			%ftmp4, %f22
	fmovd			%ftmp5, %f24
	fmovd			%ftmp6, %f26
	fmovd			%ftmp7, %f28
	ble,pn			%icc, leftstr
	 fmovd			%ftmp8, %f30
RVISEND2(12,f32)
	fnop
	fnop
	fnop
	fnop
	fnop
	fnop
	fnop
	faligndata		%f60, %f62, %f16
	ba,pt			%xcc, leftstr
	 faligndata		%f62, %ftmp1, %f18
RVISEND1(13,rvis13e,rvis13r)
	faligndata		%ftmp2, %ftmp1, %f18
	fmovd			%ftmp3, %f20
	fmovd			%ftmp4, %f22
	fmovd			%ftmp5, %f24
	fmovd			%ftmp6, %f26
	fmovd			%ftmp7, %f28
	ble,pn			%icc, leftstr
	 fmovd			%ftmp8, %f30
RVISEND2(13,f48)
	fnop
	fnop
	fnop
	fnop
	fnop
	fnop
	fnop
	faligndata		%f44, %f46, %f16
	ba,pt			%xcc, leftstr
	 faligndata		%f46, %ftmp1, %f18
RVISEND1(14,rvis14e,rvis14r)
	ble,pn			%icc, leftstr
	 faligndata		%ftmp2, %ftmp1, %f16
RVISEND2(14,f32)
	fnop
	fnop
	fnop
	fnop
	fnop
	fnop
	fnop
	ba,pt			%xcc, leftstr
	 faligndata		%f62, %ftmp1, %f16
RVISEND1(15,rvis15e,rvis15r)
	ble,pn			%icc, leftstr
	 faligndata		%ftmp2, %ftmp1, %f16
RVISEND2(15,f48)
	fnop
	fnop
	fnop
	fnop
	fnop
	fnop
	fnop
	ba,pt			%xcc, leftstr
	 faligndata		%f46, %ftmp1, %f16

#define RREPEND(tgt,tgtr)					\
tgt:								\
	brnz,pt			%srcnotdone, tgtr - 8;		\
	 mov			%srcnext, %src;			\
	ba,pt			%xcc, tgtr;			\
	 membar			SYNC;

	.align			16
RREPEND(rrep0e,rrep0r)
RREPEND(rrep1e,rrep1r)
RREPEND(rrep2e,rrep2r)
RREPEND(rrep3e,rrep3r)
RREPEND(rrep4e,rrep4r)
RREPEND(rrep5e,rrep5r)
RREPEND(rrep6e,rrep6r)
RREPEND(rrep7e,rrep7r)
RREPEND(rrep8e,rrep8r)
RREPEND(rrep9e,rrep9r)
RREPEND(rrep10e,rrep10r)
RREPEND(rrep11e,rrep11r)
RREPEND(rrep12e,rrep12r)
RREPEND(rrep13e,rrep13r)
RREPEND(rrep14e,rrep14r)
RREPEND(rrep15e,rrep15r)
