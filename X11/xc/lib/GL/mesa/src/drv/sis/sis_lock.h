/**************************************************************************

Copyright 2000 Silicon Integrated Systems Corp, Inc., HsinChu, Taiwan.
All Rights Reserved.

Permission is hereby granted, free of charge, to any person obtaining a
copy of this software and associated documentation files (the
"Software"), to deal in the Software without restriction, including
without limitation the rights to use, copy, modify, merge, publish,
distribute, sub license, and/or sell copies of the Software, and to
permit persons to whom the Software is furnished to do so, subject to
the following conditions:

The above copyright notice and this permission notice (including the
next paragraph) shall be included in all copies or substantial portions
of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NON-INFRINGEMENT.
IN NO EVENT SHALL PRECISION INSIGHT AND/OR ITS SUPPLIERS BE LIABLE FOR
ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT,
TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE
SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.

**************************************************************************/
/* $XFree86: xc/lib/GL/mesa/src/drv/sis/sis_lock.h,v 1.3 2000/09/26 15:56:48 tsi Exp $ */

/*
 * Authors:
 *    Sung-Ching Lin <sclin@sis.com.tw>
 *
 */

#ifndef _sis_dri_h_
#define _sis_dri_h_

/* from tdfx */
#define SIS_VALIDATE_DRAWABLE_INFO(dpy, psp, pdp)                       \
do {                                                                    \
    while (*(pdp->pStamp) != pdp->lastStamp) {                          \
	DRM_SPINLOCK(&psp->pSAREA->drawable_lock, psp->drawLockID);     \
	DRI_MESA_VALIDATE_DRAWABLE_INFO(dpy, psp->myNum, pdp);          \
	DRM_SPINUNLOCK(&psp->pSAREA->drawable_lock, psp->drawLockID);   \
        sis_SetDrawBuffer (ctx, ctx->Color.DriverDrawBuffer);           \
    }                                                                   \
} while (0)

#ifdef DEBUG_LOCKING
extern char *prevLockFile;
extern int prevLockLine;
#define DEBUG_LOCK() \
  do { \
    prevLockFile=(__FILE__); \
    prevLockLine=(__LINE__); \
  } while (0)
#define DEBUG_RESET() \
  do { \
    prevLockFile=NULL; \
    prevLockLine=0; \
  } while (0)
#define DEBUG_CHECK_LOCK() \
  do { \
      if(prevLockFile){ \
        fprintf(stderr, "LOCK SET : %s:%d\n", __FILE__, __LINE__); \
      } \
  } while (0)
#else
#define DEBUG_LOCK()
#define DEBUG_RESET()
#define DEBUG_CHECK_LOCK()
#endif

#ifdef XFree86Server

/* TODO, X-server will inform us if drawable state changed?  */
#define LOCK_HARDWARE() \
  do { \
    mEndPrimitive(); \
    sis_SetDrawBuffer (ctx, ctx->Color.DriverDrawBuffer); \
    if(*(hwcx->CurrentHwcxPtr) != hwcx->serialNumber) \
      sis_validate_all_state(hwcx); \
  } while (0)

#define UNLOCK_HARDWARE() \
  do { \
    mEndPrimitive(); \
  } while (0)

#else

#define DRM_LIGHT_LOCK_RETURN(fd,lock,context,__ret)                   \
	do {                                                           \
		DRM_CAS(lock,context,DRM_LOCK_HELD|context,__ret);     \
                if (__ret) drmGetLock(fd,context,0);                   \
        } while(0)

/* Lock the hardware using the global current context */
#define LOCK_HARDWARE() \
  do { \
    int stamp; \
    char __ret=0; \
    __DRIdrawablePrivate *dPriv = xmesa->driContextPriv->driDrawablePriv; \
    __DRIscreenPrivate *sPriv = dPriv->driScreenPriv; \
    mEndPrimitive(); \
    DEBUG_CHECK_LOCK(); \
    DEBUG_LOCK(); \
    DRM_LIGHT_LOCK_RETURN(sPriv->fd, &sPriv->pSAREA->lock, \
		   dPriv->driContextPriv->hHWContext, __ret); \
    stamp=dPriv->lastStamp; \
    XMESA_VALIDATE_DRAWABLE_INFO(xmesa->display, sPriv, dPriv); \
    if (*(dPriv->pStamp)!=stamp) \
     { \
       sis_SetDrawBuffer (ctx, ctx->Color.DriverDrawBuffer); \
     } \
    if(__ret && (*(hwcx->CurrentHwcxPtr) != hwcx->serialNumber)) \
      { \
        sis_validate_all_state(hwcx); \
      } \
  } while (0)

/* Unlock the hardware using the global current context */
#define UNLOCK_HARDWARE() \
  do { \
    __DRIdrawablePrivate *dPriv = xmesa->driContextPriv->driDrawablePriv; \
    __DRIscreenPrivate *sPriv = dPriv->driScreenPriv; \
    mEndPrimitive(); \
    DEBUG_RESET(); \
    *(hwcx->CurrentHwcxPtr) = hwcx->serialNumber; \
    DRM_UNLOCK(sPriv->fd, &sPriv->pSAREA->lock, \
	       dPriv->driContextPriv->hHWContext); \
  } while (0)

#endif

#endif
