/*
 * (C) Copyright IBM Corporation 2004
 * Copyright (C) 2009 Apple Inc.
 * All Rights Reserved.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * on the rights to use, copy, modify, merge, publish, distribute, sub
 * license, and/or sell copies of the Software, and to permit persons to whom
 * the Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice (including the next
 * paragraph) shall be included in all copies or substantial portions of the
 * Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NON-INFRINGEMENT.  IN NO EVENT SHALL
 * IBM AND/OR THEIR SUPPLIERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
 * DEALINGS IN THE SOFTWARE.
 */

/**
 * \file glx_pbuffer.c
 * Implementation of pbuffer related functions.
 * 
 * \author Ian Romanick <idr@us.ibm.com>
 */

#include <inttypes.h>
#include <pthread.h>
#include "glxclient.h"
#include <X11/extensions/extutil.h>
#include <X11/extensions/Xext.h>
#include <assert.h>
#include <string.h>
#include "glxextensions.h"
#include "glcontextmodes.h"


#include "apple_glx_drawable.h"
#include "glx_error.h"

/**
 * Create a new pbuffer.
 */
PUBLIC GLXPbuffer
glXCreatePbuffer(Display *dpy, GLXFBConfig config, const int *attrib_list) {
    int i, width, height;
    GLXPbuffer result;
    int errorcode;
    
    width = 0;
    height = 0;
    
    for(i = 0; attrib_list[i]; ++i) {
	switch(attrib_list[i]) {
	case GLX_PBUFFER_WIDTH:
	    width = attrib_list[i + 1];
	    ++i;
	    break;
	    
	case GLX_PBUFFER_HEIGHT:
	    height = attrib_list[i + 1];
	    ++i;
	    break;
	    
	case GLX_LARGEST_PBUFFER:
	    /* This is a hint we should probably handle, but how? */
	    ++i;
	    break;

	case GLX_PRESERVED_CONTENTS:
	    /* The contents are always preserved with AppleSGLX with CGL. */
	    ++i;
	    break;
	    
	default:
	    return None;
	}
    }
    
    if(apple_glx_pbuffer_create(dpy, config, width, height, &errorcode, 
				&result)) {
	/* 
	 * apple_glx_pbuffer_create only sets the errorcode to core X11
	 * errors. 
	 */
	__glXSendError(dpy, errorcode, 0, X_GLXCreatePbuffer, true);
	
	return None;
    }
    
   return result;
}


/**
 * Destroy an existing pbuffer.
 */
PUBLIC void
glXDestroyPbuffer(Display *dpy, GLXPbuffer pbuf)
{
    if(apple_glx_pbuffer_destroy(dpy, pbuf)) {
	__glXSendError(dpy, GLXBadPbuffer, pbuf, X_GLXDestroyPbuffer, false);
    }
}

/**
 * Query an attribute of a drawable.
 */
PUBLIC void
glXQueryDrawable(Display *dpy, GLXDrawable drawable,
		 int attribute, unsigned int *value) {
    Window root;
    int x, y;
    unsigned int width, height, bd, depth;

    if(apple_glx_pixmap_query(drawable, attribute, value))
	return; /*done*/

    if(apple_glx_pbuffer_query(drawable, attribute, value))
	return; /*done*/

    /*
     * The OpenGL spec states that we should report GLXBadDrawable if
     * the drawable is invalid, however doing so would require that we
     * use XSetErrorHandler(), which is known to not be thread safe.
     * If we use a round-trip call to validate the drawable, there could
     * be a race, so instead we just opt in favor of letting the
     * XGetGeometry request fail with a GetGeometry request X error 
     * rather than GLXBadDrawable, in what is hoped to be a rare
     * case of an invalid drawable.  In practice most and possibly all
     * X11 apps using GLX shouldn't notice a difference.
     */
    if(XGetGeometry(dpy, drawable, &root, &x, &y, &width, &height, &bd, &depth)) {
	switch(attribute) {
	case GLX_WIDTH:
	    *value = width;
	    break;

	case GLX_HEIGHT:
	    *value = height;
	    break;
	}
    }
}


/**
 * Select the event mask for a drawable.
 */
PUBLIC void
glXSelectEvent(Display *dpy, GLXDrawable drawable, unsigned long mask)
{
    XWindowAttributes xwattr;
    
    if(apple_glx_pbuffer_set_event_mask(drawable, mask))
	return; /*done*/

    /* 
     * The spec allows a window, but currently there are no valid
     * events for a window, so do nothing.
     */
    if(XGetWindowAttributes(dpy, drawable, &xwattr))
	return; /*done*/

    /* The drawable seems to be invalid.  Report an error. */

    __glXSendError(dpy, GLXBadDrawable, drawable, 
		   X_GLXChangeDrawableAttributes, false);
}


/**
 * Get the selected event mask for a drawable.
 */
PUBLIC void
glXGetSelectedEvent(Display *dpy, GLXDrawable drawable, unsigned long *mask)
{
    XWindowAttributes xwattr;
    
    if(apple_glx_pbuffer_get_event_mask(drawable, mask))
	return; /*done*/

    /* 
     * The spec allows a window, but currently there are no valid
     * events for a window, so do nothing, but set the mask to 0.
     */
    if(XGetWindowAttributes(dpy, drawable, &xwattr)) {
	/* The window is valid, so set the mask to 0.*/
	*mask = 0;
	return; /*done*/
    }

    /* The drawable seems to be invalid.  Report an error. */

    __glXSendError(dpy, GLXBadDrawable, drawable, X_GLXGetDrawableAttributes,
		   true);
}


PUBLIC GLXPixmap
glXCreatePixmap( Display *dpy, GLXFBConfig config, Pixmap pixmap,
		 const int *attrib_list )
{
    const __GLcontextModes *modes = (const __GLcontextModes *)config;

    if(apple_glx_pixmap_create(dpy, modes->screen, pixmap, modes))
	return None;

    return pixmap;
}


PUBLIC GLXWindow
glXCreateWindow( Display *dpy, GLXFBConfig config, Window win,
		 const int *attrib_list )
{
    XWindowAttributes xwattr;
    XVisualInfo *visinfo;

    (void)attrib_list; /*unused according to GLX 1.4*/
    
    XGetWindowAttributes(dpy, win, &xwattr);
    
    visinfo = glXGetVisualFromFBConfig(dpy, config);

    if(NULL == visinfo) {
	__glXSendError(dpy, GLXBadFBConfig, 0, X_GLXCreateWindow, false);
	return None;
    }

    if(visinfo->visualid != XVisualIDFromVisual(xwattr.visual)) {
	__glXSendError(dpy, BadMatch, 0, X_GLXCreateWindow, true);
	return None;
    }

    XFree(visinfo);

    return win;
}


PUBLIC void
glXDestroyPixmap(Display *dpy, GLXPixmap pixmap)
{
    if(apple_glx_pixmap_destroy(dpy, pixmap))
	__glXSendError(dpy, GLXBadPixmap, pixmap, X_GLXDestroyPixmap, false);
}


PUBLIC void
glXDestroyWindow(Display *dpy, GLXWindow win)
{
    /* no-op */
}
