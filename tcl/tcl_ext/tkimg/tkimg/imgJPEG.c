/*
 * imgJPEG.c --
 *
 *	A photo image file handler for JPEG files.
 *
 * This Tk image format handler reads and writes JPEG files in the standard
 * JFIF file format.  ("JPEG" should be the format name.)  It can also read
 * and write strings containing base64-encoded JPEG data.
 *
 * Several options can be provided in the format string, for example:
 *
 *	imageObject read input.jpg -shrink -format "jpeg -grayscale"
 *	imageObject write output.jpg -format "jpeg -quality 50 -progressive"
 *
 * The supported options for reading are:
 *	-fast:        Fast, low-quality processing
 *	-grayscale:   Force incoming image to grayscale
 * The supported options for writing are:
 *	-quality N:   Compression quality (0..100; 5-95 is useful range)
 *	              Default value: 75
 *	-smooth N:    Perform smoothing (10-30 is enough for most GIF's)
 *		      Default value: 0
 *	-grayscale:   Create monochrome JPEG file
 *	-optimize:    Optimize Huffman table
 *	-progressive: Create progressive JPEG file
 *
 *
 * Copyright (c) 1996-1997 Thomas G. Lane.
 * This file is based on tkImgPPM.c from the Tk 4.2 distribution.
 * That file is
 *	Copyright (c) 1994 The Australian National University.
 *	Copyright (c) 1994-1996 Sun Microsystems, Inc.
 *
 * See the file "license.terms" for information on usage and redistribution
 * of this file, and for a DISCLAIMER OF ALL WARRANTIES.
 *
 * You will need a copy of the IJG JPEG library, version 5 or later,
 * to use this file.  If you didn't receive it with this package, see
 *	ftp://ftp.uu.net/graphics/jpeg/
 *
 * Author: Tom Lane (tgl@sss.pgh.pa.us)
 *
 * Modified for dynamical loading, reading from channels and Tcl_Obj's by:
 *	Jan Nijtmans (j.nijtmans@chello.nl)
 *
 * SCCS: @(#) imgJPEG.c
 */

/* system includes */
#include <stdlib.h>
#include <string.h>
#include <setjmp.h>

/* Tk */
#include "imgInt.h"

/* undef Tcl macros that conflict with libjpeg stuff (sigh) */
#undef EXTERN

/* libjpeg */
#ifdef MAC_TCL
#  include "libjpeg:jpeglib.h"
#  include "libjpeg:jerror.h"
#else
#  include <sys/types.h>
#ifdef HAVE_JPEGLIB_H
#  include <jpeglib.h>
#  include <jerror.h>
#else
#  include "libjpeg/jpeglib.h"
#  include "libjpeg/jerror.h"
#endif
#endif

#ifdef __WIN32__
#define JPEG_LIB_NAME "jpeg62.dll"
#endif

#ifndef JPEG_LIB_NAME
#define JPEG_LIB_NAME "libjpeg.so"
#endif

/*
 * The format record for the JPEG file format:
 */

static int	ChnMatchJPEG _ANSI_ARGS_((Tcl_Interp *interp, Tcl_Channel chan,
		    CONST char *fileName, Tcl_Obj *format, int *widthPtr,
		    int *heightPtr));
static int	ObjMatchJPEG _ANSI_ARGS_((Tcl_Interp *interp, Tcl_Obj *data,
		    Tcl_Obj *format, int *widthPtr, int *heightPtr));
static int	ChnReadJPEG _ANSI_ARGS_((Tcl_Interp *interp,
		    Tcl_Channel chan, CONST char *fileName, Tcl_Obj *format,
		    Tk_PhotoHandle imageHandle, int destX, int destY,
		    int width, int height, int srcX, int srcY));
static int	ObjReadJPEG _ANSI_ARGS_((Tcl_Interp *interp,
		    Tcl_Obj *data, Tcl_Obj *format,
		    Tk_PhotoHandle imageHandle, int destX, int destY,
		    int width, int height, int srcX, int srcY));
static int	ChnWriteJPEG _ANSI_ARGS_((Tcl_Interp *interp,
		    CONST char *fileName, Tcl_Obj *format,
		    Tk_PhotoImageBlock *blockPtr));
static int	StringWriteJPEG _ANSI_ARGS_((Tcl_Interp *interp,
		    Tcl_DString *dataPtr, Tcl_Obj *format,
		    Tk_PhotoImageBlock *blockPtr));

Tk_PhotoImageFormat imgFmtJPEG = {
    "jpeg",					/* name */
    (Tk_ImageFileMatchProc *) ChnMatchJPEG,	/* fileMatchProc */
    (Tk_ImageStringMatchProc *) ObjMatchJPEG,	/* stringMatchProc */
    (Tk_ImageFileReadProc *) ChnReadJPEG,	/* fileReadProc */
    (Tk_ImageStringReadProc *) ObjReadJPEG,	/* stringReadProc */
    (Tk_ImageFileWriteProc *) ChnWriteJPEG,	/* fileWriteProc */
    (Tk_ImageStringWriteProc *) StringWriteJPEG,/* stringWriteProc */
};

/*
 * Declarations for libjpeg source and destination managers to handle
 * reading and writing base64-encoded strings and Tcl_Channel's.
 */

#define STRING_BUF_SIZE  4096	/* choose any convenient size */

typedef struct source_mgr {	/* Source manager for reading from string */
  struct jpeg_source_mgr pub;	/* public fields */

  MFile handle;			/* base64 stream */
  JOCTET buffer[STRING_BUF_SIZE]; /* buffer for a chunk of decoded data */
} *src_ptr;

typedef struct destination_mgr { /* Manager for string output */
  struct jpeg_destination_mgr pub; /* public fields */

  MFile handle;			/* base64 stream */
  JOCTET buffer[STRING_BUF_SIZE]; /* buffer for a chunk of decoded data */
} *dest_ptr;

/*
 * Other declarations
 */

struct my_error_mgr {		/* Extended libjpeg error manager */
  struct jpeg_error_mgr pub;	/* public fields */
  jmp_buf setjmp_buffer;	/* for return to caller from error exit */
};

/*
 * Prototypes for local procedures defined in this file:
 */

static int	CommonMatchJPEG _ANSI_ARGS_((MFile *handle,
		    int *widthPtr, int *heightPtr));
static int	CommonReadJPEG _ANSI_ARGS_((Tcl_Interp *interp,
		    j_decompress_ptr cinfo, Tcl_Obj *format,
		    Tk_PhotoHandle imageHandle, int destX, int destY,
		    int width, int height, int srcX, int srcY));
static int	CommonWriteJPEG _ANSI_ARGS_((Tcl_Interp *interp,
		    j_compress_ptr cinfo, Tcl_Obj *format,
		    Tk_PhotoImageBlock *blockPtr));
