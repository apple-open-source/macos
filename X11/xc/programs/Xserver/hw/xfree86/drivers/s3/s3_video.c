/*
 *      Copyright 2001  Ani Joshi <ajoshi@unixbox.com>
 * 
 *      XFree86 4.x driver for S3 chipsets
 * 
 * 
 * Permission to use, copy, modify, distribute, and sell this software and its
 * documentation for any purpose is hereby granted without fee, provided that
 * the above copyright notice appear in all copies and that both that copyright
 * notice and this permission notice appear in supporting documentation and
 * that the name of Ani Joshi not be used in advertising or
 * publicity pertaining to distribution of the software without specific,
 * written prior permission.  Ani Joshi makes no representations
 * about the suitability of this software for any purpose.  It is provided
 * "as-is" without express or implied warranty.
 *                 
 * ANI JOSHI DISCLAIMS ALL WARRANTIES WITH REGARD TO THIS SOFTWARE,
 * INCLUDING ALL IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS, IN NO
 * EVENT SHALL ANI JOSHI BE LIABLE FOR ANY SPECIAL, INDIRECT OR
 * CONSEQUENTIAL DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM LOSS OF USE,
 * DATA OR PROFITS, WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE OR OTHER
 * TORTIOUS ACTION, ARISING OUT OF OR IN CONNECTION WITH THE USE OR
 * PERFORMANCE OF THIS SOFTWARE.
 *
 *
 */
/* $XFree86: xc/programs/Xserver/hw/xfree86/drivers/s3/s3_video.c,v 1.2 2001/08/15 11:54:27 tsi Exp $ */

#include "xf86.h"
#include "xf86_OSproc.h"
#include "xf86_ansic.h"

#include "compiler.h"

#include "s3.h"
#include "s3_reg.h"


#define CLIENT_VIDEO_ON	0x04
#define S3_MAX_PORTS	1
#define NUM_FORMATS_OVERLAY 4
#define NUM_FORMATS_TEXTURE 4


static XF86VideoAdaptorPtr S3AllocAdaptor(ScrnInfoPtr pScrn);
static XF86VideoAdaptorPtr S3SetupImageVideoOverlay(ScreenPtr);
static int S3SetPortAttributeOverlay(ScrnInfoPtr, Atom, INT32, pointer);
static int S3GetPortAttributeOverlay(ScrnInfoPtr, Atom ,INT32 *, pointer);
static void S3StopVideo(ScrnInfoPtr, pointer, Bool);
static void S3QueryBestSize(ScrnInfoPtr, Bool, short, short, short, short,
                            unsigned int *, unsigned int *, pointer);
static int  S3PutImage(ScrnInfoPtr, short, short, short, short, short,
                       short, short, short, int, unsigned char*, short,
                       short, Bool, RegionPtr, pointer);
static int  S3QueryImageAttributes(ScrnInfoPtr, int, unsigned short *,
                        	   unsigned short *,  int *, int *);
static void S3ResetVideoOverlay(ScrnInfoPtr);



void S3InitVideo(ScreenPtr pScreen)
{
	ScrnInfoPtr pScrn = xf86Screens[pScreen->myNum];
	S3Ptr pS3 = S3PTR(pScrn);
	XF86VideoAdaptorPtr *adaptors, *newAdaptors = NULL;
	XF86VideoAdaptorPtr newAdaptor = NULL;
	int num_adaptors;

	if (((pScrn->bitsPerPixel == 16) ||
	     (pScrn->bitsPerPixel == 24)) && (pS3->S3NewMMIO)) {
		newAdaptor = S3SetupImageVideoOverlay(pScreen);
		xf86DrvMsg(pScrn->scrnIndex, X_INFO, "Using overlay video\n");
	} else
		return;

	num_adaptors = xf86XVListGenericAdaptors(pScrn, &adaptors);

    	if(newAdaptor) {
        	if(!num_adaptors) {
            		num_adaptors = 1;
            		adaptors = &newAdaptor;
        	} else {
            		newAdaptors =  /* need to free this someplace */
                		xalloc((num_adaptors + 1) * sizeof(XF86VideoAdaptorPtr*));   
            		if(newAdaptors) {
                		memcpy(newAdaptors, adaptors, num_adaptors *
                                       sizeof(XF86VideoAdaptorPtr));
                		newAdaptors[num_adaptors] = newAdaptor;
                		adaptors = newAdaptors;
                		num_adaptors++;
            		} 
        	}
    	}

    	if(num_adaptors)
        	xf86XVScreenInit(pScreen, adaptors, num_adaptors);
            
    	if(newAdaptors)
        	xfree(newAdaptors);
}


