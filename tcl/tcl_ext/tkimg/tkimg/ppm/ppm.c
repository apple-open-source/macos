/* STARTHEADER
 *
 * File :       ppm.c
 *
 * Author :     Paul Obermeier (paul@poSoft.de)
 *
 * Date :       Mon Jan 22 21:32:48 CET 2001
 *
 * Copyright :  (C) 2001-2002 Paul Obermeier
 *
 * Description :
 *
 * A photo image handler for the PPM RAW image file format.
 *
 * The following image types are supported:
 *
 *  8-bit pixels: Grayscale, also known as PGM.
 * 24-bit pixels: True-color (RGB, each channel 8 bit).
 *
 * List of currently supported features:
 *
 * Type   |     Read      |     Write     |
 *        | -file | -data | -file | -data |
 * ----------------------------------------
 *  8-bit | Yes   | Yes   | Yes   | Yes   |
 * 24-bit | Yes   | Yes   | Yes   | Yes   |
 *
 * PPM Ascii format is not supported yet.
 *
 *
 * No format options are available.
 *
 * Notes:
 *
 * - Part of this code was taken from Tk's tkImgPPM.c:
 *
 *  >> tkImgPPM.c --
 *  >>
 *  >>	A photo image file handler for PPM (Portable PixMap) files.
 *  >>
 *  >> Copyright (c) 1994 The Australian National University.
 *  >> Copyright (c) 1994-1997 Sun Microsystems, Inc.
 *  >>
 *  >> See the file "license.terms" for information on usage and redistribution
 *  >> of this file, and for a DISCLAIMER OF ALL WARRANTIES.
 *  >>
 *  >> Author: Paul Mackerras (paulus@cs.anu.edu.au),
 *  >>	   Department of Computer Science,
 *  >>	   Australian National University.
 *
 * ENDHEADER
 *
 * $Id: ppm.c 155 2008-10-22 12:06:38Z nijtmans $
 *
 */

/*
 * Generic initialization code, parameterized via CPACKAGE and PACKAGE.
 */

#include "init.c"


/* #define DEBUG_LOCAL */

/*
 * The maximum amount of memory to allocate for data read from the
 * file.  If we need more than this, we do it in pieces.
 */

#define MAX_MEMORY	10000		/* don't allocate > 10KB */

/*
 * Define PGM and PPM, i.e. gray images and color images.
 */

#define PGM 1
#define PPM 2

/*
 * Prototypes for local procedures defined in this file:
 */

static int CommonMatch _ANSI_ARGS_((tkimg_MFile *handle, int *widthPtr,
                int *heightPtr, int *maxIntensityPtr));
static int CommonRead _ANSI_ARGS_((Tcl_Interp *interp, tkimg_MFile *handle,
                const char *filename, Tcl_Obj *format,
                Tk_PhotoHandle imageHandle, int destX, int destY,
                int width, int height, int srcX, int srcY));
static int CommonWrite _ANSI_ARGS_((Tcl_Interp *interp,
                const char *filename, Tcl_Obj *format,
                tkimg_MFile *handle, Tk_PhotoImageBlock *blockPtr));
static int ReadPPMFileHeader _ANSI_ARGS_((tkimg_MFile *handle,
		int *widthPtr, int *heightPtr, int *maxIntensityPtr));


/*
 *----------------------------------------------------------------------
 *
 * ChnMatch --
 *
 *	This procedure is invoked by the photo image type to see if
 *	a file contains image data in PPM format.
 *
 * Results:
 *	The return value is >0 if the first characters in file "f" look
 *	like PPM data, and 0 otherwise.
 *
 * Side effects:
 *	The access position in f may change.
 *
 *----------------------------------------------------------------------
 */

static int ChnMatch(interp, chan, filename, format, widthPtr, heightPtr)
    Tcl_Interp *interp;		/* Interpreter to use for reporting errors. */
    Tcl_Channel chan;		/* The image file, open for reading. */
    const char *filename;	/* The name of the image file. */
    Tcl_Obj *format;		/* User-specified format object, or NULL. */
    int *widthPtr, *heightPtr;	/* The dimensions of the image are
				 * returned here if the file is a valid
				 * raw PPM file. */
{
    tkimg_MFile handle;
    int   dummy;

    tkimg_FixChanMatchProc (&interp, &chan, &filename, &format,
                         &widthPtr, &heightPtr);

    handle.data = (char *) chan;
    handle.state = IMG_CHAN;

    return CommonMatch(&handle, widthPtr, heightPtr, &dummy);
}

