/* $XFree86: xc/programs/Xserver/hw/xfree86/drivers/via/via_overlay.c,v 1.2 2003/08/27 15:16:11 tsi Exp $ */
/*
 * Copyright 1998-2003 VIA Technologies, Inc. All Rights Reserved.
 * Copyright 2001-2003 S3 Graphics, Inc. All Rights Reserved.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sub license,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice (including the
 * next paragraph) shall be included in all copies or substantial portions
 * of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NON-INFRINGEMENT. IN NO EVENT SHALL
 * VIA, S3 GRAPHICS, AND/OR ITS SUPPLIERS BE LIABLE FOR ANY CLAIM, DAMAGES OR
 * OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE,
 * ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
 * DEALINGS IN THE SOFTWARE.
 */
 
/* I N C L U D E S ---------------------------------------------------------*/

#include "xf86.h"

#include "via.h"
#include "ddmpeg.h"
#include "via_overlay.h"
#include "via_driver.h"



/* F U N C T I O N ----------------------------------------------------------*/

void viaOverlayGetV1Format(VIAPtr pVia, unsigned long dwVideoFlag,LPDDPIXELFORMAT lpDPF, unsigned long * lpdwVidCtl,unsigned long * lpdwHQVCtl )
{

   if (lpDPF->dwFlags & DDPF_FOURCC)
   {
       *lpdwVidCtl |= V1_COLORSPACE_SIGN;
       switch (lpDPF->dwFourCC) {
       case FOURCC_YV12:
            if (dwVideoFlag&VIDEO_HQV_INUSE)
            {
                *lpdwVidCtl |= (V1_YUV422 | V1_SWAP_HW_HQV );
                *lpdwHQVCtl |= HQV_SRC_SW|HQV_YUV420|HQV_ENABLE|HQV_SW_FLIP;
            }
            else
            {
                *lpdwVidCtl |= V1_YCbCr420;
            }
            break;
       case FOURCC_IYUV:
       case FOURCC_VIA:
            if (dwVideoFlag&VIDEO_HQV_INUSE)
            {
                if ((pVia->swov.overlayRecordV1.dwDisplayPictStruct == VIA_PICT_STRUCT_TOP)||
                    (pVia->swov.overlayRecordV1.dwDisplayPictStruct == VIA_PICT_STRUCT_BOTTOM)||
                    (pVia->swov.overlayRecordV1.dwMPEGDeinterlaceMode == VIA_DEINTERLACE_BOB))
                {
                    /*Field Display*/
                    *lpdwVidCtl |= (V1_YUV422 | V1_SWAP_HW_HQV );
                    if (dwVideoFlag&MPEG_USE_HW_FLIP)
                    {
                        *lpdwHQVCtl |= HQV_SRC_MC|HQV_YUV420|HQV_ENABLE |HQV_DEINTERLACE|HQV_FIELD_2_FRAME|HQV_FRAME_2_FIELD;
                        if (pVia->swov.overlayRecordV1.dwMPEGProgressiveMode == VIA_NON_PROGRESSIVE)
                        {
                            *lpdwHQVCtl |= HQV_FIELD_UV;
                        }
                    }
                    else
                    {
                        *lpdwHQVCtl |= HQV_SRC_SW|HQV_YUV420|HQV_ENABLE|HQV_SW_FLIP|HQV_DEINTERLACE|HQV_FIELD_2_FRAME|HQV_FRAME_2_FIELD;
                        if (pVia->swov.overlayRecordV1.dwMPEGProgressiveMode == VIA_NON_PROGRESSIVE)
                        {
                            *lpdwHQVCtl |= HQV_FIELD_UV;
                        }
                    }
                }
                else
                {
                    /*Frame Display*/
                    *lpdwVidCtl |= (V1_YUV422 | V1_SWAP_HW_HQV );
                    if (dwVideoFlag&MPEG_USE_HW_FLIP)
                    {
                        *lpdwHQVCtl |= HQV_SRC_MC|HQV_YUV420|HQV_ENABLE;
                    }
                    else
                    {
                        *lpdwHQVCtl |= HQV_SRC_SW|HQV_YUV420|HQV_ENABLE|HQV_SW_FLIP;
                    }
                }                
            }
            else
            {
                /*Without HQV engine*/
                if (dwVideoFlag&MPEG_USE_HW_FLIP)
                {
                    *lpdwVidCtl |= (V1_YCbCr420 | V1_SWAP_HW_MC );
                    if (((pVia->swov.overlayRecordV1.dwDisplayPictStruct == VIA_PICT_STRUCT_TOP   )||
                         (pVia->swov.overlayRecordV1.dwDisplayPictStruct == VIA_PICT_STRUCT_BOTTOM)||
                         (pVia->swov.overlayRecordV1.dwMPEGDeinterlaceMode == VIA_DEINTERLACE_BOB ))&&
                         (pVia->swov.overlayRecordV1.dwMPEGProgressiveMode == VIA_NON_PROGRESSIVE))
                    {
                        /* CLE bug 
                           *lpdwVidCtl |= V1_SRC_IS_FIELD_PIC;*/
                    }
                }
                else
                {                    
                    if ((pVia->swov.overlayRecordV1.dwDisplayPictStruct == VIA_PICT_STRUCT_TOP)||
                        (pVia->swov.overlayRecordV1.dwDisplayPictStruct == VIA_PICT_STRUCT_BOTTOM)||
                        (pVia->swov.overlayRecordV1.dwMPEGDeinterlaceMode == VIA_DEINTERLACE_BOB))
                    {
                        *lpdwVidCtl |= (V1_YCbCr420 |V1_SWAP_SW | V1_BOB_ENABLE | V1_FRAME_BASE);
                        if (pVia->swov.overlayRecordV1.dwMPEGProgressiveMode == VIA_NON_PROGRESSIVE)
                        {
                            /* CLE bug                             
                               *lpdwVidCtl |= V1_SRC_IS_FIELD_PIC;*/
                        }
                    }
                    else
                    {
                        *lpdwVidCtl |= (V1_YCbCr420 | V1_SWAP_SW );
                    }                
                }
            }
            break;

       case FOURCC_YUY2:
            DBG_DD(ErrorF("DDOver_GetV1Format : FOURCC_YUY2\n"));
            if (dwVideoFlag&VIDEO_HQV_INUSE)
            {
                *lpdwVidCtl |= (V1_YUV422 | V1_SWAP_HW_HQV );
                *lpdwHQVCtl |= HQV_SRC_SW|HQV_YUV422|HQV_ENABLE|HQV_SW_FLIP;
            }
            else
            {
                *lpdwVidCtl |= V1_YUV422;
            }
            break;

       default :
            DBG_DD(ErrorF("DDOver_GetV1Format : Invalid FOURCC format :(0x%lx)in V1!\n", lpDPF->dwFourCC));
            *lpdwVidCtl |= V1_YUV422;
            break;
       }
   }
   else if (lpDPF->dwFlags & DDPF_RGB)
   {
       switch (lpDPF->dwRGBBitCount) {
       case 16:
            if (dwVideoFlag&VIDEO_HQV_INUSE)
            {
                *lpdwVidCtl |= (V1_RGB16 | V1_SWAP_HW_HQV );
                *lpdwHQVCtl |= HQV_SRC_SW|HQV_ENABLE|HQV_SW_FLIP;
                *lpdwHQVCtl |= (lpDPF->dwGBitMask==0x07E0 ?
                            HQV_RGB16 : HQV_RGB15);
            }
            else
            {
                *lpdwVidCtl |= (lpDPF->dwGBitMask==0x07E0 ?
                            V1_RGB16 : V1_RGB15);
            }
           break;
       case 32:
            if (dwVideoFlag&VIDEO_HQV_INUSE)
            {
                *lpdwVidCtl |= (V1_RGB32 | V1_SWAP_HW_HQV );
                *lpdwHQVCtl |= HQV_SRC_SW|HQV_RGB32|HQV_ENABLE|HQV_SW_FLIP;
            }
            else
            {
                *lpdwVidCtl |= V1_RGB32;
            }
           break;

       default :
            DBG_DD(ErrorF("DDOver_GetV1Format : invalid RGB format %ld bits\n",lpDPF->dwRGBBitCount));
            break;
       }
   }
}

