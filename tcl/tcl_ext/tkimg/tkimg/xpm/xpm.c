/*
 * xpm.c --
 *
 *	A photo image file handler for XPM files.
 *
 * Written by:
 *	Jan Nijtmans
 *	email: nijtmans@users.sourceforge.net
 *	url:   http://purl.oclc.org/net/nijtmans/
 *
 * (with some code stolen from the XPM image type and the GIF handler)
 *
 * <mweilguni@sime.com> Mario Weilguni
 * Jan 1997:
 *      - fixed a bug when reading images with invalid color tables
 *        (if a pixelchar is not in the colormap)
 *      - added FileWriteXPM, StringWriteXPM and WriteXPM.
 *      - modified FileReadXPM, ReadXPM and added StringReadXPM
 *        This makes the XPM reader complete :-)
 *
 * <paul@poSoft.de> Paul Obermeier
 * Jan 2001:
 *      - Bugfix  in CommonWriteXPM: const char *fileName was overwritten.
 *      - Bugfix  in CommonWriteXPM: If used with "-from" option, dumped core.
 *      - Improve in CommonWriteXPM: Greater channel buffersize for better
 *                                   Win98 (i.e. FAT32) performance.
 *      - Improve in ChnRead: Better performance by using ReadBuffer.
 *
 * $Id: xpm.c 262 2010-05-31 15:03:33Z nijtmans $
 */

/*
 * Generic initialization code, parameterized via CPACKAGE and PACKAGE.
 */

#include "init.c"

#if defined(__WIN32__) && !defined(__GNUC__)
#define strncasecmp strnicmp
#endif

#ifndef MAC_TCL
#include <sys/types.h>
#endif

/* constants used only in this file */

#define XPM_MONO		1
#define XPM_GRAY_4		2
#define XPM_GRAY		3
#define XPM_COLOR		4
#define XPM_SYMBOLIC		5
#define XPM_UNKNOWN		6

#define MAX_BUFFER 4096

/*
 * Prototypes for local procedures defined in this file:
 */

static int CommonRead(Tcl_Interp *interp, tkimg_MFile *handle,
	Tcl_Obj *format, Tk_PhotoHandle imageHandle,
	int destX, int destY, int width, int height,
	int srcX, int srcY);
static int CommonWrite(Tcl_Interp *interp, const char *fileName,
	Tcl_DString *dataPtr, Tcl_Obj *format,
	Tk_PhotoImageBlock *blockPtr);

static int ReadXPMFileHeader(tkimg_MFile *handle,
	int *widthPtr, int *heightPtr, int *numColors, int *byteSize);
static char *GetType(char *colorDefn, int *type_ret);
static char *GetColor(char *colorDefn, char *colorName, int *type_ret);
static char *Gets(tkimg_MFile *handle, char *buffer, int size);

/*
 *----------------------------------------------------------------------
 * Gets
 *
 *      Allows other routines to get their data from channels, files as well
 *      as Tcl_DStrings.
 *
 * Results:
 *      same as fgets: NULL pointer on any error, otherwise
 *      it returns buffer
 *
 * Side effects:
 *      The access position of the file changes OR
 *      The access pointer dataPtr->p is changed
 *
 *----------------------------------------------------------------------
 */

static char *
Gets(handle, buffer, size)
    tkimg_MFile *handle;
    char *buffer;
    int size;
{
    char *p;

    /* read data from tkimg_MFile */
    p = buffer;
    while ((tkimg_Read(handle, p, 1) == 1)) {
	if (--size <= 0) {
	    *p = 0; return buffer;
	}
	if (*p++ == '\n') {
	    *p = 0;
	    return buffer;
	}
    }
    *p = 0;
    return (p != buffer) ? buffer :(char *) NULL;
}


/*
 *----------------------------------------------------------------------
 *
 * ObjMatch --
 *
 *	This procedure is invoked by the photo image type to see if
 *	a datastring contains image data in XPM format.
 *
 * Results:
 *	The return value is >0 if the first characters in data look
 *	like XPM data, and 0 otherwise.
 *
 * Side effects:
 *	none
 *
 *----------------------------------------------------------------------
 */
