/*
 * gif.c --
 *
 *  GIF photo image type, Tcl/Tk package
 *
 *	A photo image file handler for GIF files. Reads 87a and 89a GIF
 *	files. At present, there only is a file write function. GIF images may be
 *	read using the -data option of the photo image.  The data may be
 *	given as a binary string in a Tcl_Obj or by representing
 *	the data as BASE64 encoded ascii.  Derived from the giftoppm code
 *	found in the pbmplus package and tkImgFmtPPM.c in the tk4.0b2
 *	distribution.
 *
 * Copyright (c) 2002 Andreas Kupries    <andreas_kupries@users.sourceforge.net>
 * Copyright (c) 1997-2003 Jan Nijtmans  <nijtmans@users.sourceforge.net>
 *
 * Copyright (c) Reed Wade (wade@cs.utk.edu), University of Tennessee
 * Copyright (c) 1995-1997 Sun Microsystems, Inc.
 * Copyright (c) 1997 Australian National University
 *
 * See the file "license.terms" for information on usage and redistribution
 * of this file, and for a DISCLAIMER OF ALL WARRANTIES.
 *
 * This file also contains code from the giftoppm program, which is
 * copyrighted as follows:
 *
 * +-------------------------------------------------------------------+
 * | Copyright 1990, David Koblas.                                     |
 * |   Permission to use, copy, modify, and distribute this software   |
 * |   and its documentation for any purpose and without fee is hereby |
 * |   granted, provided that the above copyright notice appear in all |
 * |   copies and that both that copyright notice and this permission  |
 * |   notice appear in supporting documentation.  This software is    |
 * |   provided "as is" without express or implied warranty.           |
 * +-------------------------------------------------------------------+
 *
 * $Id: gif.c 154 2008-10-22 11:44:55Z nijtmans $
 *
 */

/*
 * Generic initialization code, parameterized via CPACKAGE and PACKAGE.
 */

#include "init.c"

/*
 * Non-ASCII encoding support:
 * Most data in a GIF image is binary and is treated as such.  However,
 * a few key bits are stashed in ASCII.  If we try to compare those pieces
 * to the char they represent, it will fail on any non-ASCII (eg, EBCDIC)
 * system.  To accomodate these systems, we test against the numeric value
 * of the ASCII characters instead of the characters themselves.  This is
 * encoding independant.
 */

#  define GIF87a         "\x47\x49\x46\x38\x37\x61" /* ASCII GIF87a */
#  define GIF89a         "\x47\x49\x46\x38\x39\x61" /* ASCII GIF89a */
#  define GIF_TERMINATOR 0x3b                       /* ASCII ; */
#  define GIF_EXTENSION  0x21                       /* ASCII ! */
#  define GIF_START      0x2c                       /* ASCII , */

typedef struct {
    unsigned char buf[280];
    int bytes;
    int done;
    unsigned int window;
    int bitsInWindow;
    unsigned char *c;
    tkimg_MFile handle;
} GIFImageConfig;

/*
 * The format record for the GIF file format:
 */

static int      CommonRead  _ANSI_ARGS_((Tcl_Interp *interp,
		    GIFImageConfig *gifConfPtr, const char *fileName, Tcl_Obj *format,
		    Tk_PhotoHandle imageHandle, int destX, int destY,
		    int width, int height, int srcX, int srcY));

static int	CommonWrite _ANSI_ARGS_((Tcl_Interp *interp,
		    tkimg_MFile *handle, Tcl_Obj *format,
		    Tk_PhotoImageBlock *blockPtr));

#define INTERLACE		0x40
#define LOCALCOLORMAP		0x80
#define BitSet(byte, bit)	(((byte) & (bit)) == (bit))
#define MAXCOLORMAPSIZE		256
#define CM_RED			0
#define CM_GREEN		1
#define CM_BLUE			2
#define CM_ALPHA		3
#define MAX_LWZ_BITS		12
#define LM_to_uint(a,b)         (((b)<<8)|(a))
#define ReadOK(handle,buf,len)	(tkimg_Read(handle, (char *)(buf), len) == len)

/*
 * Prototypes for local procedures defined in this file:
 */

static int		DoExtension _ANSI_ARGS_((GIFImageConfig *gifConfPtr, int label,
			    int *transparent));

static int		GetCode _ANSI_ARGS_((GIFImageConfig *gifConfPtr, int code_size,
			    int flag));

static int		GetDataBlock _ANSI_ARGS_((GIFImageConfig *gifConfPtr,
			    unsigned char *buf));

static int		ReadColorMap _ANSI_ARGS_((GIFImageConfig *gifConfPtr, int number,
			    unsigned char buffer[MAXCOLORMAPSIZE][4]));

static int		ReadGIFHeader _ANSI_ARGS_((GIFImageConfig *gifConfPtr,
			    int *widthPtr, int *heightPtr));

static int		ReadImage _ANSI_ARGS_((Tcl_Interp *interp,
			    char *imagePtr, GIFImageConfig *gifConfPtr, int len, int rows,
			    unsigned char cmap[MAXCOLORMAPSIZE][4],
			    int width, int height, int srcX, int srcY,
			    int interlace, int transparent));

/*
 *----------------------------------------------------------------------
 *
 * ChnMatch --
 *
 *  This procedure is invoked by the photo image type to see if
 *  a channel contains image data in GIF format.
 *
 * Results:
 *  The return value is 1 if the first characters in channel chan
 *  look like GIF data, and 0 otherwise.
 *
 * Side effects:
 *  The access position in f may change.
 *
 *----------------------------------------------------------------------
 */

static int
ChnMatch(interp, chan, fileName, format, widthPtr, heightPtr)
    Tcl_Interp *interp;		/* interpreter */
    Tcl_Channel chan;		/* The image channel, open for reading. */
    const char *fileName;	/* The name of the image file. */
    Tcl_Obj *format;		/* User-specified format object, or NULL. */
    int *widthPtr, *heightPtr;	/* The dimensions of the image are
				 * returned here if the file is a valid
				 * raw GIF file. */
{
    GIFImageConfig gifConf;

    memset(&gifConf, 0, sizeof(GIFImageConfig));

    tkimg_FixChanMatchProc(&interp, &chan, &fileName, &format, &widthPtr, &heightPtr);

    gifConf.handle.data = (char *) chan;
    gifConf.handle.state = IMG_CHAN;

    return ReadGIFHeader(&gifConf, widthPtr, heightPtr);
}

/*
 *----------------------------------------------------------------------
 *
 * ChnRead --
 *
 *	This procedure is called by the photo image type to read
 *	GIF format data from a channel and write it into a given
 *	photo image.
 *
 * Results:
 *	A standard TCL completion code.  If TCL_ERROR is returned
 *	then an error message is left in the interp's result.
 *
 * Side effects:
 *	The access position in channel chan is changed, and new data is
 *	added to the image given by imageHandle.
 *
 *----------------------------------------------------------------------
 */

