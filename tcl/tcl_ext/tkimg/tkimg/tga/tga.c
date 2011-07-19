/* STARTHEADER
 *
 * File :       tga.c
 *
 * Author :     Paul Obermeier (paul@poSoft.de)
 *
 * Date :       Wed Nov 22 21:45:17 CET 2000
 *
 * Copyright :  (C) 2000-2002 Paul Obermeier
 *
 * Description :
 *
 * A photo image handler for Truevision's TARGA file format.
 *
 * The following image types are supported:
 *
 * 24-bit pixels: True-color (RGB, each channel 8 bit).
 * 32-bit pixels: True-color with alpha channel (RGBA, each channel 8 bit).
 *
 * List of currently supported features:
 *
 * Type   |     Read      |     Write     |
 *        | -file | -data | -file | -data |
 * ----------------------------------------
 * 24-bit | Yes   | Yes   | Yes   | Yes   |
 * 32-bit | Yes   | Yes   | Yes   | Yes   |
 *
 * All images types may be either uncompressed (Targa-Type 2) or
 * run-length encoded (Targa-Type 10).
 *
 *
 * The following format options are available:
 *
 * Read  TGA image: "tga -matte <bool> -verbose <bool>"
 * Write TGA image: "tga -matte <bool> -verbose <bool> -compression <type>"
 *
 * -matte <bool>:       If set to false, a matte (alpha) channel is ignored
 *                      during reading or writing. Default is true.
 * -verbose <bool>:     If set to true, additional information about the file
 *                      format is printed to stdout. Default is false.
 * -compression <type>: Set the compression mode to either "none" or "rle".
 * 			Default is "rle".
 *
 * Notes:
 *
 * - As Targa files do not have a "magic number" somewhere in the file header,
 *   it is difficult to automatically recognize this format.
 *   Therefore it should be specified as one of the first entries in the list of
 *   package require tkimg::*.
 *
 * ENDHEADER
 *
 * $Id: tga.c 233 2010-04-01 09:28:00Z nijtmans $
 *
 */

/*
 * Generic initialization code, parameterized via CPACKAGE and PACKAGE.
 */

#include "init.c"


/* #define DEBUG_LOCAL */

/* Some defines and typedefs. */
#define TRUE  1
#define FALSE 0
typedef unsigned char Boln;	/* Boolean value: TRUE or FALSE */
typedef unsigned char UByte;	/* Unsigned  8 bit integer */
typedef char  Byte;		/* Signed    8 bit integer */
typedef short Short;		/* Signed   16 bit integer */
typedef int Int;		/* Signed   32 bit integer */

/* Supported TARGA version numbers */
#define TGA_RGB_UNCOMP	 2
#define TGA_RGB_COMP	10

/* Macros needed for run-length encoding. */
#define TGA_MODE_SAME	0
#define TGA_MODE_DIFF	1
#define MINRUN   3
#define MAXRUN 127

/* Macros for acessing header fields */
#define ENC_LEFT_RIGHT(imgdes) (((imgdes >> 4) & 0x1)? FALSE: TRUE)
#define ENC_TOP_BOTTOM(imgdes) (((imgdes >> 5) & 0x1)? TRUE: FALSE)
#define NCHAN(pixsize)         ((pixsize == 24) ? 3: 4)
#define IS_COMPRESSED(imgtyp)  ((imgtyp == TGA_RGB_COMP)? TRUE: FALSE)

/* The Targa header structure */
typedef struct {
    UByte numid;
    UByte maptyp;
    UByte imgtyp;
    Short maporig;
    Short mapsize;
    UByte mapbits;
    Short xorig;
    Short yorig;
    Short xsize;
    Short ysize;
    UByte pixsize;
    UByte imgdes;
} TGAHEADER;

/* Structure to hold information about the Targa file being processed. */
typedef struct {
    TGAHEADER th;
    Int   scanrest,	/* Number of pixels belonging to next scanline */
	  scanmode;	/* Current compression mode */
    UByte *red,		/* Pointers to step through scanlines */
	  *green,
	  *blue,
	  *matte;
    UByte *redScan,	/* Buffer for one scanline: Red   channel */
	  *greenScan,	/* Buffer for one scanline: Green channel */
	  *blueScan,	/* Buffer for one scanline: Blue  channel */
	  *matteScan;	/* Buffer for one scanline: Matte channel */
    UByte *pixbuf;
#ifdef DEBUG_LOCAL
	Int total;
#endif
} TGAFILE;

