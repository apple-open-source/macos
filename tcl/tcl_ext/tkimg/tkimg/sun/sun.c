/* STARTHEADER
 *
 * File :       sun.c
 *
 * Author :     Paul Obermeier (paul@poSoft.de)
 *
 * Date :       Mon Jan 22 21:32:48 CET 2001
 *
 * Copyright :  (C) 2001-2002 Paul Obermeier
 *
 * Description :
 *
 * A photo image handler for SUN's Raster file format.
 *
 * The following image types are supported:
 *
 *  1-bit pixels: Black and White.
 *  8-bit pixels: Grayscale or indexed.
 * 24-bit pixels: True-color (RGB, each channel 8 bit).
 * 32-bit pixels: True-color with alpha channel (RGBA, each channel 8 bit).
 *
 * List of currently supported features:
 *
 * Type   |     Read      |     Write     |
 *        | -file | -data | -file | -data |
 * ----------------------------------------
 *  1-bit | Yes   | Yes   | No    | No    |
 *  8-bit | Yes   | Yes   | No    | No    |
 * 24-bit | Yes   | Yes   | Yes   | Yes   |
 * 32-bit | Yes   | Yes   | Yes   | Yes   |
 *
 * All images types may be either uncompressed or run-length encoded.
 *
 *
 * The following format options are available:
 *
 * Read  SUN image: "sun -matte <bool> -verbose <bool>"
 * Write SUN image: "sun -matte <bool> -verbose <bool> -compression <type>"
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
 * - The "UNIX" encoding of SUN's "imagetool" is not supported.
 *
 * - Part of this code was taken from the "sunras" GIMP plugin:
 *
 *  >> The GIMP -- an image manipulation program
 *  >> Copyright (C) 1995 Spencer Kimball and Peter Mattis
 *  >> SUN raster reading and writing code Copyright (C) 1996 Peter Kirchgessner
 *  >> (email: pkirchg@aol.com, WWW: http://members.aol.com/pkirchg)
 *
 * ENDHEADER
 *
 * $Id: sun.c 152 2008-10-22 08:49:40Z nijtmans $
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
typedef unsigned char  Boln;	/* Boolean value: TRUE or FALSE */
typedef char           Byte;	/* Signed    8 bit integer */
typedef unsigned char  UByte;	/* Unsigned  8 bit integer */
typedef unsigned short UShort;	/* Unsigned 16 bit integer */
typedef unsigned int   UInt;	/* Unsigned 32 bit integer */

/* SunRaster magic number */
#define RAS_MAGIC 0x59a66a95

/* Supported SunRaster types */
#define RAS_TYPE_STD 1    /* Standard uncompressed format */
#define RAS_TYPE_RLE 2    /* Runlength compression format */

/* The SunRaster header structure */
typedef struct {
  UInt ras_magic;    /* Magic Number */
  UInt ras_width;    /* Width */
  UInt ras_height;   /* Height */
  UInt ras_depth;    /* Number of bits per pixel (1,8,24,32) */
  UInt ras_length;   /* Length of image data (but may also be 0) */
  UInt ras_type;     /* Encoding */
  UInt ras_maptype;  /* Type of colormap */
  UInt ras_maplength;/* Number of bytes for colormap */
} SUNHEADER;

/* Buffer for run-length encoding and decoding. */
typedef struct {
    int val;   /* The value that is to be repeated */
    int n;     /* How many times it is repeated */
} RLEBUF;

/* Forward declarations of static functions. */
/* This function is commented out because it is not used anywhere
static void byte2bit (UByte *byteline, int width, UByte *bitline, int invert);
*/

static void rle_startread (tkimg_MFile *ifp);
static int rle_fread (char *ptr, int sz, int nelem, tkimg_MFile *ifp);
static int sun_fread (char *ptr, int sz, int nelem, tkimg_MFile *ifp);
static int rle_fgetc (tkimg_MFile *ifp);
static int sun_getc (tkimg_MFile *ifp);
#define rle_getc(fp) ((rlebuf.n > 0) ? (rlebuf.n)--,rlebuf.val : rle_fgetc (fp))

static void rle_startwrite (tkimg_MFile *ofp);
/* This function is commented out because it is not used anywhere
static int rle_fwrite (char *ptr, int sz, int nelem, tkimg_MFile *ofp);
*/
static int rle_fputc (int val, tkimg_MFile *ofp);
static int rle_putrun (int n, int val, tkimg_MFile *ofp);
static void rle_endwrite (tkimg_MFile *ofp);

