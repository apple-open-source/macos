/*
 * Rootless setup for Aqua
 *
 * Greg Parker     gparker@cs.stanford.edu
 */
/* $XFree86: xc/programs/Xserver/hw/darwin/quartz/rootlessAqua.h,v 1.2 2003/04/30 23:15:39 torrey Exp $ */

#ifndef _ROOTLESSAQUA_H
#define _ROOTLESSAQUA_H

Bool AquaAddScreen(int index, ScreenPtr pScreen);
Bool AquaSetupScreen(int index, ScreenPtr pScreen);
void AquaDisplayInit(void);
void AquaInitInput(int argc, char **argv);

#endif /* _ROOTLESSAQUA_H */