/* client libraries expect an encoding */
static XF86VideoEncodingRec DummyEncoding[2] =  
{
 	{   /* overlay limit */
   		0,    
   		"XV_IMAGE",
   		1024, 1024,
   		{1, 1}
 	},
 	{  /* texture limit */
   		0,
   		"XV_IMAGE",
   		2046, 2046,  
   		{1, 1}
 	}
};




static XF86VideoFormatRec Formats[NUM_FORMATS_TEXTURE] =
{
  	/*{15, TrueColor},*/ {16, TrueColor}, {24, TrueColor} /* ,
    	{15, DirectColor}*/, {16, DirectColor}, {24, DirectColor}
};



#define NUM_IMAGES 3
   
static XF86ImageRec Images[NUM_IMAGES] =
{    
  	XVIMAGE_YUY2,
  	/* As in mga, YV12 & I420 are converted to YUY2 on the fly by */
  	/* copy over conversion. */
  	XVIMAGE_YV12,
  	XVIMAGE_I420
        /* XVIMAGE_UYVY */
};



static int S3SetPortAttributeOverlay(ScrnInfoPtr pScrn, Atom attribute,
				     INT32 value, pointer data)
{
	return BadMatch;
}


static int S3GetPortAttributeOverlay(ScrnInfoPtr pScrn, Atom attribute,
				     INT32 *value, pointer data)
{
	return BadMatch;
}



static void S3QueryBestSize(ScrnInfoPtr pScrn, Bool motion, short vid_w,
			    short vid_h, short drw_w, short drw_h,
                            unsigned int *p_w, unsigned int *p_h,
			    pointer data)
{
	*p_w = drw_w;
	*p_h = drw_h;
}



static void S3CopyData(unsigned char *src, unsigned char *dst,
		       int srcPitch, int dstPitch, int h, int w)
{
	w <<= 1;
	while (h--) {
		memcpy(dst, src, w);
		src += srcPitch;
		dst += dstPitch;
	}
}


static void S3CopyMungedData(unsigned char *src1, unsigned char *src2,
			     unsigned char *src3, unsigned char *dst1,
			     int srcPitch, int srcPitch2, int dstPitch,
			     int h, int w)
{
	CARD32 *dst = (CARD32*)dst1;
	int i, j;

   	dstPitch >>= 2;
   	w >>= 1;

   	for(j = 0; j < h; j++) {
        	for(i = 0; i < w; i++) {
            		dst[i] = src1[i << 1] | (src1[(i << 1) + 1] << 16) |
                     		 (src3[i] << 8) | (src2[i] << 24);
        	}
        	dst += dstPitch;
        	src1 += srcPitch;
        	if(j & 1) {
            		src2 += srcPitch2;
            		src3 += srcPitch2;
       	 	}
   	}
}



static void S3ResetVideoOverlay(ScrnInfoPtr pScrn)
{
}


static XF86VideoAdaptorPtr S3AllocAdaptor(ScrnInfoPtr pScrn)
{
	S3Ptr pS3 = S3PTR(pScrn);
	XF86VideoAdaptorPtr adapt;
	S3PortPrivPtr pPriv;
	int i;

    	if(!(adapt = xf86XVAllocateVideoAdaptorRec(pScrn)))
        	return NULL;
   
    	if(!(pPriv = xcalloc(1, sizeof(S3PortPrivRec)  +
        	                (sizeof(DevUnion) * S3_MAX_PORTS))))
    	{
        	xfree(adapt);
        	return NULL;
    	}
    
    	adapt->pPortPrivates = (DevUnion*)(&pPriv[1]);

    	for(i = 0; i < S3_MAX_PORTS; i++)
        	adapt->pPortPrivates[i].val = i;

	pPriv->colorKey = (1 << pScrn->offset.red) | (1 << pScrn->offset.green) |
			  (((pScrn->mask.blue >> pScrn->offset.blue) - 1) << pScrn->offset.blue);

	pPriv->videoStatus = 0;
	pPriv->lastPort = -1;

	pS3->adaptor = adapt;
	pS3->portPrivate = pPriv;

	return adapt;
}


