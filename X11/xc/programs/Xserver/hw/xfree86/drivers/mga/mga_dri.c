/* $XFree86: xc/programs/Xserver/hw/xfree86/drivers/mga/mga_dri.c,v 1.28 2003/02/08 21:26:58 dawes Exp $ */

/*
 * Copyright 2000 VA Linux Systems Inc., Fremont, California.
 * All Rights Reserved.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice (including the next
 * paragraph) shall be included in all copies or substantial portions of the
 * Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL
 * VA LINUX SYSTEMS AND/OR ITS SUPPLIERS BE LIABLE FOR ANY CLAIM, DAMAGES
 * OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE,
 * ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR
 * OTHER DEALINGS IN THE SOFTWARE.
 *
 * Authors:
 *    Keith Whitwell <keith@tungstengraphics.com>
 *    Gareth Hughes <gareth@valinux.com>
 */

#include "xf86.h"
#include "xf86_OSproc.h"
#include "xf86_ansic.h"
#include "xf86Priv.h"

#include "xf86PciInfo.h"
#include "xf86Pci.h"
#define PSZ 8
#include "cfb.h"
#undef PSZ
#include "cfb16.h"
#include "cfb32.h"

#include "miline.h"




#include "mga_bios.h"
#include "mga_reg.h"
#include "mga.h"
#include "mga_macros.h"
#include "mga_dri.h"
#include "mga_sarea.h"

#define _XF86DRI_SERVER_
#include "GL/glxtokens.h"
#include "sarea.h"





#include "GL/glxtokens.h"

#include "mga_bios.h"
#include "mga_reg.h"
#include "mga.h"
#include "mga_macros.h"
#include "mga_dri.h"

#include "mga_sarea.h"

static char MGAKernelDriverName[] = "mga";
static char MGAClientDriverName[] = "mga";

/* DRI buffer management
 */
extern void Mga8DRIInitBuffers( WindowPtr pWin, RegionPtr prgn,
				CARD32 index );
extern void Mga8DRIMoveBuffers( WindowPtr pParent, DDXPointRec ptOldOrg,
				RegionPtr prgnSrc, CARD32 index );

extern void Mga16DRIInitBuffers( WindowPtr pWin, RegionPtr prgn,
				 CARD32 index );
extern void Mga16DRIMoveBuffers( WindowPtr pParent, DDXPointRec ptOldOrg,
				 RegionPtr prgnSrc, CARD32 index );

extern void Mga24DRIInitBuffers( WindowPtr pWin, RegionPtr prgn,
				 CARD32 index );
extern void Mga24DRIMoveBuffers( WindowPtr pParent, DDXPointRec ptOldOrg,
				 RegionPtr prgnSrc, CARD32 index );

extern void Mga32DRIInitBuffers( WindowPtr pWin, RegionPtr prgn,
				 CARD32 index );
extern void Mga32DRIMoveBuffers( WindowPtr pParent, DDXPointRec ptOldOrg,
				 RegionPtr prgnSrc, CARD32 index );


/* Initialize the visual configs that are supported by the hardware.
 * These are combined with the visual configs that the indirect
 * rendering core supports, and the intersection is exported to the
 * client.
 */
