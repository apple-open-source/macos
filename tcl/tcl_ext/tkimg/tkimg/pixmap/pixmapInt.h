/*
 * pixmapInt.h --
 *
 *	Generic header file for the pixmap image type. This is NOT a public
 *	header file!
 *
 * Copyright (c) 1996, Expert Interface Technologies
 *
 * See the file "license.terms" for information on usage and redistribution
 * of this file, and for a DISCLAIMER OF ALL WARRANTIES.
 *
 */

#ifndef _TKIMG_PIXMAP_INT_H_
#define _TKIMG_PIXMAP_INT_H_

#include "tkimg.h"

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

/*
 * Constants
 */

#define XPM_MONO		1
#define XPM_GRAY_4		2
#define XPM_GRAY		3
#define XPM_COLOR		4
#define XPM_SYMBOLIC		5
#define XPM_UNKNOWN		6

/*
 * The following data structure represents the master for a pixmap
 * image:
 */

typedef struct PixmapMaster {
    Tk_ImageMaster tkMaster;	/* Tk's token for image master.  NULL means
				 * the image is being deleted. */
    Tcl_Interp *interp;		/* Interpreter for application that is
				 * using image. */
    Tcl_Command imageCmd;	/* Token for image command (used to delete
				 * it when the image goes away).  NULL means
				 * the image command has already been
				 * deleted. */
    char *fileString;		/* Value of -file option (malloc'ed).
				 * valid only if the -file option is specified
				 */
    char *dataString;		/* Value of -data option (malloc'ed).
				 * valid only if the -data option is specified
				 */
				/* First in list of all instances associated
				 * with this master. */
    int size[2];		/* width and height */
    int ncolors;		/* number of colors */
    int cpp;			/* characters per pixel */
    char **data;		/* The data that defines this pixmap
				 * image (array of strings). It is
				 * converted into an X Pixmap when this
				 * image is instanciated
				 */
    int isDataAlloced;		/* False iff the data is got from
				 * the -id switch */
    struct PixmapInstance *instancePtr;
} PixmapMaster;

typedef struct ColorStruct {
    char c;			/* This is used if CPP is one */
    char *cstring;		/* This is used if CPP is bigger than one */
    XColor *colorPtr;
} ColorStruct;

/*----------------------------------------------------------------------
 * PixmapInstance --
 *
 *	Represents all of the instances of an image that lie within a
 *	particular window:
 *
 *	%% ToDo
 *	Currently one instance is created for each window that uses
 *	this pixmap.  This is usually OK because pixmaps are usually
 *	not shared or only shared by a small number of windows. To
 *	improve resource allocation, we can create an instance for
 *	each (Display x Visual x Depth) combo. This will usually
 *	reduce the number of instances to one.
 *----------------------------------------------------------------------
 */
typedef struct PixmapInstance {
    int refCount;		/* Number of instances that share this
				 * data structure. */
    PixmapMaster *masterPtr;	/* Pointer to master for image. */
    Tk_Window tkwin;		/* Window in which the instances will be
				 * displayed. */
    Pixmap pixmap;		/* The pixmap to display. */
    struct PixmapInstance *nextPtr;
				/* Next in list of all instance structures
				 * associated with masterPtr (NULL means
				 * end of list).
				 */
    ColorStruct *colors;
    ClientData clientData;	/* Place holder for platform specific
				 * instance data */
} PixmapInstance;

/*
 * Functions exported by the platform specific parts for use by the
 * generic part of the implementation
 */

MODULE_SCOPE void TkimgInitPixmapInstance(
	PixmapMaster *masterPtr,
	PixmapInstance *instancePtr);
MODULE_SCOPE void TkimgXpmAllocTmpBuffer(
	PixmapMaster *masterPtr,
	PixmapInstance *instancePtr,
	XImage **imagePtr, XImage **maskPtr);
MODULE_SCOPE void TkimgXpmFreeTmpBuffer(
	PixmapMaster *masterPtr,
	PixmapInstance *instancePtr,
	XImage *image, XImage *mask);
MODULE_SCOPE void TkimgXpmSetPixel(
	PixmapInstance *instancePtr, XImage *image,
	XImage *mask, int x, int y, XColor *colorPtr,
	int *isTranspPtr);
MODULE_SCOPE void TkimgXpmRealizePixmap(
	PixmapMaster *masterPtr,
	PixmapInstance *instancePtr,
	XImage *image, XImage *mask, int isTransp);
MODULE_SCOPE void TkimgXpmFreeInstanceData(
	PixmapInstance *instancePtr, int delete);
MODULE_SCOPE void TkimgpXpmDisplay(ClientData clientData,
	Display *display, Drawable drawable,
	int imageX, int imageY, int width, int height,
	int drawableX, int drawableY);

/*
 * Declarations of internal functions, which are exported for tcl package management.
 */

extern DLLEXPORT int Tkimgpixmap_Init(Tcl_Interp *interp);
extern DLLEXPORT int Tkimgpixmap_SafeInit(Tcl_Interp *interp);

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif
