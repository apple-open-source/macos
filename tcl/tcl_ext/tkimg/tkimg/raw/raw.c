/* STARTHEADER
 *
 * File :       raw.c
 *
 * Author :     Paul Obermeier (paul@poSoft.de)
 *
 * Date :       Wed Feb 21 12:45:08 CET 2001
 *
 * Copyright :  (C) 2001-2004 Paul Obermeier
 *
 * Description :
 *
 * A photo image handler for raw data interpreted as image files.
 *
 * The following image types are currently supported:
 *
 * Grayscale image:  1 channel  of 32-bit floating point   values.
 * 		     1 channel  of 16-bit unsigned integer values.
 * 		     1 channel  of  8-bit unsigned integer values.
 * True-color image: 3 channels of 32-bit floating point   values.
 * 		     3 channels of 16-bit unsigned integer values.
 * 		     3 channels of  8-bit unsigned integer values.
 *
 * List of currently supported features:
 *
 * Type   |     Read      |     Write     |
 *        | -file | -data | -file | -data |
 * ----------------------------------------
 * Gray   | Yes   | Yes   | Yes   | Yes   |
 * RGB    | Yes   | Yes   | Yes   | Yes   |
 *
 * There are 2 supported file formats:
 * One with the pure raw data only, the other with a 7 line ASCII header of the
 * following form:
 *
 *     Magic=RAW\n		File format identifier. Fixed value.
 *     Width=128\n		Image width in pixels.
 *     Height=128\n		Image height in pixels.
 *     NumChan=1\n		Possible values: 1 or 3.
 *     ByteOrder=Intel\n	Possible values: "Intel" or "Motorola".
 *     ScanOrder=TopDown\n	Possible values: "TopDown" or "BottomUp".
 *     PixelType=byte\n		Possible values: "float", "short" or "byte".
 *
 * The following format options are available:
 *
 * For raw images with header:
 * Read  RAW image: "raw -useheader true -verbose <bool> -gamma <float>
 *			 -min <float> -max <float>"
 * Write RAW image: "raw -useheader true -verbose <bool> -nchan <int>
 *                       -scanorder <string>"
 *
 * For raw images without header:
 * Read  RAW image: "raw -useheader false -verbose <bool> -nchan <int>
 *                       -scanorder <string> -byteorder <string>
 *                       -width <int> -height <int> -gamma <float>
 *			 -pixeltype <string> -min <float> -max <float>"
 * Write RAW image: "raw -useheader false -verbose <bool> -nchan <int>
 *                       -scanorder <string>"
 *
 * -verbose <bool>:     If set to true, additional information about the file
 *                      format is printed to stdout. Default is false.
 * -useheader <bool>:   If set to true, use file header information for reading
 *                      and writing. Default is true.
 * -nchan <int>:        Specify the number of channels of the input image.
 *			Only valid, if reading image data without header.
 *			Default is 1.
 * -width <int>:        Specify the width of the input image. Only valid, if
 *                      reading image data without header. Default is 128.
 * -height <int>:       Specify the height of the input image. Only valid, if
 *                      reading image data without header. Default is 128.
 * -byteorder <string>: Specify the byteorder of the input image. Only valid, if
 *                      reading image data without header.
 *			Possible values: "Intel" or "Motorola".
 *			Default is assuming the same byteorder as that of the
 *			host computer.
 * -scanorder <string>: Specify the scanline order of the input image. Only
 *			valid, if reading image data without header.
 *			Possible values: "TopDown" or "BottomUp".
 *			Default is "TopDown".
 * -pixeltype <string>: Specify the type of the pixel values.
 *			Only valid, if reading image data without header.
 *			Possible values: "float", "short" or "byte".
 *			Default is "byte".
 * -gamma <float>:      Specify a gamma correction to be applied when mapping
 *			the input data to 8-bit image values.
 *                      Default is 1.0.
 * -nomap <bool>:       If set to true, no mapping of input values is done.
 * 			Use this option, if your image already contains RGB
 * 			values in the range of 0 ..255.
 *                      Default is false.
 * -min <float>:        Specify the minimum pixel value to be used for mapping
 *			the input data to 8-bit image values.
 *                      Default is the minimum value found in the image data.
 * -max <float>:        Specify the maximum pixel value to be used for mapping
 *			the input data to 8-bit image values.
 *                      Default is the maximum value found in the image data.
 *
 * Notes:
 * 			Currently RAW files are only written in "byte" pixel format.
 *
 * ENDHEADER
 *
 * $Id$
 *
 */

#include <stdlib.h>
#include <math.h>

/*
 * Generic initialization code, parameterized via CPACKAGE and PACKAGE.
 */

#include "init.c"


/* #define DEBUG_LOCAL */

/* Maximum length of a header line. */
#define HEADLEN 100

/* Header fields. */
#define strMagic     "Magic=%s\n"
#define strWidth     "Width=%d\n"
#define strHeight    "Height=%d\n"
#define strNumChan   "NumChan=%d\n"
#define strByteOrder "ByteOrder=%s\n"
#define strScanOrder "ScanOrder=%s\n"
#define strPixelType "PixelType=%s\n"

/* Header fields possible values. */
#define strIntel    "Intel"
#define strMotorola "Motorola"
#define strTopDown  "TopDown"
#define strBottomUp "BottomUp"
#define strFloat    "float"
#define strUShort   "short"
#define strUByte    "byte"

#define strUnknown  "Unknown"

#define BOTTOM_UP   0
#define TOP_DOWN    1
#define INTEL       0
#define MOTOROLA    1
#define TYPE_FLOAT  0
#define TYPE_USHORT 1
#define TYPE_UBYTE  2

#define MAXCHANS  4

/* Some general defines and typedefs. */
#define TRUE  1
#define FALSE 0
#define MIN(a,b) ((a)<(b)? (a): (b))
#define MAX(a,b) ((a)>(b)? (a): (b))

