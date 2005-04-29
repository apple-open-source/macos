/*
 * Copyright 2001 by Patrick LERDA
 * Portions Copyright by Stephen Blackheath
 *
 * Permission to use, copy, modify, distribute, and sell this software and its
 * documentation for any purpose is hereby granted without fee, provided that
 * the above copyright notice appear in all copies and that both that
 * copyright notice and this permission notice appear in supporting
 * documentation, and that the name of Patrick LERDA not be used in
 * advertising or publicity pertaining to distribution of the software without
 * specific, written prior permission.  Patrick LERDA makes no representations
 * about the suitability of this software for any purpose.  It is provided
 * "as is" without express or implied warranty.
 *
 * PATRICK LERDA DISCLAIMS ALL WARRANTIES WITH REGARD TO THIS SOFTWARE,
 * INCLUDING ALL IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS, IN NO
 * EVENT SHALL PATRICK LERDA BE LIABLE FOR ANY SPECIAL, INDIRECT OR
 * CONSEQUENTIAL DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM LOSS OF USE,
 * DATA OR PROFITS, WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE OR OTHER
 * TORTIOUS ACTION, ARISING OUT OF OR IN CONNECTION WITH THE USE OR
 * PERFORMANCE OF THIS SOFTWARE.
 *
 * Authors:  Patrick LERDA
 *           with modifications by Stephen Blackheath (Aug 2002)
 *          
 * REVISION HISTORY:
 *   December 2001 - Patrick LERDA's original i740fb project on SourceForge.
 *   27 August 2002 - Patrick's version would run for an hour or two on my
 *     machine, then the screen would go blank (no signal to monitor) and for
 *     some reason I sometimes couldn't even log in through the network.  I had
 *     to re-boot my machine.  This version fixes that and makes a few other
 *     unnecessary tweaks.  I am not certain, but I think the problem is that
 *     Patrick's code was reading a value from XRX register 0xD0, and or'ing
 *     with 0x10.  When I removed this from the main loop, it became reliable.
 *     I suspect the hardware (whether just my cheap clone board only I'm not
 *     sure) was sometimes returning bogus values, which were then programmed
 *     back in - but I never checked this.  This register is related to
 *     powering on or off certain subsystems of the i740 chip, so that might
 *     explain the blank screen.  - Stephen Blackheath
 *   3 September 2002 - Added software scaling in the situation where the
 *     screen size is smaller than the original video size, since scaling down
 *     is not supported by the hardware.  The implementation of this is not
 *     quite complete.
 *   12 September 2002 - Better software scaling with some averaging, giving a
 *     nicer picture.
 */
/* $XFree86: xc/programs/Xserver/hw/xfree86/drivers/i740/i740_video.c,v 1.6 2003/11/10 18:22:21 tsi Exp $ */


/*
 * i740_video.c: i740 Xv driver. Based on the mga Xv driver by Mark Vojkovich.
 */

#include "xf86.h"
#include "xf86_OSproc.h"
#include "xf86Resources.h"
#include "xf86_ansic.h"
#include "compiler.h"
#include "xf86PciInfo.h"
#include "xf86Pci.h"
#include "xf86fbman.h"
#include "regionstr.h"

#include "xf86xv.h"
#include "Xv.h"
#include "xaa.h"
#include "xaalocal.h"
#include "dixstruct.h"
#include "fourcc.h"

#include "vgaHW.h"
#include "i740.h"


#define FOURCC_RV15     0x35315652
#define FOURCC_RV16     0x36315652

/*-*/
#define i740_wc(fb,z,r,v) (pI740->writeControl(pI740,(z),(r),(v)))
#define i740_rc(fb,z,r)   (pI740->readControl(pI740,(z),(r)))
/*-*/

#define OFF_DELAY 	250  /* milliseconds */
#define FREE_DELAY 	15000

#define OFF_TIMER 	0x01
#define FREE_TIMER	0x02
#define CLIENT_VIDEO_ON	0x04

#define TIMER_MASK      (OFF_TIMER | FREE_TIMER)




#define MAKE_ATOM(a) MakeAtom(a, sizeof(a) - 1, TRUE)

static Atom xvBrightness, xvContrast, xvColorKey;



#define IMAGE_MAX_PHY_WIDTH		1024 /*720*/
#define IMAGE_MAX_PHY_HEIGHT	1024 /*576*/


#define IMAGE_MAX_LOG_WIDTH         1024 /*720*/
#define IMAGE_MAX_LOG_HEIGHT        1024 /*576*/


typedef struct {
	CARD32       YBuf0offset;
	CARD32       YBuf1offset;

	unsigned char currentBuf;

	int          brightness;
	int          contrast;

	RegionRec    clip;
	CARD32       colorKey;

	CARD32       videoStatus;
	Time         offTime;
	Time         freeTime;
	FBLinearPtr  linear;

} I740PortPrivRec, *I740PortPrivPtr;        

typedef struct {
    CARD32 OBUF_0Y;
    CARD32 OBUF_1Y;
    CARD32 OBUF_0U;
    CARD32 OBUF_0V;
    CARD32 OBUF_1U;
    CARD32 OBUF_1V;
    CARD32 OV0STRIDE;
    CARD32 YRGB_VPH;
    CARD32 UV_VPH;
    CARD32 HORZ_PH;
    CARD32 INIT_PH;
    CARD32 DWINPOS;
    CARD32 DWINSZ;
    CARD32 SWID;
    CARD32 SWIDQW;
    CARD32 SHEIGHT;
    CARD32 YRGBSCALE;
    CARD32 UVSCALE;
    CARD32 OV0CLRC0;
    CARD32 OV0CLRC1;
    CARD32 DCLRKV;
    CARD32 DCLRKM;
    CARD32 SCLRKVH;
    CARD32 SCLRKVL;
    CARD32 SCLRKM;
    CARD32 OV0CONF;
    CARD32 OV0CMD;
} I740OverlayRegRec, *I740OverlayRegPtr;

