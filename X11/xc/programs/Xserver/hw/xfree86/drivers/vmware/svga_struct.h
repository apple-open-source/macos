/* **********************************************************
 * Copyright (C) 1998-2000 VMware, Inc.
 * All Rights Reserved
 * **********************************************************/

#ifndef _SVGA_STRUCT_H_
#define _SVGA_STRUCT_H_

#define INCLUDE_ALLOW_USERLEVEL
#define INCLUDE_ALLOW_MONITOR
#include "includeCheck.h"

 /*
  * Offscreen memory surface structure
  *
  */

enum SVGASurfaceVersion {
   SVGA_SURFACE_VERSION_1 = 1  /* Initial version... */
};

typedef struct _SVGASurface {
   uint32   size;             /* Size of the structure */
   uint32   version;          /* Version of this surface structure.  */
   uint32   bpp;              /* Format of the surface */
   uint32   width;            /* Width of the surface */
   uint32   height;           /* Height of the surface */
   uint32   pitch;            /* Pitch of the surface */
   volatile uint32   numQueued;        /* Number of times this bitmap has been queued */
   volatile uint32   numDequeued;      /* Number of times this bitmap has been dequeued */
   uint32   userData;         /* Driver defined data */
   uint32   dataOffset;       /* Offset to the data */
} SVGASurface;

typedef struct SVGAPoint {
    int16 x;
    int16 y;
} SVGAPoint;

#endif