static Bool MGAInitVisualConfigs( ScreenPtr pScreen )
{
   ScrnInfoPtr pScrn = xf86Screens[pScreen->myNum];
   MGAPtr pMga = MGAPTR(pScrn);
   int numConfigs = 0;
   __GLXvisualConfig *pConfigs = 0;
   MGAConfigPrivPtr pMGAConfigs = 0;
   MGAConfigPrivPtr *pMGAConfigPtrs = 0;
   int i, db, depth, stencil, accum;

   switch ( pScrn->bitsPerPixel ) {
   case 8:
   case 24:
      break;

   case 16:
      numConfigs = 8;

      pConfigs = (__GLXvisualConfig*)xcalloc( sizeof(__GLXvisualConfig),
						numConfigs );
      if ( !pConfigs ) {
	 return FALSE;
      }

      pMGAConfigs = (MGAConfigPrivPtr)xcalloc( sizeof(MGAConfigPrivRec),
						 numConfigs );
      if ( !pMGAConfigs ) {
	 xfree( pConfigs );
	 return FALSE;
      }

      pMGAConfigPtrs = (MGAConfigPrivPtr*)xcalloc( sizeof(MGAConfigPrivPtr),
						     numConfigs );
      if ( !pMGAConfigPtrs ) {
	 xfree( pConfigs );
	 xfree( pMGAConfigs );
	 return FALSE;
      }

      for ( i = 0 ; i < numConfigs ; i++ ) {
	 pMGAConfigPtrs[i] = &pMGAConfigs[i];
      }

      i = 0;
      depth = 1;
      for ( accum = 0 ; accum <= 1 ; accum++ ) {
         for ( stencil = 0 ; stencil <= 1 ; stencil++ ) {
            for ( db = 1 ; db >= 0 ; db-- ) {
               pConfigs[i].vid			= -1;
               pConfigs[i].class		= -1;
               pConfigs[i].rgba			= TRUE;
               pConfigs[i].redSize		= 5;
               pConfigs[i].greenSize		= 6;
               pConfigs[i].blueSize		= 5;
               pConfigs[i].alphaSize		= 0;
               pConfigs[i].redMask		= 0x0000F800;
               pConfigs[i].greenMask		= 0x000007E0;
               pConfigs[i].blueMask		= 0x0000001F;
               pConfigs[i].alphaMask		= 0;
               if ( accum ) {
                  pConfigs[i].accumRedSize	= 16;
                  pConfigs[i].accumGreenSize	= 16;
                  pConfigs[i].accumBlueSize	= 16;
                  pConfigs[i].accumAlphaSize	= 0;
               } else {
                  pConfigs[i].accumRedSize	= 0;
                  pConfigs[i].accumGreenSize	= 0;
                  pConfigs[i].accumBlueSize	= 0;
                  pConfigs[i].accumAlphaSize	= 0;
               }
               if ( db ) {
                  pConfigs[i].doubleBuffer	= TRUE;
               } else {
                  pConfigs[i].doubleBuffer	= FALSE;
	       }
               pConfigs[i].stereo		= FALSE;
               pConfigs[i].bufferSize		= 16;
               if ( depth ) {
                  pConfigs[i].depthSize		= 16;
               } else {
                  pConfigs[i].depthSize		= 0;
	       }
               if ( stencil ) {
                  pConfigs[i].stencilSize	= 8;
               } else {
                  pConfigs[i].stencilSize	= 0;
	       }
               pConfigs[i].auxBuffers		= 0;
               pConfigs[i].level		= 0;
               if ( accum || stencil ) {
                  pConfigs[i].visualRating	= GLX_SLOW_VISUAL_EXT;
               } else {
                  pConfigs[i].visualRating	= GLX_NONE_EXT;
	       }
               pConfigs[i].transparentPixel	= 0;
               pConfigs[i].transparentRed	= 0;
               pConfigs[i].transparentGreen	= 0;
               pConfigs[i].transparentBlue	= 0;
               pConfigs[i].transparentAlpha	= 0;
               pConfigs[i].transparentIndex	= 0;
               i++;
            }
         }
      }
      if ( i != numConfigs ) {
         xf86DrvMsg( pScrn->scrnIndex, X_ERROR,
		     "[drm] Incorrect initialization of visuals\n" );
         return FALSE;
      }
      break;

   case 32:
      numConfigs = 8;

      pConfigs = (__GLXvisualConfig*)xcalloc( sizeof(__GLXvisualConfig),
						numConfigs );
      if ( !pConfigs ) {
	 return FALSE;
      }

      pMGAConfigs = (MGAConfigPrivPtr)xcalloc( sizeof(MGAConfigPrivRec),
						 numConfigs );
      if ( !pMGAConfigs ) {
	 xfree( pConfigs );
	 return FALSE;
      }

      pMGAConfigPtrs = (MGAConfigPrivPtr*)xcalloc( sizeof(MGAConfigPrivPtr),
						     numConfigs );
      if ( !pMGAConfigPtrs ) {
	 xfree( pConfigs );
	 xfree( pMGAConfigs );
	 return FALSE;
      }

      for ( i = 0 ; i < numConfigs ; i++ ) {
	 pMGAConfigPtrs[i] = &pMGAConfigs[i];
      }

      i = 0;
      for ( accum = 0 ; accum <= 1 ; accum++ ) {
         for ( depth = 0 ; depth <= 1 ; depth++ ) { /* and stencil */
            for ( db = 1 ; db >= 0 ; db-- ) {
               pConfigs[i].vid			= -1;
               pConfigs[i].class		= -1;
               pConfigs[i].rgba			= TRUE;
               pConfigs[i].redSize		= 8;
               pConfigs[i].greenSize		= 8;
               pConfigs[i].blueSize		= 8;
               pConfigs[i].alphaSize		= 0;
               pConfigs[i].redMask		= 0x00FF0000;
               pConfigs[i].greenMask		= 0x0000FF00;
               pConfigs[i].blueMask		= 0x000000FF;
               pConfigs[i].alphaMask		= 0x0;
               if ( accum ) {
                  pConfigs[i].accumRedSize	= 16;
                  pConfigs[i].accumGreenSize	= 16;
                  pConfigs[i].accumBlueSize	= 16;
                  pConfigs[i].accumAlphaSize	= 0;
               } else {
                  pConfigs[i].accumRedSize	= 0;
                  pConfigs[i].accumGreenSize	= 0;
                  pConfigs[i].accumBlueSize	= 0;
                  pConfigs[i].accumAlphaSize	= 0;
               }
               if ( db ) {
                  pConfigs[i].doubleBuffer	= TRUE;
               } else {
                  pConfigs[i].doubleBuffer	= FALSE;
	       }
               pConfigs[i].stereo		= FALSE;
               pConfigs[i].bufferSize		= 24;
               if ( depth ) {
		     pConfigs[i].depthSize	= 24;
                     pConfigs[i].stencilSize	= 8;
               }
               else {
                     pConfigs[i].depthSize	= 0;
                     pConfigs[i].stencilSize	= 0;
               }
               pConfigs[i].auxBuffers		= 0;
               pConfigs[i].level		= 0;
               if ( accum ) {
                  pConfigs[i].visualRating	= GLX_SLOW_VISUAL_EXT;
               } else {
                  pConfigs[i].visualRating	= GLX_NONE_EXT;
	       }
               pConfigs[i].transparentPixel	= 0;
               pConfigs[i].transparentRed	= 0;
               pConfigs[i].transparentGreen	= 0;
               pConfigs[i].transparentBlue	= 0;
               pConfigs[i].transparentAlpha	= 0;
               pConfigs[i].transparentIndex	= 0;
               i++;
            }
         }
      }
      if ( i != numConfigs ) {
         xf86DrvMsg( pScrn->scrnIndex, X_ERROR,
		     "[drm] Incorrect initialization of visuals\n" );
         return FALSE;
      }
      break;

   default:
      /* Unexpected bits/pixels */
      break;
   }

   pMga->numVisualConfigs = numConfigs;
   pMga->pVisualConfigs = pConfigs;
   pMga->pVisualConfigsPriv = pMGAConfigs;

   GlxSetVisualConfigs( numConfigs, pConfigs, (void **)pMGAConfigPtrs );

   return TRUE;
}

static Bool MGACreateContext( ScreenPtr pScreen, VisualPtr visual,
			      drmContext hwContext, void *pVisualConfigPriv,
			      DRIContextType contextStore )
{
   /* Nothing yet */
   return TRUE;
}

static void MGADestroyContext( ScreenPtr pScreen, drmContext hwContext,
			       DRIContextType contextStore )
{
   /* Nothing yet */
}


/* Quiescence, locking
 */
#define MGA_TIMEOUT		2048

static void MGAWaitForIdleDMA( ScrnInfoPtr pScrn )
{
   MGAPtr pMga = MGAPTR(pScrn);
   drmMGALock lock;
   int ret;
   int i = 0;

   memset( &lock, 0, sizeof(drmMGALock) );

   for (;;) {
      do {
         /* first ask for quiescent and flush */
         lock.flags = DRM_MGA_LOCK_QUIESCENT | DRM_MGA_LOCK_FLUSH;
         do {
	    ret = drmCommandWrite( pMga->drmFD, DRM_MGA_FLUSH,
                                   &lock, sizeof( drmMGALock ) );
         } while ( ret == -EBUSY && i++ < DRM_MGA_IDLE_RETRY );

         /* if it's still busy just try quiescent */
         if ( ret == -EBUSY ) { 
            lock.flags = DRM_MGA_LOCK_QUIESCENT;
            do {
	       ret = drmCommandWrite( pMga->drmFD, DRM_MGA_FLUSH,
                                      &lock, sizeof( drmMGALock ) );
            } while ( ret == -EBUSY && i++ < DRM_MGA_IDLE_RETRY );
         }
      } while ( ( ret == -EBUSY ) && ( i++ < MGA_TIMEOUT ) );

      if ( ret == 0 )
	 return;

      xf86DrvMsg( pScrn->scrnIndex, X_ERROR,
		  "[dri] Idle timed out, resetting engine...\n" );

      drmCommandNone( pMga->drmFD, DRM_MGA_RESET );
   }
}


void MGAGetQuiescence( ScrnInfoPtr pScrn )
{
   MGAPtr pMga = MGAPTR(pScrn);

   DRILock( screenInfo.screens[pScrn->scrnIndex], 0 );
   pMga->haveQuiescense = 1;

   if ( pMga->directRenderingEnabled ) {
      MGAFBLayout *pLayout = &pMga->CurrentLayout;

      MGAWaitForIdleDMA( pScrn );

      WAITFIFO( 11 );
      OUTREG( MGAREG_MACCESS, pMga->MAccess );
      OUTREG( MGAREG_PITCH, pLayout->displayWidth );

      pMga->PlaneMask = ~0;
      OUTREG( MGAREG_PLNWT, pMga->PlaneMask );

      pMga->BgColor = 0;
      pMga->FgColor = 0;
      OUTREG( MGAREG_BCOL, pMga->BgColor );
      OUTREG( MGAREG_FCOL, pMga->FgColor );
      OUTREG( MGAREG_SRCORG, pMga->realSrcOrg );

      pMga->SrcOrg = 0;
      OUTREG( MGAREG_DSTORG, pMga->DstOrg );
      OUTREG( MGAREG_OPMODE, MGAOPM_DMA_BLIT );
      OUTREG( MGAREG_CXBNDRY, 0xFFFF0000 ); /* (maxX << 16) | minX */
      OUTREG( MGAREG_YTOP, 0x00000000 );    /* minPixelPointer */
      OUTREG( MGAREG_YBOT, 0x007FFFFF );    /* maxPixelPointer */

      pMga->AccelFlags &= ~CLIPPER_ON;
   }
}