static Boln read_sun_header  (tkimg_MFile *ifp, SUNHEADER *sunhdr);
static Boln write_sun_header (tkimg_MFile *ofp, SUNHEADER *sunhdr);
static Boln read_sun_cols  (tkimg_MFile *ifp, SUNHEADER *sunhdr, UByte *colormap);
/* This function is commented out because it is not used anywhere
static Boln write_sun_cols (tkimg_MFile *ofp, SUNHEADER *sunhdr, UByte *colormap);
*/

static RLEBUF
	rlebuf;

/* OPA TODO: Change from ANSI-C arguments to _ANSI_ARGS_ macro. */

#if defined (DEBUG_LOCAL)
static Boln readUByte (tkimg_MFile *handle, UByte *b)
{
    char buf[1];
    if (1 != tkimg_Read(handle, buf, 1))
        return FALSE;
    *b = (UByte) buf[0];
    return TRUE;
}
#else
    /* Use this macro for better performance, esp. when reading RLE files. */
    #define readUByte(h,b) (1 == tkimg_Read((h),(char *)(b),1))
#endif

static Boln writeUByte (tkimg_MFile *handle, UByte b)
{
    UByte buf[1];
    buf[0] = b;
    if (1 != tkimg_Write(handle, (const char *)buf, 1))
        return FALSE;
    return TRUE;
}

static Boln readUInt (tkimg_MFile *ifp, UInt *i)
{
    UByte buf[4];
    UInt  c;

    if (4 != tkimg_Read(ifp, (char *)buf, 4)) {
	return FALSE;
    }

    c  = (((UInt)(buf[0])) << 24);
    c |= (((UInt)(buf[1])) << 16);
    c |= (((UInt)(buf[2])) << 8);
    c |=  ((UInt)(buf[3]));
    *i = c;
    return TRUE;
}

static Boln writeUInt (tkimg_MFile *ofp, UInt c)
{
    UByte buf[4];

    buf[0] = (c >> 24) & 0xff;
    buf[1] = (c >> 16) & 0xff;
    buf[2] = (c >>  8) & 0xff;
    buf[3] = (c      ) & 0xff;
    if (4 != tkimg_Write(ofp, (const char *)buf, 4)) {
	return FALSE;
    }
    return TRUE;
}

/* Convert n bytes of 0/1 to a line of bits */
/* This function is commented out because it is not used anywhere
static void byte2bit (UByte *byteline, int width,
                      UByte *bitline, int invert)
{
    UByte bitval;
    UByte rest[8];

    while (width >= 8) {
	bitval = 0;
        if (*(byteline++)) bitval |= 0x80;
        if (*(byteline++)) bitval |= 0x40;
        if (*(byteline++)) bitval |= 0x20;
        if (*(byteline++)) bitval |= 0x10;
        if (*(byteline++)) bitval |= 0x08;
        if (*(byteline++)) bitval |= 0x04;
        if (*(byteline++)) bitval |= 0x02;
        if (*(byteline++)) bitval |= 0x01;
        *(bitline++) = invert ? ~bitval : bitval;
        width -= 8;
    }
    if (width > 0) {
       memset (rest, 0, 8);
       memcpy (rest, byteline, width);
       bitval = 0;
       byteline = rest;
       if (*(byteline++)) bitval |= 0x80;
       if (*(byteline++)) bitval |= 0x40;
       if (*(byteline++)) bitval |= 0x20;
       if (*(byteline++)) bitval |= 0x10;
       if (*(byteline++)) bitval |= 0x08;
       if (*(byteline++)) bitval |= 0x04;
       if (*(byteline++)) bitval |= 0x02;
       *bitline = invert ? ~bitval : bitval;
    }
}
*/

/* Start reading Runlength Encoded Data */
static void rle_startread (tkimg_MFile *ifp)
{
    (void) ifp;
    rlebuf.val = rlebuf.n = 0;
}

/* Read pixels from RLE-stream */
static int rle_fread (char *ptr, int sz, int nelem, tkimg_MFile *ifp)
{
    int elem_read, cnt, val, err = 0;

    for (elem_read = 0; elem_read < nelem; elem_read++)
    {
        for (cnt = 0; cnt < sz; cnt++)
	{
	    val = rle_getc (ifp);
	    if (val < 0) {
                err = 1;
                break;
	    }
	    *(ptr++) = (char)val;
        }
        if (err)
	    break;
    }
    return elem_read;
}