static XF86VideoAdaptorPtr S3SetupImageVideoOverlay(ScreenPtr pScreen)
{
	ScrnInfoPtr pScrn = xf86Screens[pScreen->myNum];
	S3Ptr pS3 = S3PTR(pScrn);
	XF86VideoAdaptorPtr adapt;

	adapt = S3AllocAdaptor(pScrn);

    	adapt->type = XvWindowMask | XvInputMask | XvImageMask;
    	adapt->flags = VIDEO_OVERLAID_IMAGES | VIDEO_CLIP_TO_VIEWPORT;
    	adapt->name = "S3 Backend Scaler";
    	adapt->nEncodings = 1;
    	adapt->pEncodings = &DummyEncoding[0];
    	adapt->nFormats = NUM_FORMATS_OVERLAY;
    	adapt->pFormats = Formats;
    	adapt->nPorts = 1;
    	adapt->pAttributes = NULL /*Attributes*/;
	adapt->nImages = 3;
	adapt->nAttributes = 0;

    	adapt->pImages = Images;
    	adapt->PutVideo = NULL;
    	adapt->PutStill = NULL;
    	adapt->GetVideo = NULL;
    	adapt->GetStill = NULL;
    	adapt->StopVideo = S3StopVideo;  
    	/* Empty Attrib functions - required anyway */
    	adapt->SetPortAttribute = S3SetPortAttributeOverlay;
    	adapt->GetPortAttribute = S3GetPortAttributeOverlay;
    	adapt->QueryBestSize = S3QueryBestSize;
    	adapt->PutImage = S3PutImage;
    	adapt->QueryImageAttributes = S3QueryImageAttributes;
 
    	/* gotta uninit this someplace */
    	REGION_INIT(pScreen, &(pS3->portPrivate->clip), NullBox, 0);
       
    	S3ResetVideoOverlay(pScrn);
       
    	return adapt;
}   



static Bool
RegionsEqual(RegionPtr A, RegionPtr B)
{
    	int *dataA, *dataB; 
    	int num;
        
    	num = REGION_NUM_RECTS(A);
    	if(num != REGION_NUM_RECTS(B))
        	return FALSE;

    	if((A->extents.x1 != B->extents.x1) ||
       	   (A->extents.x2 != B->extents.x2) ||
       	   (A->extents.y1 != B->extents.y1) ||  
       	   (A->extents.y2 != B->extents.y2))
        	return FALSE;
   
    	dataA = (int*)REGION_RECTS(A);
    	dataB = (int*)REGION_RECTS(B);
   
    	while(num--) {
        	if((dataA[0] != dataB[0]) || (dataA[1] != dataB[1]))
           		return FALSE;
        	dataA += 2;
        	dataB += 2;
    	}
  
    	return TRUE;
}



