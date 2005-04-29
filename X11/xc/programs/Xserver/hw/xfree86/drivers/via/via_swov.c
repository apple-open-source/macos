/* $XFree86: xc/programs/Xserver/hw/xfree86/drivers/via/via_swov.c,v 1.11 2004/02/04 04:15:09 dawes Exp $ */
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
 
#include "xf86.h"
#include "xf86_OSproc.h"
#include "xf86_ansic.h"
#include "xf86fbman.h"

#include "via_compose.h"
#include "via_capture.h"
#include "via.h"
#include "ddmpeg.h"
#include "xf86drm.h"

#include "via_overlay.h"
#include "via_driver.h"
#include "via_regrec.h"
#include "via_priv.h"
#include "via_swov.h"
#include "via_common.h"



/* E X T E R N   G L O B A L S ----------------------------------------------*/

extern Bool   XserverIsUp;              /* If Xserver had run(register action) */

/* G L O B A L   V A R I A B L E S ------------------------------------------*/

static unsigned long DispatchVGARevisionID(int rev)
{
   if (rev >= VIA_REVISION_CLECX )
   	return  VIA_REVISION_CLECX;
   else
       return  rev;
}

/*************************************************************************
   Function : VIAVidCreateSurface
   Create overlay surface depend on FOURCC
*************************************************************************/
unsigned long VIAVidCreateSurface(ScrnInfoPtr pScrn, LPDDSURFACEDESC lpDDSurfaceDesc)
{
    VIAPtr  pVia = VIAPTR(pScrn);
    unsigned long   dwWidth, dwHeight, dwPitch=0;
    unsigned long   size;
    unsigned long   dwAddr;
    unsigned long   HQVFBSIZE = 0, SWFBSIZE = 0;
    int     iCount;        /* iCount for clean HQV FB use */
    unsigned char    *lpTmpAddr;    /* for clean HQV FB use */
    VIAHWRec *hwDiff = &pVia->ViaHW;
    unsigned long retCode;
    
    DBG_DD(ErrorF("//VIAVidCreateSurface: \n"));

    if ( lpDDSurfaceDesc == NULL )
        return BadAccess;
        
    ErrorF("Creating %lu surface\n", lpDDSurfaceDesc->dwFourCC);

    switch (lpDDSurfaceDesc->dwFourCC)
    {
       case FOURCC_YUY2 :
            pVia->swov.DPFsrc.dwFlags = DDPF_FOURCC;
            pVia->swov.DPFsrc.dwFourCC = FOURCC_YUY2;

            /* init Video status flag*/
            pVia->swov.gdwVideoFlagSW = VIDEO_HQV_INUSE | SW_USE_HQV | VIDEO_1_INUSE;

            /*write Color Space Conversion param.*/
            switch ( DispatchVGARevisionID(pVia->graphicInfo.RevisionID))
            {
            	case VIA_REVISION_CLECX :
                    VIDOutD(V1_ColorSpaceReg_1, ColorSpaceValue_1_3123C0);
                    VIDOutD(V1_ColorSpaceReg_2, ColorSpaceValue_2_3123C0);
                    
                    DBG_DD(ErrorF("00000284 %08x\n",ColorSpaceValue_1_3123C0));
                    DBG_DD(ErrorF("00000288 %08x\n",ColorSpaceValue_2_3123C0));
                    break;
                
                default :
                    VIDOutD(V1_ColorSpaceReg_2, ColorSpaceValue_2);
                    VIDOutD(V1_ColorSpaceReg_1, ColorSpaceValue_1);

                    DBG_DD(ErrorF("00000288 %08x\n",ColorSpaceValue_2));
                    DBG_DD(ErrorF("00000284 %08x\n",ColorSpaceValue_1));
                    break;
            } 
            

            dwWidth  = lpDDSurfaceDesc->dwWidth;
            dwHeight = lpDDSurfaceDesc->dwHeight;
            dwPitch  = ALIGN_TO_32_BYTES(dwWidth)*2;
            DBG_DD(ErrorF("    srcWidth= %ld \n", dwWidth));
            DBG_DD(ErrorF("    srcHeight= %ld \n", dwHeight));

            SWFBSIZE = dwPitch*dwHeight;    /*YUYV*/

	    VIAFreeLinear(&pVia->swov.SWOVMem);            
            if(Success != (retCode = VIAAllocLinear(&pVia->swov.SWOVMem, pScrn, SWFBSIZE * 2)))
            	return retCode;
            
            dwAddr = pVia->swov.SWOVMem.base;
            /* fill in the SW buffer with 0x8000 (YUY2-black color) to clear FB buffer*/
            lpTmpAddr = pVia->FBBase + dwAddr;

            for(iCount=0;iCount<(SWFBSIZE*2);iCount++)
            {                
                if((iCount%2) == 0)          
                    *lpTmpAddr++=0x00;
                else
                    *lpTmpAddr++=0x80;
            }

            pVia->swov.SWDevice.dwSWPhysicalAddr[0]   = dwAddr;
            pVia->swov.SWDevice.lpSWOverlaySurface[0] = pVia->FBBase+dwAddr;

            pVia->swov.SWDevice.dwSWPhysicalAddr[1] = pVia->swov.SWDevice.dwSWPhysicalAddr[0] + SWFBSIZE;
            pVia->swov.SWDevice.lpSWOverlaySurface[1] = pVia->swov.SWDevice.lpSWOverlaySurface[0] + SWFBSIZE;

            DBG_DD(ErrorF("pVia->swov.SWDevice.dwSWPhysicalAddr[0] %08lx\n", dwAddr));
            DBG_DD(ErrorF("pVia->swov.SWDevice.dwSWPhysicalAddr[1] %08lx\n", dwAddr + SWFBSIZE));

            pVia->swov.SWDevice.gdwSWSrcWidth = dwWidth;
            pVia->swov.SWDevice.gdwSWSrcHeight= dwHeight;
            pVia->swov.SWDevice.dwPitch = dwPitch;

            /* Fill image data in overlay record*/
            pVia->swov.overlayRecordV1.dwV1OriWidth  = dwWidth;
            pVia->swov.overlayRecordV1.dwV1OriHeight = dwHeight;
            pVia->swov.overlayRecordV1.dwV1OriPitch  = dwPitch;
	    if (!(pVia->swov.gdwVideoFlagSW & SW_USE_HQV))
            	break;

        case FOURCC_HQVSW :
            DBG_DD(ErrorF("//Create HQV_SW Surface\n"));
            dwWidth  = pVia->swov.SWDevice.gdwSWSrcWidth;
            dwHeight = pVia->swov.SWDevice.gdwSWSrcHeight;
            dwPitch  = pVia->swov.SWDevice.dwPitch; 

            HQVFBSIZE = dwPitch * dwHeight;
            
            if(hwDiff->dwThreeHQVBuffer)    /* CLE_C0 */
                size = HQVFBSIZE*3;
            else
                size = HQVFBSIZE*2;
            
	    VIAFreeLinear(&pVia->swov.HQVMem);                
            if(Success != (retCode = VIAAllocLinear(&pVia->swov.HQVMem, pScrn, size)))
            	return retCode;
            dwAddr = pVia->swov.HQVMem.base;
/*            dwAddr = pVia->swov.SWOVlinear->offset * depth + SWOVFBSIZE; */

            pVia->swov.overlayRecordV1.dwHQVAddr[0] = dwAddr;
            pVia->swov.overlayRecordV1.dwHQVAddr[1] = dwAddr + HQVFBSIZE;

            if ( hwDiff->dwThreeHQVBuffer )    /*CLE_C0*/
                pVia->swov.overlayRecordV1.dwHQVAddr[2] = dwAddr + 2 * HQVFBSIZE;
                

            /* fill in the HQV buffer with 0x8000 (YUY2-black color) to clear HQV buffers*/
            for(iCount=0;iCount<HQVFBSIZE*2;iCount++)
            {
                lpTmpAddr = pVia->FBBase + dwAddr + iCount;
                if((iCount%2) == 0)
                    *(lpTmpAddr)=0x00;
                else
                    *(lpTmpAddr)=0x80;
            }
            
            VIDOutD(HQV_DST_STARTADDR1,pVia->swov.overlayRecordV1.dwHQVAddr[1]);
            VIDOutD(HQV_DST_STARTADDR0,pVia->swov.overlayRecordV1.dwHQVAddr[0]);

            DBG_DD(ErrorF("000003F0 %08lx\n",VIDInD(HQV_DST_STARTADDR1) ));
            DBG_DD(ErrorF("000003EC %08lx\n",VIDInD(HQV_DST_STARTADDR0) ));
           
            if ( hwDiff->dwThreeHQVBuffer )    /*CLE_C0*/
            {
                VIDOutD(HQV_DST_STARTADDR2,pVia->swov.overlayRecordV1.dwHQVAddr[2]);
                ErrorF("000003FC %08lx\n", (unsigned long)VIDInD(HQV_DST_STARTADDR2) );
            }
           
            break;

       case FOURCC_YV12 :
            DBG_DD(ErrorF("    Create SW YV12 Surface: \n"));
            pVia->swov.DPFsrc.dwFlags = DDPF_FOURCC;
            pVia->swov.DPFsrc.dwFourCC = FOURCC_YV12;

            /* init Video status flag */
            pVia->swov.gdwVideoFlagSW = VIDEO_HQV_INUSE | SW_USE_HQV | VIDEO_1_INUSE;
            /* pVia->swov.gdwVideoFlagSW = VIDEO_1_INUSE; */

            /* if (pVia->swov.gdwVideoFlagTV1 & VIDEO_HQV_INUSE) */
/*
            if (gdwVideoFlagTV0 & VIDEO_HQV_INUSE)
            {
                lpNewVidCtrl->dwHighQVDO = VW_HIGHQVDO_OFF;
                VIADriverProc( HQVCONTROL , lpNewVidCtrl );
            }
*/
            /* write Color Space Conversion param. */
            switch ( DispatchVGARevisionID(pVia->graphicInfo.RevisionID))
            {
            	case VIA_REVISION_CLECX :
                    VIDOutD(V1_ColorSpaceReg_1, ColorSpaceValue_1_3123C0);
                    VIDOutD(V1_ColorSpaceReg_2, ColorSpaceValue_2_3123C0);
                    
                    DBG_DD(ErrorF("00000284 %08x\n",ColorSpaceValue_1_3123C0));
                    DBG_DD(ErrorF("00000288 %08x\n",ColorSpaceValue_2_3123C0));
                    break;
                
                default :
                    VIDOutD(V1_ColorSpaceReg_2, ColorSpaceValue_2);
                    VIDOutD(V1_ColorSpaceReg_1, ColorSpaceValue_1);

                    DBG_DD(ErrorF("00000288 %08x\n",ColorSpaceValue_2));
                    DBG_DD(ErrorF("00000284 %08x\n",ColorSpaceValue_1));
                    break;
            } 


            dwWidth  = lpDDSurfaceDesc->dwWidth;
            dwHeight = lpDDSurfaceDesc->dwHeight;
            dwPitch  = ALIGN_TO_32_BYTES(dwWidth);
            DBG_DD(ErrorF("    srcWidth= %ld \n", dwWidth));
            DBG_DD(ErrorF("    srcHeight= %ld \n", dwHeight));

            SWFBSIZE = dwPitch * dwHeight * 1.5;    /* 1.5 bytes per pixel */

	    VIAFreeLinear(&pVia->swov.SWfbMem);                
	    if(Success != (retCode = VIAAllocLinear(&pVia->swov.SWfbMem, pScrn, 2 * SWFBSIZE)))
	    	return retCode;
	    dwAddr = pVia->swov.SWfbMem.base;
	    
	    DEBUG(ErrorF("dwAddr for SWfbMem is %lu\n", dwAddr));
            /* fill in the SW buffer with 0x8000 (YUY2-black color) to clear FB buffer
             */
            lpTmpAddr = pVia->FBBase + dwAddr;
            for(iCount=0;iCount<(SWFBSIZE*2);iCount++)
            {
                if((iCount%2) == 0)
                    *lpTmpAddr++=0x00;
                else
                    *lpTmpAddr++=0x80;
            }

            pVia->swov.SWDevice.dwSWPhysicalAddr[0]   = dwAddr;
            pVia->swov.SWDevice.dwSWCrPhysicalAddr[0] = pVia->swov.SWDevice.dwSWPhysicalAddr[0]
                                             + (dwPitch*dwHeight);
            pVia->swov.SWDevice.dwSWCbPhysicalAddr[0] = pVia->swov.SWDevice.dwSWCrPhysicalAddr[0]
                                             + ((dwPitch>>1)*(dwHeight>>1));
            pVia->swov.SWDevice.lpSWOverlaySurface[0] = pVia->FBBase+dwAddr;

            pVia->swov.SWDevice.dwSWPhysicalAddr[1] = dwAddr + SWFBSIZE;
            pVia->swov.SWDevice.dwSWCrPhysicalAddr[1] = pVia->swov.SWDevice.dwSWPhysicalAddr[1]
                                             + (dwPitch*dwHeight);
            pVia->swov.SWDevice.dwSWCbPhysicalAddr[1] = pVia->swov.SWDevice.dwSWCrPhysicalAddr[1]
                                             + ((dwPitch>>1)*(dwHeight>>1));
            pVia->swov.SWDevice.lpSWOverlaySurface[1] = pVia->swov.SWDevice.lpSWOverlaySurface[0] + SWFBSIZE;

            DBG_DD(ErrorF("pVia->swov.SWDevice.dwSWPhysicalAddr[0] %08lx\n", dwAddr));
            DBG_DD(ErrorF("pVia->swov.SWDevice.dwSWPhysicalAddr[1] %08lx\n", dwAddr + SWFBSIZE));

            pVia->swov.SWDevice.gdwSWSrcWidth = dwWidth;
            pVia->swov.SWDevice.gdwSWSrcHeight= dwHeight;
            pVia->swov.SWDevice.dwPitch = dwPitch;

            /* Fill image data in overlay record */
            pVia->swov.overlayRecordV1.dwV1OriWidth  = dwWidth;
            pVia->swov.overlayRecordV1.dwV1OriHeight = dwHeight;
            pVia->swov.overlayRecordV1.dwV1OriPitch  = dwPitch;
/*
if (!(pVia->swov.gdwVideoFlagSW & SW_USE_HQV))
            break;
        case FOURCC_HQVSW :
*/
            /* 
             *  if sw video use HQV dwpitch should changed
             */
            DBG_DD(ErrorF("//Create HQV_SW Surface\n"));
/*          pVia->swov.DPFsrc.dwFourCC = FOURCC_YUY2; */
            dwWidth  = pVia->swov.SWDevice.gdwSWSrcWidth;
            dwHeight = pVia->swov.SWDevice.gdwSWSrcHeight;
            dwPitch  = pVia->swov.SWDevice.dwPitch; 

            HQVFBSIZE = dwPitch * dwHeight * 2;

            if ( hwDiff->dwThreeHQVBuffer )    /* CLE_C0 */
                size = HQVFBSIZE * 3;
            else
                size = HQVFBSIZE * 2;

	    VIAFreeLinear(&pVia->swov.HQVMem);                
	    if(Success != (retCode = VIAAllocLinear(&pVia->swov.HQVMem, pScrn, size)))
	    	return retCode;
            
            dwAddr = pVia->swov.HQVMem.base;
	    DEBUG(ErrorF("dwAddr for HQV is %lu\n", dwAddr));
            DBG_DD(ErrorF("HQV dwAddr = 0x%x!!!! \n",dwAddr));

            pVia->swov.overlayRecordV1.dwHQVAddr[0] = dwAddr;
            pVia->swov.overlayRecordV1.dwHQVAddr[1] = dwAddr + HQVFBSIZE;

            if ( hwDiff->dwThreeHQVBuffer )    /*CLE_C0*/
                pVia->swov.overlayRecordV1.dwHQVAddr[2] = dwAddr + 2 * HQVFBSIZE;

            /* fill in the HQV buffer with 0x8000 (YUY2-black color) to clear HQV buffers
             * FIXME: Check if should fill 3 on C0
             */
            for(iCount=0 ; iCount< HQVFBSIZE*2; iCount++)
            {
                lpTmpAddr = pVia->FBBase + dwAddr + iCount;
                if(iCount%2 == 0)
                    *(lpTmpAddr)=0x00;
                else
                    *(lpTmpAddr)=0x80;
            }
            
            VIDOutD(HQV_DST_STARTADDR1,pVia->swov.overlayRecordV1.dwHQVAddr[1]);
            VIDOutD(HQV_DST_STARTADDR0,pVia->swov.overlayRecordV1.dwHQVAddr[0]);

            DBG_DD(ErrorF("000003F0 %08lx\n",VIDInD(HQV_DST_STARTADDR1) ));
            DBG_DD(ErrorF("000003EC %08lx\n",VIDInD(HQV_DST_STARTADDR0) ));
             
            if ( hwDiff->dwThreeHQVBuffer )    /*CLE_C0*/
            {
                VIDOutD(HQV_DST_STARTADDR2,pVia->swov.overlayRecordV1.dwHQVAddr[2]);
                ErrorF("000003FC %08lx\n", (unsigned long)VIDInD(HQV_DST_STARTADDR2) );
            }
             
            break;

        default :
            break;
    }

    return Success;

} /*VIAVidCreateSurface*/