static void tgaClose(TGAFILE *tf)
{
    if (tf->redScan)   ckfree((char *)tf->redScan);
    if (tf->greenScan) ckfree((char *)tf->greenScan);
    if (tf->blueScan)  ckfree((char *)tf->blueScan);
    if (tf->matteScan) ckfree((char *)tf->matteScan);
    if (tf->pixbuf)    ckfree((char *)tf->pixbuf);
    return;
}

static Boln readError(Tcl_Interp *interp)
{
    Tcl_AppendResult(interp, "Unexpected end of file", (char *) NULL);
    return FALSE;
}

/* This function is commented out because it is not used anywhere
static Boln writeError(Tcl_Interp *interp)
{
    Tcl_AppendResult(interp, "Error writing to file", (char *) NULL);
    return FALSE;
}
*/

/* Read 1 byte, representing an unsigned integer number. */

static Boln readUByte (tkimg_MFile *handle, UByte *b)
{
    char buf[1];
    if (1 != tkimg_Read(handle, buf, 1))
        return FALSE;
    *b = buf[0];
    return TRUE;
}

/* Read 2 bytes, representing a short integer in the form <LowByte, HighByte>,
   from a file and convert them into the current machine's format. */

static Boln readShort (tkimg_MFile *handle, Short *s)
{
    char buf[2];
    if (2 != tkimg_Read(handle, buf, 2))
        return FALSE;
    *s = (buf[0] & 0xFF) | (buf[1] << 8);
    return TRUE;
}

/* Write a byte, representing an unsigned integer to a file. */

static Boln writeUByte (tkimg_MFile *handle, UByte b)
{
    UByte buf[1];
    buf[0] = b;
    if (1 != tkimg_Write(handle, (const char *)buf, 1))
        return FALSE;
    return TRUE;
}

/* Write a byte, representing a signed integer to a file. */

static Boln writeByte(tkimg_MFile *handle, Byte b)
{
    Byte buf[1];
    buf[0] = b;
    if (1 != tkimg_Write(handle, buf, 1))
        return FALSE;
    return TRUE;
}

/* Convert a short integer number into the format <LowByte, HighByte> (an array
   of 2 bytes) and write the array to a file. */

static Boln writeShort (tkimg_MFile *handle, Short s)
{
    Byte buf[2];
    buf[0] = s;
    buf[1] = s >> 8;
    if (2 != tkimg_Write(handle, buf, 2))
        return FALSE;
    return TRUE;
}

#define OUT Tcl_WriteChars (outChan, str, -1)
static void printImgInfo (TGAHEADER *th, const char *filename, const char *msg)
{
    Tcl_Channel outChan;
    char str[256];

    outChan = Tcl_GetStdChannel (TCL_STDOUT);
    if (!outChan) {
        return;
    }

    sprintf(str, "%s %s\n", msg, filename);                                   OUT;
    sprintf(str, "\tSize in pixel      : %d x %d\n", th->xsize, th->ysize);   OUT;
    sprintf(str, "\tNo. of channels    : %d\n", NCHAN(th->pixsize));          OUT;
    sprintf(str, "\tCompression        : %s\n",
		IS_COMPRESSED(th->imgtyp)? "RLE": "None");                     OUT;
    sprintf(str, "\tVertical encoding  : %s\n",
		ENC_TOP_BOTTOM(th->imgdes)? "Top -> Bottom": "Bottom -> Top"); OUT;
    sprintf(str, "\tHorizontal encoding: %s\n",
		ENC_LEFT_RIGHT(th->imgdes)? "Left -> Right": "Right -> Left"); OUT;
    Tcl_Flush(outChan);
}
#undef OUT
static Boln readHeader (tkimg_MFile *handle, TGAHEADER *th)
{
    Int i;
    UByte dummy;

    if (!readUByte (handle, &th->numid) ||
	!readUByte (handle, &th->maptyp) ||
	!readUByte (handle, &th->imgtyp) ||
	!readShort (handle, &th->maporig) ||
	!readShort (handle, &th->mapsize) ||
	!readUByte (handle, &th->mapbits) ||
	!readShort (handle, &th->xorig) ||
	!readShort (handle, &th->yorig) ||
	!readShort (handle, &th->xsize) ||
	!readShort (handle, &th->ysize) ||
	!readUByte (handle, &th->pixsize) ||
	!readUByte (handle, &th->imgdes))
	return FALSE;

    /* Try to find out if this file can possibly be a TARGA pixel file. */
    if (!((th->imgtyp == TGA_RGB_UNCOMP || th->imgtyp == TGA_RGB_COMP) &&
	  (th->pixsize == 24 || th->pixsize == 32))) {
	return FALSE;
    }

    for ( i=0; i<th->numid; i++) {
	if (!readUByte (handle, &dummy))
	    return FALSE;
    }

    if (th->xsize < 1 || th->ysize < 1) {
	return FALSE;
    }

    /* Skip colormap data, if present. */
    if (th->mapsize > 0)
    {	Int   mapbytes;
	switch (th->mapbits)
	{
	    case 15:
	    case 16:
		mapbytes = 2 * th->mapsize;
		break;
	    case 24:
		mapbytes = 3 * th->mapsize;
		break;
	    case 32:
		mapbytes = 4 * th->mapsize;
		break;
	    default:
		return FALSE;
	}
	while (mapbytes--)
	    if (!readUByte (handle, &dummy))
		return FALSE;
    }
    return TRUE;
}

