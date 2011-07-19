/*****************************************************************************
 * XvMC Wrapper including the Nonstandard VLD extension.
 *
 * Copyright (c) 2004 The Unichrome project. All rights reserved.
 *
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHOR(S) OR COPYRIGHT HOLDER(S) BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
 * DEALINGS IN THE SOFTWARE.
 *
 * Author: Thomas Hellström (2004)
 */

/*
 * BUGS: The wrapper really should maintain one symbol table per port. This 
 * could possibly be impemented, To do that, the port-independent symbols need to be lifted out,
 * and one would have to create a number of mapping tables:
 *
 *                 port  -> symbol table
 *                 context -> port
 *                 surface -> port
 *                 subpicture -> port
 *
 * and reference the right table when needed. 
 * This needs to be done only if there is a player that wants to access two displays with different
 * hardware simultaneously. Not likely as of today.
 */

#ifdef HAVE_CONFIG_H
  #include "config.h"
#endif

#include <X11/Xlib.h>
#include <X11/extensions/XvMC.h>
#include <X11/extensions/XvMClib.h>
#include <X11/extensions/vldXvMC.h>
#include <dlfcn.h>
#include <stdio.h>
#include <string.h>
#include <ctype.h>
#include <stdlib.h>


typedef Bool (*XvMCQueryExtensionP) (Display *, int *, int *);
typedef Status (*XvMCQueryVersionP) (Display *, int *,int *);
typedef XvMCSurfaceInfo * (*XvMCListSurfaceTypesP)(Display *, XvPortID, int *);
typedef Status (* XvMCCreateContextP) (Display *,XvPortID,int ,int ,int ,int ,XvMCContext * );
typedef Status (*XvMCDestroyContextP) (Display *, XvMCContext * );
typedef Status (*XvMCCreateSurfaceP)(Display *,XvMCContext *,XvMCSurface *);
typedef Status (*XvMCDestroySurfaceP)(Display *, XvMCSurface *);
typedef XvImageFormatValues * (*XvMCListSubpictureTypesP) (Display *,XvPortID ,int ,int *);
typedef Status (*XvMCPutSurfaceP)(Display *,XvMCSurface *,Drawable ,short , short , unsigned short , 
				  unsigned short ,short ,short ,unsigned short ,unsigned short ,int );
typedef Status (*XvMCHideSurfaceP)(Display *, XvMCSurface *);
typedef Status (*XvMCCreateSubpictureP) (Display *, XvMCContext *, XvMCSubpicture *, 
					 unsigned short, unsigned short,int);
typedef Status (*XvMCClearSubpictureP) (Display *,XvMCSubpicture *,short,short,unsigned short,unsigned short,
					unsigned int);
typedef Status (*XvMCCompositeSubpictureP) (Display *, XvMCSubpicture *,XvImage *,short,short,
					    unsigned short, unsigned short,short,short);
typedef Status (*XvMCDestroySubpictureP) (Display *, XvMCSubpicture *);
typedef Status (*XvMCSetSubpicturePaletteP) (Display *, XvMCSubpicture *, unsigned char *);
typedef Status (*XvMCBlendSubpictureP) (Display *d,XvMCSurface *,XvMCSubpicture *,short,
					short,unsigned short,unsigned short,short,short,
					unsigned short,unsigned short);
typedef Status (*XvMCBlendSubpicture2P) (Display *,XvMCSurface *,XvMCSurface *,
					 XvMCSubpicture *,short,short,unsigned short,
					 unsigned short,short,short,unsigned short,
					 unsigned short);
typedef Status (*XvMCSyncSurfaceP) (Display *, XvMCSurface *);
typedef Status (*XvMCFlushSurfaceP) (Display *, XvMCSurface *);
typedef Status (*XvMCGetSurfaceStatusP) (Display *, XvMCSurface *, int *);
typedef Status (*XvMCRenderSurfaceP) (Display *,XvMCContext *,unsigned int,XvMCSurface *,
				      XvMCSurface *,XvMCSurface *,unsigned int,unsigned int,
				      unsigned int,XvMCMacroBlockArray *,XvMCBlockArray *);
