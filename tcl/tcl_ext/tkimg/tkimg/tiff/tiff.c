/*
 * tiff.c --
 *
 * A photo image file handler for TIFF files.
 *
 * Uses the libtiff.so library, which is dynamically
 * loaded only when used.
 */

/* Author : Jan Nijtmans */
/* Date   : 7/16/97      */

/*
 * Generic initialization code, parameterized via CPACKAGE and PACKAGE.
 */

#include <tcl.h>
#include <tifftcl.h>
#include <zlibtcl.h>

#ifdef HAVE_STDLIB_H
#undef HAVE_STDLIB_H
#endif
#include <jpegtcl.h>


static int SetupTiffLibrary _ANSI_ARGS_ ((Tcl_Interp *interp));

#define MORE_INITIALIZATION \
    if (SetupTiffLibrary (interp) != TCL_OK) { return TCL_ERROR; }

#include "init.c"

#include <tiffInt.h>


extern int unlink _ANSI_ARGS_((const char *));

/*
 * Prototypes for local procedures defined in this file:
 */

static int getint _ANSI_ARGS_((unsigned char *buf, TIFFDataType format,
	int order));

static int CommonMatch _ANSI_ARGS_((tkimg_MFile *handle, int *widhtPtr,
	int *heightPtr));

static int CommonRead _ANSI_ARGS_((Tcl_Interp *interp, TIFF *tif,
	Tcl_Obj *format, Tk_PhotoHandle imageHandle, int destX, int destY,
	int width, int height, int srcX, int srcY));

static int CommonWrite _ANSI_ARGS_((Tcl_Interp *interp, TIFF *tif,
	int comp, Tk_PhotoImageBlock *blockPtr));

static int ParseWriteFormat _ANSI_ARGS_((Tcl_Interp *interp, Tcl_Obj *format,
	int *comp, const char **mode));

static void  _TIFFerr    _ANSI_ARGS_((const char *, const char *, va_list));
static void  _TIFFwarn   _ANSI_ARGS_((const char *, const char *, va_list));

/*
 * The functions for the TIFF input handler
 */

static int     mapDummy _ANSI_ARGS_((thandle_t, tdata_t *, toff_t *));
static void    unMapDummy _ANSI_ARGS_((thandle_t, tdata_t, toff_t));
static int     closeDummy _ANSI_ARGS_((thandle_t));
static tsize_t writeDummy _ANSI_ARGS_((thandle_t, tdata_t, tsize_t));

static tsize_t readMFile _ANSI_ARGS_((thandle_t, tdata_t, tsize_t));
static toff_t  seekMFile _ANSI_ARGS_((thandle_t, toff_t, int));
static toff_t  sizeMFile _ANSI_ARGS_((thandle_t));

static tsize_t readString _ANSI_ARGS_((thandle_t, tdata_t, tsize_t));
static tsize_t writeString _ANSI_ARGS_((thandle_t, tdata_t, tsize_t));
static toff_t  seekString _ANSI_ARGS_((thandle_t, toff_t, int));
static toff_t  sizeString _ANSI_ARGS_((thandle_t));


static char *errorMessage = NULL;

