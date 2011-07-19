/* STARTHEADER
 *
 * File :       dted.c
 *
 * Author :     Paul Obermeier (paul@poSoft.de)
 *
 * Date :       Tue Nov 20 21:24:26 CET 2001
 *
 * Copyright :  (C) 2001-2003 Paul Obermeier
 *
 * Description :
 *
 * A photo image handler for DTED elevation data interpreted as image files.
 *
 * The following image types are supported:
 *
 * Grayscale image: Load DTED data as grayscale image.
 *
 * List of currently supported features:
 *
 * Type   |     Read      |     Write     |
 *        | -file | -data | -file | -data |
 * ----------------------------------------
 * Gray   | Yes   | Yes   | No    | No   |
 *
 * The following format options are available:
 *
 * Read  DTED image: "dted -verbose <bool> -nchan <int> -nomap <bool>
 *                         -gamma <float> -min <float> -max <float>"
 *
 * -verbose <bool>:     If set to true, additional information about the file
 *                      format is printed to stdout. Default is false.
 * -nchan <int>:        Specify the number of channels of the generated image.
 *			Default is 1, i.e. generated a grayscale image.
 * -gamma <float>:      Specify a gamma correction to be applied when mapping
 *			the input data to 8-bit image values.
 *                      Default is 1.0.
 * -nomap <bool>:       If set to true, no mapping of input values is done.
 * 			Use this option, if your image already contains RGB
 * 			values in the range of 0 ..255.
 *                      Default is false.
 * -min <short>:        Specify the minimum pixel value to be used for mapping
 *			the input data to 8-bit image values.
 *                      Default is the minimum value found in the image data.
 * -max <short>:        Specify the maximum pixel value to be used for mapping
 *			the input data to 8-bit image values.
 *                      Default is the maximum value found in the image data.
 *
 * Notes:
 * 			Currently only reading DTED files as grayscale images
 *			is implemented. Color mapped images and writing will be
 *			implemented when needed.
 *			Syntax checking of DTED files is rudimentary, too.
 *			Only file reading tested right now.
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

#define strIntel    "Intel"
#define strMotorola "Motorola"

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

#define MAX_SHORT   32767
#define MIN_SHORT  -32768

#define ELEV_UNDEFINED -32000 /* All elevations smaller than this value are
			         considered undefined, and are set to the
				 minimum value. */

/* DTED file header structures */

typedef struct {
    Byte uhl_tag[3];        /* 'UHL' sentinel tag */
    Byte reserved1[1];
    Byte origin_long[8];    /* Longitude of origin */
    Byte origin_lat[8];     /* Latitude of origin */
    Byte ew_interval[4];    /* East-west data interval (tenths second) */
    Byte ns_interval[4];    /* North-south data interval (tenths second) */
    Byte accuracy[4];       /* Absolute vertical accuracy (meters) */
    Byte security[3];
    Byte reserved2[45];
} UHL_STRUCT;

typedef struct {
    Byte dsi_tag[3];              /* 'DSI' sentinel tag */
    Byte security_class[1];       /* Security classification */
    Byte security_mark[2];        /* Security control & release mark */
    Byte security_desc[27];       /* Security handling description */
    Byte reserved1[26];
    Byte level[5];                /* DMA series designator for level */
    Byte ref_num[15];             /* Reference number */
    Byte reserved2[8];
    Byte edition[2];              /* Data edition */
    Byte merge_version[1];        /* Match/merge version */
    Byte maintenance_date[4];     /* Maintenance date (YYMM) */
    Byte merge_date[4];           /* Match/Merge date (YYMM) */
    Byte maintenance_desc[4];     /* Maintenance description */
    Byte producer[8];             /* Producer */
    Byte reserved3[16];
    Byte product_num[9];          /* Product specification stock number */
    Byte product_change[2];       /* Product specification change number */
    Byte product_date[4];         /* Product specification date (YYMM) */
    Byte vertical_datum[3];       /* Vertical datum */
    Byte horizontal_datum[5];     /* Horizontal datum */
    Byte collection_sys[10];      /* Digitizing collection system */
    Byte compilation_date[4];     /* Compilation date (YYMM) */
    Byte reserved4[22];
    Byte origin_lat[9];           /* Latitude of data origin */
    Byte origin_long[10];         /* Longitude of data origin */
    Byte sw_corner_lat[7];        /* Latitude of SW corner */
    Byte sw_corner_long[8];       /* Longitude of SW corner */
    Byte nw_corner_lat[7];        /* Latitude of NW corner */
    Byte nw_corner_long[8];       /* Longitude of NW corner */
    Byte ne_corner_lat[7];        /* Latitude of NE corner */
    Byte ne_corner_long[8];       /* Longitude of NE corner */
    Byte se_corner_lat[7];        /* Latitude of SE corner */
    Byte se_corner_long[8];       /* Longitude of SE corner */
    Byte orientation[9];          /* Orientation angle */
    Byte ns_spacing[4];           /* North-south data spacing (tenths sec) */
    Byte ew_spacing[4];           /* East-west data spacing (tenths sec) */
    Byte rows[4];                 /* Number of data rows */
    Byte cols[4];                 /* Number of data cols */
    Byte cell_coverage[2];        /* Partial cell indicator */
    Byte reserved5[357];
} DSI_STRUCT;

