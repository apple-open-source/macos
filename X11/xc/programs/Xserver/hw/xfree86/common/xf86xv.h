/* $XFree86: xc/programs/Xserver/hw/xfree86/common/xf86xv.h,v 1.26 2004/02/13 23:58:40 dawes Exp $ */

/*
 * Copyright (c) 1998-2003 by The XFree86 Project, Inc.
 * All rights reserved.
 *
 * Permission is hereby granted, free of charge, to any person obtaining
 * a copy of this software and associated documentation files (the
 * "Software"), to deal in the Software without restriction, including
 * without limitation the rights to use, copy, modify, merge, publish,
 * distribute, sublicense, and/or sell copies of the Software, and to
 * permit persons to whom the Software is furnished to do so, subject
 * to the following conditions:
 *
 *   1.  Redistributions of source code must retain the above copyright
 *       notice, this list of conditions, and the following disclaimer.
 *
 *   2.  Redistributions in binary form must reproduce the above copyright
 *       notice, this list of conditions and the following disclaimer
 *       in the documentation and/or other materials provided with the
 *       distribution, and in the same place and form as other copyright,
 *       license and disclaimer information.
 *
 *   3.  The end-user documentation included with the redistribution,
 *       if any, must include the following acknowledgment: "This product
 *       includes software developed by The XFree86 Project, Inc
 *       (http://www.xfree86.org/) and its contributors", in the same
 *       place and form as other third-party acknowledgments.  Alternately,
 *       this acknowledgment may appear in the software itself, in the
 *       same form and location as other such third-party acknowledgments.
 *
 *   4.  Except as contained in this notice, the name of The XFree86
 *       Project, Inc shall not be used in advertising or otherwise to
 *       promote the sale, use or other dealings in this Software without
 *       prior written authorization from The XFree86 Project, Inc.
 *
 * THIS SOFTWARE IS PROVIDED ``AS IS'' AND ANY EXPRESSED OR IMPLIED
 * WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE XFREE86 PROJECT, INC OR ITS CONTRIBUTORS BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY,
 * OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT
 * OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR
 * BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
 * WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE
 * OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE,
 * EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef _XF86XV_H_
#define _XF86XV_H_

#include "xvdix.h"
#include "xf86str.h"

#define VIDEO_NO_CLIPPING			0x00000001
#define VIDEO_INVERT_CLIPLIST			0x00000002
#define VIDEO_OVERLAID_IMAGES			0x00000004
#define VIDEO_OVERLAID_STILLS			0x00000008
#define VIDEO_CLIP_TO_VIEWPORT			0x00000010

typedef struct {
  int id;
  int type;
  int byte_order;
  unsigned char guid[16];               
  int bits_per_pixel;
  int format;
  int num_planes;

  /* for RGB formats only */
  int depth;
  unsigned int red_mask;       
  unsigned int green_mask;   
  unsigned int blue_mask;   

  /* for YUV formats only */
  unsigned int y_sample_bits;
  unsigned int u_sample_bits;
  unsigned int v_sample_bits;   
  unsigned int horz_y_period;
  unsigned int horz_u_period;
  unsigned int horz_v_period;
  unsigned int vert_y_period;
  unsigned int vert_u_period;
  unsigned int vert_v_period;
  char component_order[32];
  int scanline_order;
} XF86ImageRec, *XF86ImagePtr; 


typedef struct {
  ScrnInfoPtr pScrn;
  int id;
  unsigned short width, height;
  int *pitches; /* bytes */
  int *offsets; /* in bytes from start of framebuffer */
  DevUnion devPrivate;  
} XF86SurfaceRec, *XF86SurfacePtr;


typedef int (* PutVideoFuncPtr)( ScrnInfoPtr pScrn, 
	short vid_x, short vid_y, short drw_x, short drw_y,
	short vid_w, short vid_h, short drw_w, short drw_h,
	RegionPtr clipBoxes, pointer data );
typedef int (* PutStillFuncPtr)( ScrnInfoPtr pScrn, 
	short vid_x, short vid_y, short drw_x, short drw_y,
	short vid_w, short vid_h, short drw_w, short drw_h,
	RegionPtr clipBoxes, pointer data );
typedef int (* GetVideoFuncPtr)( ScrnInfoPtr pScrn, 
	short vid_x, short vid_y, short drw_x, short drw_y,
	short vid_w, short vid_h, short drw_w, short drw_h,
	RegionPtr clipBoxes, pointer data );
typedef int (* GetStillFuncPtr)( ScrnInfoPtr pScrn, 
	short vid_x, short vid_y, short drw_x, short drw_y,
	short vid_w, short vid_h, short drw_w, short drw_h,
	RegionPtr clipBoxes, pointer data );
typedef void (* StopVideoFuncPtr)(ScrnInfoPtr pScrn, pointer data, Bool Exit);
typedef int (* SetPortAttributeFuncPtr)(ScrnInfoPtr pScrn, Atom attribute,
	INT32 value, pointer data);
typedef int (* GetPortAttributeFuncPtr)(ScrnInfoPtr pScrn, Atom attribute,
	INT32 *value, pointer data);
typedef void (* QueryBestSizeFuncPtr)(ScrnInfoPtr pScrn, Bool motion,
	short vid_w, short vid_h, short drw_w, short drw_h, 
	unsigned int *p_w, unsigned int *p_h, pointer data);
