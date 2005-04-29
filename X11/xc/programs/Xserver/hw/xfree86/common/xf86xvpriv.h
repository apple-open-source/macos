/* $XFree86: xc/programs/Xserver/hw/xfree86/common/xf86xvpriv.h,v 1.3 2004/02/13 23:58:40 dawes Exp $ */

/*
 * Copyright (c) 2003 by The XFree86 Project, Inc.
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

#ifndef _XF86XVPRIV_H_
#define _XF86XVPRIV_H_

#include "xf86xv.h"

/*** These are DDX layer privates ***/

extern int XF86XvScreenIndex;

typedef struct {
   DestroyWindowProcPtr		DestroyWindow;
   ClipNotifyProcPtr		ClipNotify;
   WindowExposuresProcPtr	WindowExposures;
   void                         (*AdjustFrame)(int, int, int, int);
   Bool                         (*EnterVT)(int, int);
   void                         (*LeaveVT)(int, int);
   GCPtr			videoGC;
} XF86XVScreenRec, *XF86XVScreenPtr;

typedef struct {
  int flags;  
  PutVideoFuncPtr PutVideo;
  PutStillFuncPtr PutStill;
  GetVideoFuncPtr GetVideo;
  GetStillFuncPtr GetStill;
  StopVideoFuncPtr StopVideo;
  SetPortAttributeFuncPtr SetPortAttribute;
  GetPortAttributeFuncPtr GetPortAttribute;
  QueryBestSizeFuncPtr QueryBestSize;
  PutImageFuncPtr PutImage;
  ReputImageFuncPtr ReputImage;
  QueryImageAttributesFuncPtr QueryImageAttributes;
} XvAdaptorRecPrivate, *XvAdaptorRecPrivatePtr;

typedef struct {
   ScrnInfoPtr pScrn;
   DrawablePtr pDraw;
   unsigned char type;
   unsigned int subWindowMode;
   DDXPointRec clipOrg;
   RegionPtr clientClip;
   RegionPtr pCompositeClip;
   Bool FreeCompositeClip;
   XvAdaptorRecPrivatePtr AdaptorRec;
   XvStatus isOn;
   Bool moved;
   int vid_x, vid_y, vid_w, vid_h;
   int drw_x, drw_y, drw_w, drw_h;
   DevUnion DevPriv;
} XvPortRecPrivate, *XvPortRecPrivatePtr;

typedef struct _XF86XVWindowRec{
   XvPortRecPrivatePtr PortRec;
   struct _XF86XVWindowRec *next;
} XF86XVWindowRec, *XF86XVWindowPtr;

#endif  /* _XF86XVPRIV_H_ */