static int ObjMatch(interp, data, format, widthPtr, heightPtr)
    Tcl_Interp *interp;
    Tcl_Obj *data;
    Tcl_Obj *format;
    int *widthPtr, *heightPtr;
{
    tkimg_MFile handle;
    int   dummy;

    tkimg_FixObjMatchProc (&interp, &data, &format, &widthPtr, &heightPtr);

    tkimg_ReadInit(data, 'P', &handle);
    return CommonMatch(&handle, widthPtr, heightPtr, &dummy);
}

static int CommonMatch(handle, widthPtr, heightPtr, maxIntensityPtr)
    tkimg_MFile *handle;
    int *widthPtr;
    int *heightPtr;
    int *maxIntensityPtr;
{
    return ReadPPMFileHeader (handle, widthPtr, heightPtr, maxIntensityPtr);
}


/*
 *----------------------------------------------------------------------
 *
 * ChnRead --
 *
 *	This procedure is called by the photo image type to read
 *	PPM format data from a file and write it into a given
 *	photo image.
 *
 * Results:
 *	A standard TCL completion code.  If TCL_ERROR is returned
 *	then an error message is left in the interp's result.
 *
 * Side effects:
 *	The access position in file f is changed, and new data is
 *	added to the image given by imageHandle.
 *
 *----------------------------------------------------------------------
 */

static int ChnRead(interp, chan, filename, format, imageHandle,
                    destX, destY, width, height, srcX, srcY)
    Tcl_Interp *interp;		/* Interpreter to use for reporting errors. */
    Tcl_Channel chan;		/* The image file, open for reading. */
    const char *filename;	/* The name of the image file. */
    Tcl_Obj *format;		/* User-specified format string, or NULL. */
    Tk_PhotoHandle imageHandle;	/* The photo image to write into. */
    int destX, destY;		/* Coordinates of top-left pixel in
				 * photo image to be written to. */
    int width, height;		/* Dimensions of block of photo image to
				 * be written to. */
    int srcX, srcY;		/* Coordinates of top-left pixel to be used
				 * in image being read. */
{
    tkimg_MFile handle;

    handle.data = (char *) chan;
    handle.state = IMG_CHAN;

    return CommonRead (interp, &handle, filename, format, imageHandle,
                       destX, destY, width, height, srcX, srcY);
}

static int ObjRead (interp, data, format, imageHandle,
                    destX, destY, width, height, srcX, srcY)
    Tcl_Interp *interp;
    Tcl_Obj *data;
    Tcl_Obj *format;
    Tk_PhotoHandle imageHandle;
    int destX, destY;
    int width, height;
    int srcX, srcY;
{
    tkimg_MFile handle;

    tkimg_ReadInit (data, 'P', &handle);
    return CommonRead (interp, &handle, "InlineData", format, imageHandle,
                       destX, destY, width, height, srcX, srcY);
}

typedef struct myblock {
    Tk_PhotoImageBlock ck;
    int dummy; /* extra space for offset[3], in case it is not
                  included already in Tk_PhotoImageBlock */
} myblock;
#define block bl.ck

