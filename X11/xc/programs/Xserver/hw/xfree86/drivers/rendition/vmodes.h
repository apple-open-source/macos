/* $XFree86: xc/programs/Xserver/hw/xfree86/drivers/rendition/vmodes.h,v 1.5 2002/04/04 14:05:45 eich Exp $ */
/*
 * file vmodes.h
 *
 * headerfile for vmodes.c
 */

#ifndef __VMODES_H__
#define __VMODES_H__



/*
 * includes
 */

#include "vtypes.h"



/*
 * function prototypes
 */

#if 0
int verite_setmodefixed(ScrnInfoPtr pScreenInfo);
#endif
void verite_save(ScrnInfoPtr pScreenInfo);
void verite_restore(ScrnInfoPtr pScreenInfo, RenditionRegPtr reg);
int verite_setmode(ScrnInfoPtr pScreenInfo, struct verite_modeinfo_t *mode);
void verite_setframebase(ScrnInfoPtr pScreenInfo, vu32 framebase);
int verite_getstride(ScrnInfoPtr pScreenInfo, int *width, vu16 *stride0, vu16 *stride1);



#endif /* #ifndef _VMODES_H_ */

/*
 * end of file vmodes.h
 */