/* Read uncompressed pixels from input stream "ifp" */
static int sun_fread (char *ptr, int sz, int nelem, tkimg_MFile *ifp)
{
    if (nelem*sz != tkimg_Read(ifp, ptr, nelem*sz)) {
	return -1;
    }
    return nelem;
}

/* Get one pixel from RLE-stream */
static int rle_fgetc (tkimg_MFile *ifp)
{
    UByte flag, runcnt, runval;

    if (rlebuf.n > 0)    /* Something in the buffer ? */
    {
       (rlebuf.n)--;
       return rlebuf.val;
    }

    /* Nothing in the buffer. We have to read something */
    if (!readUByte (ifp, &flag))
	return -1;
    if (flag != 0x80) return flag;    /* Single byte run ? */

    if (!readUByte (ifp, &runcnt))
	return -1;
    if (runcnt == 0) return (0x80);     /* Single 0x80 ? */

    /* The run */
    if (!readUByte (ifp, &runval))
	return -1;
    rlebuf.n = runcnt;
    rlebuf.val = runval;
    return runval;
}

/* Read one byte from input stream "ifp" */
static int sun_getc (tkimg_MFile *ifp)
{
    UByte val;
    if (!readUByte (ifp, &val))
	return -1;
    return val;
}

/* Start writing Runlength Encoded Data */
static void rle_startwrite (tkimg_MFile *ofp)
{
    (void) ofp;
    rlebuf.val = 0;
    rlebuf.n   = 0;
}

/* Write uncompressed elements to RLE-stream */
/* This function is commented out because it is not used anywhere
static int rle_fwrite (char *ptr, int sz, int nelem, tkimg_MFile *ofp)
{
    int elem_write, cnt, val, err = 0;
    UByte *pixels = (UByte *)ptr;

    for (elem_write = 0; elem_write < nelem; elem_write++) {
	for (cnt = 0; cnt < sz; cnt++) {
	    val = rle_fputc (*(pixels++), ofp);
	    if (val < 0) {
		err = 1;
		break;
	    }
        }
        if (err)
	    break;
    }
    return elem_write;
}
*/

/* Write uncompressed character to RLE-stream */
static int rle_fputc (int val, tkimg_MFile *ofp)
{
    int retval;

    if (rlebuf.n == 0) {   /* Nothing in the buffer ? Save the value */
	rlebuf.n   = 1;
	rlebuf.val = val;
	return val;
    }

    /* Something in the buffer */
    if (rlebuf.val == val) {   /* Same value in the buffer ? */
	rlebuf.n++;
	if (rlebuf.n == 257) { /* Can not be encoded in a single run ? */
	    retval = rle_putrun (256, rlebuf.val, ofp);
	    if (retval < 0)
                return retval;
	    rlebuf.n -= 256;
	}
	return val;
    }

    /* Something different in the buffer ? Write out the run */
    retval = rle_putrun (rlebuf.n, rlebuf.val, ofp);
    if (retval < 0)
        return retval;

    /* Save the new value */
    rlebuf.n = 1;
    return (rlebuf.val = val);
}

/* Write out a run with 0 < n < 257 */
static int rle_putrun (int n, int val, tkimg_MFile *ofp)
{
    int retval = 1,
        flag   = 0x80;

    /* Useful to write a 3 byte run ? */
    if ((n > 2) || ((n == 2) && (val == flag))) {
	if (!writeUByte (ofp, flag) ||
	    !writeUByte (ofp, n-1) ||
            !writeUByte (ofp, val)) {
	    retval = -1;
	}
    }
    else if (n == 2) {
        /* Write two single runs (could not be value 0x80) */
	if (!writeUByte (ofp, val) || !writeUByte (ofp, val)) {
	    retval = -1;
	}
    } else { 		/* Write a single run */
	if (val == flag) {
	    if (!writeUByte (ofp, flag) || !writeUByte (ofp, 0)) {
		retval = -1;
	    }
	} else {
	    if (!writeUByte (ofp, val)) {
		retval = -1;
	    }
	}
    }

    return ((retval < 0) ? retval : val);
}

/* End writing Runlength Encoded Data */
static void rle_endwrite (tkimg_MFile *ofp)
{
    if (rlebuf.n > 0) {
	rle_putrun (rlebuf.n, rlebuf.val, ofp);
	rlebuf.val = rlebuf.n = 0; 	/* Clear RLE-buffer */
    }
}

