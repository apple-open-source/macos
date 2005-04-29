/*
 *	imgUtil.tcl
 */

#include "imgInt.h"
#include <string.h>

#ifdef MAC_TCL
#  include "dlfcn.h"
#else
#  ifdef HAVE_DLFCN_H
#    include <dlfcn.h>
#  else
#    include "compat/dlfcn.h"
#  endif
#endif

/*
 * In some systems, like SunOS 4.1.3, the RTLD_NOW flag isn't defined
 * and this argument to dlopen must always be 1.
 */

#ifndef RTLD_NOW
#   define RTLD_NOW 1
#endif


/*
 *----------------------------------------------------------------------
 *
 * ImgPhotoPutBlock --
 *
 *	This procedure is called to put image data into a photo image.
 *	The difference with Tk_PhotoPutBlock is that it handles the
 *	transparency information as well.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	The image data is stored.  The image may be expanded.
 *	The Tk image code is informed that the image has changed.
 *
 *----------------------------------------------------------------------
 */

int
ImgPhotoPutBlock(handle, blockPtr, x, y, width, height)
    Tk_PhotoHandle handle;	/* Opaque handle for the photo image
				 * to be updated. */
    Tk_PhotoImageBlock *blockPtr;
				/* Pointer to a structure describing the
				 * pixel data to be copied into the image. */
    int x, y;			/* Coordinates of the top-left pixel to
				 * be updated in the image. */
    int width, height;		/* Dimensions of the area of the image
				 * to be updated. */
{
    int alphaOffset;

    alphaOffset = blockPtr->offset[3];
    if ((alphaOffset< 0) || (alphaOffset>= blockPtr->pixelSize)) {
	alphaOffset = blockPtr->offset[0];
	if (alphaOffset < blockPtr->offset[1]) {
	    alphaOffset = blockPtr->offset[1];
	}
	if (alphaOffset < blockPtr->offset[2]) {
	    alphaOffset = blockPtr->offset[2];
	}
	if (++alphaOffset >= blockPtr->pixelSize) {
	    alphaOffset = blockPtr->offset[0];
	}
    } else {
	if ((alphaOffset == blockPtr->offset[1]) ||
		(alphaOffset == blockPtr->offset[2])) {
	    alphaOffset = blockPtr->offset[0];
	}
    }
    if (alphaOffset != blockPtr->offset[0]) {
	int X, Y, end;
	unsigned char *pixelPtr, *imagePtr, *rowPtr;
	rowPtr = imagePtr = blockPtr->pixelPtr;
	for (Y = 0; Y < height; Y++) {
	    X = 0;
	    pixelPtr = rowPtr + alphaOffset;
	    while(X < width) {
		/* search for first non-transparent pixel */
		while ((X < width) && !(*pixelPtr)) {
		    X++; pixelPtr += blockPtr->pixelSize;
		}
		end = X;
		/* search for first transparent pixel */
		while ((end < width) && *pixelPtr) {
		    end++; pixelPtr += blockPtr->pixelSize;
		}
		if (end > X) {
 		    blockPtr->pixelPtr =  rowPtr + blockPtr->pixelSize * X;
#if (TK_MAJOR_VERSION > 8) || ((TK_MAJOR_VERSION == 8) && (TK_MINOR_VERSION > 3))
		    Tk_PhotoPutBlock(handle, blockPtr, x+X, y+Y, end-X, 1, TK_PHOTO_COMPOSITE_OVERLAY);
#else
		    Tk_PhotoPutBlock(handle, blockPtr, x+X, y+Y, end-X, 1);
#endif
		}
		X = end;
	    }
	    rowPtr += blockPtr->pitch;
	}
	blockPtr->pixelPtr = imagePtr;
    } else {
#if (TK_MAJOR_VERSION > 8) || ((TK_MAJOR_VERSION == 8) && (TK_MINOR_VERSION > 3))
	Tk_PhotoPutBlock(handle,blockPtr,x,y,width,height, TK_PHOTO_COMPOSITE_OVERLAY);
#else
	Tk_PhotoPutBlock(handle,blockPtr,x,y,width,height);
#endif
    }
    return TCL_OK;
}