void viaOverlayGetV3Format(VIAPtr pVia, unsigned long dwVideoFlag,LPDDPIXELFORMAT lpDPF, unsigned long * lpdwVidCtl,unsigned long * lpdwHQVCtl )
{

   if (lpDPF->dwFlags & DDPF_FOURCC)
   {
       *lpdwVidCtl |= V3_COLORSPACE_SIGN;
       switch (lpDPF->dwFourCC) {
       case FOURCC_YV12:
            if (dwVideoFlag&VIDEO_HQV_INUSE)
            {
                *lpdwVidCtl |= (V3_YUV422 | V3_SWAP_HW_HQV );
                *lpdwHQVCtl |= HQV_SRC_SW|HQV_YUV420|HQV_ENABLE|HQV_SW_FLIP;
            }
            else
            {
                /* *lpdwVidCtl |= V3_YCbCr420;*/
                DBG_DD(ErrorF("DDOver_GetV3Format : Invalid FOURCC format :(0x%lx)in V3!\n", lpDPF->dwFourCC));
            }
            break;
       case FOURCC_IYUV:
       case FOURCC_VIA:
            if (dwVideoFlag&VIDEO_HQV_INUSE)
            {
                if ((pVia->swov.overlayRecordV1.dwDisplayPictStruct == VIA_PICT_STRUCT_TOP)||
                    (pVia->swov.overlayRecordV1.dwDisplayPictStruct == VIA_PICT_STRUCT_BOTTOM)||
                    (pVia->swov.overlayRecordV1.dwMPEGDeinterlaceMode == VIA_DEINTERLACE_BOB))
                {
                    /*Field Display*/
                    *lpdwVidCtl |= (V3_YUV422 | V3_SWAP_HW_HQV );
                    if (dwVideoFlag&MPEG_USE_HW_FLIP)
                    {
                        *lpdwHQVCtl |= HQV_SRC_MC|HQV_YUV420|HQV_ENABLE |HQV_DEINTERLACE|HQV_FIELD_2_FRAME|HQV_FRAME_2_FIELD;
                        if (pVia->swov.overlayRecordV1.dwMPEGProgressiveMode == VIA_NON_PROGRESSIVE)
                        {
                            *lpdwHQVCtl |= HQV_FIELD_UV;
                        }
                    }
                    else
                    {
                        *lpdwHQVCtl |= HQV_SRC_SW|HQV_YUV420|HQV_ENABLE|HQV_SW_FLIP;
                        if (pVia->swov.overlayRecordV1.dwMPEGProgressiveMode == VIA_NON_PROGRESSIVE)
                        {
                            *lpdwHQVCtl |= HQV_FIELD_UV;
                        }
                    }
                }
                else
                {
                    /*Frame Display*/
                    *lpdwVidCtl |= (V3_YUV422 | V3_SWAP_HW_HQV );
                    if (dwVideoFlag&MPEG_USE_HW_FLIP)
                    {
                        *lpdwHQVCtl |= HQV_SRC_MC|HQV_YUV420|HQV_ENABLE;
                    }
                    else
                    {
                        *lpdwHQVCtl |= HQV_SRC_SW|HQV_YUV420|HQV_ENABLE|HQV_SW_FLIP;
                    }
                }                
            }
            else
            {
                DBG_DD(ErrorF("DDOver_GetV3Format : Invalid FOURCC format :(0x%lx)in V3!\n", lpDPF->dwFourCC));
            }
            break;

       case FOURCC_YUY2:
            if (dwVideoFlag&VIDEO_HQV_INUSE)
            {
                *lpdwVidCtl |= (V3_YUV422 | V3_SWAP_HW_HQV );
                *lpdwHQVCtl |= HQV_SRC_SW|HQV_YUV422|HQV_ENABLE|HQV_SW_FLIP;
            }
            else
            {
                *lpdwVidCtl |= V3_YUV422;
            }
            break;

       default :
            DBG_DD(ErrorF("DDOver_GetV3Format : Invalid FOURCC format :(0x%lx)in V3!\n", lpDPF->dwFourCC));
            *lpdwVidCtl |= V3_YUV422;
            break;
       }
   }
   else if (lpDPF->dwFlags & DDPF_RGB) {
       switch (lpDPF->dwRGBBitCount) {
       case 16:
            if (dwVideoFlag&VIDEO_HQV_INUSE)
            {
                *lpdwVidCtl |= (V3_RGB16 | V3_SWAP_HW_HQV );
                *lpdwHQVCtl |= HQV_SRC_SW|HQV_ENABLE|HQV_SW_FLIP;
                *lpdwHQVCtl |= (lpDPF->dwGBitMask==0x07E0 ?
                            HQV_RGB16 : HQV_RGB15);
            }
            else
            {
                *lpdwVidCtl |= (lpDPF->dwGBitMask==0x07E0 ?
                            V3_RGB16 : V3_RGB15);
            }
           break;
       case 32:
            if (dwVideoFlag&VIDEO_HQV_INUSE)
            {
                *lpdwVidCtl |= (V3_RGB32 | V3_SWAP_HW_HQV );
                *lpdwHQVCtl |= HQV_SRC_SW|HQV_RGB32|HQV_ENABLE|HQV_SW_FLIP;
            }
            else
            {
                *lpdwVidCtl |= V3_RGB32;
            }
           break;

       default :
            DBG_DD(ErrorF("DDOver_GetV3Format : invalid RGB format %ld bits\n",lpDPF->dwRGBBitCount));
            break;
       }
   }
}