void MGAGetQuiescenceShared( ScrnInfoPtr pScrn )
{
   MGAPtr pMga = MGAPTR(pScrn);
   MGAEntPtr pMGAEnt = pMga->entityPrivate;
   MGAPtr pMGA2 = MGAPTR(pMGAEnt->pScrn_2);

   DRILock( screenInfo.screens[pMGAEnt->pScrn_1->scrnIndex], 0 );

   pMga = MGAPTR(pMGAEnt->pScrn_1);
   pMga->haveQuiescense = 1;
   pMGA2->haveQuiescense = 1;

   if ( pMGAEnt->directRenderingEnabled ) {
      MGAWaitForIdleDMA( pMGAEnt->pScrn_1 );
      pMga->RestoreAccelState( pScrn );
      xf86SetLastScrnFlag( pScrn->entityList[0], pScrn->scrnIndex );
   }
}

static void MGASwapContext( ScreenPtr pScreen )
{
   ScrnInfoPtr pScrn = xf86Screens[pScreen->myNum];
   MGAPtr pMga = MGAPTR(pScrn);

   /* Arrange for dma_quiescence and xaa sync to be called as
    * appropriate.
    */
   pMga->haveQuiescense = 0;
   pMga->AccelInfoRec->NeedToSync = TRUE;
}

static void MGASwapContextShared( ScreenPtr pScreen )
{
   ScrnInfoPtr pScrn = xf86Screens[pScreen->myNum];
   MGAPtr pMga = MGAPTR(pScrn);
   MGAEntPtr pMGAEnt = pMga->entityPrivate;
   MGAPtr pMGA2 = MGAPTR(pMGAEnt->pScrn_2);

   pMga = MGAPTR(pMGAEnt->pScrn_1);

   pMga->haveQuiescense = 0;
   pMga->AccelInfoRec->NeedToSync = TRUE;

   pMGA2->haveQuiescense = 0;
   pMGA2->AccelInfoRec->NeedToSync = TRUE;
}

/* This is really only called from validate/postvalidate as we
 * override the dri lock/unlock.  Want to remove validate/postvalidate
 * processing, but need to remove all client-side use of drawable lock
 * first (otherwise there is noone recover when a client dies holding
 * the drawable lock).
 *
 * What does this mean?
 *
 *   - The above code gets executed every time a
 *     window changes shape or the focus changes, which isn't really
 *     optimal.
 *   - The X server therefore believes it needs to do an XAA sync
 *     *and* a dma quiescense ioctl each time that happens.
 *
 * We don't wrap wakeuphandler any longer, so at least we can say that
 * this doesn't happen *every time the mouse moves*...
 */
static void
MGADRISwapContext( ScreenPtr pScreen, DRISyncType syncType,
		   DRIContextType oldContextType, void *oldContext,
		   DRIContextType newContextType, void *newContext )
{
#if 0
   if ( syncType == DRI_3D_SYNC &&
	oldContextType == DRI_2D_CONTEXT &&
	newContextType == DRI_2D_CONTEXT )
   {
      MGASwapContext( pScreen );
   }
#endif
}

static void
MGADRISwapContextShared( ScreenPtr pScreen, DRISyncType syncType,
			  DRIContextType oldContextType, void *oldContext,
			  DRIContextType newContextType, void *newContext )
{
#if 0
   if ( syncType == DRI_3D_SYNC &&
	oldContextType == DRI_2D_CONTEXT &&
	newContextType == DRI_2D_CONTEXT )
   {
      MGASwapContextShared( pScreen );
   }
#endif
}


static void MGAWakeupHandler( int screenNum, pointer wakeupData,
			      unsigned long result, pointer pReadmask )
{
    ScreenPtr pScreen = screenInfo.screens[screenNum];
    ScrnInfoPtr pScrn = xf86Screens[pScreen->myNum];

    if ( xf86IsEntityShared( pScrn->entityList[0] ) ) {
        MGASwapContextShared( pScreen );
    } else {
        MGASwapContext( pScreen );
    }
}

static void MGABlockHandler( int screenNum, pointer blockData,
			     pointer pTimeout, pointer pReadmask )

{
   ScreenPtr pScreen = screenInfo.screens[screenNum];
   ScrnInfoPtr pScrn = xf86Screens[pScreen->myNum];
   MGAPtr pMga = MGAPTR(pScrn);
   MGAEntPtr pMGAEnt;

   if ( pMga->haveQuiescense ) {
      if ( xf86IsEntityShared( pScrn->entityList[0] ) ) {
	 /* Restore to first screen */
	 pMga->RestoreAccelState( pScrn );
	 xf86SetLastScrnFlag( pScrn->entityList[0], pScrn->scrnIndex );
	 pMGAEnt = pMga->entityPrivate;

	 if ( pMGAEnt->directRenderingEnabled ) {
	    DRIUnlock( screenInfo.screens[pMGAEnt->pScrn_1->scrnIndex] );
	 }
      } else {
	 if ( pMga->directRenderingEnabled ) {
	    DRIUnlock( pScreen );
	 }
      }
      pMga->haveQuiescense = 0;
   }
}

void MGASelectBuffer( ScrnInfoPtr pScrn, int which )
{
   MGAPtr pMga = MGAPTR(pScrn);
   MGADRIPtr pMGADRI = (MGADRIPtr)pMga->pDRIInfo->devPrivate;

   switch ( which ) {
   case MGA_BACK:
      OUTREG( MGAREG_DSTORG, pMGADRI->backOffset );
      OUTREG( MGAREG_SRCORG, pMGADRI->backOffset );
      break;
   case MGA_DEPTH:
      OUTREG( MGAREG_DSTORG, pMGADRI->depthOffset );
      OUTREG( MGAREG_SRCORG, pMGADRI->depthOffset );
      break;
   default:
   case MGA_FRONT:
      OUTREG( MGAREG_DSTORG, pMGADRI->frontOffset );
      OUTREG( MGAREG_SRCORG, pMGADRI->frontOffset );
      break;
   }
}


static unsigned int mylog2( unsigned int n )
{
   unsigned int log2 = 1;
   while ( n > 1 ) n >>= 1, log2++;
   return log2;
}

