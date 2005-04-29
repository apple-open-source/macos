/* $XFree86: xc/programs/Xserver/hw/xfree86/loader/extsym.c,v 1.10 2004/02/13 23:58:44 dawes Exp $ */

/*
 * Copyright 1999-2003 by The XFree86 Project, Inc.
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

#include "resource.h"
#include "sym.h"
#include "misc.h"
#ifdef PANORAMIX
#include "panoramiX.h"
#endif
#include "sleepuntil.h"

#ifdef HAS_SHM
extern int ShmCompletionCode;
extern int BadShmSegCode;
extern RESTYPE ShmSegType, ShmPixType;
#endif

#ifdef PANORAMIX
extern Bool noPanoramiXExtension;
extern int PanoramiXNumScreens;
extern PanoramiXData *panoramiXdataPtr;
extern XID *PanoramiXVisualTable;
extern unsigned long XRT_WINDOW;
extern unsigned long XRT_PIXMAP;
extern unsigned long XRT_GC;
extern unsigned long XRT_COLORMAP;
extern unsigned long XRC_DRAWABLE;
extern Bool XineramaRegisterConnectionBlockCallback(void (*func) (void));
extern int XineramaDeleteResource(pointer, XID);
#endif

LOOKUP extLookupTab[] = {

    SYMFUNC(ClientSleepUntil)

#ifdef HAS_SHM
    SYMVAR(ShmCompletionCode)
    SYMVAR(BadShmSegCode)
    SYMVAR(ShmSegType)
#endif

#ifdef PANORAMIX
    SYMFUNC(XineramaRegisterConnectionBlockCallback)
    SYMFUNC(XineramaDeleteResource)
    SYMVAR(noPanoramiXExtension)
    SYMVAR(PanoramiXNumScreens)
    SYMVAR(panoramiXdataPtr)
    SYMVAR(PanoramiXVisualTable)
    SYMVAR(XRT_WINDOW)
    SYMVAR(XRT_PIXMAP)
    SYMVAR(XRT_GC)
    SYMVAR(XRT_COLORMAP)
    SYMVAR(XRC_DRAWABLE)
#endif

    {0, 0}
};
