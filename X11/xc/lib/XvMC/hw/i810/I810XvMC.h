/***************************************************************************

Copyright 2001 Intel Corporation.  All Rights Reserved.

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
IN NO EVENT SHALL INTEL, AND/OR ITS SUPPLIERS BE LIABLE FOR ANY CLAIM,
DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR
OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR
THE USE OR OTHER DEALINGS IN THE SOFTWARE.

**************************************************************************/

/***************************************************************************
 * libI810XvMC.h: MC Driver i810 includes
 *
 * Authors:
 *      Matt Sottek <matthew.j.sottek@intel.com>
 *
 *
 ***************************************************************************/
/* $XFree86: xc/lib/XvMC/hw/i810/I810XvMC.h,v 1.4 2002/11/19 09:35:49 alanh Exp $ */


#ifndef I810XVMC_H
#define I810XVMC_H

/* #define XVMC_DEBUG(x) do {x; }while(0); */
#define XVMC_DEBUG(x)

#include "xf86drm.h"
#include "i810_common.h"
#include <X11/Xlibint.h>


/***************************************************************************
// i810OverlayRec: Structure that is used to reference the overlay
//  register memory. A i810OverlayRecPtr is set to the address of the
//  allocated overlay registers.
***************************************************************************/
typedef struct _i810OverlayRec {
  unsigned int OBUF_0Y;
  unsigned int OBUF_1Y;
  unsigned int OBUF_0U;
  unsigned int OBUF_0V;
  unsigned int OBUF_1U;
  unsigned int OBUF_1V;
  unsigned int OV0STRIDE;
  unsigned int YRGB_VPH;
  unsigned int UV_VPH;
  unsigned int HORZ_PH;
  unsigned int INIT_PH;
  unsigned int DWINPOS;
  unsigned int DWINSZ;
  unsigned int SWID;
  unsigned int SWIDQW;
  unsigned int SHEIGHT;
  unsigned int YRGBSCALE;
  unsigned int UVSCALE;
  unsigned int OV0CLRC0;
  unsigned int OV0CLRC1;
  unsigned int DCLRKV;
  unsigned int DCLRKM;
  unsigned int SCLRKVH;
  unsigned int SCLRKVL;
  unsigned int SCLRKM;
  unsigned int OV0CONF;
  unsigned int OV0CMD;
} i810OverlayRec, *i810OverlayRecPtr;


/***************************************************************************
// i810XvMCDrmMap: Holds the data about the DRM maps
***************************************************************************/
typedef struct _i810XvMCDrmMap {
  drmHandle offset;
  drmAddress address;
  unsigned int size;
} i810XvMCDrmMap, *i810XvMCDrmMapPtr;

/***************************************************************************
// i810XvMCContext: Private Context data referenced via the privData
//  pointer in the XvMCContext structure.
***************************************************************************/
typedef struct _i810XvMCContext {
  int fd;   /* File descriptor for /dev/dri */
  i810XvMCDrmMap overlay;
  i810XvMCDrmMap surfaces;
  drmBufMapPtr dmabufs;  /* Data structure to hold available dma buffers */
  drmContext drmcontext;
  unsigned int last_render;
  unsigned int last_flip;
  unsigned short ref;
  unsigned short current;
  int lock;   /* Lightweight lock to avoid locking twice */
  char busIdString[10]; /* PCI:0:1:0 or PCI:0:2:0 */
  i810OverlayRecPtr oregs;
  unsigned int dual_prime; /* Flag to identify when dual prime is in use. */
  unsigned int fb_base;
  Atom xv_colorkey;
  Atom xv_brightness;
  Atom xv_contrast;
  Atom xv_saturation;
  int brightness;
  int saturation;
  int contrast;
  int colorkey;
} i810XvMCContext;

/***************************************************************************
// i810XvMCSurface: Private data structure for each XvMCSurface. This
//  structure is referenced by the privData pointer in the XvMCSurface
//  structure.
***************************************************************************/
typedef struct _i810XvMCSurface {
  unsigned int pitch;
  unsigned int dbi1y;  /* Destination buffer info command word 1 for Y */
  unsigned int dbi1u; /* Destination buffer info command word 1 for U */
  unsigned int dbi1v; /* Destination buffer info command word 1 for V */
  unsigned int dbv1;  /* Destination buffer variables command word 1 */
  unsigned int mi1y;   /* Map Info command word 1 (Minus bit 28) for Y */
  unsigned int mi1u;   /* Map Info command word 1 (Minus bit 28) for U */
  unsigned int mi1v;   /* Map Info command word 1 (Minus bit 28) for V */
  unsigned int mi2y;   /* Map info command word 2 for y */
  unsigned int mi2u;   /* Map info command word 2 for y */
  unsigned int mi2v;   /* Map info command word 2 for y */
  unsigned int mi3y;   /* Map info command word 3 */
  unsigned int mi3u;   /* Map info command word 3 */
  unsigned int mi3v;   /* Map info command word 3 */
  unsigned int last_render;
  unsigned int last_flip;
  unsigned int second_field; /* Flags a surface that is only half done */
  drmAddress data;
  drmHandle offset;
  unsigned int offsets[3];
  i810XvMCContext *privContext;
} i810XvMCSurface;

