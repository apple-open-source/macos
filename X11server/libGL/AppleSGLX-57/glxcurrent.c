/*
 * SGI FREE SOFTWARE LICENSE B (Version 2.0, Sept. 18, 2008)
 * Copyright (C) 1991-2000 Silicon Graphics, Inc. All Rights Reserved.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice including the dates of first publication and
 * either this permission notice or a reference to
 * http://oss.sgi.com/projects/FreeB/
 * shall be included in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
 * OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL
 * SILICON GRAPHICS, INC. BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY,
 * WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF
 * OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 *
 * Except as contained in this notice, the name of Silicon Graphics, Inc.
 * shall not be used in advertising or otherwise to promote the sale, use or
 * other dealings in this Software without prior written authorization from
 * Silicon Graphics, Inc.
 */

/**
 * \file glxcurrent.c
 * Client-side GLX interface for current context management.
 */

#include <stdlib.h>
#include <pthread.h>
#include "glxclient.h"
#include "apple_glx.h"
#include "apple_glx_context.h"

/*
** We setup some dummy structures here so that the API can be used
** even if no context is current.
*/

static GLubyte dummyBuffer[__GLX_BUFFER_LIMIT_SIZE];

/*
** Dummy context used by small commands when there is no current context.
** All the
** gl and glx entry points are designed to operate as nop's when using
** the dummy context structure.
*/
static __GLXcontext dummyContext = {
    &dummyBuffer[0],
    &dummyBuffer[0],
    &dummyBuffer[0],
    &dummyBuffer[__GLX_BUFFER_LIMIT_SIZE],
    sizeof(dummyBuffer),
};

/*
 * Current context management and locking
 */
static pthread_once_t once_control = PTHREAD_ONCE_INIT;

_X_HIDDEN pthread_mutex_t __glXmutex = PTHREAD_MUTEX_INITIALIZER;

/**
 * Per-thread data key.
 * 
 * Once \c init_thread_data has been called, the per-thread data key will
 * take a value of \c NULL.  As each new thread is created the default
 * value, in that thread, will be \c NULL.
 */
static pthread_key_t ContextTSD;

/**
 * Initialize the per-thread data key.
 * 
 * This function is called \b exactly once per-process (not per-thread!) to
 * initialize the per-thread data key.  This is ideally done using the
 * \c pthread_once mechanism.
 */
static void init_thread_data( void )
{
    if (pthread_key_create(&ContextTSD, NULL) != 0) {
	perror("pthread_key_create");
	exit(EXIT_FAILURE);
    }
}

_X_HIDDEN void __glXSetCurrentContext( __GLXcontext * c ) {
    pthread_once(&once_control, init_thread_data);
    pthread_setspecific(ContextTSD, c);
}

_X_HIDDEN __GLXcontext * __glXGetCurrentContext( void )
{
    void * v;

    pthread_once(& once_control, init_thread_data);

    v = pthread_getspecific( ContextTSD );
    return (v == NULL) ? & dummyContext : (__GLXcontext *) v;
}


_X_HIDDEN void __glXSetCurrentContextNull(void) {
    __glXSetCurrentContext(&dummyContext);
}


/************************************************************************/

PUBLIC GLXContext glXGetCurrentContext(void)
{
    GLXContext cx = __glXGetCurrentContext();
    
    if (cx == &dummyContext) {
	return NULL;
    } else {
	return cx;
    }
}

PUBLIC GLXDrawable glXGetCurrentDrawable(void)
{
    GLXContext gc = __glXGetCurrentContext();
    return gc->currentDrawable;
}

static Bool MakeContextCurrent(Display *dpy, GLXDrawable draw,
			       GLXDrawable read, GLXContext gc,
			       Bool pre13)
{
    const GLXContext oldGC = __glXGetCurrentContext();
    bool error;
    
    error = apple_glx_make_current_context(dpy, 
					   (oldGC && oldGC != &dummyContext) ?
					   oldGC->apple : NULL, 
					   gc ? gc->apple : NULL,
					   draw);

    apple_glx_diagnostic("%s: error %s\n", __func__, error ? "YES" : "NO");

    if(error)
	return GL_FALSE;
    
    __glXLock();
   
    if (gc == oldGC) {
	/* Even though the contexts are the same the drawable might have
	 * changed.  Note that gc cannot be the dummy, and that oldGC
	 * cannot be NULL, therefore if they are the same, gc is not
	 * NULL and not the dummy.
	 */
	if(gc) {
	    gc->currentDrawable = draw;
	    gc->currentReadable = read;
	}
    } else {
	
	if (oldGC != &dummyContext) {
	    /* Old current context is no longer current to anybody */
	    oldGC->currentDpy = 0;
	    oldGC->currentDrawable = None;
	    oldGC->currentReadable = None;
	    oldGC->currentContextTag = 0;
	    
	    /*
	     * At this point we should check if the context has been
	     * through glXDestroyContext, and redestroy it if so.
	     */
	    if(oldGC->do_destroy) {
		__glXUnlock();
		/* glXDestroyContext uses the same global lock. */
		glXDestroyContext(dpy, oldGC);
		__glXLock();
	    }
	}
	
	if (gc) {
	    __glXSetCurrentContext(gc);

	    gc->currentDpy = dpy;
	    gc->currentDrawable = draw;
	    gc->currentReadable = read;
	} else {
	    __glXSetCurrentContextNull();
	}
    }
    __glXUnlock();

    return GL_TRUE;
}


PUBLIC Bool glXMakeCurrent(Display *dpy, GLXDrawable draw, GLXContext gc)
{
    return MakeContextCurrent(dpy, draw, draw, gc, True);
}

PUBLIC GLX_ALIAS(Bool, glXMakeCurrentReadSGI,
	  (Display *dpy, GLXDrawable d, GLXDrawable r, GLXContext ctx),
	  (dpy, d, r, ctx, False), MakeContextCurrent)

PUBLIC GLX_ALIAS(Bool, glXMakeContextCurrent,
	  (Display *dpy, GLXDrawable d, GLXDrawable r, GLXContext ctx),
	  (dpy, d, r, ctx, False), MakeContextCurrent)