static Boln writeHeader(tkimg_MFile *handle, TGAHEADER *th)
{
    if (!writeUByte (handle, th->numid) ||
	!writeUByte (handle, th->maptyp) ||
	!writeUByte (handle, th->imgtyp) ||
	!writeShort (handle, th->maporig) ||
	!writeShort (handle, th->mapsize) ||
	!writeUByte (handle, th->mapbits) ||
	!writeShort (handle, th->xorig) ||
	!writeShort (handle, th->yorig) ||
	!writeShort (handle, th->xsize) ||
	!writeShort (handle, th->ysize) ||
	!writeUByte (handle, th->pixsize) ||
	!writeUByte (handle, th->imgdes))
	return FALSE;
    return TRUE;
}

/* A pixel is represented by 3 or 4 bytes in the order Blue/Green/Red/Alpha.
   We are converting the order into standard RGBA order.
   Note that TARGA allows pixel values to be compressed across scanline
   boundaries.
*/

/* Read the value of a pixel from "handle" and assume it must be repeated "n"
   times.
*/

static Boln readRlePixel (Tcl_Interp *interp, tkimg_MFile *handle, UByte **pixBufPtr,
                          Int *countPtr, Int stop, Int n, TGAFILE *tf)
{
    Int i, count, nchan;
    UByte localBuf[4];

    nchan = NCHAN(tf->th.pixsize);
    if (nchan != tkimg_Read(handle, (char *)localBuf, nchan))
	return readError (interp);
    count = *countPtr;
    for (i=0; i<n; i++)
    {
#ifdef DEBUG_LOCAL
	    tf->total++;
#endif
	(*pixBufPtr)[0] = localBuf[2];
	(*pixBufPtr)[1] = localBuf[1];
	(*pixBufPtr)[2] = localBuf[0];
	if (nchan == 4)
	    (*pixBufPtr)[3] = localBuf[3];
	(*pixBufPtr) += nchan;
	count++;

	if (count == stop)
	{   /* Scanline is filled with pixel values.
	       Determine the number of pixels to keep for next scanline. */
	    tf->scanrest = n - i - 1;
	    *countPtr = count;
	    return TRUE;
	}
    }
    *countPtr = count;
    return TRUE;
}

/* The channels of scan line number "y" are read. */

