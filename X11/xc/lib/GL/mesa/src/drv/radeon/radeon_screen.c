/* $XFree86: xc/lib/GL/mesa/src/drv/radeon/radeon_screen.c,v 1.6 2002/12/16 16:18:58 dawes Exp $ */
/**************************************************************************

Copyright 2000, 2001 ATI Technologies Inc., Ontario, Canada, and
                     VA Linux Systems Inc., Fremont, California.

All Rights Reserved.

Permission is hereby granted, free of charge, to any person obtaining a
copy of this software and associated documentation files (the "Software"),
to deal in the Software without restriction, including without limitation
on the rights to use, copy, modify, merge, publish, distribute, sub
license, and/or sell copies of the Software, and to permit persons to whom
the Software is furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice (including the next
paragraph) shall be included in all copies or substantial portions of the
Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NON-INFRINGEMENT. IN NO EVENT SHALL
ATI, VA LINUX SYSTEMS AND/OR THEIR SUPPLIERS BE LIABLE FOR ANY CLAIM,
DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR
OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE
USE OR OTHER DEALINGS IN THE SOFTWARE.

**************************************************************************/

/*
 * Authors:
 *   Kevin E. Martin <martin@valinux.com>
 *   Gareth Hughes <gareth@valinux.com>
 *
 */

#include "radeon_screen.h"
#include "mem.h"


#if 1
/* Including xf86PciInfo.h introduces a bunch of errors...
 */
#define PCI_CHIP_RADEON_QD	0x5144
#define PCI_CHIP_RADEON_QE	0x5145
#define PCI_CHIP_RADEON_QF	0x5146
#define PCI_CHIP_RADEON_QG	0x5147

#define PCI_CHIP_RADEON_QY	0x5159
#define PCI_CHIP_RADEON_QZ	0x515A

#define PCI_CHIP_RADEON_LW	0x4C57 /* mobility 7 - has tcl */

#define PCI_CHIP_RADEON_LY	0x4C59
#define PCI_CHIP_RADEON_LZ	0x4C5A

#define PCI_CHIP_RV200_QW	0x5157 /* a confusing name for a radeon */
#endif


/* Create the device specific screen private data struct.
 */
