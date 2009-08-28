/* STARTHEADER
 *
 * File :       ico.c
 *
 * Author :     Paul Obermeier (paul@poSoft.de)
 *
 * Date :       Mon Aug 12 20:30:46 CEST 2002
 *
 * Copyright :  (C) 2002 Paul Obermeier
 *
 * Description :
 *
 * A photo image handler for Windows Icon file format.
 *
 * The following icon types are supported:
 *
 *  1-bit pixels: Black and White.
 *  4-bit pixels: Grayscale or indexed.
 *  8-bit pixels: Grayscale or indexed.
 * 24-bit pixels: True-color (RGB, each channel 8 bit).
 *
 * List of currently supported features:
 *
 * Type   |     Read      |     Write     |
 *        | -file | -data | -file | -data |
 * ----------------------------------------
 *  1-bit | Yes   | Yes   | No    | No    |
 *  4-bit | Yes   | Yes   | No    | No    |
 *  8-bit | Yes   | Yes   | Yes   | Yes   |
 * 24-bit | Yes   | Yes   | Yes   | Yes   |
 *
 *
 * The following format options are available:
 *
 * Read  ICO image: "ico -verbose <bool> -index <uint>"
 * Write ICO image: "ico -verbose <bool>"
 *
 * -verbose <bool>: If set to true, additional information about the file
 *                  format is printed to stdout. Default is "false".
 * -index <uint>:   Read the icon with specified index. Default is 0.
 *
 * Notes:
 *
 *
 * ENDHEADER
 *
 * $Id: ico.c 152 2008-10-22 08:49:40Z nijtmans $
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
typedef unsigned int UInt;	/* Unsigned 32 bit integer */
typedef int Int;		/* Signed   32 bit integer */

typedef struct {
   UByte  width;
   UByte  height;
   UShort nColors;
   UByte  reserved;
   UShort nPlanes;
   UShort bitCount;
   UInt   sizeInBytes;
   UInt   fileOffset;
} ICOENTRY;

/* ICO file header structure */
typedef struct {
   UShort   nIcons;
   ICOENTRY *entries;
} ICOHEADER;

typedef struct {
   UInt   size;
   UInt   width;
   UInt   height;
   UShort nPlanes;
   UShort nBitsPerPixel;
   UInt   compression;
   UInt   imageSize;
   UInt   xPixelsPerM;
   UInt   yPixelsPerM;
   UInt   nColorsUsed;
   UInt   nColorsImportant;
} INFOHEADER;

typedef struct {
   UByte red;
   UByte green;
   UByte blue;
   UByte matte;
} ICOCOLOR;

/* ICO file format options structure for use with ParseFormatOpts */
typedef struct {
    UInt  index;
    Boln  verbose;
} FMTOPT;

/* OPA TODO: Change from ANSI-C arguments to _ANSI_ARGS_ macro. */

static Boln readUByte (tkimg_MFile *handle, UByte *b)
{
    char buf[1];
    if (1 != tkimg_Read(handle, buf, 1)) {
        return FALSE;
    }
    *b = buf[0] & 0xFF;
    return TRUE;
}

/* Read 2 bytes, representing a unsigned 16 bit integer in the form
   <LowByte, HighByte>, from a file and convert them into the current
   machine's format. */

static Boln readUShort (tkimg_MFile *handle, UShort *s)
{
    char buf[2];
    UShort tmp;

    if (2 != tkimg_Read(handle, buf, 2)) {
        return FALSE;
    }
    tmp  =  buf[0] & 0xFF;
    tmp |= (buf[1] & 0xFF) << 8;
    *s = tmp;
    return TRUE;
}

/* Read 4 bytes, representing a unsigned 32 bit integer in the form
   <LowByte, HighByte>, from a file and convert them into the current
   machine's format. */

static Boln readUInt (tkimg_MFile *handle, UInt *i)
{
    char buf[4];
    UInt tmp;

    if (4 != tkimg_Read(handle, buf, 4)) {
        return FALSE;
    }
    tmp  =  buf[0] & 0xFF;
    tmp |= (buf[1] & 0xFF) <<  8;
    tmp |= (buf[2] & 0xFF) << 16;
    tmp |= (buf[3] & 0xFF) << 24;
    *i = tmp;
    return TRUE;
}

