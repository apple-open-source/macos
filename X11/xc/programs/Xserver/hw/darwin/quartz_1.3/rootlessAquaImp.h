/*
 * Rootless implementation for Mac OS X Aqua environment
 *
 * Greg Parker     gparker@cs.stanford.edu
 */
/* $XFree86: xc/programs/Xserver/hw/darwin/quartz_1.3/rootlessAquaImp.h,v 1.1 2002/03/28 02:21:20 torrey Exp $ */

#ifndef _ROOTLESSAQUAIMP_H
#define _ROOTLESSAQUAIMP_H

#include "fakeBoxRec.h"

int AquaDisplayCount();

void AquaScreenInit(int index, int *x, int *y, int *width, int *height,
                    int *rowBytes, unsigned long *bps, unsigned long *spp,
                    int *bpp);

void *AquaNewWindow(void *upperw, int x, int y, int w, int h, int isRoot);

void AquaDestroyWindow(void *rw);

void AquaMoveWindow(void *rw, int x, int y);

void AquaStartResizeWindow(void *rw, int x, int y, int w, int h);

void AquaFinishResizeWindow(void *rw, int x, int y, int w, int h);

void AquaUpdateRects(void *rw, fakeBoxRec *rects, int count);

void AquaRestackWindow(void *rw, void *lowerw);

void AquaReshapeWindow(void *rw, fakeBoxRec *rects, int count);

void AquaGetPixmap(void *rw, char **bits,
                   int *rowBytes, int *depth, int *bpp);

#endif /* _ROOTLESSAQUAIMP_H */