#define GET_PORT_PRIVATE(pScrn) \
   (I740PortPrivPtr)((I740PTR(pScrn))->adaptor->pPortPrivates[0].ptr)


/*-----------------------------------------------------------------------------------------*/
__inline__ static void i740fb_overlay_off(ScrnInfoPtr pScrn)
{
  I740Ptr pI740 = I740PTR(pScrn);

    /* 0x3C = COL_KEY_CNTL_1 */
  i740_wc(fb_p, MRX, 0x3C, (i740_rc(fb_p, MRX, 0x3C) |  0x02));
  usleep(50000);
  /*i740_wc(fb_p, XRX, 0xD0, (i740_rc(fb_p, XRX, 0xD0) & ~0x10));
  usleep(50000);*/
  i740_wc(fb_p, XRX, 0xD0, 0x2F);
}

__inline__ static void i740fb_overlay_set(ScrnInfoPtr pScrn, I740PortPrivPtr pPriv, unsigned long mem1,unsigned long mem2,
					  unsigned long isrc_w,unsigned long isrc_h, /* source      image size */
					  unsigned long idst_w,unsigned long idst_h, /* destination image size */
					  unsigned long ddst_x,unsigned long ddst_y, /* destination image pos to display */
					  unsigned long ddst_w,unsigned long ddst_h, /* destination image size to display allows trunc... */
					  unsigned long pitch,int flip,
					  unsigned char vd_mod)
{
  I740Ptr pI740 = I740PTR(pScrn);
  const int f_dbl=pScrn->currentMode->Flags & V_DBLSCAN;

  if(f_dbl) { idst_h*=2; ddst_y*=2; ddst_h*=2;  }

  ddst_x+=pI740->ov_offset_x;
  ddst_y+=pI740->ov_offset_y;

      /* Program the i740 overlay to use the new image dimensions. */

    i740_wc(fb_p, MRX, 0x24, mem1>>16);
    i740_wc(fb_p, MRX, 0x23, mem1>> 8);
    i740_wc(fb_p, MRX, 0x22, mem1>> 0);

    i740_wc(fb_p, MRX, 0x27, mem2>>16);
    i740_wc(fb_p, MRX, 0x26, mem2>> 8);
    i740_wc(fb_p, MRX, 0x25, mem2>> 0);

    i740_wc(fb_p, MRX, 0x28, (( pitch  >>3)-1)    );

    { unsigned short v=ddst_x;          i740_wc(fb_p, MRX, 0x2B, v>> 8); i740_wc(fb_p, MRX, 0x2A, v>> 0); }
    { unsigned short v=ddst_x+ddst_w-1; i740_wc(fb_p, MRX, 0x2D, v>> 8); i740_wc(fb_p, MRX, 0x2C, v>> 0); }
    { unsigned short v=ddst_y;          i740_wc(fb_p, MRX, 0x2F, v>> 8); i740_wc(fb_p, MRX, 0x2E, v>> 0); }
    { unsigned short v=ddst_y+ddst_h-1; i740_wc(fb_p, MRX, 0x31, v>> 8); i740_wc(fb_p, MRX, 0x30, v>> 0); }

    i740_wc(fb_p, MRX, 0x32, (isrc_w<<8)/(idst_w));
    i740_wc(fb_p, MRX, 0x33, (isrc_h<<8)/(idst_h));

    i740_wc(fb_p, MRX, 0x50, 0);
    i740_wc(fb_p, MRX, 0x51, 0);

    i740_wc(fb_p, MRX, 0x1E, ( idst_w > isrc_w ? 0x04 : 0x00 ) | ( idst_h > isrc_h ? 0x08 : 0x00 ));
    i740_wc(fb_p, MRX, 0x1F, ( idst_w > isrc_w ? 0x20 : 0x00 ) | ( (idst_h > isrc_h) && (pitch <= 720*2)  ? 0xC0 : 0x00 ) | (vd_mod & 0x1F));

    /*i740_wc(fb_p, MRX, 0x20, 0);*/

    i740_wc(fb_p, MRX, 0x19, 0x00);

    /*i740_wc(fb_p, XRX, 0xD0, i740_rc(fb_p, XRX, 0xD0) | 0x10 );*/
    i740_wc(fb_p, XRX, 0xD0, 0x3F);
      /* 0x3C = COL_KEY_CNTL_1 */
    i740_wc(fb_p, MRX, 0x3C, 0x05 | 0x02);

  /*i740_wc(fb_p, MRX, 0x20, (flip ? 0x14 : 0x04));*/
  /*i740_wc(fb_p, MRX, 0x20, 0);*/
  /*i740_wc(fb_p, XRX, 0xD0, i740_rc(fb_p, XRX, 0xD0) | 0x10 );*/
  /*i740_wc(fb_p, MRX, 0x19, 0x00);*/
  i740_wc(fb_p, MRX, 0x20, (flip ? 0x34 : 0x24)); /*SB*/
}

__inline__ static void i740fb_colorkey(ScrnInfoPtr pScrn,unsigned long key)
{
  I740Ptr pI740 = I740PTR(pScrn);
  unsigned char r,g,b,rm,gm,bm;

  /*//xf86DrvMsg(pScrn->scrnIndex, X_CONFIG,"colorkey=%04x depth=%d\n",key,pScrn->depth);*/

  switch(pScrn->depth)
    {
    case  4:           r=0x00;            g=0x00;              b=key;               rm=0xFF; gm=0xFF; bm=0xF0;   break;
    case  8:           r=0x00;            g=0x00;              b=key;               rm=0xFF; gm=0xFF; bm=0x00;   break;
    case 15:           r=(key&0x7C00)>>7; g=(key&0x03E0)>>2;   b=(key&0x001F)<<3;   rm=0x07; gm=0x07; bm=0x07;   break;
    case 16:           r=(key&0xF800)>>8; g=(key&0x07E0)>>3;   b=(key&0x001F)<<3;   rm=0x07; gm=0x03; bm=0x07;   break;
    default:  /*24*/   r=(key&0xFF00)>>8; g=(key&0xFF00)>>3;   b=(key&0xFF00)<<3;   rm=0x00; gm=0x00; bm=0x00;   break;
    }

  i740_wc(fb_p, MRX, 0x3D, r);
  i740_wc(fb_p, MRX, 0x3E, g);
  i740_wc(fb_p, MRX, 0x3F, b);

  i740_wc(fb_p, MRX, 0x40, rm);
  i740_wc(fb_p, MRX, 0x41, gm);
  i740_wc(fb_p, MRX, 0x42, bm);
}