static void	jpeg_obj_src _ANSI_ARGS_((j_decompress_ptr, Tcl_Obj *));
static void	jpeg_channel_src _ANSI_ARGS_((j_decompress_ptr, Tcl_Channel));
static boolean	fill_input_buffer _ANSI_ARGS_((j_decompress_ptr));
static void	skip_input_data _ANSI_ARGS_((j_decompress_ptr, long));
static void	dummy_source _ANSI_ARGS_((j_decompress_ptr));
static void	jpeg_string_dest _ANSI_ARGS_((j_compress_ptr, Tcl_DString*));
static void	jpeg_channel_dest _ANSI_ARGS_((j_compress_ptr, Tcl_Channel));
static void	my_init_destination _ANSI_ARGS_((j_compress_ptr));
static boolean	my_empty_output_buffer _ANSI_ARGS_((j_compress_ptr));
static void	my_term_destination _ANSI_ARGS_((j_compress_ptr));
static void	my_error_exit _ANSI_ARGS_((j_common_ptr cinfo));
static void	my_output_message _ANSI_ARGS_((j_common_ptr cinfo));
static void	append_jpeg_message _ANSI_ARGS_((Tcl_Interp *interp,
		    j_common_ptr cinfo));
static int	load_jpeg_library _ANSI_ARGS_((Tcl_Interp *interp));
static int	CreateCompress _ANSI_ARGS_((j_compress_ptr, int, size_t));
static int	CreateDecompress _ANSI_ARGS_((j_decompress_ptr, int, size_t));

/*
 * Stuff to support dynamic loading of libjpeg
 */

static struct JpegFunctions {
    VOID *handle;
    int (* abort_decompress) _ANSI_ARGS_((j_decompress_ptr));
    int (* destroy_compress) _ANSI_ARGS_((j_compress_ptr));
    int (* destroy_decompress) _ANSI_ARGS_((j_decompress_ptr));
    int (* finish_compress) _ANSI_ARGS_((j_compress_ptr));
    int (* finish_decompress) _ANSI_ARGS_((j_decompress_ptr));
    int (* read_header) _ANSI_ARGS_((j_decompress_ptr, int));
    JDIMENSION (* read_scanlines) _ANSI_ARGS_((j_decompress_ptr,
			JSAMPARRAY, JDIMENSION));
    boolean (* resync_to_restart) _ANSI_ARGS_((j_decompress_ptr, int));
    int (* set_defaults) _ANSI_ARGS_((j_compress_ptr));
    int (* start_compress) _ANSI_ARGS_((j_compress_ptr, int));
    int (* start_decompress) _ANSI_ARGS_((j_decompress_ptr));
    struct jpeg_error_mgr *(* std_error) _ANSI_ARGS_((struct jpeg_error_mgr *));
    JDIMENSION (* write_scanlines) _ANSI_ARGS_((j_compress_ptr,
			JSAMPARRAY, JDIMENSION));
    int (* set_colorspace) _ANSI_ARGS_((j_compress_ptr, J_COLOR_SPACE));
    int (* set_quality) _ANSI_ARGS_((j_compress_ptr, int, int));
    int (* simple_progression) _ANSI_ARGS_((j_compress_ptr));
    int (* CreateCompress) _ANSI_ARGS_((j_compress_ptr, int, size_t));
    int (* CreateDecompress) _ANSI_ARGS_((j_decompress_ptr, int, size_t));
    int (* create_compress) _ANSI_ARGS_((j_compress_ptr));
    int (* create_decompress) _ANSI_ARGS_((j_decompress_ptr));
    void (* destroy) _ANSI_ARGS_((j_common_ptr));
    JDIMENSION (* write_raw_data) _ANSI_ARGS_((j_compress_ptr,
	    JSAMPIMAGE, JDIMENSION));
    void (* suppress_tables) _ANSI_ARGS_((j_compress_ptr, boolean));
    void (* write_tables) _ANSI_ARGS_((j_compress_ptr));
    JDIMENSION (* read_raw_data) _ANSI_ARGS_((j_decompress_ptr,
	    JSAMPIMAGE, JDIMENSION));
    void (* abort) _ANSI_ARGS_((j_common_ptr));
} jpeg = {0};

static char *symbols[] = {
#ifdef NEED_SHORT_EXTERNAL_NAMES
    "jAbrtDecompress",
    "jDestCompress",
    "jDestDecompress",
    "jFinCompress",
    "jFinDecompress",
    "jReadHeader",
    "jReadScanlines",
    "jResyncRestart",
    "jSetDefaults",
    "jStrtCompress",
    "jStrtDecompress",
    "jStdError",
    "jWrtScanlines",
    /*	the following 3 symbols are not crucial. They implement
	resp. the "-grayscale", "-quality" and "-progressive"
	options. If any of the symbols are missing from the
	library, the corresponding option just has no effect. */
    "jSetColorspace",
    "jSetQuality",
    "jSimProgress",
    "jCreaCompress",
    "jCreaDecompress",
    "jCreaCompress",
    "jCreaDecompress",
    "jDestroy",
    "jWrtRawData",
    "jSuppressTables",
    "jWrtTables",
    "jReadRawData",
    "jAbort",
#else
    "jpeg_abort_decompress",
    "jpeg_destroy_compress",
    "jpeg_destroy_decompress",
    "jpeg_finish_compress",
    "jpeg_finish_decompress",
    "jpeg_read_header",
    "jpeg_read_scanlines",
    "jpeg_resync_to_restart",
    "jpeg_set_defaults",
    "jpeg_start_compress",
    "jpeg_start_decompress",
    "jpeg_std_error",
    "jpeg_write_scanlines",
    /*	the following 3 symbols are not crucial. They implement
	resp. the "-grayscale", "-quality" and "-progressive"
	options. If any of the symbols are missing from the
	library, the corresponding option just has no effect. */
    "jpeg_set_colorspace",
    "jpeg_set_quality",
    "jpeg_simple_progression",
    /* In later versions, jpeg_create_compress and jpeg_create_decompress
       are macros pointing to jpeg_CreateCompress and jpeg_CreateDecompres.
       Which one is found depends on the version. */
    "jpeg_CreateCompress",
    "jpeg_CreateDecompress",
    "jpeg_create_compress",
    "jpeg_create_decompress",
    "jpeg_destroy",
    "jpeg_write_raw_data",
    "jpeg_suppress_tables",
    "jpeg_write_tables",
    "jpeg_read_raw_data",
    "jpeg_abort",
#endif
    (char *) NULL
};