static int ObjMatch(
    Tcl_Obj *data,		/* The data supplied by the image */
    Tcl_Obj *format,		/* User-specified format object, or NULL. */
    int *widthPtr,		/* The dimensions of the image are */
	int *heightPtr,			/* returned here if the file is a valid
				 * raw XPM file. */
    Tcl_Interp *interp
) {
    int numColors, byteSize;
    tkimg_MFile handle;

    handle.data = (char *)tkimg_GetStringFromObj(data, &handle.length);
    handle.state = IMG_STRING;

    return ReadXPMFileHeader(&handle, widthPtr, heightPtr, &numColors, &byteSize);
}

/*
 *----------------------------------------------------------------------
 *
 * ChnMatch --
 *
 *	This procedure is invoked by the photo image type to see if
 *	a file contains image data in XPM format.
 *
 * Results:
 *	The return value is >0 if the first characters in channel "chan"
 *	look like XPM data, and 0 otherwise.
 *
 * Side effects:
 *	The access position in chan may change.
 *
 *----------------------------------------------------------------------
 */

static int ChnMatch(
    Tcl_Channel chan,		/* The image channel, open for reading. */
    const char *fileName,	/* The name of the image file. */
    Tcl_Obj *format,		/* User-specified format object, or NULL. */
    int *widthPtr,        	/* The dimensions of the image are */
	int *heightPtr,			/* returned here if the file is a valid
				 * raw XPM file. */
    Tcl_Interp *interp
) {
    int numColors, byteSize;
    tkimg_MFile handle;

    handle.data = (char *) chan;
    handle.state = IMG_CHAN;

    return ReadXPMFileHeader(&handle, widthPtr, heightPtr, &numColors, &byteSize);
}

/*
 *----------------------------------------------------------------------
 *
 * CommonRead --
 *
 *	This procedure is called by the photo image type to read
 *	XPM format data from a file or string and write it into a
 *	given photo image.
 *
 * Results:
 *	A standard TCL completion code.  If TCL_ERROR is returned
 *	then an error message is left in interp->result.
 *
 * Side effects:
 *	The access position in file f is changed (if read from file)
 *	and new data is added to the image given by imageHandle.
 *
 *----------------------------------------------------------------------
 */