static int CommonRead (interp, handle, filename, format, imageHandle,
                       destX, destY, width, height, srcX, srcY)
    Tcl_Interp *interp;		/* Interpreter to use for reporting errors. */
    tkimg_MFile *handle;		/* The image file, open for reading. */
    const char *filename;	/* The name of the image file. */
    Tcl_Obj *format;		/* User-specified format string, or NULL. */
    Tk_PhotoHandle imageHandle;	/* The photo image to write into. */
    int destX, destY;		/* Coordinates of top-left pixel in
				 * photo image to be written to. */
    int width, height;		/* Dimensions of block of photo image to
				 * be written to. */
    int srcX, srcY;		/* Coordinates of top-left pixel to be used
				 * in image being read. */
{
    int fileWidth, fileHeight, maxIntensity;
    int nLines, nBytes, h, type, count;
    unsigned char *pixelPtr;
    myblock bl;

    type = ReadPPMFileHeader (handle, &fileWidth, &fileHeight, &maxIntensity);
    if (type == 0) {
	Tcl_AppendResult(interp, "couldn't read raw PPM header from file \"",
			  filename, "\"", NULL);
	return TCL_ERROR;
    }

    if ((fileWidth <= 0) || (fileHeight <= 0)) {
	Tcl_AppendResult(interp, "PPM image file \"", filename,
			  "\" has dimension(s) <= 0", (char *) NULL);
	return TCL_ERROR;
    }
    if ((maxIntensity <= 0) || (maxIntensity >= 256)) {
	char buffer[TCL_INTEGER_SPACE];

	sprintf(buffer, "%d", maxIntensity);
	Tcl_AppendResult(interp, "PPM image file \"", filename,
			  "\" has bad maximum intensity value ", buffer,
			  (char *) NULL);
	return TCL_ERROR;
    }

    if ((srcX + width) > fileWidth) {
	width = fileWidth - srcX;
    }
    if ((srcY + height) > fileHeight) {
	height = fileHeight - srcY;
    }
    if ((width <= 0) || (height <= 0)
	|| (srcX >= fileWidth) || (srcY >= fileHeight)) {
	return TCL_OK;
    }

    if (type == PGM) {
	block.pixelSize = 1;
	block.offset[1] = 0;
	block.offset[2] = 0;
    }
    else {
	block.pixelSize = 3;
	block.offset[1] = 1;
	block.offset[2] = 2;
    }
    block.offset[3] = block.offset[0] = 0;
    block.width = width;
    block.pitch = block.pixelSize * fileWidth;

    Tk_PhotoExpand(imageHandle, destX + width, destY + height);

    if (srcY > 0) {
 	/* Don't read the whole image. Skip first "srcY" lines. */
	pixelPtr = (unsigned char *) ckalloc((unsigned) block.pitch);
	for (h=0; h<srcY; h++) {
	    if (block.pitch != tkimg_Read(handle, (char *) pixelPtr, block.pitch)) {
		Tcl_AppendResult(interp, "Error reading PPM image file \"",
				  filename, "\": ", (char *) NULL);
		ckfree((char *) pixelPtr);
		return TCL_ERROR;
	    }
	}
	ckfree ((char *)pixelPtr);
    }

    nLines = (MAX_MEMORY + block.pitch - 1) / block.pitch;
    if (nLines > height) {
	nLines = height;
    }
    if (nLines <= 0) {
	nLines = 1;
    }
    nBytes = nLines * block.pitch;
    pixelPtr = (unsigned char *) ckalloc((unsigned) nBytes);
    block.pixelPtr = pixelPtr + srcX * block.pixelSize;

    for (h = height; h > 0; h -= nLines) {
	if (nLines > h) {
	    nLines = h;
	    nBytes = nLines * block.pitch;
	}
	count = tkimg_Read(handle, (char *) pixelPtr, nBytes);
	if (count != nBytes) {
	    Tcl_AppendResult(interp, "Error reading PPM image file \"",
		    filename, "\": ", (char *) NULL);
	    ckfree((char *) pixelPtr);
	    return TCL_ERROR;
	}
	if (maxIntensity != 255) {
	    unsigned char *p;

	    for (p = pixelPtr; count > 0; count--, p++) {
		*p = (((int) *p) * 255)/maxIntensity;
	    }
	}
	block.height = nLines;
	tkimg_PhotoPutBlock(interp, imageHandle, &block, destX, destY, width, nLines, TK_PHOTO_COMPOSITE_SET);
	destY += nLines;
    }

    ckfree((char *) pixelPtr);
    return TCL_OK;
}


/*
 *----------------------------------------------------------------------
 *
 * ChnWrite --
 *
 *	This procedure is invoked to write image data to a file in PPM
 *	format.
 *
 * Results:
 *	A standard TCL completion code.  If TCL_ERROR is returned
 *	then an error message is left in the interp's result.
 *
 * Side effects:
 *	Data is written to the file given by "filename".
 *
 *----------------------------------------------------------------------
 */

static int ChnWrite (interp, filename, format, blockPtr)
    Tcl_Interp *interp;
    CONST84 char *filename;
    Tcl_Obj *format;
    Tk_PhotoImageBlock *blockPtr;
{
    Tcl_Channel chan;
    tkimg_MFile handle;
    int result;

    chan = tkimg_OpenFileChannel (interp, filename, 0644);
    if (!chan) {
        return TCL_ERROR;
    }

    handle.data = (char *) chan;
    handle.state = IMG_CHAN;

    result = CommonWrite (interp, filename, format, &handle, blockPtr);
    if (Tcl_Close(interp, chan) == TCL_ERROR) {
        return TCL_ERROR;
    }
    return result;
}

static int StringWrite (interp, dataPtr, format, blockPtr)
    Tcl_Interp *interp;
    Tcl_DString *dataPtr;
    Tcl_Obj *format;
    Tk_PhotoImageBlock *blockPtr;
{
    tkimg_MFile handle;
    int result;
    Tcl_DString data;

    tkimg_FixStringWriteProc (&data, &interp, &dataPtr, &format, &blockPtr);

    tkimg_WriteInit (dataPtr, &handle);
    result = CommonWrite (interp, "InlineData", format, &handle, blockPtr);
    tkimg_Putc(IMG_DONE, &handle);

    if ((result == TCL_OK) && (dataPtr == &data)) {
        Tcl_DStringResult (interp, dataPtr);
    }
    return result;
}