/***************************************************************************
// i810XvMCSubpicture: Private data structure for each XvMCSubpicture. This
//  structure is referenced by the privData pointer in the XvMCSubpicture
//  structure.
***************************************************************************/
typedef struct _i810XvMCSubpicture {
  unsigned int pitch;
  unsigned int dbi1;  /* Destination buffer info command word 1 */
  unsigned int dbv1;  /* Destination buffer variables command word */
  unsigned int mi1;   /* Map Info command word 1 (Minus bit 28) */
  unsigned int mi2;   /* Map info command word 2 */
  unsigned int mi3;   /* Map info command word 3 */
  unsigned int last_render;
  unsigned int last_flip;
  drmAddress data;
  drmHandle offset;
  unsigned int offsets[1];
  unsigned char palette[3][16];
  i810XvMCContext *privContext;
} i810XvMCSubpicture;

typedef struct _Box {
  int x1,y1,x2,y2;
} Box, *BoxPtr;

/***************************************************************************
// drm_i810_overlay_t: Structure returned by overlay info ioctl.
// NOTE: If you change this structure you will have to change the equiv.
//  structure in the kernel.
***************************************************************************/
typedef struct _drm_i810_overlay_t {
  unsigned int offset;
  unsigned int physical;
} drm_i810_overlay_t;

/***************************************************************************
// drm_i810_dma_t: Structure used by dma allocation ioctl.
// NOTE: If you change this structure you will have to change the equiv.
//  structure in the kernel.
***************************************************************************/
typedef struct _drm_i810_dma {
  void *virtual;
  int request_idx;
  int request_size;
  int granted;
} drm_i810_dma_t;

/***************************************************************************
// drm_i810_mc_t: Structure used by mc dispatch ioctl.
// NOTE: If you change this structure you will have to change the equiv.
//  structure in the kernel.
***************************************************************************/
typedef struct _drm_i810_mc {
  int idx;		/* buffer index */
  int used;		/* nr bytes in use */
  int num_blocks;         /* number of GFXBlocks */
  int *length;	        /* List of lengths for GFXBlocks */
  unsigned int last_render; /* Last render request */
} drm_i810_mc_t;

/* Subpicture fourcc */
#define FOURCC_IA44 0x34344149

/* Static Parameters */
#define I810_XVMC_MAXWIDTH  720
#define I810_XVMC_MAXHEIGHT  576
#define I810_DEFAULT16_COLORKEY 31
#define I810_DMA_BUF_NR 256

/* COMMANDS */
#define CMD_FLUSH        ((4<<23) | 0x1)
#define BOOLEAN_ENA_1    ((3<<29) | (3<<24) | (3<<2))
#define BOOLEAN_ENA_2    ((3<<29) | (4<<24) | (3<<16) | (1<<3) | (1<<2))
#define DEST_BUFFER_INFO (0x15<<23)
#define DEST_BUFFER_VAR  ((0x3<<29) | (0x1d<<24) | (0x85<<16))
#define DRAWING_RECT_INFO ((3<<29) | (0x1d<<24) | (0x80<<16) | 3)
#define GFXBLOCK         ((0x3<<29) | (0x1e<<24))
#define CMD_MAP_INFO     ((0x3<<29) | (0x1d<<24) | 0x2)
#define MAP_PALETTE_LOAD ((3<<29) | (0x1d<<24) | (0x82<<16) | 0xff)
#define VERTEX_FORMAT    ((3<<29) | (0x5<<24))
#define SRC_DEST_BLEND_MONO ((3<<29) | (8<<24))

/* Bit Patterns */

/*
 * OV0CMD - Overlay Command Register
 */
#define VERTICAL_CHROMINANCE_FILTER     0x70000000
#define VC_SCALING_OFF          0x00000000
#define VC_LINE_REPLICATION     0x10000000
#define VC_UP_INTERPOLATION     0x20000000
#define VC_PIXEL_DROPPING       0x50000000
#define VC_DOWN_INTERPOLATION   0x60000000
#define VERTICAL_LUMINANCE_FILTER       0x0E000000
#define VL_SCALING_OFF          0x00000000
#define VL_LINE_REPLICATION     0x02000000
#define VL_UP_INTERPOLATION     0x04000000
#define VL_PIXEL_DROPPING       0x0A000000
#define VL_DOWN_INTERPOLATION   0x0C000000
#define HORIZONTAL_CHROMINANCE_FILTER   0x01C00000
#define HC_SCALING_OFF          0x00000000
#define HC_LINE_REPLICATION     0x00400000
#define HC_UP_INTERPOLATION     0x00800000
#define HC_PIXEL_DROPPING       0x01400000
#define HC_DOWN_INTERPOLATION   0x01800000
#define HORIZONTAL_LUMINANCE_FILTER     0x00380000
#define HL_SCALING_OFF          0x00000000
#define HL_LINE_REPLICATION     0x00080000
#define HL_UP_INTERPOLATION     0x00100000
#define HL_PIXEL_DROPPING       0x00280000
#define HL_DOWN_INTERPOLATION   0x00300000

