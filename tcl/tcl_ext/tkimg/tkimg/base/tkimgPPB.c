/*
 * tkimgPPB.tcl
 */

#include <string.h>
#include "tkimg.h"

/*
 * Make sure that all Tk's stub entries are available, no matter what
 * Tcl version we compile against.
 */
#undef Tk_PhotoPutBlock /* 266 */
#define Tk_PhotoPutBlock ((int (*)(Tcl_Interp *, Tk_PhotoHandle, Tk_PhotoImageBlock *, int, int, int, int, int)) *((&tkStubsPtr->tk_MainLoop)+266))
#undef Tk_PhotoPutBlock_Panic /* 246 */
#define Tk_PhotoPutBlock_Panic ((void (*)(Tk_PhotoHandle, Tk_PhotoImageBlock *, int, int, int, int, int)) *((&tkStubsPtr->tk_MainLoop)+246))
#undef Tk_PhotoPutBlock_NoComposite /* 144 */
#define Tk_PhotoPutBlock_NoComposite ((void (*)(Tk_PhotoHandle, Tk_PhotoImageBlock *, int, int, int, int)) *((&tkStubsPtr->tk_MainLoop)+144))
#undef Tk_PhotoExpand /* 265 */
#define Tk_PhotoExpand ((int (*)(Tcl_Interp *, Tk_PhotoHandle, int, int)) *((&tkStubsPtr->tk_MainLoop)+265))
#undef Tk_PhotoExpand_Panic /* 148 */
#define Tk_PhotoExpand_Panic ((void (*)(Tk_PhotoHandle, int, int)) *((&tkStubsPtr->tk_MainLoop)+148))
#undef Tk_PhotoSetSize /* 268 */
#define Tk_PhotoSetSize ((int (*)(Tcl_Interp *, Tk_PhotoHandle, int, int)) *((&tkStubsPtr->tk_MainLoop)+268))
#undef Tk_PhotoSetSize_Panic /* 150 */
#define Tk_PhotoSetSize_Panic ((int (*)(Tk_PhotoHandle, int, int)) *((&tkStubsPtr->tk_MainLoop)+150))

/*
 *----------------------------------------------------------------------
 *
 * tkimg_PhotoPutBlock --
 *
 *  This procedure is called to put image data into a photo image.
 *  The difference with Tk_PhotoPutBlock is that it handles the
 *  transparency information as well.
 *
 * Results:
 *  None.
 *
 * Side effects:
 *  The image data is stored.  The image may be expanded.
 *  The Tk image code is informed that the image has changed.
 *
 *----------------------------------------------------------------------
 */

int tkimg_PhotoPutBlock(
	Tcl_Interp *interp, /* Interpreter for error-reporting. */
	Tk_PhotoHandle handle, /* Opaque handle for the photo image to be updated. */
	Tk_PhotoImageBlock *blockPtr, /* Pointer to a structure describing the
	 * pixel data to be copied into the image. */
	int x, /* Coordinates of the top-left pixel to */
	int y, /* be updated in the image. */
	int width, /* Dimensions of the area of the image */
	int height,/* to be updated. */
	int flags /* TK_PHOTO_COMPOSITE_OVERLAY or TK_PHOTO_COMPOSITE_SET */
) {
	if (tkimg_initialized & IMG_NOPANIC) {
		return Tk_PhotoPutBlock(interp, handle, blockPtr, x, y, width, height, flags);
	}
	if (tkimg_initialized & IMG_COMPOSITE) {
		Tk_PhotoPutBlock_Panic(handle, blockPtr, x, y, width, height, flags);
		return TCL_OK;
	}
	if (0 && (flags == TK_PHOTO_COMPOSITE_OVERLAY)) {
		int alphaOffset = blockPtr->offset[3];
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
				while (X < width) {
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
						blockPtr->pixelPtr = rowPtr + blockPtr->pixelSize * X;
						Tk_PhotoPutBlock_NoComposite(handle, blockPtr, x+X, y+Y, end-X, 1);
					}
					X = end;
				}
				rowPtr += blockPtr->pitch;
			}
			blockPtr->pixelPtr = imagePtr;
			return TCL_OK;
		}
	}
	Tk_PhotoPutBlock_NoComposite(handle, blockPtr, x, y, width, height);
	return TCL_OK;
}

int tkimg_PhotoExpand(
	Tcl_Interp *interp, /* Interpreter for error-reporting. */
	Tk_PhotoHandle handle, /* Opaque handle for the photo image
	 * to be updated. */
	int width, /* Dimensions of the area of the image */
	int height /* to be updated. */
) {
	if (tkimg_initialized & IMG_NOPANIC) {
		return Tk_PhotoExpand(interp, handle, width, height);
	}
	Tk_PhotoExpand_Panic(handle, width, height);
	return TCL_OK;
}

int tkimg_PhotoSetSize(
	Tcl_Interp *interp, /* Interpreter for error-reporting. */
	Tk_PhotoHandle handle, /* Opaque handle for the photo image
	 * to be updated. */
	int width, /* Dimensions of the area of the image */
	int height /* to be updated. */
) {
	if (tkimg_initialized & IMG_NOPANIC) {
		return Tk_PhotoSetSize(interp, handle, width, height);
	}
	Tk_PhotoSetSize_Panic(handle, width, height);
	return TCL_OK;
}