static int
CommonRead(interp, handle, format, imageHandle, destX, destY,
	width, height, srcX, srcY)
    Tcl_Interp *interp;		/* Interpreter to use for reporting errors. */
    tkimg_MFile *handle;	/* The image channel, open for reading. */
    Tcl_Obj *format;		/* User-specified format object, or NULL. */
    Tk_PhotoHandle imageHandle;	/* The photo image to write into. */
    int destX, destY;		/* Coordinates of top-left pixel in
				 * photo image to be written to. */
    int width, height;		/* Dimensions of block of photo image to
				 * be written to. */
    int srcX, srcY;		/* Coordinates of top-left pixel to be used
				 * in image being read. */
{
    int fileWidth = 0, fileHeight = 0, numColors = 0, byteSize = 0;
    int h, type;
    int nchan, matte = 1;
    unsigned char *pixelPtr;
    Tk_PhotoImageBlock block;
    Tcl_HashTable colorTable;
    Tk_Window tkwin = Tk_MainWindow(interp);
    Display *display = Tk_Display(tkwin);
    Colormap colormap = Tk_Colormap(tkwin);
    int depth = Tk_Depth(tkwin);
    char *p;
    char buffer[MAX_BUFFER];
    int i, isMono;
    int color1;
    unsigned int data;
    Tcl_HashEntry *hPtr;
    int result = TCL_OK;

    Tcl_InitHashTable(&colorTable, TCL_ONE_WORD_KEYS);

    switch ((Tk_Visual(tkwin))->class) {
      case StaticGray:
      case GrayScale:
	isMono = 1;
	break;
      default:
	isMono = 0;
    }

    type = ReadXPMFileHeader(handle, &fileWidth, &fileHeight, &numColors, &byteSize);
    if (type == 0) {
	Tcl_AppendResult(interp, "couldn't read raw XPM header", NULL);
	return TCL_ERROR;
    }
    if ((fileWidth <= 0) || (fileHeight <= 0)) {
	Tcl_AppendResult(interp, "XPM image file has dimension(s) <= 0",
		(char *) NULL);
	return TCL_ERROR;
    }
    if ((byteSize < 1) || (byteSize > 4)) {
	Tcl_AppendResult(interp, "XPM image file has invalid byte size ",
		"(should be 1, 2, 3 or 4)", (char *) NULL);
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

    for (i=0; i<numColors; i++) {
	char * colorDefn;		/* the color definition line */
	char * colorName;		/* temp place to hold the color name
					 * defined for one type of visual */
	char * useName;			/* the color name used for this
					 * color. If there are many names
					 * defined, choose the name that is
					 * "best" for the target visual
					 */
	XColor color;
	int found;

	p = Gets(handle, buffer,MAX_BUFFER);
	while (((p = strchr(p,'\"')) == NULL)) {
	    p = Gets(handle, buffer,MAX_BUFFER);
	    if (p == NULL) {
		return TCL_ERROR;
	    }
	    p = buffer;
	}
	colorDefn = p + byteSize + 1;
	colorName = (char*)ckalloc(strlen(colorDefn)+1);
	useName   = (char*)ckalloc(strlen(colorDefn)+1);
	found     = 0;
	color1 = 0;
	data = 0;

	while (colorDefn && *colorDefn) {
	    int type;

	    if ((colorDefn=GetColor(colorDefn, colorName, &type)) == NULL) {
		break;
	    }
	    if (colorName[0] == '\0') {
		continue;
	    }

	    switch (type) {
	      case XPM_MONO:
		if (isMono && depth == 1) {
		    strcpy(useName, colorName);
		    found = 1; goto gotcolor;
		}
		break;
	      case XPM_GRAY_4:
		if (isMono && depth == 4) {
		    strcpy(useName, colorName);
		    found = 1; goto gotcolor;
		}
		break;
	      case XPM_GRAY:
		if (isMono && depth > 4) {
		    strcpy(useName, colorName);
		    found = 1; goto gotcolor;
		}
		break;
	      case XPM_COLOR:
		if (!isMono) {
		    strcpy(useName, colorName);
		    found = 1; goto gotcolor;
		}
		break;
	    }
	    if (type != XPM_SYMBOLIC && type != XPM_UNKNOWN) {
		if (!found) {			/* use this color as default */
		    strcpy(useName, colorName);
		    found = 1;
		}
	    }
	}

      gotcolor:

	memcpy(&color1, p+1, byteSize);
	p = useName;
	while ((*p != 0) && (*p != '"') && (*p != '\t')) {
	    p++;
	}
	*p = 0;

	data = 0;
	if (strncasecmp(useName, "none",4)) {
	  if (XParseColor(display, colormap, useName, &color) == 0) {
	    color.red = color.green = color.blue = 0;
	  }
	  ((unsigned char *) &data)[0] = color.red>>8;
	  ((unsigned char *) &data)[1] = color.green>>8;
	  ((unsigned char *) &data)[2] = color.blue>>8;
	  ((unsigned char *) &data)[3] = 255;
	}

	hPtr = Tcl_CreateHashEntry(&colorTable, (char *) (size_t) color1, &found);
	Tcl_SetHashValue(hPtr, (char *) (size_t) data);

	ckfree(colorName);
	ckfree(useName);
    }

    Tk_PhotoGetImage(imageHandle, &block);

    if (tkimg_PhotoExpand(interp, imageHandle, destX + width, destY + height) == TCL_ERROR) {
	return TCL_ERROR;
    }

    nchan = block.pixelSize;
    block.pitch = nchan * fileWidth;
    block.width = width;
    block.height = 1;
    block.offset[0] = 0;
    block.offset[1] = 1;
    block.offset[2] = 2;
    block.offset[3] = (nchan == 4 && matte? 3: 0);
    block.pixelPtr = (unsigned char *) ckalloc((unsigned) nchan * width);

    i = srcY;
    while (i-- > 0) {
	p = Gets(handle, buffer,MAX_BUFFER);
	while (((p = strchr(p,'\"')) == NULL)) {
	    p = Gets(handle, buffer,MAX_BUFFER);
	    if (p == NULL) {
		return TCL_ERROR;
	    }
	    p = buffer;
	}
    }


    for (h = height; h > 0; h--) {
	p = Gets(handle, buffer,MAX_BUFFER);
	while (((p = strchr(p,'\"')) == NULL)) {
	    p = Gets(handle, buffer,MAX_BUFFER);
	    if (p == NULL) {
		return TCL_ERROR;
	    }
	    p = buffer;
	}
	p += byteSize * srcX + 1;
	pixelPtr = block.pixelPtr;

	for (i = 0; i < width; ) {
	    unsigned int col;

	    memcpy((char *) &color1, p, byteSize);
	    hPtr = Tcl_FindHashEntry(&colorTable, (char *) (size_t) color1);

	    /*
	     * if hPtr == NULL, we have an invalid color entry in the XPM
	     * file. We use transparant as default color
	     */
	    if (hPtr != (Tcl_HashEntry *)NULL)
	        col = (unsigned int) (size_t) Tcl_GetHashValue(hPtr);
	    else
	        col = (unsigned int) 0;

	    /*
	     * we've found a non-transparent pixel, let's search the next
	     * transparent pixel and copy this block to the image
	     */
	    if (col) {
	        int len = 0, j;

		j = i;
		pixelPtr = block.pixelPtr;
		do {
		    memcpy(pixelPtr, &col, block.pixelSize);
		    pixelPtr += block.pixelSize;
		    i++;
		    len++;
		    p += byteSize;

		    if (i < width) {
		        memcpy((char *) &color1, p, byteSize);
			hPtr = Tcl_FindHashEntry(&colorTable, (char *) (size_t) color1);
			if (hPtr != (Tcl_HashEntry *)NULL)
			    col = (unsigned int) (size_t) Tcl_GetHashValue(hPtr);
			else
			    col = (unsigned int) 0;
		    }
		} while ((i < width) && col);
		if (tkimg_PhotoPutBlock(interp, imageHandle, &block, destX+j, destY, len, 1, TK_PHOTO_COMPOSITE_SET) == TCL_ERROR) {
		    result = TCL_ERROR;
		    break;
		}
	    } else {
	        p += byteSize;
	        i++;
	    }
	}
	destY++;
    }

    Tcl_DeleteHashTable(&colorTable);

    ckfree((char *) block.pixelPtr);
    return result;
}

/*
 *----------------------------------------------------------------------
 *
 * ChnRead --
 *
 *	This procedure is called by the photo image type to read
 *	XPM format data from a channel and write it into a given
 *	photo image.
 *
 * Results:
 *	A standard TCL completion code.  If TCL_ERROR is returned
 *	then an error message is left in interp->result.
 *
 * Side effects:
 *	The access position in channel chan is changed, and new
 *	data is added to the image given by imageHandle.
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
    tkimg_MFile handle;
    int retVal;

    handle.data = (char *) chan;
    handle.state = IMG_CHAN;

    tkimg_ReadBuffer (1);
    retVal = CommonRead (interp, &handle, format, imageHandle,
		         destX, destY, width, height, srcX, srcY);
    tkimg_ReadBuffer (0);
    return retVal;
}


/*
 *----------------------------------------------------------------------
 *
 * ObjRead --
 *
 *	This procedure is called by the photo image type to read
 *	XPM format data from a string and write it into a given
 *	photo image.
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
ObjRead(interp, data, format, imageHandle, destX, destY,
	width, height, srcX, srcY)
    Tcl_Interp *interp;		/* Interpreter to use for reporting errors. */
    Tcl_Obj *data;
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

    handle.data = (char *)tkimg_GetStringFromObj(data, &handle.length);
    handle.state = IMG_STRING;

    return CommonRead(interp, &handle, format, imageHandle,
	    destX, destY, width, height, srcX, srcY);
}