typedef unsigned char Boln;	/* Boolean value: TRUE or FALSE */
typedef unsigned char UByte;	/* Unsigned  8 bit integer */
typedef char  Byte;		/* Signed    8 bit integer */
typedef unsigned short UShort;	/* Unsigned 16 bit integer */
typedef short Short;		/* Signed   16 bit integer */
typedef int UInt;		/* Unsigned 32 bit integer */
typedef int Int;		/* Signed   32 bit integer */
typedef float Float;		/* IEEE     32 bit floating point */
typedef double Double;		/* IEEE     64 bit floating point */

/* RAW file header structure */
typedef struct {
    char  id[3];
    Int   nChans;
    Int   width;
    Int   height;
    Int   scanOrder;
    Int   byteOrder;
    Int   pixelType;
} RAWHEADER;

/* RAW file format options structure for use with ParseFormatOpts */
typedef struct {
    Int   width;
    Int   height;
    Int   nchan;
    Int   scanOrder;
    Int   byteOrder;
    Int   pixelType;
    Float minVal;
    Float maxVal;
    Float gamma;
    Boln  nomap;
    Boln  verbose;
    Boln  useHeader;
} FMTOPT;

/* Structure to hold information about the Targa file being processed. */
typedef struct {
    RAWHEADER th;
    UByte  *pixbuf;
    Float  *floatBuf;
    UShort *ushortBuf;
    UByte  *ubyteBuf;
} RAWFILE;

#define MINGAMMA 0.01
#define MAXGAMMA 100.0
#define GTABSIZE 257

/* Given a pixel value in Float format, "valIn", and a gamma-correction
 * lookup table, "tab", macro "gcorrectFloat" returns the gamma-corrected
 * pixel value in "valOut".
 */

#define gcorrectFloat(valIn,tab,valOut)                                 \
    {                                                                   \
        Int     gc_i;                                                   \
        Float   gc_t;                                                   \
        gc_t = (valIn) * (GTABSIZE - 2);                                \
        gc_i = gc_t;                                                    \
        gc_t -= gc_i;                                                   \
        (valOut) = (tab)[gc_i] * (1.0-gc_t) + (tab)[gc_i+1] * gc_t;     \
    }

static Boln gtableFloat (Float gamma, Float table[])
{
    Int i;

    if (gamma < MINGAMMA || gamma > MAXGAMMA) {
        printf ("Invalid gamma value %f\n", gamma);
        return FALSE;
    }
    for (i = 0; i < GTABSIZE - 1; ++i) {
        table[i] = pow ((Float) i / (Float) (GTABSIZE - 2), 1.0 / gamma);
    }
    table[GTABSIZE - 1] = 1.0;
    return TRUE;
}

/* If no gamma correction is needed (i.e. gamma == 1.0), specify NULL for
 * parameter gtable.
 */
static void FloatGammaUByte (Int n, const Float floatIn[],
                             const Float gtable[], UByte ubOut[])
{
    const Float *src, *stop;
    Float       ftmp;
    Int         itmp;
    UByte       *ubdest;

    src = floatIn;
    stop = floatIn + n;
    ubdest = ubOut;

    /* Handle a gamma value of 1.0 (gtable == NULL) as a special case.
       Quite nice speed improvement for the maybe most used case. */
    if (gtable) {
	while (src < stop) {
	    ftmp = MAX (0.0, MIN (*src, 1.0));
	    gcorrectFloat (ftmp, gtable, ftmp);
	    itmp = (Int)(ftmp * 255.0 + 0.5);
	    *ubdest = MAX (0, MIN (itmp, 255));
	    ++ubdest;
	    ++src;
	}
    } else {
	while (src < stop) {
	    itmp = (Int)(*src * 255.0 + 0.5);
	    *ubdest = MAX (0, MIN (itmp, 255));
	    ++ubdest;
	    ++src;
	}
    }
    return;
}

/* If no gamma correction is needed (i.e. gamma == 1.0), specify NULL for
 * parameter gtable.
 */
static void UShortGammaUByte (Int n, const UShort shortIn[],
                              const Float gtable[], UByte ubOut[])
{
    const UShort *src, *stop;
    Float        ftmp;
    Int          itmp;
    UByte        *ubdest;

    src = shortIn;
    stop = shortIn + n;
    ubdest = ubOut;

    /* Handle a gamma value of 1.0 (gtable == NULL) as a special case.
       Quite nice speed improvement for the maybe most used case. */
    if (gtable) {
	while (src < stop) {
	    ftmp = *src / 65535.0;
	    ftmp = MAX (0.0, MIN (ftmp, 1.0));
	    gcorrectFloat (ftmp, gtable, ftmp);
	    itmp = (Int)(ftmp * 255.0 + 0.5);
	    *ubdest = MAX (0, MIN (itmp, 255));
	    ++ubdest;
	    ++src;
	}
    } else {
	while (src < stop) {
	    itmp = (Int)(*src / 256);
	    *ubdest = MAX (0, MIN (itmp, 255));
	    ++ubdest;
	    ++src;
	}
    }
    return;
}

/* This function determines at runtime, whether we are on an Intel system. */

static int isIntel (void)
{
    char order[] = { 1, 2, 3, 4};
    unsigned long val = (unsigned long)*((short *)order);
    /* On Intel (little-endian) systems this value is equal to 513.
       On big-endian systems this value equals 258. */
    return (val == 513);
}

static void rawClose (RAWFILE *tf)
{
    if (tf->pixbuf)    ckfree ((char *)tf->pixbuf);
    if (tf->floatBuf)  ckfree ((char *)tf->floatBuf);
    if (tf->ushortBuf) ckfree ((char *)tf->ushortBuf);
    if (tf->ubyteBuf)  ckfree ((char *)tf->ubyteBuf);
    return;
}