#define OUT Tcl_WriteChars (outChan, str, -1)
static void printImgInfo (SUNHEADER *sh, const char *filename, const char *msg)
{
    Tcl_Channel outChan;
    char str[256];
    UInt type = sh->ras_type;

    outChan = Tcl_GetStdChannel (TCL_STDOUT);
    if (!outChan) {
        return;
    }

    sprintf(str, "%s %s\n", msg, filename);                                       OUT;
    sprintf(str, "\tSize in pixel   : %d x %d\n", sh->ras_width, sh->ras_height); OUT;
    sprintf(str, "\tDepth of pixels : %d\n", sh->ras_depth);                      OUT;
    sprintf(str, "\tCompression     : %s\n", (type == RAS_TYPE_STD? "None":
				              (type == RAS_TYPE_RLE? "RLE":
					                             "Unknown"))); OUT;
    sprintf(str, "\tColormap type   : %d\n", sh->ras_maptype);                    OUT;
    Tcl_Flush(outChan);
}
#undef OUT

static Boln read_sun_header (tkimg_MFile *ifp, SUNHEADER *sunhdr)
{
    int  i;
    UInt *cp;

    cp = (UInt *)sunhdr;

    /* Read in all 32-bit values of the header and check for byte order */
    for (i=0; i<sizeof(SUNHEADER) / sizeof(sunhdr->ras_magic); i++) {
	if (!readUInt (ifp, cp))
	    return FALSE;
	cp++;
    }
    if (sunhdr->ras_magic != RAS_MAGIC)
	return FALSE;
    return TRUE;
}

/* Write out a SUN-fileheader */
static Boln write_sun_header (tkimg_MFile *ofp, SUNHEADER *sunhdr)
{
    int i, hdr_entries;
    UInt *cp;

    cp = (UInt *)sunhdr;

    hdr_entries = sizeof (SUNHEADER)/sizeof(sunhdr->ras_magic);
    /* Write out all 32-bit values of the header and check for byte order */
    for (i=0; i<hdr_entries; i++) {
	if (!writeUInt (ofp, *(cp++))) {
	    return FALSE;
	}
    }
    return TRUE;
}

/* Read the sun colourmap */
static Boln read_sun_cols (tkimg_MFile *ifp, SUNHEADER *sunhdr, UByte *colormap)
{
    int ncols;

    /* Read in SUN-raster Colormap */
    ncols = sunhdr->ras_maplength / 3;
    if (ncols <= 0)
	return FALSE;

    if (3*ncols != tkimg_Read(ifp, (char *)colormap, 3*ncols)) {
	return FALSE;
    }
    return TRUE;
}

/* Write a sun colourmap */
/* This function is commented out because it is not used anywhere
static Boln write_sun_cols (tkimg_MFile *ofp, SUNHEADER *sunhdr, UByte *colormap)
{
    int ncols;

    ncols = sunhdr->ras_maplength / 3;
    if (3*ncols != tkimg_Write(ofp, (const char *)colormap, 3*ncols)) {
	return FALSE;
    }
    return TRUE;
}
*/

typedef struct myblock {
    Tk_PhotoImageBlock ck;
    int dummy; /* extra space for offset[3], in case it is not
		  included already in Tk_PhotoImageBlock */
} myblock;
#define block bl.ck

