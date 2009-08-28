/* STARTHEADER
 *
 * File :       pcx.c
 *
 * Author :     Paul Obermeier (paul@poSoft.de)
 *
 * Date :       Die Feb 20 14:27:18 CET 2001
 *
 * Copyright :  (C) 2001-2002 Paul Obermeier
 *
 * Description :
 *
 * A photo image handler for PaintBrush's PCX file format.
 *
 * The following image types are supported:
 *
 *  1-bit pixels: Black and White.
 *  8-bit pixels: Grayscale or indexed.
 * 24-bit pixels: True-color (RGB, each channel 8 bit).
 *
 * List of currently supported features:
 *
 * Type   |     Read      |     Write     |
 *        | -file | -data | -file | -data |
 * ----------------------------------------
 *  1-bit | Yes   | Yes   | No    | No    |
 *  8-bit | Yes   | Yes   | No    | No    |
 * 24-bit | Yes   | Yes   | Yes   | Yes   |
 *
 * All images types may be either uncompressed or run-length encoded.
 *
 *
 * The following format options are available:
 *
 * Read  PCX image: "pcx -verbose <bool>"
 * Write PCX image: "pcx -verbose <bool> -compression <type>"
 *
 * -verbose <bool>:     If set to true, additional information about the file
 *                      format is printed to stdout. Default is "false".
 * -compression <type>: Set the compression mode to either "none" or "rle".
 *			Default is "rle".
 *
 * Notes:
 *
 * - Part of this code was taken from the "pcx" GIMP plugin:
 *
 *  >> pcx.c GIMP plug-in for loading & saving PCX files
 *  >>
 *  >> This code is based in parts on code by Francisco Bustamante, but the
 *  >> largest portion of the code has been rewritten and is now maintained
 *  >> occasionally by Nick Lamb njl195@zepler.org.uk
 *
 * ENDHEADER
 *
 * $Id: pcx.c 152 2008-10-22 08:49:40Z nijtmans $
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
typedef unsigned short UShort;	/* Unsigned 16 bit integer */
typedef short Short;		/* Signed   16 bit integer */
typedef int Int;		/* Signed   32 bit integer */

typedef struct {
  UByte manufacturer;
  UByte version;
  UByte compression;
  UByte bpp;
  Short x1;
  Short y1;
  Short x2;
  Short y2;
  Short hdpi;
  Short vdpi;
  UByte colormap[48];
  UByte reserved;
  UByte planes;
  Short bytesperline;
  Short color;
  UByte filler[58];
} PCXHEADER;

/* OPA TODO: Change from ANSI-C arguments to _ANSI_ARGS_ macro. */

/* This function determines at runtime, whether we have to swap bytes.
   The PCX image format expects data to be in Intel (Little-endian) format. */

static int isIntel (void)
{
    char order[] = { 1, 2, 3, 4};
    unsigned long val = (unsigned long)*((short *)order);
    /* On Intel (little-endian) systems this value is equal to 513.
       On big-endian systems this value equals 258. */
    return (val == 513);
}

#define htoqs(x) qtohs(x)
static UShort qtohs (UShort x)
{
    if (!isIntel ()) {
	return ((UShort)((((UShort)(x) & 0x00ff) << 8) | \
			 (((UShort)(x) & 0xff00) >> 8)));
    } else {
	return x;
    }
}

/* Read 1 byte, representing an unsigned integer number. */

#if defined (DEBUG_LOCAL)
static Boln readUByte (tkimg_MFile *handle, UByte *b)
{
    char buf[1];
    if (1 != tkimg_Read(handle, (char *) buf, 1))
        return FALSE;
    *b = buf[0];
    return TRUE;
}
#else
    /* Use this macro for better performance, esp. when reading RLE files. */
    #define readUByte(h,b) (1 == tkimg_Read((h),(char *)(b),1))
#endif

/* Write 1 byte, representing an unsigned integer to a file. */