static Boln readFloatRow (tkimg_MFile *handle, Float *pixels, Int nFloats,
                          char *buf, Boln swapBytes)
{
    int   i;
    Float *mPtr = pixels;
    char  *bufPtr = buf;

    #ifdef DEBUG_LOCAL
	printf ("Reading %d floats\n", nFloats);
    #endif
    if (4 * nFloats != tkimg_Read (handle, buf, 4 * nFloats))
        return FALSE;

    if (swapBytes) {
	for (i=0; i<nFloats; i++) {
	    ((char *)mPtr)[0] = bufPtr[3];
	    ((char *)mPtr)[1] = bufPtr[2];
	    ((char *)mPtr)[2] = bufPtr[1];
	    ((char *)mPtr)[3] = bufPtr[0];
	    mPtr++;
	    bufPtr += 4;
	}
    } else {
	for (i=0; i<nFloats; i++) {
	    ((char *)mPtr)[0] = bufPtr[0];
	    ((char *)mPtr)[1] = bufPtr[1];
	    ((char *)mPtr)[2] = bufPtr[2];
	    ((char *)mPtr)[3] = bufPtr[3];
	    mPtr++;
	    bufPtr += 4;
	}
    }
    return TRUE;
}

static Boln readUShortRow (tkimg_MFile *handle, UShort *pixels, Int nShorts,
                           char *buf, Boln swapBytes)
{
    int    i;
    UShort *mPtr = pixels;
    char   *bufPtr = buf;

    #ifdef DEBUG_LOCAL
	printf ("Reading %d UShorts\n", nShorts);
    #endif
    if (2 * nShorts != tkimg_Read (handle, buf, 2 * nShorts))
        return FALSE;

    if (swapBytes) {
	for (i=0; i<nShorts; i++) {
	    ((char *)mPtr)[0] = bufPtr[1];
	    ((char *)mPtr)[1] = bufPtr[0];
	    mPtr++;
	    bufPtr += 2;
	}
    } else {
	for (i=0; i<nShorts; i++) {
	    ((char *)mPtr)[0] = bufPtr[0];
	    ((char *)mPtr)[1] = bufPtr[1];
	    mPtr++;
	    bufPtr += 2;
	}
    }
    return TRUE;
}

static Boln readUByteRow (tkimg_MFile *handle, UByte *pixels, Int nBytes,
                          char *buf, Boln swapBytes)
{
    int    i;
    UByte *mPtr = pixels;
    char   *bufPtr = buf;

    #ifdef DEBUG_LOCAL
	printf ("Reading %d UBytes\n", nBytes);
    #endif
    if (nBytes != tkimg_Read (handle, buf, nBytes))
        return FALSE;

    for (i=0; i<nBytes; i++) {
	((char *)mPtr)[0] = bufPtr[0];
	mPtr++;
	bufPtr += 1;
    }
    return TRUE;
}

#define OUT Tcl_WriteChars (outChan, str, -1)
static void printImgInfo (RAWHEADER *th, FMTOPT *opts,
                          const char *filename, const char *msg)
{
    Tcl_Channel outChan;
    char str[256];

    outChan = Tcl_GetStdChannel (TCL_STDOUT);
    if (!outChan) {
        return;
    }
    sprintf (str, "%s %s\n", msg, filename);                                                  OUT;
    sprintf (str, "\tSize in pixel    : %d x %d\n", th->width, th->height);                   OUT;
    sprintf (str, "\tNo. of channels  : %d\n", th->nChans);                                   OUT;
    sprintf (str, "\tPixel type       : %s\n", (th->pixelType == TYPE_FLOAT?  strFloat:
                                               (th->pixelType == TYPE_USHORT? strUShort:
                                               (th->pixelType == TYPE_UBYTE?  strUByte:
                                                                              strUnknown)))); OUT;
    sprintf (str, "\tVertical encoding: %s\n", th->scanOrder == TOP_DOWN?
                                               strTopDown: strBottomUp);                      OUT;
    sprintf (str, "\tGamma correction : %f\n", opts->gamma);                                  OUT;
    sprintf (str, "\tMinimum map value: %f\n", opts->minVal);                                 OUT;
    sprintf (str, "\tMaximum map value: %f\n", opts->maxVal);                                 OUT;
    sprintf (str, "\tHost byte order  : %s\n", isIntel ()?  strIntel: strMotorola);           OUT;
    sprintf (str, "\tFile byte order  : %s\n", th->byteOrder == INTEL?
                                               strIntel: strMotorola);                        OUT;
    Tcl_Flush (outChan);
}
#undef OUT

static Boln readHeaderLine (Tcl_Interp *interp, tkimg_MFile *handle, char *buf)
{
    char c, *bufPtr, *bufEndPtr;
    Boln failure;

    bufPtr    = buf;
    bufEndPtr = buf + HEADLEN;
    failure   = TRUE;

    #ifdef DEBUG_LOCAL
	printf ("readHeaderLine\n"); fflush (stdout);
    #endif

    while (tkimg_Read (handle, &c, 1) == 1 && bufPtr < bufEndPtr) {
	if (c == '\n') {
	    *bufPtr = '\0';
	    failure = FALSE;
 	    break;
        }
	*bufPtr = c;
	bufPtr++;
    }
    if (failure) {
	Tcl_AppendResult (interp, "RAW handler: Error reading header line (",
                          buf, ")\n", NULL);
 	return FALSE;
    }
    return TRUE;
}