static int
load_jpeg_library(interp)
    Tcl_Interp *interp;
{
    struct jpeg_compress_struct *cinfo; /* libjpeg's parameter structure */
    struct my_error_mgr jerror;	/* for controlling libjpeg error handling */
    int i;

    if (ImgLoadLib(interp, JPEG_LIB_NAME, &jpeg.handle, symbols, 13)
	    != TCL_OK) {
	return TCL_ERROR;
    }
    if (jpeg.CreateCompress == NULL) {
	if (jpeg.create_compress == NULL) {
	    goto load_failed;
	}
	jpeg.CreateCompress = CreateCompress;
    }
    if (jpeg.CreateDecompress == NULL) {
	if (jpeg.create_decompress == NULL) {
	    goto load_failed;
	}
	jpeg.CreateDecompress = CreateDecompress;
    }

    /* The followin code tries to determine if the JPEG library is
       valid at all. The library might be configured differently,
       which will produce core dumps. Also it might be that
       fields appear in different places in jpeg_compress_struct
       or jpeg_decompress_struct. This will make the library totally
       unusable. In stead of a core-dump, we better have a proper
       error message */

    /* overallocat size, so we don't get a core-dump if the library
       thinks that the structure is much larger */
    cinfo = (struct jpeg_compress_struct *)
	    ckalloc(8*sizeof(struct jpeg_compress_struct));
    cinfo->err = jpeg.std_error(&jerror.pub);
    jerror.pub.error_exit = my_error_exit;
    jerror.pub.output_message = my_output_message;
    /* Establish the setjmp return context for my_error_exit to use. */
    if (setjmp(jerror.setjmp_buffer)) {
      /* If we get here, the JPEG library is invalid. */
      jpeg.destroy_compress(cinfo);
      ckfree((char *)cinfo);
load_failed:
      if (interp) {
	Tcl_AppendResult(interp, "couldn't load \"", JPEG_LIB_NAME,
		"\": please upgrade to at least version 6a", (char *) NULL);
      }
      ImgLoadFailed(&jpeg.handle);
      return TCL_ERROR;
    }

    /* Now we can initialize libjpeg. */
    ((char *) cinfo)[sizeof(struct jpeg_compress_struct)] = 53;
    jpeg.CreateCompress(cinfo, JPEG_LIB_VERSION,
			(size_t) sizeof(struct jpeg_compress_struct));
    if (((char *) cinfo)[sizeof(struct jpeg_compress_struct)] != 53) {
	/* Oops. The library changed this value, which is outside the
	 * structure. Definitely, the library is invalid!!!! */
	ERREXIT(cinfo, JMSG_NOMESSAGE);
    }

    /* Set up JPEG compression parameters. */
    cinfo->image_width = 16;
    cinfo->image_height = 16;
    cinfo->input_components = 3;
    cinfo->in_color_space = JCS_RGB;
    cinfo->data_precision = -1;
    cinfo->optimize_coding = TRUE;
    cinfo->dct_method = -1;
    cinfo->X_density = 0;
    cinfo->Y_density = 0;
    jpeg.set_defaults(cinfo);

    if ((cinfo->data_precision != BITS_IN_JSAMPLE) ||
	    (cinfo->optimize_coding != FALSE) ||
	    (cinfo->dct_method != JDCT_DEFAULT) ||
	    (cinfo->X_density != 1) ||
	    (cinfo->Y_density != 1)) {
	ERREXIT(cinfo, JMSG_NOMESSAGE);
    }
    for (i = 0; i < NUM_ARITH_TBLS; i++) {
	if ((cinfo->arith_dc_L[i] != 0) ||
		(cinfo->arith_dc_U[i] != 1) ||
		(cinfo->arith_ac_K[i] != 5)) {
	    ERREXIT(cinfo, JMSG_NOMESSAGE);
	}
    }
    jpeg.destroy_compress(cinfo);
    ckfree((char *) cinfo);
    return TCL_OK;
}

int
ImgLoadJpegLibrary()
{
    if ((load_jpeg_library(NULL) == TCL_OK)
	    && jpeg.destroy
	    && jpeg.write_raw_data
	    && jpeg.suppress_tables
	    && jpeg.write_tables
	    && jpeg.read_raw_data
	    && jpeg.abort) {
	return TCL_OK;
    }
    return TCL_ERROR;
}

boolean Imgjpeg_resync_to_restart _ANSI_ARGS_((j_decompress_ptr, int));
JDIMENSION Imgjpeg_read_scanlines _ANSI_ARGS_((j_decompress_ptr,
			JSAMPARRAY, JDIMENSION));
int Imgjpeg_set_colorspace _ANSI_ARGS_((j_compress_ptr, J_COLOR_SPACE));
int Imgjpeg_set_defaults _ANSI_ARGS_((j_compress_ptr));
int Imgjpeg_start_decompress _ANSI_ARGS_((j_decompress_ptr));
void Imgjpeg_destroy _ANSI_ARGS_((j_common_ptr));
struct jpeg_error_mgr * Imgjpeg_std_error _ANSI_ARGS_((struct jpeg_error_mgr *));
int Imgjpeg_CreateDecompress _ANSI_ARGS_((j_decompress_ptr, int, size_t));
JDIMENSION Imgjpeg_write_raw_data _ANSI_ARGS_((j_compress_ptr,
	    JSAMPIMAGE, JDIMENSION));
void Imgjpeg_suppress_tables _ANSI_ARGS_((j_compress_ptr, boolean));
void Imgjpeg_abort _ANSI_ARGS_((j_common_ptr));
int Imgjpeg_read_header _ANSI_ARGS_((j_decompress_ptr, int));
int Imgjpeg_start_compress _ANSI_ARGS_((j_compress_ptr, int));
void Imgjpeg_write_tables _ANSI_ARGS_((j_compress_ptr));
int Imgjpeg_finish_decompress _ANSI_ARGS_((j_decompress_ptr));
int Imgjpeg_CreateCompress _ANSI_ARGS_((j_compress_ptr, int, size_t));
int Imgjpeg_finish_compress _ANSI_ARGS_((j_compress_ptr));
JDIMENSION Imgjpeg_read_raw_data _ANSI_ARGS_((j_decompress_ptr,
	    JSAMPIMAGE, JDIMENSION));
int Imgjpeg_set_quality _ANSI_ARGS_((j_compress_ptr, int, int));
JDIMENSION Imgjpeg_write_scanlines _ANSI_ARGS_((j_compress_ptr,
			JSAMPARRAY, JDIMENSION));

static int
CreateCompress(cinfo, version, size)
    j_compress_ptr cinfo;
    int version;
    size_t size;
{
    return jpeg.create_compress(cinfo);
}

static int
CreateDecompress(cinfo, version, size)
    j_decompress_ptr cinfo;
    int version;
    size_t size;
{
    return jpeg.create_decompress(cinfo);
}

int
Imgjpeg_CreateCompress(cinfo, version, size)
    j_compress_ptr cinfo;
    int version;
    size_t size;
{
    return jpeg.CreateCompress(cinfo, version, size);
}

int
Imgjpeg_CreateDecompress(cinfo, version, size)
    j_decompress_ptr cinfo;
    int version;
    size_t size;
{
    return jpeg.CreateDecompress(cinfo, version, size);
}

boolean
Imgjpeg_resync_to_restart(a,b)
    j_decompress_ptr a;
    int b;
{
    return jpeg.resync_to_restart(a,b);
}

JDIMENSION
Imgjpeg_read_scanlines(a,b,c)
    j_decompress_ptr a;
    JSAMPARRAY b;
    JDIMENSION c;
{
    return jpeg.read_scanlines(a,b,c);
}

int
Imgjpeg_set_colorspace(a,b)
    j_compress_ptr a;
    J_COLOR_SPACE b;
{
    return jpeg.set_colorspace(a,b);
}

int
Imgjpeg_set_defaults(a)
    j_compress_ptr a;
{
    return jpeg.set_defaults(a);
}

int
Imgjpeg_start_decompress(a)
    j_decompress_ptr a;
{
    return jpeg.start_decompress(a);
}

void
Imgjpeg_destroy(a)
    j_common_ptr a;
{
    jpeg.destroy(a);
}

struct jpeg_error_mgr *
Imgjpeg_std_error(a)
    struct jpeg_error_mgr *a;
{
    return jpeg.std_error(a);
}