static Boln tgaReadScan (Tcl_Interp *interp, tkimg_MFile *handle,
                         TGAFILE *tf, Int y)
{
    Int   nchan;
    Int   count, stop;
    UByte localBuf[4];
    UByte *pixBufPtr;

    count = 0;
    stop  = tf->th.xsize;
    nchan = NCHAN(tf->th.pixsize);
    pixBufPtr = tf->pixbuf;

#ifdef DEBUG_LOCAL
	tf->total = 0;
#endif

    if (IS_COMPRESSED (tf->th.imgtyp)) {
	Byte cbuf[1];
	Int  pix, numpix;
	/* While there are pixels left from the previous scanline,
	   either fill the current scanline with the pixel value
	   still stored in "pixbuf" (TGA_MODE_SAME) or read in the
	   appropriate number of pixel values (TGA_MODE_DIFF). */
	while (tf->scanrest) {
	    if (tf->scanmode == TGA_MODE_DIFF) {
		if (nchan != tkimg_Read(handle, (char *)localBuf, nchan))
		    return readError (interp);
	    }
#ifdef DEBUG_LOCAL
		tf->total++;
#endif
	    *pixBufPtr++ = localBuf[2];
	    *pixBufPtr++ = localBuf[1];
	    *pixBufPtr++ = localBuf[0];
	    if (nchan == 4)
		*pixBufPtr++ = localBuf[3];
	    count++;

	    tf->scanrest--;
	    /* If the image is small, the compression might go over several
	       scanlines. */
	    if (count == stop)
		return TRUE;
	}

	/* Read the byte telling us the compression mode and the compression
	   count. Then read the pixel values till a scanline is filled. */
	do {
	    if (1 != tkimg_Read(handle, cbuf, 1))
		return readError (interp);
	    numpix = (cbuf[0] & 0x7F) + 1;

	    if ((cbuf[0] & 0x80) != 0x80) {
		tf->scanmode = TGA_MODE_DIFF;
		for (pix=0; pix<numpix; pix++) {
		    if (!readRlePixel (interp, handle, &pixBufPtr,
				       &count, stop, 1, tf))
			return FALSE;
		    if (count == stop) {
			tf->scanrest = numpix - pix - 1;
			break;
		    }
		}
	    } else {
		tf->scanmode = TGA_MODE_SAME;
		if (!readRlePixel (interp, handle, &pixBufPtr,
				   &count, stop, numpix, tf))
		    return FALSE;
	    }
	} while (count < stop);

#ifdef DEBUG_LOCAL
	    printf("\tScanline %d: Pixels: %d Rest: %d\n",
		    y, tf->total, tf->scanrest);
#endif
    } else {
	/* Read uncompressed pixel data. */
	Int   i, bytesPerLine;
	UByte curPix;

	bytesPerLine = nchan * tf->th.xsize;
	if (bytesPerLine != tkimg_Read(handle, (char *)tf->pixbuf, bytesPerLine))
	    return readError (interp);

	for (i=0; i<stop; i++) {
	    curPix = pixBufPtr[2];
	    pixBufPtr[2] = pixBufPtr[0];
	    pixBufPtr[0] = curPix;
	    pixBufPtr += nchan;
	}
    }
    return TRUE;
}

static Boln writePixel(tkimg_MFile *handle, UByte b, UByte g,
			UByte r, UByte m, Int nchan)
{
    UByte buf[4];
    buf[0] = b;
    buf[1] = g;
    buf[2] = r;
    buf[3] = m;
    if (nchan != tkimg_Write(handle, (const char *)buf, nchan))
	return FALSE;
    return TRUE;
}