/* Load SUN Raster file with depth 1 */
static Boln load_sun_d1 (Tcl_Interp *interp, tkimg_MFile *ifp,
                         Tk_PhotoHandle imageHandle, int destX, int destY,
                         int width, int height, int srcX, int srcY,
			 int fileWidth, int fileHeight, int type)
{
    UByte *dest, bit2byte[256*8];
    UByte *pixbuf;
    myblock bl;
    int pix8;
    int linepad;
    int x, y;
    int stopY, outY;
    int i, j;
    int err = 0, rle;
    char errMsg[200];

    pixbuf = (UByte *) ckalloc (fileWidth);
    if (!pixbuf) {
	sprintf(errMsg, "Can't allocate memory of size %d", fileWidth);
	Tcl_AppendResult(interp, errMsg, (char *)NULL);
	return TCL_ERROR;
    }

    block.pixelSize = 1;
    block.pitch     = fileWidth;
    block.width     = width;
    block.height    = 1;
    block.offset[0] = 0;
    block.offset[1] = 0;
    block.offset[2] = 0;
    block.offset[3] = 0;
    block.pixelPtr  = pixbuf + srcX;

    rle = (type == RAS_TYPE_RLE);
    linepad = ((fileWidth+7)/8) % 2; 	/* Check for 16bit align */

    if (rle)
	rle_startread (ifp);

    /* Get an array for mapping 8 bits in a byte to 8 bytes */
    dest = bit2byte;
    for (j=0; j<256; j++) {
	for (i=7; i>=0; i--) {
	    *(dest++) = ((j & (1 << i)) == 0) * 255;
	}
    }

    stopY = srcY + height;
    outY = destY;

    for (y=0; y<stopY; y++) {
	dest = pixbuf;
	x = fileWidth;
	while (x >= 8) {
	    pix8 = rle ? rle_getc (ifp) : sun_getc (ifp);
	    if (pix8 < 0) { err = 1; pix8 = 0; }

	    memcpy (dest, bit2byte + pix8*8, 8);
	    dest += 8;
	    x -= 8;
	}

	if (x>0) {
	    pix8 = rle ? rle_getc (ifp) : sun_getc (ifp);
	    if (pix8 < 0) { err = 1; pix8 = 0; }

	    memcpy (dest, bit2byte + pix8*8, x);
	    dest += x;
	}

	if (linepad)
	    err |= ((rle ? rle_getc (ifp) : sun_getc (ifp)) < 0);

	if (err) {
	    sprintf(errMsg, "Unexpected EOF while reading scanline %d", y);
	    Tcl_AppendResult(interp, errMsg, (char *) NULL);
	    return FALSE;
        }
	if (y >= srcY) {
	    tkimg_PhotoPutBlock(interp, imageHandle, &block, destX, outY, width, 1, TK_PHOTO_COMPOSITE_SET);
	    outY++;
	}
    }
    return TRUE;
}

/* Load SUN Raster file with depth 8 */
static Boln load_sun_d8 (Tcl_Interp *interp, tkimg_MFile *ifp,
                         Tk_PhotoHandle imageHandle, int destX, int destY,
                         int width, int height, int srcX, int srcY,
                         int fileWidth, int fileHeight,
			 int type, UByte *suncolmap, int maplength)
{
    UByte *dest, *indData = NULL, *src;
    UByte *pixbuf = NULL;
    myblock bl;
    int linepad;
    int x, y;
    int stopY, outY;
    int ncols;
    int greyscale, nchan;
    int err, rle;
    char errMsg[200];

    rle     = (type == RAS_TYPE_RLE);
    linepad = fileWidth % 2;
    ncols   = maplength / 3;

    /* Check, if it's a greyscale or color indexed image. */
    greyscale = 1;
    nchan     = 1;
    if ((ncols > 0) && (suncolmap != NULL)) {
	greyscale = 0;
	nchan     = 3;
    }

    if (!greyscale) {
	pixbuf = (UByte *) ckalloc (fileWidth * nchan);
	if (!pixbuf) {
	    sprintf(errMsg, "Can't allocate memory of size %d",
			      fileWidth * nchan);
	    Tcl_AppendResult(interp, errMsg, (char *)NULL);
	    if (suncolmap)
		ckfree ((char *)suncolmap);
	    return TCL_ERROR;
	}
    }

    /* This buffer contains either the color indices or the greyscale value. */
    indData = (UByte *)ckalloc (fileWidth * sizeof (UByte));
    if (!indData) {
	sprintf(errMsg, "Can't allocate memory of size %d",
			  fileWidth * sizeof (UByte));
	Tcl_AppendResult(interp, errMsg, (char *)NULL);
	return TCL_ERROR;
    }

    block.pixelSize = nchan;
    block.pitch     = fileWidth * nchan;
    block.width     = width;
    block.height    = 1;
    block.offset[0] = 0;
    block.offset[1] = greyscale? 0: 1;
    block.offset[2] = greyscale? 0: 2;
    block.offset[3] = 0;
    block.pixelPtr  = (greyscale?
	              (indData + srcX * nchan):
		      (pixbuf + srcX * nchan));

    if (rle)
	rle_startread (ifp);  /* Initialize RLE-buffer */

    stopY = srcY + height;
    outY = destY;

    for (y=0; y<stopY; y++) {
	src  = indData;
	memset ((char *)src, 0, fileWidth);
	err = ((rle ? rle_fread ((char *)src, 1, fileWidth, ifp) :
		      sun_fread ((char *)src, 1, fileWidth, ifp)) != fileWidth);
	if (err && (y != height -1)) {
	    sprintf(errMsg, "Unexpected EOF while reading scanline %d", y);
	    Tcl_AppendResult(interp, errMsg, (char *) NULL);
	    ckfree ((char *)indData);
	    return FALSE;
	}
	if (linepad) {
	    err = ((rle ? rle_getc (ifp) : sun_getc (ifp)) < 0);
	    if (err) {
		sprintf(errMsg, "Unexpected EOF while reading scanline %d", y);
		Tcl_AppendResult(interp, errMsg, (char *) NULL);
		ckfree ((char *)indData);
		return FALSE;
	    }
	}

	if (!greyscale) {
	    src  = indData;
	    dest = pixbuf;
	    for (x=0; x<width; x++) {
		*dest++ = suncolmap[*src];
		*dest++ = suncolmap[*src + ncols];
		*dest++ = suncolmap[*src + 2*ncols];
		src++;
	    }
	}

	if (y >= srcY) {
	    tkimg_PhotoPutBlock(interp, imageHandle, &block, destX, outY, width, 1, TK_PHOTO_COMPOSITE_SET);
	    outY++;
	}
    }
    ckfree ((char *)indData);
    return TRUE;
}