static Bool S3ClipVideo(BoxPtr dst, INT32 *x1, INT32 *x2, INT32 *y1, INT32 *y2,
			RegionPtr reg, INT32 width, INT32 height)
{
    	INT32 vscale, hscale, delta;
    	BoxPtr extents = REGION_EXTENTS(DummyScreen, reg);
    	int diff;

    hscale = ((*x2 - *x1) << 16) / (dst->x2 - dst->x1);
    vscale = ((*y2 - *y1) << 16) / (dst->y2 - dst->y1);
     
    *x1 <<= 16; *x2 <<= 16;
    *y1 <<= 16; *y2 <<= 16;
        
    diff = extents->x1 - dst->x1;
    if(diff > 0) {
        dst->x1 = extents->x1;
        *x1 += diff * hscale;
    }
    diff = dst->x2 - extents->x2;
    if(diff > 0) {
        dst->x2 = extents->x2;
        *x2 -= diff * hscale;   
    }
    diff = extents->y1 - dst->y1;
    if(diff > 0) {
        dst->y1 = extents->y1;
        *y1 += diff * vscale;
    }
    diff = dst->y2 - extents->y2;
    if(diff > 0) {
        dst->y2 = extents->y2;
        *y2 -= diff * vscale;
    }
     
    if(*x1 < 0) {
        diff =  (- *x1 + hscale - 1)/ hscale;
        dst->x1 += diff;
        *x1 += diff * hscale;
    }
    delta = *x2 - (width << 16);
    if(delta > 0) {
        diff = (delta + hscale - 1)/ hscale;
        dst->x2 -= diff;
        *x2 -= diff * hscale;
    }   
    if(*x1 >= *x2) return FALSE; 
    
    if(*y1 < 0) {
        diff =  (- *y1 + vscale - 1)/ vscale;
        dst->y1 += diff;
        *y1 += diff * vscale;
    }
    delta = *y2 - (height << 16);
    if(delta > 0) {
        diff = (delta + vscale - 1)/ vscale;
        dst->y2 -= diff;
        *y2 -= diff * vscale;
    }
    if(*y1 >= *y2) return FALSE;
     
    if((dst->x1 != extents->x1) || (dst->x2 != extents->x2) ||
       (dst->y1 != extents->y1) || (dst->y2 != extents->y2))
    {
        RegionRec clipReg;   
        REGION_INIT(DummyScreen, &clipReg, dst, 1);
        REGION_INTERSECT(DummyScreen, reg, reg, &clipReg);
        REGION_UNINIT(DummyScreen, &clipReg);
    }
    return TRUE;
}


static void S3StopVideo(ScrnInfoPtr pScrn, pointer data, Bool exit)
{
	S3Ptr pS3 = S3PTR(pScrn);
	S3PortPrivPtr pPriv = pS3->portPrivate;

	REGION_EMPTY(pScrn->pScreen, &pPriv->clip);

	if (exit) {
		if (pPriv->videoStatus & CLIENT_VIDEO_ON)
			SET_BLEND_CNTL(0x01000000);

		if (pPriv->area) {
			xf86FreeOffscreenArea(pPriv->area);
	        	pPriv->area = NULL;
		}

		pPriv->videoStatus = 0;
	}
}


static FBAreaPtr S3AllocateMemory(ScrnInfoPtr pScrn, FBAreaPtr area,
				  int numlines)
{
	ScreenPtr pScreen;
	FBAreaPtr new_area;

   if(area) {
        if((area->box.y2 - area->box.y1) >= numlines)
           return area;
            
        if(xf86ResizeOffscreenArea(area, pScrn->displayWidth, numlines))
           return area;
     
        xf86FreeOffscreenArea(area);
   }
      
   pScreen = screenInfo.screens[pScrn->scrnIndex];
  
   new_area = xf86AllocateOffscreenArea(pScreen, pScrn->displayWidth,
                                numlines, 0, NULL, NULL, NULL);

   if(!new_area) {
        int max_w, max_h;

        xf86QueryLargestOffscreenArea(pScreen, &max_w, &max_h, 0,
                        FAVOR_WIDTH_THEN_AREA, PRIORITY_EXTREME);
           
        if((max_w < pScrn->displayWidth) || (max_h < numlines))
           return NULL;

        xf86PurgeUnlockedOffscreenAreas(pScreen);
        new_area = xf86AllocateOffscreenArea(pScreen, pScrn->displayWidth,
                                numlines, 0, NULL, NULL, NULL);
   }
  
   return new_area;  
}