static Boln tgaWriteScan(Tcl_Interp *interp, tkimg_MFile *handle,
			  TGAFILE *tf, Int y)
{
    UByte *stop, *red_end, *green_end, *blue_end, *matte_end;
    Int nchan;

    tf->red = tf->redScan;
    tf->green = tf->greenScan;
    tf->blue = tf->blueScan;
    tf->matte = tf->matteScan;
    stop = tf->red + tf->th.xsize;
    nchan = NCHAN(tf->th.pixsize);

    /* Write the scanline data to the file. */
    if (! IS_COMPRESSED(tf->th.imgtyp)) {
	while (tf->red < stop)
	{
	    if (!writePixel(handle, *tf->blue, *tf->green, *tf->red, *tf->matte, nchan))
		return FALSE;
	    tf->blue++;
	    tf->green++;
	    tf->red++;
	    tf->matte++;
	}
    }
    else	/* Run-length Compression */
    {
	red_end = tf->red + 1;
	green_end = tf->green + 1;
	blue_end = tf->blue + 1;
	matte_end = tf->matte + 1;
	while (tf->red < stop)
	{
	    while (red_end < stop &&
		   *tf->red == *red_end &&
		   *tf->green == *green_end &&
		   *tf->blue == *blue_end &&
		   red_end - tf->red - 1 < MAXRUN)
	    {
		if (nchan == 4)
		{
		    if (*tf->matte != *matte_end)
			break;
		}
		red_end++;
		green_end++;
		blue_end++;
		matte_end++;
	    }
	    if (red_end - tf->red >= MINRUN)
	    {	/* Found a run of compressable data */
		if (!writeByte(handle, (Byte)(((red_end - tf->red)-1)|0x80)) ||
		    !writePixel(handle, *tf->blue, *tf->green, *tf->red, *tf->matte, nchan))
		    return FALSE;
		tf->red = red_end;
		tf->green = green_end;
		tf->blue = blue_end;
		tf->matte = matte_end;
	    }
	    else
	    {	/* Found a run of uncompressable data */
		while (red_end < stop &&
		       ((red_end + 1 >= stop ||
			*red_end != *(red_end + 1)) ||
			(red_end + 2 >= stop ||
			*(red_end + 1) != *(red_end + 2))) &&
		       ((green_end + 1 >= stop ||
			*green_end != *(green_end + 1)) ||
			(green_end + 2 >= stop ||
			*(green_end + 1) != *(green_end + 2))) &&
		       ((blue_end + 1 >= stop ||
			*blue_end != *(blue_end + 1)) ||
			(blue_end + 2 >= stop ||
			*(blue_end + 1) != *(blue_end + 2))) &&
			red_end - tf->red < MAXRUN)
		{
		    if (nchan == 4)
		    {
		        if (! ((matte_end + 1 >= stop ||
			       *matte_end != *(matte_end + 1)) ||
			       (matte_end + 2 >= stop ||
			       *(matte_end + 1) != *(matte_end + 2))))
			    break;
		    }
		    red_end++;
		    green_end++;
		    blue_end++;
		    matte_end++;
		}
		if (!writeByte(handle, (Byte)((red_end - tf->red) - 1)))
		    return FALSE;
		while (tf->red < red_end)
		{
		    if (!writePixel(handle, *tf->blue, *tf->green, *tf->red, *tf->matte, nchan))
			return FALSE;
		    tf->red++;
		    tf->green++;
		    tf->blue++;
		    tf->matte++;
		}
	    }
	    red_end++;
	    green_end++;
	    blue_end++;
	    matte_end++;
	}
    }
    return TRUE;
}

/*
 * Here is the start of the standard functions needed for every image format.
 */

/*
 * Prototypes for local procedures defined in this file:
 */

static int ParseFormatOpts(Tcl_Interp *interp, Tcl_Obj *format,
	int *comp, int *verb, int *matte);
static int CommonMatch(tkimg_MFile *handle, int *widthPtr,
	int *heightPtr, TGAHEADER *tgaHeaderPtr);
static int CommonRead(Tcl_Interp *interp, tkimg_MFile *handle,
	const char *filename, Tcl_Obj *format,
	Tk_PhotoHandle imageHandle, int destX, int destY,
	int width, int height, int srcX, int srcY);
static int CommonWrite(Tcl_Interp *interp,
	const char *filename, Tcl_Obj *format,
	tkimg_MFile *handle, Tk_PhotoImageBlock *blockPtr);