static Bool MGADRIAgpInit(ScreenPtr pScreen)
{
   ScrnInfoPtr pScrn = xf86Screens[pScreen->myNum];
   MGAPtr pMga = MGAPTR(pScrn);
   MGADRIServerPrivatePtr pMGADRIServer = pMga->DRIServerInfo;
   unsigned long mode;
   unsigned int vendor, device;
   int ret, count, i;

   if(pMga->agpSize < 12)pMga->agpSize = 12;
   if(pMga->agpSize > 64)pMga->agpSize = 64; /* cap */

   /* FIXME: Make these configurable...
    */
   pMGADRIServer->agp.size = pMga->agpSize * 1024 * 1024;

   pMGADRIServer->warp.offset = 0;
   pMGADRIServer->warp.size = MGA_WARP_UCODE_SIZE;

   pMGADRIServer->primary.offset = (pMGADRIServer->warp.offset +
				    pMGADRIServer->warp.size);
   pMGADRIServer->primary.size = 1024 * 1024;

   pMGADRIServer->buffers.offset = (pMGADRIServer->primary.offset +
				    pMGADRIServer->primary.size);
   pMGADRIServer->buffers.size = MGA_NUM_BUFFERS * MGA_BUFFER_SIZE;


   pMGADRIServer->agpTextures.offset = (pMGADRIServer->buffers.offset +
                                    pMGADRIServer->buffers.size);

   pMGADRIServer->agpTextures.size = pMGADRIServer->agp.size -
                                     pMGADRIServer->agpTextures.offset;

   if ( drmAgpAcquire( pMga->drmFD ) < 0 ) {
      xf86DrvMsg( pScreen->myNum, X_ERROR, "[agp] AGP not available\n" );
      return FALSE;
   }

   mode   = drmAgpGetMode( pMga->drmFD );        /* Default mode */
   vendor = drmAgpVendorId( pMga->drmFD );
   device = drmAgpDeviceId( pMga->drmFD );

   mode &= ~MGA_AGP_MODE_MASK;
   switch ( pMga->agpMode ) {
   case 4:
      mode |= MGA_AGP_4X_MODE;
   case 2:
      mode |= MGA_AGP_2X_MODE;
   case 1:
   default:
      mode |= MGA_AGP_1X_MODE;
   }

   xf86DrvMsg( pScreen->myNum, X_INFO,
	       "[agp] Mode 0x%08lx [AGP 0x%04x/0x%04x; Card 0x%04x/0x%04x]\n",
	       mode, vendor, device,
	       pMga->PciInfo->vendor,
	       pMga->PciInfo->chipType );

   if ( drmAgpEnable( pMga->drmFD, mode ) < 0 ) {
      xf86DrvMsg( pScreen->myNum, X_ERROR, "[agp] AGP not enabled\n" );
      drmAgpRelease( pMga->drmFD );
      return FALSE;
   }

   if ( pMga->Chipset == PCI_CHIP_MGAG200 ) {
      switch ( pMga->agpMode ) {
      case 2:
	 xf86DrvMsg( pScreen->myNum, X_INFO,
		     "[drm] Enabling AGP 2x PLL encoding\n" );
	 OUTREG( MGAREG_AGP_PLL, MGA_AGP2XPLL_ENABLE );
	 break;

      case 1:
      default:
	 xf86DrvMsg( pScreen->myNum, X_INFO,
		     "[drm] Disabling AGP 2x PLL encoding\n" );
	 OUTREG( MGAREG_AGP_PLL, MGA_AGP2XPLL_DISABLE );
	 pMga->agpMode = 1;
	 break;
      }
   }

   ret = drmAgpAlloc( pMga->drmFD, pMGADRIServer->agp.size,
		      0, NULL, &pMGADRIServer->agp.handle );
   if ( ret < 0 ) {
      xf86DrvMsg( pScreen->myNum, X_ERROR, "[agp] Out of memory (%d)\n", ret );
      drmAgpRelease( pMga->drmFD );
      return FALSE;
   }
   xf86DrvMsg( pScreen->myNum, X_INFO,
	       "[agp] %d kB allocated with handle 0x%08x\n",
	       pMGADRIServer->agp.size/1024, pMGADRIServer->agp.handle );

   if ( drmAgpBind( pMga->drmFD, pMGADRIServer->agp.handle, 0 ) < 0 ) {
      xf86DrvMsg( pScreen->myNum, X_ERROR, "[agp] Could not bind memory\n" );
      drmAgpFree( pMga->drmFD, pMGADRIServer->agp.handle );
      drmAgpRelease( pMga->drmFD );
      return FALSE;
   }

   /* WARP microcode space
    */
   if ( drmAddMap( pMga->drmFD,
		   pMGADRIServer->warp.offset,
		   pMGADRIServer->warp.size,
		   DRM_AGP, DRM_READ_ONLY,
		   &pMGADRIServer->warp.handle ) < 0 ) {
      xf86DrvMsg( pScreen->myNum, X_ERROR,
		  "[agp] Could not add WARP microcode mapping\n" );
      return FALSE;
   }
   xf86DrvMsg( pScreen->myNum, X_INFO,
	       "[agp] WARP microcode handle = 0x%08lx\n",
	       pMGADRIServer->warp.handle );

   if ( drmMap( pMga->drmFD,
		pMGADRIServer->warp.handle,
		pMGADRIServer->warp.size,
		&pMGADRIServer->warp.map ) < 0 ) {
      xf86DrvMsg( pScreen->myNum, X_ERROR,
		  "[agp] Could not map WARP microcode\n" );
      return FALSE;
   }
   xf86DrvMsg( pScreen->myNum, X_INFO,
	       "[agp] WARP microcode mapped at 0x%08lx\n",
	       (unsigned long)pMGADRIServer->warp.map );

   /* Primary DMA space
    */
   if ( drmAddMap( pMga->drmFD,
		   pMGADRIServer->primary.offset,
		   pMGADRIServer->primary.size,
		   DRM_AGP, DRM_READ_ONLY,
		   &pMGADRIServer->primary.handle ) < 0 ) {
      xf86DrvMsg( pScreen->myNum, X_ERROR,
		  "[agp] Could not add primary DMA mapping\n" );
      return FALSE;
   }
   xf86DrvMsg( pScreen->myNum, X_INFO,
	       "[agp] Primary DMA handle = 0x%08lx\n",
	       pMGADRIServer->primary.handle );

   if ( drmMap( pMga->drmFD,
		pMGADRIServer->primary.handle,
		pMGADRIServer->primary.size,
		&pMGADRIServer->primary.map ) < 0 ) {
      xf86DrvMsg( pScreen->myNum, X_ERROR,
		  "[agp] Could not map primary DMA\n" );
      return FALSE;
   }
   xf86DrvMsg( pScreen->myNum, X_INFO,
	       "[agp] Primary DMA mapped at 0x%08lx\n",
	       (unsigned long)pMGADRIServer->primary.map );

   /* DMA buffers
    */
   if ( drmAddMap( pMga->drmFD,
		   pMGADRIServer->buffers.offset,
		   pMGADRIServer->buffers.size,
		   DRM_AGP, 0,
		   &pMGADRIServer->buffers.handle ) < 0 ) {
      xf86DrvMsg( pScreen->myNum, X_ERROR,
		  "[agp] Could not add DMA buffers mapping\n" );
      return FALSE;
   }
   xf86DrvMsg( pScreen->myNum, X_INFO,
	       "[agp] DMA buffers handle = 0x%08lx\n",
	       pMGADRIServer->buffers.handle );

   if ( drmMap( pMga->drmFD,
		pMGADRIServer->buffers.handle,
		pMGADRIServer->buffers.size,
		&pMGADRIServer->buffers.map ) < 0 ) {
      xf86DrvMsg( pScreen->myNum, X_ERROR,
		  "[agp] Could not map DMA buffers\n" );
      return FALSE;
   }
   xf86DrvMsg( pScreen->myNum, X_INFO,
	       "[agp] DMA buffers mapped at 0x%08lx\n",
	       (unsigned long)pMGADRIServer->buffers.map );

   count = drmAddBufs( pMga->drmFD,
		       MGA_NUM_BUFFERS, MGA_BUFFER_SIZE,
		       DRM_AGP_BUFFER, pMGADRIServer->buffers.offset );
   if ( count <= 0 ) {
      xf86DrvMsg( pScrn->scrnIndex, X_INFO,
		  "[drm] failure adding %d %d byte DMA buffers\n",
		  MGA_NUM_BUFFERS, MGA_BUFFER_SIZE );
      return FALSE;
   }
   xf86DrvMsg( pScreen->myNum, X_INFO,
	       "[drm] Added %d %d byte DMA buffers\n",
	       count, MGA_BUFFER_SIZE );

   i = mylog2(pMGADRIServer->agpTextures.size / MGA_NR_TEX_REGIONS);
   if(i < MGA_LOG_MIN_TEX_REGION_SIZE)
      i = MGA_LOG_MIN_TEX_REGION_SIZE;
   pMGADRIServer->agpTextures.size = (pMGADRIServer->agpTextures.size >> i) << i;

   if ( drmAddMap( pMga->drmFD,
                   pMGADRIServer->agpTextures.offset,
                   pMGADRIServer->agpTextures.size,
                   DRM_AGP, 0,
                   &pMGADRIServer->agpTextures.handle ) < 0 ) {
      xf86DrvMsg( pScreen->myNum, X_ERROR,
                  "[agp] Could not add agpTexture mapping\n" );
      return FALSE;
   }
/* should i map it ? */
   xf86DrvMsg( pScreen->myNum, X_INFO,
               "[agp] agpTexture handle = 0x%08lx\n",
               pMGADRIServer->agpTextures.handle );
   xf86DrvMsg( pScreen->myNum, X_INFO,
               "[agp] agpTexture size: %d kb\n", pMGADRIServer->agpTextures.size/1024 );

   return TRUE;
}