static Boln readHeader (Tcl_Interp *interp, tkimg_MFile *handle, RAWHEADER *th)
{
    char buf[HEADLEN];
    char tmpStr[HEADLEN];

    if (!readHeaderLine (interp, handle, buf) ||
        (1 != sscanf (buf, strMagic, th->id))) {
	Tcl_AppendResult (interp, "Unable to parse header field Magic\n", NULL);
	return FALSE;
    }
    if (strcmp (th->id, "RAW") != 0) {
	Tcl_AppendResult (interp, "Invalid value for header field Magic:",
                                  "Must be \"RAW\"\n", NULL);
	return FALSE;
    }

    if (!readHeaderLine (interp, handle, buf) ||
        (1 != sscanf (buf, strWidth, &th->width))) {
	Tcl_AppendResult (interp, "Unable to parse header field Width\n", NULL);
	return FALSE;
    }
    if (th->width < 1) {
	Tcl_AppendResult (interp, "Invalid value for header field Width:",
                                  "Must be greater than zero\n", NULL);
	return FALSE;
    }

    if (!readHeaderLine (interp, handle, buf) ||
        (1 != sscanf (buf, strHeight, &th->height))) {
	Tcl_AppendResult (interp, "Unable to parse header field Height\n", NULL);
	return FALSE;
    }
    if (th->height < 1) {
	Tcl_AppendResult (interp, "Invalid value for header field Height:",
                                  "Must be greater than zero\n", NULL);
	return FALSE;
    }

    if (!readHeaderLine (interp, handle, buf) ||
        (1 != sscanf (buf, strNumChan, &th->nChans))) {
	Tcl_AppendResult (interp, "Unable to parse header field NumChan\n", NULL);
	return FALSE;
    }
    if (th->nChans != 1 && th->nChans != 3) {
	Tcl_AppendResult (interp, "Invalid value for header field NumChan:",
                                  "Must be 1 or 3\n", NULL);
	return FALSE;
    }

    if (!readHeaderLine (interp, handle, buf) ||
        (1 != sscanf (buf, strByteOrder, tmpStr))) {
	Tcl_AppendResult (interp, "Unable to parse header field ByteOrder\n", NULL);
	return FALSE;
    }

    if (strcmp (tmpStr, strIntel) == 0) {
	th->byteOrder = INTEL;
    } else if (strcmp (tmpStr, strMotorola) == 0) {
	th->byteOrder = MOTOROLA;
    } else {
	Tcl_AppendResult (interp, "Invalid value for header field ByteOrder:",
                                  "Must be ", strIntel, " or ", strMotorola,
				  "\n", NULL);
	return FALSE;
    }

    if (!readHeaderLine (interp, handle, buf) ||
        (1 != sscanf (buf, strScanOrder, tmpStr))) {
	Tcl_AppendResult (interp, "Unable to parse header field ScanOrder\n", NULL);
	return FALSE;
    }
    if (strcmp (tmpStr, strTopDown) == 0) {
	th->scanOrder = TOP_DOWN;
    } else if (strcmp (tmpStr, strBottomUp) == 0) {
	th->scanOrder = BOTTOM_UP;
    } else {
	Tcl_AppendResult (interp, "Invalid value for header field ScanOrder:",
                                  "Must be ", strTopDown, " or ", strBottomUp,
                                  "\n", NULL);
	return FALSE;
    }

    if (!readHeaderLine (interp, handle, buf) ||
        (1 != sscanf (buf, strPixelType, tmpStr))) {
	Tcl_AppendResult (interp, "Unable to parse header field PixelType\n", NULL);
	return FALSE;
    }
    if (strcmp (tmpStr, strFloat) == 0) {
	th->pixelType = TYPE_FLOAT;
    } else if (strcmp (tmpStr, strUShort) == 0) {
	th->pixelType = TYPE_USHORT;
    } else if (strcmp (tmpStr, strUByte) == 0) {
	th->pixelType = TYPE_UBYTE;
    } else {
	Tcl_AppendResult (interp, "Invalid value for header field PixelType:",
                                  "Must be ", strFloat, ",", strUShort, " or ", strUByte,
                                  "\n", NULL);
	return FALSE;
    }

    return TRUE;
}

static Boln writeHeader (tkimg_MFile *handle, RAWHEADER *th)
{
    char buf[1024];

    sprintf (buf, strMagic, "RAW");
    tkimg_Write (handle, buf, strlen (buf));
    sprintf (buf, strWidth, th->width);
    tkimg_Write (handle, buf, strlen (buf));
    sprintf (buf, strHeight, th->height);
    tkimg_Write (handle, buf, strlen (buf));
    sprintf (buf, strNumChan, th->nChans);
    tkimg_Write (handle, buf, strlen (buf));
    sprintf (buf, strByteOrder, isIntel()? strIntel: strMotorola);
    tkimg_Write (handle, buf, strlen (buf));
    sprintf (buf, strScanOrder, th->scanOrder == TOP_DOWN?
                                strTopDown: strBottomUp);
    tkimg_Write (handle, buf, strlen (buf));
    sprintf (buf, strPixelType, (th->pixelType == TYPE_FLOAT?  strFloat:
				(th->pixelType == TYPE_USHORT? strUShort:
				(th->pixelType == TYPE_UBYTE?  strUByte:
			                                       strUnknown))));
    tkimg_Write (handle, buf, strlen (buf));
    return TRUE;
}

static void initHeader (RAWHEADER *th)
{
    th->id[0]     = 'R';
    th->id[1]     = 'A';
    th->id[2]     = 'W';
    th->nChans    = 1;
    th->width     = 128;
    th->height    = 128;
    th->scanOrder = TOP_DOWN;
    th->byteOrder = INTEL;
    th->pixelType = TYPE_UBYTE;
    return;
}

static Boln readFloatFile (tkimg_MFile *handle, Float *buf, Int width, Int height,
                           Int nchan, Boln swapBytes, Boln verbose,
                           Float minVals[], Float maxVals[])
{
    Int x, y, c;
    Float *bufPtr = buf;
    char  *line;

    #ifdef DEBUG_LOCAL
	printf ("readFloatFile: Width=%d Height=%d nchan=%d swapBytes=%s\n",
                 width, height, nchan, swapBytes? "yes": "no");
    #endif
    for (c=0; c<nchan; c++) {
	minVals[c] =  1.0E30;
	maxVals[c] = -1.0E30;
    }
    line = ckalloc (sizeof (Float) * nchan * width);

    for (y=0; y<height; y++) {
	if (!readFloatRow (handle, bufPtr, nchan * width, line, swapBytes))
	    return FALSE;
	for (x=0; x<width; x++) {
	    for (c=0; c<nchan; c++) {
		if (*bufPtr > maxVals[c]) maxVals[c] = *bufPtr;
		if (*bufPtr < minVals[c]) minVals[c] = *bufPtr;
		bufPtr++;
	    }
	}
    }
    if (verbose) {
	printf ("\tMinimum pixel values :");
	for (c=0; c<nchan; c++) {
	    printf (" %f", minVals[c]);
	}
	printf ("\n");
	printf ("\tMaximum pixel values :");
	for (c=0; c<nchan; c++) {
	    printf (" %f", maxVals[c]);
	}
	printf ("\n");
	fflush (stdout);
    }
    ckfree (line);
    return TRUE;
}