static int ParseFormatOpts(interp, format, comp, verb, matte)
    Tcl_Interp *interp;
    Tcl_Obj *format;
    int *comp;
    int *verb;
    int *matte;
{
    static const char *const tgaOptions[] = {"-compression", "-verbose", "-matte"};
    int objc, length, c, i, index;
    Tcl_Obj **objv;
    const char *compression, *verbose, *transp;

    *comp = TGA_RGB_COMP;
    *verb = 0;
    *matte = 1;
    if (tkimg_ListObjGetElements(interp, format, &objc, &objv) != TCL_OK)
	return TCL_ERROR;
    if (objc) {
	compression = "rle";
	verbose     = "0";
	transp      = "1";
	for (i=1; i<objc; i++) {
	    if (Tcl_GetIndexFromObj(interp, objv[i], (CONST84 char *CONST86 *)tgaOptions,
		    "format option", 0, &index) != TCL_OK) {
		return TCL_ERROR;
	    }
	    if (++i >= objc) {
		Tcl_AppendResult(interp, "No value for option \"",
			Tcl_GetStringFromObj (objv[--i], (int *) NULL),
			"\"", (char *) NULL);
		return TCL_ERROR;
	    }
	    switch(index) {
		case 0:
		    compression = Tcl_GetStringFromObj(objv[i], (int *) NULL);
		    break;
		case 1:
		    verbose = Tcl_GetStringFromObj(objv[i], (int *) NULL);
		    break;
		case 2:
		    transp = Tcl_GetStringFromObj(objv[i], (int *) NULL);
		    break;
	    }
	}

	c = compression[0]; length = strlen (compression);
	if ((c == 'n') && (!strncmp (compression, "none", length))) {
	    *comp = TGA_RGB_UNCOMP;
	} else if ((c == 'r') && (!strncmp (compression, "rle",length))) {
	    *comp = TGA_RGB_COMP;
	} else {
	    Tcl_AppendResult(interp, "invalid compression mode \"",
		    compression, "\": should be rle or none", (char *) NULL);
	    return TCL_ERROR;
	}

	c = verbose[0]; length = strlen (verbose);
	if (!strncmp (verbose, "1", length) || \
	    !strncmp (verbose, "true", length) || \
	    !strncmp (verbose, "on", length)) {
	    *verb = 1;
	} else if (!strncmp (verbose, "0", length) || \
	    !strncmp (verbose, "false", length) || \
	    !strncmp (verbose, "off", length)) {
	    *verb = 0;
	} else {
	    Tcl_AppendResult(interp, "invalid verbose mode \"", verbose,
                              "\": should be 1 or 0, on or off, true or false",
			      (char *) NULL);
	    return TCL_ERROR;
	}

        c = transp[0]; length = strlen (transp);
        if (!strncmp (transp, "1", length) || \
            !strncmp (transp, "true", length) || \
            !strncmp (transp, "on", length)) {
            *matte = 1;
        } else if (!strncmp (transp, "0", length) || \
            !strncmp (transp, "false", length) || \
            !strncmp (transp, "off", length)) {
            *matte = 0;
        } else {
            Tcl_AppendResult(interp, "invalid alpha (matte) mode \"", verbose,
                              "\": should be 1 or 0, on or off, true or false",
                              (char *) NULL);
            return TCL_ERROR;
        }
    }
    return TCL_OK;
}

static int ChnMatch(
    Tcl_Channel chan,
    const char *filename,
    Tcl_Obj *format,
    int *widthPtr,
    int *heightPtr,
    Tcl_Interp *interp
) {
    tkimg_MFile handle;

    handle.data = (char *) chan;
    handle.state = IMG_CHAN;

    return CommonMatch(&handle, widthPtr, heightPtr, NULL);
}

static int ObjMatch(
    Tcl_Obj *data,
    Tcl_Obj *format,
    int *widthPtr,
    int *heightPtr,
    Tcl_Interp *interp
) {
    tkimg_MFile handle;

    if (!tkimg_ReadInit (data, 0, &handle)) {
        tkimg_ReadInit (data, '*', &handle);
    }
    return CommonMatch(&handle, widthPtr, heightPtr, NULL);
}

static int CommonMatch(handle, widthPtr, heightPtr, tgaHeaderPtr)
    tkimg_MFile *handle;
    int *widthPtr;
    int *heightPtr;
    TGAHEADER *tgaHeaderPtr;
{
    TGAHEADER th;

    if (!readHeader (handle, &th))
	return 0;

    *widthPtr  = th.xsize;
    *heightPtr = th.ysize;
    if (tgaHeaderPtr)
	*tgaHeaderPtr = th;
    return 1;
}

static int ChnRead(interp, chan, filename, format, imageHandle,
                    destX, destY, width, height, srcX, srcY)
    Tcl_Interp *interp;         /* Interpreter to use for reporting errors. */
    Tcl_Channel chan;           /* The image channel, open for reading. */
    const char *filename;       /* The name of the image file. */
    Tcl_Obj *format;            /* User-specified format object, or NULL. */
    Tk_PhotoHandle imageHandle; /* The photo image to write into. */
    int destX, destY;           /* Coordinates of top-left pixel in
				 * photo image to be written to. */
    int width, height;          /* Dimensions of block of photo image to
				 * be written to. */
    int srcX, srcY;             /* Coordinates of top-left pixel to be used
			         * in image being read. */
{
    tkimg_MFile handle;

    handle.data = (char *) chan;
    handle.state = IMG_CHAN;

    return CommonRead(interp, &handle, filename, format, imageHandle,
                       destX, destY, width, height, srcX, srcY);
}

static int ObjRead(interp, data, format, imageHandle,
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

    if (!tkimg_ReadInit(data, 0, &handle)) {
        tkimg_ReadInit(data, '*', &handle);
    }
    return CommonRead(interp, &handle, "InlineData", format, imageHandle,
                       destX, destY, width, height, srcX, srcY);
}

