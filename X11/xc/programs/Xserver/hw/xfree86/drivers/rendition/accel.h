/*
 * file accel.h
 *
 * header file for accel.c
 */
/* $XFree86: xc/programs/Xserver/hw/xfree86/drivers/rendition/accel.h,v 1.3 1999/11/19 13:54:44 hohndel Exp $ */

#ifndef __ACCEL_H__
#define __ACCEL_H__
 


/*
 * includes
 */

#include "vtypes.h"



/*
 * function prototypes
 */

void RENDITIONAccelPreInit(ScrnInfoPtr pScreenInfo);
void RENDITIONAccelXAAInit(ScreenPtr pScreen);
void RENDITIONAccelNone(ScrnInfoPtr pScreenInfo);
int RENDITIONInitUcode(ScrnInfoPtr pScreenInfo);
int RENDITIONLoadUcode(ScrnInfoPtr pScreenInfo);

void RENDITIONDumpUcode(ScrnInfoPtr pScreenInfo);
void RENDITIONDrawSomething(ScrnInfoPtr pScreenInfo);



#endif /* #ifdef __ACCEL_H__ */

/*
 * end of file accel.h
 */
