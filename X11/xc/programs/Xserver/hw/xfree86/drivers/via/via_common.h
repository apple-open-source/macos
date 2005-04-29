/* $XFree86: xc/programs/Xserver/hw/xfree86/drivers/via/via_common.h,v 1.2 2003/08/27 15:16:08 tsi Exp $ */

#ifndef _VIA_COMMON_H_
#define _VIA_COMMON_H_ 1

#include "xf86drm.h"

/* WARNING: If you change any of these defines, make sure to change
 * the kernel include file as well (via_drm.h)
 */

/* Driver specific DRM command indices
 * NOTE: these are not OS specific, but they are driver specific
 */

#define DRM_VIA_ALLOCMEM	0
#define DRM_VIA_FREEMEM		1
#define DRM_VIA_AGP_INIT	2
#define DRM_VIA_FB_INIT		3
#define DRM_VIA_MAP_INIT	4

#define VIDEO 0

typedef struct 
{
    unsigned int context;
    unsigned int type;
    unsigned int size;
    unsigned long index;
    unsigned long offset;
} drmViaMem;

typedef struct {
    unsigned int offset;
    unsigned int size;
} drmViaAgp;

typedef struct {
    unsigned int offset;
    unsigned int size;
} drmViaFb;

typedef struct 
{
    enum {
	VIA_INIT_MAP = 0x01,
	VIA_CLEANUP_MAP = 0x02
    } func;
    unsigned long sarea_priv_offset;
    unsigned long fb_offset;
    unsigned long mmio_offset;
    unsigned long agpAddr;
} drmViaInit;

#endif /* _VIA_COMMON_H_ */