static int CommonRead(interp, handle, filename, format, imageHandle,
                       destX, destY, width, height, srcX, srcY)
    Tcl_Interp *interp;         /* Interpreter to use for reporting errors. */
    tkimg_MFile *handle;        /* The image file, open for reading. */
    const char *filename;       /* The name of the image file. */
    Tcl_Obj *format;            /* User-specified format object, or NULL. */
    Tk_PhotoHandle imageHandle; /* The photo image to write into. */
    int destX, destY;           /* Coordinates of top-left pixel in
				 * photo image to be written to. */
    int width, height;          /* Dimensions of block of photo image to
			         * be written to. */
    int srcX, srcY;             /* Coordinates of top-left pixel to be used
			         * in image being read. */
{
	Tk_PhotoImageBlock block;
    Int y, nchan;
    int fileWidth, fileHeight;
    int stopY, outY, outWidth, outHeight;
    TGAFILE tf;
    int compr, verbose, matte;
    char errMsg[200];
    int result = TCL_OK;

    memset (&tf, 0, sizeof (TGAFILE));
    if (ParseFormatOpts (interp, format, &compr, &verbose, &matte) != TCL_OK) {
	return TCL_ERROR;
    }

    if (!CommonMatch(handle, &fileWidth, &fileHeight, &tf.th))
	return TCL_ERROR;
    if (verbose)
	printImgInfo (&tf.th, filename, "Reading image:");

    if ((srcX + width) > fileWidth) {
	outWidth = fileWidth - srcX;
    } else {
	outWidth = width;
    }
    if ((srcY + height) > fileHeight) {
	outHeight = fileHeight - srcY;
    } else {
	outHeight = height;
    }
    if ((outWidth <= 0) || (outHeight <= 0)
	|| (srcX >= fileWidth) || (srcY >= fileHeight)) {
	return TCL_OK;
    }

    if (tkimg_PhotoExpand(interp, imageHandle, destX + outWidth, destY + outHeight) == TCL_ERROR) {
	return TCL_ERROR;
    }

    if (IS_COMPRESSED(tf.th.imgtyp)) {
	tkimg_ReadBuffer(1);
    }

    tf.scanmode = TGA_MODE_DIFF;
    nchan = NCHAN(tf.th.pixsize);

    tf.pixbuf = (UByte *) ckalloc (fileWidth * nchan);
    if (!tf.pixbuf) {
	sprintf(errMsg, "Can't allocate memory of size %d", fileWidth * nchan);
	Tcl_AppendResult(interp, errMsg, (char *)NULL);
	tkimg_ReadBuffer (0);
	return TCL_ERROR;
    }

    block.pixelSize = nchan;
    block.pitch = fileWidth * nchan;
    block.width = outWidth;
    block.height = 1;
    block.offset[0] = 0;
    block.offset[1] = 1;
    block.offset[2] = 2;
    if (nchan < 4) {
	matte = 0;
    }
    block.offset[3] = matte? 3: 0;
    block.pixelPtr = tf.pixbuf + srcX * nchan;

    stopY = srcY + outHeight;

    if (ENC_TOP_BOTTOM (tf.th.imgdes)) {
	outY = destY;
	for (y=0; y<stopY; y++) {
	    tgaReadScan(interp, handle, &tf, y);
	    if (y >= srcY) {
		if (tkimg_PhotoPutBlock(interp, imageHandle, &block, destX, outY, width, 1, matte? TK_PHOTO_COMPOSITE_OVERLAY: TK_PHOTO_COMPOSITE_SET) == TCL_ERROR) {
		    result = TCL_ERROR;
		    break;
		}
		outY++;
	    }
	}
    } else {
	outY = destY + outHeight - 1;
	for (y=fileHeight-1; y>=0; y--) {
	    tgaReadScan(interp, handle, &tf, y);
	    if (y >= srcY && y < stopY) {
		if (tkimg_PhotoPutBlock(interp, imageHandle, &block, destX, outY, width, 1, TK_PHOTO_COMPOSITE_SET) == TCL_ERROR) {
		    result = TCL_ERROR;
		    break;
		}
		outY--;
	    }
	}
    }
    tgaClose(&tf);
    tkimg_ReadBuffer(0);
    return result;
}