static int
SetupTiffLibrary (interp)
    Tcl_Interp *interp;
{
    static int initialized = 0;

    if (Tifftcl_InitStubs(interp, TIFFTCL_VERSION, 0) == NULL) {
        return TCL_ERROR;
    }

    if (errorMessage) {
	ckfree(errorMessage);
	errorMessage = NULL;
    }
    if (TIFFSetErrorHandler != NULL) {
        TIFFSetErrorHandler(_TIFFerr);
    }
    if (TIFFSetWarningHandler != NULL) {
	TIFFSetWarningHandler(_TIFFwarn);
    }

    /*
     * Initialize jpeg and zlib too, for use by the CODEC's we register
     * with the base TIFF library in this package.
     */

    if (Jpegtcl_InitStubs(interp, "1.0", 0) == NULL) {
        return TCL_ERROR;
    }

    if (!initialized) {
	initialized = 1;
	if (
	    TIFFRegisterCODEC   && TIFFError        && TIFFPredictorInit &&
	    TIFFMergeFieldInfo  && TIFFFlushData1   && _TIFFNoPostDecode &&
	    TIFFTileRowSize     && TIFFScanlineSize && _TIFFsetByteArray &&
	    TIFFVSetField       && TIFFSwabArrayOfShort
	    ) {

	  if (Zlibtcl_InitStubs(interp, "1.0", 0) == NULL) {
	    return TCL_ERROR;
	  }
	  TIFFRegisterCODEC (COMPRESSION_DEFLATE,  "Deflate",  TkimgTIFFInitZip);
	  TIFFRegisterCODEC (COMPRESSION_ADOBE_DEFLATE, "AdobeDeflate", TkimgTIFFInitZip);

	  if (Jpegtcl_InitStubs(interp, "1.0", 0) == NULL) {
	    return TCL_ERROR;
	  }
	  TIFFRegisterCODEC (COMPRESSION_JPEG,     "JPEG",     TkimgTIFFInitJpeg);
	  TIFFRegisterCODEC (COMPRESSION_PIXARLOG, "PixarLog", TkimgTIFFInitPixar);
	}
    }
    return TCL_OK;
}



static int
getint(buf, format, order)
    unsigned char *buf;
    TIFFDataType format;
    int order;
{
    int result;

    switch (format) {
	case TIFF_BYTE:
	    result = buf[0]; break;
	case TIFF_SHORT:
	    result = (buf[order]<<8) + buf[1-order]; break;
	case TIFF_LONG:
	    if (order) {
		result = (buf[3]<<24) + (buf[2]<<16) + (buf[1]<<8) + buf[0];
	    } else {
		result = (buf[0]<<24) + (buf[1]<<16) + (buf[2]<<8) + buf[3];
	    }; break;
	default:
	    result = -1;
    }
    return result;
}


static void
_TIFFerr(module, fmt, ap)
     const char *module;
     const char *fmt;
     va_list     ap;
{
  char buf [2048];
  char *cp = buf;

  if (module != NULL) {
    sprintf(cp, "%s: ", module);
    cp += strlen(module) + 2;
  }

  vsprintf(cp, fmt, ap);
  if (errorMessage) {
    ckfree(errorMessage);
  }
  errorMessage = (char *) ckalloc(strlen(buf)+1);
  strcpy(errorMessage, buf);
}

/* warnings are not processed in Tcl */
static void
_TIFFwarn(module, fmt, ap)
     const char *module;
     const char *fmt;
     va_list     ap;
{
}

static int
mapDummy(fd, base, size)
    thandle_t fd;
    tdata_t *base;
    toff_t *size;
{
    return (toff_t) 0;
}

static void
unMapDummy(fd, base, size)
    thandle_t fd;
    tdata_t base;
    toff_t size;
{
}

static int
closeDummy(fd)
    thandle_t fd;
{
    return 0;
}

static tsize_t
writeDummy(fd, data, size)
    thandle_t fd;
    tdata_t data;
    tsize_t size;
{
   return size;
}

static tsize_t
readMFile(fd, data, size)
    thandle_t fd;
    tdata_t data;
    tsize_t size;
{
    return (tsize_t) tkimg_Read((tkimg_MFile *) fd, (char *) data, (int) size) ;
}

static toff_t
seekMFile(fd, off, whence)
    thandle_t fd;
    toff_t off;
    int whence;
{
    return Tcl_Seek((Tcl_Channel) ((tkimg_MFile *) fd)->data, (int) off, whence);
}

static toff_t
sizeMFile(fd)
    thandle_t fd;
{
    int fsize;
    return (fsize = Tcl_Seek((Tcl_Channel) ((tkimg_MFile *) fd)->data,
	    (int) 0, SEEK_END)) < 0 ? 0 : (toff_t) fsize;
}