/* Write a byte, representing an unsigned integer to a file. */

static Boln writeUByte (tkimg_MFile *handle, UByte b)
{
    UByte buf[1];
    buf[0] = b;
    if (1 != tkimg_Write(handle, (const char *)buf, 1)) {
        return FALSE;
    }
    return TRUE;
}

/* Convert a unsigned 16 bit integer number into the format
   <LowByte, HighByte> (an array of 2 bytes) and write the array to a file. */

static Boln writeUShort (tkimg_MFile *handle, UShort s)
{
    Byte buf[2];
    buf[0] = s;
    buf[1] = s >> 8;
    if (2 != tkimg_Write(handle, buf, 2)) {
        return FALSE;
    }
    return TRUE;
}

/* Convert a unsigned 32 bit integer number into the format
   <LowByte, HighByte> (an array of 4 bytes) and write the array to a file. */

static Boln writeUInt (tkimg_MFile *handle, UInt i)
{
    Byte buf[4];
    buf[0] = i;
    buf[1] = i >> 8;
    buf[2] = i >> 16;
    buf[3] = i >> 24;
    if (4 != tkimg_Write(handle, buf, 4)) {
        return FALSE;
    }
    return TRUE;
}

#define OUT Tcl_WriteChars (outChan, str, -1)
static void printImgInfo (ICOHEADER *th, INFOHEADER *ih, FMTOPT *opts,
                          const char *filename, const char *msg)
{
    Tcl_Channel outChan;
    char str[256];
    int i = opts->index;

    outChan = Tcl_GetStdChannel (TCL_STDOUT);
    if (!outChan) {
        return;
    }
    sprintf(str, "%s %s\n", msg, filename);                                 OUT;
    sprintf(str, "  No. of icons : %d\n", th->nIcons);                      OUT;
    sprintf(str, "  Icon %d:\n", i);                                        OUT;
    sprintf(str, "    Width and Height: %dx%d\n", ih->width, ih->height);   OUT;
    sprintf(str, "    Number of colors: %d\n", th->entries[i].nColors);     OUT;
    sprintf(str, "    Number of planes: %d\n", ih->nPlanes);                OUT;
    sprintf(str, "    Bits per pixel:   %d\n", ih->nBitsPerPixel);          OUT;
    sprintf(str, "    Size in bytes:    %d\n", th->entries[i].sizeInBytes); OUT;
    sprintf(str, "    File offset:      %d\n", th->entries[i].fileOffset);  OUT;
    Tcl_Flush(outChan);
}
#undef OUT

static Boln readIcoHeader (tkimg_MFile *handle, ICOHEADER *th)
{
    int    i;
    UByte  nColors;
    UShort reserved, type, nIcons;

    if (!readUShort (handle, &reserved)) {
	return FALSE;
    }
    if (reserved != 0) {
	return FALSE;
    }

    if (!readUShort (handle, &type)) {
	return FALSE;
    }
    if (type != 1) {
	return FALSE;
    }
    if (!readUShort (handle, &nIcons)) {
	return FALSE;
    }
    if (nIcons <= 0) {
	return FALSE;
    }

    th->nIcons = nIcons;
    if (!(th->entries = (ICOENTRY *)ckalloc (sizeof (ICOENTRY) * nIcons))) {
	return FALSE;
    }

    for (i=0; i<nIcons; i++) {
	if (!readUByte  (handle, &th->entries[i].width)  ||
	    !readUByte  (handle, &th->entries[i].height) ||
	    !readUByte  (handle, &nColors) ||
	    !readUByte  (handle, &th->entries[i].reserved) ||
	    !readUShort (handle, &th->entries[i].nPlanes) ||
	    !readUShort (handle, &th->entries[i].bitCount) ||
	    !readUInt   (handle, &th->entries[i].sizeInBytes) ||
	    !readUInt   (handle, &th->entries[i].fileOffset)) {
            ckfree ((char *)th->entries);
	    return FALSE;
	}
        th->entries[i].nColors = (nColors == 0? 256: nColors);
    }
    return TRUE;
}

