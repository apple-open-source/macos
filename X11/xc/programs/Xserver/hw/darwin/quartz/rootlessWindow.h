/*
 * Rootless window management
 *
 * Greg Parker     gparker@cs.stanford.edu
 */
/* $XFree86: xc/programs/Xserver/hw/darwin/quartz/rootlessWindow.h,v 1.2 2002/04/03 00:06:32 torrey Exp $ */

#ifndef _ROOTLESSWINDOW_H
#define _ROOTLESSWINDOW_H

#include "rootlessCommon.h"


Bool RootlessCreateWindow(WindowPtr pWin);
Bool RootlessDestroyWindow(WindowPtr pWin);

#ifdef SHAPE
void RootlessSetShape(WindowPtr pWin);
#endif // SHAPE

Bool RootlessChangeWindowAttributes(WindowPtr pWin, unsigned long vmask);
Bool RootlessPositionWindow(WindowPtr pWin, int x, int y);
Bool RootlessRealizeWindow(WindowPtr pWin);
Bool RootlessUnrealizeWindow(WindowPtr pWin);
void RootlessRestackWindow(WindowPtr pWin, WindowPtr pOldNextSib);
void RootlessCopyWindow(WindowPtr pWin,DDXPointRec ptOldOrg,RegionPtr prgnSrc);
void RootlessMoveWindow(WindowPtr pWin,int x,int y,WindowPtr pSib,VTKind kind);
void RootlessResizeWindow(WindowPtr pWin, int x, int y,
			  unsigned int w, unsigned int h, WindowPtr pSib);
void RootlessPaintWindowBackground(WindowPtr pWin, RegionPtr pRegion,
                                   int what);
void RootlessPaintWindowBorder(WindowPtr pWin, RegionPtr pRegion,
                               int what);
void RootlessChangeBorderWidth(WindowPtr pWin, unsigned int width);

#endif
