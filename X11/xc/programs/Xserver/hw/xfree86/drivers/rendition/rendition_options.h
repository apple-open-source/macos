/* $XFree86: xc/programs/Xserver/hw/xfree86/drivers/rendition/rendition_options.h,v 1.5 2001/05/04 19:05:42 dawes Exp $ */

#ifndef __RENDITION_OPTION_H__
#define __RENDITION_OPTION_H__

/* supported options */
typedef enum {
    OPTION_FBWC,
    OPTION_SW_CURSOR,
    OPTION_NOACCEL,
    OPTION_OVERCLOCK_MEM,
    OPTION_NO_DDC,
    OPTION_SHADOW_FB,
    OPTION_ROTATE
} renditionOpts;



#ifdef DO_NOT_ACTIVATE
/*********************************/
      For reference only <DI>

static const OptionInfoRec renditionOptions[]={
    { OPTION_FBWC,      "FramebufferWC", OPTV_BOOLEAN, {0}, FALSE },
    { OPTION_SW_CURSOR, "SWCursor", OPTV_BOOLEAN, {0}, FALSE },
    { OPTION_NOACCEL,   "NoAccel",  OPTV_BOOLEAN, {0}, FALSE },
    { OPTION_OVERCLOCK_MEM,"Overclock_Mem",  OPTV_BOOLEAN, {0}, FALSE },
    { OPTION_NO_DDC,    "NoDDC",    OPTV_BOOLEAN, {0}, FALSE },
    { OPTION_SHADOW_FB, "ShadowFB", OPTV_BOOLEAN, {0}, FALSE },
    { OPTION_ROTATE,    "Rotate",   OPTV_ANYSTR,  {0}, FALSE },
    { -1,                NULL,      OPTV_NONE,    {0}, FALSE }
};
/*********************************/
#endif /* For reference only */


#endif /* __RENDITION_OPTION_H__ */