/*
 * In the following functions "handle" is used differently for speed reasons:
 *
 *	handle.buffer   (writing only) dstring used for writing.
 *	handle.data	pointer to first character
 *	handle.lenght	size of data
 *	handle.state	"file" position pointer.
 *
 * After a read, only the position pointer is adapted, not the other fields.
 */

static tsize_t
readString(fd, data, size)
    thandle_t fd;
    tdata_t data;
    tsize_t size;
{
    register tkimg_MFile *handle = (tkimg_MFile *) fd;

    if ((size + handle->state) > handle->length) {
	size = handle->length - handle->state;
    }
    if (size) {
	memcpy((char *) data, handle->data + handle->state, (size_t) size);
	handle->state += size;
    }
    return size;
}

static tsize_t
writeString(fd, data, size)
    thandle_t fd;
    tdata_t data;
    tsize_t size;
{
    register tkimg_MFile *handle = (tkimg_MFile *) fd;

    if (handle->state + size > handle->length) {
	handle->length = handle->state + size;
	Tcl_DStringSetLength(handle->buffer, handle->length);
	handle->data = Tcl_DStringValue(handle->buffer);
    }
    memcpy(handle->data + handle->state, (char *) data, (size_t) size);
    handle->state += size;
    return size;
}

static toff_t
seekString(fd, off, whence)
    thandle_t fd;
    toff_t off;
    int whence;
{
    register tkimg_MFile *handle = (tkimg_MFile *) fd;

    switch (whence) {
	case SEEK_SET:
	    handle->state = (int) off;
	    break;
	case SEEK_CUR:
	    handle->state += (int) off;
	    break;
	case SEEK_END:
	    handle->state = handle->length + (int) off;
	    break;
    }
    if (handle->state < 0) {
	handle->state = 0;
	return -1;
    }
    return (toff_t) handle->state;
}

static toff_t
sizeString(fd)
    thandle_t fd;
{
    return ((tkimg_MFile *) fd)->length;
}


/*
 *----------------------------------------------------------------------
 *
 * ObjMatchTIFF --
 *
 *  This procedure is invoked by the photo image type to see if
 *  a string contains image data in TIFF format.
 *
 * Results:
 *  The return value is 1 if the first characters in the string
 *  is like TIFF data, and 0 otherwise.
 *
 * Side effects:
 *  the size of the image is placed in widthPre and heightPtr.
 *
 *----------------------------------------------------------------------
 */

static int
ObjMatch(interp, data, format, widthPtr, heightPtr)
    Tcl_Interp *interp;
    Tcl_Obj *data;		/* the object containing the image data */
    Tcl_Obj *format;		/* the image format string */
    int *widthPtr;		/* where to put the string width */
    int *heightPtr;		/* where to put the string height */
{
    tkimg_MFile handle;

    tkimg_FixObjMatchProc(&interp, &data, &format, &widthPtr, &heightPtr);

    if (!tkimg_ReadInit(data, '\111', &handle) &&
	    !tkimg_ReadInit(data, '\115', &handle)) {
	return 0;
    }

    return CommonMatch(&handle, widthPtr, heightPtr);
}

static int
ChnMatch(interp, chan, fileName, format, widthPtr, heightPtr)
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

    return CommonMatch(&handle, widthPtr, heightPtr);
}