/*
 *----------------------------------------------------------------------
 *
 * ReadXPMFileHeader --
 *
 *	This procedure reads the XPM header from the beginning of a
 *	XPM file and returns information from the header.
 *
 * Results:
 *	The return value is 1 if file "f" appears to start with a valid
 *      XPM header, and 0 otherwise.  If the header is valid,
 *	then *widthPtr and *heightPtr are modified to hold the
 *	dimensions of the image and *numColors holds the number of
 *	colors and byteSize the number of bytes used for 1 pixel.
 *
 * Side effects:
 *	The access position in f advances.
 *
 *----------------------------------------------------------------------
 */

#define UCHAR(c) ((unsigned char) (c))

static int
ReadXPMFileHeader(handle, widthPtr, heightPtr, numColors, byteSize)
    tkimg_MFile *handle;		/* handle to read the header from */
    int *widthPtr, *heightPtr;	/* The dimensions of the image are
				 * returned here. */
    int *numColors;		/* the number of colors is returned here */
    int *byteSize;		/* number of bytes per pixel */
{
    char buffer[MAX_BUFFER];
    char *p;

    p = Gets(handle, buffer,MAX_BUFFER);
    if (p == NULL) {
	return 0;
    }
    p = buffer;
    while (*p && isspace(UCHAR(*p))) {
	p++;
    }
    if (strncmp("/* XPM", p, 6) != 0) {
	return 0;
    }
    while ((p = strchr(p,'{')) == NULL) {
	p = Gets(handle, buffer,MAX_BUFFER);
	if (p == NULL) {
	    return 0;
	}
	p = buffer;
    }
    while ((p = strchr(p,'"')) == NULL) {
	p = Gets(handle, buffer,MAX_BUFFER);
	if (p == NULL) {
	    return 0;
	}
	p = buffer;
    }
    p++;
    while (p && *p && isspace(UCHAR(*p))) {
	p++;
    }
    *widthPtr = strtoul(p, &p, 0);
    if (p == NULL || *widthPtr <= 0) {
	return 0;
    }
    while (p && *p && isspace(UCHAR(*p))) {
	p++;
    }
    *heightPtr = strtoul(p, &p, 0);
    if (p == NULL || *heightPtr <= 0) {
	return 0;
    }
    while (p && *p && isspace(UCHAR(*p))) {
	p++;
    }
    *numColors = strtoul(p, &p, 0);
    if (p == NULL) {
	return 0;
    }
    while (p && *p && isspace(UCHAR(*p))) {
	p++;
    }
    *byteSize = strtoul(p, &p, 0);
    if (p == NULL) {
	return 0;
    }
    return 1;
}