static Bool MGADRIMapInit( ScreenPtr pScreen )
{
   ScrnInfoPtr pScrn = xf86Screens[pScreen->myNum];
   MGAPtr pMga = MGAPTR(pScrn);
   MGADRIServerPrivatePtr pMGADRIServer = pMga->DRIServerInfo;

   pMGADRIServer->registers.size = MGAIOMAPSIZE;

   if ( drmAddMap( pMga->drmFD,
		   (drmHandle)pMga->IOAddress,
		   pMGADRIServer->registers.size,
		   DRM_REGISTERS, DRM_READ_ONLY,
		   &pMGADRIServer->registers.handle ) < 0 ) {
      xf86DrvMsg( pScreen->myNum, X_ERROR,
		  "[drm] Could not add MMIO registers mapping\n" );
      return FALSE;
   }
   xf86DrvMsg( pScreen->myNum, X_INFO,
	       "[drm] Registers handle = 0x%08lx\n",
	       pMGADRIServer->registers.handle );

   pMGADRIServer->status.size = SAREA_MAX;

   if ( drmAddMap( pMga->drmFD, 0, pMGADRIServer->status.size,
		   DRM_SHM, DRM_READ_ONLY | DRM_LOCKED | DRM_KERNEL,
		   &pMGADRIServer->status.handle ) < 0 ) {
      xf86DrvMsg( pScreen->myNum, X_ERROR,
		  "[drm] Could not add status page mapping\n" );
      return FALSE;
   }
   xf86DrvMsg( pScreen->myNum, X_INFO,
	       "[drm] Status handle = 0x%08lx\n",
	       pMGADRIServer->status.handle );

   if ( drmMap( pMga->drmFD,
		pMGADRIServer->status.handle,
		pMGADRIServer->status.size,
		&pMGADRIServer->status.map ) < 0 ) {
      xf86DrvMsg( pScreen->myNum, X_ERROR,
		  "[agp] Could not map status page\n" );
      return FALSE;
   }
   xf86DrvMsg( pScreen->myNum, X_INFO,
	       "[agp] Status page mapped at 0x%08lx\n",
	       (unsigned long)pMGADRIServer->status.map );

   return TRUE;
}

static Bool MGADRIKernelInit( ScreenPtr pScreen )
{
   ScrnInfoPtr pScrn = xf86Screens[pScreen->myNum];
   MGAPtr pMga = MGAPTR(pScrn);
   MGADRIServerPrivatePtr pMGADRIServer = pMga->DRIServerInfo;
   drmMGAInit init;
   int ret;

   memset( &init, 0, sizeof(drmMGAInit) );

   init.func = MGA_INIT_DMA;
   init.sarea_priv_offset = sizeof(XF86DRISAREARec);

   switch ( pMga->Chipset ) {
   case PCI_CHIP_MGAG550:
   case PCI_CHIP_MGAG400:
      init.chipset = MGA_CARD_TYPE_G400;
      break;
   case PCI_CHIP_MGAG200:
   case PCI_CHIP_MGAG200_PCI:
      init.chipset = MGA_CARD_TYPE_G200;
      break;
   default:
      return FALSE;
   }
   init.sgram = !pMga->HasSDRAM;

   init.maccess = pMga->MAccess;

   init.fb_cpp		= pScrn->bitsPerPixel / 8;
   init.front_offset	= pMGADRIServer->frontOffset;
   init.front_pitch	= pMGADRIServer->frontPitch / init.fb_cpp;
   init.back_offset	= pMGADRIServer->backOffset;
   init.back_pitch	= pMGADRIServer->backPitch / init.fb_cpp;

   init.depth_cpp	= pScrn->bitsPerPixel / 8;
   init.depth_offset	= pMGADRIServer->depthOffset;
   init.depth_pitch	= pMGADRIServer->depthPitch / init.depth_cpp;

   init.texture_offset[0] = pMGADRIServer->textureOffset;
   init.texture_size[0] = pMGADRIServer->textureSize;

   init.fb_offset = pMGADRIServer->fb.handle;
   init.mmio_offset = pMGADRIServer->registers.handle;
   init.status_offset = pMGADRIServer->status.handle;

   init.warp_offset = pMGADRIServer->warp.handle;
   init.primary_offset = pMGADRIServer->primary.handle;
   init.buffers_offset = pMGADRIServer->buffers.handle;

   init.texture_offset[1] = pMGADRIServer->agpTextures.handle;
   init.texture_size[1] = pMGADRIServer->agpTextures.size;

   ret = drmCommandWrite( pMga->drmFD, DRM_MGA_INIT, &init, sizeof(drmMGAInit));
   if ( ret < 0 ) {
      xf86DrvMsg( pScrn->scrnIndex, X_ERROR,
		  "[drm] Failed to initialize DMA! (%d)\n", ret );
      return FALSE;
   }

#if 0
   /* FIXME: This is just here to clean up after the engine reset test
    * in the kernel module.  Please remove it later...
    */
   pMga->GetQuiescence( pScrn );
#endif

   return TRUE;
}