static void S3DisplayVideoOverlay(ScrnInfoPtr pScrn, int id, int offset,
				  short width, short height, int pitch,
				  int x1, int y1, int x2, int y2,
				  BoxPtr dstBox, short src_w, short src_h,
				  short drw_w, short drw_h)
{
	S3Ptr pS3 = S3PTR(pScrn);
	S3PortPrivPtr pPriv = pS3->portPrivate;
	int tmp;

	if (drw_w == src_w)
		tmp = 0;
	else
		tmp = 2;

	SET_SSTREAM_CNTL(tmp << 28 | 0x01000000 |
			 ((((src_w-1)<<1)-(drw_w-1)) & 0xfff));
	SET_SSTRETCH(((src_w - 1) & 0x7ff) | (((src_w-drw_w) & 0x7ff) << 16));
	SET_BLEND_CNTL(0x05000000);
	SET_SSTREAM_FBADDR(offset & 0x3fffff);
	SET_SSTREAM_STRIDE(pitch & 0xfff);

	SET_K1_VSCALE(src_h - 1);
	SET_K2_VSCALE((src_h - drw_h) & 0x7ff);

	SET_DDA_VERT((((~drw_h)-1)) & 0xfff);

	SET_SSTREAM_START(((dstBox->x1 +1) << 16) | (dstBox->y1 +1));
	SET_SSTREAM_WIND(( ((drw_w-1) << 16) | (drw_h ) ) & 0x7ff07ff);

	SET_CHROMA_KEY(0x10000000 |
		       ((pScrn->weight.red-1) << 24) |
		       ((pPriv->colorKey & pScrn->mask.red) >> pScrn->offset.red) <<
			(16 + 8-pScrn->weight.red) |
		       ((pPriv->colorKey & pScrn->mask.green) >> pScrn->offset.green) <<
			(8 + 8-pScrn->weight.green) |
		       ((pPriv->colorKey & pScrn->mask.blue) >> pScrn->offset.blue) <<
			(8-pScrn->weight.blue));
}


static int S3PutImage(ScrnInfoPtr pScrn, short src_x, short src_y,
		  short drw_x, short drw_y, short src_w, short src_h,
		  short drw_w, short drw_h, int id, unsigned char *buf,
		  short width, short height, Bool sync, RegionPtr clipBoxes,
		  pointer data)
{
	S3Ptr pS3 = S3PTR(pScrn);
	S3PortPrivPtr pPriv = pS3->portPrivate;
   	INT32 x1, x2, y1, y2;
   	unsigned char *dst_start; 
   	int pitch, new_h, offset, offset2=0, offset3=0;
   	int srcPitch, srcPitch2=0, dstPitch;
   	int top, left, npixels, nlines;
   	BoxRec dstBox;
   	CARD32 tmp;
   	static int once = 1;
   	static int once2 = 1;

   /* Clip */
   x1 = src_x;
   x2 = src_x + src_w;
   y1 = src_y;
   y2 = src_y + src_h;
   
   dstBox.x1 = drw_x;
   dstBox.x2 = drw_x + drw_w;
   dstBox.y1 = drw_y;
   dstBox.y2 = drw_y + drw_h;
   
   if(!S3ClipVideo(&dstBox, &x1, &x2, &y1, &y2, clipBoxes, width, height))
        return Success;
        
   /*if(!pMga->TexturedVideo) {*/
        dstBox.x1 -= pScrn->frameX0;
        dstBox.x2 -= pScrn->frameX0;
        dstBox.y1 -= pScrn->frameY0;
        dstBox.y2 -= pScrn->frameY0;
        /*}*/
        
   pitch = pScrn->bitsPerPixel * pScrn->displayWidth >> 3;
   dstPitch = ((width << 1) + 15) & ~15;
   new_h = ((dstPitch * height) + pitch - 1) / pitch;
   
   switch(id) {
   case FOURCC_YV12:
   case FOURCC_I420:
        srcPitch = (width + 3) & ~3;
        offset2 = srcPitch * height;
        srcPitch2 = ((width >> 1) + 3) & ~3;
        offset3 = (srcPitch2 * (height >> 1)) + offset2;
        break;
   case FOURCC_UYVY:  
   case FOURCC_YUY2:
   default:
        srcPitch = (width << 1);
        break;
   }
   
   if(!(pPriv->area = S3AllocateMemory(pScrn, pPriv->area, new_h)))
        return BadAlloc;
   
    /* copy data */
    top = y1 >> 16;
    left = (x1 >> 16) & ~1;
    npixels = ((((x2 + 0xffff) >> 16) + 1) & ~1) - left;
    left <<= 1;
        
    offset = pPriv->area->box.y1 * pitch;
    dst_start = pS3->FBBase + offset + left + (top * dstPitch);
    switch(id) {
    case FOURCC_YV12:
    case FOURCC_I420:
        top &= ~1;    
        tmp = ((top >> 1) * srcPitch2) + (left >> 2);
        offset2 += tmp;
        offset3 += tmp;
        if(id == FOURCC_I420) {
           tmp = offset2;
           offset2 = offset3;
           offset3 = tmp;
        }
        nlines = ((((y2 + 0xffff) >> 16) + 1) & ~1) - top;
        S3CopyMungedData(buf + (top * srcPitch) + (left >> 1),
                         buf + offset2, buf + offset3, dst_start,
                         srcPitch, srcPitch2, dstPitch, nlines, npixels);
        once2 = 0;
        break; 
    case FOURCC_UYVY:
    case FOURCC_YUY2:
    default:
        buf += (top * srcPitch) + left;
        nlines = ((y2 + 0xffff) >> 16) - top;
        S3CopyData(buf, dst_start, srcPitch, dstPitch, nlines, npixels);
        once = 0;
        break;
    }
        
    /* update cliplist */
        if(!RegionsEqual(&pPriv->clip, clipBoxes)) {
            REGION_COPY(pScreen, &pPriv->clip, clipBoxes);
            /* draw these */
            (*pS3->pXAA->FillSolidRects)(pScrn, pPriv->colorKey, GXcopy, ~0,
                                         REGION_NUM_RECTS(clipBoxes),
                                         REGION_RECTS(clipBoxes));
        }
                          
        offset += left + (top * dstPitch);
        S3DisplayVideoOverlay(pScrn, id, offset, width, height, dstPitch,
             x1, y1, x2, y2, &dstBox, src_w, src_h, drw_w, drw_h);
    
        pPriv->videoStatus = CLIENT_VIDEO_ON;
    return Success;
}       