static char * GetType(colorDefn, type_ret)
    char * colorDefn;
    int  * type_ret;
{
    char * p = colorDefn;

    /* skip white spaces */
    while (*p && isspace(UCHAR(*p))) {
	p ++;
    }

    /* parse the type */
    if (p[0] != '\0' && p[0] == 'm' &&
	p[1] != '\0' && isspace(UCHAR(p[1]))) {
	*type_ret = XPM_MONO;
	p += 2;
    }
    else if (p[0] != '\0' && p[0] == 'g' &&
	     p[1] != '\0' && p[1] == '4' &&
	     p[2] != '\0' && isspace(UCHAR(p[2]))) {
	*type_ret = XPM_GRAY_4;
	p += 3;
    }
    else if (p[0] != '\0' && p[0] == 'g' &&
	     p[1] != '\0' && isspace(UCHAR(p[1]))) {
	*type_ret = XPM_GRAY;
	p += 2;
    }
    else if (p[0] != '\0' && p[0] == 'c' &&
	     p[1] != '\0' && isspace(UCHAR(p[1]))) {
	*type_ret = XPM_COLOR;
	p += 2;
    }
    else if (p[0] != '\0' && p[0] == 's' &&
	     p[1] != '\0' && isspace(UCHAR(p[1]))) {
	*type_ret = XPM_SYMBOLIC;
	p += 2;
    }
    else {
	*type_ret = XPM_UNKNOWN;
	return NULL;
    }

    return p;
}

/* colorName is guaranteed to be big enough */
static char * GetColor(colorDefn, colorName, type_ret)
    char * colorDefn;
    char * colorName;		/* if found, name is copied to this array */
    int  * type_ret;
{
    int type;
    char * p;

    if (!colorDefn) {
	return NULL;
    }

    if ((colorDefn = GetType(colorDefn, &type)) == NULL) {
	/* unknown type */
	return NULL;
    }
    else {
	*type_ret = type;
    }

    /* skip white spaces */
    while (*colorDefn && isspace(UCHAR(*colorDefn))) {
	colorDefn ++;
    }

    p = colorName;

    while (1) {
	int dummy;

	while (*colorDefn && !isspace(UCHAR(*colorDefn))) {
	    *p++ = *colorDefn++;
	}

	if (!*colorDefn) {
	    break;
	}

	if (GetType(colorDefn, &dummy) == NULL) {
	    /* the next string should also be considered as a part of a color
	     * name */

	    while (*colorDefn && isspace(UCHAR(*colorDefn))) {
		*p++ = *colorDefn++;
	    }
	} else {
	    break;
	}
	if (!*colorDefn) {
	    break;
	}
    }

    /* Mark the end of the colorName */
    *p = '\0';

    return colorDefn;
}