typedef struct {
    Byte abs_horiz_acc[4];	/* Absolute horizontal accuracy (meters) */
    Byte abs_vert_acc[4];	/* Absolute vertical accuracy (meters) */
    Byte rel_horiz_acc[4];	/* Relative horizontal accuracy (meters) */
    Byte rel_vert_acc[4];	/* Relative vertical accuracy (meters) */
} ACCURACY_STRUCT;

typedef struct {
    Byte latitude[9];		/* Latitude */
    Byte longitude[10];		/* Longitude */
} COORD_STRUCT;

typedef struct {
    ACCURACY_STRUCT acc;	/* Accuracy of subregion */
    Byte no_coords[2];		/* Number of coordinates (03-14) */
    COORD_STRUCT coords[14];	/* Outline of subregion */
} SUBREGION_STRUCT;

typedef struct {
    Byte acc_tag[3];		     /* 'ACC' sentinel tag */
    ACCURACY_STRUCT global_acc;      /* Accuracy of product */
    Byte reserved1[36];
    Byte no_acc_subregions[2];	     /* Number of accuracy subregions
                                        (00 = no, 02-09) */
    SUBREGION_STRUCT subregions[9];  /* Accuracy subregions */
    Byte reserved2[87];
} ACC_STRUCT;

typedef struct {
    UHL_STRUCT uhl;
    DSI_STRUCT dsi;
    ACC_STRUCT acc;
} DTEDHEADER;

/* DTED file format options structure for use with ParseFormatOpts */
typedef struct {
    Int   nchan;
    Short minVal;
    Short maxVal;
    Float gamma;
    Boln  nomap;
    Boln  verbose;
} FMTOPT;

/* Structure to hold information about the DTED file being processed. */
typedef struct {
    DTEDHEADER th;
    UByte *pixbuf;
    Short *rawbuf;
} DTEDFILE;

#define MINGAMMA 0.01
#define MAXGAMMA 100.0
#define GTABSIZE 257

/* Given a pixel value in Float format, "fin", and a gamma-correction
lookup table, "ftab", macro "gcorrectFloat" returns the gamma-corrected pixel
value in "fout". */

#define gcorrectFloat(fin,ftab,fout)                                    \
    {                                                                   \
        Int     gc_i;                                                   \
        Float   gc_t;                                                   \
        gc_t = (fin) * (GTABSIZE - 2);                                  \
        gc_i = gc_t;                                                    \
        gc_t -= gc_i;                                                   \
        (fout) = (ftab)[gc_i] * (1.0-gc_t) + (ftab)[gc_i+1] * gc_t;     \
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
#ifdef DEBUG_LOCAL
	    printf ("gammatable[%d] = %f\n", i, table[i]);
#endif
    }
    table[GTABSIZE - 1] = 1.0;
#ifdef DEBUG_LOCAL
	printf ("gammatable[%d] = %f\n", GTABSIZE-1, table[GTABSIZE-1]);
#endif
    return TRUE;
}