radeonScreenPtr radeonCreateScreen( __DRIscreenPrivate *sPriv )
{
   radeonScreenPtr radeonScreen;
   RADEONDRIPtr radeonDRIPriv = (RADEONDRIPtr)sPriv->pDevPriv;

   /* Check the DRI version */
   {
      int major, minor, patch;
      if ( XF86DRIQueryVersion( sPriv->display, &major, &minor, &patch ) ) {
         if ( major != 4 || minor < 0 ) {
            __driUtilMessage( "Radeon DRI driver expected DRI version 4.0.x "
			      "but got version %d.%d.%d",
			      major, minor, patch );
            return NULL;
         }
      }
   }

   /* Check that the DDX driver version is compatible */
   if ( sPriv->ddxMajor != 4 ||
	sPriv->ddxMinor < 0 ) {
      __driUtilMessage( "Radeon DRI driver expected DDX driver version 4.0.x "
			"but got version %d.%d.%d", 
			sPriv->ddxMajor, sPriv->ddxMinor, sPriv->ddxPatch );
      return NULL;
   }

   /* Check that the DRM driver version is compatible */
   /* KW:  Check minor number here too -- compatibility mode is broken
    * atm. 
    */
   if ( sPriv->drmMajor != 1 ||
	sPriv->drmMinor < 3) {
      __driUtilMessage( "Radeon DRI driver expected DRM driver version 1.3.x "
			"or newer but got version %d.%d.%d", 
			sPriv->drmMajor, sPriv->drmMinor, sPriv->drmPatch );
      return NULL;
   }


   /* Allocate the private area */
   radeonScreen = (radeonScreenPtr) CALLOC( sizeof(*radeonScreen) );
   if ( !radeonScreen ) {
      __driUtilMessage("%s: CALLOC radeonScreen struct failed",
		       __FUNCTION__);
      return NULL;
   }

   if ( sPriv->drmMinor < 3 ||
        getenv("RADEON_COMPAT")) {
	   fprintf( stderr, "Radeon DRI driver:\n\t"
		    "Compatibility mode for DRM driver version %d.%d.%d\n\t"
		    "TCL will be disabled, expect reduced performance\n\t"
		    "(prefer DRM radeon.o 1.3.x or newer)\n\t", 
		    sPriv->drmMajor, sPriv->drmMinor, sPriv->drmPatch ); 
   }


   /* This is first since which regions we map depends on whether or
    * not we are using a PCI card.
    */
   radeonScreen->IsPCI = radeonDRIPriv->IsPCI;

   if (sPriv->drmMinor >= 3) {
      int ret;
      drmRadeonGetParam gp;

      gp.param = RADEON_PARAM_AGP_BUFFER_OFFSET;
      gp.value = &radeonScreen->agp_buffer_offset;

      ret = drmCommandWriteRead( sPriv->fd, DRM_RADEON_GETPARAM,
				 &gp, sizeof(gp));
      if (ret) {
	 FREE( radeonScreen );
	 fprintf(stderr, "drmRadeonGetParam (RADEON_PARAM_AGP_BUFFER_OFFSET): %d\n", ret);
	 return NULL;
      }

      if (sPriv->drmMinor >= 6) {
	 gp.param = RADEON_PARAM_IRQ_NR;
	 gp.value = &radeonScreen->irq;

	 ret = drmCommandWriteRead( sPriv->fd, DRM_RADEON_GETPARAM,
				    &gp, sizeof(gp));
	 if (ret) {
	    FREE( radeonScreen );
	    fprintf(stderr, "drmRadeonGetParam (RADEON_PARAM_IRQ_NR): %d\n", ret);
	    return NULL;
	 }
      }

   }

   radeonScreen->mmio.handle = radeonDRIPriv->registerHandle;
   radeonScreen->mmio.size   = radeonDRIPriv->registerSize;
   if ( drmMap( sPriv->fd,
		radeonScreen->mmio.handle,
		radeonScreen->mmio.size,
		&radeonScreen->mmio.map ) ) {
      FREE( radeonScreen );
      __driUtilMessage("radeonCreateScreen(): drmMap failed\n");
      return NULL;
   }

   radeonScreen->status.handle = radeonDRIPriv->statusHandle;
   radeonScreen->status.size   = radeonDRIPriv->statusSize;
   if ( drmMap( sPriv->fd,
		radeonScreen->status.handle,
		radeonScreen->status.size,
		&radeonScreen->status.map ) ) {
      drmUnmap( radeonScreen->mmio.map, radeonScreen->mmio.size );
      FREE( radeonScreen );
      __driUtilMessage("radeonCreateScreen(): drmMap (2) failed\n");
      return NULL;
   }
   radeonScreen->scratch = (__volatile__ CARD32 *)
      ((GLubyte *)radeonScreen->status.map + RADEON_SCRATCH_REG_OFFSET);

   radeonScreen->buffers = drmMapBufs( sPriv->fd );
   if ( !radeonScreen->buffers ) {
      drmUnmap( radeonScreen->status.map, radeonScreen->status.size );
      drmUnmap( radeonScreen->mmio.map, radeonScreen->mmio.size );
      FREE( radeonScreen );
      __driUtilMessage("radeonCreateScreen(): drmMapBufs failed\n");
      return NULL;
   }

   if ( !radeonScreen->IsPCI ) {
      radeonScreen->agpTextures.handle = radeonDRIPriv->agpTexHandle;
      radeonScreen->agpTextures.size   = radeonDRIPriv->agpTexMapSize;
      if ( drmMap( sPriv->fd,
		   radeonScreen->agpTextures.handle,
		   radeonScreen->agpTextures.size,
		   (drmAddressPtr)&radeonScreen->agpTextures.map ) ) {
	 drmUnmapBufs( radeonScreen->buffers );
	 drmUnmap( radeonScreen->status.map, radeonScreen->status.size );
	 drmUnmap( radeonScreen->mmio.map, radeonScreen->mmio.size );
	 FREE( radeonScreen );
         __driUtilMessage("radeonCreateScreen(): IsPCI failed\n");
	 return NULL;
      }
   }

   radeonScreen->chipset = 0;
   switch ( radeonDRIPriv->deviceID ) {
   default:
      fprintf(stderr, "unknown chip id, assuming full radeon support\n");
   case PCI_CHIP_RADEON_QD:
   case PCI_CHIP_RADEON_QE:
   case PCI_CHIP_RADEON_QF:
   case PCI_CHIP_RADEON_QG:
   case PCI_CHIP_RV200_QW:
   case PCI_CHIP_RADEON_LW:
      radeonScreen->chipset |= RADEON_CHIPSET_TCL;
   case PCI_CHIP_RADEON_QY:
   case PCI_CHIP_RADEON_QZ:
   case PCI_CHIP_RADEON_LY:
   case PCI_CHIP_RADEON_LZ:
      break;
   }

   radeonScreen->cpp = radeonDRIPriv->bpp / 8;
   radeonScreen->AGPMode = radeonDRIPriv->AGPMode;

   radeonScreen->frontOffset	= radeonDRIPriv->frontOffset;
   radeonScreen->frontPitch	= radeonDRIPriv->frontPitch;
   radeonScreen->backOffset	= radeonDRIPriv->backOffset;
   radeonScreen->backPitch	= radeonDRIPriv->backPitch;
   radeonScreen->depthOffset	= radeonDRIPriv->depthOffset;
   radeonScreen->depthPitch	= radeonDRIPriv->depthPitch;

   radeonScreen->texOffset[RADEON_CARD_HEAP] = radeonDRIPriv->textureOffset;
   radeonScreen->texSize[RADEON_CARD_HEAP] = radeonDRIPriv->textureSize;
   radeonScreen->logTexGranularity[RADEON_CARD_HEAP] =
      radeonDRIPriv->log2TexGran;

   if ( radeonScreen->IsPCI ) {
      radeonScreen->numTexHeaps = RADEON_NR_TEX_HEAPS - 1;
      radeonScreen->texOffset[RADEON_AGP_HEAP] = 0;
      radeonScreen->texSize[RADEON_AGP_HEAP] = 0;
      radeonScreen->logTexGranularity[RADEON_AGP_HEAP] = 0;
   } else {
      radeonScreen->numTexHeaps = RADEON_NR_TEX_HEAPS;
      radeonScreen->texOffset[RADEON_AGP_HEAP] =
	 radeonDRIPriv->agpTexOffset + RADEON_AGP_TEX_OFFSET;
      radeonScreen->texSize[RADEON_AGP_HEAP] = radeonDRIPriv->agpTexMapSize;
      radeonScreen->logTexGranularity[RADEON_AGP_HEAP] =
	 radeonDRIPriv->log2AGPTexGran;
   }

   radeonScreen->driScreen = sPriv;
   radeonScreen->sarea_priv_offset = radeonDRIPriv->sarea_priv_offset;
   return radeonScreen;
}

/* Destroy the device specific screen private data struct.
 */
void radeonDestroyScreen( __DRIscreenPrivate *sPriv )
{
   radeonScreenPtr radeonScreen = (radeonScreenPtr)sPriv->private;

   if (!radeonScreen)
      return;

   if ( !radeonScreen->IsPCI ) {
      drmUnmap( radeonScreen->agpTextures.map,
		radeonScreen->agpTextures.size );
   }
   drmUnmapBufs( radeonScreen->buffers );
   drmUnmap( radeonScreen->status.map, radeonScreen->status.size );
   drmUnmap( radeonScreen->mmio.map, radeonScreen->mmio.size );

   FREE( radeonScreen );
   sPriv->private = NULL;
}