unsigned long viaOverlayGetSrcStartAddress(VIAPtr pVia, unsigned long dwVideoFlag,RECTL rSrc,RECTL rDest, unsigned long dwSrcPitch,LPDDPIXELFORMAT lpDPF,unsigned long * lpHQVoffset )
{
   unsigned long dwOffset=0;
   unsigned long dwHQVsrcWidth=0,dwHQVdstWidth=0;
   unsigned long dwHQVsrcHeight=0,dwHQVdstHeight=0;
   unsigned long dwHQVSrcTopOffset=0,dwHQVSrcLeftOffset=0;
   
   dwHQVsrcWidth = (unsigned long)(rSrc.right - rSrc.left);
   dwHQVdstWidth = (unsigned long)(rDest.right - rDest.left);
   dwHQVsrcHeight = (unsigned long)(rSrc.bottom - rSrc.top);
   dwHQVdstHeight = (unsigned long)(rDest.bottom - rDest.top);

   if ( (rSrc.left != 0) || (rSrc.top != 0) )
   {

       if (lpDPF->dwFlags & DDPF_FOURCC)
       {
           switch (lpDPF->dwFourCC)
           {
            case FOURCC_YUY2:
            case FOURCC_UYVY:
            case FOURCC_YVYU:
                DBG_DD(ErrorF("GetSrcStartAddress : FOURCC format :(0x%lx)\n", lpDPF->dwFourCC));
                if (dwVideoFlag&VIDEO_HQV_INUSE)
                {
                    dwOffset = (((rSrc.top&~3) * (dwSrcPitch)) +
                               ((rSrc.left << 1)&~31));
                    if (dwHQVsrcHeight>dwHQVdstHeight)
                    {
                        dwHQVSrcTopOffset = ((rSrc.top&~3) * dwHQVdstHeight / dwHQVsrcHeight)* dwSrcPitch;
                    }
                    else
                    {
                        dwHQVSrcTopOffset = (rSrc.top&~3) * (dwSrcPitch);
                    }
                    
                    if (dwHQVsrcWidth>dwHQVdstWidth)
                    {
                        dwHQVSrcLeftOffset = ((rSrc.left << 1)&~31) * dwHQVdstWidth / dwHQVsrcWidth;
                    }
                    else
                    {
                        dwHQVSrcLeftOffset = (rSrc.left << 1)&~31 ;
                    }
                    *lpHQVoffset = dwHQVSrcTopOffset+dwHQVSrcLeftOffset;
                }
                else
                {   
                    dwOffset = ((rSrc.top * dwSrcPitch) +
                               ((rSrc.left << 1)&~15));
                }
                break;

            case FOURCC_IYUV:
            case FOURCC_VIA:
                if (dwVideoFlag&VIDEO_HQV_INUSE)
                {   
                    unsigned long dwDstTop=0, dwDstLeft=0;

                    dwDstTop = ((rSrc.top * pVia->swov.MPGDevice.gdwMPGDstHeight) + (pVia->swov.MPGDevice.dwHeight>>1))/pVia->swov.MPGDevice.dwHeight;
                    dwDstLeft = ((rSrc.left * pVia->swov.MPGDevice.gdwMPGDstWidth) + (pVia->swov.MPGDevice.dwWidth>>1))/pVia->swov.MPGDevice.dwWidth;

                    if (pVia->swov.MPGDevice.gdwMPGDstHeight < pVia->swov.MPGDevice.dwHeight)
                        dwOffset = dwDstTop * (pVia->swov.MPGDevice.dwPitch <<1);
                    else
                        dwOffset = rSrc.top * (pVia->swov.MPGDevice.dwPitch <<1);
                
                    if (pVia->swov.MPGDevice.gdwMPGDstWidth < pVia->swov.MPGDevice.dwWidth)
                        dwOffset += (dwDstLeft<<1)&~31;
                    else
                        dwOffset += (rSrc.left<<1)&~31;
                }
                else
                {
                     dwOffset = ((((rSrc.top&~3) * dwSrcPitch) +
                                rSrc.left)&~31) ;
                     if (rSrc.top >0)
                     {
                        pVia->swov.overlayRecordV1.dwUVoffset = (((((rSrc.top&~3)>>1) * dwSrcPitch) +
                                        rSrc.left)&~31) >>1;
                     }
                     else
                     {
                        pVia->swov.overlayRecordV1.dwUVoffset = dwOffset >>1 ;
                     }
                }
                break;
            case FOURCC_YV12:
                if (dwVideoFlag&VIDEO_HQV_INUSE)
                {
                    dwOffset = (((rSrc.top&~3) * (dwSrcPitch<<1)) +
                                ((rSrc.left << 1)&~31));
                }
                else
                {
                    dwOffset = ((((rSrc.top&~3) * dwSrcPitch) +
                                rSrc.left)&~31) ;
                    if (rSrc.top >0)
                    {
                        pVia->swov.overlayRecordV1.dwUVoffset = (((((rSrc.top&~3)>>1) * dwSrcPitch) +
                                       rSrc.left)&~31) >>1;
                    }
                    else
                    {
                        pVia->swov.overlayRecordV1.dwUVoffset = dwOffset >>1 ;
                    }
                }
                break;

            default:
                DBG_DD(ErrorF("DDOver_GetSrcStartAddress : Invalid FOURCC format :(0x%lx)in V3!\n", lpDPF->dwFourCC));
                break;
            }
       }
       else if (lpDPF->dwFlags & DDPF_RGB)
       {
                if (dwVideoFlag&VIDEO_HQV_INUSE)
                {
                    dwOffset = (((rSrc.top&~3) * (dwSrcPitch<<1)) +
                                ((rSrc.left << 1)&~31));

                    if (dwHQVsrcHeight>dwHQVdstHeight)
                    {
                        dwHQVSrcTopOffset = ((rSrc.top&~3) * dwHQVdstHeight / dwHQVsrcHeight)* dwSrcPitch;
                    }
                    else
                    {
                        dwHQVSrcTopOffset = (rSrc.top&~3) * (dwSrcPitch);
                    }
                    
                    if (dwHQVsrcWidth>dwHQVdstWidth)
                    {
                        dwHQVSrcLeftOffset = ((rSrc.left << 1)&~31) * dwHQVdstWidth / dwHQVsrcWidth;
                    }
                    else
                    {
                        dwHQVSrcLeftOffset = (rSrc.left << 1)&~31 ;
                    }
                    *lpHQVoffset = dwHQVSrcTopOffset+dwHQVSrcLeftOffset;

                }
                else
                {
                    dwOffset = (rSrc.top * dwSrcPitch) +
                           ((rSrc.left * lpDPF->dwRGBBitCount) >> 3);
                }
       }
   }
   else 
   {
        pVia->swov.overlayRecordV1.dwUVoffset = dwOffset = 0;
   }

   return dwOffset;
}