static void MGADRIIrqInit(MGAPtr pMga, ScreenPtr pScreen)
{
   ScrnInfoPtr pScrn = xf86Screens[pScreen->myNum];

   /*   version = drmGetVersion(pMga->drmFD);
      if ( version ) {
         if ( version->version_major != 3 ||
	      version->version_minor < 0 ) {*/
   if (!pMga->irq) {
      pMga->irq = drmGetInterruptFromBusID(
	 pMga->drmFD,
	 ((pciConfigPtr)pMga->PciInfo->thisCard)->busnum,
	 ((pciConfigPtr)pMga->PciInfo->thisCard)->devnum,
	 ((pciConfigPtr)pMga->PciInfo->thisCard)->funcnum);

      if((drmCtlInstHandler(pMga->drmFD, pMga->irq)) != 0) {
	 xf86DrvMsg(pScrn->scrnIndex, X_INFO,
		    "[drm] failure adding irq handler, "
		    "there is a device already using that irq\n"
		    "[drm] falling back to irq-free operation\n");
	 pMga->irq = 0;
      } else {
          pMga->reg_ien = INREG( MGAREG_IEN );
      }
   }

   if (pMga->irq)
      xf86DrvMsg(pScrn->scrnIndex, X_INFO,
		 "[drm] dma control initialized, using IRQ %d\n",
		 pMga->irq);
}

static Bool MGADRIBuffersInit( ScreenPtr pScreen )
{
   ScrnInfoPtr pScrn = xf86Screens[pScreen->myNum];
   MGAPtr pMga = MGAPTR(pScrn);
   MGADRIServerPrivatePtr pMGADRIServer = pMga->DRIServerInfo;


   pMGADRIServer->drmBuffers = drmMapBufs( pMga->drmFD );
   if ( !pMGADRIServer->drmBuffers ) {
	xf86DrvMsg( pScreen->myNum, X_ERROR,
		    "[drm] Failed to map DMA buffers list\n" );
	return FALSE;
    }
    xf86DrvMsg( pScreen->myNum, X_INFO,
		"[drm] Mapped %d DMA buffers\n",
		pMGADRIServer->drmBuffers->count );

    return TRUE;
}