/*************************************************************************
   Function : VIAVidLockSurface
   Lock Surface
*************************************************************************/
unsigned long VIAVidLockSurface(ScrnInfoPtr pScrn, LPDDLOCK lpLock)
{
    VIAPtr  pVia = VIAPTR(pScrn);

    switch (lpLock->dwFourCC)
    {
       case FOURCC_YUY2 :
       case FOURCC_YV12 :
            lpLock->SWDevice = pVia->swov.SWDevice ;
            lpLock->dwPhysicalBase = pVia->FrameBufferBase;

    }
         
    return PI_OK;

} /*VIAVidLockSurface*/

/*************************************************************************
 *  Destroy Surface
*************************************************************************/
unsigned long VIAVidDestroySurface(ScrnInfoPtr pScrn,  LPDDSURFACEDESC lpDDSurfaceDesc)
{
    VIAPtr pVia = VIAPTR(pScrn);

    DBG_DD(ErrorF("//VIAVidDestroySurface: \n"));

    switch (lpDDSurfaceDesc->dwFourCC)
    {
       case FOURCC_YUY2 :
            pVia->swov.DPFsrc.dwFlags = 0;
            pVia->swov.DPFsrc.dwFourCC = 0;
            
            VIAFreeLinear(&pVia->swov.SWOVMem);
            if (!(pVia->swov.gdwVideoFlagSW & SW_USE_HQV))
            {
                pVia->swov.gdwVideoFlagSW = 0;
                break;
            }

       case FOURCC_HQVSW :
       	    VIAFreeLinear(&pVia->swov.HQVMem);
            pVia->swov.gdwVideoFlagSW = 0;
/*            if (pVia->swov.gdwVideoFlagTV1 != 0)
            {
                DBG_DD(ErrorF(" Assign HQV to TV1 \n"));
                lpNewVidCtrl->dwHighQVDO = VW_HIGHQVDO_TV1;
                DriverProc( HQVCONTROL , lpNewVidCtrl );
            }
*/
            break;

       case FOURCC_YV12 :
            pVia->swov.DPFsrc.dwFlags = 0;
            pVia->swov.DPFsrc.dwFourCC = 0;
                    
            VIAFreeLinear(&pVia->swov.SWfbMem);
            VIAFreeLinear(&pVia->swov.HQVMem);
            pVia->swov.gdwVideoFlagSW = 0;
            break;
    }
    DBG_DD(ErrorF("\n//VIAVidDestroySurface : OK!!\n"));
    return PI_OK;

} /*VIAVidDestroySurface*/


/****************************************************************************
 *
 * Upd_Video()
 *
 ***************************************************************************/