static int CommonWrite (interp, filename, format, handle, blockPtr)
    Tcl_Interp *interp;
    const char *filename;
    Tcl_Obj *format;
    tkimg_MFile *handle;
    Tk_PhotoImageBlock *blockPtr;
{
    int w, h;
    int redOff, greenOff, blueOff, nBytes;
    unsigned char *scanline, *scanlinePtr;
    unsigned char *pixelPtr, *pixLinePtr;
    char header[16 + TCL_INTEGER_SPACE * 2];

    sprintf(header, "P6\n%d %d\n255\n", blockPtr->width, blockPtr->height);
    if (tkimg_Write(handle, header, strlen (header)) != strlen (header)) {
	goto writeerror;
    }

    pixLinePtr = blockPtr->pixelPtr + blockPtr->offset[0];
    redOff     = 0;
    greenOff   = blockPtr->offset[1] - blockPtr->offset[0];
    blueOff    = blockPtr->offset[2] - blockPtr->offset[0];

    nBytes = blockPtr->width * 3; /* Only RGB images allowed. */
    scanline = (unsigned char *) ckalloc((unsigned) nBytes);
    for (h = blockPtr->height; h > 0; h--) {
	pixelPtr = pixLinePtr;
	scanlinePtr = scanline;
	for (w = blockPtr->width; w > 0; w--) {
	    *(scanlinePtr++) = pixelPtr[redOff];
	    *(scanlinePtr++) = pixelPtr[greenOff];
	    *(scanlinePtr++) = pixelPtr[blueOff];
	    pixelPtr += blockPtr->pixelSize;
	}
	if (tkimg_Write(handle, (char *) scanline, nBytes) != nBytes) {
	    goto writeerror;
	}
	pixLinePtr += blockPtr->pitch;
    }
    ckfree ((char *) scanline);
    return TCL_OK;

 writeerror:
    Tcl_AppendResult(interp, "Error writing \"", filename, "\": ",
                      (char *) NULL);
    return TCL_ERROR;
}


/*
 *----------------------------------------------------------------------
 *
 * ReadPPMFileHeader --
 *
 *	This procedure reads the PPM header from the beginning of a
 *	PPM file and returns information from the header.
 *
 * Results:
 *	The return value is PGM if file "f" appears to start with
 *	a valid PGM header, PPM if "f" appears to start with a valid
 *      PPM header, and 0 otherwise.  If the header is valid,
 *	then *widthPtr and *heightPtr are modified to hold the
 *	dimensions of the image and *maxIntensityPtr is modified to
 *	hold the value of a "fully on" intensity value.
 *
 * Side effects:
 *	The access position in f advances.
 *
 *----------------------------------------------------------------------
 */

static int
ReadPPMFileHeader (handle, widthPtr, heightPtr, maxIntensityPtr)
    tkimg_MFile *handle;	/* Image file to read the header from */
    int *widthPtr, *heightPtr;	/* The dimensions of the image are
				 * returned here. */
    int *maxIntensityPtr;	/* The maximum intensity value for
				 * the image is stored here. */
{
#define BUFFER_SIZE 1000
    char buffer[BUFFER_SIZE];
    int i, numFields;
    int type = 0;
    char c;

    /*
     * Read 4 space-separated fields from the file, ignoring
     * comments (any line that starts with "#").
     */

    if (tkimg_Read(handle, &c, 1) != 1) {
	return 0;
    }
    i = 0;
    for (numFields = 0; numFields < 4; numFields++) {
	/*
	 * Skip comments and white space.
	 */

	while (1) {
	    while (isspace((unsigned char)c)) {
		if (tkimg_Read(handle, &c, 1) != 1) {
		    return 0;
		}
	    }
	    if (c != '#') {
		break;
	    }
	    do {
		if (tkimg_Read(handle, &c, 1) != 1) {
		    return 0;
		}
	    } while (c != '\n');
	}

	/*
	 * Read a field (everything up to the next white space).
	 */

	while (!isspace((unsigned char)c)) {
	    if (i < (BUFFER_SIZE-2)) {
		buffer[i] = c;
		i++;
	    }
	    if (tkimg_Read(handle, &c, 1) != 1) {
		goto done;
	    }
	}
	if (i < (BUFFER_SIZE-1)) {
	    buffer[i] = ' ';
	    i++;
	}
    }
    done:
    buffer[i] = 0;

    /*
     * Parse the fields, which are: id, width, height, maxIntensity.
     */

    if (strncmp(buffer, "P6 ", 3) == 0) {
	type = PPM;
    } else if (strncmp(buffer, "P5 ", 3) == 0) {
	type = PGM;
    } else {
	return 0;
    }
    if (sscanf(buffer+3, "%d %d %d", widthPtr, heightPtr, maxIntensityPtr)
	    != 3) {
	return 0;
    }
    return type;
}