static int ChnWrite(interp, filename, format, blockPtr)
    Tcl_Interp *interp;
    const char *filename;
    Tcl_Obj *format;
    Tk_PhotoImageBlock *blockPtr;
{
    Tcl_Channel chan;
    tkimg_MFile handle;
    int result;

    chan = tkimg_OpenFileChannel(interp, filename, 0644);
    if (!chan) {
	return TCL_ERROR;
    }

    handle.data = (char *) chan;
    handle.state = IMG_CHAN;

    result = CommonWrite(interp, filename, format, &handle, blockPtr);
    if (Tcl_Close(interp, chan) == TCL_ERROR) {
	return TCL_ERROR;
    }
    return result;
}

static int StringWrite(
    Tcl_Interp *interp,
    Tcl_Obj *format,
    Tk_PhotoImageBlock *blockPtr
) {
    tkimg_MFile handle;
    int result;
    Tcl_DString data;

    Tcl_DStringInit(&data);
    tkimg_WriteInit(&data, &handle);
    result = CommonWrite(interp, "InlineData", format, &handle, blockPtr);
    tkimg_Putc(IMG_DONE, &handle);

    if (result == TCL_OK) {
	Tcl_DStringResult(interp, &data);
    } else {
	Tcl_DStringFree(&data);
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
    Int     x, y, nchan;
    Int     redOffset, greenOffset, blueOffset, alphaOffset;
    UByte   *pixelPtr, *rowPixPtr;
    TGAFILE tf;
    int compr, verbose, matte; /* Format options */
    char errMsg[200];

    memset(&tf, 0, sizeof(TGAFILE));
    if (ParseFormatOpts(interp, format, &compr, &verbose, &matte) != TCL_OK) {
	return TCL_ERROR;
    }

    redOffset   = 0;
    greenOffset = blockPtr->offset[1] - blockPtr->offset[0];
    blueOffset  = blockPtr->offset[2] - blockPtr->offset[0];
    alphaOffset = blockPtr->offset[0];

    if (alphaOffset < blockPtr->offset[2]) {
        alphaOffset = blockPtr->offset[2];
    }
    if (++alphaOffset < blockPtr->pixelSize) {
        alphaOffset -= blockPtr->offset[0];
    } else {
        alphaOffset = 0;
    }

    nchan = ((matte && alphaOffset)? 4: 3);

    tf.redScan   = (UByte *) ckalloc(blockPtr->width);
    tf.greenScan = (UByte *) ckalloc(blockPtr->width);
    tf.blueScan  = (UByte *) ckalloc(blockPtr->width);
    tf.matteScan = (UByte *) ckalloc(blockPtr->width);
    if (!tf.redScan || !tf.greenScan || !tf.blueScan || !tf.matteScan) {
	sprintf(errMsg, "Can't allocate memory of size %d", blockPtr->width);
	Tcl_AppendResult(interp, errMsg, (char *)NULL);
	return TCL_ERROR;
    }

    /* Fill the targa header struct and write the header to the channel. */
    tf.th.pixsize = nchan * 8;
    tf.th.xsize = blockPtr->width;
    tf.th.ysize = blockPtr->height;
    tf.th.imgdes = (1 << 5);    	/* Top->Bottom, Left->Right encoding */
    tf.th.imgtyp = compr;		/* Uncompressed or RLE-compressed */

    if (!writeHeader(handle, &tf.th)) {
	return TCL_ERROR;
    }

    rowPixPtr = blockPtr->pixelPtr + blockPtr->offset[0];
    for (y = 0; y < blockPtr->height; y++) {
	tf.red = tf.redScan;
	tf.green = tf.greenScan;
	tf.blue = tf.blueScan;
	tf.matte = tf.matteScan;
	pixelPtr = rowPixPtr;
	for (x = 0; x < blockPtr->width; x++) {
	    *(tf.red++)   = pixelPtr[redOffset];
	    *(tf.green++) = pixelPtr[greenOffset];
	    *(tf.blue++)  = pixelPtr[blueOffset];
	    if (nchan == 4) {
		/* Have a matte channel and write it. */
		*(tf.matte++) = pixelPtr[alphaOffset];
	    }
	    pixelPtr += blockPtr->pixelSize;
	}
	if (!tgaWriteScan(interp, handle, &tf, y)) {
	    tgaClose (&tf);
	    return TCL_ERROR;
	}
	rowPixPtr += blockPtr->pitch;
    }
    if (verbose)
        printImgInfo(&tf.th, filename, "Saving image:");
    tgaClose (&tf);
    return TCL_OK;
}