YCBCRREC viaOverlayGetYCbCrStartAddress(unsigned long dwVideoFlag,unsigned long dwStartAddr, unsigned long dwOffset,unsigned long dwUVoffset,unsigned long dwSrcPitch/*lpGbl->lPitch*/,unsigned long dwSrcHeight/*lpGbl->wHeight*/)
{
   YCBCRREC YCbCr;

   /*dwStartAddr =  (unsigned long)lpGbl->fpVidMem - VideoBase;*/
   if (dwVideoFlag&VIDEO_HQV_INUSE)
   {
       YCbCr.dwY   =  dwStartAddr;
       YCbCr.dwCB  =  dwStartAddr + dwSrcPitch * dwSrcHeight ;
       YCbCr.dwCR  =  dwStartAddr + dwSrcPitch * dwSrcHeight
                         + dwSrcPitch * (dwSrcHeight >>2);
   }
   else
   {
       YCbCr.dwY   =  dwStartAddr+dwOffset;
       YCbCr.dwCB  =  dwStartAddr + dwSrcPitch * dwSrcHeight 
                         + dwUVoffset;
       YCbCr.dwCR  =  dwStartAddr + dwSrcPitch * dwSrcHeight
                         + dwSrcPitch * (dwSrcHeight >>2) 
                         + dwUVoffset;
   }
   return YCbCr;
}