/*-----------------------------------------------------------------------------------------*/



static void I740ResetVideo(ScrnInfoPtr pScrn) 
{
  I740Ptr pI740 = I740PTR(pScrn);
  I740PortPrivPtr pPriv = pI740->adaptor->pPortPrivates[0].ptr;

  xf86DrvMsg(pScrn->scrnIndex, X_CONFIG, "I740ResetVideo entered\n");  /* ### */

  /*
   * Enable destination color keying
   */

  i740fb_colorkey(pScrn,pPriv->colorKey);
}



/* I740ClipVideo -  

   Takes the dst box in standard X BoxRec form (top and left
   edges inclusive, bottom and right exclusive).  The new dst
   box is returned.  The source boundaries are given (x1, y1 
   inclusive, x2, y2 exclusive) and returned are the new source 
   boundaries in 16.16 fixed point. 
*/

static void I740ClipVideo(
  BoxPtr dst, 
  INT32 *x1, 
  INT32 *x2, 
  INT32 *y1, 
  INT32 *y2,
  BoxPtr extents,            /* extents of the clip region */
  INT32 width, 
  INT32 height
){
    INT32 vscale, hscale, delta;
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
} 

static void I740StopVideo(ScrnInfoPtr pScrn, pointer data, Bool exit)
{
  I740PortPrivPtr pPriv = (I740PortPrivPtr)data;
  /*xf86DrvMsg(pScrn->scrnIndex, X_CONFIG, "I740StopVideo entered %p %d\n", (void*)data, (int)exit);*/  /* ### */

  REGION_EMPTY(pScrn->pScreen, &pPriv->clip);   

  if(exit)
    {
      if(pPriv->videoStatus & CLIENT_VIDEO_ON)
	i740fb_overlay_off(pScrn);

      if(pPriv->linear)
	{
	  xf86FreeOffscreenLinear(pPriv->linear);
	  pPriv->linear = NULL;
	}
      pPriv->videoStatus = 0;
    }
  else
    {
      if(pPriv->videoStatus & CLIENT_VIDEO_ON)
	{
	  pPriv->videoStatus |= OFF_TIMER;
	  pPriv->offTime = currentTime.milliseconds + OFF_DELAY; 
	}
    }

}

static int I740SetPortAttribute(ScrnInfoPtr pScrn, Atom attribute, INT32 value, pointer data)
{
  I740PortPrivPtr pPriv = (I740PortPrivPtr)data;
  I740Ptr pI740 = I740PTR(pScrn);
  I740OverlayRegPtr overlay = (I740OverlayRegPtr) (pI740->FbBase + pI740->OverlayStart); 

  /*xf86DrvMsg(pScrn->scrnIndex, X_CONFIG, "I740SetPortAttribute entered %d\n", (int) value);*/  /* ### */

  if(attribute == xvBrightness)
    {
      if((value < -128) || (value > 127))
	return BadValue;
      pPriv->brightness = value;
      overlay->OV0CLRC0 = (pPriv->contrast << 8) | (pPriv->brightness & 0xff);
      /*//OVERLAY_UPDATE(pI740->OverlayPhysical);*/
    }
  else
    if(attribute == xvContrast)
      {
	if((value < 0) || (value > 255))
	  return BadValue;
	pPriv->contrast = value;
	overlay->OV0CLRC0 = (pPriv->contrast << 8) | (pPriv->brightness & 0xff);
	/*//OVERLAY_UPDATE(pI740->OverlayPhysical);*/
      }
    else
      if(attribute == xvColorKey)
	{
	  pPriv->colorKey = value;

	  i740fb_colorkey(pScrn,pPriv->colorKey);

	  /*//OVERLAY_UPDATE(pI740->OverlayPhysical);*/
	  REGION_EMPTY(pScrn->pScreen, &pPriv->clip);   
	}
      else
	return BadMatch;

  return Success;
}

static int I740GetPortAttribute(
  ScrnInfoPtr pScrn, 
  Atom attribute,
  INT32 *value, 
  pointer data
){
  I740PortPrivPtr pPriv = (I740PortPrivPtr)data;

  if(attribute == xvBrightness) {
	*value = pPriv->brightness;
  } else
  if(attribute == xvContrast) {
	*value = pPriv->contrast;
  } else
  if(attribute == xvColorKey) {
	*value = pPriv->colorKey;
  } else return BadMatch;

  return Success;
}

static void I740QueryBestSize(
  ScrnInfoPtr pScrn, 
  Bool motion,
  short vid_w, short vid_h, 
  short drw_w, short drw_h, 
  unsigned int *p_w, unsigned int *p_h, 
  pointer data
){
   if(vid_w > (drw_w << 1)) drw_w = vid_w >> 1;
   if(vid_h > (drw_h << 1)) drw_h = vid_h >> 1;
  xf86DrvMsg(pScrn->scrnIndex, X_CONFIG, "I740QueryBestSize entered %d %d %d %d\n", (int) vid_w, (int) vid_h, (int) drw_w, (int) drw_h);  /* ### */
  fprintf(stderr, "fprintf - I740QueryBestSize entered %d %d %d %d\n", (int) vid_w, (int) vid_h, (int) drw_w, (int) drw_h);  /* ### */

  *p_w = drw_w;
  *p_h = drw_h; 
}