Bool MGADRIScreenInit( ScreenPtr pScreen )
{
   ScrnInfoPtr pScrn = xf86Screens[pScreen->myNum];
   MGAPtr pMga = MGAPTR(pScrn);
   DRIInfoPtr pDRIInfo;
   MGADRIPtr pMGADRI;
   MGADRIServerPrivatePtr pMGADRIServer;

   switch(pMga->Chipset) {
   case PCI_CHIP_MGAG550:
   case PCI_CHIP_MGAG400:
   case PCI_CHIP_MGAG200:
#if 0
   case PCI_CHIP_MGAG200_PCI:
#endif
      break;
   default:
      xf86DrvMsg(pScrn->scrnIndex, X_ERROR, "[drm] Direct rendering only supported with G200/G400/G550 AGP\n");
      return FALSE;
   }

   /* Check that the GLX, DRI, and DRM modules have been loaded by testing
    * for canonical symbols in each module.
    */
   if ( !xf86LoaderCheckSymbol( "GlxSetVisualConfigs" ) )	return FALSE;
   if ( !xf86LoaderCheckSymbol( "DRIScreenInit" ) )		return FALSE;
   if ( !xf86LoaderCheckSymbol( "drmAvailable" ) )		return FALSE;
   if ( !xf86LoaderCheckSymbol( "DRIQueryVersion" ) ) {
      xf86DrvMsg( pScreen->myNum, X_ERROR,
		  "[dri] MGADRIScreenInit failed (libdri.a too old)\n" );
      return FALSE;
   }

   /* Check the DRI version */
   {
      int major, minor, patch;
      DRIQueryVersion( &major, &minor, &patch );
      if ( major != 4 || minor < 0 ) {
         xf86DrvMsg( pScreen->myNum, X_ERROR,
		     "[dri] MGADRIScreenInit failed because of a version mismatch.\n"
		     "[dri] libDRI version = %d.%d.%d but version 4.0.x is needed.\n"
		     "[dri] Disabling the DRI.\n",
		     major, minor, patch );
         return FALSE;
      }
   }

   xf86DrvMsg( pScreen->myNum, X_INFO,
	       "[drm] bpp: %d depth: %d\n",
	       pScrn->bitsPerPixel, pScrn->depth );

   if ( (pScrn->bitsPerPixel / 8) != 2 &&
	(pScrn->bitsPerPixel / 8) != 4 ) {
      xf86DrvMsg( pScreen->myNum, X_ERROR,
		  "[dri] Direct rendering only supported in 16 and 32 bpp modes\n" );
      return FALSE;
   }

   pDRIInfo = DRICreateInfoRec();
   if ( !pDRIInfo ) {
      xf86DrvMsg( pScreen->myNum, X_ERROR,
		  "[dri] DRICreateInfoRec() failed\n" );
      return FALSE;
   }
   pMga->pDRIInfo = pDRIInfo;

   pDRIInfo->drmDriverName = MGAKernelDriverName;
   pDRIInfo->clientDriverName = MGAClientDriverName;
   pDRIInfo->busIdString = xalloc(64);
   sprintf( pDRIInfo->busIdString, "PCI:%d:%d:%d",
	    ((pciConfigPtr)pMga->PciInfo->thisCard)->busnum,
	    ((pciConfigPtr)pMga->PciInfo->thisCard)->devnum,
	    ((pciConfigPtr)pMga->PciInfo->thisCard)->funcnum );
   pDRIInfo->ddxDriverMajorVersion = MGA_MAJOR_VERSION;
   pDRIInfo->ddxDriverMinorVersion = MGA_MINOR_VERSION;
   pDRIInfo->ddxDriverPatchVersion = MGA_PATCHLEVEL;
   pDRIInfo->frameBufferPhysicalAddress = pMga->FbAddress;
   pDRIInfo->frameBufferSize = pMga->FbMapSize;
   pDRIInfo->frameBufferStride = pScrn->displayWidth*(pScrn->bitsPerPixel/8);
   pDRIInfo->ddxDrawableTableEntry = MGA_MAX_DRAWABLES;

   pDRIInfo->wrap.BlockHandler = MGABlockHandler;
   pDRIInfo->wrap.WakeupHandler = MGAWakeupHandler;
   pDRIInfo->wrap.ValidateTree = NULL;
   pDRIInfo->wrap.PostValidateTree = NULL;

   pDRIInfo->createDummyCtx = TRUE;
   pDRIInfo->createDummyCtxPriv = FALSE;

   if ( SAREA_MAX_DRAWABLES < MGA_MAX_DRAWABLES ) {
      pDRIInfo->maxDrawableTableEntry = SAREA_MAX_DRAWABLES;
   } else {
      pDRIInfo->maxDrawableTableEntry = MGA_MAX_DRAWABLES;
   }

   /* For now the mapping works by using a fixed size defined
    * in the SAREA header.
    */
   if ( sizeof(XF86DRISAREARec) + sizeof(MGASAREAPrivRec) > SAREA_MAX ) {
      xf86DrvMsg( pScrn->scrnIndex, X_ERROR,
		  "[drm] Data does not fit in SAREA\n" );
      return FALSE;
   }

   xf86DrvMsg( pScrn->scrnIndex, X_INFO,
	       "[drm] Sarea %d+%d: %d\n",
	       sizeof(XF86DRISAREARec), sizeof(MGASAREAPrivRec),
	       sizeof(XF86DRISAREARec) + sizeof(MGASAREAPrivRec) );

   pDRIInfo->SAREASize = SAREA_MAX;

   pMGADRI = (MGADRIPtr)xcalloc( sizeof(MGADRIRec), 1 );
   if ( !pMGADRI ) {
      DRIDestroyInfoRec( pMga->pDRIInfo );
      pMga->pDRIInfo = 0;
      xf86DrvMsg( pScrn->scrnIndex, X_ERROR,
		  "[drm] Failed to allocate memory for private record\n" );
      return FALSE;
   }

   pMGADRIServer = (MGADRIServerPrivatePtr)
      xcalloc( sizeof(MGADRIServerPrivateRec), 1 );
   if ( !pMGADRIServer ) {
      xfree( pMGADRI );
      DRIDestroyInfoRec( pMga->pDRIInfo );
      pMga->pDRIInfo = 0;
      xf86DrvMsg( pScrn->scrnIndex, X_ERROR,
		  "[drm] Failed to allocate memory for private record\n" );
      return FALSE;
   }
   pMga->DRIServerInfo = pMGADRIServer;

   pDRIInfo->devPrivate = pMGADRI;
   pDRIInfo->devPrivateSize = sizeof(MGADRIRec);
   pDRIInfo->contextSize = sizeof(MGADRIContextRec);

   pDRIInfo->CreateContext = MGACreateContext;
   pDRIInfo->DestroyContext = MGADestroyContext;
   if ( xf86IsEntityShared( pScrn->entityList[0] ) ) {
      pDRIInfo->SwapContext = MGADRISwapContextShared;
   } else {
      pDRIInfo->SwapContext = MGADRISwapContext;
   }

   switch( pScrn->bitsPerPixel ) {
   case 8:
       pDRIInfo->InitBuffers = Mga8DRIInitBuffers;
       pDRIInfo->MoveBuffers = Mga8DRIMoveBuffers;
   case 16:
       pDRIInfo->InitBuffers = Mga16DRIInitBuffers;
       pDRIInfo->MoveBuffers = Mga16DRIMoveBuffers;
   case 24:
       pDRIInfo->InitBuffers = Mga24DRIInitBuffers;
       pDRIInfo->MoveBuffers = Mga24DRIMoveBuffers;
   case 32:
       pDRIInfo->InitBuffers = Mga32DRIInitBuffers;
       pDRIInfo->MoveBuffers = Mga32DRIMoveBuffers;
   }

   pDRIInfo->bufferRequests = DRI_ALL_WINDOWS;

   if ( !DRIScreenInit( pScreen, pDRIInfo, &pMga->drmFD ) ) {
      xfree( pMGADRIServer );
      pMga->DRIServerInfo = 0;
      xfree( pDRIInfo->devPrivate );
      pDRIInfo->devPrivate = 0;
      DRIDestroyInfoRec( pMga->pDRIInfo );
      pMga->pDRIInfo = 0;
      xf86DrvMsg( pScreen->myNum, X_ERROR,
		  "[drm] DRIScreenInit failed.  Disabling DRI.\n" );
      return FALSE;
   }

   /* Check the DRM versioning */
   {
      drmVersionPtr version;

      /* Check the DRM lib version.
	 drmGetLibVersion was not supported in version 1.0, so check for
	 symbol first to avoid possible crash or hang.
       */
      if (xf86LoaderCheckSymbol("drmGetLibVersion")) {
         version = drmGetLibVersion(pMga->drmFD);
      }
      else {
	 /* drmlib version 1.0.0 didn't have the drmGetLibVersion
	    entry point.  Fake it by allocating a version record
	    via drmGetVersion and changing it to version 1.0.0
	  */
	 version = drmGetVersion(pMga->drmFD);
	 version->version_major      = 1;
	 version->version_minor      = 0;
	 version->version_patchlevel = 0;
      }

      if (version) {
	 if (version->version_major != 1 ||
	     version->version_minor < 1) {
	     /* incompatible drm library version */
	    xf86DrvMsg(pScreen->myNum, X_ERROR,
		       "[dri] MGADRIScreenInit failed because of a version mismatch.\n"
		       "[dri] libdrm.a module version is %d.%d.%d but version 1.1.x is needed.\n"
		       "[dri] Disabling DRI.\n",
		       version->version_major,
		       version->version_minor,
		       version->version_patchlevel);
	    drmFreeVersion(version);
	    MGADRICloseScreen( pScreen );		/* FIXME: ??? */
	    return FALSE;
	 }
	 drmFreeVersion(version);
      }

      /* Check the MGA DRM version */
      version = drmGetVersion(pMga->drmFD);
      if ( version ) {
         if ( version->version_major != 3 ||
	      version->version_minor < 0 ) {
            /* incompatible drm version */
            xf86DrvMsg( pScreen->myNum, X_ERROR,
			"[dri] MGADRIScreenInit failed because of a version mismatch.\n"
			"[dri] mga.o kernel module version is %d.%d.%d but version 3.0.x is needed.\n"
			"[dri] Disabling DRI.\n",
			version->version_major,
			version->version_minor,
			version->version_patchlevel );
            drmFreeVersion( version );
	    MGADRICloseScreen( pScreen );		/* FIXME: ??? */
            return FALSE;
         }
         drmFreeVersion( version );
      }
   }

#if 0
   /* Calculate texture constants for AGP texture space.
    * FIXME: move!
    */
   {
      CARD32 agpTextureOffset = MGA_DMA_BUF_SZ * MGA_DMA_BUF_NR;
      CARD32 agpTextureSize = pMGADRI->agp.size - agpTextureOffset;

      i = mylog2(agpTextureSize / MGA_NR_TEX_REGIONS);
      if (i < MGA_LOG_MIN_TEX_REGION_SIZE)
         i = MGA_LOG_MIN_TEX_REGION_SIZE;

      pMGADRI->logAgpTextureGranularity = i;
      pMGADRI->agpTextureSize = (agpTextureSize >> i) << i;
      pMGADRI->agpTextureOffset = agpTextureOffset;
   }
#endif

   if ( !MGADRIAgpInit( pScreen ) ) {
      DRICloseScreen( pScreen );
      return FALSE;
   }

   if ( !MGADRIMapInit( pScreen ) ) {
      DRICloseScreen( pScreen );
      return FALSE;
   }
   {
       void *scratch_ptr;
       int scratch_int;

       DRIGetDeviceInfo(pScreen, &pMGADRIServer->fb.handle,
			&scratch_int, &scratch_int, 
			&scratch_int, &scratch_int,
			&scratch_ptr);
   }

   if ( !MGAInitVisualConfigs( pScreen ) ) {
      DRICloseScreen( pScreen );
      return FALSE;
   }
   xf86DrvMsg( pScrn->scrnIndex, X_INFO, "[dri] visual configs initialized\n" );

   return TRUE;
}