static unsigned long Upd_Video(ScrnInfoPtr pScrn, unsigned long dwVideoFlag,unsigned long dwStartAddr,RECTL rSrc,RECTL rDest,unsigned long dwSrcPitch,
                 unsigned long dwOriSrcWidth,unsigned long dwOriSrcHeight,LPDDPIXELFORMAT lpDPFsrc,
                 unsigned long dwDeinterlaceMode,unsigned long dwColorKey,unsigned long dwChromaKey,
                 unsigned long dwKeyLow,unsigned long dwKeyHigh,unsigned long dwChromaLow,unsigned long dwChromaHigh, unsigned long dwFlags)
{
    VIAPtr  pVia = VIAPTR(pScrn);
    unsigned long dwVidCtl=0, dwCompose=(VIDInD(V_COMPOSE_MODE)&~(SELECT_VIDEO_IF_COLOR_KEY|V1_COMMAND_FIRE|V3_COMMAND_FIRE))|V_COMMAND_LOAD_VBI;
    unsigned long srcWidth, srcHeight,dstWidth,dstHeight;
    unsigned long zoomCtl=0, miniCtl=0;
    unsigned long dwHQVCtl=0;
    unsigned long dwHQVfilterCtl=0,dwHQVminiCtl=0;
    unsigned long dwHQVzoomflagH=0,dwHQVzoomflagV=0;
    unsigned long dwHQVsrcWidth=0,dwHQVdstWidth=0;
    unsigned long dwHQVsrcFetch = 0,dwHQVoffset=0;
    unsigned long dwOffset=0,dwFetch=0,dwTmp=0;
    unsigned long dwDisplayCountW=0;
    VIAHWRec *hwDiff = &pVia->ViaHW;

    DBG_DD(ErrorF("// Upd_Video:\n"));
    DBG_DD(ErrorF("Modified rSrc  X (%ld,%ld) Y (%ld,%ld)\n",
                rSrc.left, rSrc.right,rSrc.top, rSrc.bottom));
    DBG_DD(ErrorF("Modified rDest  X (%ld,%ld) Y (%ld,%ld)\n",
                rDest.left, rDest.right,rDest.top, rDest.bottom));

    if (dwVideoFlag & VIDEO_SHOW)    
    {        
        pVia->swov.overlayRecordV1.dwWidth=dstWidth = rDest.right - rDest.left;
        pVia->swov.overlayRecordV1.dwHeight=dstHeight = rDest.bottom - rDest.top;
        srcWidth = (unsigned long)rSrc.right - rSrc.left;
        srcHeight = (unsigned long)rSrc.bottom - rSrc.top;
        DBG_DD(ErrorF("===srcWidth= %ld \n", srcWidth));
        DBG_DD(ErrorF("===srcHeight= %ld \n", srcHeight));

        if (dwVideoFlag & VIDEO_1_INUSE)
        {
            /*=* Modify for C1 FIFO *=*/
            switch ( DispatchVGARevisionID(pVia->graphicInfo.RevisionID) )
            {
            	case VIA_REVISION_CLECX :
                    dwVidCtl = (V1_ENABLE|V1_EXPIRE_NUM_F);
                    break;
                    
                default:
                    /* Overlay source format for V1*/
                    if (pVia->swov.gdwUseExtendedFIFO)
                    {
                        dwVidCtl = (V1_ENABLE|V1_EXPIRE_NUM_A|V1_FIFO_EXTENDED);
                    }
                    else
                    {
                        dwVidCtl = (V1_ENABLE|V1_EXPIRE_NUM);
                    }
                    break;
            }
            
            viaOverlayGetV1Format(pVia, dwVideoFlag,lpDPFsrc,&dwVidCtl,&dwHQVCtl);
            viaOverlayGetV1Format(pVia, dwVideoFlag,lpDPFsrc,&dwVidCtl,&dwHQVCtl);
        }
        else
        {
            /*=* Modify for C1 FIFO *=*/
            switch ( DispatchVGARevisionID(pVia->graphicInfo.RevisionID) )
            {
            	case VIA_REVISION_CLECX :
                    dwVidCtl = (V3_ENABLE|V3_EXPIRE_NUM_F);
                    break;
                    
                default:
                    /* Overlay source format for V1*/
                    dwVidCtl = (V3_ENABLE|V3_EXPIRE_NUM);
                    break;
            }
            
            viaOverlayGetV3Format(pVia, dwVideoFlag,lpDPFsrc,&dwVidCtl,&dwHQVCtl);            
        }

        if ( hwDiff->dwThreeHQVBuffer )    /*CLE_C0*/
        {
            /* HQV support 3 HQV buffer */
            dwHQVCtl &= ~HQV_SW_FLIP;
            dwHQVCtl |= HQV_TRIPLE_BUFF | HQV_FLIP_STATUS; 
        }

        /* Starting address of source and Source offset*/
        dwOffset = viaOverlayGetSrcStartAddress (pVia, dwVideoFlag,rSrc,rDest,dwSrcPitch,lpDPFsrc,&dwHQVoffset );
        DBG_DD(ErrorF("===dwOffset= 0x%lx \n", dwOffset));

        pVia->swov.overlayRecordV1.dwOffset = dwOffset;

        if (pVia->swov.DPFsrc.dwFourCC == FOURCC_YV12)
        {
            YCBCRREC YCbCr;
            if (dwVideoFlag & VIDEO_HQV_INUSE)    
            {
                dwHQVsrcWidth=(unsigned long)rSrc.right - rSrc.left;
                dwHQVdstWidth=(unsigned long)rDest.right - rDest.left;
                if (dwHQVsrcWidth>dwHQVdstWidth)
                {
                    dwOffset = dwOffset * dwHQVdstWidth / dwHQVsrcWidth;
                }
                
                if (dwVideoFlag & VIDEO_1_INUSE)
                {
                    viaMacro_VidREGRec(pVia, VIDREGREC_SAVE_REGISTER, V1_STARTADDR_0, pVia->swov.overlayRecordV1.dwHQVAddr[0]+dwOffset);
                    viaMacro_VidREGRec(pVia, VIDREGREC_SAVE_REGISTER, V1_STARTADDR_1, pVia->swov.overlayRecordV1.dwHQVAddr[1]+dwOffset);
                    
                    if ( hwDiff->dwThreeHQVBuffer )    /*CLE_C0*/
                        viaMacro_VidREGRec(pVia, VIDREGREC_SAVE_REGISTER, V1_STARTADDR_2, pVia->swov.overlayRecordV1.dwHQVAddr[2]+dwOffset);
                }
                else
                {
                    viaMacro_VidREGRec(pVia, VIDREGREC_SAVE_REGISTER, V3_STARTADDR_0, pVia->swov.overlayRecordV1.dwHQVAddr[0]+dwOffset);                    
                    viaMacro_VidREGRec(pVia, VIDREGREC_SAVE_REGISTER, V3_STARTADDR_1, pVia->swov.overlayRecordV1.dwHQVAddr[1]+dwOffset);
                    
                    if ( hwDiff->dwThreeHQVBuffer )    /*CLE_C0*/
                        viaMacro_VidREGRec(pVia, VIDREGREC_SAVE_REGISTER, V3_STARTADDR_2, pVia->swov.overlayRecordV1.dwHQVAddr[2]+dwOffset); 
                }
                YCbCr = viaOverlayGetYCbCrStartAddress(dwVideoFlag,dwStartAddr,pVia->swov.overlayRecordV1.dwOffset,pVia->swov.overlayRecordV1.dwUVoffset,dwSrcPitch,dwOriSrcHeight);
                viaMacro_VidREGRec(pVia, VIDREGREC_SAVE_REGISTER, HQV_SRC_STARTADDR_Y, YCbCr.dwY);
                viaMacro_VidREGRec(pVia, VIDREGREC_SAVE_REGISTER, HQV_SRC_STARTADDR_U, YCbCr.dwCR);
                viaMacro_VidREGRec(pVia, VIDREGREC_SAVE_REGISTER, HQV_SRC_STARTADDR_V, YCbCr.dwCB);
            }
            else
            {
                YCbCr = viaOverlayGetYCbCrStartAddress(dwVideoFlag,dwStartAddr,pVia->swov.overlayRecordV1.dwOffset,pVia->swov.overlayRecordV1.dwUVoffset,dwSrcPitch,dwOriSrcHeight);
                if (dwVideoFlag & VIDEO_1_INUSE)
                {
                    viaMacro_VidREGRec(pVia, VIDREGREC_SAVE_REGISTER, V1_STARTADDR_0, YCbCr.dwY);
                    viaMacro_VidREGRec(pVia, VIDREGREC_SAVE_REGISTER, V1_STARTADDR_CB0, YCbCr.dwCR);
                    viaMacro_VidREGRec(pVia, VIDREGREC_SAVE_REGISTER, V1_STARTADDR_CR0, YCbCr.dwCB);
                }
                else
                {
                    DBG_DD(ErrorF("Upd_Video() : We do not support YV12 with V3!\n"));
                }
            }
        }
        else
        {
            if (dwVideoFlag & VIDEO_HQV_INUSE)    
            {
                dwHQVsrcWidth=(unsigned long)rSrc.right - rSrc.left;
                dwHQVdstWidth=(unsigned long)rDest.right - rDest.left;
                if (dwHQVsrcWidth>dwHQVdstWidth)
                {
                    dwOffset = dwOffset * dwHQVdstWidth / dwHQVsrcWidth;
                }
                
                if (dwVideoFlag & VIDEO_1_INUSE)
                {
                    viaMacro_VidREGRec(pVia, VIDREGREC_SAVE_REGISTER, V1_STARTADDR_0, pVia->swov.overlayRecordV1.dwHQVAddr[0]+dwHQVoffset);
                    viaMacro_VidREGRec(pVia, VIDREGREC_SAVE_REGISTER, V1_STARTADDR_1, pVia->swov.overlayRecordV1.dwHQVAddr[1]+dwHQVoffset);
                    
                    if ( hwDiff->dwThreeHQVBuffer )    /*CLE_C0*/
                        viaMacro_VidREGRec(pVia, VIDREGREC_SAVE_REGISTER, V1_STARTADDR_2, pVia->swov.overlayRecordV1.dwHQVAddr[2]+dwHQVoffset);
                }
                else
                {
                    viaMacro_VidREGRec(pVia, VIDREGREC_SAVE_REGISTER, V3_STARTADDR_0, pVia->swov.overlayRecordV1.dwHQVAddr[0]+dwHQVoffset);
                    viaMacro_VidREGRec(pVia, VIDREGREC_SAVE_REGISTER, V3_STARTADDR_1, pVia->swov.overlayRecordV1.dwHQVAddr[1]+dwHQVoffset);                    
                    
                    if ( hwDiff->dwThreeHQVBuffer )    /*CLE_C0*/
                        viaMacro_VidREGRec(pVia, VIDREGREC_SAVE_REGISTER, V3_STARTADDR_2, pVia->swov.overlayRecordV1.dwHQVAddr[2]+dwHQVoffset);
                }
                viaMacro_VidREGRec(pVia, VIDREGREC_SAVE_REGISTER, HQV_SRC_STARTADDR_Y, dwStartAddr);
            }
            else
            {
                dwStartAddr += dwOffset;

                if (dwVideoFlag & VIDEO_1_INUSE)
                {
                    viaMacro_VidREGRec(pVia, VIDREGREC_SAVE_REGISTER, V1_STARTADDR_0, dwStartAddr);
                }
                else
                {
                    viaMacro_VidREGRec(pVia, VIDREGREC_SAVE_REGISTER, V3_STARTADDR_0, dwStartAddr);                    
                }
            }
        }

        dwFetch = viaOverlayGetFetch(dwVideoFlag,lpDPFsrc,srcWidth,dstWidth,dwOriSrcWidth,&dwHQVsrcFetch);
        DBG_DD(ErrorF("===dwFetch= 0x%lx \n", dwFetch));
/*
        //For DCT450 test-BOB INTERLEAVE
        if ( (dwDeinterlaceMode & DDOVER_INTERLEAVED) && (dwDeinterlaceMode & DDOVER_BOB ) )
        {
            if (dwVideoFlag & VIDEO_HQV_INUSE)    
            {
                dwHQVCtl |= HQV_FIELD_2_FRAME|HQV_FRAME_2_FIELD|HQV_DEINTERLACE;                   
            }
            else
            {
                dwVidCtl |= (V1_BOB_ENABLE | V1_FRAME_BASE);
            }
        }
        else if (dwDeinterlaceMode & DDOVER_BOB )
        {
            if (dwVideoFlag & VIDEO_HQV_INUSE)
            {
                //The HQV source data line count should be two times of the original line count
                dwHQVCtl |= HQV_FIELD_2_FRAME|HQV_DEINTERLACE;
            }
            else
            {
                dwVidCtl |= V1_BOB_ENABLE;                
            }
        }
*/
        if (dwVideoFlag & VIDEO_HQV_INUSE)
        {
            if ( !(dwDeinterlaceMode & DDOVER_INTERLEAVED) && (dwDeinterlaceMode & DDOVER_BOB ) )
            {
                if ( hwDiff->dwHQVFetchByteUnit )     /* CLE_C0 */
                    viaMacro_VidREGRec(pVia, VIDREGREC_SAVE_REGISTER, HQV_SRC_FETCH_LINE, (((dwHQVsrcFetch)-1)<<16)|((dwOriSrcHeight<<1)-1)); 
                else
                    viaMacro_VidREGRec(pVia, VIDREGREC_SAVE_REGISTER, HQV_SRC_FETCH_LINE, (((dwHQVsrcFetch>>3)-1)<<16)|((dwOriSrcHeight<<1)-1));
            }
            else
            {
                if ( hwDiff->dwHQVFetchByteUnit )     /* CLE_C0 */
                    viaMacro_VidREGRec(pVia, VIDREGREC_SAVE_REGISTER, HQV_SRC_FETCH_LINE, (((dwHQVsrcFetch)-1)<<16)|(dwOriSrcHeight-1));
                else
                viaMacro_VidREGRec(pVia, VIDREGREC_SAVE_REGISTER, HQV_SRC_FETCH_LINE, (((dwHQVsrcFetch>>3)-1)<<16)|(dwOriSrcHeight-1));
            }
            if (pVia->swov.DPFsrc.dwFourCC == FOURCC_YV12)
            {
                if (dwVideoFlag & VIDEO_1_INUSE)
                {
                    viaMacro_VidREGRec(pVia, VIDREGREC_SAVE_REGISTER, V1_STRIDE, dwSrcPitch<<1);
                }
                else
                {                  
                    viaMacro_VidREGRec(pVia, VIDREGREC_SAVE_REGISTER, V3_STRIDE, dwSrcPitch<<1);
                }
                viaMacro_VidREGRec(pVia, VIDREGREC_SAVE_REGISTER, HQV_SRC_STRIDE, ((dwSrcPitch>>1)<<16)|dwSrcPitch);
                viaMacro_VidREGRec(pVia, VIDREGREC_SAVE_REGISTER, HQV_DST_STRIDE, (dwSrcPitch<<1));
            }
            else
            {
                if (dwVideoFlag & VIDEO_1_INUSE)
                {
                    viaMacro_VidREGRec(pVia, VIDREGREC_SAVE_REGISTER, V1_STRIDE, dwSrcPitch);
                }
                else
                {                  
                    viaMacro_VidREGRec(pVia, VIDREGREC_SAVE_REGISTER, V3_STRIDE, dwSrcPitch);
                }
                viaMacro_VidREGRec(pVia, VIDREGREC_SAVE_REGISTER, HQV_SRC_STRIDE, dwSrcPitch);
                viaMacro_VidREGRec(pVia, VIDREGREC_SAVE_REGISTER, HQV_DST_STRIDE, dwSrcPitch);
            }
                
        }
        else
        {
            if (dwVideoFlag & VIDEO_1_INUSE)
            {
/*                viaMacro_VidREGRec(pVia, VIDREGREC_SAVE_REGISTER, V1_STRIDE, dwSrcPitch | (dwSrcPitch <<15) );*/
                viaMacro_VidREGRec(pVia, VIDREGREC_SAVE_REGISTER, V1_STRIDE, dwSrcPitch );
            }
            else
            {
                viaMacro_VidREGRec(pVia, VIDREGREC_SAVE_REGISTER, V3_STRIDE, dwSrcPitch | (dwSrcPitch <<15) );                
            }
        }

        DBG_DD(ErrorF("rSrc  X (%ld,%ld) Y (%ld,%ld)\n",
                rSrc.left, rSrc.right,rSrc.top, rSrc.bottom));
        DBG_DD(ErrorF("rDest  X (%ld,%ld) Y (%ld,%ld)\n",
                rDest.left, rDest.right,rDest.top, rDest.bottom));

        /* Destination window key*/

        if (dwVideoFlag & VIDEO_1_INUSE)    
        {
            /*modify for HW DVI limitation,
            //When we enable the CRT and DVI both, then change resolution.
            //If the resolution small than the panel physical size, the video display in Y direction will be cut.
            //So, we need to adjust the Y top and bottom position.                                   */
            if  ((pVia->graphicInfo.dwDVIOn)&&(pVia->graphicInfo.dwExpand))
            {
                 viaMacro_VidREGRec(pVia, VIDREGREC_SAVE_REGISTER,V1_WIN_END_Y,
                                ((rDest.right-1)<<16) + (rDest.bottom*(pVia->graphicInfo.dwPanelHeight)/pVia->graphicInfo.dwHeight));
                 if (rDest.top > 0)
                 {
                      viaMacro_VidREGRec(pVia, VIDREGREC_SAVE_REGISTER, V1_WIN_START_Y,
                                     (rDest.left<<16) + (rDest.top*(pVia->graphicInfo.dwPanelHeight)/pVia->graphicInfo.dwHeight));
                 }
                 else
                 {
                      viaMacro_VidREGRec(pVia, VIDREGREC_SAVE_REGISTER, V1_WIN_START_Y,(rDest.left<<16));
                 }
            }
            else
            {
                 viaMacro_VidREGRec(pVia, VIDREGREC_SAVE_REGISTER,V1_WIN_END_Y, ((rDest.right-1)<<16) + (rDest.bottom-1));
                 if (rDest.top > 0)
                 {
                     viaMacro_VidREGRec(pVia, VIDREGREC_SAVE_REGISTER, V1_WIN_START_Y, (rDest.left<<16) + rDest.top );
                 }
                 else 
                 {
                     viaMacro_VidREGRec(pVia, VIDREGREC_SAVE_REGISTER, V1_WIN_START_Y, (rDest.left<<16));
                 }
	        }
        }
        else
        {
            viaMacro_VidREGRec(pVia, VIDREGREC_SAVE_REGISTER,V3_WIN_END_Y, ((rDest.right-1)<<16) + (rDest.bottom-1));
            if (rDest.top > 0)
            {
                viaMacro_VidREGRec(pVia, VIDREGREC_SAVE_REGISTER, V1_WIN_START_Y, (rDest.left<<16) + rDest.top );
            }
            else 
            {
                viaMacro_VidREGRec(pVia, VIDREGREC_SAVE_REGISTER, V1_WIN_START_Y, (rDest.left<<16));
            }
        }

        dwCompose |= ALWAYS_SELECT_VIDEO;


        /* Setup X zoom factor*/
        pVia->swov.overlayRecordV1.dwFetchAlignment = 0;

        if ( viaOverlayHQVCalcZoomWidth(pVia, dwVideoFlag, srcWidth , dstWidth,
                                  &zoomCtl, &miniCtl, &dwHQVfilterCtl, &dwHQVminiCtl,&dwHQVzoomflagH) == PI_ERR )
        {
            /* too small to handle*/
            dwFetch <<= 20;
            if (dwVideoFlag & VIDEO_1_INUSE)    
            {
                viaMacro_VidREGRec(pVia, VIDREGREC_SAVE_REGISTER, V12_QWORD_PER_LINE, dwFetch);
                viaMacro_VidREGRec(pVia, VIDREGREC_SAVE_REGISTER, V_COMPOSE_MODE , dwCompose|V1_COMMAND_FIRE );
            }
            else
            {
                dwFetch |=(VIDInD(V3_ALPHA_QWORD_PER_LINE)&(~V3_FETCH_COUNT));
                viaMacro_VidREGRec(pVia, VIDREGREC_SAVE_REGISTER, V3_ALPHA_QWORD_PER_LINE, dwFetch);
                viaMacro_VidREGRec(pVia, VIDREGREC_SAVE_REGISTER, V_COMPOSE_MODE , dwCompose|V3_COMMAND_FIRE );
                
            }
            viaMacro_VidREGFlush(pVia);
            return PI_ERR;
        }

        dwFetch <<= 20;
        if (dwVideoFlag & VIDEO_1_INUSE)    
        {
            viaMacro_VidREGRec(pVia, VIDREGREC_SAVE_REGISTER, V12_QWORD_PER_LINE, dwFetch);
        }
        else
        {
            dwFetch |=(VIDInD(V3_ALPHA_QWORD_PER_LINE)&(~V3_FETCH_COUNT));
            viaMacro_VidREGRec(pVia, VIDREGREC_SAVE_REGISTER, V3_ALPHA_QWORD_PER_LINE, dwFetch);
        }

        /*
        // Setup Y zoom factor
        */

        /*For DCT450 test-BOB INTERLEAVE*/
        if ( (dwDeinterlaceMode & DDOVER_INTERLEAVED) && (dwDeinterlaceMode & DDOVER_BOB))
        {
            if (!(dwVideoFlag & VIDEO_HQV_INUSE))
            {
                srcHeight /=2;
                if (dwVideoFlag & VIDEO_1_INUSE)    
                {
                    dwVidCtl |= (V1_BOB_ENABLE | V1_FRAME_BASE);
                }
                else
                {
                    dwVidCtl |= (V3_BOB_ENABLE | V3_FRAME_BASE);                    
                }
            }
            else
            {
                dwHQVCtl |= HQV_FIELD_2_FRAME|HQV_FRAME_2_FIELD|HQV_DEINTERLACE;                 
            }
        }
        else if (dwDeinterlaceMode & DDOVER_BOB )
        {
            if (dwVideoFlag & VIDEO_HQV_INUSE)
            {
                srcHeight <<=1;
                dwHQVCtl |= HQV_FIELD_2_FRAME|HQV_DEINTERLACE;
            }
            else
            {
                if (dwVideoFlag & VIDEO_1_INUSE)    
                {
                    dwVidCtl |= V1_BOB_ENABLE;                
                }
                else
                {
                    dwVidCtl |= V3_BOB_ENABLE;                    
                }
            }
        }

        viaOverlayGetDisplayCount(pVia, dwVideoFlag,lpDPFsrc,srcWidth,&dwDisplayCountW);
        
        if (dwVideoFlag & VIDEO_1_INUSE) 
        {
            viaMacro_VidREGRec(pVia, VIDREGREC_SAVE_REGISTER, V1_SOURCE_HEIGHT, (srcHeight<<16)|dwDisplayCountW);
        }
        else
        {
            viaMacro_VidREGRec(pVia, VIDREGREC_SAVE_REGISTER,V3_SOURCE_WIDTH, dwDisplayCountW);
        }
        
        if ( viaOverlayHQVCalcZoomHeight(pVia, srcHeight,dstHeight,&zoomCtl,&miniCtl, &dwHQVfilterCtl, &dwHQVminiCtl ,&dwHQVzoomflagV) == PI_ERR )
        {
            if (dwVideoFlag & VIDEO_1_INUSE)    
            {
                viaMacro_VidREGRec(pVia, VIDREGREC_SAVE_REGISTER, V_COMPOSE_MODE , dwCompose|V1_COMMAND_FIRE );
            }
            else
            {
                viaMacro_VidREGRec(pVia, VIDREGREC_SAVE_REGISTER, V_COMPOSE_MODE , dwCompose|V3_COMMAND_FIRE );                
            }
            
            viaMacro_VidREGFlush(pVia);
            return PI_ERR;
        }

        if (miniCtl & V1_Y_INTERPOLY)
        {
            if (pVia->swov.DPFsrc.dwFourCC == FOURCC_YV12)
            {                
                if (dwVideoFlag & VIDEO_HQV_INUSE)
                {
                    if (dwVideoFlag & VIDEO_1_INUSE)    
                    {
                        /*=* Modify for C1 FIFO *=*/
                        switch ( DispatchVGARevisionID(pVia->graphicInfo.RevisionID) )
                        {
            	            case VIA_REVISION_CLECX :
                                viaMacro_VidREGRec(pVia, VIDREGREC_SAVE_REGISTER, V_FIFO_CONTROL,
                                    V1_FIFO_DEPTH64 | V1_FIFO_PRETHRESHOLD56 | V1_FIFO_THRESHOLD56);
                                break;
                                
                            default:
                                viaMacro_VidREGRec(pVia, VIDREGREC_SAVE_REGISTER, V_FIFO_CONTROL,
                                    V1_FIFO_DEPTH32 | V1_FIFO_PRETHRESHOLD29 | V1_FIFO_THRESHOLD16);
                                break;
                        }
                    }
                    else
                    {
                        /*=* Modify for C1 FIFO *=*/
                        switch ( DispatchVGARevisionID(pVia->graphicInfo.RevisionID) )
                        {
            	            case VIA_REVISION_CLECX :
                                viaMacro_VidREGRec(pVia, VIDREGREC_SAVE_REGISTER, ALPHA_V3_FIFO_CONTROL,
                                    (VIDInD(ALPHA_V3_FIFO_CONTROL)&ALPHA_FIFO_MASK)|V3_FIFO_DEPTH64 | V3_FIFO_THRESHOLD56);                        
                                viaMacro_VidREGRec(pVia, VIDREGREC_SAVE_REGISTER, ALPHA_V3_PREFIFO_CONTROL ,V3_FIFO_PRETHRESHOLD56 |
                                    ( VIDInD(ALPHA_V3_PREFIFO_CONTROL)& (~V3_FIFO_MASK)) );
                                break;
                                
                            default:
                                viaMacro_VidREGRec(pVia, VIDREGREC_SAVE_REGISTER, ALPHA_V3_FIFO_CONTROL,
                                    (VIDInD(ALPHA_V3_FIFO_CONTROL)&ALPHA_FIFO_MASK)|V3_FIFO_DEPTH32 | V3_FIFO_THRESHOLD16);                        
                                viaMacro_VidREGRec(pVia, VIDREGREC_SAVE_REGISTER, ALPHA_V3_PREFIFO_CONTROL ,(V3_FIFO_THRESHOLD16>>8) |
                                    ( VIDInD(ALPHA_V3_PREFIFO_CONTROL)& (~V3_FIFO_MASK)) );
                                break;
                        }
                    }
                }
                else
                {
                    /*Minified Video will be skewed if not work around*/
                    if (srcWidth <= 80) /*Fetch count <= 5*/
                    {
                        if (dwVideoFlag & VIDEO_1_INUSE)    
                        {
                            viaMacro_VidREGRec(pVia, VIDREGREC_SAVE_REGISTER, V_FIFO_CONTROL,V1_FIFO_DEPTH16 );                            
                        }    
                        else
                        {
                            viaMacro_VidREGRec(pVia, VIDREGREC_SAVE_REGISTER, ALPHA_V3_FIFO_CONTROL,
                                (VIDInD(ALPHA_V3_FIFO_CONTROL)&ALPHA_FIFO_MASK)|V3_FIFO_DEPTH16 );                            
                            viaMacro_VidREGRec(pVia, VIDREGREC_SAVE_REGISTER, ALPHA_V3_PREFIFO_CONTROL ,(V3_FIFO_THRESHOLD16>>8) |
                                ( VIDInD(ALPHA_V3_PREFIFO_CONTROL)& (~V3_FIFO_MASK)) );
                        }
                    }
                    else
                    {
                        if (dwVideoFlag & VIDEO_1_INUSE)    
                        {
                            /*=* Modify for C1 FIFO *=*/
                            switch ( DispatchVGARevisionID(pVia->graphicInfo.RevisionID) )
                            {
            	                case VIA_REVISION_CLECX :
                                    viaMacro_VidREGRec(pVia, VIDREGREC_SAVE_REGISTER, V_FIFO_CONTROL,
                                        V1_FIFO_DEPTH64 | V1_FIFO_PRETHRESHOLD56 | V1_FIFO_THRESHOLD56);
                                    break;
                                    
                                default:
                                    viaMacro_VidREGRec(pVia, VIDREGREC_SAVE_REGISTER, V_FIFO_CONTROL,
                                        V1_FIFO_DEPTH16 | V1_FIFO_PRETHRESHOLD12 | V1_FIFO_THRESHOLD8);
                                    break;
                            }
                        }
                        else
                        {
                            /*=* Modify for C1 FIFO *=*/
                            switch ( DispatchVGARevisionID(pVia->graphicInfo.RevisionID) )
                            {
            	                case VIA_REVISION_CLECX :
                                    viaMacro_VidREGRec(pVia, VIDREGREC_SAVE_REGISTER, ALPHA_V3_FIFO_CONTROL,
                                        (VIDInD(ALPHA_V3_FIFO_CONTROL)&ALPHA_FIFO_MASK)|V3_FIFO_DEPTH64 | V3_FIFO_THRESHOLD56);                            
                                    viaMacro_VidREGRec(pVia, VIDREGREC_SAVE_REGISTER, ALPHA_V3_PREFIFO_CONTROL ,V3_FIFO_PRETHRESHOLD56 |
                                        ( VIDInD(ALPHA_V3_PREFIFO_CONTROL)& (~V3_FIFO_MASK)) );
                                    break;
                                    
                                default:
                                    viaMacro_VidREGRec(pVia, VIDREGREC_SAVE_REGISTER, ALPHA_V3_FIFO_CONTROL,
                                        (VIDInD(ALPHA_V3_FIFO_CONTROL)&ALPHA_FIFO_MASK)|V3_FIFO_DEPTH16 | V3_FIFO_THRESHOLD8);                            
                                    viaMacro_VidREGRec(pVia, VIDREGREC_SAVE_REGISTER, ALPHA_V3_PREFIFO_CONTROL ,(V3_FIFO_THRESHOLD16>>8) |
                                        ( VIDInD(ALPHA_V3_PREFIFO_CONTROL)& (~V3_FIFO_MASK)) );
                                    break;
                            }
                        }
                    }
                }
            }
            else
            {
                if (dwVideoFlag & VIDEO_1_INUSE)    
                {
                    /*=* Modify for C1 FIFO *=*/
                    switch ( DispatchVGARevisionID(pVia->graphicInfo.RevisionID) )
                    {
            	        case VIA_REVISION_CLECX :
                            viaMacro_VidREGRec(pVia, VIDREGREC_SAVE_REGISTER, V_FIFO_CONTROL,
                                V1_FIFO_DEPTH64 | V1_FIFO_PRETHRESHOLD56 | V1_FIFO_THRESHOLD56);
                            break;
                            
                        default:
                            if (pVia->swov.gdwUseExtendedFIFO)
                            {
                                viaMacro_VidREGRec(pVia, VIDREGREC_SAVE_REGISTER, V_FIFO_CONTROL,
                                    V1_FIFO_DEPTH48 | V1_FIFO_PRETHRESHOLD40 | V1_FIFO_THRESHOLD40);
                            }
                            else
                            {
                                viaMacro_VidREGRec(pVia, VIDREGREC_SAVE_REGISTER, V_FIFO_CONTROL,
                                    V1_FIFO_DEPTH32 | V1_FIFO_PRETHRESHOLD29 | V1_FIFO_THRESHOLD16);
                            }                        
                            break;
                    }                        
                }
                else
                {
                    /*Fix V3 bug*/
                    if (srcWidth <= 8)
                    {
                        viaMacro_VidREGRec(pVia, VIDREGREC_SAVE_REGISTER, ALPHA_V3_FIFO_CONTROL,
                            (VIDInD(ALPHA_V3_FIFO_CONTROL)&ALPHA_FIFO_MASK));
                        viaMacro_VidREGRec(pVia, VIDREGREC_SAVE_REGISTER, ALPHA_V3_PREFIFO_CONTROL ,
                            ( VIDInD(ALPHA_V3_PREFIFO_CONTROL)& (~V3_FIFO_MASK)) );
                    }
                    else
                    {
                        /*=* Modify for C1 FIFO *=*/
                        switch ( DispatchVGARevisionID(pVia->graphicInfo.RevisionID) )
                        {
            	            case VIA_REVISION_CLECX :
                                viaMacro_VidREGRec(pVia, VIDREGREC_SAVE_REGISTER, ALPHA_V3_FIFO_CONTROL,
                                    (VIDInD(ALPHA_V3_FIFO_CONTROL)&ALPHA_FIFO_MASK)|V3_FIFO_DEPTH64 | V3_FIFO_THRESHOLD56);
                                viaMacro_VidREGRec(pVia, VIDREGREC_SAVE_REGISTER, ALPHA_V3_PREFIFO_CONTROL ,V3_FIFO_PRETHRESHOLD56 |
                                    ( VIDInD(ALPHA_V3_PREFIFO_CONTROL)& (~V3_FIFO_MASK)) );                        
                                break;
                                
                            default:
                                viaMacro_VidREGRec(pVia, VIDREGREC_SAVE_REGISTER, ALPHA_V3_FIFO_CONTROL,
                                    (VIDInD(ALPHA_V3_FIFO_CONTROL)&ALPHA_FIFO_MASK)|V3_FIFO_DEPTH32 | V3_FIFO_THRESHOLD16);
                                viaMacro_VidREGRec(pVia, VIDREGREC_SAVE_REGISTER, ALPHA_V3_PREFIFO_CONTROL ,(V3_FIFO_THRESHOLD16>>8) |
                                    ( VIDInD(ALPHA_V3_PREFIFO_CONTROL)& (~V3_FIFO_MASK)) );                        
                                break;
                        }
                    }
                }
            }
        }
        else 
        {
            if (pVia->swov.DPFsrc.dwFourCC == FOURCC_YV12)
            {                
                if (dwVideoFlag & VIDEO_HQV_INUSE)
                {
                    if (dwVideoFlag & VIDEO_1_INUSE)    
                    {
                        /*=* Modify for C1 FIFO *=*/
                        switch ( DispatchVGARevisionID(pVia->graphicInfo.RevisionID) )
                        {
            	            case VIA_REVISION_CLECX :
                                viaMacro_VidREGRec(pVia, VIDREGREC_SAVE_REGISTER, V_FIFO_CONTROL,
                                    V1_FIFO_DEPTH64 | V1_FIFO_PRETHRESHOLD56 | V1_FIFO_THRESHOLD56);
                                break;
                                
                            default:
                                viaMacro_VidREGRec(pVia, VIDREGREC_SAVE_REGISTER, V_FIFO_CONTROL,
                                    V1_FIFO_DEPTH32 | V1_FIFO_PRETHRESHOLD29 | V1_FIFO_THRESHOLD16);
                                break;
                        }
                    }
                    else
                    {
                        /*=* Modify for C1 FIFO *=*/
                        switch ( DispatchVGARevisionID(pVia->graphicInfo.RevisionID) )
                        {
            	            case VIA_REVISION_CLECX :
                                viaMacro_VidREGRec(pVia, VIDREGREC_SAVE_REGISTER, ALPHA_V3_FIFO_CONTROL,
                                    (VIDInD(ALPHA_V3_FIFO_CONTROL)&ALPHA_FIFO_MASK)|V3_FIFO_DEPTH64 | V3_FIFO_THRESHOLD56);                        
                                viaMacro_VidREGRec(pVia, VIDREGREC_SAVE_REGISTER, ALPHA_V3_PREFIFO_CONTROL ,V3_FIFO_PRETHRESHOLD56 |
                                    ( VIDInD(ALPHA_V3_PREFIFO_CONTROL)& (~V3_FIFO_MASK)) );
                                break;
                                
                            default:
                                viaMacro_VidREGRec(pVia, VIDREGREC_SAVE_REGISTER, ALPHA_V3_FIFO_CONTROL,
                                    (VIDInD(ALPHA_V3_FIFO_CONTROL)&ALPHA_FIFO_MASK)|V3_FIFO_DEPTH32 | V3_FIFO_THRESHOLD16);                        
                                viaMacro_VidREGRec(pVia, VIDREGREC_SAVE_REGISTER, ALPHA_V3_PREFIFO_CONTROL ,(V3_FIFO_THRESHOLD16>>8) |
                                    ( VIDInD(ALPHA_V3_PREFIFO_CONTROL)& (~V3_FIFO_MASK)) );
                                break;
                        }
                    }
                }
                else
                {
                    /*Minified Video will be skewed if not work around*/
                    if (srcWidth <= 80) /*Fetch count <= 5*/
                    {
                        if (dwVideoFlag & VIDEO_1_INUSE)    
                        {
                            viaMacro_VidREGRec(pVia, VIDREGREC_SAVE_REGISTER, V_FIFO_CONTROL,V1_FIFO_DEPTH16 );                            
                        }    
                        else
                        {
                            viaMacro_VidREGRec(pVia, VIDREGREC_SAVE_REGISTER, ALPHA_V3_FIFO_CONTROL,
                                (VIDInD(ALPHA_V3_FIFO_CONTROL)&ALPHA_FIFO_MASK)|V3_FIFO_DEPTH16 );                            
                            viaMacro_VidREGRec(pVia, VIDREGREC_SAVE_REGISTER, ALPHA_V3_PREFIFO_CONTROL ,(V3_FIFO_THRESHOLD16>>8) |
                                ( VIDInD(ALPHA_V3_PREFIFO_CONTROL)& (~V3_FIFO_MASK)) );
                        }
                    }
                    else
                    {
                        if (dwVideoFlag & VIDEO_1_INUSE)    
                        {
                            /*=* Modify for C1 FIFO *=*/
                            switch ( DispatchVGARevisionID(pVia->graphicInfo.RevisionID) )
                            {
            	                case VIA_REVISION_CLECX :
                                    viaMacro_VidREGRec(pVia, VIDREGREC_SAVE_REGISTER, V_FIFO_CONTROL,
                                        V1_FIFO_DEPTH64 | V1_FIFO_PRETHRESHOLD56 | V1_FIFO_THRESHOLD56);
                                    break;
                                    
                                default:
                                    viaMacro_VidREGRec(pVia, VIDREGREC_SAVE_REGISTER, V_FIFO_CONTROL,
                                        V1_FIFO_DEPTH16 | V1_FIFO_PRETHRESHOLD12 | V1_FIFO_THRESHOLD8);
                                    break;
                            }
                        }
                        else
                        {
                            /*=* Modify for C1 FIFO *=*/
                            switch ( DispatchVGARevisionID(pVia->graphicInfo.RevisionID) )
                            {
            	                case VIA_REVISION_CLECX :
                                    viaMacro_VidREGRec(pVia, VIDREGREC_SAVE_REGISTER, ALPHA_V3_FIFO_CONTROL,
                                        (VIDInD(ALPHA_V3_FIFO_CONTROL)&ALPHA_FIFO_MASK)|V3_FIFO_DEPTH64 | V3_FIFO_THRESHOLD56);                            
                                    viaMacro_VidREGRec(pVia, VIDREGREC_SAVE_REGISTER, ALPHA_V3_PREFIFO_CONTROL ,V3_FIFO_PRETHRESHOLD56 |
                                        ( VIDInD(ALPHA_V3_PREFIFO_CONTROL)& (~V3_FIFO_MASK)) );
                                    break;
                                    
                                default:
                                    viaMacro_VidREGRec(pVia, VIDREGREC_SAVE_REGISTER, ALPHA_V3_FIFO_CONTROL,
                                        (VIDInD(ALPHA_V3_FIFO_CONTROL)&ALPHA_FIFO_MASK)|V3_FIFO_DEPTH16 | V3_FIFO_THRESHOLD8);                            
                                    viaMacro_VidREGRec(pVia, VIDREGREC_SAVE_REGISTER, ALPHA_V3_PREFIFO_CONTROL ,(V3_FIFO_THRESHOLD16>>8) |
                                        ( VIDInD(ALPHA_V3_PREFIFO_CONTROL)& (~V3_FIFO_MASK)) );
                                    break;
                            }
                        }
                    }
                }
            }
            else
            {
                if (dwVideoFlag & VIDEO_1_INUSE)    
                {
                    /*=* Modify for C1 FIFO *=*/
                    switch ( DispatchVGARevisionID(pVia->graphicInfo.RevisionID) )
                    {
            	        case VIA_REVISION_CLECX :
                            viaMacro_VidREGRec(pVia, VIDREGREC_SAVE_REGISTER, V_FIFO_CONTROL,
                                V1_FIFO_DEPTH64 | V1_FIFO_PRETHRESHOLD56 | V1_FIFO_THRESHOLD56);                        
                            break;
                            
                        default:            
                            if (pVia->swov.gdwUseExtendedFIFO)
                            {
                                viaMacro_VidREGRec(pVia, VIDREGREC_SAVE_REGISTER, V_FIFO_CONTROL,
                                    V1_FIFO_DEPTH48 | V1_FIFO_PRETHRESHOLD40 | V1_FIFO_THRESHOLD40);                        
                            }
                            else
                            {
                                viaMacro_VidREGRec(pVia, VIDREGREC_SAVE_REGISTER, V_FIFO_CONTROL,
                                    V1_FIFO_DEPTH32 | V1_FIFO_PRETHRESHOLD29 | V1_FIFO_THRESHOLD16);
                            }
                            break;
                    }
                }
                else
                {
                    /*Fix V3 bug*/
                    if (srcWidth <= 8)
                    {
                        viaMacro_VidREGRec(pVia, VIDREGREC_SAVE_REGISTER, ALPHA_V3_FIFO_CONTROL,
                            (VIDInD(ALPHA_V3_FIFO_CONTROL)&ALPHA_FIFO_MASK));
                        viaMacro_VidREGRec(pVia, VIDREGREC_SAVE_REGISTER, ALPHA_V3_PREFIFO_CONTROL ,
                            ( VIDInD(ALPHA_V3_PREFIFO_CONTROL)& (~V3_FIFO_MASK)) );
                    }
                    else
                    {
                        /*=* Modify for C1 FIFO *=*/
                        switch ( DispatchVGARevisionID(pVia->graphicInfo.RevisionID) )
                        {
            	            case VIA_REVISION_CLECX :
                                viaMacro_VidREGRec(pVia, VIDREGREC_SAVE_REGISTER, ALPHA_V3_FIFO_CONTROL,
                                    (VIDInD(ALPHA_V3_FIFO_CONTROL)&ALPHA_FIFO_MASK)|V3_FIFO_DEPTH64 | V3_FIFO_THRESHOLD56);
                                viaMacro_VidREGRec(pVia, VIDREGREC_SAVE_REGISTER, ALPHA_V3_PREFIFO_CONTROL ,V3_FIFO_PRETHRESHOLD56 |
                                    ( VIDInD(ALPHA_V3_PREFIFO_CONTROL)& (~V3_FIFO_MASK)) );                        
                                break;
                                
                            default:
                                viaMacro_VidREGRec(pVia, VIDREGREC_SAVE_REGISTER, ALPHA_V3_FIFO_CONTROL,
                                   (VIDInD(ALPHA_V3_FIFO_CONTROL)&ALPHA_FIFO_MASK)|V3_FIFO_DEPTH32 | V3_FIFO_THRESHOLD16);
                                viaMacro_VidREGRec(pVia, VIDREGREC_SAVE_REGISTER, ALPHA_V3_PREFIFO_CONTROL ,(V3_FIFO_THRESHOLD16>>8) |
                                   ( VIDInD(ALPHA_V3_PREFIFO_CONTROL)& (~V3_FIFO_MASK)) );                        
                                break;
                        }
                    }
                }
            }
        }

        if (dwVideoFlag & VIDEO_HQV_INUSE)
        {
            miniCtl=0;
            if (dwHQVzoomflagH||dwHQVzoomflagV)
            {
                dwTmp = 0;
                if (dwHQVzoomflagH)
                {
                    miniCtl = V1_X_INTERPOLY;
                    dwTmp = (zoomCtl&0xffff0000);
                }
                
                if (dwHQVzoomflagV)
                {
                    miniCtl |= (V1_Y_INTERPOLY | V1_YCBCR_INTERPOLY);
                    dwTmp |= (zoomCtl&0x0000ffff);
                    dwHQVfilterCtl &= 0xfffdffff;
                }
       
                /*Temporarily fix for 2D bandwidth problem. 2002/08/01*/
                if ((pVia->swov.gdwUseExtendedFIFO))
                {
                    miniCtl &= ~V1_Y_INTERPOLY;
                }
       
                if (dwVideoFlag & VIDEO_1_INUSE)
                {
                    viaMacro_VidREGRec(pVia, VIDREGREC_SAVE_REGISTER,V1_MINI_CONTROL, miniCtl);
                    viaMacro_VidREGRec(pVia, VIDREGREC_SAVE_REGISTER,V1_ZOOM_CONTROL, dwTmp);                
                }
                else
                {
                    viaMacro_VidREGRec(pVia, VIDREGREC_SAVE_REGISTER,V3_MINI_CONTROL, miniCtl);
                    viaMacro_VidREGRec(pVia, VIDREGREC_SAVE_REGISTER,V3_ZOOM_CONTROL, dwTmp);                                    
                }
            }
            else
            {
                if (srcHeight==dstHeight)
                {
                    dwHQVfilterCtl &= 0xfffdffff;
                }
                if (dwVideoFlag & VIDEO_1_INUSE)
                {
                    viaMacro_VidREGRec(pVia, VIDREGREC_SAVE_REGISTER,V1_MINI_CONTROL, 0);
                    viaMacro_VidREGRec(pVia, VIDREGREC_SAVE_REGISTER,V1_ZOOM_CONTROL, 0);
                }
                else
                {
                    viaMacro_VidREGRec(pVia, VIDREGREC_SAVE_REGISTER,V3_MINI_CONTROL, 0);
                    viaMacro_VidREGRec(pVia, VIDREGREC_SAVE_REGISTER,V3_ZOOM_CONTROL, 0);
                }
            }
            viaMacro_VidREGRec(pVia, VIDREGREC_SAVE_REGISTER,HQV_MINIFY_CONTROL, dwHQVminiCtl);
            viaMacro_VidREGRec(pVia, VIDREGREC_SAVE_REGISTER,HQV_FILTER_CONTROL, dwHQVfilterCtl);
        }
        else
        {
            if (dwVideoFlag & VIDEO_1_INUSE)
            {
                viaMacro_VidREGRec(pVia, VIDREGREC_SAVE_REGISTER,V1_MINI_CONTROL, miniCtl);
                viaMacro_VidREGRec(pVia, VIDREGREC_SAVE_REGISTER,V1_ZOOM_CONTROL, zoomCtl);
            }
            else
            {
                viaMacro_VidREGRec(pVia, VIDREGREC_SAVE_REGISTER,V3_MINI_CONTROL, miniCtl);
                viaMacro_VidREGRec(pVia, VIDREGREC_SAVE_REGISTER,V3_ZOOM_CONTROL, zoomCtl);                                
            }
        }


        /* Colorkey*/
        if (dwColorKey) {
            DBG_DD(ErrorF("Overlay colorkey= low:%08lx high:%08lx\n", dwKeyLow, dwKeyHigh));

            dwKeyLow &= 0x00FFFFFF;
            /*viaMacro_VidREGRec(pVia, VIDREGREC_SAVE_REGISTER,V_COLOR_KEY, dwKeyLow);*/
            
            if (dwVideoFlag & VIDEO_1_INUSE)
            {
                viaMacro_VidREGRec(pVia, VIDREGREC_SAVE_REGISTER,V_COLOR_KEY, dwKeyLow);
            }
            else
            {
                if ( hwDiff->dwSupportTwoColorKey )    /*CLE_C0*/
                    viaMacro_VidREGRec(pVia, VIDREGREC_SAVE_REGISTER,V3_COLOR_KEY, dwKeyLow);
            }

            /*dwCompose = (dwCompose & ~0x0f) | SELECT_VIDEO_IF_COLOR_KEY;*/
            /*CLE_C0*/
            dwCompose = (dwCompose & ~0x0f) | SELECT_VIDEO_IF_COLOR_KEY | SELECT_VIDEO3_IF_COLOR_KEY;
            /*dwCompose = (dwCompose & ~0x0f)  ;*/
        }

        if (dwChromaKey) {
            DBG_DD(ErrorF("Overlay Chromakey= low:%08lx high:%08lx\n", dwKeyLow, dwKeyHigh));

            dwChromaLow  &= CHROMA_KEY_LOW;
            dwChromaHigh &= CHROMA_KEY_HIGH;

            dwChromaLow  |= (VIDInD(V_CHROMAKEY_LOW)&(~CHROMA_KEY_LOW));
            dwChromaHigh |= (VIDInD(V_CHROMAKEY_HIGH)&(~CHROMA_KEY_HIGH));

            /*Added by Scottie[2001.12.5] for Chroma Key*/
            if (pVia->swov.DPFsrc.dwFlags & DDPF_FOURCC)
            {
                    switch (pVia->swov.DPFsrc.dwFourCC) {
                    case FOURCC_YV12:
                            /*to be continued...*/
                            break;
                    case FOURCC_YUY2:
                            /*to be continued...*/
                            break;
                    default:
                            /*TOINT3;*/
                            break;
                    }
            }
            else if (pVia->swov.DPFsrc.dwFlags & DDPF_RGB)
            {
                    unsigned long dwtmpLowR;
                    unsigned long dwtmpLowG;
                    unsigned long dwtmpLowB;
                    unsigned long dwtmpChromaLow;
                    unsigned long dwtmpHighR;
                    unsigned long dwtmpHighG;
                    unsigned long dwtmpHighB;
                    unsigned long dwtmpChromaHigh;

                    switch (pVia->swov.DPFsrc.dwRGBBitCount) {
                    case 16:
                            if (pVia->swov.DPFsrc.dwGBitMask==0x07E0) /*RGB16(5:6:5)*/
                            {
                                    dwtmpLowR = (((dwChromaLow >> 11) << 3) | ((dwChromaLow >> 13) & 0x07)) & 0xFF;
                                    dwtmpLowG = (((dwChromaLow >> 5) << 2) | ((dwChromaLow >> 9) & 0x03)) & 0xFF;

                                    dwtmpHighR = (((dwChromaHigh >> 11) << 3) | ((dwChromaHigh >> 13) & 0x07)) & 0xFF;
                                    dwtmpHighG = (((dwChromaHigh >> 5) << 2) | ((dwChromaHigh >> 9) & 0x03)) & 0xFF;
                            }
                            else /*RGB15(5:5:5)*/
                            {
                                    dwtmpLowR = (((dwChromaLow >> 10) << 3) | ((dwChromaLow >> 12) & 0x07)) & 0xFF;
                                    dwtmpLowG = (((dwChromaLow >> 5) << 3) | ((dwChromaLow >> 7) & 0x07)) & 0xFF;

                                    dwtmpHighR = (((dwChromaHigh >> 10) << 3) | ((dwChromaHigh >> 12) & 0x07)) & 0xFF;
                                    dwtmpHighG = (((dwChromaHigh >> 5) << 3) | ((dwChromaHigh >> 7) & 0x07)) & 0xFF;
                            }
                            dwtmpLowB = (((dwChromaLow << 3) | (dwChromaLow >> 2)) & 0x07) & 0xFF;
                            dwtmpChromaLow = (dwtmpLowG << 16) | (dwtmpLowR << 8) | dwtmpLowB;
                            dwChromaLow = ((dwChromaLow >> 24) << 24) | dwtmpChromaLow;

                            dwtmpHighB = (((dwChromaHigh << 3) | (dwChromaHigh >> 2)) & 0x07) & 0xFF;
                            dwtmpChromaHigh = (dwtmpHighG << 16) | (dwtmpHighR << 8) | dwtmpHighB;
                            dwChromaHigh = ((dwChromaHigh >> 24) << 24) | dwtmpChromaHigh;
                            break;

                    case 32: /*32 bit RGB*/
                            dwtmpLowR = (dwChromaLow >> 16) & 0xFF;
                            dwtmpLowG = (dwChromaLow >> 8) & 0xFF;
                            dwtmpLowB = dwChromaLow & 0xFF;
                            dwtmpChromaLow = (dwtmpLowG << 16) | (dwtmpLowR << 8) | dwtmpLowB;
                            dwChromaLow = ((dwChromaLow >> 24) << 24) | dwtmpChromaLow;

                            dwtmpHighR = (dwChromaHigh >> 16) & 0xFF;
                            dwtmpHighG = (dwChromaHigh >> 8) & 0xFF;
                            dwtmpHighB = dwChromaHigh & 0xFF;
                            dwtmpChromaHigh = (dwtmpHighG << 16) | (dwtmpHighR << 8) | dwtmpHighB;
                            dwChromaHigh = ((dwChromaHigh >> 24) << 24) | dwtmpChromaHigh;
                            break;

                    default:
                            /*TOINT3;*/
                            break;
                    }
            }/*End of DDPF_FOURCC*/

            viaMacro_VidREGRec(pVia, VIDREGREC_SAVE_REGISTER,V_CHROMAKEY_HIGH,dwChromaHigh);
            if (dwVideoFlag & VIDEO_1_INUSE)
            {
                viaMacro_VidREGRec(pVia, VIDREGREC_SAVE_REGISTER,V_CHROMAKEY_LOW, dwChromaLow);
                /*Temporarily solve the H/W Interpolation error when using Chroma Key*/
                viaMacro_VidREGRec(pVia, VIDREGREC_SAVE_REGISTER,V1_MINI_CONTROL, miniCtl & 0xFFFFFFF8);
            }
            else
            {
                viaMacro_VidREGRec(pVia, VIDREGREC_SAVE_REGISTER,V_CHROMAKEY_LOW, dwChromaLow|V_CHROMAKEY_V3);
                viaMacro_VidREGRec(pVia, VIDREGREC_SAVE_REGISTER,V3_MINI_CONTROL, miniCtl & 0xFFFFFFF8);
            }

            /*Modified by Scottie[2001.12.5] for select video if (color key & chroma key)*/
            if (dwCompose==SELECT_VIDEO_IF_COLOR_KEY)
               dwCompose = SELECT_VIDEO_IF_COLOR_KEY | SELECT_VIDEO_IF_CHROMA_KEY;
            else
               dwCompose = (dwCompose & ~0x0f) | SELECT_VIDEO_IF_CHROMA_KEY;
        }

        /* determine which video stream is on top */
        /*
        DBG_DD(ErrorF("        dwFlags= 0x%08lx\n", dwFlags));
        if (dwFlags & DDOVER_CLIP)
            dwCompose |= COMPOSE_V3_TOP;
        else
            dwCompose |= COMPOSE_V1_TOP;
        */    
        DBG_DD(ErrorF("        pVia->Video.dwCompose 0x%lx\n", pVia->Video.dwCompose));

        if (pVia->Video.dwCompose & (VW_TV1_TOP | VW_TV_TOP) )
            dwCompose |= COMPOSE_V3_TOP;
        else if (pVia->Video.dwCompose & (VW_TV0_TOP | VW_DVD_TOP) )
            dwCompose &= ~COMPOSE_V3_TOP;

        DBG_DD(ErrorF("        dwCompose 0x%8lx\n", dwCompose));

        /* Setup video control*/
        if (dwVideoFlag & VIDEO_HQV_INUSE)
        {
            if (!pVia->swov.SWVideo_ON)
            /*if (0)*/
            {
                DBG_DD(ErrorF("    First HQV\n")); 
              
                viaMacro_VidREGFlush(pVia);

		DBG_DD(ErrorF(" Wait flips"));
                if ( hwDiff->dwHQVInitPatch )  
                {
		    DBG_DD(ErrorF(" Wait flips 1"));
                    viaWaitHQVFlipClear(pVia, ((dwHQVCtl&~HQV_SW_FLIP)|HQV_FLIP_STATUS)&~HQV_ENABLE);
                    VIDOutD(HQV_CONTROL, dwHQVCtl);
		    DBG_DD(ErrorF(" Wait flips2"));
                    viaWaitHQVFlip(pVia);
		    DBG_DD(ErrorF(" Wait flips 3"));
                    viaWaitHQVFlipClear(pVia, ((dwHQVCtl&~HQV_SW_FLIP)|HQV_FLIP_STATUS)&~HQV_ENABLE);
                    VIDOutD(HQV_CONTROL, dwHQVCtl);
		    DBG_DD(ErrorF(" Wait flips4"));
                    viaWaitHQVFlip(pVia);
                }
                else    /* CLE_C0 */
                {
                    VIDOutD(HQV_CONTROL, dwHQVCtl & ~HQV_SW_FLIP);
                    VIDOutD(HQV_CONTROL, dwHQVCtl | HQV_SW_FLIP);
		    DBG_DD(ErrorF(" Wait flips5"));
                    viaWaitHQVFlip(pVia);
		    DBG_DD(ErrorF(" Wait flips6"));
                }

                if (dwVideoFlag & VIDEO_1_INUSE)
                {
                    VIDOutD(V1_CONTROL, dwVidCtl);
                    VIDOutD(V_COMPOSE_MODE, (dwCompose|V1_COMMAND_FIRE ));
                    if (pVia->swov.gdwUseExtendedFIFO)
                    {
                        /*Set Display FIFO*/
			DBG_DD(ErrorF(" Wait flips7"));
                        viaWaitVBI(pVia);
			DBG_DD(ErrorF(" Wait flips 8"));
                        /*outb(0x17, 0x3C4); outb(0x2f, 0x3C5);
                        outb(0x16, 0x3C4); outb((pVia->swov.Save_3C4_16&0xf0)|0x14, 0x3C5);
                        outb(0x18, 0x3C4); outb(0x56, 0x3C5);*/
                        
                        VGAOUT8(0x3C4, 0x17); VGAOUT8(0x3C5, 0x2f);
                        VGAOUT8(0x3C4, 0x16); VGAOUT8(0x3C5, (pVia->swov.Save_3C4_16&0xf0)|0x14);
                        VGAOUT8(0x3C4, 0x18); VGAOUT8(0x3C5, 0x56);
			DBG_DD(ErrorF(" Wait flips 9"));
                    }
                }
                else
                {
		    DBG_DD(ErrorF(" Wait flips 10"));
                    VIDOutD(V3_CONTROL, dwVidCtl);
                    VIDOutD(V_COMPOSE_MODE, (dwCompose|V3_COMMAND_FIRE ));                    
                }
		DBG_DD(ErrorF(" Done flips"));
            }
            else
            {
                DBG_DD(ErrorF("    Normal called\n"));
                if (dwVideoFlag & VIDEO_1_INUSE)
                {
                    viaMacro_VidREGRec(pVia, VIDREGREC_SAVE_REGISTER,V1_CONTROL, dwVidCtl);
                    viaMacro_VidREGRec(pVia, VIDREGREC_SAVE_REGISTER,V_COMPOSE_MODE, (dwCompose|V1_COMMAND_FIRE ));
                }
                else
                {
                    viaMacro_VidREGRec(pVia, VIDREGREC_SAVE_REGISTER,V3_CONTROL, dwVidCtl);
                    viaMacro_VidREGRec(pVia, VIDREGREC_SAVE_REGISTER,V_COMPOSE_MODE, (dwCompose|V3_COMMAND_FIRE ));                    
                }
                viaMacro_VidREGRec(pVia, VIDREGREC_SAVE_REGISTER, HQV_CONTROL, dwHQVCtl|HQV_FLIP_STATUS);
                viaWaitHQVDone(pVia);
                viaMacro_VidREGFlush(pVia);                
            }
        }
        else
        {
            if (dwVideoFlag & VIDEO_1_INUSE)
            {
                viaMacro_VidREGRec(pVia, VIDREGREC_SAVE_REGISTER,V1_CONTROL, dwVidCtl);
                viaMacro_VidREGRec(pVia, VIDREGREC_SAVE_REGISTER,V_COMPOSE_MODE, (dwCompose|V1_COMMAND_FIRE ));
            }
            else
            {
                viaMacro_VidREGRec(pVia, VIDREGREC_SAVE_REGISTER,V3_CONTROL, dwVidCtl);
                viaMacro_VidREGRec(pVia, VIDREGREC_SAVE_REGISTER,V_COMPOSE_MODE, (dwCompose|V3_COMMAND_FIRE ));                
            }
            viaWaitHQVDone(pVia);
            viaMacro_VidREGFlush(pVia);
        }
        pVia->swov.SWVideo_ON = TRUE;      
    }
    else
    {
        /*Hide overlay*/
        
        if ( hwDiff->dwHQVDisablePatch )     /*CLE_C0*/
        {
            VGAOUT8(0x3C4, 0x2E); 
            VGAOUT8(0x3C5, 0xEF);
        }
        
        viaMacro_VidREGRec(pVia, VIDREGREC_SAVE_REGISTER,V_FIFO_CONTROL,V1_FIFO_PRETHRESHOLD12 |
             V1_FIFO_THRESHOLD8 |V1_FIFO_DEPTH16);
        viaMacro_VidREGRec(pVia, VIDREGREC_SAVE_REGISTER,ALPHA_V3_FIFO_CONTROL, ALPHA_FIFO_THRESHOLD4 
             | ALPHA_FIFO_DEPTH8  | V3_FIFO_THRESHOLD24 | V3_FIFO_DEPTH32 );

        if (dwVideoFlag&VIDEO_HQV_INUSE)
        {
            viaMacro_VidREGRec(pVia, VIDREGREC_SAVE_REGISTER,HQV_CONTROL, (VIDInD(HQV_CONTROL) & (~HQV_ENABLE)));            
        }

        if (dwVideoFlag&VIDEO_1_INUSE)
        {
            viaMacro_VidREGRec(pVia, VIDREGREC_SAVE_REGISTER,V1_CONTROL, (VIDInD(V1_CONTROL) & (~V1_ENABLE)));
            viaMacro_VidREGRec(pVia, VIDREGREC_SAVE_REGISTER,V_COMPOSE_MODE, (VIDInD(V_COMPOSE_MODE)|V1_COMMAND_FIRE));
        }
        else
        {
            viaMacro_VidREGRec(pVia, VIDREGREC_SAVE_REGISTER,V3_CONTROL, (VIDInD(V3_CONTROL) & (~V3_ENABLE)));        
            viaMacro_VidREGRec(pVia, VIDREGREC_SAVE_REGISTER,V_COMPOSE_MODE, (VIDInD(V_COMPOSE_MODE)|V3_COMMAND_FIRE));
        }
        
        viaMacro_VidREGFlush(pVia);
        
        if ( hwDiff->dwHQVDisablePatch )     /*CLE_C0*/
        {
            VGAOUT8(0x3C4, 0x2E); 
            VGAOUT8(0x3C5, 0xFF);
        }
            
    }  
    DBG_DD(ErrorF(" Done Upd_Video"));

    return PI_OK;    
    
} /* Upd_Video */