JDIMENSION Imgjpeg_write_raw_data(a,b,c)
    j_compress_ptr a;
    JSAMPIMAGE b;
    JDIMENSION c;
{
    return jpeg.write_raw_data(a,b,c);
}

void
Imgjpeg_suppress_tables(a,b)
    j_compress_ptr a;
    boolean b;
{
    jpeg.suppress_tables(a,b);
}

void
Imgjpeg_abort(a)
    j_common_ptr a;
{
    jpeg.abort(a);
}

int
Imgjpeg_read_header(a,b)
    j_decompress_ptr a;
    int b;
{
    return jpeg.read_header(a,b);
}

int
Imgjpeg_start_compress(a,b)
    j_compress_ptr a;
    int b;
{
    return jpeg.start_compress(a,b);
}

void
Imgjpeg_write_tables(a)
    j_compress_ptr a;
{
    jpeg.write_tables(a);
}

int
Imgjpeg_finish_decompress(a)
    j_decompress_ptr a;
{
    return jpeg.finish_decompress(a);
}

int
Imgjpeg_finish_compress(a)
    j_compress_ptr a;
{
    return jpeg.finish_compress(a);
}

JDIMENSION
Imgjpeg_read_raw_data(a,b,c)
    j_decompress_ptr a;
    JSAMPIMAGE b;
    JDIMENSION c;
{
    return jpeg.read_raw_data(a,b,c);
}

int
Imgjpeg_set_quality(a,b,c)
    j_compress_ptr a;
    int b;
    int c;
{
    return jpeg.set_quality(a,b,c);
}

JDIMENSION
Imgjpeg_write_scanlines(a,b,c)
    j_compress_ptr a;
    JSAMPARRAY b;
    JDIMENSION c;
{
    return jpeg.write_scanlines(a,b,c);
}


/*
 *----------------------------------------------------------------------
 *
 * ChnMatchJPEG --
 *
 *	This procedure is invoked by the photo image type to see if
 *	a channel contains image data in JPEG format.
 *
 * Results:
 *	The return value is >0 if the first characters in channel "chan"
 *	look like JPEG data, and 0 otherwise.  For a valid file, the
 *	image dimensions are determined.
 *
 * Side effects:
 *	The access position in f may change.
 *
 *----------------------------------------------------------------------
 */

static int
ChnMatchJPEG(interp, chan, fileName, format, widthPtr, heightPtr)
    Tcl_Interp *interp;
    Tcl_Channel chan;		/* The image channel, open for reading. */
    CONST char *fileName;	/* The name of the image file. */
    Tcl_Obj *format;		/* User-specified format string, or NULL. */
    int *widthPtr, *heightPtr;	/* The dimensions of the image are
				 * returned here if the file is a valid
				 * JPEG file. */
{
    MFile handle;

    ImgFixChanMatchProc(&interp, &chan, &fileName, &format, &widthPtr, &heightPtr);

    handle.data = (char *) chan;
    handle.state = IMG_CHAN;
    return CommonMatchJPEG(&handle, widthPtr, heightPtr);
}

/*
 *----------------------------------------------------------------------
 *
 * ObjMatchJPEG --
 *
 *	This procedure is invoked by the photo image type to see if
 *	a string contains image data in JPEG format.
 *
 * Results:
 *	The return value is >0 if the first characters in the string look
 *	like JPEG data, and 0 otherwise.  For a valid image, the image
 *	dimensions are determined.
 *
 * Side effects:
 *  the size of the image is placed in widthPtr and heightPtr.
 *
 *----------------------------------------------------------------------
 */

static int
ObjMatchJPEG(interp, data, format, widthPtr, heightPtr)
    Tcl_Interp *interp;
    Tcl_Obj *data;		/* the object containing the image data */
    Tcl_Obj *format;		/* User-specified format object, or NULL. */
    int *widthPtr, *heightPtr;	/* The dimensions of the image are
				 * returned here if the string is a valid
				 * JPEG image. */
{
    MFile handle;

    ImgFixObjMatchProc(&interp, &data, &format, &widthPtr, &heightPtr);

    ImgReadInit(data, '\377', &handle);
    return CommonMatchJPEG(&handle, widthPtr, heightPtr);
}

/*
 *----------------------------------------------------------------------
 *
 * CommonMatchJPEG --
 *
 *	This procedure is invoked by the photo image type to see if
 *	a string contains image data in JPEG format.
 *
 * Results:
 *	The return value is >0 if the first characters in the string look
 *	like JPEG data, and 0 otherwise.  For a valid image, the image
 *	dimensions are determined.
 *
 * Side effects:
 *  the size of the image is placed in widthPtr and heightPtr.
 *
 *----------------------------------------------------------------------
 */

static int
CommonMatchJPEG(handle, widthPtr, heightPtr)
    MFile *handle;		/* the "file" handle */
    int *widthPtr, *heightPtr;	/* The dimensions of the image are
				 * returned here if the string is a valid
				 * JPEG image. */
{
    char buf[256];
    int i;

    i = ImgRead(handle, buf, 3);
    if ((i != 3)||strncmp(buf,"\377\330\377", 3)) {
	return 0;
    }

    buf[0] = buf[2];
    /* at top of loop: have just read first FF of a marker into buf[0] */
    for (;;) {
	/* get marker type byte, skipping any padding FFs */
	while (buf[0] == (char) 0xff) {
	    if (ImgRead(handle, buf,1) != 1) {
		return 0;
	    }
	}
	/* look for SOF0, SOF1, or SOF2, which are the only JPEG variants
	 * currently accepted by libjpeg.
	 */
	if (buf[0] == (char) 0xc0 || buf[0] == (char) 0xc1
		|| buf[0] == (char) 0xc2)
	    break;
	/* nope, skip the marker parameters */
	if (ImgRead(handle, buf, 2) != 2) {
	    return 0;
	}
	i = ((buf[0] & 0x0ff)<<8) + (buf[1] & 0x0ff) - 1;
	while (i>256) {
	    ImgRead(handle, buf, 256);
	    i -= 256;
	}
	if ((i<1) || (ImgRead(handle, buf, i)) != i) {
	    return 0;
	}
	buf[0] = buf[i-1];
	/* skip any inter-marker junk (there shouldn't be any, really) */
	while (buf[0] != (char) 0xff) {
	    if (ImgRead(handle, buf,1) != 1) {
		return 0;
	    }
	}
    }
    /* Found the SOFn marker, get image dimensions */
    if (ImgRead(handle, buf, 7) != 7) {
	return 0;
    }
    *heightPtr = ((buf[3] & 0x0ff)<<8) + (buf[4] & 0x0ff);
    *widthPtr = ((buf[5] & 0x0ff)<<8) + (buf[6] & 0x0ff);

    return 1;
}

/*
 *----------------------------------------------------------------------
 *
 * ChnReadJPEG --
 *
 *	This procedure is called by the photo image type to read
 *	JPEG format data from a channel, and give it to
 *	the photo image.
 *
 * Results:
 *	A standard TCL completion code.  If TCL_ERROR is returned
 *	then an error message is left in interp->result.
 *
 * Side effects:
 *	New data is added to the image given by imageHandle.
 *
 *----------------------------------------------------------------------
 */

