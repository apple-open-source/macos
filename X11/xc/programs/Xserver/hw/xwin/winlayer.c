/*
 *Copyright (C) 1994-2000 The XFree86 Project, Inc. All Rights Reserved.
 *
 *Permission is hereby granted, free of charge, to any person obtaining
 * a copy of this software and associated documentation files (the
 *"Software"), to deal in the Software without restriction, including
 *without limitation the rights to use, copy, modify, merge, publish,
 *distribute, sublicense, and/or sell copies of the Software, and to
 *permit persons to whom the Software is furnished to do so, subject to
 *the following conditions:
 *
 *The above copyright notice and this permission notice shall be
 *included in all copies or substantial portions of the Software.
 *
 *THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
 *EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 *MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
 *NONINFRINGEMENT. IN NO EVENT SHALL THE XFREE86 PROJECT BE LIABLE FOR
 *ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF
 *CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION
 *WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 *
 *Except as contained in this notice, the name of the XFree86 Project
 *shall not be used in advertising or otherwise to promote the sale, use
 *or other dealings in this Software without prior written authorization
 *from the XFree86 Project.
 *
 * Authors:	Harold L Hunt II
 */
/* $XFree86: xc/programs/Xserver/hw/xwin/winlayer.c,v 1.9 2002/10/31 23:04:39 alanh Exp $ */

#include "win.h"

#if WIN_LAYER_SUPPORT
/*
 * Create initial layer.  Cygwin only needs one initial layer.
 */

LayerPtr
winLayerCreate (ScreenPtr pScreen)
{
  winScreenPriv(pScreen);
  winScreenInfo		*pScreenInfo = pScreenPriv->pScreenInfo;
  PixmapPtr		pPixmap = NULL;
  DWORD			dwLayerKind;

  ErrorF ("winLayerCreate - dwDepth: %d dwBPP %d\n",
	  pScreenInfo->dwDepth, pScreenInfo->dwBPP);

  /* We only need a single layer kind: shadow */
  dwLayerKind = LAYER_SHADOW;
  pPixmap = LAYER_SCREEN_PIXMAP;

  return LayerCreate (pScreen,
		      dwLayerKind,
		      pScreenInfo->dwBPP,
		      pPixmap,
		      pScreenPriv->pwinShadowUpdate,
		      NULL, /* No ShadowWindowProc */
		      0, /* Rotate */
		      0);
}


/*
 * Used as a function parameter to WalkTree.
 */

int
winLayerAdd (WindowPtr pWindow, pointer value)
{
  ScreenPtr		pScreen = pWindow->drawable.pScreen;
  LayerPtr		pLayer = (LayerPtr) value;
  
  ErrorF ("winLayerAdd ()\n");

  if (!LayerWindowAdd (pScreen, pLayer, pWindow))
    return WT_STOPWALKING;
  
  return WT_WALKCHILDREN;
}


/*
 * Used as a function parameter to WalkTree.
 */

int
winLayerRemove (WindowPtr pWindow, pointer value)
{
  ScreenPtr		pScreen = pWindow->drawable.pScreen;
  LayerPtr		pLayer = (LayerPtr) value;
  
  ErrorF ("winLayerRemove ()\n");

  LayerWindowRemove (pScreen, pLayer, pWindow);

  return WT_WALKCHILDREN;
}

#ifdef RANDR
/*
 * Answer queries about the RandR features supported.
 */

Bool
winRandRGetInfo (ScreenPtr pScreen, Rotation *pRotations)
{
  winScreenPriv(pScreen);
  winScreenInfo			*pScreenInfo = pScreenPriv->pScreenInfo;
  int				n;
  Rotation			rotateKind;
  RRScreenSizePtr		pSize;

  ErrorF ("winRandRGetInfo ()\n");

  /* Don't support rotations, yet */
  *pRotations = RR_Rotate_0; /* | RR_Rotate_90 | RR_Rotate_180 | ... */
  
#if 0
  /* Check for something naughty.  Don't know what exactly... */
  for (n = 0; n < pScreen->numDepths; n++)
    if (pScreen->allowedDepths[n].numVids)
      break;
  if (n == pScreen->numDepths)
    return FALSE;
#endif
  
  /*
   * Register supported sizes.  This can be called many times, but
   * we only support one size for now.
   */
  pSize = RRRegisterSize (pScreen,
			  pScreenInfo->dwWidth,
			  pScreenInfo->dwHeight,
			  pScreenInfo->dwWidth_mm,
			  pScreenInfo->dwHeight_mm);
  
  /* Only one allowed rotation for now */
  rotateKind = RR_Rotate_0;

  /* Tell RandR what the current config is */
  RRSetCurrentConfig (pScreen, rotateKind, pSize);
  
  return TRUE;
}


/*
 * Configure which RandR features are supported.
 */

Bool
winRandRSetConfig (ScreenPtr		pScreen,
		   Rotation		rotateKind,
		   RRScreenSizePtr	pSize)
{
  ErrorF ("winRandRSetConfig ()\n");

  /*
   * Apparently the only thing that can change is rotation.
   * We don't support rotation, so that means nothing can change, right?
   */
  
  assert (rotateKind == RR_Rotate_0);

  return TRUE;
}


/*
 * Initialize the RandR layer.
 */

Bool
winRandRInit (ScreenPtr pScreen)
{
  rrScrPrivPtr		pRRScrPriv;

  ErrorF ("winRandRInit ()\n");

  if (!RRScreenInit (pScreen))
    {
      ErrorF ("winRandRInit () - RRScreenInit () failed\n");
      return FALSE;
    }

  /* Set some RandR function pointers */
  pRRScrPriv = rrGetScrPriv (pScreen);
  pRRScrPriv->rrGetInfo = winRandRGetInfo;
  pRRScrPriv->rrSetConfig = winRandRSetConfig;

  return TRUE;
}
#endif
#endif