static int
CommonMatch(handle, widthPtr, heightPtr)
    tkimg_MFile *handle;
    int *widthPtr, *heightPtr;
{
    unsigned char buf[4096];
    int i, j, order, w = 0, h = 0;

    i = tkimg_Read(handle, (char *) buf, 8);
    order = (buf[0] == '\111');
    if ((i != 8) || (buf[0] != buf[1])
	    || ((buf[0] != '\111') && (buf[0] != '\115'))
	    || (getint(buf+2,TIFF_SHORT,order) != 42)) {
	return 0;
    }
    i = getint(buf+4,TIFF_LONG,order);

    while (i > 4104) {
	i -= 4096;
	tkimg_Read(handle, (char *) buf, 4096);
    }
    if (i>8) {
        tkimg_Read(handle, (char *) buf, i-8);
    }
    tkimg_Read(handle, (char *) buf, 2);
    i = getint(buf,TIFF_SHORT,order);
    while (i--) {
	tkimg_Read(handle, (char *) buf, 12);
	if (buf[order]!=1) continue;
	j = getint(buf+2,TIFF_SHORT,order);
	j = getint(buf+8, (TIFFDataType) j, order);
	if (buf[1-order]==0) {
	    w = j;
	    if (h>0) break;
	} else if (buf[1-order]==1) {
	    h = j;
	    if (w>0) break;
	}
    }

    if ((w <= 0) || (h <= 0)) {
	return 0;
    }
    *widthPtr = w;
    *heightPtr = h;
    return 1;
}

static int
ObjRead(interp, data, format, imageHandle,
	destX, destY, width, height, srcX, srcY)
    Tcl_Interp *interp;
    Tcl_Obj *data;			/* object containing the image */
    Tcl_Obj *format;
    Tk_PhotoHandle imageHandle;
    int destX, destY;
    int width, height;
    int srcX, srcY;
{
    TIFF *tif;
    char tempFileName[256];
    int count, result;
    tkimg_MFile handle;
    char buffer[1024];
    char *dataPtr = NULL;

    if (!tkimg_ReadInit(data, '\115', &handle)) {
	    tkimg_ReadInit(data, '\111', &handle);
    }

    if (TIFFClientOpen) {
	tempFileName[0] = 0;
	if (handle.state != IMG_STRING) {
	    dataPtr = ckalloc((handle.length*3)/4 + 2);
	    handle.length = tkimg_Read(&handle, dataPtr, handle.length);
	    handle.data = dataPtr;
	}
	handle.state = 0;
	tif = TIFFClientOpen("inline data", "r", (thandle_t) &handle,
		readString, writeString, seekString, closeDummy,
		sizeString, mapDummy, unMapDummy);
    } else {
	Tcl_Channel outchan;
	tmpnam(tempFileName);
	outchan = tkimg_OpenFileChannel(interp, tempFileName, 0644);
	if (!outchan) {
	    return TCL_ERROR;
	}

	count = tkimg_Read(&handle, buffer, 1024);
	while (count == 1024) {
	    Tcl_Write(outchan, buffer, count);
	    count = tkimg_Read(&handle, buffer, 1024);
	}
	if (count>0){
	    Tcl_Write(outchan, buffer, count);
	}
	if (Tcl_Close(interp, outchan) == TCL_ERROR) {
	    return TCL_ERROR;
	}
	tif = TIFFOpen(tempFileName, "r");
    }

    if (tif != NULL) {
	result = CommonRead(interp, tif, format, imageHandle,
		destX, destY, width, height, srcX, srcY);
    } else {
	result = TCL_ERROR;
    }
    if (tempFileName[0]) {
	unlink(tempFileName);
    }
    if (result == TCL_ERROR) {
	Tcl_AppendResult(interp, errorMessage, (char *) NULL);
	ckfree(errorMessage);
	errorMessage = NULL;
    }
    if (dataPtr) {
	ckfree(dataPtr);
    }
    return result;
}