unsigned long viaOverlayHQVCalcZoomWidth(VIAPtr pVia, unsigned long dwVideoFlag, unsigned long srcWidth , unsigned long dstWidth,
                           unsigned long * lpzoomCtl, unsigned long * lpminiCtl, unsigned long * lpHQVfilterCtl, unsigned long * lpHQVminiCtl,unsigned long * lpHQVzoomflag)
{
    unsigned long dwTmp;

    if (srcWidth == dstWidth)
    {       
        *lpHQVfilterCtl |= HQV_H_FILTER_DEFAULT;
    }
    else
    {
    
        if (srcWidth < dstWidth) {
            /* zoom in*/
            *lpzoomCtl = srcWidth*0x0800 / dstWidth;
            *lpzoomCtl = (((*lpzoomCtl) & 0x7FF) << 16) | V1_X_ZOOM_ENABLE;
            *lpminiCtl |= ( V1_X_INTERPOLY );  /* set up interpolation*/
            *lpHQVzoomflag = 1;
            *lpHQVfilterCtl |= HQV_H_FILTER_DEFAULT ;
        } else if (srcWidth > dstWidth) {
            /* zoom out*/
            unsigned long srcWidth1;
    
            /*HQV rounding patch
            //dwTmp = dstWidth*0x0800 / srcWidth;*/
            dwTmp = dstWidth*0x0800*0x400 / srcWidth;
            dwTmp = dwTmp / 0x400 + ((dwTmp & 0x3ff)?1:0);

            *lpHQVminiCtl = (dwTmp & 0x7FF)| HQV_H_MINIFY_ENABLE;
    
    
            srcWidth1 = srcWidth >> 1;
            if (srcWidth1 <= dstWidth) {
                *lpminiCtl |= V1_X_DIV_2+V1_X_INTERPOLY;
                if (dwVideoFlag&VIDEO_1_INUSE)
                {
                    pVia->swov.overlayRecordV1.dwFetchAlignment = 3;
                    pVia->swov.overlayRecordV1.dwminifyH = 2;
                }
                else
                {
                    pVia->swov.overlayRecordV3.dwFetchAlignment = 3;
                    pVia->swov.overlayRecordV3.dwminifyH = 2;
                }
                *lpHQVfilterCtl |= HQV_H_TAP4_121;
                /* *lpHQVminiCtl = 0x00000c00;*/
            }
            else {
                srcWidth1 >>= 1;
    
                if (srcWidth1 <= dstWidth) {
                    *lpminiCtl |= V1_X_DIV_4+V1_X_INTERPOLY;
                    if (dwVideoFlag&VIDEO_1_INUSE)
                    {
                        pVia->swov.overlayRecordV1.dwFetchAlignment = 7;
                        pVia->swov.overlayRecordV1.dwminifyH = 4;
                    }
                    else
                    {
                        pVia->swov.overlayRecordV3.dwFetchAlignment = 7;
                        pVia->swov.overlayRecordV3.dwminifyH = 4;
                    }
                    *lpHQVfilterCtl |= HQV_H_TAP4_121;
                    /* *lpHQVminiCtl = 0x00000a00;*/
                }
                else {
                    srcWidth1 >>= 1;
    
                    if (srcWidth1 <= dstWidth) {
                        *lpminiCtl |= V1_X_DIV_8+V1_X_INTERPOLY;
                        if (dwVideoFlag&VIDEO_1_INUSE)
                        {
                            pVia->swov.overlayRecordV1.dwFetchAlignment = 15;
                            pVia->swov.overlayRecordV1.dwminifyH = 8;
                        }
                        else
                        {
                            pVia->swov.overlayRecordV3.dwFetchAlignment = 15;
                            pVia->swov.overlayRecordV3.dwminifyH = 8;
                        }
                        *lpHQVfilterCtl |= HQV_H_TAP8_12221;
                        /* *lpHQVminiCtl = 0x00000900;*/
                    }
                    else {
                        srcWidth1 >>= 1;
    
                        if (srcWidth1 <= dstWidth) {
                            *lpminiCtl |= V1_X_DIV_16+V1_X_INTERPOLY;
                            if (dwVideoFlag&VIDEO_1_INUSE)
                            {
                                pVia->swov.overlayRecordV1.dwFetchAlignment = 15;
                                pVia->swov.overlayRecordV1.dwminifyH = 16;
                            }
                            else
                            {
                                pVia->swov.overlayRecordV3.dwFetchAlignment = 15;
                                pVia->swov.overlayRecordV3.dwminifyH = 16;
                            }
                            *lpHQVfilterCtl |= HQV_H_TAP8_12221;
                            /* *lpHQVminiCtl = 0x00000880;*/
                        }
                        else {
                            /* too small to handle
                            //VIDOutD(V_COMPOSE_MODE, dwCompose);
                            //lpUO->ddRVal = PI_OK;
                            //return DDHAL_DRIVER_NOTHANDLED;*/
                            *lpminiCtl |= V1_X_DIV_16+V1_X_INTERPOLY;
                            if (dwVideoFlag&VIDEO_1_INUSE)
                            {
                                pVia->swov.overlayRecordV1.dwFetchAlignment = 15;
                                pVia->swov.overlayRecordV1.dwminifyH = 16;
                            }
                            else
                            {
                                pVia->swov.overlayRecordV3.dwFetchAlignment = 15;
                                pVia->swov.overlayRecordV3.dwminifyH = 16;
                            }
                            *lpHQVfilterCtl |= HQV_H_TAP8_12221;
                        }
                    }
                }
            }
    
            *lpHQVminiCtl |= HQV_HDEBLOCK_FILTER;

            if (srcWidth1 < dstWidth) {
                /* CLE bug
                   *lpzoomCtl = srcWidth1*0x0800 / dstWidth;*/
                *lpzoomCtl = (srcWidth1-2)*0x0800 / dstWidth;                
                *lpzoomCtl = ((*lpzoomCtl & 0x7FF) << 16) | V1_X_ZOOM_ENABLE;
            }
        }
    }

    return ~PI_ERR;
}