typedef Status (*XvMCSyncSubpictureP) (Display *, XvMCSubpicture *);
typedef Status (*XvMCFlushSubpictureP) (Display *, XvMCSubpicture *);
typedef Status (*XvMCGetSubpictureStatusP) (Display *, XvMCSubpicture *, int *);
typedef Status (*XvMCCreateBlocksP) (Display *, XvMCContext *,unsigned int,XvMCBlockArray *);
typedef Status (*XvMCDestroyBlocksP) (Display *,XvMCBlockArray *);
typedef Status (*XvMCCreateMacroBlocksP) (Display *,XvMCContext *,unsigned int,
					  XvMCMacroBlockArray *);
typedef Status (*XvMCDestroyMacroBlocksP) (Display *,XvMCMacroBlockArray *);
typedef XvAttribute *(*XvMCQueryAttributesP) (Display *,XvMCContext *,int *);
typedef Status (*XvMCSetAttributeP) (Display *,XvMCContext *, Atom, int);
typedef Status (*XvMCGetAttributeP) (Display *,XvMCContext *, Atom, int *);

/*
 * Nonstandard VLD acceleration level:
 */ 
  
typedef Status (*XvMCBeginSurfaceP) (Display *,XvMCContext *,XvMCSurface *,
				     XvMCSurface *,XvMCSurface *f,const XvMCMpegControl *);
typedef Status (*XvMCLoadQMatrixP) (Display *, XvMCContext *,const XvMCQMatrix *);
typedef Status (*XvMCPutSliceP)(Display *,XvMCContext *, char *,int);
typedef Status (*XvMCPutSlice2P)(Display *,XvMCContext *, char *,int, unsigned);
typedef Status (*XvMCGetDRInfoP)(Display *, XvPortID, char **, char **, int *, int *, 
				  int *, int *);


typedef struct {
    XvMCQueryExtensionP   XvMCQueryExtension; 
    XvMCQueryVersionP   XvMCQueryVersion; 
    XvMCListSurfaceTypesP  XvMCListSurfaceTypes;
    XvMCCreateContextP   XvMCCreateContext; 
    XvMCDestroyContextP   XvMCDestroyContext; 
    XvMCCreateSurfaceP  XvMCCreateSurface;
    XvMCDestroySurfaceP  XvMCDestroySurface;
    XvMCListSubpictureTypesP    XvMCListSubpictureTypes; 
    XvMCPutSurfaceP   XvMCPutSurface;
    XvMCHideSurfaceP   XvMCHideSurface;
    XvMCCreateSubpictureP   XvMCCreateSubpicture;
    XvMCClearSubpictureP     XvMCClearSubpicture; 			      
    XvMCCompositeSubpictureP    XvMCCompositeSubpicture; 
    XvMCDestroySubpictureP    XvMCDestroySubpicture; 
    XvMCSetSubpicturePaletteP    XvMCSetSubpicturePalette; 
    XvMCBlendSubpictureP    XvMCBlendSubpicture; 
    XvMCBlendSubpicture2P   XvMCBlendSubpicture2;
    XvMCSyncSurfaceP    XvMCSyncSurface; 
    XvMCFlushSurfaceP    XvMCFlushSurface; 
    XvMCGetSurfaceStatusP    XvMCGetSurfaceStatus; 
    XvMCRenderSurfaceP    XvMCRenderSurface; 
    XvMCSyncSubpictureP    XvMCSyncSubpicture; 
    XvMCFlushSubpictureP    XvMCFlushSubpicture; 
    XvMCGetSubpictureStatusP    XvMCGetSubpictureStatus; 
    XvMCCreateBlocksP    XvMCCreateBlocks; 
    XvMCDestroyBlocksP    XvMCDestroyBlocks; 
    XvMCCreateMacroBlocksP   XvMCCreateMacroBlocks;
    XvMCDestroyMacroBlocksP    XvMCDestroyMacroBlocks; 
    XvMCQueryAttributesP    XvMCQueryAttributes; 
    XvMCSetAttributeP    XvMCSetAttribute; 
    XvMCGetAttributeP    XvMCGetAttribute; 

    /*
     * Nonstandard VLD acceleration level:
     */ 
  
    XvMCBeginSurfaceP    XvMCBeginSurface; 
    XvMCLoadQMatrixP    XvMCLoadQMatrix; 
    XvMCPutSliceP   XvMCPutSlice;
    XvMCPutSlice2P   XvMCPutSlice2;

    /*
     * Driver name function.
     */

    XvMCGetDRInfoP XvMCGetDRInfo;

    int preInitialised;
    int initialised;
    int vldextension;
} XvMCWrapper;