static int
ChnRead(interp, chan, fileName, format, imageHandle,
	destX, destY, width, height, srcX, srcY)
    Tcl_Interp *interp;
    Tcl_Channel chan;
    const char *fileName;
    Tcl_Obj *format;
    Tk_PhotoHandle imageHandle;
    int destX, destY;
    int width, height;
    int srcX, srcY;
{
    TIFF *tif;
    char tempFileName[256];
    int count, result;
    char buffer[1024];

    if (TIFFClientOpen) {
	tkimg_MFile handle;
	tempFileName[0] = 0;
	handle.data = (char *) chan;
	handle.state = IMG_CHAN;
	tif = TIFFClientOpen(fileName, "r", (thandle_t) &handle,
		readMFile, writeDummy, seekMFile, closeDummy,
		sizeMFile, mapDummy, unMapDummy);
    } else {
	Tcl_Channel outchan;
	tmpnam(tempFileName);
	outchan = tkimg_OpenFileChannel(interp, tempFileName, 0644);
	if (!outchan) {
	    return TCL_ERROR;
	}

	count = Tcl_Read(chan, buffer, 1024);
	while (count == 1024) {
	    Tcl_Write(outchan, buffer, count);
	    count = Tcl_Read(chan, buffer, 1024);
	}
	if (count>0){
	    Tcl_Write(outchan, buffer, count);
	}
	if (Tcl_Close(interp, outchan) == TCL_ERROR) {
	    return TCL_ERROR;
	}

	tif = TIFFOpen(tempFileName, "r");
    }
    if (tif) {
	result = CommonRead(interp, tif, format, imageHandle,
		destX, destY, width, height, srcX, srcY);
    } else {
	result = TCL_ERROR;
    }
    if (tempFileName[0]) {
	unlink(tempFileName);
    }
    if (result == TCL_ERROR) {
	Tcl_AppendResult(interp, errorMessage, (char *) NULL);
	ckfree(errorMessage);
	errorMessage = 0;
    }
    return result;
}


typedef struct myblock {
    Tk_PhotoImageBlock ck;
    int dummy; /* extra space for offset[3], if not included already
		  in Tk_PhotoImageBlock */
} myblock;

#define block bl.ck

static int
CommonRead(interp, tif, format, imageHandle,
	destX, destY, width, height, srcX, srcY)
    Tcl_Interp *interp;
    TIFF *tif;
    Tcl_Obj *format;
    Tk_PhotoHandle imageHandle;
    int destX, destY;
    int width, height;
    int srcX, srcY;
{
    myblock bl;
    uint32 w, h;
    size_t npixels;
    uint32 *raster;

#ifdef WORDS_BIGENDIAN
    block.offset[0] = 3;
    block.offset[1] = 2;
    block.offset[2] = 1;
    block.offset[3] = 0;
#else
    block.offset[0] = 0;
    block.offset[1] = 1;
    block.offset[2] = 2;
    block.offset[3] = 3;
#endif
    block.pixelSize = sizeof (uint32);

    TIFFGetField(tif, TIFFTAG_IMAGEWIDTH, &w);
    TIFFGetField(tif, TIFFTAG_IMAGELENGTH, &h);
    npixels = w * h;

    raster = (uint32*) TkimgTIFFmalloc(npixels * sizeof (uint32));
    block.width = w;
    block.height = h;
    block.pitch = - (block.pixelSize * (int) w);
    block.pixelPtr = ((unsigned char *) raster) + ((1-h) * block.pitch);
    if (raster == NULL) {
	printf("cannot malloc\n");
	return TCL_ERROR;
    }

    if (!TIFFReadRGBAImage(tif, w, h, raster, 0) || errorMessage) {
        TkimgTIFFfree (raster);
	if (errorMessage) {
	    Tcl_AppendResult(interp, errorMessage, (char *) NULL);
	    ckfree(errorMessage);
	    errorMessage = NULL;
	}
	return TCL_ERROR;
    }

    block.pixelPtr += srcY * block.pitch
	    + srcX * block.pixelSize;
    block.offset[3] = block.offset[0]; /* don't use transparency */
    tkimg_PhotoPutBlock(interp, imageHandle, &block, destX,
			destY, width, height, TK_PHOTO_COMPOSITE_SET);

    TkimgTIFFfree (raster);
    TIFFClose(tif);
    return TCL_OK;
}