static Boln writeIcoHeader (tkimg_MFile *handle, ICOHEADER *th)
{
    int    i;
    UByte  nColors;
    UShort reserved = 0,
           type = 1;

    if (!writeUShort (handle, reserved)) {
	return FALSE;
    }
    if (!writeUShort (handle, type)) {
	return FALSE;
    }
    if (!writeUShort (handle, th->nIcons)) {
	return FALSE;
    }
    for (i=0; i<th->nIcons; i++) {
        nColors = (th->entries[i].nColors == 256? 0: th->entries[i].nColors);
	if (!writeUByte  (handle, th->entries[i].width)  ||
	    !writeUByte  (handle, th->entries[i].height) ||
	    !writeUByte  (handle, nColors) ||
	    !writeUByte  (handle, th->entries[i].reserved) ||
	    !writeUShort (handle, th->entries[i].nPlanes) ||
	    !writeUShort (handle, th->entries[i].bitCount) ||
	    !writeUInt   (handle, th->entries[i].sizeInBytes) ||
	    !writeUInt   (handle, th->entries[i].fileOffset)) {
	    return FALSE;
	}
    }
    return TRUE;
}

static Boln readInfoHeader (tkimg_MFile *handle, INFOHEADER *ih)
{
    if (!readUInt   (handle, &ih->size) ||
	!readUInt   (handle, &ih->width) ||
	!readUInt   (handle, &ih->height) ||
	!readUShort (handle, &ih->nPlanes) ||
	!readUShort (handle, &ih->nBitsPerPixel) ||
	!readUInt   (handle, &ih->compression) ||
	!readUInt   (handle, &ih->imageSize) ||
	!readUInt   (handle, &ih->xPixelsPerM) ||
	!readUInt   (handle, &ih->yPixelsPerM) ||
	!readUInt   (handle, &ih->nColorsUsed) ||
	!readUInt   (handle, &ih->nColorsImportant)) {
	return FALSE;
    }
    #if defined (DEBUG_LOCAL)
	printf("Info header:\n");
	printf("Size: %d\n", ih->size);
	printf("Width: %d\n", ih->width);
	printf("Height: %d\n", ih->height);
	printf("Planes: %d\n", ih->nPlanes);
	printf("BitsPerPixel: %d\n", ih->nBitsPerPixel);
	printf("Compression: %d\n", ih->compression);
	printf("Image size: %d\n", ih->imageSize);
	printf("XPixelsPerM: %d\n", ih->xPixelsPerM);
	printf("YPixelsPerM: %d\n", ih->yPixelsPerM);
	printf("ColorsUsed: %d\n", ih->nColorsUsed);
	printf("ColorsImportant: %d\n", ih->nColorsImportant);
    #endif
    return TRUE;
}

static Boln writeInfoHeader (tkimg_MFile *handle, INFOHEADER *ih)
{
    if (!writeUInt   (handle, ih->size) ||
	!writeUInt   (handle, ih->width) ||
	!writeUInt   (handle, ih->height) ||
	!writeUShort (handle, ih->nPlanes) ||
	!writeUShort (handle, ih->nBitsPerPixel) ||
	!writeUInt   (handle, ih->compression) ||
	!writeUInt   (handle, ih->imageSize) ||
	!writeUInt   (handle, ih->xPixelsPerM) ||
	!writeUInt   (handle, ih->yPixelsPerM) ||
	!writeUInt   (handle, ih->nColorsUsed) ||
	!writeUInt   (handle, ih->nColorsImportant)) {
	return FALSE;
    }
    #if defined (DEBUG_LOCAL)
	printf("Writing Info header:\n");
	printf("Size        : %d\n", ih->size);
	printf("Width       : %d\n", ih->width);
	printf("Height      : %d\n", ih->height);
	printf("Planes      : %d\n", ih->nPlanes);
	printf("BitsPerPixel: %d\n", ih->nBitsPerPixel);
	printf("Compression : %d\n", ih->compression);
	printf("Image size  : %d\n", ih->imageSize);
	printf("XPixelsPerM : %d\n", ih->xPixelsPerM);
	printf("YPixelsPerM : %d\n", ih->yPixelsPerM);
	printf("ColorsUsed  : %d\n", ih->nColorsUsed);
	printf("ColorsImport: %d\n", ih->nColorsImportant);
    #endif
    return TRUE;
}

