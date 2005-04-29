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

#include "tk.h"

#ifndef CONST84
#define CONST84
#endif

/*
 * These macros are used to control whether functions are being declared for
 * import or export in Windows, 
 * They map to no-op declarations on non-Windows systems.
 * Assumes that tcl.h defines DLLEXPORT & DLLIMPORT correctly.
 * The default build on windows is for a DLL, which causes the DLLIMPORT
 * and DLLEXPORT macros to be nonempty. To build a static library, the
 * macro STATIC_BUILD should be defined before the inclusion of tcl.h
 *
 * If a function is being declared while it is being built
 * to be included in a shared library, then it should have the DLLEXPORT
 * storage class.  If is being declared for use by a module that is going to
 * link against the shared library, then it should have the DLLIMPORT storage
 * class.  If the symbol is beind declared for a static build or for use from a
 * stub library, then the storage class should be empty.
 *
 * The convention is that a macro called BUILD_xxxx, where xxxx is the
 * name of a library we are building, is set on the compile line for sources
 * that are to be placed in the library.  When this macro is set, the
 * storage class will be set to DLLEXPORT.  At the end of the header file, the
 * storage class will be reset to DLLIMPORt.
 */

#undef TCL_STORAGE_CLASS
#ifdef BUILD_tkimgpixmap
# define TCL_STORAGE_CLASS DLLEXPORT
#else
# ifdef USE_TKIMGPIXMAP_STUBS
#  define TCL_STORAGE_CLASS
# else
#  define TCL_STORAGE_CLASS DLLIMPORT
# endif
#endif

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
    char ** data;		/* The data that defines this pixmap 
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
    char * cstring;		/* This is used if CPP is bigger than one */
    XColor * colorPtr;
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
    ColorStruct * colors;
    ClientData clientData;	/* Place holder for platform specific
				 * instance data */
} PixmapInstance;

/*
 * Functions exported by the platform specific parts for use by the
 * generic part of the implementation
 */

EXTERN void 	TkimgInitPixmapInstance _ANSI_ARGS_((
			    PixmapMaster *masterPtr,
			    PixmapInstance *instancePtr));
EXTERN void 	TkimgXpmAllocTmpBuffer _ANSI_ARGS_((
			    PixmapMaster * masterPtr,
			    PixmapInstance * instancePtr,
			    XImage ** imagePtr, XImage ** maskPtr));
EXTERN void 	TkimgXpmFreeTmpBuffer _ANSI_ARGS_((
			    PixmapMaster * masterPtr,
			    PixmapInstance * instancePtr,
			    XImage * image, XImage * mask));
EXTERN void 	TkimgXpmSetPixel _ANSI_ARGS_((
			    PixmapInstance * instancePtr, XImage * image,
			    XImage * mask, int x, int y, XColor * colorPtr,
			    int * isTranspPtr));
EXTERN void 	TkimgXpmRealizePixmap _ANSI_ARGS_((
			    PixmapMaster * masterPtr,
			    PixmapInstance * instancePtr,
			    XImage * image, XImage * mask, int isTransp));
EXTERN void 	TkimgXpmFreeInstanceData _ANSI_ARGS_((
			    PixmapInstance *instancePtr, int delete));
EXTERN void 	TkimgpXpmDisplay _ANSI_ARGS_((ClientData clientData,
			    Display *display, Drawable drawable,
			    int imageX, int imageY, int width, int height,
			    int drawableX, int drawableY));

/*
 * Declarations of internal functions, which are exported for tcl package management.
 */

EXTERN int Tkimgpixmap_Init     _ANSI_ARGS_((Tcl_Interp *interp));
EXTERN int Tkimgpixmap_SafeInit _ANSI_ARGS_((Tcl_Interp *interp));


#undef  TCL_STORAGE_CLASS
#define TCL_STORAGE_CLASS DLLIMPORT

#endif