static int
StringWrite(interp, dataPtr, format, blockPtr)
    Tcl_Interp *interp;
    Tcl_DString *dataPtr;
    Tcl_Obj *format;
    Tk_PhotoImageBlock *blockPtr;
{
    TIFF *tif;
    int result, comp;
    tkimg_MFile handle;
    char tempFileName[256];
    Tcl_DString dstring;
    const char *mode;
    Tcl_DString data;

    tkimg_FixStringWriteProc(&data, &interp, &dataPtr, &format, &blockPtr);

    if (ParseWriteFormat(interp, format, &comp, &mode) != TCL_OK) {
    	return TCL_ERROR;
    }

    if (TIFFClientOpen) {
	tempFileName[0] = 0;
	Tcl_DStringInit(&dstring);
	tkimg_WriteInit(&dstring, &handle);
	tif = TIFFClientOpen("inline data", mode, (thandle_t) &handle,
		readString, writeString, seekString, closeDummy,
		sizeString, mapDummy, unMapDummy);
    } else {
	tmpnam(tempFileName);
	tif = TIFFOpen(tempFileName,mode);
    }

    result = CommonWrite(interp, tif, comp, blockPtr);
    TIFFClose(tif);

    if (result != TCL_OK) {
	if (tempFileName[0]) {
	    unlink(tempFileName);
	}
	Tcl_AppendResult(interp, errorMessage, (char *) NULL);
	ckfree(errorMessage);
	errorMessage = NULL;
	return TCL_ERROR;
    }

    if (tempFileName[0]) {
	Tcl_Channel inchan;
	char buffer[1024];
	inchan = tkimg_OpenFileChannel(interp, tempFileName, 0644);
	if (!inchan) {
	    return TCL_ERROR;
	}
	tkimg_WriteInit(dataPtr, &handle);

	result = Tcl_Read(inchan, buffer, 1024);
	while ((result == TCL_OK) && !Tcl_Eof(inchan)) {
	    tkimg_Write(&handle, buffer, result);
	    result = Tcl_Read(inchan, buffer, 1024);
	}
	if (result == TCL_OK) {
	    tkimg_Write(&handle, buffer, result);
	    result = Tcl_Close(interp, inchan);
	}
	unlink(tempFileName);
    } else {
	int length = handle.length;
	tkimg_WriteInit(dataPtr, &handle);
	tkimg_Write(&handle, Tcl_DStringValue(&dstring), length);
	Tcl_DStringFree(&dstring);
    }
    tkimg_Putc(IMG_DONE, &handle);
    if ((result == TCL_OK) && (dataPtr == &data)) {
	Tcl_DStringResult(interp, dataPtr);
    }
    return result;
}

static int
ChnWrite(interp, filename, format, blockPtr)
    Tcl_Interp *interp;
    CONST84 char *filename;
    Tcl_Obj *format;
    Tk_PhotoImageBlock *blockPtr;
{
    TIFF *tif;
    int result, comp;
    Tcl_DString nameBuffer;
    const char *fullname, *mode;

    if (!(fullname=Tcl_TranslateFileName(interp, filename, &nameBuffer))) {
	return TCL_ERROR;
    }

    if (ParseWriteFormat(interp, format, &comp, &mode) != TCL_OK) {
	Tcl_DStringFree(&nameBuffer);
    	return TCL_ERROR;
    }

    if (!(tif = TIFFOpen(fullname, mode))) {
	Tcl_AppendResult(interp, filename, ": ", Tcl_PosixError(interp),
		(char *)NULL);
	Tcl_DStringFree(&nameBuffer);
	return TCL_ERROR;
    }

    Tcl_DStringFree(&nameBuffer);

    result = CommonWrite(interp, tif, comp, blockPtr);
    TIFFClose(tif);
    return result;
}

