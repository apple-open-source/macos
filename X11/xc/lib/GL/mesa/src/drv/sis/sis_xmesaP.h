/*
 * Mesa 3-D graphics library
 * Version:  3.3
 * 
 * Copyright (C) 1999-2000  Brian Paul   All Rights Reserved.
 * 
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 * 
 * The above copyright notice and this permission notice shall be included
 * in all copies or substantial portions of the Software.
 * 
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
 * OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL
 * BRIAN PAUL BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN
 * AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
 * CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 */
/* $XFree86: xc/lib/GL/mesa/src/drv/sis/sis_xmesaP.h,v 1.5 2000/09/26 15:56:49 tsi Exp $ */

#ifndef XMESAP_H
#define XMESAP_H


#ifdef XFree86Server
# include "GL/xf86glx.h"
# include "xf86glx_util.h"
#else
# ifdef GLX_DIRECT_RENDERING
#  include "dri_mesa.h"
# endif
#endif

#ifdef XFree86Server
# include "GL/xmesa.h"
#else
# include <X11/Xlib.h>
# include <X11/Xutil.h>
# include "GL/xmesa_x.h"
# include "GL/gl.h"
typedef struct xmesa_context *XMesaContext;
typedef struct xmesa_visual *XMesaVisual;
typedef struct xmesa_buffer *XMesaBuffer;
#endif

#include "types.h"

#if defined(GLX_DIRECT_RENDERING) && !defined(XFree86Server)
#  include "xdriP.h"
#else
#  define DRI_DRAWABLE_ARG
#  define DRI_DRAWABLE_PARM
#  define DRI_CTX_ARG
#endif

struct xmesa_visual {
   GLvisual *gl_visual;		/* Device independent visual parameters */
   XMesaDisplay *display;	/* The X11 display */
   XMesaVisualInfo visinfo;	/* X's visual info */
};

struct xmesa_context {
   GLcontext *gl_ctx;		/* the core library context */
   XMesaVisual xm_visual;	/* Describes the buffers */
   XMesaBuffer xm_buffer;	/* current draw framebuffer */
   XMesaBuffer xm_read_buffer;	/* current read framebuffer */
   GLboolean use_read_buffer;	/* read from the xm_read_buffer/ */

   XMesaDisplay *display;	/* == xm_visual->display */

#if defined(GLX_DIRECT_RENDERING) && !defined(XFree86Server)
  __DRIcontextPrivate *driContextPriv; /* back pointer to DRI context
					* used for locking
					*/
#endif
  void *private;			/* device-specific private context */
};

struct xmesa_buffer {
   GLboolean wasCurrent;	/* was ever the current buffer? */
   GLframebuffer *gl_buffer;	/* depth, stencil, accum, etc buffers */
   XMesaVisual xm_visual;	/* the X/Mesa visual */

   XMesaContext xm_context;     /* the context associated with this buffer */
   XMesaDisplay *display;
   GLboolean pixmap_flag;	/* is the buffer a Pixmap? */
   XMesaDrawable frontbuffer;	/* either a window or pixmap */
   XMesaImage *backimage;	/* back buffer simulated XImage */

   GLvoid *depthbuffer;
   
   GLboolean db_state;		/* GL_FALSE = single buffered */

   GLuint width, height;	/* size of buffer */

   GLint bottom;		/* used for FLIP macro below */

#if defined(GLX_DIRECT_RENDERING) && !defined(XFree86Server)
  __DRIdrawablePrivate *driDrawPriv;	/* back pointer to DRI drawable
					 * used for direct access to framebuffer
					 */
#endif

  void *private;			/* device-specific private drawable */

  struct xmesa_buffer *Next;	/* Linked list pointer: */
};

#endif
