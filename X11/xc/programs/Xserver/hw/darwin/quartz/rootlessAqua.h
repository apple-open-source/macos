/*
 * Rootless setup for Aqua
 *
 * Greg Parker     gparker@cs.stanford.edu
 */
/* $XFree86: xc/programs/Xserver/hw/darwin/quartz/rootlessAqua.h,v 1.1 2002/03/28 02:21:19 torrey Exp $ */

#ifndef _ROOTLESSAQUA_H
#define _ROOTLESSAQUA_H

Bool AquaAddScreen(int index, ScreenPtr pScreen);
Bool AquaSetupScreen(int index, ScreenPtr pScreen);
void AquaDisplayInit(void);

#endif /* _ROOTLESSAQUA_H */
