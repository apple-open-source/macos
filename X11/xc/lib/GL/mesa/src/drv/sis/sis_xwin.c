/**************************************************************************

Copyright 2000 Silicon Integrated Systems Corp, Inc., HsinChu, Taiwan.
All Rights Reserved.

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
IN NO EVENT SHALL PRECISION INSIGHT AND/OR ITS SUPPLIERS BE LIABLE FOR
ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT,
TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE
SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.

**************************************************************************/
/* $XFree86: xc/lib/GL/mesa/src/drv/sis/sis_xwin.c,v 1.3 2000/09/26 15:56:49 tsi Exp $ */

/*
 * Authors:
 *    Sung-Ching Lin <sclin@sis.com.tw>
 *
 */

#include "sis_ctx.h"
#include "sis_mesa.h"

#ifdef XFree86Server

GLboolean
sis_get_clip_rects (XMesaContext xmesa, BoxPtr *ppExtents, int *pCount)
{
  XMesaDrawable d = xmesa->xm_buffer->frontbuffer;

  if (d->type == DRAWABLE_WINDOW)
  {
    RegionPtr pClipList = &((WindowPtr) d)->clipList;
    RegDataPtr data = pClipList->data;

    if (data)
      {
	*ppExtents =
	  (BoxPtr) ((GLubyte *) (pClipList->data) + sizeof (RegDataRec));
	*pCount = data->numRects;
      }
    else
      {
	*ppExtents = &(pClipList->extents);
	*pCount = 1;
      }
  }
  else{
    /* Pixmap */
    /* 
     * TODO : sis_clear_color_buffer, sis_line_clip, sis_tri_clip don't
     *        consider this situation and result in page fault
     */         
    *ppExtents = NULL;
    *pCount = 0;
    return GL_FALSE;    
  }
  
  return GL_TRUE;
}

void *
sis_get_drawable_pos (XMesaContext xmesa)
{
  __GLSiScontext *hwcx = (__GLSiScontext *) xmesa->private;
  XMesaDrawable d = xmesa->xm_buffer->frontbuffer;

  return GET_FbBase (hwcx) +
    (d->x) * GET_DEPTH (hwcx) + (d->y) * GET_PITCH (hwcx);
}

void
sis_get_drawable_origin (XMesaContext xmesa, GLuint * x, GLuint * y)
{
  XMesaDrawable d = xmesa->xm_buffer->frontbuffer;

  *x = d->x;
  *y = d->y;
}

void
sis_get_drawable_size (XMesaContext xmesa, GLuint * w, GLuint * h)
{
  XMesaDrawable d = xmesa->xm_buffer->frontbuffer;

  *w = d->width;
  *h = d->height;
}

void
sis_get_drawable_box (XMesaContext xmesa, BoxPtr pBox)
{
  XMesaDrawable d = xmesa->xm_buffer->frontbuffer;

  pBox->x1 = d->x;
  pBox->y1 = d->y;
  pBox->x2 = d->x + d->width;
  pBox->y2 = d->y + d->height;
}

#else

GLboolean
sis_get_clip_rects (XMesaContext xmesa, BoxPtr * ppExtents, int *pCount)
{
  __DRIdrawablePrivate *dPriv = xmesa->driContextPriv->driDrawablePriv;

/*
  if (!sis_is_window (xmesa))
    {
      return GL_FALSE;
    }
*/

/*
  XMESA_VALIDATE_DRAWABLE_INFO (xmesa->display, 
                                xmesa->driContextPriv->driScreenPriv, 
                                dPriv);
*/

  *ppExtents = (BoxPtr) dPriv->pClipRects;
  *pCount = dPriv->numClipRects;

  return GL_TRUE;
}

void *
sis_get_drawable_pos (XMesaContext xmesa)
{
  __GLSiScontext *hwcx = (__GLSiScontext *) xmesa->private;

  __DRIdrawablePrivate *dPriv = xmesa->driContextPriv->driDrawablePriv;

/*
  XMESA_VALIDATE_DRAWABLE_INFO (xmesa->display, 
                                xmesa->driContextPriv->driScreenPriv, 
                                dPriv);
*/

  return GET_FbBase (hwcx) + (dPriv->x) * GET_DEPTH (hwcx) +
    (dPriv->y) * GET_PITCH (hwcx);
}

void
sis_get_drawable_origin (XMesaContext xmesa, GLuint * x, GLuint * y)
{
  __DRIdrawablePrivate *dPriv = xmesa->driContextPriv->driDrawablePriv;

/*
  XMESA_VALIDATE_DRAWABLE_INFO (xmesa->display, 
                                xmesa->driContextPriv->driScreenPriv, 
                                dPriv);
*/

  *x = dPriv->x;
  *y = dPriv->y;
}

void
sis_get_drawable_size (XMesaContext xmesa, GLuint * w, GLuint * h)
{
  __DRIdrawablePrivate *dPriv = xmesa->driContextPriv->driDrawablePriv;

/*
  XMESA_VALIDATE_DRAWABLE_INFO (xmesa->display, 
                                xmesa->driContextPriv->driScreenPriv, 
                                dPriv);
*/

  *w = dPriv->w;
  *h = dPriv->h;
}

void
sis_get_drawable_box (XMesaContext xmesa, BoxPtr pBox)
{
  __DRIdrawablePrivate *dPriv = xmesa->driContextPriv->driDrawablePriv;

/*
  XMESA_VALIDATE_DRAWABLE_INFO (xmesa->display, 
                                xmesa->driContextPriv->driScreenPriv, 
                                dPriv);
*/

  pBox->x1 = dPriv->x;
  pBox->y1 = dPriv->y;
  pBox->x2 = dPriv->x + dPriv->w;
  pBox->y2 = dPriv->y + dPriv->h;
}

#endif