static void I740CopyMungedData(ScrnInfoPtr pScrn,
   unsigned char *src1, unsigned char *src2, unsigned char *src3,
   int srcPitch,
   int srcPitch2,
   int dstPitch,
   int h,
   int w,
   long scalex,
   long scaley)
{
  I740Ptr pI740 = I740PTR(pScrn);
  I740PortPrivPtr pPriv = pI740->adaptor->pPortPrivates[0].ptr;
  CARD32 *dst;
  int i, j;

  if (pPriv->currentBuf == 0)
    dst = (CARD32 *)(pI740->FbBase + pPriv->YBuf0offset);
  else
    dst = (CARD32 *)(pI740->FbBase + pPriv->YBuf1offset);
 
  dstPitch >>= 2;
  w >>= 1;

    /* If the space on the screen is smaller than the source image, then we use
     * software scaling to make it smaller. */
  if (scalex > 0x10000 || scaley > 0x10000) {
    int dsth = ((long) h * 0x10000L + scaley - 1) / scaley;
    long halfx = scalex/2;
    long dstj = 0;
    for(j = 0; j < dsth; j++) {
      int dstj_rnd = dstj >> 16;
      unsigned char* src1_ = src1 + (dstj_rnd * srcPitch);
      unsigned char* src2_ = src2 + ((dstj_rnd/2) * srcPitch2);
      unsigned char* src3_ = src3 + ((dstj_rnd/2) * srcPitch2);
      int dstw = ((long) w * 0x10000L + halfx - 1) / scalex;
      long srci = 0;
      for(i = 0; i < dstw; i++) {
	long srci_rnd = srci >> 16;
	long srci2_rnd = (srci + halfx) >> 16;
	dst[i] =
	  (((src1_[srci_rnd << 1] | (src1_[(srci_rnd << 1) + 1] << 16) |
	  (src3_[srci_rnd] << 8) | (src2_[srci_rnd] << 24))) >> 1 & 0x7F7F7F7FL)
	    +
	  (((src1_[srci2_rnd << 1] | (src1_[(srci2_rnd << 1) + 1] << 16) |
	  (src3_[srci2_rnd] << 8) | (src2_[srci2_rnd] << 24))) >> 1 & 0x7F7F7F7FL);
	srci += scalex;
      }
      dst += dstPitch;
      dstj += scaley;
    }
  }
  else {
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
}


__inline__ static void I740CopyPackedData(ScrnInfoPtr pScrn, 
					  unsigned char *buf,
					  int srcPitch,
					  int dstPitch,
					  int top,
					  int left,
					  int h,
					  int w
					  )
{
    I740Ptr pI740 = I740PTR(pScrn);
    I740PortPrivPtr pPriv = pI740->adaptor->pPortPrivates[0].ptr;
    unsigned char *src, *dst;
    
    src = buf + (top*srcPitch) + (left<<1);

    if (pPriv->currentBuf == 0)
	dst = pI740->FbBase + pPriv->YBuf0offset;
    else
	dst = pI740->FbBase + pPriv->YBuf1offset;

    w <<= 1;
    while(h--) {
	memcpy(dst, src, w);
	src += srcPitch;
	dst += dstPitch;
    }
}


__inline__ static void I740DisplayVideo(ScrnInfoPtr pScrn, int id, short width, short height,
					int dstPitch,  /* of chroma for 4:2:0 */
					int x1, int y1, int x2, int y2,
					BoxPtr dstBox,
					short src_w, short src_h, short drw_w, short drw_h,
					unsigned char vd_mod
					)
{
  I740Ptr pI740 = I740PTR(pScrn);
  I740PortPrivPtr pPriv = pI740->adaptor->pPortPrivates[0].ptr;

  i740fb_overlay_set(pScrn,pPriv,pPriv->YBuf0offset,pPriv->YBuf1offset,
		     src_w,src_h,                                        /* source      image size */
		     drw_w,drw_h,                                        /* destination image size */
		     dstBox->x1,dstBox->y1,                              /* destination image pos to display */
		     dstBox->x2 - dstBox->x1,dstBox->y2 - dstBox->y1,    /* destination image size to display allows trunc... */
		     dstPitch,(pPriv->currentBuf != 0),
		     vd_mod);

  i740fb_colorkey(pScrn,pPriv->colorKey);                                /* needed to reset properly the i740 board after switching from framebuffer */
}

static FBLinearPtr I740AllocateMemory(ScrnInfoPtr pScrn, FBLinearPtr linear, int size)
{
  ScreenPtr pScreen;
  FBLinearPtr new_linear;

  if(linear)
    {
      if(linear->size >= size)
	return linear;

      if(xf86ResizeOffscreenLinear(linear, size)) {
        xf86DrvMsg(pScrn->scrnIndex, X_CONFIG, "I740AllocateMemory resized to %d - %p\n", (int) size, (void *)linear);  /* ### */
	return linear;
      }

      xf86DrvMsg(pScrn->scrnIndex, X_CONFIG, "I740AllocateMemory free %p - %d < %d\n", (void *)linear, (int) linear->size, (int) size);  /* ### */
      xf86FreeOffscreenLinear(linear);
    }

  pScreen = screenInfo.screens[pScrn->scrnIndex];

  new_linear = xf86AllocateOffscreenLinear(pScreen, size, 4, NULL, NULL, NULL);

  if(!new_linear)
    {
      int max_size;

      xf86QueryLargestOffscreenLinear(pScreen, &max_size, 4, PRIORITY_EXTREME);

      if(max_size < size) {
	xf86DrvMsg(pScrn->scrnIndex, X_CONFIG, "I740AllocateMemory can't purge %d < %d\n", (int) max_size, (int) size);  /* ### */
	return NULL;
      }

      xf86DrvMsg(pScrn->scrnIndex, X_CONFIG, "I740AllocateMemory purged %d\n", (int) max_size);  /* ### */
      xf86PurgeUnlockedOffscreenAreas(pScreen);
      new_linear = xf86AllocateOffscreenLinear(pScreen, size, 4, 
					       NULL, NULL, NULL);
    } 
  xf86DrvMsg(pScrn->scrnIndex, X_CONFIG, "I740AllocateMemory allocated %d - %p\n", (int) size, (void *)new_linear);  /* ### */

  return new_linear;
}

static int I740PutImage(ScrnInfoPtr pScrn, 
			short src_x, short src_y, short drw_x, short drw_y,
			short src_w, short src_h, short drw_w, short drw_h,
			int id, unsigned char* buf, 
			short width, short height, 
			Bool sync, RegionPtr clipBoxes, pointer data
			)
{
  ScreenPtr pScreen = pScrn->pScreen;
  I740Ptr pI740 = I740PTR(pScrn);
  I740PortPrivPtr pPriv = (I740PortPrivPtr)data;
  INT32 x1, x2, y1, y2;
  int srcPitch, dstPitch, srcPitch2=0;
  int top, left, npixels, nlines, size;
  BoxRec dstBox;
  int offset2=0, offset3=0;

  /*xf86DrvMsg(pScrn->scrnIndex, X_CONFIG, "I740PutImage entered %d %d\n", (int) width, (int) height, id);*/  /* ### */

  /* Clip */
  x1 = src_x;
  x2 = src_x + src_w;
  y1 = src_y;
  y2 = src_y + src_h;

  dstBox.x1 = drw_x;
  dstBox.x2 = drw_x + drw_w;
  dstBox.y1 = drw_y;
  dstBox.y2 = drw_y + drw_h;

  I740ClipVideo(&dstBox, &x1, &x2, &y1, &y2, 
		REGION_EXTENTS(pScreen, clipBoxes), width, height);

  if((x1 >= x2) || (y1 >= y2)) {
    return Success;
  }

  dstBox.x1 -= pScrn->frameX0;
  dstBox.x2 -= pScrn->frameX0;
  dstBox.y1 -= pScrn->frameY0;
  dstBox.y2 -= pScrn->frameY0;

  switch(id)
    {
    case FOURCC_YV12:
    case FOURCC_I420:
      srcPitch = (width + 3) & ~3;
      offset2 = srcPitch * height;
      srcPitch2 = ((width >> 1) + 3) & ~3;
      offset3 = (srcPitch2 * (height >> 1)) + offset2;
      dstPitch = ((width << 1) + 15) & ~15;
      size = dstPitch * height;
      break;
    case FOURCC_UYVY:
    case FOURCC_YUY2:
    default:
      srcPitch = (width << 1);
      dstPitch = (srcPitch + 7) & ~7;
      size = dstPitch * height;
      break;
    }  

  {
    FBLinearPtr new_linear = I740AllocateMemory(pScrn, pPriv->linear, size);
    if (new_linear != pPriv->linear) {
      pPriv->linear = new_linear;
    }
  }
  if(!pPriv->linear)
    return BadAlloc;

  /* fixup pointers */
  pPriv->YBuf0offset = pPriv->linear->offset*pI740->cpp;
  pPriv->YBuf1offset = (pPriv->linear->offset*pI740->cpp) + size;


#if 0 /*???*/
  /* wait for the last rendered buffer to be flipped in */
  while (((INREG(DOV0STA)&0x00100000)>>20) != pPriv->currentBuf);
#endif

  /* buffer swap */
  pPriv->currentBuf ^= 1;

  /* copy data */
  top = y1 >> 16;
  left = (x1 >> 16) & ~1;
  npixels = ((((x2 + 0xffff) >> 16) + 1) & ~1) - left;

  switch(id) {
  case FOURCC_YV12:
  case FOURCC_I420:
    {
      CARD32 tmp;

      top &= ~1;
      tmp = ((top >> 1) * srcPitch2) + (left >> 1);
      offset2 += tmp;
      offset3 += tmp;

      if(id == FOURCC_I420)
	{
	  tmp = offset2;
	  offset2 = offset3;
	  offset3 = tmp;
	}

      nlines = ((((y2 + 0xffff) >> 16) + 1) & ~1) - top;

        /* If the screen image size is smaller than the video image size, then
	 * we use software scaling to make it smaller.  The hardware takes care
	 * of scaling up - see i740fb_overlay_set.
	 */
      {
	long scalex = 0x10000, scaley = 0x10000;
	if (drw_w < src_w)
	  scalex = (0x10000L * (long) src_w) / (long) drw_w;
	if (drw_h < src_h)
	  scaley = (0x10000L * (long) src_h) / (long) drw_h;

	I740CopyMungedData(pScrn, buf + (top * srcPitch) + left,
			   buf + offset2, buf + offset3,
			   srcPitch, srcPitch2, dstPitch, nlines, npixels, scalex, scaley);
      }
    }
    break;

  case FOURCC_UYVY: case FOURCC_YUY2: default:
    {
      nlines = ((y2 + 0xffff) >> 16) - top;
        /* Add software scaling as above. */
      I740CopyPackedData(pScrn, buf, srcPitch, dstPitch, top, left, nlines, 
			 npixels);
    }
    break;
  }

  /* update cliplist */
  if(!REGION_EQUAL(pScreen, &pPriv->clip, clipBoxes))
    {
      REGION_COPY(pScreen, &pPriv->clip, clipBoxes);
      /* draw these */
      xf86XVFillKeyHelper(pScreen, pPriv->colorKey, clipBoxes);
    }

  {
    unsigned char vd_mod;

    switch(id)
      {
      case FOURCC_RV15: vd_mod=0x09; break;
      case FOURCC_RV16: vd_mod=0x08; break;
      default:          vd_mod=0x00; break;
      }

    I740DisplayVideo(pScrn, id, width, height, dstPitch, 
		     x1, y1, x2, y2, &dstBox, src_w, src_h, drw_w, drw_h,
		     vd_mod);
  }

  pPriv->videoStatus = CLIENT_VIDEO_ON;

  return Success;
}


static int I740QueryImageAttributes(ScrnInfoPtr pScrn, int id, unsigned short *w, unsigned short *h, int *pitches, int *offsets )
{
  int size,tmp;

  if(*w > IMAGE_MAX_PHY_WIDTH) *w = IMAGE_MAX_PHY_WIDTH;
  if(*h > IMAGE_MAX_PHY_HEIGHT) *h = IMAGE_MAX_PHY_HEIGHT;

  *w = (*w + 1) & ~1;
  if(offsets) offsets[0] = 0;

  switch(id)
    {
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

static void I740BlockHandler(int i, pointer blockData, pointer pTimeout, pointer pReadmask)
{
  ScreenPtr   pScreen = screenInfo.screens[i];
  ScrnInfoPtr pScrn = xf86Screens[i];
  I740Ptr      pI740 = I740PTR(pScrn);
  I740PortPrivPtr pPriv = GET_PORT_PRIVATE(pScrn);

  /*xf86DrvMsg(pScrn->scrnIndex, X_CONFIG, "I740BlockHandler entered\n"); */  /* ### */

  pScreen->BlockHandler = pI740->BlockHandler;
    
  (*pScreen->BlockHandler) (i, blockData, pTimeout, pReadmask);

  pScreen->BlockHandler = I740BlockHandler;

  if(pPriv->videoStatus & TIMER_MASK)
    {
      UpdateCurrentTime();
      if(pPriv->videoStatus & OFF_TIMER)
	{
	  if(pPriv->offTime < currentTime.milliseconds)
	    {
              /*xf86DrvMsg(pScrn->scrnIndex, X_CONFIG, "I740BlockHandler: OFF_TIMER expired\n");*/  /* ### */
	      /* Turn off the overlay */
	      i740fb_overlay_off(pScrn);

	      pPriv->videoStatus = FREE_TIMER;
	      pPriv->freeTime = currentTime.milliseconds + FREE_DELAY;
	    }
	}
      else
	{  /* FREE_TIMER */
	  if(pPriv->freeTime < currentTime.milliseconds)
	    {
	      if(pPriv->linear)
		{
                  /*xf86DrvMsg(pScrn->scrnIndex, X_CONFIG, "I740BlockHandler: FREE_TIMER expired\n");*/  /* ### */
		  xf86FreeOffscreenLinear(pPriv->linear);
		  pPriv->linear = NULL;
		}
	      pPriv->videoStatus = 0;
	    }
        }
    }
}


/***************************************************************************
 * Offscreen Images
 ***************************************************************************/

typedef struct {
  FBLinearPtr linear;
  Bool isOn;
} OffscreenPrivRec, * OffscreenPrivPtr;

static int I740AllocateSurface(
    ScrnInfoPtr pScrn,
    int id,
    unsigned short w, 	
    unsigned short h,
    XF86SurfacePtr surface
){
    FBLinearPtr linear;
    int pitch, size;
    OffscreenPrivPtr pPriv;
    I740Ptr pI740 = I740PTR(pScrn);

    xf86DrvMsg(pScrn->scrnIndex, X_CONFIG, "I740AllocateSurface entered %d %d\n", (int) w, (int) h);  /* ### */

    if((w > IMAGE_MAX_LOG_WIDTH) || (h > IMAGE_MAX_LOG_HEIGHT))
	return BadAlloc;

    w = (w + 1) & ~1;
    pitch = ((w << 1) + 15) & ~15;
    size = pitch * h;

    if(!(linear = I740AllocateMemory(pScrn, NULL, size)))
	return BadAlloc;

    surface->width = w;
    surface->height = h;

    if(!(surface->pitches = xalloc(sizeof(int)))) {
	xf86FreeOffscreenLinear(linear);
	return BadAlloc;
    }
    if(!(surface->offsets = xalloc(sizeof(int)))) {
	xfree(surface->pitches);
	xf86FreeOffscreenLinear(linear);
	return BadAlloc;
    }
    if(!(pPriv = xalloc(sizeof(OffscreenPrivRec)))) {
	xfree(surface->pitches);
	xfree(surface->offsets);
	xf86FreeOffscreenLinear(linear);
	return BadAlloc;
    }

    pPriv->linear = linear;
    pPriv->isOn = FALSE;

    surface->pScrn = pScrn;
    surface->id = id;   
    surface->pitches[0] = pitch;
    surface->offsets[0] = linear->offset*pI740->cpp;
    surface->devPrivate.ptr = (pointer)pPriv;

    /*//memset(pI740->FbBase + surface->offsets[0],0,size);*/

    return Success;
}

static int I740StopSurface(XF86SurfacePtr surface)
{
  OffscreenPrivPtr pPriv = (OffscreenPrivPtr)surface->devPrivate.ptr;

  if(pPriv->isOn)
    {
      /*//i740fb_overlay_off(pScrn);*/

      pPriv->isOn = FALSE;
    }

  return Success;
}


static int I740FreeSurface(XF86SurfacePtr surface)
{
  OffscreenPrivPtr pPriv = (OffscreenPrivPtr)surface->devPrivate.ptr;

  if(pPriv->isOn)
    {
      I740StopSurface(surface);
    }

  xf86FreeOffscreenLinear(pPriv->linear);
  xfree(surface->pitches);
  xfree(surface->offsets);
  xfree(surface->devPrivate.ptr);

  return Success;
}

static int I740GetSurfaceAttribute(ScrnInfoPtr pScrn, Atom attribute, INT32 *value)
{
  return I740GetPortAttribute(pScrn, attribute, value, 0);
}

static int I740SetSurfaceAttribute(ScrnInfoPtr pScrn, Atom attribute, INT32 value)
{
  return I740SetPortAttribute(pScrn, attribute, value, 0);
}


static int I740DisplaySurface(XF86SurfacePtr surface, short src_x, short src_y, 
			      short drw_x, short drw_y,
			      short src_w, short src_h, 
			      short drw_w, short drw_h,
			      RegionPtr clipBoxes)
{
  OffscreenPrivPtr pPriv = (OffscreenPrivPtr)surface->devPrivate.ptr;
  ScrnInfoPtr pScrn = surface->pScrn;
  I740PortPrivPtr pI740Priv =  GET_PORT_PRIVATE(pScrn);

  INT32 x1, y1, x2, y2;
  BoxRec dstBox;

  xf86DrvMsg(pScrn->scrnIndex, X_CONFIG, "I740DisplaySurface entered\n");  /* ### */
  /*xf86DrvMsg(pScrn->scrnIndex, X_CONFIG,"I740DisplaySurface-----------------------------------\n");*/
  /*//fprintf(stderr, "I740DisplaySurface-----------------------------------\n");*/


  x1 = src_x;
  x2 = src_x + src_w;
  y1 = src_y;
  y2 = src_y + src_h;

  dstBox.x1 = drw_x;
  dstBox.x2 = drw_x + drw_w;
  dstBox.y1 = drw_y;
  dstBox.y2 = drw_y + drw_h;

  I740ClipVideo(&dstBox, &x1, &x2, &y1, &y2,
		REGION_EXTENTS(screenInfo.screens[0], clipBoxes),
		surface->width, surface->height);

  dstBox.x1 -= pScrn->frameX0;
  dstBox.x2 -= pScrn->frameX0;
  dstBox.y1 -= pScrn->frameY0;
  dstBox.y2 -= pScrn->frameY0;

  /* fixup pointers */
  pI740Priv->YBuf0offset = surface->offsets[0];
  pI740Priv->YBuf1offset = pI740Priv->YBuf0offset;

#if 0 /*???*/
  /* wait for the last rendered buffer to be flipped in */
  while (((INREG(DOV0STA)&0x00100000)>>20) != pI740Priv->currentBuf) {
    if(loops == 200000) {
      xf86DrvMsg(pScrn->scrnIndex, X_INFO, "Overlay Lockup\n");
      break;
    }
    loops++;
  }
#endif

  /* buffer swap */
  if (pI740Priv->currentBuf == 0)
    pI740Priv->currentBuf = 1;
  else
    pI740Priv->currentBuf = 0;

  I740ResetVideo(pScrn);

  I740DisplayVideo(pScrn, surface->id, surface->width, surface->height,
		   surface->pitches[0], x1, y1, x2, y2, &dstBox,
		   src_w, src_h, drw_w, drw_h,
		   0x00);

  xf86XVFillKeyHelper(pScrn->pScreen, pI740Priv->colorKey, clipBoxes);

  pPriv->isOn = TRUE;
  /* we've prempted the XvImage stream so set its free timer */
  if(pI740Priv->videoStatus & CLIENT_VIDEO_ON) {
    REGION_EMPTY(pScrn->pScreen, & pI740Priv->clip);   
    UpdateCurrentTime();
    pI740Priv->videoStatus = FREE_TIMER;
    pI740Priv->freeTime = currentTime.milliseconds + FREE_DELAY;
    pScrn->pScreen->BlockHandler = I740BlockHandler;
  }

  return Success;
}

/*-------------------------------------------------------------------------------------------*/
#define NUM_IMAGES (sizeof(i740vid_Images)/sizeof(XF86ImageRec))
static XF86ImageRec i740vid_Images[] =
{
  XVIMAGE_YUY2,
  XVIMAGE_UYVY,
  XVIMAGE_YV12, /* converted to YUV2 while copying */
  XVIMAGE_I420, /* converted to YUV2 while copying */
  {
    FOURCC_RV15,
    XvRGB,
    LSBFirst,
    {'R','V','1','5',
     0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00},
    16,
    XvPacked,
    1,
    15, 0x001F, 0x03E0, 0x7C00,
    0, 0, 0,
    0, 0, 0,
    0, 0, 0,
    {'R','V','B',0,
     0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0},
    XvTopToBottom
  },
  {
    FOURCC_RV16,
    XvRGB,
    LSBFirst,
    {'R','V','1','6',
     0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00},
    16,
    XvPacked,
    1,
    16, 0x001F, 0x07E0, 0xF800,
    0, 0, 0,
    0, 0, 0,
    0, 0, 0,
    {'R','V','B',0,
     0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0},
    XvTopToBottom
  },
};

#define NUM_ATTRIBUTES (sizeof(i740vid_Attributes)/sizeof(XF86AttributeRec))

static XF86AttributeRec i740vid_Attributes[] =
{
   {XvSettable | XvGettable, 0, (1 << 24) - 1, "XV_COLORKEY"},
   {XvSettable | XvGettable, -128, 127,        "XV_BRIGHTNESS"},
   {XvSettable | XvGettable, 0, 255,           "XV_CONTRAST"}
};


static void I740InitOffscreenImages(ScreenPtr pScreen)
{
  XF86OffscreenImagePtr offscreenImages;
  {
    ScrnInfoPtr pScrn = xf86Screens[pScreen->myNum];
    xf86DrvMsg(pScrn->scrnIndex, X_CONFIG, "I740InitOffscreenImages entered\n");  /* ### */
  }

  /* need to free this someplace */
  if(!(offscreenImages = xalloc(sizeof(XF86OffscreenImageRec))))
    {
      return;
    }

  offscreenImages[0].image          = &i740vid_Images[0];
  offscreenImages[0].flags          = VIDEO_OVERLAID_IMAGES | VIDEO_CLIP_TO_VIEWPORT;
  offscreenImages[0].alloc_surface  = I740AllocateSurface;
  offscreenImages[0].free_surface   = I740FreeSurface;
  offscreenImages[0].display        = I740DisplaySurface;
  offscreenImages[0].stop           = I740StopSurface;
  offscreenImages[0].setAttribute   = I740SetSurfaceAttribute;
  offscreenImages[0].getAttribute   = I740GetSurfaceAttribute;
  offscreenImages[0].max_width      = IMAGE_MAX_LOG_WIDTH;
  offscreenImages[0].max_height     = IMAGE_MAX_LOG_HEIGHT;
  offscreenImages[0].num_attributes = NUM_ATTRIBUTES;
  offscreenImages[0].attributes     = i740vid_Attributes;

  xf86XVRegisterOffscreenImages(pScreen, offscreenImages, 1);
}



static XF86VideoAdaptorPtr I740SetupImageVideo(ScreenPtr pScreen)
{
  /* client libraries expect an encoding */
  static XF86VideoEncodingRec DummyEncoding[1] =
  {
    {
      0,
      "XV_IMAGE",
      IMAGE_MAX_PHY_WIDTH, IMAGE_MAX_PHY_HEIGHT,
      {1, 1}
    }
  };

#define NUM_FORMATS (sizeof(i740vid_Formats)/sizeof(XF86VideoFormatRec))
  static XF86VideoFormatRec i740vid_Formats[] = 
  {
    {15, TrueColor}, {16, TrueColor}, {24, TrueColor},  {8, PseudoColor}
  };

  ScrnInfoPtr pScrn = xf86Screens[pScreen->myNum];
  I740Ptr pI740 = I740PTR(pScrn);
  XF86VideoAdaptorPtr adapt;
  I740PortPrivPtr pPriv;

  /*xf86DrvMsg(pScrn->scrnIndex, X_CONFIG, "I740SetupImageVideo entered\n");*/  /* ### */
  {
    const int n=sizeof(XF86VideoAdaptorRec)+sizeof(I740PortPrivRec)+sizeof(DevUnion);

    if(!(adapt = xcalloc(1, n)))
      return NULL;

    /*//memset(adapt,0,n);*/
  }

  adapt->type = XvWindowMask | XvInputMask | XvImageMask;
  adapt->flags = VIDEO_OVERLAID_IMAGES | VIDEO_CLIP_TO_VIEWPORT;
  adapt->name = "I740 Video Overlay";
  adapt->nEncodings = 1;
  adapt->pEncodings = DummyEncoding;
  adapt->nFormats = NUM_FORMATS;
  adapt->pFormats = i740vid_Formats;
  adapt->nPorts = 1;
  adapt->pPortPrivates = (DevUnion*)(&adapt[1]);

  pPriv = (I740PortPrivPtr)((unsigned char *)(&adapt[1])+sizeof(sizeof(DevUnion)));

  adapt->pPortPrivates[0].ptr = (pointer)(pPriv);
  adapt->pAttributes          = i740vid_Attributes;
  adapt->nImages              = NUM_IMAGES;
  adapt->nAttributes          = NUM_ATTRIBUTES;
  adapt->pImages              = i740vid_Images;
  adapt->PutVideo             = NULL;
  adapt->PutStill             = NULL;
  adapt->GetVideo             = NULL;
  adapt->GetStill             = NULL;
  adapt->StopVideo            = I740StopVideo;
  adapt->SetPortAttribute     = I740SetPortAttribute;
  adapt->GetPortAttribute     = I740GetPortAttribute;
  adapt->QueryBestSize        = I740QueryBestSize;
  adapt->PutImage             = I740PutImage;
  adapt->QueryImageAttributes = I740QueryImageAttributes;

  pPriv->colorKey    = pI740->colorKey & ((1 << pScrn->depth) - 1);
  pPriv->videoStatus = 0;
  pPriv->brightness  = 0;
  pPriv->contrast    = 64;
  pPriv->linear      = NULL;
  pPriv->currentBuf  = 0;

  /* gotta uninit this someplace */
  REGION_NULL(pScreen, &pPriv->clip);

  pI740->adaptor = adapt;

  pI740->BlockHandler = pScreen->BlockHandler;
  pScreen->BlockHandler = I740BlockHandler;

  xvBrightness = MAKE_ATOM("XV_BRIGHTNESS");
  xvContrast   = MAKE_ATOM("XV_CONTRAST");
  xvColorKey   = MAKE_ATOM("XV_COLORKEY");

  I740ResetVideo(pScrn);

  return adapt;
}

void I740InitVideo(ScreenPtr pScreen)
{
  ScrnInfoPtr pScrn = xf86Screens[pScreen->myNum];
  XF86VideoAdaptorPtr newAdaptor = NULL;
  xf86DrvMsg(pScrn->scrnIndex, X_CONFIG, "I740InitVideo entered\n");  /* ### */

#if 0 /*disable!*/
  {
    XF86VideoAdaptorPtr *ptr;
    int n;

    n = xf86XVListGenericAdaptors(pScrn,&ptr);
    if (n) {
      xf86XVScreenInit(pScreen, ptr, n);
    }

    return;
  }
#endif

  {
    newAdaptor = I740SetupImageVideo(pScreen);
    I740InitOffscreenImages(pScreen);
  }

  {
    XF86VideoAdaptorPtr *adaptors_oldptrs, *adaptors_newptrs = NULL;
    int num_adaptors;

    num_adaptors = xf86XVListGenericAdaptors(pScrn, &adaptors_oldptrs);

    if(newAdaptor)
      {
	if(!num_adaptors)
	  {
	    xf86XVScreenInit(pScreen, &newAdaptor, 1);
	  }
	else
	  {
	    if((adaptors_newptrs = xalloc((num_adaptors + 1) * sizeof(XF86VideoAdaptorPtr))))
	      {
		memcpy(adaptors_newptrs, adaptors_oldptrs, num_adaptors * sizeof(XF86VideoAdaptorPtr));
		adaptors_newptrs[num_adaptors] = newAdaptor;

		/*//xf86DrvMsg(pScrn->scrnIndex, X_CONFIG,"num--- %d [%d] %08x %08x\n",num_adaptors,num_adaptors * sizeof(XF86VideoAdaptorPtr),*/
		/*//	   adaptors_newptrs[0],adaptors_newptrs[1]);*/


		xf86XVScreenInit(pScreen, adaptors_newptrs, num_adaptors+1);
		xfree(adaptors_newptrs);
	      }
	  }
      }

  }
}
