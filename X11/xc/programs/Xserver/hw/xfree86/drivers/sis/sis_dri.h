/* $XFree86: xc/programs/Xserver/hw/xfree86/drivers/sis/sis_dri.h,v 1.6 2001/05/16 13:43:17 alanh Exp $ */

/* modified from tdfx_dri.h */

#ifndef _SIS_DRI_
#define _SIS_DRI_

#include "xf86drm.h"

#define SIS_MAX_DRAWABLES 256
#define SISIOMAPSIZE (64*1024)

typedef struct {
  int CtxOwner;
  int QueueLength;
  unsigned int AGPCmdBufNext;
  unsigned int FrameCount;
} SISSAREAPriv;

#define SIS_FRONT 0
#define SIS_BACK 1
#define SIS_DEPTH 2

typedef struct {
  drmHandle handle;
  drmSize size;
  drmAddress map;
} sisRegion, *sisRegionPtr;

typedef struct {
  sisRegion regs, agp;
  int deviceID;
  int width;
  int height;
  int mem;
  int bytesPerPixel;
  int priv1;
  int priv2;
  int fbOffset;
  int backOffset;
  int depthOffset;
  int textureOffset;
  int textureSize;
  unsigned int AGPCmdBufOffset;
  unsigned int AGPCmdBufSize;
  int irqEnabled;
  unsigned int scrnX, scrnY;
} SISDRIRec, *SISDRIPtr;

typedef struct {
  /* Nothing here yet */
  int dummy;
} SISConfigPrivRec, *SISConfigPrivPtr;

typedef struct {
  /* Nothing here yet */
  int dummy;
} SISDRIContextRec, *SISDRIContextPtr;

#ifdef XFree86Server

#include "screenint.h"

Bool SISDRIScreenInit(ScreenPtr pScreen);
void SISDRICloseScreen(ScreenPtr pScreen);
Bool SISDRIFinishScreenInit(ScreenPtr pScreen);

#endif
#endif