static Boln readColorMap (tkimg_MFile *handle, int mapSize, ICOCOLOR *colorMap)
{
    int i;
    ICOCOLOR color;

    for (i=0; i<mapSize; i++) {
        if (!readUByte (handle, &color.blue) ||
	    !readUByte (handle, &color.green) ||
	    !readUByte (handle, &color.red) ||
	    !readUByte (handle, &color.matte)) {
	    return FALSE;
	}
        colorMap[i] = color;
    }
    return TRUE;
}

static Boln writeColorMap (tkimg_MFile *handle, int mapSize, ICOCOLOR *colorMap)
{
    int i;

    for (i=0; i<mapSize; i++) {
        if (!writeUByte (handle, colorMap[i].blue) ||
	    !writeUByte (handle, colorMap[i].green) ||
	    !writeUByte (handle, colorMap[i].red) ||
	    !writeUByte (handle, colorMap[i].matte)) {
	    return FALSE;
	}
    }
    return TRUE;
}

/*
 * Prototypes for local procedures defined in this file.
 */

static int ParseFormatOpts _ANSI_ARGS_((Tcl_Interp *interp, Tcl_Obj *format,
               FMTOPT *opts));
static int CommonMatch _ANSI_ARGS_((tkimg_MFile *handle, int *widthPtr,
	       int *heightPtr, ICOHEADER *icoHeaderPtr));
static int CommonRead _ANSI_ARGS_((Tcl_Interp *interp, tkimg_MFile *handle,
	       const char *filename, Tcl_Obj *format,
	       Tk_PhotoHandle imageHandle, int destX, int destY,
	       int width, int height, int srcX, int srcY));
static int CommonWrite _ANSI_ARGS_((Tcl_Interp *interp, tkimg_MFile *handle,
               Tk_PhotoImageBlock *blockPtr));