static Boln writeUByte (tkimg_MFile *handle, UByte b)
{
    UByte buf[1];
    buf[0] = b;
    if (1 != tkimg_Write(handle, (const char *)buf, 1))
        return FALSE;
    return TRUE;
}

static Boln read_pcx_header (tkimg_MFile *ifp, PCXHEADER *pcxhdr)
{
    if (tkimg_Read(ifp, (char *)pcxhdr, 128) != 128) {
	return FALSE;
    }

    if (pcxhdr->manufacturer != 10) {
	return FALSE;
    }
    if (pcxhdr->bpp != 1 && pcxhdr->bpp != 8) {
        return FALSE;
    }
    if (pcxhdr->planes != 1 && pcxhdr->planes != 3 && pcxhdr->planes != 4) {
        return FALSE;
    }
    return TRUE;
}

#define OUT Tcl_WriteChars (outChan, str, -1)
static void printImgInfo (PCXHEADER *ph, const char *filename, const char *msg)
{
    Tcl_Channel outChan;
    char str[256];
    Int width, height;

    outChan = Tcl_GetStdChannel (TCL_STDOUT);
    if (!outChan) {
        return;
    }
    width  = qtohs (ph->x2) - qtohs (ph->x1) + 1;
    height = qtohs (ph->y2) - qtohs (ph->y1) + 1;

    sprintf(str, "%s %s\n", msg, filename);                                 OUT;
    sprintf(str, "\tSize in pixel   : %d x %d\n", width, height);           OUT;
    sprintf(str, "\tNo. of channels : %d\n", ph->planes);                   OUT;
    sprintf(str, "\tBytes per pixel : %d\n", ph->bpp);                      OUT;
    sprintf(str, "\tRLE compression : %s\n", ph->compression? "yes": "no"); OUT;
    Tcl_Flush(outChan);
}
#undef OUT

static Boln readline (tkimg_MFile *handle, UByte *buffer, Int bytes, Int compr)
{
    static UByte count = 0, value = 0;

    if (compr) {
	while (bytes--) {
	    if (count == 0) {
	        if (!readUByte (handle, &value)) {
		    return FALSE;
		}
	        if (value < 0xc0) {
		    count = 1;
		} else {
		    count = value - 0xc0;
		    if (!readUByte (handle, &value)) {
			return FALSE;
		    }
		}
	    }
	    count--;
	    *(buffer++) = value;
	}
    } else {
	if (bytes != tkimg_Read(handle, (char *)buffer, bytes)) {
	    return FALSE;
	}
    }
    return TRUE;
}

static Boln writeline (tkimg_MFile *handle, UByte *buffer, Int bytes)
{
    UByte value, count;
    UByte *finish = buffer + bytes;

    while (buffer < finish) {
        value = *(buffer++);
        count = 1;

        while (buffer < finish && count < 63 && *buffer == value) {
	    count++;
            buffer++;
	}

        if (value < 0xc0 && count == 1) {
	    if (!writeUByte (handle, value)) {
		return FALSE;
	    }
	} else {
	    if (!writeUByte (handle, 0xc0 + count)) {
		return FALSE;
	    }
	    if (!writeUByte (handle, value)) {
		return FALSE;
	    }
	}
    }
    return TRUE;
}

typedef struct myblock {
    Tk_PhotoImageBlock ck;
    int dummy; /* extra space for offset[3], in case it is not
                  included already in Tk_PhotoImageBlock */
} myblock;
#define block bl.ck