static int
ChnRead(interp, chan, fileName, format, imageHandle, destX, destY,
	width, height, srcX, srcY)
    Tcl_Interp *interp;		/* Interpreter to use for reporting errors. */
    Tcl_Channel chan;		/* The image channel, open for reading. */
    const char *fileName;	/* The name of the image file. */
    Tcl_Obj *format;		/* User-specified format object, or NULL. */
    Tk_PhotoHandle imageHandle;	/* The photo image to write into. */
    int destX, destY;		/* Coordinates of top-left pixel in
				 * photo image to be written to. */
    int width, height;		/* Dimensions of block of photo image to
				 * be written to. */
    int srcX, srcY;		/* Coordinates of top-left pixel to be used
				 * in image being read. */
{
    GIFImageConfig gifConf;

    memset(&gifConf, 0, sizeof(GIFImageConfig));

    gifConf.handle.data = (char *) chan;
    gifConf.handle.state = IMG_CHAN;

    return CommonRead(interp, &gifConf, fileName, format,
    	    imageHandle, destX, destY, width, height, srcX, srcY);
}

/*
 *----------------------------------------------------------------------
 *
 * CommonRead --
 *
 *	This procedure is called by the photo image type to read
 *	GIF format data from a file and write it into a given
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

typedef struct {
    Tk_PhotoImageBlock ck;
    int dummy; /* extra space for offset[3], if not included already
		  in Tk_PhotoImageBlock */
} myblock;

#define block bl.ck