static XvMCWrapper xW;
static int wrapperInit = 0;
static int wrapperPreInit = 0;
static void *xvhandle;
static void *handle2;

#define BUFLEN 200

#define STRS(ARG) STR(ARG)
#define STR(ARG) #ARG

#define XW_RSYM(base,handle,handle2,pointer, retval)			\
    do {								\
	register char *err;						\
	base.pointer = (pointer##P) dlsym((handle),#pointer);		\
	if ((err = dlerror()) != NULL) {				\
	    if (!handle2) {						\
		fprintf(stderr,"%s\n",err); return retval;		\
	    }								\
	    base.pointer = (pointer##P) dlsym((handle2),#pointer);	\
	    if ((err = dlerror()) != NULL) {				\
		fprintf(stderr,"%s\n",err); return retval;		\
	    }								\
	}								\
    } while (0);

#define XW_RSYM2(base,handle,handle2,pointer)			\
    base.pointer = (pointer##P) dlsym((handle),#pointer);	\
    if (dlerror() != NULL) {					\
	base.pointer = (pointer##P) dlsym((handle2),#pointer);	\
	if (dlerror() != NULL) return;				\
    }							


/*
 * Try to dlopen a shared library, versionless first.
 */


static void  *dlopenversion(const char *lib, const char *version, int flag) 
{
  void *ret;
  int curLen,verLen;
  char *curName;
  const char *tail;

  
  curLen = strlen(lib) + (verLen = strlen(version)) + 1;
  curName = (char *) malloc(curLen * sizeof(char));
  strncpy( curName, lib, curLen);
  if (verLen > 1) {
    if (NULL != (tail = strstr(version+1,"."))) {
      strncat( curName, version, tail - version);
    } else {
      strncat( curName, version, verLen);
    }
  }
  ret = dlopen(curName, flag);
  free(curName);
  return ret;
}

static int preInitW(Display *dpy) 
{

    /*
     * Resolve functions that are not hw driver specific.
     */

    void *handleZ = NULL;

    wrapperPreInit = 1;
    xW.preInitialised = 0;
    xW.initialised = 0;
    xvhandle = dlopenversion("libXv.so", XV_SOVERSION, RTLD_LAZY | RTLD_GLOBAL);
    if (!xvhandle) {
	fprintf(stderr,"XvMCWrapper: Warning! Could not open shared "
		"library \"libXv.so" XV_SOVERSION "\"\nThis may cause relocation "
		"errors later.\nError was: \"%s\".\n",dlerror());
    } 
    handle2 = dlopenversion("libXvMC.so", XVMC_SOVERSION, RTLD_LAZY | RTLD_GLOBAL);
    if (!handle2) {
	fprintf(stderr,"XvMCWrapper: Could not load XvMC "
		"library \"libXvMC.so" XVMC_SOVERSION "\". Failing\n");
	fprintf(stderr,"%s\n",dlerror());
	return 1;
    }
    XW_RSYM(xW, handle2, handleZ, XvMCQueryExtension, 1);
    XW_RSYM(xW, handle2, handleZ, XvMCQueryVersion, 1);
    xW.preInitialised = 1;
    return 0;
}

static void initW(Display *dpy, XvPortID port) 
{
    char nameBuffer[BUFLEN];
    void *handle;
    int tmp;
    char *clientName = NULL;
    char *err;
    FILE *configFile; 
    int nameLen = 0;
    int major,minor,patchLevel,isLocal;
    char *busID = NULL;

    wrapperInit = 1;
    xW.initialised = 0;

    if (!wrapperPreInit) 
	if (preInitW( dpy )) return;

    /*
     * Will the DDX tell us the client driver name?
     */ 

    xW.XvMCGetDRInfo = (XvMCGetDRInfoP)
	dlsym(handle2,"XvMCGetDRInfo");

    if ((err = dlerror()) == NULL) {
	if (0 == xW.XvMCGetDRInfo( dpy, port, &clientName, &busID, &major, 
				    &minor,&patchLevel, &isLocal)) {
	    nameLen = strlen(clientName);
	    XFree(busID);
	    if (!isLocal) {
		fprintf(stderr,"XvMCWrapper: X server is not local. Cannot run XvMC.\n");
		XFree(clientName);
		return;
	    }
	} else {
	    clientName = NULL;
	}
    } 

    if (clientName && (nameLen < BUFLEN-7) && (nameLen > 0)) {
	nameLen += 3;
	strncpy(nameBuffer,"lib",BUFLEN-1);
	strncpy(nameBuffer+3, clientName, BUFLEN-4);
	strncpy(nameBuffer + nameLen, ".so", BUFLEN-nameLen-1);
	nameBuffer[BUFLEN-1] = 0;
	XFree(clientName);
	handle = dlopenversion(nameBuffer, XVMC_SOVERSION,RTLD_LAZY);
    } else {
	/*
	 * No. Try to obtain it from the config file.
	 */
      
	if (clientName) XFree(clientName);

	configFile = fopen(STRS(XVMC_CONFIGDIR) "/XvMCConfig","r");
      
	xW.initialised = 0;
	xW.vldextension = 0;

	if (NULL == configFile) {
	    fprintf(stderr,"XvMCWrapper: Could not open config file \"%s\".\n",
		    STRS(XVMC_CONFIGDIR) "/XvMCConfig");
	    perror("XvMCWrapper");
	    return;
	}

	if (NULL == fgets(nameBuffer, BUFLEN, configFile)) {
	    fclose(configFile);
	    fprintf(stderr,"XvMCWrapper: Could not read XvMC library name.\n");
	    perror("XvMCWrapper");
	    return;
	}
	
	fclose(configFile);
	if ((tmp = strlen(nameBuffer)) == 0) {
	    fprintf(stderr,"XvMCWrapper: Zero length XvMC library name.\n");
	    fprintf(stderr,"%s\n",dlerror());
	    return;
	}

	/*
	 * Skip trailing newlines and garbage.
	 */
	
	while (iscntrl(nameBuffer[tmp-1])) {
	    nameBuffer[tmp-1] = 0;
	    if (--tmp == 0) {
		fprintf(stderr,"XvMCWrapper: Zero length XvMC library name.\n");
		return;
	    }
	}
	handle = dlopen(nameBuffer,RTLD_LAZY);
    }
    if (!handle) {
	fprintf(stderr,"XvMCWrapper: Could not load hardware specific XvMC "
		"library \"%s\".\n",nameBuffer);
	fprintf(stderr,"%s\n",dlerror());
	return;
    }

    XW_RSYM(xW, handle, handle2, XvMCListSurfaceTypes,);
    XW_RSYM(xW, handle, handle2, XvMCCreateContext,);
    XW_RSYM(xW, handle, handle2, XvMCDestroyContext,);
    XW_RSYM(xW, handle, handle2, XvMCCreateSurface,);
    XW_RSYM(xW, handle, handle2, XvMCDestroySurface,);
    XW_RSYM(xW, handle, handle2, XvMCListSubpictureTypes,);
    XW_RSYM(xW, handle, handle2, XvMCHideSurface,);
    XW_RSYM(xW, handle, handle2, XvMCCreateSubpicture,);
    XW_RSYM(xW, handle, handle2, XvMCClearSubpicture,);
    XW_RSYM(xW, handle, handle2, XvMCCompositeSubpicture,);
    XW_RSYM(xW, handle, handle2, XvMCDestroySubpicture,);
    XW_RSYM(xW, handle, handle2, XvMCSetSubpicturePalette,);
    XW_RSYM(xW, handle, handle2, XvMCBlendSubpicture,);
    XW_RSYM(xW, handle, handle2, XvMCBlendSubpicture2,);
    XW_RSYM(xW, handle, handle2, XvMCPutSurface,);
    XW_RSYM(xW, handle, handle2, XvMCSyncSurface,);
    XW_RSYM(xW, handle, handle2, XvMCFlushSurface,);
    XW_RSYM(xW, handle, handle2, XvMCGetSurfaceStatus,);
    XW_RSYM(xW, handle, handle2, XvMCRenderSurface,);
    XW_RSYM(xW, handle, handle2, XvMCSyncSubpicture,);
    XW_RSYM(xW, handle, handle2, XvMCFlushSubpicture,);
    XW_RSYM(xW, handle, handle2, XvMCGetSubpictureStatus,);
    XW_RSYM(xW, handle, handle2, XvMCCreateBlocks,);
    XW_RSYM(xW, handle, handle2, XvMCDestroyBlocks,);
    XW_RSYM(xW, handle, handle2, XvMCCreateMacroBlocks,);
    XW_RSYM(xW, handle, handle2, XvMCDestroyMacroBlocks,);
    XW_RSYM(xW, handle, handle2, XvMCQueryAttributes,);
    XW_RSYM(xW, handle, handle2, XvMCSetAttribute,);
    XW_RSYM(xW, handle, handle2, XvMCGetAttribute,);
    xW.initialised = 1;
    XW_RSYM2(xW, handle, handle2, XvMCBeginSurface);
    XW_RSYM(xW, handle, handle2, XvMCLoadQMatrix,);
    XW_RSYM(xW, handle, handle2, XvMCPutSlice,);
    XW_RSYM(xW, handle, handle2, XvMCPutSlice2,);
    xW.vldextension = 1;
}


Bool XvMCQueryExtension (Display *display, int *eventBase, int *errBase)
{
    if (!wrapperPreInit) preInitW( display );
    if (!xW.preInitialised) return 0;
    return (*xW.XvMCQueryExtension)(display, eventBase, errBase);
}

Status XvMCQueryVersion (Display *display, int *major_versionp,
			 int *minor_versionp)
{
    if (!wrapperPreInit) preInitW( display );
    if (!xW.preInitialised) return 0;
    return (*xW.XvMCQueryVersion)(display, major_versionp, minor_versionp);
}


XvMCSurfaceInfo * XvMCListSurfaceTypes(Display *dpy, XvPortID port, int *num) 
{
    if (!wrapperInit) initW( dpy, port);
    if (!xW.initialised) return NULL;
    return (*xW.XvMCListSurfaceTypes)(dpy, port, num);
}

Status XvMCCreateContext (
    Display *display,
    XvPortID port,
    int surface_type_id,
    int width,
    int height,
    int flags,
    XvMCContext * context
    )
{
    if (!wrapperInit) initW(display, port);
    if (!xW.initialised) return BadValue;
    return (*xW.XvMCCreateContext)(display, port, surface_type_id,
				   width, height, flags, context);
}

Status XvMCDestroyContext (Display *display, XvMCContext * context)
{
    if (!xW.initialised) return BadValue;
    return (*xW.XvMCDestroyContext)(display, context);
}

Status 
XvMCCreateSurface(
    Display *display,
    XvMCContext * context,
    XvMCSurface * surface
    )
{
    if (!xW.initialised) return BadValue;
    return (*xW.XvMCCreateSurface)(display, context, surface);
}

Status XvMCDestroySurface(Display *display, XvMCSurface *surface)
{
    if (!xW.initialised) return BadValue;
    return (*xW.XvMCDestroySurface)(display, surface);
}


XvImageFormatValues * XvMCListSubpictureTypes (
    Display * display,
    XvPortID port,
    int surface_type_id,
    int *count_return
    )
{
    if (!xW.initialised) return NULL;
    return (*xW.XvMCListSubpictureTypes)(display, port, surface_type_id,
					 count_return);
}


Status
XvMCPutSurface(
    Display *display,
    XvMCSurface *surface,
    Drawable draw,
    short srcx, 
    short srcy, 
    unsigned short srcw, 
    unsigned short srch,
    short destx,
    short desty,
    unsigned short destw,
    unsigned short desth,
    int flags
    )
{
    if (!xW.initialised) return BadValue;
    return (*xW.XvMCPutSurface)(display, surface, draw, srcx, srcy, srcw, srch,
				destx, desty, destw, desth, flags);
}

Status XvMCHideSurface(Display *display, XvMCSurface *surface)
{
    if (!xW.initialised) return BadValue;
    return (*xW.XvMCHideSurface)(display, surface);
}


Status
XvMCCreateSubpicture (
    Display *display, 
    XvMCContext *context,
    XvMCSubpicture *subpicture, 
    unsigned short width,
    unsigned short height,
    int xvimage_id
    )
{
    if (!xW.initialised) return BadValue;
    return (*xW.XvMCCreateSubpicture)(display, context, subpicture, width, height,
				      xvimage_id);
}


Status
XvMCClearSubpicture (
    Display *display,
    XvMCSubpicture *subpicture,
    short x,
    short y,
    unsigned short width,
    unsigned short height,
    unsigned int color
    )
{
    if (!xW.initialised) return BadValue;
    return (*xW.XvMCClearSubpicture)(display, subpicture, x, y, width, height, color);
}


Status
XvMCCompositeSubpicture (
    Display *display,
    XvMCSubpicture *subpicture,
    XvImage *image,
    short srcx,
    short srcy,
    unsigned short width,
    unsigned short height,
    short dstx,
    short dsty
    )
{
    if (!xW.initialised) return BadValue;
    return (*xW.XvMCCompositeSubpicture)(display, subpicture, image, srcx, srcy,
					 width, height, dstx, dsty);
}

Status
XvMCDestroySubpicture (Display *display, XvMCSubpicture *subpicture)
{
    if (!xW.initialised) return BadValue;
    return (*xW.XvMCDestroySubpicture)(display, subpicture);
}

Status
XvMCSetSubpicturePalette (
    Display *display, 
    XvMCSubpicture *subpicture, 
    unsigned char *palette
    )
{
    if (!xW.initialised) return BadValue;
    return (*xW.XvMCSetSubpicturePalette)(display, subpicture, palette);
}


Status
XvMCBlendSubpicture (
    Display *display,
    XvMCSurface *target_surface,
    XvMCSubpicture *subpicture,
    short subx,
    short suby,
    unsigned short subw,
    unsigned short subh,
    short surfx,
    short surfy,
    unsigned short surfw,
    unsigned short surfh
    )
{
    if (!xW.initialised) return BadValue;
    return (*xW.XvMCBlendSubpicture)(display, target_surface, subpicture,
				     subx, suby, subw, subh, surfx, surfy,
				     surfw, surfh);
}

Status
XvMCBlendSubpicture2 (
    Display *display,
    XvMCSurface *source_surface,
    XvMCSurface *target_surface,
    XvMCSubpicture *subpicture,
    short subx,
    short suby,
    unsigned short subw,
    unsigned short subh,
    short surfx,
    short surfy,
    unsigned short surfw,
    unsigned short surfh
    )
{
    if (!xW.initialised) return BadValue;
    return (*xW.XvMCBlendSubpicture2)(display, source_surface, target_surface, subpicture,
				      subx, suby, subw, subh, surfx, surfy, surfw, surfh);
}


Status XvMCSyncSurface (Display *display, XvMCSurface *surface)
{
    if (!xW.initialised) return BadValue;
    return (*xW.XvMCSyncSurface)(display, surface);
}

Status XvMCFlushSurface (Display *display, XvMCSurface *surface)
{
    if (!xW.initialised) return BadValue;
    return (*xW.XvMCFlushSurface)(display, surface);
}

Status XvMCGetSurfaceStatus (Display *display, XvMCSurface *surface, int *stat)
{
    if (!xW.initialised) return BadValue;
    return (*xW.XvMCGetSurfaceStatus)(display, surface, stat);
}

Status XvMCRenderSurface ( 
    Display *display,
    XvMCContext *context,
    unsigned int picture_structure,
    XvMCSurface *target_surface,
    XvMCSurface *past_surface,
    XvMCSurface *future_surface,
    unsigned int flags,
    unsigned int num_macroblocks,
    unsigned int first_macroblock,
    XvMCMacroBlockArray *macroblock_array,
    XvMCBlockArray *blocks
    )
{
    if (!xW.initialised) return BadValue;
    return (*xW.XvMCRenderSurface)(display, context, picture_structure, target_surface,
				   past_surface, future_surface, flags, num_macroblocks,
				   first_macroblock, macroblock_array, blocks);
}

Status XvMCSyncSubpicture (Display *display, XvMCSubpicture *subpicture)
{
    if (!xW.initialised) return BadValue;
    return (*xW.XvMCSyncSubpicture)(display, subpicture);
}

Status XvMCFlushSubpicture (Display *display, XvMCSubpicture *subpicture)
{
    if (!xW.initialised) return BadValue;
    return (*xW.XvMCFlushSubpicture)(display, subpicture);
}
Status
XvMCGetSubpictureStatus (Display *display, XvMCSubpicture *subpic, int *stat)
{
    if (!xW.initialised) return BadValue;
    return (*xW.XvMCGetSubpictureStatus)(display, subpic, stat);
}

Status XvMCCreateBlocks (
    Display *display, 
    XvMCContext *context,
    unsigned int num_blocks,
    XvMCBlockArray *block
    )
{
    if (!xW.initialised) return BadValue;
    return (*xW.XvMCCreateBlocks)(display, context, num_blocks, block);
}


Status XvMCDestroyBlocks (Display *display,XvMCBlockArray *block)
{
    if (!xW.initialised) return BadValue;
    return (*xW.XvMCDestroyBlocks)(display, block);
}

Status XvMCCreateMacroBlocks (
    Display *display,
    XvMCContext *context,
    unsigned int num_blocks,
    XvMCMacroBlockArray *blocks
    )
{
    if (!xW.initialised) return BadValue;
    return (*xW.XvMCCreateMacroBlocks)(display, context, num_blocks, blocks);
}


Status XvMCDestroyMacroBlocks (
    Display *display,
    XvMCMacroBlockArray *block
    )
{
    if (!xW.initialised) return BadValue;
    return (*xW.XvMCDestroyMacroBlocks)(display, block);
}


XvAttribute *
XvMCQueryAttributes (
    Display *display,
    XvMCContext *context,
    int *number
    )
{
    if (!xW.initialised) return NULL;
    return (*xW.XvMCQueryAttributes)(display, context, number);
}
  

Status
XvMCSetAttribute (
    Display *display,
    XvMCContext *context, 
    Atom attribute, 
    int value
    )
{
    if (!xW.initialised) return BadValue;
    return (*xW.XvMCSetAttribute)(display, context, attribute, value);
}


Status
XvMCGetAttribute (
    Display *display,
    XvMCContext *context, 
    Atom attribute, 
    int *value
    )
{
    if (!xW.initialised) return BadValue;
    return (*xW.XvMCGetAttribute)(display, context, attribute, value);
}


Status XvMCBeginSurface(Display *display,
			XvMCContext *context,
			XvMCSurface *target_surface,
			XvMCSurface *past_surface,
			XvMCSurface *future_surface,
			const XvMCMpegControl *control)
{
    if (!xW.vldextension) return BadValue;
    return (*xW.XvMCBeginSurface)(display, context, target_surface, past_surface, future_surface, 
				  control);
}

Status XvMCLoadQMatrix(Display *display, XvMCContext *context,
		       const XvMCQMatrix *qmx)
{
    if (!xW.vldextension) return BadValue;
    return (*xW.XvMCLoadQMatrix)(display, context, qmx);
}

Status XvMCPutSlice(Display *display,XvMCContext *context,
		    char *slice, int nBytes)
{
    if (!xW.vldextension) return BadValue;
    return (*xW.XvMCPutSlice)(display, context, slice, nBytes);
}

Status XvMCPutSlice2(Display *display,XvMCContext *context,
		     char *slice, int nBytes, int sliceCode)
{
    if (!xW.vldextension) return BadValue;
    return (*xW.XvMCPutSlice2)(display, context, slice, nBytes, sliceCode);
}
