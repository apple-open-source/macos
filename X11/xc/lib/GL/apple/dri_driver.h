/* $XFree86: xc/lib/GL/mesa/dri/dri_mesa.h,v 1.4 2000/06/17 00:02:50 martin Exp $ */
/**************************************************************************

Copyright 1998-1999 Precision Insight, Inc., Cedar Park, Texas.
Copyright (c) 2002 Apple Computer, Inc.
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

/*
 * Authors:
 *   Kevin E. Martin <kevin@precisioninsight.com>
 *   Brian Paul <brian@precisioninsight.com>
 */

#ifndef _DRI_DRIVER_H_
#define _DRI_DRIVER_H_

#include "Xplugin.h"
#include "Xthreads.h"
#include <CoreGraphics/CoreGraphics.h>

#ifdef GLX_DIRECT_RENDERING

typedef struct __DRIdisplayPrivateRec  __DRIdisplayPrivate;
typedef struct __DRIscreenPrivateRec   __DRIscreenPrivate;
typedef struct __DRIvisualPrivateRec   __DRIvisualPrivate;
typedef struct __DRIcontextPrivateRec  __DRIcontextPrivate;
typedef struct __DRIdrawablePrivateRec __DRIdrawablePrivate;

#endif

#define DRI_MESA_VALIDATE_DRAWABLE_INFO(dpy,scrn,pDrawPriv)  \
    do {                                                     \
	if (*(pDrawPriv->pStamp) != pDrawPriv->lastStamp) {  \
	    driMesaUpdateDrawableInfo(dpy,scrn,pDrawPriv);   \
	}                                                    \
    } while (0)

struct __DRIdrawablePrivateRec {
    /*
    ** X's drawable ID associated with this private drawable.
    */
    GLXDrawable draw;

    /*
    ** Reference count for number of context's currently bound to this
    ** drawable.  Once the refcount reaches 0, the drawable can be
    ** destroyed.  This behavior will change with GLX 1.3.
    */
    int refcount;

    xp_surface_id surface_id;
    unsigned int uid;

    /*
    ** Pointer to contexts to which this drawable is currently bound.
    */
    __DRIcontextPrivate *driContextPriv;

    /*
    ** Pointer to screen on which this drawable was created.
    */
    __DRIscreenPrivate *driScreenPriv;

    /*
    ** Set when the drawable on the server is known to have gone away
    */
    unsigned int destroyed :1;
};

struct __DRIcontextPrivateRec {
    /*
    ** Other contexts bound to the same drawable.
    */
    __DRIcontextPrivate *next, *prev; 

    /*
    ** Kernel context handle used to access the device lock.
    */
    XID contextID;

    CGLContextObj ctx;

    /*
    ** Set when attached
    */
    xp_surface_id surface_id;
    xthread_t thread_id;

    /*
    ** This context's display pointer.
    */
    Display *display;

    /*
    ** Pointer to drawable currently bound to this context.
    */
    __DRIdrawablePrivate *driDrawablePriv;

    /*
    ** Pointer to screen on which this context was created.
    */
    __DRIscreenPrivate *driScreenPriv;

    /*
    ** wrapped CGL vectors
    */
    struct {
	void (*viewport) (GLIContext ctx, GLint x, GLint y,
			  GLsizei width, GLsizei height);
	void (*new_list)(GLIContext ctx, GLuint list, GLenum mode);
	void (*end_list) (GLIContext ctx);
    } disp;

    unsigned int pending_update :1;
    unsigned int pending_clear :1;
};

struct __DRIvisualPrivateRec {
    /*
    ** X's visual ID associated with this private visual.
    */
    VisualID vid;

    /*
    ** CGL object representing the visual
    */
    CGLPixelFormatObj pixel_format;
};

struct __DRIscreenPrivateRec {
    /*
    ** Display for this screen
    */
    Display *display;


    /*
    ** Mutex for this screen
    */
    xmutex_t mutex;

    /*
    ** Current screen's number
    */
    int myNum;

    /*
    ** Core rendering library's visuals associated with the current
    ** screen.
    */
    __DRIvisualPrivate *visuals;
    int numVisuals;

    /*
    ** Hash table to hold the drawable information for this screen.
    */
    void *drawHash;
};


extern void driMesaUpdateDrawableInfo(Display *dpy, int scrn,
				      __DRIdrawablePrivate *pdp);


#endif /* _DRI_DRIVER_H_ */