static int
ChnReadJPEG(interp, chan, fileName, format, imageHandle, destX, destY,
	width, height, srcX, srcY)
    Tcl_Interp *interp;		/* Interpreter to use for reporting errors. */
    Tcl_Channel chan;		/* The image channel, open for reading. */
    CONST char *fileName;	/* The name of the image file. */
    Tcl_Obj *format;		/* User-specified format string, or NULL. */
    Tk_PhotoHandle imageHandle;	/* The photo image to write into. */
    int destX, destY;		/* Coordinates of top-left pixel in
				 * photo image to be written to. */
    int width, height;		/* Dimensions of block of photo image to
				 * be written to. */
    int srcX, srcY;		/* Coordinates of top-left pixel to be used
				 * in image being read. */
{
    struct jpeg_decompress_struct cinfo; /* libjpeg's parameter structure */
    struct my_error_mgr jerror;	/* for controlling libjpeg error handling */
    int result;

    if (load_jpeg_library(interp) != TCL_OK) {
	return TCL_ERROR;
    }

    /* Initialize JPEG error handler */
    /* We set up the normal JPEG error routines, then override error_exit. */
    cinfo.err = jpeg.std_error(&jerror.pub);
    jerror.pub.error_exit = my_error_exit;
    jerror.pub.output_message = my_output_message;

    /* Establish the setjmp return context for my_error_exit to use. */
    if (setjmp(jerror.setjmp_buffer)) {
      /* If we get here, the JPEG code has signaled an error. */
      Tcl_AppendResult(interp, "couldn't read JPEG string: ", (char *) NULL);
      append_jpeg_message(interp, (j_common_ptr) &cinfo);
      jpeg.destroy_decompress(&cinfo);
      return TCL_ERROR;
    }

    /* Now we can initialize libjpeg. */
    jpeg.CreateDecompress(&cinfo, JPEG_LIB_VERSION,
			(size_t) sizeof(struct jpeg_decompress_struct));
    jpeg_channel_src(&cinfo, chan);

    /* Share code with ObjReadJPEG. */
    result = CommonReadJPEG(interp, &cinfo, format, imageHandle,
			    destX, destY, width, height, srcX, srcY);

    /* Reclaim libjpeg's internal resources. */
    jpeg.destroy_decompress(&cinfo);

    return result;
}

/*
 *----------------------------------------------------------------------
 *
 * ObjReadJPEG --
 *
 *	This procedure is called by the photo image type to read
 *	JPEG format data from a base64 encoded string, and give it to
 *	the photo image.
 *
 * Results:
 *	A standard TCL completion code.  If TCL_ERROR is returned
 *	then an error message is left in interp->result.
 *
 * Side effects:
 *	New data is added to the image given by imageHandle.
 *
 *----------------------------------------------------------------------
 */

static int
ObjReadJPEG(interp, data, format, imageHandle, destX, destY,
	width, height, srcX, srcY)
    Tcl_Interp *interp;		/* Interpreter to use for reporting errors. */
    Tcl_Obj *data;		/* Object containing the image data. */
    Tcl_Obj *format;		/* User-specified format object, or NULL. */
    Tk_PhotoHandle imageHandle;	/* The photo image to write into. */
    int destX, destY;		/* Coordinates of top-left pixel in
				 * photo image to be written to. */
    int width, height;		/* Dimensions of block of photo image to
				 * be written to. */
    int srcX, srcY;		/* Coordinates of top-left pixel to be used
				 * in image being read. */
{
    struct jpeg_decompress_struct cinfo; /* libjpeg's parameter structure */
    struct my_error_mgr jerror;	/* for controlling libjpeg error handling */
    int result;

    if (load_jpeg_library(interp) != TCL_OK) {
	return TCL_ERROR;
    }

    /* Initialize JPEG error handler */
    /* We set up the normal JPEG error routines, then override error_exit. */
    cinfo.err = jpeg.std_error(&jerror.pub);
    jerror.pub.error_exit = my_error_exit;
    jerror.pub.output_message = my_output_message;

    /* Establish the setjmp return context for my_error_exit to use. */
    if (setjmp(jerror.setjmp_buffer)) {
      /* If we get here, the JPEG code has signaled an error. */
      Tcl_AppendResult(interp, "couldn't read JPEG string: ", (char *) NULL);
      append_jpeg_message(interp, (j_common_ptr) &cinfo);
      jpeg.destroy_decompress(&cinfo);
      return TCL_ERROR;
    }

    /* Now we can initialize libjpeg. */
    jpeg.CreateDecompress(&cinfo, JPEG_LIB_VERSION,
			(size_t) sizeof(struct jpeg_decompress_struct));
    jpeg_obj_src(&cinfo, data);

    /* Share code with ChnReadJPEG. */
    result = CommonReadJPEG(interp, &cinfo, format, imageHandle,
			    destX, destY, width, height, srcX, srcY);

    /* Reclaim libjpeg's internal resources. */
    jpeg.destroy_decompress(&cinfo);

    return result;
}

/*
 *----------------------------------------------------------------------
 *
 * CommonReadJPEG --
 *
 *	The common guts of ChnReadJPEG and ObjReadJPEG.
 *	The decompress struct has already been set up and the
 *	appropriate data source manager initialized.
 *	The caller should do jpeg_destroy_decompress upon return.
 *
 *----------------------------------------------------------------------
 */

typedef struct myblock {
    Tk_PhotoImageBlock ck;
    int dummy; /* extra space for offset[3], if not included already
		  in Tk_PhotoImageBlock */
} myblock;

