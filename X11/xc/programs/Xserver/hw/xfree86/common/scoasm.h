/* $XFree86: xc/programs/Xserver/hw/xfree86/common/scoasm.h,v 3.2 2004/02/13 23:58:35 dawes Exp $ */

/*
 * Copyright (c) 1996 by The XFree86 Project, Inc.
 * All rights reserved.
 *
 * Permission is hereby granted, free of charge, to any person obtaining
 * a copy of this software and associated documentation files (the
 * "Software"), to deal in the Software without restriction, including
 * without limitation the rights to use, copy, modify, merge, publish,
 * distribute, sublicense, and/or sell copies of the Software, and to
 * permit persons to whom the Software is furnished to do so, subject
 * to the following conditions:
 *
 *   1.  Redistributions of source code must retain the above copyright
 *       notice, this list of conditions, and the following disclaimer.
 *
 *   2.  Redistributions in binary form must reproduce the above copyright
 *       notice, this list of conditions and the following disclaimer
 *       in the documentation and/or other materials provided with the
 *       distribution, and in the same place and form as other copyright,
 *       license and disclaimer information.
 *
 *   3.  The end-user documentation included with the redistribution,
 *       if any, must include the following acknowledgment: "This product
 *       includes software developed by The XFree86 Project, Inc
 *       (http://www.xfree86.org/) and its contributors", in the same
 *       place and form as other third-party acknowledgments.  Alternately,
 *       this acknowledgment may appear in the software itself, in the
 *       same form and location as other such third-party acknowledgments.
 *
 *   4.  Except as contained in this notice, the name of The XFree86
 *       Project, Inc shall not be used in advertising or otherwise to
 *       promote the sale, use or other dealings in this Software without
 *       prior written authorization from The XFree86 Project, Inc.
 *
 * THIS SOFTWARE IS PROVIDED ``AS IS'' AND ANY EXPRESSED OR IMPLIED
 * WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE XFREE86 PROJECT, INC OR ITS CONTRIBUTORS BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY,
 * OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT
 * OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR
 * BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
 * WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE
 * OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE,
 * EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

/*
 * scoasm.h - used to define inline versions of certain functions which
 * do NOT appear in sys/inline.h.
 */
#ifdef SCO325
#ifndef _SCOASM_HDR_INC
#define _SCOASM_HDR_INC

asm     void outl(port,val)
{
%reg	port,val;
	movl	port, %edx
	movl	val, %eax
	outl	(%dx)
%reg	port; mem	val;
	movl	port, %edx
	movl    val, %eax
	outl	(%dx)
%mem	port; reg	val;
	movw	port, %dx
	movl	val, %eax
	outl	(%dx)
%mem	port,val;
	movw	port, %dx
	movl    val, %eax
	outl	(%dx)
}

asm	void outw(port,val)
{
%reg	port,val;
	movl	port, %edx
	movl	val, %eax
	data16
	outl	(%dx)
%reg	port; mem	val;
	movl	port, %edx
	movw	val, %ax
	data16
	outl	(%dx)
%mem	port; reg	val;
	movw	port, %dx
	movl	val, %eax
	data16
	outl	(%dx)
%mem	port,val;
	movw	port, %dx
	movw	val, %ax
	data16
	outl	(%dx)
}

asm	void outb(port,val)
{
%reg	port,val;
	movl	port, %edx
	movl	val, %eax
	outb	(%dx)
%reg	port; mem	val;
	movl	port, %edx
	movb	val, %al
	outb	(%dx)
%mem	port; reg	val;
	movw	port, %dx
	movl	val, %eax
	outb	(%dx)
%mem	port,val;
	movw	port, %dx
	movb	val, %al
	outb	(%dx)
}

asm     int inl(port)
{
%reg	port;
	movl	port, %edx
	inl	(%dx)
%mem	port;
	movw	port, %dx
	inl	(%dx)
}

asm	int inw(port)
{
%reg	port;
	subl    %eax, %eax
	movl	port, %edx
	data16
	inl	(%dx)
%mem	port;
	subl    %eax, %eax
	movw	port, %dx
	data16
	inl	(%dx)
}

asm	int inb(port)
{
%reg	port;
	subl    %eax, %eax
	movl	port, %edx
	inb	(%dx)
%mem	port;
	subl    %eax, %eax
	movw	port, %dx
	inb	(%dx)
}

#endif /* _SCOASM_HDR_INC */
#endif /* SCO325 */