static void gammaShortUByte (Int n, const Short s_in[],
                             const Float gtable[], UByte ub_out[])
{
    const Short *ssrc, *sstop;
    Float       ftmp;
    Int         itmp;
    UByte       *ubdest;

    ssrc = s_in;
    sstop = s_in + n;
    ubdest = ub_out;

    /* Handle a gamma value of 1.0 (gtable == NULL) as a special case.
       Quite nice speed improvement for the maybe most used case. */
    if (gtable) {
	while (ssrc < sstop) {
	    /* Map short values from the range [MIN_SHORT .. MAX_SHORT] to
	       the range [0.0 .. 1.0], do gamma correction and then map into
	       the displayable range [0 .. 255]. */
	    ftmp = (Float)(*ssrc * 1.0 / 65535.0  + 0.5);
	    gcorrectFloat (ftmp, gtable, ftmp);
	    itmp = (Int)(ftmp * 255.0 + 0.5);
	    *ubdest = MAX (0, MIN (itmp, 255));
#ifdef DEBUG_LOCAL
		printf ("Gamma %d --> %f --> %d --> %d\n",
		        *ssrc, ftmp, itmp, *ubdest);
#endif
	    ++ubdest;
	    ++ssrc;
	}
    } else {
	while (ssrc < sstop) {
	    /* Map short values from the range [MIN_SHORT .. MAX_SHORT] to
	       the displayable range [0 .. 255]. */
	    itmp = (Int)(*ssrc * 255.0 / 65535.0  + 128);
	    *ubdest = MAX (0, MIN (itmp, 255));
#ifdef DEBUG_LOCAL
		printf ("NoGamma %d --> %d --> %d\n", *ssrc, itmp, *ubdest);
#endif
	    ++ubdest;
	    ++ssrc;
	}
    }
    return;
}

/* This function determines at runtime, whether we are on an Intel system. */

static int isIntel (void)
{
    unsigned long val = 513;
    /* On Intel (little-endian) systems this value is equal to "\01\02\00\00".
       On big-endian systems this value equals "\00\00\02\01" */
    return memcmp(&val, "\01\02", 2) == 0;
}

static void dtedClose (DTEDFILE *tf)
{
    if (tf->pixbuf) ckfree ((char *)tf->pixbuf);
    if (tf->rawbuf) ckfree ((char *)tf->rawbuf);
    return;
}

/* Read 1 byte, representing an unsigned integer number. */

#if 0 /* unused */
static Boln readUByte (tkimg_MFile *handle, UByte *b)
{
    char buf[1];
    if (1 != tkimg_Read (handle, buf, 1))
        return FALSE;
    *b = buf[0];
    return TRUE;
}
#endif  /* unused */

/* Read 2 bytes, representing a signed 16 bit integer in the form
   <LowByte, HighByte>, from a file and convert them into the current
   machine's format. */

static Boln readShort (tkimg_MFile *handle, Short *s)
{
    char buf[2];
    if (2 != tkimg_Read (handle, buf, 2))
        return FALSE;
    *s = (buf[0] & 0xFF) | (buf[1] << 8);
    return TRUE;
}

/* Read 4 bytes, representing a signed 32 bit integer in the form
   <LowByte, HighByte>, from a file and convert them into the current
   machine's format. */

static Boln readInt (tkimg_MFile *handle, Int *i)
{
    char buf[4];
    if (4 != tkimg_Read (handle, buf, 4))
        return FALSE;
    *i = ((((Int)buf[0] & 0x000000FFU) << 24) | \
          (((Int)buf[1] & 0x0000FF00U) <<  8) | \
          (((Int)buf[2] & 0x00FF0000U) >>  8) | \
          (((Int)buf[3] & 0x0000FF00U) >> 24));
    return TRUE;
}

/* Write a byte, representing an unsigned integer to a file. */

#if 0 /* unused */
static Boln writeUByte (tkimg_MFile *handle, UByte b)
{
    UByte buf[1];
    buf[0] = b;
    if (1 != tkimg_Write (handle, (const char *)buf, 1))
        return FALSE;
    return TRUE;
}
#endif /* unused */

/* Write a byte, representing a signed integer to a file. */

#if 0 /* unused */
static Boln writeByte (tkimg_MFile *handle, Byte b)
{
    Byte buf[1];
    buf[0] = b;
    if (1 != tkimg_Write (handle, buf, 1))
        return FALSE;
    return TRUE;
}
#endif /* unused */

/* Convert a signed 16 bit integer number into the format
   <LowByte, HighByte> (an array of 2 bytes) and write the array to a file. */

#if 0 /* unused */
static Boln writeShort (tkimg_MFile *handle, Short s)
{
    Byte buf[2];
    buf[0] = s;
    buf[1] = s >> 8;
    if (2 != tkimg_Write (handle, buf, 2))
        return FALSE;
    return TRUE;
}
#endif /* unused */