/* Load SUN Raster file with true color image: depth = 24 or 32 */

static Boln load_rgb (Tcl_Interp *interp, tkimg_MFile *ifp,
                      Tk_PhotoHandle imageHandle, int destX, int destY,
                      int width, int height, int srcX, int srcY,
		      int fileWidth, int fileHeight,
		      int nchan, int type, int showMatte)
{
    UByte *dest, tmp;
    UByte *pixbuf;
    myblock bl;
    int linepad;
    int x, y;
    int stopY, outY;
    int err, rle;
    char errMsg[200];

    pixbuf = (UByte *) ckalloc (fileWidth * nchan);
    if (!pixbuf) {
	sprintf(errMsg, "Can't allocate memory of size %d",
                          fileWidth * nchan);
	Tcl_AppendResult(interp, errMsg, (char *)NULL);
	return TCL_ERROR;
    }

    block.pixelSize = nchan;
    block.pitch = fileWidth * nchan;
    block.width = width;
    block.height = 1;
    block.offset[0] = 0;
    block.offset[1] = 1;
    block.offset[2] = 2;
    if (nchan < 4) {
	showMatte = 0;
    }
    block.offset[3] = showMatte? 3: 0;

    block.pixelPtr = pixbuf + srcX * nchan;

    rle     = (type == RAS_TYPE_RLE);
    linepad = (fileWidth*nchan) % 2;

    if (rle)
        rle_startread (ifp);  	/* Initialize RLE-buffer */

    stopY = srcY + height;
    outY  = destY;

    for (y=0; y<stopY; y++) {
	dest = pixbuf;
	memset ((char *)dest, 0, nchan*fileWidth);
	err = ((rle ? rle_fread ((char *)dest, nchan, fileWidth, ifp) :
		      sun_fread ((char *)dest, nchan, fileWidth, ifp)) != fileWidth);
	if (err && (y != height -1)) {
	    sprintf(errMsg, "Unexpected EOF while reading scanline %d", y);
	    Tcl_AppendResult(interp, errMsg, (char *) NULL);
	    ckfree ((char *)pixbuf);
	    return FALSE;
	}
	if (linepad) {
	    err = ((rle ? rle_getc (ifp) : sun_getc (ifp)) < 0);
	    if (err) {
		sprintf(errMsg, "Unexpected EOF while reading scanline %d", y);
		Tcl_AppendResult(interp, errMsg, (char *) NULL);
		ckfree ((char *)pixbuf);
		return FALSE;
	    }
	}

	if (y >= srcY) {
	    dest = pixbuf + srcX * nchan;
	    if (type != 3) {
		if (nchan == 3) {		/* GBR Format. Swap to RGB. */
		    for (x=0; x<width; x++) {
			tmp = dest[0];
			dest[0] = dest[2];
			dest[2] = tmp;

			dest += 3;
		    }
		} else { 			/* AGBR Format. Swap to RGBA. */
		    for (x=0; x<width; x++) {
			tmp = dest[0];
			dest[0] = dest[3];
			dest[3] = tmp;

			tmp = dest[1];
			dest[1] = dest[2];
			dest[2] = tmp;

			dest += 4;
		    }
		}
	    }
	    tkimg_PhotoPutBlock(interp, imageHandle, &block, destX, outY, width, 1, showMatte? TK_PHOTO_COMPOSITE_OVERLAY: TK_PHOTO_COMPOSITE_SET);
	    outY++;
	}
    }
    ckfree ((char *)pixbuf);
    return TRUE;
}

/*
 * Here is the start of the standard functions needed for every image format.
 */

