/* $XFree86: xc/programs/Xserver/hw/xfree86/common/fourcc.h,v 1.6 2004/02/13 23:58:35 dawes Exp $ */

/*
 * Copyright (c) 2000-2003 by The XFree86 Project, Inc.
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
   This header file contains listings of STANDARD guids for video formats.
   Please do not place non-registered, or incomplete entries in this file.
   A list of some popular fourcc's are at: http://www.webartz.com/fourcc/
   For an explanation of fourcc <-> guid mappings see RFC2361.
*/

#ifndef _XF86_FOURCC_H_
#define _XF86_FOURCC_H_ 1

#define FOURCC_YUY2 0x32595559
#define XVIMAGE_YUY2 \
   { \
	FOURCC_YUY2, \
        XvYUV, \
	LSBFirst, \
	{'Y','U','Y','2', \
	  0x00,0x00,0x00,0x10,0x80,0x00,0x00,0xAA,0x00,0x38,0x9B,0x71}, \
	16, \
	XvPacked, \
	1, \
	0, 0, 0, 0, \
	8, 8, 8, \
	1, 2, 2, \
	1, 1, 1, \
	{'Y','U','Y','V', \
	  0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0}, \
	XvTopToBottom \
   }

#define FOURCC_YV12 0x32315659
#define XVIMAGE_YV12 \
   { \
	FOURCC_YV12, \
        XvYUV, \
	LSBFirst, \
	{'Y','V','1','2', \
	  0x00,0x00,0x00,0x10,0x80,0x00,0x00,0xAA,0x00,0x38,0x9B,0x71}, \
	12, \
	XvPlanar, \
	3, \
	0, 0, 0, 0, \
	8, 8, 8, \
	1, 2, 2, \
	1, 2, 2, \
	{'Y','V','U', \
	  0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0}, \
	XvTopToBottom \
   }

#define FOURCC_I420 0x30323449
#define XVIMAGE_I420 \
   { \
	FOURCC_I420, \
        XvYUV, \
	LSBFirst, \
	{'I','4','2','0', \
	  0x00,0x00,0x00,0x10,0x80,0x00,0x00,0xAA,0x00,0x38,0x9B,0x71}, \
	12, \
	XvPlanar, \
	3, \
	0, 0, 0, 0, \
	8, 8, 8, \
	1, 2, 2, \
	1, 2, 2, \
	{'Y','U','V', \
	  0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0}, \
	XvTopToBottom \
   }


#define FOURCC_UYVY 0x59565955
#define XVIMAGE_UYVY \
   { \
	FOURCC_UYVY, \
        XvYUV, \
	LSBFirst, \
	{'U','Y','V','Y', \
	  0x00,0x00,0x00,0x10,0x80,0x00,0x00,0xAA,0x00,0x38,0x9B,0x71}, \
	16, \
	XvPacked, \
	1, \
	0, 0, 0, 0, \
	8, 8, 8, \
	1, 2, 2, \
	1, 1, 1, \
	{'U','Y','V','Y', \
	  0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0}, \
	XvTopToBottom \
   }

#define FOURCC_IA44 0x34344149
#define XVIMAGE_IA44 \
   { \
        FOURCC_IA44, \
        XvYUV, \
        LSBFirst, \
        {'I','A','4','4', \
          0x00,0x00,0x00,0x10,0x80,0x00,0x00,0xAA,0x00,0x38,0x9B,0x71}, \
        8, \
        XvPacked, \
        1, \
        0, 0, 0, 0, \
        8, 8, 8, \
        1, 1, 1, \
        1, 1, 1, \
        {'A','I', \
          0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0}, \
        XvTopToBottom \
   }

#define FOURCC_AI44 0x34344941
#define XVIMAGE_AI44 \
   { \
        FOURCC_AI44, \
        XvYUV, \
        LSBFirst, \
        {'A','I','4','4', \
          0x00,0x00,0x00,0x10,0x80,0x00,0x00,0xAA,0x00,0x38,0x9B,0x71}, \
        8, \
        XvPacked, \
        1, \
        0, 0, 0, 0, \
        8, 8, 8, \
        1, 1, 1, \
        1, 1, 1, \
        {'I','A', \
          0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0}, \
        XvTopToBottom \
   }

#endif /* _XF86_FOURCC_H_ */