static int
CommonReadJPEG(interp, cinfo, format, imageHandle, destX, destY,
	width, height, srcX, srcY)
    Tcl_Interp *interp;		/* Interpreter to use for reporting errors. */
    j_decompress_ptr cinfo;	/* Already-constructed decompress struct. */
    Tcl_Obj *format;		/* User-specified format string, or NULL. */
    Tk_PhotoHandle imageHandle;	/* The photo image to write into. */
    int destX, destY;		/* Coordinates of top-left pixel in
				 * photo image to be written to. */
    int width, height;		/* Dimensions of block of photo image to
				 * be written to. */
    int srcX, srcY;		/* Coordinates of top-left pixel to be used
				 * in image being read. */
{
    static char *jpegReadOptions[] = {"-fast", "-grayscale", NULL};
    int fileWidth, fileHeight, stopY, curY, outY, outWidth, outHeight;
    myblock bl;
#define block bl.ck
    JSAMPARRAY buffer;		/* Output row buffer */
    int objc, i, index;
    Tcl_Obj **objv = (Tcl_Obj **) NULL;

    /* Ready to read header data. */
    jpeg.read_header(cinfo, TRUE);

    /* This code only supports 8-bit-precision JPEG files. */
    if ((cinfo->data_precision != 8) ||
	    (sizeof(JSAMPLE) != sizeof(unsigned char))) {
	Tcl_AppendResult(interp, "Unsupported JPEG precision", (char *) NULL);
	return TCL_ERROR;
    }

    /* Process format parameters. */
    if (ImgListObjGetElements(interp, format, &objc, &objv) != TCL_OK) {
	return TCL_ERROR;
    }
    if (objc) {
	for (i=1; i<objc; i++) {
	    if (Tcl_GetIndexFromObj(interp, objv[i], jpegReadOptions, "format option",
		    0, &index)!=TCL_OK) {
		return TCL_ERROR;
	    }
	    switch (index) {
		case 0: {
		    /* Select fast processing mode. */
		    cinfo->two_pass_quantize = FALSE;
		    cinfo->dither_mode = JDITHER_ORDERED;
		    cinfo->dct_method = JDCT_FASTEST;
		    cinfo->do_fancy_upsampling = FALSE;
		    break;
		}
		case 1: {
		    /* Force monochrome output. */
		    cinfo->out_color_space = JCS_GRAYSCALE;
		    break;
		}
	    }
	}
    }

    jpeg.start_decompress(cinfo);

    /* Check dimensions. */
    fileWidth = (int) cinfo->output_width;
    fileHeight = (int) cinfo->output_height;
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

    /* Check colorspace. */
    switch (cinfo->out_color_space) {
    case JCS_GRAYSCALE:
	/* a single-sample grayscale pixel is expanded into equal R,G,B values */
	block.pixelSize = 1;
	block.offset[0] = 0;
	block.offset[1] = 0;
	block.offset[2] = 0;
	break;
    case JCS_RGB:
	/* note: this pixel layout assumes default configuration of libjpeg. */
	block.pixelSize = 3;
	block.offset[0] = 0;
	block.offset[1] = 1;
	block.offset[2] = 2;
	break;
    default:
	Tcl_AppendResult(interp, "Unsupported JPEG color space", (char *) NULL);
	return TCL_ERROR;
    }
    block.width = outWidth;
    block.height = 1;
    block.pitch = block.pixelSize * fileWidth;
    block.offset[3] = block.offset[0];

    Tk_PhotoExpand(imageHandle, destX + outWidth, destY + outHeight);

    /* Make a temporary one-row-high sample array */
    buffer = (*cinfo->mem->alloc_sarray)
		((j_common_ptr) cinfo, JPOOL_IMAGE,
		 cinfo->output_width * cinfo->output_components, 1);
    block.pixelPtr = (unsigned char *) buffer[0] + srcX * block.pixelSize;

    /* Read as much of the data as we need to */
    stopY = srcY + outHeight;
    outY = destY;
    for (curY = 0; curY < stopY; curY++) {
      jpeg.read_scanlines(cinfo, buffer, 1);
      if (curY >= srcY) {
#if (TK_MAJOR_VERSION > 8) || ((TK_MAJOR_VERSION == 8) && (TK_MINOR_VERSION > 3))
	Tk_PhotoPutBlock(imageHandle, &block, destX, outY, outWidth, 1, TK_PHOTO_COMPOSITE_OVERLAY);
#else
	Tk_PhotoPutBlock(imageHandle, &block, destX, outY, outWidth, 1);
#endif
	outY++;
      }
    }

    /* Do normal cleanup if we read the whole image; else early abort */
    if (cinfo->output_scanline == cinfo->output_height)
	jpeg.finish_decompress(cinfo);
    else
	jpeg.abort_decompress(cinfo);

    return TCL_OK;
}

/*
 *----------------------------------------------------------------------
 *
 * ChnWriteJPEG --
 *
 *	This procedure is invoked to write image data to a file in JPEG
 *	format.
 *
 * Results:
 *	A standard TCL completion code.  If TCL_ERROR is returned
 *	then an error message is left in interp->result.
 *
 * Side effects:
 *	Data is written to the file given by "fileName".
 *
 *----------------------------------------------------------------------
 */

static int
ChnWriteJPEG(interp, fileName, format, blockPtr)
    Tcl_Interp *interp;
    CONST char *fileName;
    Tcl_Obj *format;
    Tk_PhotoImageBlock *blockPtr;
{
    struct jpeg_compress_struct cinfo; /* libjpeg's parameter structure */
    struct my_error_mgr jerror;	/* for controlling libjpeg error handling */
    Tcl_Channel chan;
    int result;

    if (load_jpeg_library(interp) != TCL_OK) {
	return TCL_ERROR;
    }

    chan = ImgOpenFileChannel(interp, fileName, 0644);
    if (!chan) {
	return TCL_ERROR;
    }

    /* Initialize JPEG error handler */
    /* We set up the normal JPEG error routines, then override error_exit. */
    cinfo.err = jpeg.std_error(&jerror.pub);
    jerror.pub.error_exit = my_error_exit;
    jerror.pub.output_message = my_output_message;

    /* Establish the setjmp return context for my_error_exit to use. */
    if (setjmp(jerror.setjmp_buffer)) {
      /* If we get here, the JPEG code has signaled an error. */
      Tcl_AppendResult(interp, "couldn't write JPEG file \"", fileName,
		       "\": ", (char *) NULL);
      append_jpeg_message(interp, (j_common_ptr) &cinfo);
      jpeg.destroy_compress(&cinfo);
      Tcl_Close(interp, chan);
      return TCL_ERROR;
    }

    /* Now we can initialize libjpeg. */
    jpeg.CreateCompress(&cinfo, JPEG_LIB_VERSION,
			(size_t) sizeof(struct jpeg_compress_struct));
    jpeg_channel_dest(&cinfo, chan);

    /* Share code with StringWriteJPEG. */
    result = CommonWriteJPEG(interp, &cinfo, format, blockPtr);

    jpeg.destroy_compress(&cinfo);
    if (Tcl_Close(interp, chan) == TCL_ERROR) {
	return TCL_ERROR;
    }
    return result;
}

/*
 *----------------------------------------------------------------------
 *
 * StringWriteJPEG --
 *
 *	This procedure is called by the photo image type to write
 *	JPEG format data to a base-64 encoded string from the photo block.
 *
 * Results:
 *	A standard TCL completion code.  If TCL_ERROR is returned
 *	then an error message is left in interp->result.
 *
 * Side effects:
 *	None.
 *
 *----------------------------------------------------------------------
 */

static int
StringWriteJPEG(interp, dataPtr, format, blockPtr)
    Tcl_Interp *interp;
    Tcl_DString *dataPtr;
    Tcl_Obj *format;
    Tk_PhotoImageBlock *blockPtr;
{
    struct jpeg_compress_struct cinfo; /* libjpeg's parameter structure */
    struct my_error_mgr jerror;	/* for controlling libjpeg error handling */
    int result;
    Tcl_DString data;

    if (load_jpeg_library(interp) != TCL_OK) {
	return TCL_ERROR;
    }

    ImgFixStringWriteProc(&data, &interp, &dataPtr, &format, &blockPtr);

    /* Initialize JPEG error handler */
    /* We set up the normal JPEG error routines, then override error_exit. */
    cinfo.err = jpeg.std_error(&jerror.pub);
    jerror.pub.error_exit = my_error_exit;
    jerror.pub.output_message = my_output_message;

    /* Establish the setjmp return context for my_error_exit to use. */
    if (setjmp(jerror.setjmp_buffer)) {
      /* If we get here, the JPEG code has signaled an error. */
      Tcl_AppendResult(interp, "couldn't write JPEG string: ", (char *) NULL);
      append_jpeg_message(interp, (j_common_ptr) &cinfo);
      result = TCL_ERROR;
      goto writeend;
    }

    /* Now we can initialize libjpeg. */
    jpeg.CreateCompress(&cinfo, JPEG_LIB_VERSION,
	    (size_t) sizeof(struct jpeg_compress_struct));
    jpeg_string_dest(&cinfo, dataPtr);

    /* Share code with ChnWriteJPEG. */
    result = CommonWriteJPEG(interp, &cinfo, format, blockPtr);

writeend:

    jpeg.destroy_compress(&cinfo);
    if (dataPtr == &data) {
	if (result == TCL_OK) {
	    Tcl_DStringResult(interp, dataPtr);
	} else {
	    Tcl_DStringFree(dataPtr);
	}
    }

    return result;
}