unsigned long viaOverlayHQVCalcZoomHeight (VIAPtr pVia, unsigned long srcHeight,unsigned long dstHeight,
                             unsigned long * lpzoomCtl, unsigned long * lpminiCtl, unsigned long * lpHQVfilterCtl, unsigned long * lpHQVminiCtl,unsigned long * lpHQVzoomflag)
{
    unsigned long dwTmp;
    if (pVia->graphicInfo.dwExpand)
    {
        dstHeight = dstHeight + 1;
    }
    
    if (srcHeight < dstHeight) 
    {
        /* zoom in*/
        dwTmp = srcHeight * 0x0400 / dstHeight;
        *lpzoomCtl |= ((dwTmp & 0x3FF) | V1_Y_ZOOM_ENABLE);
        *lpminiCtl |= (V1_Y_INTERPOLY | V1_YCBCR_INTERPOLY);
        *lpHQVzoomflag = 1;
        *lpHQVfilterCtl |= HQV_V_TAP4_121;
    } 
    else if (srcHeight == dstHeight)
    {       
        *lpHQVfilterCtl |= HQV_V_TAP4_121;
    }
    else if (srcHeight > dstHeight) 
    {
        /* zoom out*/
        unsigned long srcHeight1;
      
        /*HQV rounding patch
        //dwTmp = dstHeight*0x0800 / srcHeight;*/
        dwTmp = dstHeight*0x0800*0x400 / srcHeight;
        dwTmp = dwTmp / 0x400 + ((dwTmp & 0x3ff)?1:0);
        
        *lpHQVminiCtl |= ((dwTmp& 0x7FF)<<16)|HQV_V_MINIFY_ENABLE;
      
        srcHeight1 = srcHeight >> 1;
        if (srcHeight1 <= dstHeight) 
        {
            *lpminiCtl |= V1_Y_DIV_2;
            *lpHQVfilterCtl |= HQV_V_TAP4_121 ;
            /* *lpHQVminiCtl |= 0x0c000000;*/
        }
        else 
        {
            srcHeight1 >>= 1;
            if (srcHeight1 <= dstHeight) 
            {
                *lpminiCtl |= V1_Y_DIV_4;
                *lpHQVfilterCtl |= HQV_V_TAP4_121 ;
                /* *lpHQVminiCtl |= 0x0a000000;*/
            }
            else 
            {
                srcHeight1 >>= 1;
      
                if (srcHeight1 <= dstHeight) 
                {
                    *lpminiCtl |= V1_Y_DIV_8;
                    *lpHQVfilterCtl |= HQV_V_TAP8_12221;
                    /* *lpHQVminiCtl |= 0x09000000;*/
                }
                else 
                {
                    srcHeight1 >>= 1;
      
                    if (srcHeight1 <= dstHeight) 
                    {
                        *lpminiCtl |= V1_Y_DIV_16;
                        *lpHQVfilterCtl |= HQV_V_TAP8_12221;
                        /* *lpHQVminiCtl |= 0x08800000;*/
                    }
                    else 
                    {
                        /* too small to handle
                        //VIDOutD(V_COMPOSE_MODE, dwCompose);
                        //lpUO->ddRVal = PI_OK;
                        //Fixed QAW91013
                        //return DDHAL_DRIVER_NOTHANDLED;*/
                        *lpminiCtl |= V1_Y_DIV_16;
                        *lpHQVfilterCtl |= HQV_V_TAP8_12221;
                    }
                }
            }
        }
      
        *lpHQVminiCtl |= HQV_VDEBLOCK_FILTER;

        if (srcHeight1 < dstHeight) 
        {
            dwTmp = srcHeight1 * 0x0400 / dstHeight;
            *lpzoomCtl |= ((dwTmp & 0x3FF) | V1_Y_ZOOM_ENABLE);
            *lpminiCtl |= ( V1_Y_INTERPOLY|V1_YCBCR_INTERPOLY);
        }
    }
    
    return ~PI_ERR;
}


