/*
 * quartzShared.h
 *
 * Shared definitions between the Darwin X Server and the Cocoa front end
 *
 * This file is included in all parts of the Darwin X Server and must not
 * include any types defined in X11 or Mac OS X specific headers.
 * Definitions that are internal to the Quartz modes or use Mac OS X
 * specific types should be in quartzCommon.h instead of here.
 */
/*
 * Copyright (c) 2001 Torrey T. Lyons and Greg Parker.
 *                 All Rights Reserved.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL
 * THE ABOVE LISTED COPYRIGHT HOLDER(S) BE LIABLE FOR ANY CLAIM, DAMAGES OR
 * OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE,
 * ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
 * DEALINGS IN THE SOFTWARE.
 *
 * Except as contained in this notice, the name(s) of the above copyright
 * holders shall not be used in advertising or otherwise to promote the sale,
 * use or other dealings in this Software without prior written authorization.
 */
/* $XFree86: xc/programs/Xserver/hw/darwin/quartz/quartzShared.h,v 1.2 2002/10/12 00:32:45 torrey Exp $ */

#ifndef _QUARTZSHARED_H
#define _QUARTZSHARED_H

// User preferences used by generic Darwin X server code
extern int                  quartzMouseAccelChange;
extern int                  darwinFakeButtons;
extern int                  darwinFakeMouse2Mask;
extern int                  darwinFakeMouse3Mask;
extern char                 *darwinKeymapFile;
extern unsigned int         darwinDesiredWidth, darwinDesiredHeight;
extern int                  darwinDesiredDepth;
extern int                  darwinDesiredRefresh;

// location of X11's (0,0) point in global screen coordinates
extern int                  darwinMainScreenX;
extern int                  darwinMainScreenY;

#endif	/* _QUARTZSHARED_H */