static int S3QueryImageAttributes(ScrnInfoPtr pScrn, int id,
				  unsigned short *w, unsigned short *h,
				  int *pitches, int *offsets)
{
	int size, tmp;

    *w = (*w + 1) & ~1;
    if(offsets) offsets[0] = 0;
    
    switch(id) {
    case FOURCC_YV12:
    case FOURCC_I420:
        *h = (*h + 1) & ~1;
        size = (*w + 3) & ~3;
        if(pitches) pitches[0] = size;
        size *= *h;
        if(offsets) offsets[1] = size;
        tmp = ((*w >> 1) + 3) & ~3;
        if(pitches) pitches[1] = pitches[2] = tmp;
        tmp *= (*h >> 1);
        size += tmp;
        if(offsets) offsets[2] = size;
        size += tmp;
        break;
    case FOURCC_UYVY:
    case FOURCC_YUY2:
    default:
        size = *w << 1;
        if(pitches) pitches[0] = size;
        size *= *h;
        break;
    } 
        
    return size;
}



void S3InitStreams(ScrnInfoPtr pScrn, DisplayModePtr mode)
{       
        S3Ptr pS3 = S3PTR(pScrn);
        unsigned int pst_wind = (mode->HDisplay-1) << 16 | (mode->VDisplay);
        
        SET_PSTREAM_CNTL(0x05000000 & 0x77000000);
        SET_CHROMA_KEY(0x00);
        SET_SSTREAM_CNTL(0x03000000);
        SET_BLEND_CNTL(0x01000000);
        SET_PSTREAM_STRIDE((pScrn->displayWidth * 2) & 0x0fff);
        SET_SSTREAM_STRIDE(0x01);
        SET_OPAQUE_OVERLAY(0x40000000);
        SET_PSTREAM_START(0x00010001);
        SET_PSTREAM_WIND(pst_wind & 0x07ff07ff);
        SET_SSTREAM_START(0x07ff07ff);
        SET_SSTREAM_WIND(0x00010001);
}       