static int
ParseWriteFormat(interp, format, comp, mode)
    Tcl_Interp *interp;
    Tcl_Obj *format;
    int *comp;
    const char **mode;
{
    static const char *const tiffWriteOptions[] = {
      "-compression",
      "-byteorder",
      NULL
    };
    int objc, length, c, i, index;
    Tcl_Obj **objv;
    const char *compression, *byteorder;

    *comp = COMPRESSION_NONE;
    *mode = "w";
    if (tkimg_ListObjGetElements(interp, format, &objc, &objv) != TCL_OK)
	return TCL_ERROR;
    if (objc) {
	compression = "none";
	byteorder = "";
	for (i=1; i<objc; i++) {
	    if (Tcl_GetIndexFromObj(interp, objv[i], (CONST84 char *CONST86 *)tiffWriteOptions,
		    "format option", 0, &index) !=TCL_OK) {
		return TCL_ERROR;
	    }
	    if (++i >= objc) {
		Tcl_AppendResult(interp, "No value for option \"",
			Tcl_GetStringFromObj(objv[--i], (int *) NULL),
			"\"", (char *) NULL);
		return TCL_ERROR;
	    }
	    switch(index) {
		case 0:
		    compression = Tcl_GetStringFromObj(objv[i], (int *) NULL); break;
		case 1:
		    byteorder = Tcl_GetStringFromObj(objv[i], (int *) NULL); break;
	    }
	}
	c = compression[0]; length = strlen(compression);
	if ((c == 'n') && (!strncmp(compression,"none",length))) {
	    *comp = COMPRESSION_NONE;
	} else if ((c == 'd') && (!strncmp(compression,"deflate",length))) {
	    *comp = COMPRESSION_DEFLATE;
	} else if ((c == 'j') && (!strncmp(compression,"jpeg",length))) {
	    *comp = COMPRESSION_JPEG;
	} else if ((c == 'l') && (!strncmp(compression,"logluv",length))) {
	    *comp = COMPRESSION_SGILOG;
/* disabled, because of patented lzw-algorithm.
	} else if ((c == 'l') && (length>1) && (!strncmp(compression,"lzw",length))) {
	    *comp = COMPRESSION_LZW;
*/
	} else if ((c == 'p') && (length>1) && (!strncmp(compression,"packbits",length))) {
	    *comp = COMPRESSION_PACKBITS;
	} else if ((c == 'p') && (length>1) && (!strncmp(compression,"pixarlog",length))) {
	    *comp = COMPRESSION_PIXARLOG;
	} else {
	    Tcl_AppendResult(interp, "invalid compression mode \"",
		     compression,"\": should be deflate, jpeg, logluv, lzw, ",
		    "packbits, pixarlog, or none", (char *) NULL);
	    return TCL_ERROR;
	}
	c = byteorder[0]; length = strlen(byteorder);
	if (c == 0) {
	    *mode = "w";
	} else if ((c == 's') && (!strncmp(byteorder,"smallendian", length))) {
	    *mode = "wl";
	} else if ((c == 'l') && (!strncmp(byteorder,"littleendian", length))) {
	    *mode = "wl";
	} else if ((c == 'b') && (!strncmp(byteorder,"bigendian", length))) {
	    *mode = "wb";
	} else if ((c == 'n') && (!strncmp(byteorder,"network", length))) {
	    *mode = "wb";
	} else {
	    Tcl_AppendResult(interp, "invalid byteorder \"",
		     byteorder,"\": should be bigendian, littleendian",
		    "network, smallendian, or {}", (char *) NULL);
	    return TCL_ERROR;
	}
    }
    return TCL_OK;
}