static int ParseFormatOpts (interp, format, opts)
    Tcl_Interp *interp;
    Tcl_Obj *format;
    FMTOPT *opts;
{
    static const char *const icoOptions[] = {
         "-verbose", "-index"
    };
    int objc, length, c, i, index;
    Tcl_Obj **objv;
    CONST84 char *indexStr, *verboseStr;

    /* Initialize format options with default values. */
    verboseStr = "0";
    indexStr   = "0";

    if (tkimg_ListObjGetElements(interp, format, &objc, &objv) != TCL_OK)
	return TCL_ERROR;
    if (objc) {
	for (i=1; i<objc; i++) {
	    if (Tcl_GetIndexFromObj(interp, objv[i], (CONST84 char *CONST86 *)icoOptions,
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
		    verboseStr = Tcl_GetStringFromObj(objv[i], (int *) NULL);
		    break;
		case 1:
		    indexStr = Tcl_GetStringFromObj(objv[i], (int *) NULL);
		    break;
	    }
	}
    }

    /* OPA TODO: Check for valid integer strings. */
    opts->index  = atoi (indexStr);

    c = verboseStr[0]; length = strlen (verboseStr);
    if (!strncmp (verboseStr, "1", length) || \
	!strncmp (verboseStr, "true", length) || \
	!strncmp (verboseStr, "on", length)) {
	opts->verbose = 1;
    } else if (!strncmp (verboseStr, "0", length) || \
	!strncmp (verboseStr, "false", length) || \
	!strncmp (verboseStr, "off", length)) {
	opts->verbose = 0;
    } else {
	Tcl_AppendResult(interp, "invalid verbose mode \"", verboseStr,
			  "\": should be 1 or 0, on or off, true or false",
			  (char *) NULL);
	return TCL_ERROR;
    }

    return TCL_OK;
}

static int ChnMatch(interp, chan, fileName, format, widthPtr, heightPtr)
    Tcl_Interp *interp;
    Tcl_Channel chan;
    const char *fileName;
    Tcl_Obj *format;
    int *widthPtr, *heightPtr;
{
    tkimg_MFile handle;

    tkimg_FixChanMatchProc(&interp, &chan, &fileName, &format, &widthPtr, &heightPtr);

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

    tkimg_FixObjMatchProc(&interp, &data, &format, &widthPtr, &heightPtr);

    if (!tkimg_ReadInit(data, '\000', &handle)) {
	return 0;
    }
    return CommonMatch(&handle, widthPtr, heightPtr, NULL);
}

static int CommonMatch(handle, widthPtr, heightPtr, icoHeaderPtr)
    tkimg_MFile *handle;
    int *widthPtr, *heightPtr;
    ICOHEADER *icoHeaderPtr;
{
    ICOHEADER icoHeader, *headerPtr;

    if (!icoHeaderPtr) {
        headerPtr = &icoHeader;
    } else {
	headerPtr = icoHeaderPtr;
    }
    if (!readIcoHeader (handle, headerPtr)) {
	return 0;
    }

    *widthPtr  = headerPtr->entries[0].width;
    *heightPtr = headerPtr->entries[0].height;

    if (!icoHeaderPtr) {
	ckfree ((char *) headerPtr->entries);
    }
    return 1;
}

static int ChnRead(interp, chan, filename, format, imageHandle,
                    destX, destY, width, height, srcX, srcY)
    Tcl_Interp *interp;
    Tcl_Channel chan;
    const char *filename;
    Tcl_Obj *format;
    Tk_PhotoHandle imageHandle;
    int destX, destY;
    int width, height;
    int srcX, srcY;
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

    tkimg_ReadInit(data, '\000', &handle);

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
    Tcl_Interp *interp;         /* Interpreter to use for reporting errors. */
    tkimg_MFile *handle;              /* The image file, open for reading. */
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
    myblock bl;
    int x, y;
    int fileWidth, fileHeight;
    int outWidth, outHeight, outY;
    int bytesPerLine;
    int nBytesToSkip;
    int errorFlag = TCL_OK;
    unsigned char *line = NULL, *expline = NULL;
    char msgStr[1024];
    ICOHEADER  icoHeader;
    INFOHEADER infoHeader;
    ICOCOLOR   colorMap[256];
    FMTOPT opts;

    if (ParseFormatOpts(interp, format, &opts) != TCL_OK) {
        return TCL_ERROR;
    }

    if (!CommonMatch(handle, &fileWidth, &fileHeight, &icoHeader)) {
	Tcl_AppendResult(interp, "Error reading header", (char *)NULL);
        errorFlag = TCL_ERROR;
        goto error;
    }

    if (opts.index < 0 || opts.index >= icoHeader.nIcons) {
        sprintf(msgStr, "Invalid icon index: %d", opts.index);
	Tcl_AppendResult(interp, msgStr, (char *)NULL);
        errorFlag = TCL_ERROR;
        goto error;
    }

    /* Instead of seeking, which does not work on strings,
       we calculate the number of bytes from the current position
       till the start of the INFOHEADER and read these bytes with tkimg_Read. */
    nBytesToSkip = icoHeader.entries[opts.index].fileOffset -6 -
                   16 * icoHeader.nIcons;
    if (nBytesToSkip > 0) {
        char *dummy = ckalloc (nBytesToSkip);
	tkimg_Read(handle, dummy, nBytesToSkip);
        ckfree ((char *) dummy);
    }

    /* Read Info header and color map */
    if (!readInfoHeader (handle, &infoHeader)) {
	Tcl_AppendResult(interp, "Error reading info header", (char *)NULL);
        errorFlag = TCL_ERROR;
        goto error;
    }
    if (infoHeader.nBitsPerPixel != 24) {
	if (!readColorMap (handle, icoHeader.entries[opts.index].nColors,
			   colorMap)) {
	    Tcl_AppendResult(interp, "Error reading color map", (char *)NULL);
	    errorFlag = TCL_ERROR;
	    goto error;
	}
    }

    fileWidth  = infoHeader.width;
    fileHeight = infoHeader.height / 2;
    outWidth   = fileWidth;
    outHeight  = fileHeight;
    if (fileWidth != width || fileHeight != height) {
        if (srcX != 0 || srcY != 0 || destX != 0 || destY != 0) {
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
	}
    }
    if ((outWidth <= 0) || (outHeight <= 0)
	|| (srcX >= fileWidth) || (srcY >= fileHeight)) {
	return TCL_OK;
    }

    if (opts.verbose) {
        printImgInfo (&icoHeader, &infoHeader, &opts,
		      filename, "Reading image:");
    }

    Tk_PhotoSetSize(imageHandle, destX + outWidth, destY + outHeight);
    Tk_PhotoExpand(imageHandle, destX + outWidth, destY + outHeight);

    bytesPerLine = ((infoHeader.nBitsPerPixel * fileWidth + 31)/32)*4;

    block.pixelSize = 4;
    block.pitch = fileWidth * 4;
    block.width = outWidth;
    block.height = 1;
    block.offset[0] = 0;
    block.offset[1] = 1;
    block.offset[2] = 2;
    block.offset[3] = 3;
    block.pixelPtr = (unsigned char *) ckalloc (4 * fileWidth * fileHeight);
    expline = block.pixelPtr;

    line = (unsigned char *) ckalloc(bytesPerLine);
    switch (infoHeader.nBitsPerPixel) {
	case 24:
	    for (y=0; y<fileHeight; y++) {
                tkimg_Read(handle, (char *)line, bytesPerLine);
		for (x = 0; x < fileWidth; x++) {
		    expline[0] = line[x*3 + 2];
		    expline[1] = line[x*3 + 1];
		    expline[2] = line[x*3 + 0];
		    expline += 4;
		}
            }
            break;
	case 8:
	    for (y=0; y<fileHeight; y++) {
		tkimg_Read(handle, (char *)line, bytesPerLine);
		for (x = 0; x < fileWidth; x++) {
		    expline[0] = colorMap[line[x]].red;
		    expline[1] = colorMap[line[x]].green;
		    expline[2] = colorMap[line[x]].blue;
		    expline += 4;
		}
	    }
	    break;
	case 4:
	    for (y=0; y<fileHeight; y++) {
		int c;
		tkimg_Read(handle, (char *)line, bytesPerLine);
		for (x=0; x<fileWidth; x++) {
		    if (x&1) {
			c = line[x/2] & 0x0f;
		    } else {
			c = line[x/2] >> 4;
		    }
		    expline[0] = colorMap[c].red;
		    expline[1] = colorMap[c].green;
		    expline[2] = colorMap[c].blue;
		    expline += 4;
		}
	    }
	    break;
	case 1:
	    for (y=0; y<fileHeight; y++) {
		int c;
		tkimg_Read(handle, (char *)line, bytesPerLine);
		for (x=0; x<fileWidth; x++) {
		    c = (line[x/8] >> (7-(x%8))) & 1;
		    expline[0] = colorMap[c].red;
		    expline[1] = colorMap[c].green;
		    expline[2] = colorMap[c].blue;
		    expline += 4;
		}
	    }
	    break;
	default:
	    sprintf(msgStr,"%d-bits ICO file not supported",
                     infoHeader.nBitsPerPixel);
	    Tcl_AppendResult(interp, msgStr, (char *)NULL);
	    errorFlag = TCL_ERROR;
	    goto error;
    }

    /* Read XAND bitmap. */
    bytesPerLine = ((1 * fileWidth + 31)/32)*4;

    expline = block.pixelPtr;
    for (y=0; y<fileHeight; y++) {
	int c;
	tkimg_Read(handle, (char *)line, bytesPerLine);
	for (x=0; x<fileWidth; x++) {
	    c = (line[x/8] >> (7-(x%8))) & 1;
	    expline[3] = (c? 0: 255);
	    expline += 4;
	}
    }

    /* Store the pointer to allocated buffer for later freeing. */
    expline = block.pixelPtr;
    block.pixelPtr += srcX * 4;

    outY = destY + outHeight - 1;
    for (y=fileHeight-1; y>=0; y--) {
        if (y >= srcY && y < srcY + outHeight) {
	    tkimg_PhotoPutBlock(interp, imageHandle, &block, destX, outY,
		    outWidth, 1, TK_PHOTO_COMPOSITE_SET);
	    outY--;
        }
        block.pixelPtr += 4 * fileWidth;
    }
    block.pixelPtr = expline;

error:
    if (icoHeader.entries) {
	ckfree((char *) icoHeader.entries);
    }
    if (line) {
	ckfree((char *) line);
    }
    if (expline) {
	ckfree((char *) block.pixelPtr);
    }
    return errorFlag;
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

    chan = tkimg_OpenFileChannel(interp, filename, 0644);
    if (!chan) {
	return TCL_ERROR;
    }

    handle.data = (char *) chan;
    handle.state = IMG_CHAN;

    result = CommonWrite(interp, &handle, blockPtr);
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

    tkimg_FixStringWriteProc(&data, &interp, &dataPtr, &format, &blockPtr);

    tkimg_WriteInit(dataPtr, &handle);
    result = CommonWrite(interp, &handle, blockPtr);
    tkimg_Putc(IMG_DONE, &handle);

    if ((result == TCL_OK) && (dataPtr == &data)) {
	Tcl_DStringResult(interp, dataPtr);
    }
    return result;
}