static Boln readUShortFile (tkimg_MFile *handle, UShort *buf, Int width, Int height,
                            Int nchan, Boln swapBytes, Boln verbose,
                            Float minVals[], Float maxVals[])
{
    Int    x, y, c;
    UShort *bufPtr = buf;
    char   *line;

    #ifdef DEBUG_LOCAL
	printf ("readUShortFile: Width=%d Height=%d nchan=%d swapBytes=%s\n",
                 width, height, nchan, swapBytes? "yes": "no");
    #endif
    for (c=0; c<nchan; c++) {
	minVals[c] =  1.0E30;
	maxVals[c] = -1.0E30;
    }
    line = ckalloc (sizeof (UShort) * nchan * width);

    for (y=0; y<height; y++) {
	if (!readUShortRow (handle, bufPtr, nchan * width, line, swapBytes))
	    return FALSE;
	for (x=0; x<width; x++) {
	    for (c=0; c<nchan; c++) {
		if (*bufPtr > maxVals[c]) maxVals[c] = *bufPtr;
		if (*bufPtr < minVals[c]) minVals[c] = *bufPtr;
		bufPtr++;
	    }
	}
    }
    if (verbose) {
	printf ("\tMinimum pixel values :");
	for (c=0; c<nchan; c++) {
	    printf (" %d", (UShort)minVals[c]);
	}
	printf ("\n");
	printf ("\tMaximum pixel values :");
	for (c=0; c<nchan; c++) {
	    printf (" %d", (UShort)maxVals[c]);
	}
	printf ("\n");
	fflush (stdout);
    }
    ckfree (line);
    return TRUE;
}

static Boln readUByteFile (tkimg_MFile *handle, UByte *buf, Int width, Int height,
                           Int nchan, Boln swapBytes, Boln verbose,
                           Float minVals[], Float maxVals[])
{
    Int   x, y, c;
    UByte *bufPtr = buf;
    char  *line;

    #ifdef DEBUG_LOCAL
	printf ("readUByteFile: Width=%d Height=%d nchan=%d swapBytes=%s\n",
                 width, height, nchan, swapBytes? "yes": "no");
    #endif
    for (c=0; c<nchan; c++) {
	minVals[c] =  1.0E30;
	maxVals[c] = -1.0E30;
    }
    line = ckalloc (sizeof (UByte) * nchan * width);

    for (y=0; y<height; y++) {
	if (!readUByteRow (handle, bufPtr, nchan * width, line, swapBytes))
	    return FALSE;
	for (x=0; x<width; x++) {
	    for (c=0; c<nchan; c++) {
		if (*bufPtr > maxVals[c]) maxVals[c] = *bufPtr;
		if (*bufPtr < minVals[c]) minVals[c] = *bufPtr;
		bufPtr++;
	    }
	}
    }
    if (verbose) {
	printf ("\tMinimum pixel values :");
	for (c=0; c<nchan; c++) {
	    printf (" %d", (UByte)minVals[c]);
	}
	printf ("\n");
	printf ("\tMaximum pixel values :");
	for (c=0; c<nchan; c++) {
	    printf (" %d", (UByte)maxVals[c]);
	}
	printf ("\n");
	fflush (stdout);
    }
    ckfree (line);
    return TRUE;
}

static Boln remapFloatValues (Float *buf, Int width, Int height, Int nchan,
                              Float minVals[], Float maxVals[])
{
    Int x, y, c;
    Float *bufPtr = buf;
    Float m[MAXCHANS], t[MAXCHANS];

    for (c=0; c<nchan; c++) {
	m[c] = (1.0 - 0.0) / (maxVals[c] - minVals[c]);
	t[c] = 0.0 - m[c] * minVals[c];
    }
    for (y=0; y<height; y++) {
	for (x=0; x<width; x++) {
	    for (c=0; c<nchan; c++) {
		*bufPtr = *bufPtr * m[c] + t[c];
		if (*bufPtr < 0.0) *bufPtr = 0.0;
		if (*bufPtr > 1.0) *bufPtr = 1.0;
	        bufPtr++;
	    }
	}
    }
    return TRUE;
}