static int
CommonRead(interp, gifConfPtr, fileName, format, imageHandle, destX, destY,
	width, height, srcX, srcY)
    Tcl_Interp *interp;		/* Interpreter to use for reporting errors. */
    GIFImageConfig *gifConfPtr;		/* The image file, open for reading. */
    const char *fileName;	/* The name of the image file. */
    Tcl_Obj *format;		/* User-specified format object, or NULL. */
    Tk_PhotoHandle imageHandle;	/* The photo image to write into. */
    int destX, destY;		/* Coordinates of top-left pixel in
				 * photo image to be written to. */
    int width, height;		/* Dimensions of block of photo image to
				 * be written to. */
    int srcX, srcY;		/* Coordinates of top-left pixel to be used
				 * in image being read. */
{
    int fileWidth, fileHeight;
    int nBytes, index = 0, objc = 0;
    Tcl_Obj **objv = NULL;
    myblock bl;
    unsigned char buf[100];
    char *trashBuffer = NULL;
    unsigned char *pixBuf = NULL;
    int bitPixel;
    unsigned int colorResolution;
    unsigned int background;
    unsigned int aspectRatio;
    unsigned char colorMap[MAXCOLORMAPSIZE][4];
    int transparent = -1;

    if (tkimg_ListObjGetElements(interp, format, &objc, &objv) != TCL_OK) {
	return TCL_ERROR;
    }
    if (objc > 1) {
	char *c = Tcl_GetStringFromObj(objv[1], &nBytes);
	if ((objc > 3) || ((objc == 3) && ((c[0] != '-') ||
		(c[1] != 'i') || strncmp(c, "-index", strlen(c))))) {
	    Tcl_AppendResult(interp, "invalid format: \"",
		    tkimg_GetStringFromObj(format, NULL), "\"", (char *) NULL);
	    return TCL_ERROR;
	}
    }
    if (objc > 1) {
	if (Tcl_GetIntFromObj(interp, objv[objc-1], &index) != TCL_OK) {
	    return TCL_ERROR;
	}
    }

    if (!ReadGIFHeader(gifConfPtr, &fileWidth, &fileHeight)) {
	Tcl_AppendResult(interp, "couldn't read GIF header from file \"",
		fileName, "\"", NULL);
	return TCL_ERROR;
    }
    if ((fileWidth <= 0) || (fileHeight <= 0)) {
	Tcl_AppendResult(interp, "GIF image file \"", fileName,
 		"\" has dimension(s) <= 0", (char *) NULL);
	return TCL_ERROR;
    }

    if (tkimg_Read(&gifConfPtr->handle, (char *)buf, 3) != 3) {
	return TCL_OK;
    }

    bitPixel = 2<<(buf[0]&0x07);
    colorResolution = ((((unsigned int) buf[0]&0x70)>>3)+1);
    background = buf[1];
    aspectRatio = buf[2];

    if (BitSet(buf[0], LOCALCOLORMAP)) {    /* Global Colormap */
	if (!ReadColorMap(gifConfPtr, bitPixel, colorMap)) {
	    Tcl_AppendResult(interp, "error reading color map",
		    (char *) NULL);
	    return TCL_ERROR;
	}
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

    Tk_PhotoExpand(imageHandle, destX + width, destY + height);

    block.pixelSize = 4;
    block.offset[0] = 0;
    block.offset[1] = 1;
    block.offset[2] = 2;
    block.offset[3] = 3;
    block.pixelPtr = NULL;

    while (1) {
	if (tkimg_Read(&gifConfPtr->handle, (char *)buf, 1) != 1) {
	    /*
	     * Premature end of image.  We should really notify
	     * the user, but for now just show garbage.
	     */

	    break;
	}

	if (buf[0] == GIF_TERMINATOR) {
	    /*
	     * GIF terminator.
	     */

	    Tcl_AppendResult(interp,"no image data for this index",
		    (char *) NULL);
	    goto error;
	}

	if (buf[0] == GIF_EXTENSION) {
	    /*
	     * This is a GIF extension.
	     */

	    if (tkimg_Read(&gifConfPtr->handle, (char *)buf, 1) != 1) {
		Tcl_AppendResult(interp,
			"error reading extension function code in GIF image",
			(char *) NULL);
		goto error;
	    }
	    if (DoExtension(gifConfPtr, buf[0], &transparent) < 0) {
		Tcl_AppendResult(interp, "error reading extension in GIF image",
			(char *) NULL);
		goto error;
	    }
	    continue;
	}

	if (buf[0] != GIF_START) {
	    /*
	     * Not a valid start character; ignore it.
	     */
	    continue;
	}

	if (tkimg_Read(&gifConfPtr->handle, (char *)buf, 9) != 9) {
	    Tcl_AppendResult(interp,
		    "couldn't read left/top/width/height in GIF image",
		    (char *) NULL);
	    goto error;
	}

	fileWidth = LM_to_uint(buf[4],buf[5]);
	fileHeight = LM_to_uint(buf[6],buf[7]);

	bitPixel = 2<<(buf[8]&0x07);

	if (index--) {
	    /* this is not the image we want to read: skip it. */
	    if (BitSet(buf[8], LOCALCOLORMAP)) {
		if (!ReadColorMap(gifConfPtr, bitPixel, colorMap)) {
		    Tcl_AppendResult(interp,
			    "error reading color map", (char *) NULL);
		    goto error;
		}
	    }

	    /* If we've not yet allocated a trash buffer, do so now */
	    if (trashBuffer == NULL) {
		nBytes = fileWidth * fileHeight * 3;
		trashBuffer = (char *) ckalloc((unsigned int) nBytes);
	    }

	    /*
	     * Slurp!  Process the data for this image and stuff it in a
	     * trash buffer.
	     *
	     * Yes, it might be more efficient here to *not* store the data
	     * (we're just going to throw it away later).  However, I elected
	     * to implement it this way for good reasons.  First, I wanted to
	     * avoid duplicating the (fairly complex) LWZ decoder in ReadImage.
	     * Fine, you say, why didn't you just modify it to allow the use of
	     * a NULL specifier for the output buffer?  I tried that, but it
	     * negatively impacted the performance of what I think will be the
	     * common case:  reading the first image in the file.  Rather than
	     * marginally improve the speed of the less frequent case, I chose
	     * to maintain high performance for the common case.
	     */
	    if (ReadImage(interp, trashBuffer, gifConfPtr, fileWidth,
			  fileHeight, colorMap, 0, 0, 0, 0, 0, -1) != TCL_OK) {
	      goto error;
	    }
	    continue;
	}

	/* If a trash buffer has been allocated, free it now */
	if (trashBuffer != NULL) {
	    ckfree((char *)trashBuffer);
	    trashBuffer = NULL;
	}
	if (BitSet(buf[8], LOCALCOLORMAP)) {
	    if (!ReadColorMap(gifConfPtr, bitPixel, colorMap)) {
		    Tcl_AppendResult(interp, "error reading color map",
			    (char *) NULL);
		    goto error;
	    }
	}

	index = LM_to_uint(buf[0],buf[1]);
	srcX -= index;
	if (srcX<0) {
	    destX -= srcX; width += srcX;
	    srcX = 0;
	}

	if (width > fileWidth) {
	    width = fileWidth;
	}

	index = LM_to_uint(buf[2],buf[3]);
	srcY -= index;
	if (index > srcY) {
	    destY -= srcY; height += srcY;
	    srcY = 0;
	}
	if (height > fileHeight) {
	    height = fileHeight;
	}

	if ((width <= 0) || (height <= 0)) {
	    block.pixelPtr = 0;
	    goto noerror;
	}

	block.width = width;
	block.height = height;
	block.pixelSize = (transparent>=0)?4:3;
	block.pitch = block.pixelSize * fileWidth;
	nBytes = block.pitch * fileHeight;
	pixBuf = (unsigned char *) ckalloc((unsigned) nBytes);
	block.pixelPtr = pixBuf;

	if (ReadImage(interp, (char *) block.pixelPtr, gifConfPtr, fileWidth, fileHeight,
		colorMap, fileWidth, fileHeight, srcX, srcY,
		BitSet(buf[8], INTERLACE), transparent) != TCL_OK) {
	    goto error;
	}
	break;
    }

    block.pixelPtr = pixBuf + srcY * block.pitch + srcX * block.pixelSize;
    tkimg_PhotoPutBlock(interp, imageHandle, &block, destX, destY, width, height,
	    (transparent == -1)? TK_PHOTO_COMPOSITE_SET: TK_PHOTO_COMPOSITE_OVERLAY);

    noerror:
    if (pixBuf) {
	ckfree((char *) pixBuf);
    }
    return TCL_OK;

    error:
    if (pixBuf) {
	ckfree((char *) pixBuf);
    }
    return TCL_ERROR;

}

/*
 *----------------------------------------------------------------------
 *
 * ObjMatch --
 *
 *  This procedure is invoked by the photo image type to see if
 *  an object contains image data in GIF format.
 *
 * Results:
 *  The return value is 1 if the first characters in the object are
 *  like GIF data, and 0 otherwise.
 *
 * Side effects:
 *  the size of the image is placed in widthPtr and heightPtr.
 *
 *----------------------------------------------------------------------
 */

static int
ObjMatch(interp, data, format, widthPtr, heightPtr)
    Tcl_Interp *interp;		/* interpreter */
    Tcl_Obj *data;		/* the object containing the image data */
    Tcl_Obj *format;		/* the image format object */
    int *widthPtr;		/* where to put the image width */
    int *heightPtr;		/* where to put the image height */
{
    GIFImageConfig gifConf;

    memset(&gifConf, 0, sizeof(GIFImageConfig));

    tkimg_FixObjMatchProc(&interp, &data, &format, &widthPtr, &heightPtr);

    if (!tkimg_ReadInit(data, 'G', &gifConf.handle)) {
	return 0;
    }
    return ReadGIFHeader(&gifConf, widthPtr, heightPtr);
}

/*
 *----------------------------------------------------------------------
 *
 * ObjRead --
 *
 *	This procedure is called by the photo image type to read
 *	GIF format data from a base64 encoded string, and give it to
 *	the photo image.
 *
 * Results:
 *	A standard TCL completion code.  If TCL_ERROR is returned
 *	then an error message is left in the interp's result.
 *
 * Side effects:
 *	new data is added to the image given by imageHandle.  This
 *	procedure calls FileReadGif by redefining the operation of
 *	fprintf temporarily.
 *
 *----------------------------------------------------------------------
 */

static int
ObjRead(interp, data, format, imageHandle,
	destX, destY, width, height, srcX, srcY)
    Tcl_Interp *interp;		/* interpreter for reporting errors in */
    Tcl_Obj *data;		/* object containing the image */
    Tcl_Obj *format;		/* format object if any */
    Tk_PhotoHandle imageHandle;	/* the image to write this data into */
    int destX, destY;		/* The rectangular region of the  */
    int  width, height;		/*   image to copy */
    int srcX, srcY;
{
    GIFImageConfig gifConf;

    memset(&gifConf, 0, sizeof(GIFImageConfig));

    tkimg_ReadInit(data, 'G', &gifConf.handle);
    return CommonRead(interp, &gifConf, "inline data", format,
	    imageHandle, destX, destY, width, height, srcX, srcY);
}

/*
 *----------------------------------------------------------------------
 *
 * ReadGIFHeader --
 *
 *	This procedure reads the GIF header from the beginning of a
 *	GIF file and returns the dimensions of the image.
 *
 * Results:
 *	The return value is 1 if file "f" appears to start with
 *	a valid GIF header, 0 otherwise.  If the header is valid,
 *	then *widthPtr and *heightPtr are modified to hold the
 *	dimensions of the image.
 *
 * Side effects:
 *	The access position in f advances.
 *
 *----------------------------------------------------------------------
 */

static int
ReadGIFHeader(gifConfPtr, widthPtr, heightPtr)
    GIFImageConfig *gifConfPtr;		/* Image file to read the header from */
    int *widthPtr, *heightPtr;	/* The dimensions of the image are
				 * returned here. */
{
    unsigned char buf[7];

    if ((tkimg_Read(&gifConfPtr->handle, (char *)buf, 6) != 6)
	    || ((strncmp(GIF87a, (char *) buf, 6) != 0)
	    && (strncmp(GIF89a, (char *) buf, 6) != 0))) {
	return 0;
    }

    if (tkimg_Read(&gifConfPtr->handle, (char *)buf, 4) != 4) {
	return 0;
    }

    *widthPtr = LM_to_uint(buf[0],buf[1]);
    *heightPtr = LM_to_uint(buf[2],buf[3]);
    return 1;
}

/*
 *-----------------------------------------------------------------
 * The code below is copied from the giftoppm program and modified
 * just slightly.
 *-----------------------------------------------------------------
 */

static int
ReadColorMap(gifConfPtr, number, buffer)
     GIFImageConfig *gifConfPtr;
     int number;
     unsigned char buffer[MAXCOLORMAPSIZE][4];
{
	int i;
	unsigned char rgb[3];

	for (i = 0; i < number; ++i) {
	    if (! ReadOK(&gifConfPtr->handle, rgb, sizeof(rgb))) {
		return 0;
	    }

	    if (buffer) {
		buffer[i][CM_RED] = rgb[0] ;
		buffer[i][CM_GREEN] = rgb[1] ;
		buffer[i][CM_BLUE] = rgb[2] ;
		buffer[i][CM_ALPHA] = 255 ;
	    }
	}
	return 1;
}

static int
DoExtension(gifConfPtr, label, transparent)
     GIFImageConfig *gifConfPtr;
     int label;
     int *transparent;
{
    int count;

    switch (label) {
	case 0x01:      /* Plain Text Extension */
	    break;

	case 0xff:      /* Application Extension */
	    break;

	case 0xfe:      /* Comment Extension */
	    do {
		count = GetDataBlock(gifConfPtr, (unsigned char*) gifConfPtr->buf);
	    } while (count > 0);
	    return count;

	case 0xf9:      /* Graphic Control Extension */
	    count = GetDataBlock(gifConfPtr, (unsigned char*) gifConfPtr->buf);
	    if (count < 0) {
		return 1;
	    }
	    if ((gifConfPtr->buf[0] & 0x1) != 0) {
		*transparent = gifConfPtr->buf[3];
	    }

	    do {
		count = GetDataBlock(gifConfPtr, (unsigned char*) gifConfPtr->buf);
	    } while (count > 0);
	    return count;
    }

    do {
	count = GetDataBlock(gifConfPtr, (unsigned char*) gifConfPtr->buf);
    } while (count > 0);
    return count;
}

static int
GetDataBlock(gifConfPtr, buf)
     GIFImageConfig *gifConfPtr;
     unsigned char *buf;
{
    unsigned char count;

    if (! ReadOK(&gifConfPtr->handle,&count,1)) {
	return -1;
    }

    if ((count != 0) && (! ReadOK(&gifConfPtr->handle, buf, count))) {
	return -1;
    }

    return count;
}



/*
 *----------------------------------------------------------------------
 *
 * ReadImage --
 *
 *	Process a GIF image from a given source, with a given height,
 *      width, transparency, etc.
 *
 *      This code is based on the code found in the ImageMagick GIF decoder,
 *      which is (c) 2000 ImageMagick Studio.
 *
 *      Some thoughts on our implementation:
 *      It sure would be nice if ReadImage didn't take 11 parameters!  I think
 *      that if we were smarter, we could avoid doing that.
 *
 *      Possible further optimizations:  we could pull the GetCode function
 *      directly into ReadImage, which would improve our speed.
 *
 * Results:
 *	Processes a GIF image and loads the pixel data into a memory array.
 *
 * Side effects:
 *	None.
 *
 *----------------------------------------------------------------------
 */

static int
ReadImage(interp, imagePtr, gifConfPtr, len, rows, cmap,
	width, height, srcX, srcY, interlace, transparent)
     Tcl_Interp *interp;
     char 	*imagePtr;
     GIFImageConfig *gifConfPtr;
     int len, rows;
     unsigned char   cmap[MAXCOLORMAPSIZE][4];
     int width, height;
     int srcX, srcY;
     int interlace;
     int transparent;
{
    unsigned char initialCodeSize;
    int v;
    int xpos = 0, ypos = 0, pass = 0, i;
    register char *pixelPtr;
    const static int interlaceStep[] = { 8, 8, 4, 2 };
    const static int interlaceStart[] = { 0, 4, 2, 1 };
    unsigned short prefix[(1 << MAX_LWZ_BITS)];
    unsigned char  append[(1 << MAX_LWZ_BITS)];
    unsigned char  stack[(1 << MAX_LWZ_BITS)*2];
    register unsigned char *top;
    int codeSize, clearCode, inCode, endCode, oldCode, maxCode,
	code, firstCode;

    /*
     *  Initialize the decoder
     */
    if (! ReadOK(&gifConfPtr->handle,&initialCodeSize,1))  {
	Tcl_AppendResult(interp, "error reading GIF image: ",
		Tcl_PosixError(interp), (char *) NULL);
	return TCL_ERROR;
    }
    if (initialCodeSize > MAX_LWZ_BITS) {
	Tcl_AppendResult(interp, "error reading GIF image: malformed image", (char *) NULL);
	return TCL_ERROR;
    }
    if (transparent!=-1) {
	cmap[transparent][CM_RED] = 0;
	cmap[transparent][CM_GREEN] = 0;
	cmap[transparent][CM_BLUE] = 0;
	cmap[transparent][CM_ALPHA] = 0;
    }

    pixelPtr = imagePtr;

    /* Initialize the decoder */
    /* Set values for "special" numbers:
     * clear code	reset the decoder
     * end code		stop decoding
     * code size	size of the next code to retrieve
     * max code		next available table position
     */
    clearCode   = 1 << (int) initialCodeSize;
    endCode     = clearCode + 1;
    codeSize    = (int) initialCodeSize + 1;
    maxCode     = clearCode + 2;
    oldCode     = -1;
    firstCode   = -1;

    memset((void *)prefix, 0, (1 << MAX_LWZ_BITS) * sizeof(short));
    memset((void *)append, 0, (1 << MAX_LWZ_BITS) * sizeof(char));
    for (i = 0; i < clearCode; i++) {
	append[i] = i;
    }
    top = stack;

    GetCode(gifConfPtr, 0, 1);

    /* Read until we finish the image */
    for (i = 0, ypos = 0; i < rows; i++) {
	for (xpos = 0; xpos < len; ) {

	    if (top == stack) {
		/* Bummer -- our stack is empty.  Now we have to work! */
		code = GetCode(gifConfPtr, codeSize, 0);
		if (code < 0) {
		    return TCL_OK;
		}

		if (code > maxCode || code == endCode) {
		    /*
		     * If we're doing things right, we should never
		     * receive a code that is greater than our current
		     * maximum code.  If we do, bail, because our decoder
		     * does not yet have that code set up.
		     *
		     * If the code is the magic endCode value, quit.
		     */
		    return TCL_OK;
		}

		if (code == clearCode) {
		    /* Reset the decoder */
		    codeSize    = initialCodeSize + 1;
		    maxCode     = clearCode + 2;
		    oldCode     = -1;
		    continue;
		}

		if (oldCode == -1) {
		    /*
		     * Last pass reset the decoder, so the first code we
		     * see must be a singleton.  Seed the stack with it,
		     * and set up the old/first code pointers for
		     * insertion into the string table.  We can't just
		     * roll this into the clearCode test above, because
		     * at that point we have not yet read the next code.
		     */
		    *top++=append[code];
		    oldCode = code;
		    firstCode = code;
		    continue;
		}

		inCode = code;

		if (code == maxCode) {
		    /*
		     * maxCode is always one bigger than our highest assigned
		     * code.  If the code we see is equal to maxCode, then
		     * we are about to add a new string to the table. ???
		     */
		    *top++ = firstCode;
		    code = oldCode;
		}

		while (code > clearCode) {
		    /*
		     * Populate the stack by tracing the string in the
		     * string table from its tail to its head
		     */
		    *top++ = append[code];
		    code = prefix[code];
		}
		firstCode = append[code];

		/*
		 * If there's no more room in our string table, quit.
		 * Otherwise, add a new string to the table
		 */
		if (maxCode >= (1 << MAX_LWZ_BITS)) {
		    return TCL_OK;
		}

		/* Push the head of the string onto the stack */
		*top++ = firstCode;

		/* Add a new string to the string table */
		prefix[maxCode] = oldCode;
		append[maxCode] = firstCode;
		maxCode++;

		/* maxCode tells us the maximum code value we can accept.
		 * If we see that we need more bits to represent it than
		 * we are requesting from the unpacker, we need to increase
		 * the number we ask for.
		 */
		if ((maxCode >= (1 << codeSize))
			&& (maxCode < (1<<MAX_LWZ_BITS))) {
		    codeSize++;
		}
		oldCode = inCode;
	    }

	    /* Pop the next color index off the stack */
	    v = *(--top);
	    if (v < 0) {
		return TCL_OK;
	    }

	    /*
	     * If pixelPtr is null, we're skipping this image (presumably
	     * there are more in the file and we will be called to read
	     * one of them later)
	     */
	    *pixelPtr++ = cmap[v][CM_RED];
	    *pixelPtr++ = cmap[v][CM_GREEN];
	    *pixelPtr++ = cmap[v][CM_BLUE];
	    if (transparent >= 0) {
		*pixelPtr++ = cmap[v][CM_ALPHA];
	    }
	    xpos++;

	}

	/* If interlacing, the next ypos is not just +1 */
	if (interlace) {
	    ypos += interlaceStep[pass];
	    while (ypos >= height) {
		pass++;
		if (pass > 3) {
		    return TCL_OK;
		}
		ypos = interlaceStart[pass];
	    }
	} else {
	    ypos++;
	}
	pixelPtr = imagePtr + (ypos) * len * ((transparent>=0)?4:3);
    }
    return TCL_OK;
}

/*
 *----------------------------------------------------------------------
 *
 * GetCode --
 *
 *      Extract the next compression code from the file.  In GIF's, the
 *      compression codes are between 3 and 12 bits long and are then
 *      packed into 8 bit bytes, left to right, for example:
 *                 bbbaaaaa
 *                 dcccccbb
 *                 eeeedddd
 *                 ...
 *      We use a byte buffer read from the file and a sliding window
 *      to unpack the bytes.  Thanks to ImageMagick for the sliding window
 *      idea.
 *      args:  handle         the handle to read from
 *             code_size    size of the code to extract
 *             flag         boolean indicating whether the extractor
 *                          should be reset or not
 *
 * Results:
 *	code                the next compression code
 *
 * Side effects:
 *	May consume more input from chan.
 *
 *----------------------------------------------------------------------
 */

static int
GetCode(gifConfPtr, code_size, flag)
     GIFImageConfig *gifConfPtr;
     int code_size;
     int flag;
{
    int ret;

    if (flag) {
	/* Initialize the decoder */
	gifConfPtr->bitsInWindow = 0;
	gifConfPtr->bytes = 0;
	gifConfPtr->window = 0;
	gifConfPtr->done = 0;
	gifConfPtr->c = NULL;
	return 0;
    }

    while (gifConfPtr->bitsInWindow < code_size) {
	/* Not enough bits in our window to cover the request */
	if (gifConfPtr->done) {
	    return -1;
	}
	if (gifConfPtr->bytes == 0) {
	    /* Not enough bytes in our buffer to add to the window */
	    gifConfPtr->bytes = GetDataBlock(gifConfPtr, gifConfPtr->buf);
	    gifConfPtr->c = gifConfPtr->buf;
	    if (gifConfPtr->bytes <= 0) {
		gifConfPtr->done = 1;
		break;
	    }
	}
	/* Tack another byte onto the window, see if that's enough */
	gifConfPtr->window += (*gifConfPtr->c) << gifConfPtr->bitsInWindow;
	gifConfPtr->c++;
	gifConfPtr->bitsInWindow += 8;
	gifConfPtr->bytes--;
    }


    /* The next code will always be the last code_size bits of the window */
    ret = gifConfPtr->window & ((1 << code_size) - 1);

    /* Shift data in the window to put the next code at the end */
    gifConfPtr->window >>= code_size;
    gifConfPtr->bitsInWindow -= code_size;
    return ret;
}

/*
 * This software is copyrighted as noted below.  It may be freely copied,
 * modified, and redistributed, provided that the copyright notice is
 * preserved on all copies.
 *
 * There is no warranty or other guarantee of fitness for this software,
 * it is provided solely "as is".  Bug reports or fixes may be sent
 * to the author, who may or may not act on them as he desires.
 *
 * You may not include this software in a program or other software product
 * without supplying the source, or without informing the end-user that the
 * source is available for no extra charge.
 *
 * If you modify this software, you should include a notice giving the
 * name of the person performing the modification, the date of modification,
 * and the reason for such modification.
 */


/*
 * ChnWrite - writes a image in GIF format.
 *-------------------------------------------------------------------------
 * Author:          		Lolo
 *                              Engeneering Projects Area
 *	            		Department of Mining
 *                  		University of Oviedo
 * e-mail			zz11425958@zeus.etsimo.uniovi.es
 *                  		lolo@pcsig22.etsimo.uniovi.es
 * Date:            		Fri September 20 1996
 *
 * Modified for transparency handling (gif89a) and miGIF compression
 * by Jan Nijtmans <nijtmans@users.sourceforge.net>
 *
 *----------------------------------------------------------------------
 * FileWriteGIF-
 *
 *    This procedure is called by the photo image type to write
 *    GIF format data from a photo image into a given file
 *
 * Results:
 *	A standard TCL completion code.  If TCL_ERROR is returned
 *	then an error message is left in the interp's result.
 *
 *----------------------------------------------------------------------
 */

 /*
  *  Types, defines and variables needed to write and compress a GIF.
  */

#define LSB(a)                  ((unsigned char) (((short)(a)) & 0x00FF))
#define MSB(a)                  ((unsigned char) (((short)(a)) >> 8))

#define GIFBITS 12
#define HSIZE  5003            /* 80% occupancy */

typedef struct {
    int ssize;
    int csize;
    int rsize;
    unsigned char *pixelo;
    int pixelSize;
    int pixelPitch;
    int greenOffset;
    int blueOffset;
    int alphaOffset;
    int num;
    unsigned char mapa[MAXCOLORMAPSIZE][3];
} GifWriterState;

typedef int (* ifunptr) _ANSI_ARGS_((GifWriterState *statePtr));

/*
 *	Definition of new functions to write GIFs
 */

static int color _ANSI_ARGS_((GifWriterState *statePtr, int red, int green, int blue));

static void compress _ANSI_ARGS_((GifWriterState *statePtr, int init_bits, tkimg_MFile *handle,
		ifunptr readValue));

static int nuevo _ANSI_ARGS_((GifWriterState *statePtr, int red, int green ,int blue));

static int savemap _ANSI_ARGS_((GifWriterState *statePtr, Tk_PhotoImageBlock *blockPtr));

static int ReadValue _ANSI_ARGS_((GifWriterState *statePtr));

static int no_bits _ANSI_ARGS_((int colors));

static int
ChnWrite (interp, filename, format, blockPtr)
    Tcl_Interp *interp;		/* Interpreter to use for reporting errors. */
    const char	*filename;
    Tcl_Obj *format;
    Tk_PhotoImageBlock *blockPtr;
{
    Tcl_Channel chan = NULL;
    tkimg_MFile handle;
    int result;

    chan = tkimg_OpenFileChannel(interp, filename, 0644);
    if (!chan) {
	return TCL_ERROR;
    }

    handle.data = (char *) chan;
    handle.state = IMG_CHAN;

    result = CommonWrite(interp, &handle, format, blockPtr);
    if (Tcl_Close(interp, chan) == TCL_ERROR) {
	return TCL_ERROR;
    }
    return result;
}

static int
StringWrite(interp, dataPtr, format, blockPtr)
    Tcl_Interp *interp;
    Tcl_DString *dataPtr;
    Tcl_Obj *format;
    Tk_PhotoImageBlock *blockPtr;
{
    int result;
    tkimg_MFile handle;
    Tcl_DString data;

    tkimg_FixStringWriteProc(&data, &interp, &dataPtr, &format, &blockPtr);

    Tcl_DStringSetLength(dataPtr, 1024);
    tkimg_WriteInit(dataPtr, &handle);

    result = CommonWrite(interp, &handle, format, blockPtr);
    tkimg_Putc(IMG_DONE, &handle);

    if ((result == TCL_OK) && (dataPtr == &data)) {
	Tcl_DStringResult(interp, dataPtr);
    }
    return result;
}

static int
CommonWrite(interp, handle, format, blockPtr)
    Tcl_Interp *interp;
    tkimg_MFile *handle;
    Tcl_Obj *format;
    Tk_PhotoImageBlock *blockPtr;
{
    GifWriterState state;
    int  resolution;
    long  numcolormap;

    long  width,height,x;
    unsigned char c;
    unsigned int top,left;
    int num;

    top = 0;
    left = 0;

    state.pixelSize=blockPtr->pixelSize;
    state.greenOffset=blockPtr->offset[1]-blockPtr->offset[0];
    state.blueOffset=blockPtr->offset[2]-blockPtr->offset[0];
    state.alphaOffset = blockPtr->offset[0];
    if (state.alphaOffset < blockPtr->offset[2]) {
	state.alphaOffset = blockPtr->offset[2];
    }
    if (++state.alphaOffset < state.pixelSize) {
	state.alphaOffset -= blockPtr->offset[0];
    } else {
	state.alphaOffset = 0;
    }

    tkimg_Write(handle, (const char *) (state.alphaOffset ? GIF89a:GIF87a), 6);

    for (x=0;x<MAXCOLORMAPSIZE;x++) {
	state.mapa[x][CM_RED] = 255;
	state.mapa[x][CM_GREEN] = 255;
	state.mapa[x][CM_BLUE] = 255;
    }


    width=blockPtr->width;
    height=blockPtr->height;
    state.pixelo=blockPtr->pixelPtr + blockPtr->offset[0];
    state.pixelPitch=blockPtr->pitch;
    if ((num=savemap(&state,blockPtr))<0) {
	Tcl_AppendResult(interp, "too many colors", (char *) NULL);
	return TCL_ERROR;
    }
    if (state.num<3) state.num=3;
    c=LSB(width);
    tkimg_Putc(c,handle);
    c=MSB(width);
    tkimg_Putc(c,handle);
    c=LSB(height);
    tkimg_Putc(c,handle);
    c=MSB(height);
    tkimg_Putc(c,handle);

    c= (1 << 7) | (no_bits(num) << 4) | (no_bits(num));
    tkimg_Putc(c,handle);
    resolution = no_bits(num)+1;

    numcolormap=1 << resolution;

    /*  background color */

    tkimg_Putc(0,handle);

    /*  zero for future expansion  */

    tkimg_Putc(0,handle);

    for (x=0; x<numcolormap ;x++) {
	tkimg_Putc(state.mapa[x][CM_RED],handle);
	tkimg_Putc(state.mapa[x][CM_GREEN],handle);
	tkimg_Putc(state.mapa[x][CM_BLUE],handle);
    }

    /*
     * Write out extension for transparent colour index, if necessary.
     */

    if (state.alphaOffset) {
	tkimg_Write(handle, "!\371\4\1\0\0\0", 8);
    }

    c = GIF_START;
    tkimg_Putc(c,handle);
    c=LSB(top);
    tkimg_Putc(c,handle);
    c=MSB(top);
    tkimg_Putc(c,handle);
    c=LSB(left);
    tkimg_Putc(c,handle);
    c=MSB(left);
    tkimg_Putc(c,handle);

    c=LSB(width);
    tkimg_Putc(c,handle);
    c=MSB(width);
    tkimg_Putc(c,handle);

    c=LSB(height);
    tkimg_Putc(c,handle);
    c=MSB(height);
    tkimg_Putc(c,handle);

    c=0;
    tkimg_Putc(c,handle);
    c=resolution;
    tkimg_Putc(c,handle);

    state.ssize = state.rsize = blockPtr->width;
    state.csize = blockPtr->height;
    compress(&state, resolution+1, handle, ReadValue);

    tkimg_Putc(0,handle);
    c = GIF_TERMINATOR;
    tkimg_Putc(c,handle);

    return TCL_OK;
}

static int
color(statePtr, red, green, blue)
    GifWriterState *statePtr;
    int red;
    int green;
    int blue;
{
    int x;
    for (x=(statePtr->alphaOffset != 0);x<=MAXCOLORMAPSIZE;x++) {
	if ((statePtr->mapa[x][CM_RED]==red) && (statePtr->mapa[x][CM_GREEN]==green) &&
		(statePtr->mapa[x][CM_BLUE]==blue)) {
	    return x;
	}
    }
    return -1;
}


static int
nuevo(statePtr, red, green, blue)
    GifWriterState *statePtr;
    int red,green,blue;
{
    int x;
    for (x=(statePtr->alphaOffset != 0);x<statePtr->num;x++) {
	if ((statePtr->mapa[x][CM_RED]==red) && (statePtr->mapa[x][CM_GREEN]==green) &&
		(statePtr->mapa[x][CM_BLUE]==blue)) {
	    return 0;
	}
    }
    return 1;
}

static int
savemap(statePtr, blockPtr)
    GifWriterState *statePtr;
    Tk_PhotoImageBlock *blockPtr;
{
    unsigned char  *colores;
    int x,y;
    unsigned char  red,green,blue;

    if (statePtr->alphaOffset) {
	statePtr->num = 1;
	statePtr->mapa[0][CM_RED] = 0xd9;
	statePtr->mapa[0][CM_GREEN] = 0xd9;
	statePtr->mapa[0][CM_BLUE] = 0xd9;
    } else {
	statePtr->num = 0;
    }

    for(y=0;y<blockPtr->height;y++) {
	colores=blockPtr->pixelPtr + blockPtr->offset[0]
		+ y * blockPtr->pitch;
	for(x=0;x<blockPtr->width;x++) {
	    if (!statePtr->alphaOffset || (colores[statePtr->alphaOffset] != 0)) {
		red = colores[0];
		green = colores[statePtr->greenOffset];
		blue = colores[statePtr->blueOffset];
		if (nuevo(statePtr, red, green, blue)) {
		    if (statePtr->num>255)
			return -1;

		    statePtr->mapa[statePtr->num][CM_RED]=red;
		    statePtr->mapa[statePtr->num][CM_GREEN]=green;
		    statePtr->mapa[statePtr->num][CM_BLUE]=blue;
		    statePtr->num++;
		}
	    }
	    colores += statePtr->pixelSize;
	}
    }
    return statePtr->num;
}

static int
ReadValue(statePtr)
    GifWriterState *statePtr;
{
    unsigned int col;

    if (statePtr->csize == 0) {
	return EOF;
    }
    if (statePtr->alphaOffset && (statePtr->pixelo[statePtr->alphaOffset]==0)) {
	col = 0;
    } else {
	col = color(statePtr, statePtr->pixelo[0],statePtr->pixelo[statePtr->greenOffset],statePtr->pixelo[statePtr->blueOffset]);
    }
    statePtr->pixelo += statePtr->pixelSize;
    if (--statePtr->ssize <= 0) {
	statePtr->ssize = statePtr->rsize;
	statePtr->csize--;
	statePtr->pixelo += statePtr->pixelPitch - (statePtr->rsize * statePtr->pixelSize);
    }

    return col;
}

/*
 * Return the number of bits ( -1 ) to represent a given
 * number of colors ( ex: 256 colors => 7 ).
 */
static int
no_bits( colors )
int colors;
{
    register int bits = 0;

    colors--;
    while ( colors >> bits ) {
	bits++;
    }

    return (bits-1);
}


/*
 *
 * GIF Image compression - modified 'compress'
 *
 * Based on: compress.c - File compression ala IEEE Computer, June 1984.
 *
 * By Authors:  Spencer W. Thomas       (decvax!harpo!utah-cs!utah-gr!thomas)
 *              Jim McKie               (decvax!mcvax!jim)
 *              Steve Davies            (decvax!vax135!petsd!peora!srd)
 *              Ken Turkowski           (decvax!decwrl!turtlevax!ken)
 *              James A. Woods          (decvax!ihnp4!ames!jaw)
 *              Joe Orost               (decvax!vax135!petsd!joe)
 *
 */

#define MAXCODE(n_bits)		(((long) 1 << (n_bits)) - 1)

typedef struct {
    int n_bits;		/* number of bits/code */
    long maxcode;		/* maximum code, given n_bits */
    int		htab[HSIZE];
    unsigned int	codetab[HSIZE];

    long hsize;	/* for dynamic table sizing */

/*
 * To save much memory, we overlay the table used by compress() with those
 * used by decompress().  The tab_prefix table is the same size and type
 * as the codetab.  The tab_suffix table needs 2**GIFBITS characters.  We
 * get this from the beginning of htab.  The output stack uses the rest
 * of htab, and contains characters.  There is plenty of room for any
 * possible stack (stack used to be 8000 characters).
 */

    int free_ent;  /* first unused entry */

/*
 * block compression parameters -- after all codes are used up,
 * and compression rate changes, start over.
 */
    int clear_flg;

    int offset;
    unsigned int in_count;            /* length of input */
    unsigned int out_count;           /* # of codes output (for debugging) */

/*
 * compress stdin to stdout
 *
 * Algorithm:  use open addressing double hashing (no chaining) on the
 * prefix code / next character combination.  We do a variant of Knuth's
 * algorithm D (vol. 3, sec. 6.4) along with G. Knott's relatively-prime
 * secondary probe.  Here, the modular division first probe is gives way
 * to a faster exclusive-or manipulation.  Also do block compression with
 * an adaptive reset, whereby the code table is cleared when the compression
 * ratio decreases, but after the table fills.  The variable-length output
 * codes are re-sized at this point, and a special CLEAR code is generated
 * for the decompressor.  Late addition:  construct the table according to
 * file size for noticeable speed improvement on small files.  Please direct
 * questions about this implementation to ames!jaw.
 */

    int g_init_bits;
    tkimg_MFile *g_outfile;

    int ClearCode;
    int EOFCode;

    unsigned long cur_accum;
    int  cur_bits;

/*
 * Number of characters so far in this 'packet'
 */
    int a_count;

/*
 * Define the storage for the packet accumulator
 */
    unsigned char accum[256];
} GIFState_t;

static void output _ANSI_ARGS_((GIFState_t *statePtr, long code));
static void cl_block _ANSI_ARGS_((GIFState_t *statePtr));
static void cl_hash _ANSI_ARGS_((GIFState_t *statePtr, int hsize));
static void char_init _ANSI_ARGS_((GIFState_t *statePtr));
static void char_out _ANSI_ARGS_((GIFState_t *statePtr, int c));
static void flush_char _ANSI_ARGS_((GIFState_t *statePtr));

static void compress(statePtr, init_bits, handle, readValue)
    GifWriterState *statePtr;
    int init_bits;
    tkimg_MFile *handle;
    ifunptr readValue;
{
    register long fcode;
    register long i = 0;
    register int c;
    register long ent;
    register long disp;
    register long hsize_reg;
    register int hshift;
    GIFState_t state;

    memset(&state, 0, sizeof(state));
    /*
     * Set up the globals:  g_init_bits - initial number of bits
     *                      g_outfile   - pointer to output file
     */
    state.g_init_bits = init_bits;
    state.g_outfile = handle;

    /*
     * Set up the necessary values
     */
    state.offset = 0;
    state.hsize = HSIZE;
    state.out_count = 0;
    state.clear_flg = 0;
    state.in_count = 1;
    state.maxcode = MAXCODE(state.n_bits = state.g_init_bits);

    state.ClearCode = (1 << (init_bits - 1));
    state.EOFCode = state.ClearCode + 1;
    state.free_ent = state.ClearCode + 2;

    char_init(&state);

    ent = readValue(statePtr);

    hshift = 0;
    for ( fcode = (long) state.hsize;  fcode < 65536L; fcode *= 2L )
        hshift++;
    hshift = 8 - hshift;                /* set hash code range bound */

    hsize_reg = state.hsize;
    cl_hash(&state, (int) hsize_reg);            /* clear hash table */

    output(&state, (long)state.ClearCode);

#ifdef SIGNED_COMPARE_SLOW
    while ( (c = readValue(statePtr) ) != (unsigned) EOF ) {
#else
    while ( (c = readValue(statePtr)) != EOF ) {
#endif

        state.in_count++;

        fcode = (long) (((long) c << GIFBITS) + ent);
        i = (((long)c << hshift) ^ ent);    /* xor hashing */

        if (state.htab[i] == fcode) {
            ent = state.codetab[i];
            continue;
        } else if ( (long) state.htab[i] < 0 )      /* empty slot */
            goto nomatch;
        disp = hsize_reg - i;           /* secondary hash (after G. Knott) */
        if ( i == 0 )
            disp = 1;
probe:
        if ( (i -= disp) < 0 )
            i += hsize_reg;

        if (state.htab[i] == fcode) {
            ent = state.codetab[i];
            continue;
        }
        if ( (long) state.htab[i] > 0 )
            goto probe;
nomatch:
        output (&state, (long) ent);
        state.out_count++;
        ent = c;
#ifdef SIGNED_COMPARE_SLOW
        if ( (unsigned) free_ent < (unsigned) ((long)1 << GIFBITS)) {
#else
        if (state.free_ent < ((long)1 << GIFBITS)) {
#endif
            state.codetab[i] = state.free_ent++; /* code -> hashtable */
            state.htab[i] = fcode;
        } else
                cl_block(&state);
    }
    /*
     * Put out the final code.
     */
    output(&state, (long)ent);
    state.out_count++;
    output(&state, (long) state.EOFCode);

    return;
}

/*****************************************************************
 * TAG( output )
 *
 * Output the given code.
 * Inputs:
 *      code:   A n_bits-bit integer.  If == -1, then EOF.  This assumes
 *              that n_bits =< (long) wordsize - 1.
 * Outputs:
 *      Outputs code to the file.
 * Assumptions:
 *      Chars are 8 bits long.
 * Algorithm:
 *      Maintain a GIFBITS character long buffer (so that 8 codes will
 * fit in it exactly).  Use the VAX insv instruction to insert each
 * code in turn.  When the buffer fills up empty it and start over.
 */

static const
unsigned long masks[] = { 0x0000, 0x0001, 0x0003, 0x0007, 0x000F,
                                  0x001F, 0x003F, 0x007F, 0x00FF,
                                  0x01FF, 0x03FF, 0x07FF, 0x0FFF,
                                  0x1FFF, 0x3FFF, 0x7FFF, 0xFFFF };

static void
output(statePtr, code)
    GIFState_t *statePtr;
    long  code;
{
    statePtr->cur_accum &= masks[statePtr->cur_bits];

    if (statePtr->cur_bits > 0) {
	statePtr->cur_accum |= ((long) code << statePtr->cur_bits);
    } else {
	statePtr->cur_accum = code;
    }

    statePtr->cur_bits += statePtr->n_bits;

    while (statePtr->cur_bits >= 8 ) {
	char_out(statePtr, (unsigned int)(statePtr->cur_accum & 0xff));
	statePtr->cur_accum >>= 8;
	statePtr->cur_bits -= 8;
    }

    /*
     * If the next entry is going to be too big for the code size,
     * then increase it, if possible.
     */

    if ((statePtr->free_ent > statePtr->maxcode)|| statePtr->clear_flg ) {
	if (statePtr->clear_flg) {
	    statePtr->maxcode = MAXCODE(statePtr->n_bits = statePtr->g_init_bits);
	    statePtr->clear_flg = 0;
	} else {
	    statePtr->n_bits++;
	    if (statePtr->n_bits == GIFBITS) {
		statePtr->maxcode = (long)1 << GIFBITS;
	    } else {
		statePtr->maxcode = MAXCODE(statePtr->n_bits);
	    }
	}
    }

    if (code == statePtr->EOFCode) {
	/*
	 * At EOF, write the rest of the buffer.
	 */
        while (statePtr->cur_bits > 0) {
	    char_out(statePtr, (unsigned int)(statePtr->cur_accum & 0xff));
	    statePtr->cur_accum >>= 8;
	    statePtr->cur_bits -= 8;
	}
	flush_char(statePtr);
    }
}

/*
 * Clear out the hash table
 */
static void
cl_block(statePtr)             /* table clear for block compress */
    GIFState_t *statePtr;
{

        cl_hash (statePtr, (int) statePtr->hsize);
        statePtr->free_ent = statePtr->ClearCode + 2;
        statePtr->clear_flg = 1;

        output(statePtr, (long) statePtr->ClearCode);
}

static void
cl_hash(statePtr, hsize)          /* reset code table */
    GIFState_t *statePtr;
    int hsize;
{
    register int *htab_p = statePtr->htab+hsize;
    register long i;
    register long m1 = -1;

    i = hsize - 16;
    do {                            /* might use Sys V memset(3) here */
	*(htab_p-16) = m1;
	*(htab_p-15) = m1;
	*(htab_p-14) = m1;
	*(htab_p-13) = m1;
	*(htab_p-12) = m1;
	*(htab_p-11) = m1;
	*(htab_p-10) = m1;
	*(htab_p-9) = m1;
	*(htab_p-8) = m1;
	*(htab_p-7) = m1;
	*(htab_p-6) = m1;
	*(htab_p-5) = m1;
	*(htab_p-4) = m1;
	*(htab_p-3) = m1;
	*(htab_p-2) = m1;
	*(htab_p-1) = m1;
	htab_p -= 16;
    } while ((i -= 16) >= 0);

    for (i += 16; i > 0; i--) {
	*--htab_p = m1;
    }
}


/******************************************************************************
 *
 * GIF Specific routines
 *
 ******************************************************************************/

/*
 * Set up the 'byte output' routine
 */
static void
char_init(statePtr)
    GIFState_t *statePtr;
{
    statePtr->a_count = 0;
    statePtr->cur_accum = 0;
    statePtr->cur_bits = 0;
}

/*
 * Add a character to the end of the current packet, and if it is 254
 * characters, flush the packet to disk.
 */
static void
char_out(statePtr, c)
    GIFState_t *statePtr;
    int c;
{
    statePtr->accum[statePtr->a_count++] = c;
    if (statePtr->a_count >= 254) {
	flush_char(statePtr);
    }
}

/*
 * Flush the packet to disk, and reset the accumulator
 */
static void
flush_char(statePtr)
    GIFState_t *statePtr;
{
    unsigned char c;
    if (statePtr->a_count > 0) {
	c = statePtr->a_count;
	tkimg_Write(statePtr->g_outfile, (const char *) &c, 1);
	tkimg_Write(statePtr->g_outfile, (const char *) statePtr->accum, statePtr->a_count);
	statePtr->a_count = 0;
    }
}

/* The End */
