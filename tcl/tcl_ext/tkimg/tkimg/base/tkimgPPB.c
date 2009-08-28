/*
 *  tkimgPPB.tcl
 */

#include "tkimg.h"
#include <string.h>

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

int
tkimg_PhotoPutBlock(interp, handle, blockPtr, x, y, width, height, flags)
Tcl_Interp *interp; /* Interpreter for error-reporting. */
Tk_PhotoHandle handle; /* Opaque handle for the photo image
 * to be updated. */
Tk_PhotoImageBlock *blockPtr;
/* Pointer to a structure describing the
 * pixel data to be copied into the image. */
int x, y; /* Coordinates of the top-left pixel to
* be updated in the image. */
int width, height; /* Dimensions of the area of the image
* to be updated. */
int flags; /* TK_PHOTO_COMPOSITE_OVERLAY or TK_PHOTO_COMPOSITE_SET */
{
#if (TK_MAJOR_VERSION > 8) || ((TK_MAJOR_VERSION == 8) && (TK_MINOR_VERSION > 3))
	if (tkimg_initialized & IMG_COMPOSITE) {
		Tk_PhotoPutBlock(handle, blockPtr, x, y, width, height, flags);
		return TCL_OK;
	}
#else
#   define Tk_PhotoPutBlock_NoComposite Tk_PhotoPutBlock
#endif
	if (flags == TK_PHOTO_COMPOSITE_OVERLAY) {
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