/*
 *----------------------------------------------------------------------
 *
 * ImgLoadLib --
 *
 *	This procedure is called to load a shared library into memory.
 *	If the extension is ".so" (e.g. Solaris, Linux) or ".sl" (HP-UX)
 *	it is possible that the extension is appended or replaced with
 *	a major version number. If the file cannot be found, the version
 *	numbers will be stripped off one by one. e.g.
 *
 *	HP-UX:	libtiff.3.4	Linux,Solaris:	libtiff.so.3.4
 *		libtiff.3			libtiff.so.3
 *		libtiff.sl			libtiff.so
 *
 * Results:
 *	TCL_OK if function succeeds. Otherwise TCL_ERROR while the
 *	interpreter will contain an error-message. The last parameter
 *	"num" contains the minimum number of symbols that is required
 *	by the application to succeed. Only the first <num> symbols
 *	will produce an error if they cannot be found.
 *
 * Side effects:
 *	At least <num> Library functions become available by the
 *	application.
 *
 *----------------------------------------------------------------------
 */

typedef struct Functions {
    VOID *handle;
    int (* first) _ANSI_ARGS_((void));
    int (* next) _ANSI_ARGS_((void));
} Functions;

#define IMG_FAILED ((VOID *) -114)

int
ImgLoadLib(interp, libName, handlePtr, symbols, num)
    Tcl_Interp *interp;
    CONST char *libName;
    VOID **handlePtr;
    char **symbols;
    int num;
{
    VOID *handle = (VOID *) NULL;
    Functions *lib = (Functions *) handlePtr;
    char **p = (char **) &(lib->first);
    char **q = symbols;
    char buf[256];
    char *r;
    int length;

    if (lib->handle != NULL) {
	return (lib->handle != IMG_FAILED) ? TCL_OK : TCL_ERROR;
    }

    length = strlen(libName);
    strcpy(buf,libName);
    handle = dlopen(buf, RTLD_NOW);

    while (handle == NULL) {
	if ((r = strrchr(buf,'.')) != NULL) {
	    if ((r[1] < '0') || (r[1] > '9')) {
		if (interp) {
		    Tcl_AppendResult(interp,"cannot open ",libName,
			    ": ", dlerror(), (char *) NULL);
		} else {
		    printf("cannot open %s: %s\n",libName,dlerror());
		}
		lib->handle = IMG_FAILED;
		return TCL_ERROR;
	    }
	    length = r - buf;
	    *r = 0;
	}
	if (strchr(buf,'.') == NULL) {
	    strcpy(buf+length,".sl");
	    length += 3;
	}
	dlerror();
	handle = dlopen(buf, RTLD_NOW);
    }

    buf[0] = '_';
    while (*q) {
	*p = (char *) dlsym(handle,*q);
	if (*p == (char *)NULL) {
	    strcpy(buf+1,*q);
	    *p = (char *) dlsym(handle,buf);
	    if ((num > 0) && (*p == (char *)NULL)) {
		if (interp) {
		    Tcl_AppendResult(interp,"cannot open ",libName,
			    ": symbol \"",*q,"\" not found", (char *) NULL);
		} else {
		    printf("cannot open %s: symbol \"%s\" not found",
			    libName, *q);
		}
		dlclose(handle);
		lib->handle = IMG_FAILED;
		return TCL_ERROR;
	    }
	}
	q++; num--;
	p += (Tk_Offset(Functions, next) - Tk_Offset(Functions, first)) /
		sizeof(char *);
    }
    lib->handle = handle;

    return TCL_OK;
}

/*
 *----------------------------------------------------------------------
 *
 * ImgLoadFailed --
 *
 *	Mark the loaded library as invalid. Remove it from memory
 *	if possible. It will no longer be used in the future.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	Next time the same handle is used by ImgLoadLib, it will
 *	fail immediately, without trying to load it.
 *
 *----------------------------------------------------------------------
 */

void
ImgLoadFailed(handlePtr)
    VOID **handlePtr;
{
    if ((*handlePtr != NULL) && (*handlePtr != IMG_FAILED)) {
	/* Oops, still loaded. First remove it from menory */
	dlclose(*handlePtr);
    }
    *handlePtr = IMG_FAILED;
}