unsigned long viaOverlayGetFetch(unsigned long dwVideoFlag,LPDDPIXELFORMAT lpDPF,unsigned long dwSrcWidth,unsigned long dwDstWidth,unsigned long dwOriSrcWidth,unsigned long * lpHQVsrcFetch)
{
   unsigned long dwFetch=0;
   
   if (lpDPF->dwFlags & DDPF_FOURCC)
   {
       DBG_DD(ErrorF("DDOver_GetFetch : FourCC= 0x%lx\n", lpDPF->dwFourCC));
       switch (lpDPF->dwFourCC) {
       case FOURCC_YV12:
            if (dwVideoFlag&VIDEO_HQV_INUSE)
            {
                *lpHQVsrcFetch = dwOriSrcWidth;
                if (dwDstWidth >= dwSrcWidth)
                    dwFetch = ((((dwSrcWidth<<1)+V1_FETCHCOUNT_ALIGNMENT)&~V1_FETCHCOUNT_ALIGNMENT) >> V1_FETCHCOUNT_UNIT)+1;
                else
                    dwFetch = ((((dwDstWidth<<1)+V1_FETCHCOUNT_ALIGNMENT)&~V1_FETCHCOUNT_ALIGNMENT) >> V1_FETCHCOUNT_UNIT)+1;            
            }
            else
            {
                /* we fetch one more quadword to avoid get less video data
                //dwFetch = (((dwSrcWidth +V1_FETCHCOUNT_ALIGNMENT)&~V1_FETCHCOUNT_ALIGNMENT)>> V1_FETCHCOUNT_UNIT) +1;*/
                dwFetch = (((dwSrcWidth + 0x1F)&~0x1f)>> V1_FETCHCOUNT_UNIT);
            }
            break;
       case FOURCC_IYUV:
       case FOURCC_VIA:
            if (dwVideoFlag&VIDEO_HQV_INUSE)
            {
                if (dwDstWidth >= dwSrcWidth)
                    dwFetch = ((((dwSrcWidth<<1)+V1_FETCHCOUNT_ALIGNMENT)&~V1_FETCHCOUNT_ALIGNMENT) >> V1_FETCHCOUNT_UNIT)+1;
                else
                    dwFetch = ((((dwDstWidth<<1)+V1_FETCHCOUNT_ALIGNMENT)&~V1_FETCHCOUNT_ALIGNMENT) >> V1_FETCHCOUNT_UNIT)+1;
            
            }
            else
            {
                /*Comment by Vinecnt ,we fetch one more quadword to avoid get less video data*/
                dwFetch = (((dwSrcWidth +V1_FETCHCOUNT_ALIGNMENT)&~V1_FETCHCOUNT_ALIGNMENT)>> V1_FETCHCOUNT_UNIT) +1;
            }
            break;
       case FOURCC_UYVY:
       case FOURCC_YVYU:
       case FOURCC_YUY2:
            if (dwVideoFlag&VIDEO_HQV_INUSE)
            {
                *lpHQVsrcFetch = dwOriSrcWidth<<1;
                if (dwDstWidth >= dwSrcWidth)
                    dwFetch = ((((dwSrcWidth<<1)+V1_FETCHCOUNT_ALIGNMENT)&~V1_FETCHCOUNT_ALIGNMENT) >> V1_FETCHCOUNT_UNIT)+1;
                else
                    dwFetch = ((((dwDstWidth<<1)+V1_FETCHCOUNT_ALIGNMENT)&~V1_FETCHCOUNT_ALIGNMENT) >> V1_FETCHCOUNT_UNIT)+1;
            }
            else
            {
                /*Comment by Vinecnt ,we fetch one more quadword to avoid get less video data*/
                dwFetch = ((((dwSrcWidth<<1)+V1_FETCHCOUNT_ALIGNMENT)&~V1_FETCHCOUNT_ALIGNMENT) >> V1_FETCHCOUNT_UNIT)+1;
            }
            break;
       default :
            DBG_DD(ErrorF("DDOver_GetFetch : Invalid FOURCC format :(0x%lx)in V1!\n", lpDPF->dwFourCC));
            break;
       }
   }
   else if (lpDPF->dwFlags & DDPF_RGB) {
       switch (lpDPF->dwRGBBitCount) {
       case 16:
            if (dwVideoFlag&VIDEO_HQV_INUSE)
            {
                *lpHQVsrcFetch = dwOriSrcWidth<<1;
                if (dwDstWidth >= dwSrcWidth)
                    dwFetch = ((((dwSrcWidth<<1)+V1_FETCHCOUNT_ALIGNMENT)&~V1_FETCHCOUNT_ALIGNMENT) >> V1_FETCHCOUNT_UNIT)+1;
                else
                    dwFetch = ((((dwDstWidth<<1)+V1_FETCHCOUNT_ALIGNMENT)&~V1_FETCHCOUNT_ALIGNMENT) >> V1_FETCHCOUNT_UNIT)+1;
            }
            else
            {
                /*Comment by Vinecnt ,we fetch one more quadword to avoid get less video data*/
                dwFetch = ((((dwSrcWidth<<1)+V1_FETCHCOUNT_ALIGNMENT)&~V1_FETCHCOUNT_ALIGNMENT) >> V1_FETCHCOUNT_UNIT)+1;
            }
           break;
       case 32:
            if (dwVideoFlag&VIDEO_HQV_INUSE)
            {
                *lpHQVsrcFetch = dwOriSrcWidth<<2;
                if (dwDstWidth >= dwSrcWidth)
                    dwFetch = ((((dwSrcWidth<<2)+V1_FETCHCOUNT_ALIGNMENT)&~V1_FETCHCOUNT_ALIGNMENT) >> V1_FETCHCOUNT_UNIT)+1;
                else
                    dwFetch = ((((dwDstWidth<<2)+V1_FETCHCOUNT_ALIGNMENT)&~V1_FETCHCOUNT_ALIGNMENT) >> V1_FETCHCOUNT_UNIT)+1;
            }
            else
            {
                /*Comment by Vinecnt ,we fetch one more quadword to avoid get less video data*/
                dwFetch = ((((dwSrcWidth<<2)+V1_FETCHCOUNT_ALIGNMENT)&~V1_FETCHCOUNT_ALIGNMENT) >> V1_FETCHCOUNT_UNIT)+1;
            }
           break;

       default :
            DBG_DD(ErrorF("DDOver_GetFetch : invalid RGB format %ld bits\n",lpDPF->dwRGBBitCount));
            break;
       }
   }

   /*Fix plannar mode problem*/
   if (dwFetch <4)
   {
        dwFetch = 4;
   }
   return dwFetch;
}