/*
 *----------------------------------------------------------------------
 *
 * ChnWrite
 *
 *	Writes a XPM image to a file. Just calls CommonWrite
 *      with appropriate arguments.
 *
 * Results:
 *	Returns the return value of CommonWrite
 *
 * Side effects:
 *	A file is (hopefully) created on success.
 *
 *----------------------------------------------------------------------
 */
static int
ChnWrite(interp, fileName, format, blockPtr)
    Tcl_Interp *interp;
    const char *fileName;
    Tcl_Obj *format;
    Tk_PhotoImageBlock *blockPtr;
{
    return CommonWrite(interp, fileName, (Tcl_DString *)NULL, format, blockPtr);
}


/*
 *----------------------------------------------------------------------
 *
 * StringWrite
 *
 *	Writes a XPM image to a string. Just calls CommonWrite
 *      with appropriate arguments.
 *
 * Results:
 *	Returns the return value of CommonWrite
 *
 * Side effects:
 *	The Tcl_DString dataPtr is modified on success.
 *
 *----------------------------------------------------------------------
 */
static int StringWrite(
    Tcl_Interp *interp,
    Tcl_Obj *format,
    Tk_PhotoImageBlock *blockPtr
) {
    int result;
    Tcl_DString data;

    Tcl_DStringInit(&data);
    result = CommonWrite(interp, "InlineData", &data, format, blockPtr);
    if (result == TCL_OK) {
	Tcl_DStringResult(interp, &data);
    } else {
	Tcl_DStringFree(&data);
    }
    return result;
}


/*
 * Yes, I know these macros are dangerous. But it should work fine
 */
#define WRITE(buf) { if (!dataPtr) Tcl_Write(chan, buf, -1); else Tcl_DStringAppend(dataPtr, buf, -1);}

/*
 *----------------------------------------------------------------------
 *
 * CommonWrite
 *
 *	This procedure writes a XPM image to the file filename
 *      (if filename != NULL) or to dataPtr.
 *
 * Results:
 *	Returns TCL_OK on success, or TCL_ERROR on error.
 *      Possible failures are:
 *      1. cannot access file (permissions or path not found)
 *      2. TkPhotoGetMask fails to retrieve the region mask
 *         for the image (should not happen)
 *
 * Side effects:
 *	varies (see StringWrite and ChnWrite)
 *
 *----------------------------------------------------------------------
 */