/* Convert a unsigned 16 bit integer number into the format
   <LowByte, HighByte> (an array of 2 bytes) and write the array to a file. */

#if 0 /* unused */
static Boln writeUShort (tkimg_MFile *handle, UShort s)
{
    Byte buf[2];
    buf[0] = s;
    buf[1] = s >> 8;
    if (2 != tkimg_Write (handle, buf, 2))
        return FALSE;
    return TRUE;
}
#endif /* unused */

#define OUT Tcl_WriteChars (outChan, str, -1)
static void printImgInfo (DTEDHEADER *th, FMTOPT *opts,
                          const char *filename, const char *msg)
{
    Tcl_Channel outChan;
    char str[256];

    outChan = Tcl_GetStdChannel (TCL_STDOUT);
    if (!outChan) {
        return;
    }
    sprintf (str, "%s\n", msg);                                              OUT;
    sprintf (str, "\tLongitude of origin  : %.8s\n", th->uhl.origin_long);   OUT;
    sprintf (str, "\tLatitude of origin   : %.8s\n", th->uhl.origin_lat);    OUT;
    sprintf (str, "\tEast-West interval   : %.4s\n", th->uhl.ew_interval);   OUT;
    sprintf (str, "\tNorth-South interval : %.4s\n", th->uhl.ns_interval);   OUT;
    sprintf (str, "\tVertical accuracy    : %.4s\n", th->uhl.accuracy);      OUT;
    sprintf (str, "\tSecurity Code        : %.3s\n", th->uhl.security);      OUT;
    sprintf (str, "\tDTED level           : %.5s\n", th->dsi.level);         OUT;
    sprintf (str, "\tNumber of rows       : %.4s\n", th->dsi.rows);          OUT;
    sprintf (str, "\tNumber of columns    : %.4s\n", th->dsi.cols);          OUT;
    sprintf (str, "\tCell coverage        : %.2s\n", th->dsi.cell_coverage); OUT;
    sprintf (str, "\tNo. of channels      : %d\n", opts->nchan);             OUT;
    sprintf (str, "\tGamma correction     : %f\n", opts->gamma);             OUT;
    sprintf (str, "\tMinimum map value    : %d\n", opts->minVal);            OUT;
    sprintf (str, "\tMaximum map value    : %d\n", opts->maxVal);            OUT;
    sprintf (str, "\tHost byte order      : %s\n", isIntel ()?
                                                   strIntel: strMotorola);   OUT;
    Tcl_Flush (outChan);
}
#undef OUT
static Boln readHeader (tkimg_MFile *handle, DTEDHEADER *th)
{
    if (sizeof (DTEDHEADER) != tkimg_Read (handle, (char *)th, sizeof(DTEDHEADER))) {
        return FALSE;
    }
    if (strncmp ((char *)th->uhl.uhl_tag, "UHL", 3) != 0) {
	return FALSE;
    }

    /* OPA: More tests to follow. */
    return TRUE;
}

#if 0 /* unused */
static Boln writeHeader (tkimg_MFile *handle, DTEDHEADER *th)
{
    return TRUE;
}
#endif /* unused */

#if 0 /* unused */
static void initHeader (DTEDHEADER *th)
{
    th->uhl.uhl_tag[0] = 'U';
    th->uhl.uhl_tag[1] = 'H';
    th->uhl.uhl_tag[2] = 'L';
    /* OPA: More to follow for DTED writing */
    return;
}
#endif /* unused */