static int CommonWrite (interp, handle, blockPtr)
    Tcl_Interp *interp;
    tkimg_MFile *handle;
    Tk_PhotoImageBlock *blockPtr;
{
    int bytesPerLineXOR, bytesPerLineAND, nbytes, ncolors, i, x, y;
    int redOffset, greenOffset, blueOffset, alphaOffset;
    int foundColor;
    UByte *imagePtr, *pixelPtr;
    UByte buf[4];
    ICOHEADER  icoHeader;
    INFOHEADER infoHeader;
    ICOCOLOR   colorMap[256];
    ICOCOLOR   pixel;

    if (blockPtr->width > 255 || blockPtr->height > 255) {
	Tcl_AppendResult(interp, "ICO images must be less than 256 pixels.",
		          (char *) NULL);
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
    ncolors = 0;
    if (greenOffset || blueOffset) {
	for (y = 0; ncolors <= 256 && y < blockPtr->height; y++) {
	    pixelPtr = blockPtr->pixelPtr + y*blockPtr->pitch + blockPtr->offset[0];
	    for (x=0; ncolors <= 256 && x<blockPtr->width; x++) {
		pixel.red   = pixelPtr[redOffset];
		pixel.green = pixelPtr[greenOffset];
		pixel.blue  = pixelPtr[blueOffset];
		if (alphaOffset && (pixelPtr[alphaOffset] == 0)) {
		    pixel.matte = 0;
		} else {
		    pixel.matte = 1;
		}
		foundColor = 0;
		for (i=0; i<ncolors; i++) {
		    if (pixel.red   == colorMap[i].red &&
                        pixel.green == colorMap[i].green &&
                        pixel.blue  == colorMap[i].blue) {
			foundColor = 1;
			break;
		    }
		}
		if (!foundColor) {
		    if (ncolors < 256) {
			colorMap[ncolors] = pixel;
		    }
		    ncolors++;
		}
		pixelPtr += blockPtr->pixelSize;
	    }
	}
	if (ncolors <= 256) {
	    pixel.red = pixel.green = pixel.blue = pixel.matte = 0;
            while (ncolors < 256) {
		colorMap[ncolors++] = pixel;
	    }
	    nbytes = 1;
	} else {
	    nbytes = 3;
	    ncolors = 0;
	}
    } else {
        nbytes = 1;
    }

    bytesPerLineXOR = ((blockPtr->width * nbytes + 3) / 4) * 4;
    bytesPerLineAND = ((blockPtr->width * 1      + 31) / 32 )* 4;

    icoHeader.nIcons = 1;
    if (!(icoHeader.entries = (ICOENTRY *) ckalloc (sizeof (ICOENTRY)))) {
	return TCL_ERROR;
    }
    icoHeader.entries[0].width       = blockPtr->width;
    icoHeader.entries[0].height      = blockPtr->height;
    icoHeader.entries[0].nColors     = (ncolors > 0? ncolors: 0);
    icoHeader.entries[0].reserved    = 0;
    icoHeader.entries[0].nPlanes     = 1;
    icoHeader.entries[0].bitCount    = (ncolors > 0? 8: 24);
    icoHeader.entries[0].sizeInBytes = sizeof (INFOHEADER) +
				       ncolors * sizeof (ICOCOLOR) +
                                       bytesPerLineXOR * blockPtr->height +
                                       bytesPerLineAND * blockPtr->height;
    icoHeader.entries[0].fileOffset  = 6 + icoHeader.nIcons * 16;

    if (!writeIcoHeader (handle, &icoHeader)) {
	return TCL_ERROR;
    }

    infoHeader.size = sizeof (INFOHEADER);
    infoHeader.width = blockPtr->width;
    infoHeader.height = blockPtr->height * 2;
    infoHeader.nPlanes = 1;
    infoHeader.nBitsPerPixel = (ncolors > 0? 8: 24);
    infoHeader.compression = 0;
    infoHeader.imageSize = 0;
    infoHeader.xPixelsPerM = 0;
    infoHeader.yPixelsPerM = 0;
    infoHeader.nColorsUsed = 0;
    infoHeader.nColorsImportant = 0;

    if (!writeInfoHeader (handle, &infoHeader)) {
	return TCL_ERROR;
    }

    if (ncolors > 0) {
	if (!writeColorMap (handle, ncolors, colorMap)) {
	    return TCL_ERROR;
        }
    }

    bytesPerLineXOR -= blockPtr->width * nbytes;

    imagePtr = blockPtr->pixelPtr + blockPtr->offset[0] +
               blockPtr->height * blockPtr->pitch;
    for (y = 0; y < blockPtr->height; y++) {
	pixelPtr = imagePtr -= blockPtr->pitch;
	for (x=0; x<blockPtr->width; x++) {
	    if (ncolors) {
		for (i=0; i<ncolors; i++) {
                    if (pixelPtr[redOffset]   == colorMap[i].red &&
                        pixelPtr[greenOffset] == colorMap[i].green &&
                        pixelPtr[blueOffset]  == colorMap[i].blue) {
			buf[0] = i;
		    }
		}
	    } else {
		buf[0] = pixelPtr[blueOffset];
		buf[1] = pixelPtr[greenOffset];
		buf[2] = pixelPtr[redOffset];
	    }
	    tkimg_Write(handle, (char *) buf, nbytes);
	    pixelPtr += blockPtr->pixelSize;
	}
	if (bytesPerLineXOR) {
	    tkimg_Write(handle, "\0\0\0", bytesPerLineXOR);
	}
    }

    bytesPerLineAND -= blockPtr->width / 8;

    imagePtr = blockPtr->pixelPtr + blockPtr->offset[0] +
               blockPtr->height * blockPtr->pitch;
    for (y = 0; y < blockPtr->height; y++) {
	int c;
	pixelPtr = imagePtr -= blockPtr->pitch;
	for (x=0; x<blockPtr->width; x++) {
	    if (x % 8 == 0) {
		buf[0] = 0;
	    }
	    if (alphaOffset) {
		c = pixelPtr[alphaOffset];
		if (c == 0) {
		    buf[0] |= 1<<(7-x%8);
		}
	    }
	    if (x % 8 == 7) {
		tkimg_Write(handle, (char *) buf, 1);
	    }
	    pixelPtr += blockPtr->pixelSize;
	}
	if (bytesPerLineAND) {
	    tkimg_Write(handle, "\0\0\0", bytesPerLineAND);
	}
    }
    return TCL_OK;
}