void viaOverlayGetDisplayCount(VIAPtr pVia, unsigned long dwVideoFlag,LPDDPIXELFORMAT lpDPF,unsigned long dwSrcWidth,unsigned long * lpDisplayCountW)
{
    
   /*unsigned long dwFetch=0;*/
   
   if (lpDPF->dwFlags & DDPF_FOURCC)
   {
       switch (lpDPF->dwFourCC) {
       case FOURCC_YV12:
       case FOURCC_UYVY:
       case FOURCC_YVYU:
       case FOURCC_YUY2:
       case FOURCC_IYUV:
       case FOURCC_VIA:
            if (dwVideoFlag&VIDEO_HQV_INUSE)
            {
                *lpDisplayCountW = dwSrcWidth - 1;
            }
            else
            {
                /* *lpDisplayCountW = dwSrcWidth - 2*pVia->swov.overlayRecordV1.dwminifyH;*/
                *lpDisplayCountW = dwSrcWidth - pVia->swov.overlayRecordV1.dwminifyH;
            }
            break;
       default :
            DBG_DD(ErrorF("DDOver_GetDisplayCount : Invalid FOURCC format :(0x%lx)in V1!\n", lpDPF->dwFourCC));
            if (dwVideoFlag&VIDEO_HQV_INUSE)
            {
                *lpDisplayCountW = dwSrcWidth - 1;
            }
            else
            {
                /* *lpDisplayCountW = dwSrcWidth - 2*pVia->swov.overlayRecordV1.dwminifyH;*/
                *lpDisplayCountW = dwSrcWidth - pVia->swov.overlayRecordV1.dwminifyH;
            }
            break;
       }
   }
   else if (lpDPF->dwFlags & DDPF_RGB) {
       switch (lpDPF->dwRGBBitCount) {
       case 16:
       case 32:
            if (dwVideoFlag&VIDEO_HQV_INUSE)
            {
                *lpDisplayCountW = dwSrcWidth - 1;
            }
            else
            {
                *lpDisplayCountW = dwSrcWidth - pVia->swov.overlayRecordV1.dwminifyH;
            }
            break;

       default :
            DBG_DD(ErrorF("DDOver_GetDisplayCount : invalid RGB format %ld bits\n",lpDPF->dwRGBBitCount));
            if (dwVideoFlag&VIDEO_HQV_INUSE)
            {
                *lpDisplayCountW = dwSrcWidth - 1;
            }
            else
            {
                *lpDisplayCountW = dwSrcWidth - pVia->swov.overlayRecordV1.dwminifyH;
            }
            break;
       }
   }   
}