/*
 *----------------------------------------------------------------------
 *
 * CommonWriteJPEG --
 *
 *	The common guts of ChnWriteJPEG and StringWriteJPEG.
 *	The compress struct has already been set up and the
 *	appropriate data destination manager initialized.
 *	The caller should do jpeg_destroy_compress upon return,
 *	and also close the destination as necessary.
 *
 *----------------------------------------------------------------------
 */

static int
CommonWriteJPEG(interp, cinfo, format, blockPtr)
    Tcl_Interp *interp;
    j_compress_ptr cinfo;	
    Tcl_Obj *format;
    Tk_PhotoImageBlock *blockPtr;
{
    static char *jpegWriteOptions[] = {"-grayscale", "-optimize",
	"-progressive", "-quality", "-smooth", NULL};
    JSAMPROW row_pointer[1];	/* pointer to original data scanlines */
    JSAMPARRAY buffer;		/* Intermediate row buffer */
    JSAMPROW bufferPtr;
    int w, h;
    int greenOffset, blueOffset, alphaOffset;
    unsigned char *pixelPtr, *pixLinePtr;
    int objc, i, index, grayscale = 0;
    Tcl_Obj **objv = (Tcl_Obj **) NULL;

    greenOffset = blockPtr->offset[1] - blockPtr->offset[0];
    blueOffset = blockPtr->offset[2] - blockPtr->offset[0];
    alphaOffset = blockPtr->offset[0];
    if (alphaOffset < blockPtr->offset[2]) {
        alphaOffset = blockPtr->offset[2];
    }
    if (++alphaOffset < blockPtr->pixelSize) {
	alphaOffset -= blockPtr->offset[0];
    } else {
	alphaOffset = 0;
    }

    /* Set up JPEG compression parameters. */
    cinfo->image_width = blockPtr->width;
    cinfo->image_height = blockPtr->height;
    cinfo->input_components = 3;
    cinfo->in_color_space = JCS_RGB;

    jpeg.set_defaults(cinfo);

    /* Parse options, if any, and alter default parameters */

    if (ImgListObjGetElements(interp, format, &objc, &objv) != TCL_OK) {
	return TCL_ERROR;
    }
    if (objc) {
	for (i=1; i<objc; i++) {
	    if (Tcl_GetIndexFromObj(interp, objv[i], jpegWriteOptions,
		    "format option", 0, &index)!=TCL_OK) {
		return TCL_ERROR;
	    }
	    switch (index) {
		case 0: {
		    grayscale = 1;
		    break;
		}
		case 1: {
		    cinfo->optimize_coding = TRUE;
		    break;
		}
		case 2: {
		    if (jpeg.simple_progression != NULL) {
			/* Select simple progressive mode. */
			jpeg.simple_progression(cinfo);
		    }
		    break;
		}
		case 3: {
		    int quality = 0;
		    if (++i >= objc) {
			Tcl_AppendResult(interp, "No value for option \"",
				Tcl_GetStringFromObj(objv[--i], (int *) NULL), "\"", (char *) NULL);
			return TCL_ERROR;
		    }
		    if (Tcl_GetIntFromObj(interp, objv[i], &quality) != TCL_OK) {
			return TCL_ERROR;
		    }
		    jpeg.set_quality(cinfo, quality, FALSE);
		    break;
		}
		case 4: {
		    int smooth = 0;
		    if (++i >= objc) {
			Tcl_AppendResult(interp, "No value for option \"",
				Tcl_GetStringFromObj(objv[--i], (int *) NULL), "\"", (char *) NULL);
			return TCL_ERROR;
		    }
		    if (Tcl_GetIntFromObj(interp, objv[i], &smooth) != TCL_OK) {
			return TCL_ERROR;
		    }
		    cinfo->smoothing_factor = smooth;
		    break;
		}
	    }
	}
    }

    pixLinePtr = blockPtr->pixelPtr + blockPtr->offset[0];
    greenOffset = blockPtr->offset[1] - blockPtr->offset[0];
    blueOffset = blockPtr->offset[2] - blockPtr->offset[0];
    if ((jpeg.set_colorspace != NULL) &&
	    (grayscale || (!greenOffset && !blueOffset))) {
	/* Generate monochrome JPEG file if source block is grayscale. */
	jpeg.set_colorspace(cinfo, JCS_GRAYSCALE);
    }

    jpeg.start_compress(cinfo, TRUE);
    
    /* note: we assume libjpeg is configured for standard RGB pixel order. */
    if ((greenOffset == 1) && (blueOffset == 2)
	&& (blockPtr->pixelSize == 3)) {
	/* No need to reformat pixels before passing data to libjpeg */
	for (h = blockPtr->height; h > 0; h--) {
	    row_pointer[0] = (JSAMPROW) pixLinePtr;
	    jpeg.write_scanlines(cinfo, row_pointer, 1);
	    pixLinePtr += blockPtr->pitch;
	}
    } else {
	/* Must convert data format.  Create a one-scanline work buffer. */
	buffer = (*cinfo->mem->alloc_sarray)
	  ((j_common_ptr) cinfo, JPOOL_IMAGE,
	   cinfo->image_width * cinfo->input_components, 1);
	for (h = blockPtr->height; h > 0; h--) {
	    pixelPtr = pixLinePtr;
	    bufferPtr = buffer[0];
	    for (w = blockPtr->width; w > 0; w--) {
		if (alphaOffset && !pixelPtr[alphaOffset]) {
		    /* if pixel is transparant, better use gray
		     * than the default black.
		     */
		    *bufferPtr++ = 0xd9;
		    *bufferPtr++ = 0xd9;
		    *bufferPtr++ = 0xd9;
		} else {
		    *bufferPtr++ = pixelPtr[0];
		    *bufferPtr++ = pixelPtr[greenOffset];
		    *bufferPtr++ = pixelPtr[blueOffset];
		}
		pixelPtr += blockPtr->pixelSize;
	    }
	    jpeg.write_scanlines(cinfo, buffer, 1);
	    pixLinePtr += blockPtr->pitch;
	}
    }

    jpeg.finish_compress(cinfo);
    return TCL_OK;
}

/*
 * libjpeg source manager for reading from base64-encoded strings
 * and from Tcl_Channels.
 */