static int
CommonWrite(interp, tif, comp, blockPtr)
    Tcl_Interp *interp;
    TIFF *tif;
    int comp;
    Tk_PhotoImageBlock *blockPtr;
{
    int numsamples;
    unsigned char *data = NULL;

    TIFFSetField(tif, TIFFTAG_IMAGEWIDTH,  blockPtr->width);
    TIFFSetField(tif, TIFFTAG_IMAGELENGTH, blockPtr->height);
    TIFFSetField(tif, TIFFTAG_COMPRESSION, comp);

    TIFFSetField(tif, TIFFTAG_PLANARCONFIG,    PLANARCONFIG_CONTIG);
    TIFFSetField(tif, TIFFTAG_SAMPLESPERPIXEL, 1);
    TIFFSetField(tif, TIFFTAG_ORIENTATION,     ORIENTATION_TOPLEFT);
    TIFFSetField(tif, TIFFTAG_ROWSPERSTRIP,    blockPtr->height);

    TIFFSetField(tif, TIFFTAG_RESOLUTIONUNIT, (int)2);
    TIFFSetField(tif, TIFFTAG_XRESOLUTION,    (float)1200.0);
    TIFFSetField(tif, TIFFTAG_YRESOLUTION,    (float)1200.0);

    TIFFSetField(tif, TIFFTAG_BITSPERSAMPLE,   8);
    if ((blockPtr->offset[0] == blockPtr->offset[1])
	    && (blockPtr->offset[0] == blockPtr->offset[2])) {
	numsamples = 1;
	TIFFSetField(tif, TIFFTAG_SAMPLESPERPIXEL, 1);
	TIFFSetField(tif, TIFFTAG_PHOTOMETRIC,     PHOTOMETRIC_MINISBLACK);
    } else {
	numsamples = 3;
	TIFFSetField(tif, TIFFTAG_SAMPLESPERPIXEL, 3);
	TIFFSetField(tif, TIFFTAG_PHOTOMETRIC,     PHOTOMETRIC_RGB);
    }

    if ((blockPtr->pitch == numsamples * blockPtr->width)
	    && (blockPtr->pixelSize == numsamples)) {
	data = blockPtr->pixelPtr;
    } else {
	unsigned char *srcPtr, *dstPtr, *rowPtr;
	int greenOffset, blueOffset, alphaOffset, x, y;
	dstPtr = data = (unsigned char *) ckalloc(numsamples *
		blockPtr->width * blockPtr->height);
	rowPtr = blockPtr->pixelPtr + blockPtr->offset[0];
	greenOffset = blockPtr->offset[1] - blockPtr->offset[0];
	blueOffset = blockPtr->offset[2] - blockPtr->offset[0];
	alphaOffset =  blockPtr->offset[0];
	if (alphaOffset < blockPtr->offset[2]) {
	    alphaOffset = blockPtr->offset[2];
	}
	if (++alphaOffset < blockPtr->pixelSize) {
	    alphaOffset -= blockPtr->offset[0];
	} else {
	    alphaOffset = 0;
	}
	if (blueOffset || greenOffset) {
	    for (y = blockPtr->height; y > 0; y--) {
		srcPtr = rowPtr;
		for (x = blockPtr->width; x>0; x--) {
		    if (alphaOffset && !srcPtr[alphaOffset]) {
			*dstPtr++ = 0xd9;
			*dstPtr++ = 0xd9;
			*dstPtr++ = 0xd9;
		    } else {
			*dstPtr++ = srcPtr[0];
			*dstPtr++ = srcPtr[greenOffset];
			*dstPtr++ = srcPtr[blueOffset];
		    }
		    srcPtr += blockPtr->pixelSize;
		}
		rowPtr += blockPtr->pitch;
	    }
	} else {
	    for (y = blockPtr->height; y > 0; y--) {
		srcPtr = rowPtr;
		for (x = blockPtr->width; x>0; x--) {
		    *dstPtr++ = srcPtr[0];
		    srcPtr += blockPtr->pixelSize;
		}
		rowPtr += blockPtr->pitch;
	    }
	}
    }

    TIFFWriteEncodedStrip(tif, 0, data,
	    numsamples * blockPtr->width * blockPtr->height);
    if (data != blockPtr->pixelPtr) {
	ckfree((char *) data);
    }

    return TCL_OK;
}

void
TkimgTIFFfree (data)
    tdata_t data;
{
    if (_TIFFfree) {
	_TIFFfree(data);
    } else {
	ckfree((char *) data);
    }
}

tdata_t
TkimgTIFFmalloc(size)
    tsize_t size;
{
    if (_TIFFmalloc) {
	return _TIFFmalloc(size);
    } else {
	return ckalloc(size);
    }
}

tdata_t
TkimgTIFFrealloc(data, size)
    tdata_t data;
    tsize_t size;
{
    if (_TIFFrealloc) {
	return _TIFFrealloc(data, size);
    } else {
	return ckrealloc(data, size);
    }
}