static Boln remapUShortValues (UShort *buf, Int width, Int height, Int nchan,
                               Float minVals[], Float maxVals[])
{
    Int x, y, c;
    UShort *bufPtr = buf;
    Float m[MAXCHANS], t[MAXCHANS];

    for (c=0; c<nchan; c++) {
	m[c] = (65535.0 - 0.0) / (maxVals[c] - minVals[c]);
	t[c] = 0.0 - m[c] * minVals[c];
    }
    for (y=0; y<height; y++) {
	for (x=0; x<width; x++) {
	    for (c=0; c<nchan; c++) {
		*bufPtr = *bufPtr * m[c] + t[c];
	        bufPtr++;
	    }
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

static int ParseFormatOpts _ANSI_ARGS_((Tcl_Interp *interp, Tcl_Obj *format,
               FMTOPT *opts));
static int CommonMatch _ANSI_ARGS_((Tcl_Interp *interp, tkimg_MFile *handle,
               Tcl_Obj *format, int *widthPtr, int *heightPtr,
	       RAWHEADER *rawHeaderPtr));
static int CommonRead _ANSI_ARGS_((Tcl_Interp *interp, tkimg_MFile *handle,
	       const char *filename, Tcl_Obj *format,
	       Tk_PhotoHandle imageHandle, int destX, int destY,
	       int width, int height, int srcX, int srcY));
static int CommonWrite _ANSI_ARGS_((Tcl_Interp *interp,
	       const char *filename, Tcl_Obj *format,
	       tkimg_MFile *handle, Tk_PhotoImageBlock *blockPtr));

static int ParseFormatOpts (interp, format, opts)
    Tcl_Interp *interp;
    Tcl_Obj *format;
    FMTOPT *opts;
{
    static const char *const rawOptions[] = {
         "-verbose", "-width", "-height", "-nchan", "-byteorder",
         "-scanorder", "-pixeltype", "-min", "-max", "-gamma", "-useheader", "-nomap"
    };
    int objc, length, c, i, index;
    Tcl_Obj **objv;
    const char *widthStr, *heightStr, *nchanStr, *verboseStr, *useheaderStr,
         *byteOrderStr, *scanorderStr, *pixelTypeStr,
         *minStr, *maxStr, *gammaStr, *nomapStr;

    /* Initialize format options with default values. */
    verboseStr   = "0";
    byteOrderStr = isIntel ()? strIntel: strMotorola;
    widthStr     = "128";
    heightStr    = "128";
    nchanStr     = "1";
    scanorderStr = strTopDown;
    pixelTypeStr = strUByte;
    minStr       = "0.0";
    maxStr       = "0.0";
    gammaStr     = "1.0";
    useheaderStr = "1";
    nomapStr     = "0";

    if (tkimg_ListObjGetElements (interp, format, &objc, &objv) != TCL_OK)
	return TCL_ERROR;
    if (objc) {
	for (i=1; i<objc; i++) {
	    if (Tcl_GetIndexFromObj (interp, objv[i], (CONST84 char *CONST86 *)rawOptions,
		    "format option", 0, &index) != TCL_OK) {
		return TCL_ERROR;
	    }
	    if (++i >= objc) {
		Tcl_AppendResult (interp, "No value for option \"",
			Tcl_GetStringFromObj (objv[--i], (int *) NULL),
			"\"", (char *) NULL);
		return TCL_ERROR;
	    }
	    switch(index) {
		case 0:
		    verboseStr = Tcl_GetStringFromObj(objv[i], (int *) NULL);
		    break;
		case 1:
		    widthStr = Tcl_GetStringFromObj(objv[i], (int *) NULL);
		    break;
		case 2:
		    heightStr = Tcl_GetStringFromObj(objv[i], (int *) NULL);
		    break;
		case 3:
		    nchanStr = Tcl_GetStringFromObj(objv[i], (int *) NULL);
		    break;
		case 4:
		    byteOrderStr = Tcl_GetStringFromObj(objv[i], (int *) NULL);
		    break;
		case 5:
		    scanorderStr = Tcl_GetStringFromObj(objv[i], (int *) NULL);
		    break;
		case 6:
		    pixelTypeStr = Tcl_GetStringFromObj(objv[i], (int *) NULL);
		    break;
		case 7:
		    minStr = Tcl_GetStringFromObj(objv[i], (int *) NULL);
		    break;
		case 8:
		    maxStr = Tcl_GetStringFromObj(objv[i], (int *) NULL);
		    break;
		case 9:
		    gammaStr = Tcl_GetStringFromObj(objv[i], (int *) NULL);
		    break;
		case 10:
		    useheaderStr = Tcl_GetStringFromObj(objv[i], (int *) NULL);
		    break;
		case 11:
		    nomapStr = Tcl_GetStringFromObj(objv[i], (int *) NULL);
		    break;
	    }
	}
    }

    /* OPA TODO: Check for valid integer and float strings. */
    opts->width  = atoi (widthStr);
    opts->height = atoi (heightStr);
    opts->nchan  = atoi (nchanStr);

    opts->minVal = atof (minStr);
    opts->maxVal = atof (maxStr);
    opts->gamma  = atof (gammaStr);

    c = byteOrderStr[0]; length = strlen (byteOrderStr);
    if (!strncmp (byteOrderStr, strIntel, length)) {
	opts->byteOrder = INTEL;
    } else if (!strncmp (byteOrderStr, strMotorola, length)) {
	opts->byteOrder = MOTOROLA;
    } else {
	Tcl_AppendResult (interp, "Invalid byteorder mode \"", byteOrderStr,
			  "\": Should be ", strIntel, " or ", strMotorola,
			  (char *) NULL);
	return TCL_ERROR;
    }

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
	Tcl_AppendResult (interp, "invalid verbose mode \"", verboseStr,
			  "\": should be 1 or 0, on or off, true or false",
			  (char *) NULL);
	return TCL_ERROR;
    }

    c = useheaderStr[0]; length = strlen (useheaderStr);
    if (!strncmp (useheaderStr, "1", length) || \
	!strncmp (useheaderStr, "true", length) || \
	!strncmp (useheaderStr, "on", length)) {
	opts->useHeader = 1;
    } else if (!strncmp (useheaderStr, "0", length) || \
	!strncmp (useheaderStr, "false", length) || \
	!strncmp (useheaderStr, "off", length)) {
	opts->useHeader = 0;
    } else {
	Tcl_AppendResult (interp, "invalid useheader mode \"", useheaderStr,
			  "\": should be 1 or 0, on or off, true or false",
			  (char *) NULL);
	return TCL_ERROR;
    }

    c = nomapStr[0]; length = strlen (nomapStr);
    if (!strncmp (nomapStr, "1", length) || \
	!strncmp (nomapStr, "true", length) || \
	!strncmp (nomapStr, "on", length)) {
	opts->nomap = 1;
    } else if (!strncmp (nomapStr, "0", length) || \
	!strncmp (nomapStr, "false", length) || \
	!strncmp (nomapStr, "off", length)) {
	opts->nomap = 0;
    } else {
	Tcl_AppendResult (interp, "invalid nomap mode \"", nomapStr,
			  "\": should be 1 or 0, on or off, true or false",
			  (char *) NULL);
	return TCL_ERROR;
    }

    c = scanorderStr[0]; length = strlen (scanorderStr);
    if (!strncmp (scanorderStr, strTopDown, length)) {
	opts->scanOrder = TOP_DOWN;
    } else if (!strncmp (scanorderStr, strBottomUp, length)) {
	opts->scanOrder = BOTTOM_UP;
    } else {
	Tcl_AppendResult (interp, "invalid scanline order \"", scanorderStr,
			  "\": should be TopDown or BottomUp",
			  (char *) NULL);
	return TCL_ERROR;
    }

    c = pixelTypeStr[0]; length = strlen (pixelTypeStr);
    if (!strncmp (pixelTypeStr, strFloat, length)) {
	opts->pixelType = TYPE_FLOAT;
    } else if (!strncmp (pixelTypeStr, strUShort, length)) {
	opts->pixelType = TYPE_USHORT;
    } else if (!strncmp (pixelTypeStr, strUByte, length)) {
	opts->pixelType = TYPE_UBYTE;
    } else {
	Tcl_AppendResult (interp, "invalid pixel type \"", pixelTypeStr,
			  "\": should be float, short or byte",
			  (char *) NULL);
	return TCL_ERROR;
    }
    return TCL_OK;
}

static int ChnMatch (interp, chan, filename, format, widthPtr, heightPtr)
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

    return CommonMatch (interp, &handle, format, widthPtr, heightPtr, NULL);
}

static int ObjMatch (interp, data, format, widthPtr, heightPtr)
    Tcl_Interp *interp;
    Tcl_Obj *data;
    Tcl_Obj *format;
    int *widthPtr, *heightPtr;
{
    tkimg_MFile handle;

    tkimg_FixObjMatchProc (&interp, &data, &format, &widthPtr, &heightPtr);

    tkimg_ReadInit(data, 'M', &handle);
    return CommonMatch (interp, &handle, format, widthPtr, heightPtr, NULL);
}

static int CommonMatch (interp, handle, format, widthPtr, heightPtr, rawHeaderPtr)
    Tcl_Interp *interp;
    tkimg_MFile *handle;
    Tcl_Obj *format;
    int *widthPtr;
    int *heightPtr;
    RAWHEADER *rawHeaderPtr;
{
    RAWHEADER th;
    FMTOPT opts;

    initHeader (&th);

    if (ParseFormatOpts (interp, format, &opts) != TCL_OK) {
	return TCL_ERROR;
    }
    if (opts.useHeader) {
	if (!readHeader (interp, handle, &th))
	    return 0;
    } else {
	th.width  = opts.width;
	th.height = opts.height;
	th.nChans = opts.nchan;
	th.pixelType = opts.pixelType;
	th.scanOrder = opts.scanOrder;
	th.byteOrder = opts.byteOrder;
    }
    *widthPtr  = th.width;
    *heightPtr = th.height;
    if (rawHeaderPtr) {
	*rawHeaderPtr = th;
    }
    return 1;
}

static int ChnRead (interp, chan, filename, format, imageHandle,
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

    tkimg_ReadInit (data, 'M', &handle);
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
    myblock bl;
    Int x, y, c;
    Int fileWidth, fileHeight;
    Float minVals[MAXCHANS], maxVals[MAXCHANS];
    int stopY, outY, outWidth, outHeight;
    RAWFILE tf;
    FMTOPT opts;
    Boln swapBytes;
    Int  byteOrder;
    Int  scanOrder;
    Int  pixelType;
    Int  matte = 0;
    UByte  *pixbufPtr;
    Float  *floatBufPtr;
    UShort *ushortBufPtr;
    UByte  *ubyteBufPtr;
    Float  gtable[GTABSIZE];

    memset (&tf, 0, sizeof (RAWFILE));
    initHeader (&tf.th);

    CommonMatch (interp, handle, format, &fileWidth, &fileHeight, &tf.th);

    if (ParseFormatOpts (interp, format, &opts) != TCL_OK) {
	return TCL_ERROR;
    }

    gtableFloat (opts.gamma, gtable);

    if (opts.verbose)
	printImgInfo (&tf.th, &opts, filename, "Reading image:");

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

    byteOrder = opts.useHeader? tf.th.byteOrder: opts.byteOrder;
    scanOrder = opts.useHeader? tf.th.scanOrder: opts.scanOrder;
    pixelType = opts.useHeader? tf.th.pixelType: opts.pixelType;
    swapBytes = (( isIntel () && (byteOrder != INTEL)) ||
                 (!isIntel () && (byteOrder == INTEL)));

    switch (pixelType) {
        case TYPE_FLOAT: {
	    tf.floatBuf = (Float *)ckalloc (fileWidth*fileHeight*tf.th.nChans*sizeof (Float));
	    readFloatFile (handle, tf.floatBuf, fileWidth, fileHeight, tf.th.nChans,
			   swapBytes, opts.verbose, minVals, maxVals);
	    break;
        }
        case TYPE_USHORT: {
	    tf.ushortBuf = (UShort *)ckalloc (fileWidth*fileHeight*tf.th.nChans*sizeof (UShort));
	    readUShortFile (handle, tf.ushortBuf, fileWidth, fileHeight, tf.th.nChans,
			    swapBytes, opts.verbose, minVals, maxVals);
	    break;
        }
        case TYPE_UBYTE: {
	    tf.ubyteBuf = (UByte *)ckalloc (fileWidth*fileHeight*tf.th.nChans*sizeof (UByte));
	    readUByteFile (handle, tf.ubyteBuf, fileWidth, fileHeight, tf.th.nChans,
			    swapBytes, opts.verbose, minVals, maxVals);
	    break;
        }
    }
    if (opts.nomap) {
	for (c=0; c<tf.th.nChans; c++) {
	    minVals[c] = 0.0;
	    maxVals[c] = 255.0;
	}
    } else if (opts.minVal != 0.0 || opts.maxVal != 0.0) {
	for (c=0; c<tf.th.nChans; c++) {
	    minVals[c] = opts.minVal;
	    maxVals[c] = opts.maxVal;
	}
    }
    switch (pixelType) {
        case TYPE_FLOAT: {
	    remapFloatValues (tf.floatBuf, fileWidth, fileHeight, tf.th.nChans,
			      minVals, maxVals);
	    break;
        }
        case TYPE_USHORT: {
	    remapUShortValues (tf.ushortBuf, fileWidth, fileHeight, tf.th.nChans,
			       minVals, maxVals);
	    break;
        }
    }

    Tk_PhotoExpand (imageHandle, destX + outWidth, destY + outHeight);

    tf.pixbuf = (UByte *) ckalloc (fileWidth * tf.th.nChans);

    block.pixelSize = tf.th.nChans;
    block.pitch = fileWidth * tf.th.nChans;
    block.width = outWidth;
    block.height = 1;
    block.offset[0] = 0;
    block.offset[1] = (tf.th.nChans > 1? 1: 0);
    block.offset[2] = (tf.th.nChans > 1? 2: 0);
    block.offset[3] = (tf.th.nChans == 4 && matte? 3: 0);
    block.pixelPtr = tf.pixbuf + srcX * tf.th.nChans;

    stopY = srcY + outHeight;
    outY = destY;

    for (y=0; y<stopY; y++) {
	pixbufPtr = tf.pixbuf;
	switch (pixelType) {
	    case TYPE_FLOAT: {
		if (scanOrder == BOTTOM_UP) {
		    floatBufPtr = tf.floatBuf + (fileHeight -1 - y) * fileWidth * tf.th.nChans;
		} else {
		    floatBufPtr = tf.floatBuf + y * fileWidth * tf.th.nChans;
		}
		FloatGammaUByte (fileWidth * tf.th.nChans, floatBufPtr,
				 opts.gamma != 1.0? gtable: NULL, pixbufPtr);
		floatBufPtr += fileWidth * tf.th.nChans;
		break;
	    }
	    case TYPE_USHORT: {
		if (scanOrder == BOTTOM_UP) {
		    ushortBufPtr = tf.ushortBuf + (fileHeight -1 - y) * fileWidth * tf.th.nChans;
		} else {
		    ushortBufPtr = tf.ushortBuf + y * fileWidth * tf.th.nChans;
		}
		UShortGammaUByte (fileWidth * tf.th.nChans, ushortBufPtr,
				  opts.gamma != 1.0? gtable: NULL, pixbufPtr);
		ushortBufPtr += fileWidth * tf.th.nChans;
		break;
	    }
	    case TYPE_UBYTE: {
		if (scanOrder == BOTTOM_UP) {
		    ubyteBufPtr = tf.ubyteBuf + (fileHeight -1 - y) * fileWidth * tf.th.nChans;
		} else {
		    ubyteBufPtr = tf.ubyteBuf + y * fileWidth * tf.th.nChans;
		}
		for (x=0; x<fileWidth * tf.th.nChans; x++) {
		    pixbufPtr[x] = ubyteBufPtr[x];
		}
		ubyteBufPtr += fileWidth * tf.th.nChans;
		break;
	    }
	}
	if (y >= srcY) {
	    tkimg_PhotoPutBlock(interp, imageHandle, &block, destX, outY,
                                width, 1,
                                block.offset[3]?
                                TK_PHOTO_COMPOSITE_SET:
                                TK_PHOTO_COMPOSITE_OVERLAY);
	    outY++;
	}
    }
    rawClose (&tf);
    return TCL_OK;
}

static int ChnWrite (interp, filename, format, blockPtr)
    Tcl_Interp *interp;
    const char *filename;
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
    Int     x, y;
    Int     redOffset, greenOffset, blueOffset, alphaOffset;
    UByte   *pixelPtr, *rowPixPtr;
    RAWFILE tf;
    FMTOPT opts;
    UByte *ubyteBufPtr;
    Int bytesPerLine;

    memset (&tf, 0, sizeof (RAWFILE));
    if (ParseFormatOpts (interp, format, &opts) != TCL_OK) {
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

    initHeader (&tf.th);
    tf.th.width = blockPtr->width;
    tf.th.height = blockPtr->height;
    tf.th.nChans = opts.nchan;
    tf.th.scanOrder = opts.scanOrder;
    tf.th.pixelType = TYPE_UBYTE;

    writeHeader (handle, &tf.th);
    bytesPerLine = blockPtr->width * tf.th.nChans * sizeof (UByte);
    tf.ubyteBuf = (UByte *)ckalloc (bytesPerLine);

    rowPixPtr = blockPtr->pixelPtr + blockPtr->offset[0];
    for (y = 0; y < blockPtr->height; y++) {
	ubyteBufPtr = tf.ubyteBuf;
	pixelPtr = rowPixPtr;
	if (tf.th.nChans == 1) {
	    for (x=0; x<blockPtr->width; x++) {
		*ubyteBufPtr = pixelPtr[redOffset];
		ubyteBufPtr++;
		pixelPtr += blockPtr->pixelSize;
	    }
	} else {
	    for (x=0; x<blockPtr->width; x++) {
		*(ubyteBufPtr++) = pixelPtr[redOffset];
		*(ubyteBufPtr++) = pixelPtr[greenOffset];
		*(ubyteBufPtr++) = pixelPtr[blueOffset];
		if (tf.th.nChans == 4) {
		    /* Have a matte channel and write it. */
		    *(ubyteBufPtr++) = pixelPtr[alphaOffset];
		}
		pixelPtr += blockPtr->pixelSize;
	    }
	}
	if (tkimg_Write (handle, (char *)tf.ubyteBuf, bytesPerLine) != bytesPerLine) {
	    rawClose (&tf);
	    return TCL_ERROR;
	}
	rowPixPtr += blockPtr->pitch;
    }
    if (opts.verbose)
        printImgInfo (&tf.th, &opts, filename, "Saving image:");
    rawClose (&tf);
    return TCL_OK;
}