static Boln readDtedColumn (tkimg_MFile *handle, Short *pixels, Int nRows,
                            Int nCols, Int curCol, char *buf, Boln hostIsIntel)
{
    Int   i, nBytes;
    Short *mPtr;
    char  *bufPtr = buf;
    Short meridian, parallel;
    Int   block_count;
    UByte *cp;
    Int  checksum, checksum1 = 0;
    UByte sentinel;

    /* Read data column header. */
    if (!readInt   (handle, &block_count) ||
        !readShort (handle, &meridian) ||
        !readShort (handle, &parallel)) {
        printf ("Error reading column header\n");
	return FALSE;
    }

    /* Calculate checksum, part 1 */
    cp = (UByte *) &block_count;
    checksum1 += cp[0] + cp[1] + cp[2] + cp[3];
    cp = (UByte *) &meridian;
    checksum1 += cp[0] + cp[1];
    cp = (UByte *) &parallel;
    checksum1 += cp[0] + cp[1];

    if (hostIsIntel) {
	sentinel = (UByte) ((block_count & 0xff000000) >> 24);
	block_count = block_count & 0x00ffffff;
    } else {
	sentinel = (UByte) (block_count & 0x000000ff);
	block_count = (block_count & 0xffffff00) >> 8;
    }

    /* Read the elevation data into the supplied column buffer "buf". */
    nBytes = sizeof (Short) * nRows;
    if (nBytes != tkimg_Read (handle, buf, nBytes)) {
        printf ("Error reading elevation data\n");
        return FALSE;
    }

    /* Copy (and swap bytes, if needed) from the column buffer into the
       pixel array (shorts) . */
    if (hostIsIntel) {
        for (i=0; i<nRows; i++) {
	    mPtr = pixels + (i * nCols) + curCol;
	    ((char *)mPtr)[0] = bufPtr[1];
            ((char *)mPtr)[1] = bufPtr[0];
            bufPtr += sizeof (Short);
        }
    } else {
        for (i=0; i<nRows; i++) {
	    mPtr = pixels + (i * nCols) + curCol;
	    ((char *)mPtr)[0] = bufPtr[0];
            ((char *)mPtr)[1] = bufPtr[1];
            bufPtr += sizeof (Short);
        }
    }

    /* Read the checksum */
    if (!readInt (handle, &checksum)) {
        printf ("Error reading checksum\n");
	return FALSE;
    }

    /* Calculate checksum, part 2. OPA TODO Incorrect  */
    cp = (UByte *) pixels;
    for (i=0; i<nRows*2; i++, cp++) {
	checksum1 += *cp;
    }

    if (checksum != checksum1) {
	/* printf ("DTED Checksum Error (%d vs. %d).\n", checksum, checksum1); */
        /* return FALSE; */
    }
    return TRUE;
}

static Boln readDtedFile (tkimg_MFile *handle, Short *buf, Int width, Int height,
                          Int nchan, Boln hostIsIntel, Boln verbose,
                          Short minVals[], Short maxVals[])
{
    Int x, y, c;
    Short *bufPtr = buf;
    char  *colBuf;

#ifdef DEBUG_LOCAL
	printf ("readDtedFile: Width=%d Height=%d nchan=%d hostIsIntel=%s\n",
                 width, height, nchan, hostIsIntel? "yes": "no");
#endif
    for (c=0; c<nchan; c++) {
	minVals[c] =  MAX_SHORT;
	maxVals[c] =  MIN_SHORT;
    }
    colBuf = ckalloc (sizeof (Short) * nchan * height);

    /* Read the elevation data column by column. */
    for (x=0; x<width; x++) {
	if (!readDtedColumn (handle, buf, height, width,
	                     x, colBuf, hostIsIntel)) {
	    return FALSE;
	}
    }

    /* Loop through the elevation data and find minimum and maximum values.
       Ignore elevation values equal to -32767, because these indicate, no
       elevation data available. See also function remapShortValues.
       Note: We extend the range of undefined elevations to all values
       smaller than ELEV_UNDEFINED, because of DTED files not fully
       compliant to the specification. */
    bufPtr = buf;
    for (x=0; x<width; x++) {
	for (y=0; y<height; y++) {
	    for (c=0; c<nchan; c++) {
		if ( *bufPtr >= ELEV_UNDEFINED ) {
		    if (*bufPtr > maxVals[c]) maxVals[c] = *bufPtr;
		    if (*bufPtr < minVals[c]) minVals[c] = *bufPtr;
		}
		bufPtr++;
	    }
	}
    }
    if (verbose) {
	printf ("\tMinimum pixel values :");
	for (c=0; c<nchan; c++) {
	    printf (" %d", minVals[c]);
	}
	printf ("\n");
	printf ("\tMaximum pixel values :");
	for (c=0; c<nchan; c++) {
	    printf (" %d", maxVals[c]);
	}
	printf ("\n");
	fflush (stdout);
    }
    ckfree (colBuf);
    return TRUE;
}

/* Map the original short values into the range [MIN_SHORT .. MAX_SHORT].
   We must take care of values equal to -32767, which indicate that no
   elevation data is available. So we map this value to the minimum value.
   See also function readDtedFile. */