static Boln load_8 (Tcl_Interp *interp, tkimg_MFile *ifp,
                    Tk_PhotoHandle imageHandle, int destX, int destY,
                    int width, int height, int srcX, int srcY,
                    int fileWidth, int fileHeight, int bytesPerLine, int compr)
{
    Int x, y;
    Int stopY, outY;
    myblock bl;
    UByte *line, *buffer, *indBuf, *indBufPtr;
    UByte cmap[768], sepChar;

    line   = (UByte *) ckalloc (fileWidth);
    buffer = (UByte *) ckalloc (fileWidth * 3);
    indBuf = (UByte *) ckalloc (fileWidth * fileHeight);
    indBufPtr = indBuf;

    block.pixelSize = 3;
    block.pitch = fileWidth * 3;
    block.width = width;
    block.height = 1;
    block.offset[0] = 0;
    block.offset[1] = 1;
    block.offset[2] = 2;
    block.offset[3] = 0;

    block.pixelPtr = buffer + srcX * 3;

    stopY = srcY + height;
    outY  = destY;

    /* Read in the whole image data as indices. */
    for (y=0; y<stopY; y++) {
        if (!readline (ifp, line, bytesPerLine, compr)) {
	    ckfree ((char *) line);
	    ckfree ((char *) buffer);
	    ckfree ((char *) indBuf);
	    return FALSE;
	}
        memcpy (indBufPtr, line, fileWidth);
	indBufPtr += fileWidth;
    }
    /* Read the colormap: 256 entries */
    if ((tkimg_Read(ifp, (char *)&sepChar, 1) != 1) ||
        (tkimg_Read(ifp, (char *)&cmap, 768) != 768)) {
	ckfree ((char *) line);
	ckfree ((char *) buffer);
	ckfree ((char *) indBuf);
	return FALSE;
    }

    for (y=srcY; y<stopY; y++) {
        for (x=0; x<fileWidth; x++) {
            buffer[x * 3 + 0] = cmap[indBuf[y*fileWidth + x]*3 + 0 ];
            buffer[x * 3 + 1] = cmap[indBuf[y*fileWidth + x]*3 + 1 ];
            buffer[x * 3 + 2] = cmap[indBuf[y*fileWidth + x]*3 + 2 ];
        }
        tkimg_PhotoPutBlock(interp, imageHandle, &block, destX, outY, width, 1, TK_PHOTO_COMPOSITE_SET);
        outY++;
    }
    ckfree ((char *) line);
    ckfree ((char *) buffer);
    ckfree ((char *) indBuf);
    return TRUE;
}

static Boln load_24 (Tcl_Interp *interp, tkimg_MFile *ifp,
                     Tk_PhotoHandle imageHandle, int destX, int destY,
                     int width, int height, int srcX, int srcY,
                     int fileWidth, int fileHeight, int bytesPerLine, int compr)
{
    Int x, y, c;
    Int stopY, outY;
    myblock bl;
    UByte *line, *buffer;

    line   = (UByte *) ckalloc (bytesPerLine);
    buffer = (UByte *) ckalloc (fileWidth * 3);

    block.pixelSize = 3;
    block.pitch = fileWidth * 3;
    block.width = width;
    block.height = 1;
    block.offset[0] = 0;
    block.offset[1] = 1;
    block.offset[2] = 2;
    block.offset[3] = 0;

    block.pixelPtr = buffer + srcX * 3;

    stopY = srcY + height;
    outY  = destY;

    for (y=0; y<stopY; y++) {
	for (c=0; c<3; c++) {
	    if (!readline (ifp, line, bytesPerLine, compr)) {
		ckfree ((char *) line);
		ckfree ((char *) buffer);
		return FALSE;
	    }
	    for (x=0; x<fileWidth; x++) {
	        buffer[x * 3 + c] = line[x];
	    }
	}
	if (y >= srcY) {
	    tkimg_PhotoPutBlock(interp, imageHandle, &block, destX, outY, width, 1, TK_PHOTO_COMPOSITE_SET);
	    outY++;
	}
    }
    ckfree ((char *) line);
    ckfree ((char *) buffer);
    return TRUE;
}