static void
jpeg_obj_src (cinfo, dataObj)
    j_decompress_ptr cinfo;
    Tcl_Obj *dataObj;
{
  src_ptr src;

  src = (src_ptr)
      (*cinfo->mem->alloc_small) ((j_common_ptr) cinfo, JPOOL_PERMANENT,
				  sizeof(struct source_mgr));
  cinfo->src = (struct jpeg_source_mgr *) src;

  src->pub.init_source = dummy_source;
  src->pub.fill_input_buffer = fill_input_buffer;
  src->pub.skip_input_data = skip_input_data;
  src->pub.resync_to_restart = jpeg.resync_to_restart; /* use default method */
  src->pub.term_source = dummy_source;

  ImgReadInit(dataObj, '\377', &src->handle);

  src->pub.bytes_in_buffer = 0; /* forces fill_input_buffer on first read */
  src->pub.next_input_byte = NULL; /* until buffer loaded */
}

static boolean
fill_input_buffer(cinfo)
    j_decompress_ptr cinfo;
{
  src_ptr src = (src_ptr) cinfo->src;
  int nbytes;

  nbytes = ImgRead(&src->handle, src->buffer, STRING_BUF_SIZE);

  if (nbytes <= 0) {
    /* Insert a fake EOI marker */
    src->buffer[0] = (JOCTET) 0xFF;
    src->buffer[1] = (JOCTET) JPEG_EOI;
    nbytes = 2;
  }

  src->pub.next_input_byte = src->buffer;
  src->pub.bytes_in_buffer = nbytes;

  return TRUE;
}

static void
skip_input_data(cinfo, num_bytes)
    j_decompress_ptr cinfo;
    long num_bytes;
{
  src_ptr src = (src_ptr) cinfo->src;

  if (num_bytes > 0) {
    while (num_bytes > (long) src->pub.bytes_in_buffer) {
      num_bytes -= (long) src->pub.bytes_in_buffer;
      fill_input_buffer(cinfo);
    }
    src->pub.next_input_byte += (size_t) num_bytes;
    src->pub.bytes_in_buffer -= (size_t) num_bytes;
  }
}

static void
dummy_source(cinfo)
    j_decompress_ptr cinfo;
{
  /* no work necessary here */
}

/*
 * libjpeg source manager for reading from channels.
 */
static void
jpeg_channel_src (cinfo, chan)
    j_decompress_ptr cinfo;
    Tcl_Channel chan;
{
  src_ptr src;

  src = (src_ptr)
      (*cinfo->mem->alloc_small) ((j_common_ptr) cinfo, JPOOL_PERMANENT,
				  sizeof(struct source_mgr));
  cinfo->src = (struct jpeg_source_mgr *) src;

  src->pub.init_source = dummy_source;
  src->pub.fill_input_buffer = fill_input_buffer;
  src->pub.skip_input_data = skip_input_data;
  src->pub.resync_to_restart = jpeg.resync_to_restart; /* use default method */
  src->pub.term_source = dummy_source;

  src->handle.data = (char *) chan;
  src->handle.state = IMG_CHAN;

  src->pub.bytes_in_buffer = 0; /* forces fill_input_buffer on first read */
  src->pub.next_input_byte = NULL; /* until buffer loaded */
}


/*
 * libjpeg destination manager for writing to base64-encoded strings
 * and Tcl_Channel's.
 */

static void
jpeg_string_dest (cinfo, dstring)
    j_compress_ptr cinfo;
    Tcl_DString* dstring;
{
  dest_ptr dest;

  if (cinfo->dest == NULL) {	/* first time for this JPEG object? */
    cinfo->dest = (struct jpeg_destination_mgr *)
      (*cinfo->mem->alloc_small) ((j_common_ptr) cinfo, JPOOL_PERMANENT,
				  sizeof(struct destination_mgr));
  }

  dest = (dest_ptr) cinfo->dest;
  dest->pub.init_destination = my_init_destination;
  dest->pub.empty_output_buffer = my_empty_output_buffer;
  dest->pub.term_destination = my_term_destination;
  Tcl_DStringSetLength(dstring, dstring->spaceAvl);
  dest->handle.buffer = dstring;
  dest->handle.data = Tcl_DStringValue(dstring);
  dest->handle.state = 0;
  dest->handle.length = 0;
}

static void
jpeg_channel_dest (cinfo, chan)
    j_compress_ptr cinfo;
    Tcl_Channel chan;
{
  dest_ptr dest;

  if (cinfo->dest == NULL) {	/* first time for this JPEG object? */
    cinfo->dest = (struct jpeg_destination_mgr *)
      (*cinfo->mem->alloc_small) ((j_common_ptr) cinfo, JPOOL_PERMANENT,
				  sizeof(struct destination_mgr));
  }

  dest = (dest_ptr) cinfo->dest;
  dest->pub.init_destination = my_init_destination;
  dest->pub.empty_output_buffer = my_empty_output_buffer;
  dest->pub.term_destination = my_term_destination;
  dest->handle.data = (char *) chan;
  dest->handle.state = IMG_CHAN;
}

static void
my_init_destination (cinfo)
    j_compress_ptr cinfo;
{
  dest_ptr dest = (dest_ptr) cinfo->dest;
  dest->pub.next_output_byte = dest->buffer;
  dest->pub.free_in_buffer = STRING_BUF_SIZE;
}

static boolean
my_empty_output_buffer (cinfo)
    j_compress_ptr cinfo;
{
  dest_ptr dest = (dest_ptr) cinfo->dest;
  if (ImgWrite(&dest->handle, (char *) dest->buffer, STRING_BUF_SIZE)
  	!= STRING_BUF_SIZE)
    ERREXIT(cinfo, JERR_FILE_WRITE);

  dest->pub.next_output_byte = dest->buffer;
  dest->pub.free_in_buffer = STRING_BUF_SIZE;

  return TRUE;
}

static void
my_term_destination (cinfo)
    j_compress_ptr cinfo;
{
  dest_ptr dest = (dest_ptr) cinfo->dest;
  int datacount = STRING_BUF_SIZE - (int) dest->pub.free_in_buffer;

  /* Write any data remaining in the buffer */
  if (datacount > 0) {
    if (ImgWrite(&dest->handle, (char *) dest->buffer, datacount)
	!= datacount)
      ERREXIT(cinfo, JERR_FILE_WRITE);
  }
  /* Empty any partial-byte from the base64 encoder */
  ImgPutc(IMG_DONE, &dest->handle);
}


/*
 * Error handler to replace (or extend, really) libjpeg's default handler
 */

static void
my_error_exit (cinfo)
    j_common_ptr cinfo;
{
  struct my_error_mgr *myerr = (struct my_error_mgr *) cinfo->err;
  /* Exit back to outer level */
  longjmp(myerr->setjmp_buffer, 1);
}

static void
append_jpeg_message (interp, cinfo)
    Tcl_Interp *interp;
    j_common_ptr cinfo;
{
  /* Append libjpeg error message to interp->result */
  char buffer[JMSG_LENGTH_MAX];
  (*cinfo->err->format_message) (cinfo, buffer);
  Tcl_AppendResult(interp, buffer, (char *) NULL);
}

static void
my_output_message (cinfo)
    j_common_ptr cinfo;
{
  /* Override libjpeg's output_message to do nothing.
   * This ensures that warning messages will not appear on stderr,
   * even for a corrupted JPEG file.  Too bad there's no way
   * to report a "warning" message to the calling Tcl script.
   */
}