Bool MGADRIFinishScreenInit( ScreenPtr pScreen )
{
   ScrnInfoPtr pScrn = xf86Screens[pScreen->myNum];
   MGAPtr pMga = MGAPTR(pScrn);
   MGADRIServerPrivatePtr pMGADRIServer = pMga->DRIServerInfo;
   MGADRIPtr pMGADRI = (MGADRIPtr)pMga->pDRIInfo->devPrivate;
   int i;

   if ( !pMga->pDRIInfo )
      return FALSE;

   pMga->pDRIInfo->driverSwapMethod = DRI_HIDE_X_CONTEXT;

   /* NOTE: DRIFinishScreenInit must be called before *DRIKernelInit
    * because *DRIKernelInit requires that the hardware lock is held by
    * the X server, and the first time the hardware lock is grabbed is
    * in DRIFinishScreenInit.
    */
   if ( !DRIFinishScreenInit( pScreen ) ) {
      MGADRICloseScreen( pScreen );
      return FALSE;
   }

   if ( !MGADRIKernelInit( pScreen ) ) {
      MGADRICloseScreen( pScreen );
      return FALSE;
   }

   if ( !MGADRIBuffersInit( pScreen ) ) {
      MGADRICloseScreen( pScreen );
      return FALSE;
   }

   MGADRIIrqInit(pMga, pScreen);

   switch(pMga->Chipset) {
   case PCI_CHIP_MGAG550:
   case PCI_CHIP_MGAG400:
      pMGADRI->chipset = MGA_CARD_TYPE_G400;
      break;
   case PCI_CHIP_MGAG200:
   case PCI_CHIP_MGAG200_PCI:
      pMGADRI->chipset = MGA_CARD_TYPE_G200;
      break;
   default:
      return FALSE;
   }
   pMGADRI->width		= pScrn->virtualX;
   pMGADRI->height		= pScrn->virtualY;
   pMGADRI->mem			= pScrn->videoRam * 1024;
   pMGADRI->cpp			= pScrn->bitsPerPixel / 8;

   pMGADRI->agpMode		= pMga->agpMode;

   pMGADRI->frontOffset		= pMGADRIServer->frontOffset;
   pMGADRI->frontPitch		= pMGADRIServer->frontPitch;
   pMGADRI->backOffset		= pMGADRIServer->backOffset;
   pMGADRI->backPitch		= pMGADRIServer->backPitch;
   pMGADRI->depthOffset		= pMGADRIServer->depthOffset;
   pMGADRI->depthPitch		= pMGADRIServer->depthPitch;
   pMGADRI->textureOffset	= pMGADRIServer->textureOffset;
   pMGADRI->textureSize		= pMGADRIServer->textureSize;

   i = mylog2( pMGADRI->textureSize / MGA_NR_TEX_REGIONS );
   if ( i < MGA_LOG_MIN_TEX_REGION_SIZE )
      i = MGA_LOG_MIN_TEX_REGION_SIZE;

   pMGADRI->logTextureGranularity = i;
   pMGADRI->textureSize = (pMGADRI->textureSize >> i) << i; /* truncate */

   i = mylog2( pMGADRIServer->agpTextures.size / MGA_NR_TEX_REGIONS );
   if ( i < MGA_LOG_MIN_TEX_REGION_SIZE )
      i = MGA_LOG_MIN_TEX_REGION_SIZE;

   pMGADRI->logAgpTextureGranularity = i;
   pMGADRI->agpTextureOffset = (unsigned int)pMGADRIServer->agpTextures.handle;
   pMGADRI->agpTextureSize = (unsigned int)pMGADRIServer->agpTextures.size;

   pMGADRI->registers.handle	= pMGADRIServer->registers.handle;
   pMGADRI->registers.size	= pMGADRIServer->registers.size;
   pMGADRI->status.handle	= pMGADRIServer->status.handle;
   pMGADRI->status.size		= pMGADRIServer->status.size;
   pMGADRI->primary.handle	= pMGADRIServer->primary.handle;
   pMGADRI->primary.size	= pMGADRIServer->primary.size;
   pMGADRI->buffers.handle	= pMGADRIServer->buffers.handle;
   pMGADRI->buffers.size	= pMGADRIServer->buffers.size;
   pMGADRI->sarea_priv_offset = sizeof(XF86DRISAREARec);
   return TRUE;
}


void MGADRICloseScreen( ScreenPtr pScreen )
{
   ScrnInfoPtr pScrn = xf86Screens[pScreen->myNum];
   MGAPtr pMga = MGAPTR(pScrn);
   MGADRIServerPrivatePtr pMGADRIServer = pMga->DRIServerInfo;
   drmMGAInit init;

   if ( pMGADRIServer->drmBuffers ) {
      drmUnmapBufs( pMGADRIServer->drmBuffers );
      pMGADRIServer->drmBuffers = NULL;
   }

   if (pMga->irq) {
      drmCtlUninstHandler(pMga->drmFD);
      pMga->irq = 0;
   }

   /* Cleanup DMA */
   memset( &init, 0, sizeof(drmMGAInit) );
   init.func = MGA_CLEANUP_DMA;
   drmCommandWrite( pMga->drmFD, DRM_MGA_INIT, &init, sizeof(drmMGAInit) );

   if ( pMGADRIServer->status.map ) {
      drmUnmap( pMGADRIServer->status.map, pMGADRIServer->status.size );
      pMGADRIServer->status.map = NULL;
   }
   if ( pMGADRIServer->buffers.map ) {
      drmUnmap( pMGADRIServer->buffers.map, pMGADRIServer->buffers.size );
      pMGADRIServer->buffers.map = NULL;
   }
   if ( pMGADRIServer->primary.map ) {
      drmUnmap( pMGADRIServer->primary.map, pMGADRIServer->primary.size );
      pMGADRIServer->primary.map = NULL;
   }
   if ( pMGADRIServer->warp.map ) {
      drmUnmap( pMGADRIServer->warp.map, pMGADRIServer->warp.size );
      pMGADRIServer->warp.map = NULL;
   }

   if ( pMGADRIServer->agpTextures.map ) {
      drmUnmap( pMGADRIServer->agpTextures.map, pMGADRIServer->agpTextures.size );
      pMGADRIServer->agpTextures.map = NULL;
   }

   if ( pMGADRIServer->agp.handle ) {
      drmAgpUnbind( pMga->drmFD, pMGADRIServer->agp.handle );
      drmAgpFree( pMga->drmFD, pMGADRIServer->agp.handle );
      pMGADRIServer->agp.handle = 0;
      drmAgpRelease( pMga->drmFD );
   }

   DRICloseScreen( pScreen );

   if ( pMga->pDRIInfo ) {
      if ( pMga->pDRIInfo->devPrivate ) {
	 xfree( pMga->pDRIInfo->devPrivate );
	 pMga->pDRIInfo->devPrivate = 0;
      }
      DRIDestroyInfoRec( pMga->pDRIInfo );
      pMga->pDRIInfo = 0;
   }
   if ( pMga->DRIServerInfo ) {
      xfree( pMga->DRIServerInfo );
      pMga->DRIServerInfo = 0;
   }
   if ( pMga->pVisualConfigs ) {
      xfree( pMga->pVisualConfigs );
   }
   if ( pMga->pVisualConfigsPriv ) {
      xfree( pMga->pVisualConfigsPriv );
   }
}