typedef int (* PutImageFuncPtr)( ScrnInfoPtr pScrn, 
	short src_x, short src_y, short drw_x, short drw_y,
	short src_w, short src_h, short drw_w, short drw_h,
	int image, unsigned char* buf, short width, short height, Bool Sync,
	RegionPtr clipBoxes, pointer data );
typedef int (* ReputImageFuncPtr)( ScrnInfoPtr pScrn, short drw_x, short drw_y,
        RegionPtr clipBoxes, pointer data );
typedef int (*QueryImageAttributesFuncPtr)(ScrnInfoPtr pScrn, 
	int image, unsigned short *width, unsigned short *height, 
	int *pitches, int *offsets);

typedef enum {
    XV_OFF,
    XV_PENDING,
    XV_ON
} XvStatus;

/*** this is what the driver needs to fill out ***/

typedef struct {
  int id;
  char *name;
  unsigned short width, height;
  XvRationalRec rate;
} XF86VideoEncodingRec, *XF86VideoEncodingPtr;

typedef struct {
  char 	depth;  
  short class;
} XF86VideoFormatRec, *XF86VideoFormatPtr;

typedef struct {
  int   flags;
  int   min_value;
  int   max_value;
  char  *name;
} XF86AttributeRec, *XF86AttributePtr;

typedef struct {
  unsigned int type; 
  int flags;
  char *name;
  int nEncodings;
  XF86VideoEncodingPtr pEncodings;  
  int nFormats;
  XF86VideoFormatPtr pFormats;  
  int nPorts;
  DevUnion *pPortPrivates;
  int nAttributes;
  XF86AttributePtr pAttributes;
  int nImages;
  XF86ImagePtr pImages;
  PutVideoFuncPtr PutVideo;
  PutStillFuncPtr PutStill;
  GetVideoFuncPtr GetVideo;
  GetStillFuncPtr GetStill;
  StopVideoFuncPtr StopVideo;
  SetPortAttributeFuncPtr SetPortAttribute;
  GetPortAttributeFuncPtr GetPortAttribute;
  QueryBestSizeFuncPtr QueryBestSize;
  PutImageFuncPtr PutImage;
  ReputImageFuncPtr ReputImage;
  QueryImageAttributesFuncPtr QueryImageAttributes;
} XF86VideoAdaptorRec, *XF86VideoAdaptorPtr;

typedef struct {
  XF86ImagePtr image;
  int flags;
  int (*alloc_surface)(ScrnInfoPtr pScrn,
		  int id,
		  unsigned short width, 	
		  unsigned short height,
		  XF86SurfacePtr surface);
  int (*free_surface)(XF86SurfacePtr surface);
  int (*display) (XF86SurfacePtr surface,
		  short vid_x, short vid_y, 
		  short drw_x, short drw_y,
		  short vid_w, short vid_h, 
		  short drw_w, short drw_h,
		  RegionPtr clipBoxes);
  int (*stop)    (XF86SurfacePtr surface);
  int (*getAttribute) (ScrnInfoPtr pScrn, Atom attr, INT32 *value);
  int (*setAttribute) (ScrnInfoPtr pScrn, Atom attr, INT32 value);
  int max_width;
  int max_height;
  int num_attributes;
  XF86AttributePtr attributes;
} XF86OffscreenImageRec, *XF86OffscreenImagePtr;

Bool
xf86XVScreenInit(
   ScreenPtr pScreen, 
   XF86VideoAdaptorPtr 	*Adaptors,
   int num
);

typedef int (* xf86XVInitGenericAdaptorPtr)(ScrnInfoPtr pScrn,
	XF86VideoAdaptorPtr **Adaptors);

int
xf86XVRegisterGenericAdaptorDriver(
    xf86XVInitGenericAdaptorPtr InitFunc
);

int
xf86XVListGenericAdaptors(
    ScrnInfoPtr          pScrn,
    XF86VideoAdaptorPtr  **Adaptors
);

Bool 
xf86XVRegisterOffscreenImages(
   ScreenPtr pScreen,
   XF86OffscreenImagePtr images,
   int num
);

XF86OffscreenImagePtr
xf86XVQueryOffscreenImages(
   ScreenPtr pScreen,
   int *num
);
   
XF86VideoAdaptorPtr xf86XVAllocateVideoAdaptorRec(ScrnInfoPtr pScrn);

void xf86XVFreeVideoAdaptorRec(XF86VideoAdaptorPtr ptr);

void
xf86XVFillKeyHelper (ScreenPtr pScreen, CARD32 key, RegionPtr clipboxes);

Bool
xf86XVClipVideoHelper(
    BoxPtr dst,
    INT32 *xa,
    INT32 *xb,
    INT32 *ya,
    INT32 *yb,
    RegionPtr reg,
    INT32 width,
    INT32 height
);

void
xf86XVCopyYUV12ToPacked(
    const void *srcy,
    const void *srcv,
    const void *srcu,
    void *dst,
    int srcPitchy,
    int srcPitchuv,
    int dstPitch,
    int h,
    int w
);

void
xf86XVCopyPacked(
    const void *src,
    void *dst,
    int srcPitch,
    int dstPitch,
    int h,
    int w
);

#endif  /* _XF86XV_H_ */