/*
 * Prototypes for local procedures defined in this file:
 */

static int   ParseFormatOpts _ANSI_ARGS_((Tcl_Interp *interp, Tcl_Obj *format,
                 int *comp, int *verb, int *matte));
static int   CommonMatch _ANSI_ARGS_((tkimg_MFile *handle, int *widthPtr,
	         int *heightPtr, SUNHEADER *sunHeaderPtr));
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
    static const char *const sunOptions[] = {"-compression", "-verbose", "-matte"};
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
	    if (Tcl_GetIndexFromObj(interp, objv[i], (CONST84 char *CONST86 *)sunOptions,
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

    #if defined (DEBUG_LOCAL)
	printf("ChnMatch\n"); fflush(stdout);
    #endif

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

    #if defined (DEBUG_LOCAL)
        printf("ObjMatch\n"); fflush(stdout);
    #endif

    tkimg_FixObjMatchProc (&interp, &data, &format, &widthPtr, &heightPtr);

    if (!tkimg_ReadInit(data, 'Y', &handle)) {
	return 0;
    }
    return CommonMatch(&handle, widthPtr, heightPtr, NULL);
}

static int CommonMatch(handle, widthPtr, heightPtr, sunHeaderPtr)
    tkimg_MFile *handle;
    int   *widthPtr;
    int   *heightPtr;
    SUNHEADER *sunHeaderPtr;
{
    SUNHEADER sh;

    if (!read_sun_header (handle, &sh))
	return 0;

    *widthPtr  = sh.ras_width;
    *heightPtr = sh.ras_height;
    if (sunHeaderPtr)
	*sunHeaderPtr = sh;
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

    tkimg_ReadInit (data, 'Y', &handle);
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
    SUNHEADER sh;
    UByte *suncolmap = NULL;
    int compr, verbose, matte;
    char errMsg[200];

    if (ParseFormatOpts(interp, format, &compr, &verbose, &matte) != TCL_OK) {
        return TCL_ERROR;
    }

    CommonMatch(handle, &fileWidth, &fileHeight, &sh);
    if (verbose)
        printImgInfo (&sh, filename, "Reading image:");

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

    if (sh.ras_type > 5) {
	sprintf(errMsg, "Unknown Sun Raster type: %d", sh.ras_type);
	Tcl_AppendResult(interp, errMsg, (char *)NULL);
	return TCL_ERROR;
    }

    if (sh.ras_type == RAS_TYPE_RLE)
	tkimg_ReadBuffer (1);

    /* Is there a RGB colourmap ? */
    if ((sh.ras_maptype == 1) && (sh.ras_maplength > 0)) {
	suncolmap = (UByte *)ckalloc (sh.ras_maplength);
	if (!suncolmap) {
	    sprintf(errMsg, "Can't allocate memory of size %d",
			      sh.ras_maplength);
	    Tcl_AppendResult(interp, errMsg, (char *)NULL);
	    tkimg_ReadBuffer (0);
            return TCL_ERROR;
	}

	if (!read_sun_cols (handle, &sh, suncolmap)) {
	    Tcl_AppendResult(interp, "Unable to read color map", (char *)NULL);
	    ckfree ((char *)suncolmap);
	    tkimg_ReadBuffer (0);
            return TCL_ERROR;
	}
	#if defined(DEBUG)
        {
	    int j, ncols;
	    printf("Colormap values:\n");
	    ncols = sh.ras_maplength/3;
	    for (j=0; j < ncols; j++)
		printf("Entry 0x%08x: 0x%04x,  0x%04x, 0x%04x\n",
                       j,suncolmap[j],suncolmap[j+ncols],suncolmap[j+2*ncols]);
	}
	#endif
    } else if (sh.ras_maplength > 0) {
	UByte dummy[1];
	int d, length;

	/* This type of colourmap is not supported. Ignore it. */
	length = (sizeof (SUNHEADER)/sizeof (UInt)) * 4 + sh.ras_maplength;
	for (d=0; d<length; d++) {
	    (void) readUByte (handle, dummy);
	}
    }

    Tk_PhotoExpand(imageHandle, destX + outWidth, destY + outHeight);

    nchan = (sh.ras_depth == 32? 4: 3);

    switch (sh.ras_depth)
    {
	case 1:    /* 2 colors B/W */
            if (!load_sun_d1 (interp, handle, imageHandle, destX, destY,
			      outWidth, outHeight, srcX, srcY,
			      fileWidth, fileHeight, sh.ras_type))
		retCode = TCL_ERROR;
	    break;

	case 8:    /* 256 colours */
            if (!load_sun_d8 (interp, handle, imageHandle, destX, destY,
			      outWidth, outHeight, srcX, srcY,
			      fileWidth, fileHeight, sh.ras_type,
			      suncolmap, sh.ras_maplength))
		retCode = TCL_ERROR;
	    break;

	case 24:   /* True color */
	case 32:   /* True color with matte channel */
            if (!load_rgb (interp, handle, imageHandle, destX, destY,
			   outWidth, outHeight, srcX, srcY,
			   fileWidth, fileHeight,
			   nchan, sh.ras_type, matte))
		retCode = TCL_ERROR;
	    break;

	default:
	    sprintf(errMsg, "Image has invalid pixel depth: %d", sh.ras_depth);
	    Tcl_AppendResult(interp, errMsg, (char *)NULL);
	    retCode = TCL_ERROR;
	    break;
    }
    if (suncolmap)
	ckfree ((char *)suncolmap);
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
    int     x, y, nchan, nBytes, linepad;
    int     redOffset, greenOffset, blueOffset, alphaOffset;
    UByte   *pixelPtr, *pixRowPtr;
    SUNHEADER sh;
    UByte *row, *rowPtr;
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

    nchan   = ((matte && alphaOffset)? 4: 3);
    nBytes  = blockPtr->width * nchan;
    linepad = nBytes % 2;

    /* Fill the Sun header struct and write the header to the channel. */
    sh.ras_magic     = RAS_MAGIC;
    sh.ras_width     = blockPtr->width;
    sh.ras_height    = blockPtr->height;
    sh.ras_depth     = 8 * nchan;
    sh.ras_length    = (nBytes + linepad) * blockPtr->height;
    sh.ras_type      = (compr) ? RAS_TYPE_RLE : RAS_TYPE_STD;
    sh.ras_maptype   = 0;   		/* No colourmap */
    sh.ras_maplength = 0; 		/* Length of colourmap */

    write_sun_header (handle, &sh);

    /* Now write out the image data. */
    pixRowPtr = blockPtr->pixelPtr + blockPtr->offset[0];
    if (!compr) {
	row = (UByte *) ckalloc (nBytes);
	if (!row) {
	    sprintf(errMsg, "Can't allocate memory of size %d", nBytes);
	    Tcl_AppendResult(interp, errMsg, (char *)NULL);
            return TCL_ERROR;
	}
	for (y=0; y<blockPtr->height; y++) {
	    rowPtr = row;
	    pixelPtr = pixRowPtr;
	    for (x=0; x<blockPtr->width; x++) {
		if (nchan == 4) {
		    /* Have a matte channel and write it. */
		    *(rowPtr++) = pixelPtr[alphaOffset];
		}
		*(rowPtr++) = pixelPtr[blueOffset];
		*(rowPtr++) = pixelPtr[greenOffset];
		*(rowPtr++) = pixelPtr[redOffset];
		pixelPtr += blockPtr->pixelSize;
	    }
	    if (nBytes != tkimg_Write(handle, (const char *)row, nBytes)) {
		sprintf(errMsg, "Can't write %d bytes to image file", nBytes);
		Tcl_AppendResult(interp, errMsg, (char *)NULL);
		ckfree ((char *)row);
		return TCL_ERROR;
	    }
	    for (x=0; x<linepad; x++) {
		writeUByte (handle, 0);
	    }
	    pixRowPtr += blockPtr->pitch;
	}
	ckfree ((char *)row);
    } else { 			/* RLE compression */
	rle_startwrite (handle);
	for (y = 0; y < blockPtr->height; y++) {
	    pixelPtr = pixRowPtr;
	    for (x = 0; x < blockPtr->width; x++) {
		if (nchan == 4) {
		    /* Have a matte channel and write it. */
		    rle_fputc (pixelPtr[alphaOffset], handle);
		}
		rle_fputc (pixelPtr[blueOffset], handle);
		rle_fputc (pixelPtr[greenOffset], handle);
		rle_fputc (pixelPtr[redOffset], handle);
		pixelPtr += blockPtr->pixelSize;
	    }
	    for (x=0; x<linepad; x++) {
		rle_fputc (0, handle);
	    }
	    pixRowPtr += blockPtr->pitch;
	}
	rle_endwrite (handle);
    }
    if (verbose)
        printImgInfo (&sh, filename, "Saving image:");
    return TCL_OK;
}