#define Y_ADJUST                0x00010000
#define OV_BYTE_ORDER           0x0000C000
#define UV_SWAP                 0x00004000
#define Y_SWAP                  0x00008000
#define Y_AND_UV_SWAP           0x0000C000
#define SOURCE_FORMAT           0x00003C00
#define RGB_555                 0x00000800
#define RGB_565                 0x00000C00
#define YUV_422                 0x00002000
#define YUV_411                 0x00002400
#define YUV_420                 0x00003000
#define YUV_410                 0x00003800
#define VERTICAL_PHASE_BOTH     0x00000020
#define FLIP_TYPE_FIELD         0x00000020
#define FLIP_TYPE_FRAME         0x00000000
#define BUFFER_AND_FIELD        0x00000006
#define BUFFER0_FIELD0          0x00000000
#define BUFFER0_FIELD1          0x00000002
#define BUFFER1_FIELD0          0x00000004
#define BUFFER1_FIELD1          0x00000006
#define OVERLAY_ENABLE          0x00000001

/*
 * DOV0STA - Display/Overlay 0 Status Register
 */
#define DOV0STA         0x30008
#define OV0ADD         0x30000
#define MINUV_SCALE     0x1

#define RGB16ToColorKey(c) \
        (((c & 0xF800) << 8) | ((c & 0x07E0) << 5) | ((c & 0x001F) << 3))

/* Locking Macros lightweight lock used to prevent relocking */
#define I810_LOCK(c,f)                     \
  if(!c->lock) {                           \
    drmGetLock(c->fd, c->drmcontext, f);   \
  }                                        \
  c->lock++;

#define I810_UNLOCK(c)                     \
  c->lock--;                               \
  if(!c->lock) {                           \
    drmUnlock(c->fd, c->drmcontext);       \
  }

/*
  Block until the passed in value (n) is the active
  buffer on the overlay.
*/
#define BLOCK_OVERLAY(c,n)                        \
  do {                                            \
   int temp,i=0;                                  \
   while(i < 100000) {                            \
     temp = GET_FSTATUS(c);                       \
     if(((temp & (1<<20))>>20) == n) {            \
       break;                                     \
     }                                            \
     usleep(10);                                  \
   }                                              \
   if(i == 100000) {                              \
     printf("Overlay Lockup.\n");                 \
     return BadAlloc;                             \
   }                                              \
 }while(0);

#define OVERLAY_INFO(c,i) drmCommandRead(c->fd, DRM_I810_OV0INFO, &i, sizeof(i))
#define OVERLAY_FLIP(c) drmCommandNone(c->fd, DRM_I810_OV0FLIP)
#define GET_FSTATUS(c) drmCommandNone(c->fd, DRM_I810_FSTATUS)
#define I810_MC(c,mc) drmCommandWrite(c->fd, DRM_I810_MC, &mc, sizeof(mc))
#define GET_RSTATUS(c) drmCommandNone(c->fd, DRM_I810_RSTATUS)
#define GET_BUFFER(c,dma) drmCommandWriteRead(c->fd, DRM_I810_GETBUF, &dma, sizeof(drmI810DMA))
#define FLUSH(c) drmCommandNone(c->fd, DRM_I810_FLUSH)

/*
  Definitions for temporary wire protocol hooks to be replaced
  when a HW independent libXvMC is created.
*/
extern Status _xvmc_create_context(Display *dpy, XvMCContext *context,
				   int *priv_count, uint **priv_data);

extern Status _xvmc_destroy_context(Display *dpy, XvMCContext *context);

extern Status _xvmc_create_surface(Display *dpy, XvMCContext *context,
				   XvMCSurface *surface, int *priv_count,
				   uint **priv_data);

extern Status _xvmc_destroy_surface(Display *dpy, XvMCSurface *surface);

extern Status  _xvmc_create_subpicture(Display *dpy, XvMCContext *context,
				       XvMCSubpicture *subpicture,
				       int *priv_count, uint **priv_data);

extern Status   _xvmc_destroy_subpicture(Display *dpy,
					 XvMCSubpicture *subpicture);

/*
  Prototypes
*/
drmBufPtr i810_get_free_buffer(i810XvMCContext *pI810XvMC);
void i810_free_privContext(i810XvMCContext *pI810XvMC);
void dp(unsigned int *address, unsigned int i);

#endif







































































