/*************************************************************************
 *  VIAVidUpdateOverlay
 *  Parameters:   src rectangle, dst rectangle, colorkey...
 *  Return Value: unsigned long of state
 *  note: Update the overlay image param.
*************************************************************************/
unsigned long VIAVidUpdateOverlay(ScrnInfoPtr pScrn, LPDDUPDATEOVERLAY lpUpdate)
{
    VIAPtr  pVia = VIAPTR(pScrn);
    unsigned long dwFlags = lpUpdate->dwFlags;
    unsigned long dwKeyLow=0, dwKeyHigh=0;
    unsigned long dwChromaLow=0, dwChromaHigh=0;
    unsigned long dwVideoFlag=0;
    unsigned long dwColorKey=0, dwChromaKey=0;
    /*DDUPDATEOVERLAY UpdateOverlayTemp;*/
    int   nDstTop, nDstBottom, nDstLeft, nDstRight;

    DBG_DD(ErrorF("// VIAVidUpdateOverlay: %08lx\n", dwFlags));
    
    /* Adjust to fix panning mode bug */
    lpUpdate->rDest.left = lpUpdate->rDest.left - (pVia->swov.panning_x - pVia->swov.panning_old_x);
    lpUpdate->rDest.top = lpUpdate->rDest.top - (pVia->swov.panning_y - pVia->swov.panning_old_y);
    lpUpdate->rDest.right = lpUpdate->rDest.right - (pVia->swov.panning_x - pVia->swov.panning_old_x);
    lpUpdate->rDest.bottom = lpUpdate->rDest.bottom - (pVia->swov.panning_y - pVia->swov.panning_old_y);

    DBG_DD(ErrorF("Raw rSrc  X (%ld,%ld) Y (%ld,%ld)\n",
                lpUpdate->rSrc.left, lpUpdate->rSrc.right, lpUpdate->rSrc.top, lpUpdate->rSrc.bottom));
    DBG_DD(ErrorF("Raw rDest  X (%ld,%ld) Y (%ld,%ld)\n",
                lpUpdate->rDest.left, lpUpdate->rDest.right, lpUpdate->rDest.top, lpUpdate->rDest.bottom));

/*    if (pVia->swov.gdwVideoFlagTV1 && !gdwOverlaySupport)
        return PI_OK;
*/
    if ( (pVia->swov.DPFsrc.dwFourCC == FOURCC_YUY2)||( pVia->swov.DPFsrc.dwFourCC == FOURCC_YV12 ) )
        dwVideoFlag = pVia->swov.gdwVideoFlagSW;

    dwFlags |= DDOVER_INTERLEAVED;

    /* For Alpha windows setting */
    if (pVia->swov.gdwAlphaEnabled)
        dwFlags &= ~DDOVER_KEYDEST;

    viaMacro_VidREGRec(pVia, VIDREGREC_RESET_COUNTER, 0,0);

    if ( dwFlags & DDOVER_HIDE )
    {
        DBG_DD(ErrorF("//    :DDOVER_HIDE \n"));
        
        dwVideoFlag &= ~VIDEO_SHOW;
        if (Upd_Video(pScrn, dwVideoFlag,0,lpUpdate->rSrc,lpUpdate->rDest,0,0,0,&pVia->swov.DPFsrc,0,
                0,0,0,0,0,0, dwFlags)== PI_ERR)
        {
                return PI_ERR;                    
        }
        pVia->swov.SWVideo_ON = FALSE;
        pVia->swov.UpdateOverlayBackup.rDest.left = 0;
        pVia->swov.UpdateOverlayBackup.rDest.top = 0;
        pVia->swov.UpdateOverlayBackup.rDest.right = 0;
        pVia->swov.UpdateOverlayBackup.rDest.bottom = 0;

        if (pVia->swov.gdwUseExtendedFIFO)
        {
            /*Restore Display fifo*/
            /*outb(0x16, 0x3C4); outb(pVia->swov.Save_3C4_16, 0x3C5);*/
            VGAOUT8(0x3C4, 0x16); VGAOUT8(0x3C5, pVia->swov.Save_3C4_16);
            DBG_DD(ErrorF("Restore 3c4.16 : %08x \n",VGAIN8(0x3C5)));
            
            /*outb(0x17, 0x3C4); outb(pVia->swov.Save_3C4_17, 0x3C5);*/
            VGAOUT8(0x3C4, 0x17); VGAOUT8(0x3C5, pVia->swov.Save_3C4_17);
            DBG_DD(ErrorF("        3c4.17 : %08x \n",VGAIN8(0x3C5)));
            
            /*outb(0x18, 0x3C4); outb(pVia->swov.Save_3C4_18, 0x3C5);*/
            VGAOUT8(0x3C4, 0x18); VGAOUT8(0x3C5, pVia->swov.Save_3C4_18);
            DBG_DD(ErrorF("        3c4.18 : %08x \n",VGAIN8(0x3C5)));
            pVia->swov.gdwUseExtendedFIFO = 0;
		}
        return PI_OK;
    }

    /* If the dest rectangle doesn't change, we can return directly */
    /*
    if ( (pVia->swov.UpdateOverlayBackup.rDest.left ==  lpUpdate->rDest.left) &&
         (pVia->swov.UpdateOverlayBackup.rDest.top ==  lpUpdate->rDest.top) &&
         (pVia->swov.UpdateOverlayBackup.rDest.right ==  lpUpdate->rDest.right) &&
         (pVia->swov.UpdateOverlayBackup.rDest.bottom ==  lpUpdate->rDest.bottom) )
        return PI_OK;
    */
    pVia->swov.UpdateOverlayBackup = * (LPDDUPDATEOVERLAY) lpUpdate;

    if ( dwFlags & DDOVER_KEYDEST )
    {
        DBG_DD(ErrorF("//    :DDOVER_KEYDEST \n"));
        
        dwColorKey = 1;
        dwKeyLow = lpUpdate->dwColorSpaceLowValue;
    }

    if (dwFlags & DDOVER_SHOW)    
    {
        unsigned long dwStartAddr=0, dwDeinterlaceMode=0;
        unsigned long dwScnWidth, dwScnHeight;

        DBG_DD(ErrorF("//    :DDOVER_SHOW \n"));

        /*for SW decode HW overlay use*/
        dwStartAddr = VIDInD(HQV_SRC_STARTADDR_Y);
        DBG_DD(ErrorF("dwStartAddr= 0x%lx\n", dwStartAddr));

        if (dwFlags & DDOVER_INTERLEAVED)
        {
            dwDeinterlaceMode |= DDOVER_INTERLEAVED;
            DBG_DD(ErrorF("DDOVER_INTERLEAVED\n"));
        }
        if (dwFlags & DDOVER_BOB)
        {
            dwDeinterlaceMode |= DDOVER_BOB;
            DBG_DD(ErrorF("DDOVER_BOB\n"));
        }

        if ((pVia->graphicInfo.dwWidth > 1024))
	{
	    DBG_DD(ErrorF("UseExtendedFIFO\n"));
	    pVia->swov.gdwUseExtendedFIFO = 1;
	}
	/*
	else
	{
	    //Set Display FIFO
	    outb(0x16, 0x3C4); outb((pVia->swov->Save_3C4_16&0xf0)|0x0c, 0x3C5);
	    DBG_DD(ErrorF("set     3c4.16 : %08x \n",inb(0x3C5)));
	    outb(0x18, 0x3C4); outb(0x4c, 0x3C5);
	    DBG_DD(ErrorF("        3c4.18 : %08x \n",inb(0x3C5)));
	}
        */
        dwVideoFlag |= VIDEO_SHOW;

        /*
         * Figure out actual rSrc rectangle
         * Coz the Src rectangle AP sent is always original, ex:size(720,480) at (0,0)
         * so the driver need to re-calc
         * 
         * transfer unsigned long to signed int for calc
         */
        nDstLeft = lpUpdate->rDest.left;
        nDstTop  = lpUpdate->rDest.top;
        nDstRight= lpUpdate->rDest.right;
        nDstBottom=lpUpdate->rDest.bottom;

        dwScnWidth  = pVia->graphicInfo.dwWidth;
        dwScnHeight = pVia->graphicInfo.dwHeight;

        if (nDstLeft<0)
            lpUpdate->rSrc.left  = (((-nDstLeft) * pVia->swov.overlayRecordV1.dwV1OriWidth) + ((nDstRight-nDstLeft)>>1)) / (nDstRight-nDstLeft);
        else 
            lpUpdate->rSrc.left = 0;

        if (nDstRight>dwScnWidth)
            lpUpdate->rSrc.right = (((dwScnWidth-nDstLeft) * pVia->swov.overlayRecordV1.dwV1OriWidth) + ((nDstRight-nDstLeft)>>1)) / (nDstRight-nDstLeft);
        else    
            lpUpdate->rSrc.right = pVia->swov.overlayRecordV1.dwV1OriWidth;

        if (nDstTop<0)
           lpUpdate->rSrc.top   =  (((-nDstTop) * pVia->swov.overlayRecordV1.dwV1OriHeight) + ((nDstBottom-nDstTop)>>1))/ (nDstBottom-nDstTop);
        else
           lpUpdate->rSrc.top   = 0;

        if (nDstBottom >dwScnHeight)
            lpUpdate->rSrc.bottom = (((dwScnHeight-nDstTop) * pVia->swov.overlayRecordV1.dwV1OriHeight) + ((nDstBottom-nDstTop)>>1)) / (nDstBottom-nDstTop);
        else 
            lpUpdate->rSrc.bottom = pVia->swov.overlayRecordV1.dwV1OriHeight;

        /* save modified src & original dest rectangle param.*/
        if ( (pVia->swov.DPFsrc.dwFourCC == FOURCC_YUY2)||( pVia->swov.DPFsrc.dwFourCC == FOURCC_YV12 ) )
        {   
            pVia->swov.SWDevice.gdwSWDstLeft     = lpUpdate->rDest.left + (pVia->swov.panning_x - pVia->swov.panning_old_x);
            pVia->swov.SWDevice.gdwSWDstTop      = lpUpdate->rDest.top + (pVia->swov.panning_y - pVia->swov.panning_old_y);
            pVia->swov.SWDevice.gdwSWDstWidth    = lpUpdate->rDest.right - lpUpdate->rDest.left;
            pVia->swov.SWDevice.gdwSWDstHeight   = lpUpdate->rDest.bottom - lpUpdate->rDest.top;

            pVia->swov.SWDevice.gdwSWSrcWidth    = pVia->swov.overlayRecordV1.dwV1SrcWidth = lpUpdate->rSrc.right - lpUpdate->rSrc.left;
            pVia->swov.SWDevice.gdwSWSrcHeight   = pVia->swov.overlayRecordV1.dwV1SrcHeight = lpUpdate->rSrc.bottom - lpUpdate->rSrc.top;
        }

        pVia->swov.overlayRecordV1.dwV1SrcLeft   = lpUpdate->rSrc.left;
        pVia->swov.overlayRecordV1.dwV1SrcRight  = lpUpdate->rSrc.right;
        pVia->swov.overlayRecordV1.dwV1SrcTop    = lpUpdate->rSrc.top;
        pVia->swov.overlayRecordV1.dwV1SrcBot    = lpUpdate->rSrc.bottom;
        
        /*
        // Figure out actual rDest rectangle
        */
        lpUpdate->rDest.left= nDstLeft<0 ? 0 : nDstLeft;
        lpUpdate->rDest.top= nDstTop<0 ? 0 : nDstTop;
        if ( lpUpdate->rDest.top >= dwScnHeight)
           lpUpdate->rDest.top = dwScnHeight-1;
        /*lpUpdate->rDest.top= top>=dwScnHeight   ? dwScnHeight-1: top;*/
        lpUpdate->rDest.right= nDstRight>dwScnWidth ? dwScnWidth: nDstRight;
        lpUpdate->rDest.bottom= nDstBottom>dwScnHeight ? dwScnHeight: nDstBottom;

        /* 
         *	Check which update func. (upd_MPEG, upd_video, 
         *	upd_capture) to call
         */
        if (Upd_Video(pScrn, dwVideoFlag,dwStartAddr,lpUpdate->rSrc,lpUpdate->rDest,pVia->swov.SWDevice.dwPitch,
             pVia->swov.overlayRecordV1.dwV1OriWidth,pVia->swov.overlayRecordV1.dwV1OriHeight,&pVia->swov.DPFsrc,
             dwDeinterlaceMode,dwColorKey,dwChromaKey,
             dwKeyLow,dwKeyHigh,dwChromaLow,dwChromaHigh, dwFlags)== PI_ERR)
             {
                 return PI_ERR;                    
             }                
            pVia->swov.SWVideo_ON = FALSE;

        return PI_OK;
        
    } /*end of DDOVER_SHOW*/

    pVia->swov.panning_old_x = pVia->swov.panning_x;
    pVia->swov.panning_old_y = pVia->swov.panning_y;

    return PI_OK;

} /*VIAVidUpdateOverlay*/



/*************************************************************************
 *  ADJUST FRAME
*************************************************************************/
unsigned long VIAVidAdjustFrame(ScrnInfoPtr pScrn, LPADJUSTFRAME lpAdjustFrame)
{
    VIAPtr pVia = VIAPTR(pScrn);
    DBG_DD(ErrorF("//VIAVidAdjustFrame\n"));

    pVia->swov.panning_x = lpAdjustFrame->x;
    pVia->swov.panning_y = lpAdjustFrame->y;            

    return PI_OK;
}