static Boln load_1 (Tcl_Interp *interp, tkimg_MFile *ifp,
                    Tk_PhotoHandle imageHandle, int destX, int destY,
                    int width, int height, int srcX, int srcY,
                    int fileWidth, int fileHeight, int bytesPerLine, int compr)
{
    Int x, y;
    Int stopY, outY;
    myblock bl;
    UByte *line, *buffer;

    line   = (UByte *) ckalloc (fileWidth);
    buffer = (UByte *) ckalloc (fileWidth * 1);

    block.pixelSize = 1;
    block.pitch = fileWidth * 1;
    block.width = width;
    block.height = 1;
    block.offset[0] = 0;
    block.offset[1] = 0;
    block.offset[2] = 0;
    block.offset[3] = 0;

    block.pixelPtr = buffer + srcX * 1;

    stopY = srcY + height;
    outY  = destY;

    for (y=0; y<stopY; y++) {
        if (!readline (ifp, line, bytesPerLine, compr)) {
	    ckfree ((char *) line);
	    ckfree ((char *) buffer);
            return FALSE;
        }
        for (x=0; x<fileWidth; x++) {
	    if (line[x/8] & (128 >> (x%8))) {
	        buffer[x] = 255;
	    } else {
	        buffer[x] = 0;
	    }
	}
        if (y >= srcY) {
            tkimg_PhotoPutBlock(interp, imageHandle, &block, destX, outY, width, 1, TK_PHOTO_COMPOSITE_SET);
            outY++;
        }
    }
    ckfree ((char *) line);
    ckfree ((char *) buffer);
    return TRUE;
}

/*
 * Prototypes for local procedures defined in this file:
 */

static int   ParseFormatOpts _ANSI_ARGS_((Tcl_Interp *interp, Tcl_Obj *format,
                 int *comp, int *verb, int *matte));
static int   CommonMatch _ANSI_ARGS_((tkimg_MFile *handle, int *widthPtr,
	         int *heightPtr, PCXHEADER *pcxHeaderPtr));
static int   CommonRead _ANSI_ARGS_((Tcl_Interp *interp, tkimg_MFile *handle,
	         const char *filename, Tcl_Obj *format,
	         Tk_PhotoHandle imageHandle, int destX, int destY,
		 int width, int height, int srcX, int srcY));
static int   CommonWrite _ANSI_ARGS_((Tcl_Interp *interp,
                 const char *filename, Tcl_Obj *format,
                 tkimg_MFile *handle, Tk_PhotoImageBlock *blockPtr));