static int
CommonWrite(interp, fileName, dataPtr, format, blockPtr)
    Tcl_Interp *interp;
    const char *fileName;
    Tcl_DString *dataPtr;
    Tcl_Obj *format;
    Tk_PhotoImageBlock *blockPtr;
{
    int x, y, i;
    int found;
    Tcl_Channel chan = (Tcl_Channel) NULL;
    Tcl_HashTable colors;
    Tcl_HashEntry *entry;
    Tcl_HashSearch search;
    unsigned char *pp;
    unsigned char *pixelPtr, *rowPixPtr;
    int ncolors, chars_per_pixel;
    int greenOffset, blueOffset, alphaOffset;
    union {
	ClientData value;
	char component[3];
    } col;
    union {
	ClientData value;
	char component[5];
    } temp;
    char buffer[256], *p, *imgName;

    /*
     * xpm_chars[] must be 64 chars long
     */
    static char xpm_chars[] =
	    ".#abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789";

    greenOffset = blockPtr->offset[1] - blockPtr->offset[0];
    blueOffset = blockPtr->offset[2] - blockPtr->offset[0];
    alphaOffset = blockPtr->offset[0];
    if (alphaOffset < blockPtr->offset[1]) alphaOffset = blockPtr->offset[1];
    if (alphaOffset < blockPtr->offset[2]) alphaOffset = blockPtr->offset[2];
    if (++alphaOffset < blockPtr->pixelSize) {
	alphaOffset -= blockPtr->offset[0];
    } else {
	alphaOffset = 0;
    }

    /* open the output file (if needed) */
    if (!dataPtr) {
      chan = Tcl_OpenFileChannel(interp, (CONST84 char *) fileName, "w", 0644);
      if (!chan) {
	return TCL_ERROR;
      }
      if (Tcl_SetChannelOption (interp, chan, "-buffersize", "131072") != TCL_OK) {
        Tcl_Close(interp, chan);
        return TCL_ERROR;
      }
    }

    /* compute image name */

    imgName = (char*)ckalloc(strlen(fileName)+1);
    memcpy (imgName, fileName, strlen(fileName)+1);
    p = strrchr(imgName, '/');
    if (p) {
	imgName = p+1;
    }
    p = strrchr(imgName, '\\');
    if (p) {
	imgName = p+1;
    }
    p = strrchr(imgName, ':');
    if (p) {
	imgName = p+1;
    }
    p = strchr(imgName, '.');
    if (p) {
	*p = 0;
    }
    sprintf(buffer, "/* XPM */\nstatic char * %s[] = {\n", imgName);
    WRITE(buffer);

    /*
     * Compute size of colortable
     */
    Tcl_InitHashTable(&colors, TCL_ONE_WORD_KEYS);
    ncolors = 0;
    col.value = 0;
    for (y = 0; y < blockPtr->height; y++) {
	pp = blockPtr->pixelPtr + y * blockPtr->pitch + blockPtr->offset[0];
	for (x = blockPtr->width; x >0; x--) {
	    if (!alphaOffset || pp[alphaOffset]) {
		col.component[0] = pp[0];
		col.component[1] = pp[greenOffset];
		col.component[2] = pp[blueOffset];
		if (Tcl_FindHashEntry(&colors, (char *) col.value) == NULL) {
		    ncolors++;
		    entry = Tcl_CreateHashEntry(&colors, (char *) col.value,
		    	    &found);
		}
	    }
	    pp += blockPtr->pixelSize;
	}
    }

    /* compute number of characters per pixel */
    chars_per_pixel = 1;
    i = ncolors;
    while (i > 64) {
	chars_per_pixel++;
	i /= 64;
    }

    /* write image info into XPM */
    sprintf(buffer, "\"%d %d %d %d\",\n", blockPtr->width, blockPtr->height,
	    ncolors+(alphaOffset != 0), chars_per_pixel);
    WRITE(buffer);

    /* write transparent color id if transparency is available*/
    if (alphaOffset) {
	strcpy(temp.component, "    ");
	temp.component[chars_per_pixel] = 0;
 	sprintf(buffer, "\"%s s None c None\",\n", temp.component);
	WRITE(buffer);
    }

    /* write colormap strings */
    entry = Tcl_FirstHashEntry(&colors, &search);
    y = 0;
    temp.component[chars_per_pixel] = 0;
    while (entry) {
	/* compute a color identifier for color #y */
	for (i = 0, x = y++; i < chars_per_pixel; i++, x /= 64)
	    temp.component[i] = xpm_chars[x & 63];

	/*
	 * and put it in the hashtable
	 * this is a little bit tricky
	 */
	Tcl_SetHashValue(entry, (char *) temp.value);
	pp = (unsigned char *)&entry->key.oneWordValue;
	sprintf(buffer, "\"%s c #%02x%02x%02x\",\n",
		temp.component, pp[0], pp[1], pp[2]);
	WRITE(buffer);
	entry = Tcl_NextHashEntry(&search);
    }

    /* write image itself */
    rowPixPtr = blockPtr->pixelPtr + blockPtr->offset[0];
    buffer[chars_per_pixel] = 0;
    for (y = 0; y < blockPtr->height; y++) {
	WRITE("\"");
	pixelPtr = rowPixPtr;
	for (x = 0; x < blockPtr->width; x++) {
	    if (!alphaOffset || pixelPtr[alphaOffset]) {
		col.component[0] = pixelPtr[0];
		col.component[1] = pixelPtr[greenOffset];
		col.component[2] = pixelPtr[blueOffset];
		entry = Tcl_FindHashEntry(&colors, (char *) col.value);
		temp.value = Tcl_GetHashValue(entry);
		memcpy(buffer, temp.component, chars_per_pixel);
	    } else {
		/* make transparent pixel */
		memcpy(buffer, "    ", chars_per_pixel);
	    }
	    pixelPtr += blockPtr->pixelSize;
	    WRITE(buffer);
	}
	if (y == blockPtr->height - 1) {
	    WRITE("\"};");
	} else {
	    WRITE("\",\n");
	}
	rowPixPtr += blockPtr->pitch;
    }

    /* Delete the hash table */
    Tcl_DeleteHashTable(&colors);

    /* close the file */
    if (chan) {
	Tcl_Close(interp, chan);
    }
    return TCL_OK;
}