static Boln remapShortValues (Short *buf, Int width, Int height, Int nchan,
                              Short minVals[], Short maxVals[])
{
    Int   x, y, c;
    Int   tmpInt;
    Short tmpShort;
    Short *bufPtr = buf;
    Float m[MAXCHANS], t[MAXCHANS];

    for (c=0; c<nchan; c++) {
	m[c] = (Float)(MAX_SHORT - MIN_SHORT) /
	       (Float)(maxVals[c] - minVals[c]);
	t[c] = MIN_SHORT - m[c] * minVals[c];
    }
    for (y=0; y<height; y++) {
	for (x=0; x<width; x++) {
	    for (c=0; c<nchan; c++) {
		tmpShort = (*bufPtr >= ELEV_UNDEFINED? *bufPtr: minVals[c]);
		tmpInt = (Int)(tmpShort * m[c] + t[c] + 0.5);
#ifdef DEBUG_LOCAL
		    printf ("Remap %d --> %d --> %d --> ",
		             *bufPtr, tmpShort, tmpInt);
#endif
		if (tmpInt < MIN_SHORT) {
		    *bufPtr = MIN_SHORT;
		} else if (tmpInt > MAX_SHORT) {
		    *bufPtr = MAX_SHORT;
		} else {
		    *bufPtr = tmpInt;
		}
#ifdef DEBUG_LOCAL
		    printf ("%d (%f %f)\n", *bufPtr, m[c], t[c]);
#endif
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

static int ParseFormatOpts(Tcl_Interp *interp, Tcl_Obj *format,
	FMTOPT *opts);
static int CommonMatch(Tcl_Interp *interp, tkimg_MFile *handle,
	Tcl_Obj *format, int *widthPtr, int *heightPtr,
	DTEDHEADER *dtedHeaderPtr);
static int CommonRead(Tcl_Interp *interp, tkimg_MFile *handle,
	const char *filename, Tcl_Obj *format,
	Tk_PhotoHandle imageHandle, int destX, int destY,
	int width, int height, int srcX, int srcY);
static int CommonWrite(Tcl_Interp *interp,
	const char *filename, Tcl_Obj *format,
	tkimg_MFile *handle, Tk_PhotoImageBlock *blockPtr);

static int ParseFormatOpts (interp, format, opts)
    Tcl_Interp *interp;
    Tcl_Obj *format;
    FMTOPT *opts;
{
    static const char *const dtedOptions[] = {
         "-verbose", "-nchan", "-min", "-max", "-gamma", "-nomap"
    };
    int objc, length, c, i, index;
    Tcl_Obj **objv;
    const char *nchanStr, *verboseStr, *minStr, *maxStr, *gammaStr, *nomapStr;

    /* Initialize format options with default values. */
    verboseStr   = "0";
    nchanStr     = "1";
    minStr       = "0.0";
    maxStr       = "0.0";
    gammaStr     = "1.0";
    nomapStr     = "0";

    if (tkimg_ListObjGetElements (interp, format, &objc, &objv) != TCL_OK)
	return TCL_ERROR;
    if (objc) {
	for (i=1; i<objc; i++) {
	    if (Tcl_GetIndexFromObj (interp, objv[i], (CONST84 char *CONST86 *)dtedOptions,
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
		    nchanStr = Tcl_GetStringFromObj(objv[i], (int *) NULL);
		    break;
		case 2:
		    minStr = Tcl_GetStringFromObj(objv[i], (int *) NULL);
		    break;
		case 3:
		    maxStr = Tcl_GetStringFromObj(objv[i], (int *) NULL);
		    break;
		case 4:
		    gammaStr = Tcl_GetStringFromObj(objv[i], (int *) NULL);
		    break;
		case 5:
		    nomapStr = Tcl_GetStringFromObj(objv[i], (int *) NULL);
		    break;
	    }
	}
    }

    /* OPA TODO: Check for valid integer and float strings. */
    opts->nchan  = atoi (nchanStr);
    opts->minVal = atoi (minStr);
    opts->maxVal = atoi (maxStr);
    opts->gamma  = atof (gammaStr);

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

    return CommonMatch(interp, &handle, format, widthPtr, heightPtr, NULL);
}

static int ObjMatch(
    Tcl_Obj *data,
    Tcl_Obj *format,
    int *widthPtr,
    int *heightPtr,
    Tcl_Interp *interp
) {
    tkimg_MFile handle;

    tkimg_ReadInit(data, 'U', &handle);
    return CommonMatch (interp, &handle, format, widthPtr, heightPtr, NULL);
}

static int CommonMatch (interp, handle, format,
                        widthPtr, heightPtr, dtedHeaderPtr)
    Tcl_Interp *interp;
    tkimg_MFile *handle;
    Tcl_Obj *format;
    int *widthPtr;
    int *heightPtr;
    DTEDHEADER *dtedHeaderPtr;
{
    DTEDHEADER th;
    FMTOPT opts;
    Int nRows, nCols;

    if (ParseFormatOpts (interp, format, &opts) != TCL_OK) {
	return TCL_ERROR;
    }

    if (!readHeader (handle, &th)) {
	return 0;
    }
    sscanf (th.dsi.rows, "%4d", &nRows);
    sscanf (th.dsi.cols, "%4d", &nCols);
    *widthPtr  = nCols;
    *heightPtr = nRows;
    if (dtedHeaderPtr)
	*dtedHeaderPtr = th;
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

    tkimg_ReadInit (data, 'U', &handle);
    return CommonRead (interp, &handle, "InlineData", format, imageHandle,
                       destX, destY, width, height, srcX, srcY);
}

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
	Tk_PhotoImageBlock block;
    Int y, c;
    Int fileWidth, fileHeight;
    Short minVals[MAXCHANS], maxVals[MAXCHANS];
    int stopY, outY, outWidth, outHeight;
    DTEDFILE tf;
    FMTOPT   opts;
    Boln hostIsIntel;
    Int matte = 0;
    UByte *pixbufPtr;
    Short *rawbufPtr;
    Float gtable[GTABSIZE];

    memset (&tf, 0, sizeof (DTEDFILE));
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

    hostIsIntel = isIntel ();

    tf.rawbuf = (Short *)ckalloc (fileWidth * fileHeight *
                                  opts.nchan * sizeof (Short));
    readDtedFile (handle, tf.rawbuf, fileWidth, fileHeight, opts.nchan,
                  hostIsIntel, opts.verbose, minVals, maxVals);
    if (opts.nomap) {
	for (c=0; c<opts.nchan; c++) {
	    minVals[c] = 0;
	    maxVals[c] = 255;
	}
    } else if (opts.minVal != 0 || opts.maxVal != 0) {
	for (c=0; c<opts.nchan; c++) {
	    minVals[c] = opts.minVal;
	    maxVals[c] = opts.maxVal;
	}
    }
    remapShortValues (tf.rawbuf, fileWidth, fileHeight, opts.nchan,
                      minVals, maxVals);

    if (tkimg_PhotoExpand(interp, imageHandle, destX + outWidth, destY + outHeight) == TCL_ERROR) {
        dtedClose(&tf);
    	return TCL_ERROR;
    }

    tf.pixbuf = (UByte *) ckalloc (fileWidth * opts.nchan);

    block.pixelSize = opts.nchan;
    block.pitch = fileWidth * opts.nchan;
    block.width = outWidth;
    block.height = 1;
    block.offset[0] = 0;
    block.offset[1] = (opts.nchan > 1? 1: 0);
    block.offset[2] = (opts.nchan > 1? 2: 0);
    block.offset[3] = (opts.nchan == 4 && matte? 3: 0);
    block.pixelPtr = tf.pixbuf + srcX * opts.nchan;

    stopY = srcY + outHeight;
    outY = destY;

    for (y=0; y<stopY; y++) {
	pixbufPtr = tf.pixbuf;
	rawbufPtr = tf.rawbuf + (fileHeight - 1 - y) * fileWidth * opts.nchan;
	gammaShortUByte (fileWidth * opts.nchan, rawbufPtr,
                         opts.gamma != 1.0? gtable: NULL, pixbufPtr);
	rawbufPtr += fileWidth * opts.nchan;
	if (y >= srcY) {
	    if (tkimg_PhotoPutBlock(interp, imageHandle, &block, destX, outY,
                width, 1, TK_PHOTO_COMPOSITE_OVERLAY) == TCL_ERROR) {
                dtedClose(&tf);
                return TCL_ERROR;
        }
	    outY++;
	}
    }
    dtedClose(&tf);
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

static int StringWrite(
    Tcl_Interp *interp,
    Tcl_Obj *format,
    Tk_PhotoImageBlock *blockPtr
) {
    tkimg_MFile handle;
    int result;
    Tcl_DString data;

    Tcl_DStringInit(&data);
    tkimg_WriteInit (&data, &handle);
    result = CommonWrite (interp, "InlineData", format, &handle, blockPtr);
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
    return TCL_OK;
}