static int ParseFormatOpts (interp, format, comp, verb, matte)
    Tcl_Interp *interp;
    Tcl_Obj *format;
    int *comp;
    int *verb;
    int *matte;
{
    static const char *const pcxOptions[] = {"-compression", "-verbose", "-matte"};
    int objc, length, c, i, index;
    Tcl_Obj **objv;
    CONST84 char *compression, *verbose, *transp;

    *comp = 1;
    *verb = 0;
    *matte = 1;
    if (tkimg_ListObjGetElements(interp, format, &objc, &objv) != TCL_OK)
	return TCL_ERROR;
    if (objc) {
	compression = "rle";
	verbose     = "0";
	transp      = "1";
	for (i=1; i<objc; i++) {
	    if (Tcl_GetIndexFromObj(interp, objv[i], (CONST84 char *CONST86 *)pcxOptions,
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
	    *comp = 0;
	} else if ((c == 'r') && (!strncmp (compression, "rle",length))) {
	    *comp = 1;
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

static int ChnMatch(interp, chan, filename, format, widthPtr, heightPtr)
    Tcl_Interp *interp;
    Tcl_Channel chan;
    const char *filename;
    Tcl_Obj *format;
    int *widthPtr, *heightPtr;
{
    tkimg_MFile handle;

    tkimg_FixChanMatchProc (&interp, &chan, &filename, &format,
                         &widthPtr, &heightPtr);

    handle.data = (char *) chan;
    handle.state = IMG_CHAN;

    return CommonMatch(&handle, widthPtr, heightPtr, NULL);
}

static int ObjMatch(interp, data, format, widthPtr, heightPtr)
    Tcl_Interp *interp;
    Tcl_Obj *data;
    Tcl_Obj *format;
    int *widthPtr, *heightPtr;
{
    tkimg_MFile handle;

    tkimg_FixObjMatchProc (&interp, &data, &format, &widthPtr, &heightPtr);

    if (!tkimg_ReadInit(data, 10, &handle)) {
	return 0;
    }
    return CommonMatch(&handle, widthPtr, heightPtr, NULL);
}

static int CommonMatch(handle, widthPtr, heightPtr, pcxHeaderPtr)
    tkimg_MFile *handle;
    int   *widthPtr;
    int   *heightPtr;
    PCXHEADER *pcxHeaderPtr;
{
    PCXHEADER ph;
    Int offset_x, offset_y;

    if (!read_pcx_header (handle, &ph))
	return 0;

    offset_x = qtohs (ph.x1);
    offset_y = qtohs (ph.y1);

    if (offset_x < 0 || offset_y < 0)
	return 0;

    *widthPtr  = qtohs (ph.x2) - offset_x + 1;
    *heightPtr = qtohs (ph.y2) - offset_y + 1;

    if (*widthPtr < 1 || *heightPtr < 1)
	return 0;

    if (pcxHeaderPtr)
	*pcxHeaderPtr = ph;
    return 1;
}

static int ChnRead(interp, chan, filename, format, imageHandle,
	            destX, destY, width, height, srcX, srcY)
    Tcl_Interp *interp;		/* Interpreter to use for reporting errors. */
    Tcl_Channel chan;		/* The image channel, open for reading. */
    const char *filename;	/* The name of the image file. */
    Tcl_Obj *format;		/* User-specified format object, or NULL. */
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

    return CommonRead (interp, &handle, filename, format,
		       imageHandle, destX, destY,
		       width, height, srcX, srcY);
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

    tkimg_ReadInit (data, 10, &handle);
    return CommonRead (interp, &handle, "InlineData", format, imageHandle,
		       destX, destY, width, height, srcX, srcY);
}

static int CommonRead (interp, handle, filename, format, imageHandle,
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
    int nchan;
    int fileWidth, fileHeight;
    int outWidth, outHeight;
    int retCode = TCL_OK;
    PCXHEADER ph;
    int compr, verbose, matte;
    char errMsg[200];

    if (ParseFormatOpts(interp, format, &compr, &verbose, &matte) != TCL_OK) {
        return TCL_ERROR;
    }

    CommonMatch(handle, &fileWidth, &fileHeight, &ph);
    if (verbose)
        printImgInfo (&ph, filename, "Reading image:");

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

    if (ph.compression)
	tkimg_ReadBuffer (1);

    Tk_PhotoExpand(imageHandle, destX + outWidth, destY + outHeight);

    nchan = ph.planes;

    if (ph.planes == 1 && ph.bpp == 1) {
        if (!load_1 (interp, handle, imageHandle, destX, destY,
                     outWidth, outHeight, srcX, srcY, fileWidth, fileHeight,
                     qtohs (ph.bytesperline), ph.compression))
	    retCode = TCL_ERROR;
    } else if (ph.planes == 4 && ph.bpp == 1) {
	Tcl_AppendResult(interp, "Format (4 channels, 1 bit per channel) ",
                          "is not supported yet.", (char *)NULL);
	retCode = TCL_ERROR;
    } else if (ph.planes == 1 && ph.bpp == 8) {
        if (!load_8 (interp, handle, imageHandle, destX, destY,
                     outWidth, outHeight, srcX, srcY, fileWidth, fileHeight,
                     qtohs (ph.bytesperline), ph.compression))
	    retCode = TCL_ERROR;
    } else if (ph.planes == 3 && ph.bpp == 8) {
        if (!load_24 (interp, handle, imageHandle, destX, destY,
                      outWidth, outHeight, srcX, srcY, fileWidth, fileHeight,
                      qtohs (ph.bytesperline), ph.compression))
	    retCode = TCL_ERROR;
    } else {
	sprintf(errMsg, "Image has invalid channel/bpp combination: (%d, %d)",
			  ph.planes, ph.bpp);
	Tcl_AppendResult(interp, errMsg, (char *)NULL);
	retCode = TCL_ERROR;
    }
    tkimg_ReadBuffer (0);
    return retCode;
}

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

    tkimg_WriteInit(dataPtr, &handle);
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
    int     x, y, nchan, nBytes;
    int     redOffset, greenOffset, blueOffset, alphaOffset;
    UByte   *pixelPtr, *pixRowPtr;
    PCXHEADER ph;
    UByte *row;
    int compr, verbose, matte; /* Format options */
    char errMsg[200];

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

    nchan   = 3;
    nBytes  = blockPtr->width * nchan;

    /* Fill the PCX header struct and write the header to the channel. */
    memset (&ph, 0, sizeof (PCXHEADER));
    ph.manufacturer = 0x0a;
    ph.version = 5;
    ph.compression = compr;
    ph.bpp = 8;
    ph.planes = 3;
    ph.color = htoqs (1);
    ph.bytesperline = htoqs (blockPtr->width);
    ph.x1 = htoqs (0);
    ph.y1 = htoqs (0);
    ph.x2 = htoqs (blockPtr->width  - 1);
    ph.y2 = htoqs (blockPtr->height - 1);

    ph.hdpi = htoqs (300);
    ph.vdpi = htoqs (300);
    ph.reserved = 0;

    if (tkimg_Write(handle, (const char *)&ph, 128) != 128) {
	Tcl_AppendResult(interp, "Can't write PCX header.", (char *)NULL);
	return TCL_ERROR;
    }

    row = (UByte *) ckalloc (nBytes);
    /* Now write out the image data. */
    pixRowPtr = blockPtr->pixelPtr + blockPtr->offset[0];
    if (!compr) {
	for (y=0; y<blockPtr->height; y++) {
	    pixelPtr = pixRowPtr;
	    for (x=0; x<blockPtr->width; x++) {
		row[x + 0*blockPtr->width] = pixelPtr[redOffset];
		row[x + 1*blockPtr->width] = pixelPtr[greenOffset];
		row[x + 2*blockPtr->width] = pixelPtr[blueOffset];
		pixelPtr += blockPtr->pixelSize;
	    }
	    if (nBytes != tkimg_Write(handle, (const char *)row, nBytes)) {
		sprintf(errMsg, "Can't write %d bytes to image file.", nBytes);
		Tcl_AppendResult(interp, errMsg, (char *)NULL);
		ckfree ((char *)row);
		return TCL_ERROR;
	    }
	    pixRowPtr += blockPtr->pitch;
	}
    } else { 			/* RLE compression */
	for (y = 0; y < blockPtr->height; y++) {
	    pixelPtr = pixRowPtr;
	    for (x = 0; x < blockPtr->width; x++) {
		row[x + 0*blockPtr->width] = pixelPtr[redOffset];
		row[x + 1*blockPtr->width] = pixelPtr[greenOffset];
		row[x + 2*blockPtr->width] = pixelPtr[blueOffset];
		pixelPtr += blockPtr->pixelSize;
	    }
	    if (!writeline (handle, row, nBytes)) {
		sprintf(errMsg, "Can't write %d bytes to image file.", nBytes);
		Tcl_AppendResult(interp, errMsg, (char *)NULL);
		ckfree ((char *)row);
		return TCL_ERROR;
	    }
	    pixRowPtr += blockPtr->pitch;
	}
    }
    if (verbose)
        printImgInfo (&ph, filename, "Saving image:");
    ckfree ((char *)row);
    return TCL_OK;
}
