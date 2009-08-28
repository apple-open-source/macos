/*
 * ps.c --
 *
 *  PS + PDF photo image type, Tcl/Tk package
 *
 * Author : Jan Nijtmans *
 * Date   : 7/24/97        *
 *
 * Copyright (c) 2002 Andreas Kupries <andreas_kupries@users.sourceforge.net>
 *
 * $Id: ps.c 174 2008-12-01 15:39:10Z nijtmans $
 *
 */

/*
 * Generic initialization code, parameterized via CPACKAGE and PACKAGE.
 */

/*
 * Activate second 'format' definition in init.c
 * share all functions except for the matching
 */
#define SECOND_FORMAT
#define SECOND_CHNREAD	ChnRead
#define SECOND_OBJREAD	ObjRead
#define SECOND_CHNWRITE	ChnWrite
#define SECOND_STRWRITE	StringWrite

#include "init.c"


#include <string.h>
#include <stdlib.h>

/*
 * Prototypes for local procedures defined in this file:
 */

static int CommonMatchPS _ANSI_ARGS_((tkimg_MFile *handle, Tcl_Obj *format,
	int *widthPtr, int *heightPtr));

static int CommonMatchPDF _ANSI_ARGS_((tkimg_MFile *handle, Tcl_Obj *format,
	int *widthPtr, int *heightPtr));

static int parseFormat _ANSI_ARGS_((Tcl_Obj *format, int *zoomx,
	int *zoomy));

static int CommonRead _ANSI_ARGS_((Tcl_Interp *interp, tkimg_MFile *handle,
	Tcl_Obj *format, Tk_PhotoHandle imageHandle, int destX, int destY,
	int width, int height, int srcX, int srcY));

static int CommonWrite _ANSI_ARGS_((Tcl_Interp *interp, tkimg_MFile *handle,
	Tcl_Obj *format, Tk_PhotoImageBlock *blockPtr));

static int
parseFormat(format, zoomx, zoomy)
     Tcl_Obj *format;
     int *zoomx;
     int *zoomy;
{
    int objc, i, length, index = 0;
    Tcl_Obj **objv = NULL;
    char *p;
    double zx = 1.0, zy = 1.0;

    if (!format) {
	*zoomx = (int) (72 * zx + 0.5);
	*zoomy = (int) (72 * zy + 0.5);
    }

    if (tkimg_ListObjGetElements((Tcl_Interp*) NULL, format, &objc, &objv) != TCL_OK) {
	return -1;
    }
    for (i=1; i<objc; i++) {
	p = Tcl_GetStringFromObj(objv[i], &length);
	if ((p[0] == '-') && ((i+1)<objc)) {
	    if (length < 2) {
		index = -1; break;
	    }
	    if (!strncmp(p,"-index", length)) {
		if (Tcl_GetIntFromObj((Tcl_Interp *) NULL, objv[++i], &index) != TCL_OK) {
		    index = -1; break;
		}
	    } else if (!strncmp(p, "-zoom", length)) {
		if (Tcl_GetDoubleFromObj((Tcl_Interp *) NULL, objv[++i], &zx) != TCL_OK) {
		    index = -1; break;
		}
		if (i > objc) {
		    zy = zx;
		} else {
		    p = Tcl_GetStringFromObj(objv[i+1], &length);
		    if (p[0] != '-') {
			if (Tcl_GetDoubleFromObj((Tcl_Interp *) NULL, objv[++i], &zy) != TCL_OK) {
			    index = -1; break;
			}
		    } else {
			zy = zx;
		    }
		}
	    } else {
		index = -1; break;
	    }
	} else {
	    if (Tcl_GetIntFromObj((Tcl_Interp *) NULL, objv[++i], &index) != TCL_OK) {
		index = -1; break;
	    }
	}
    }
    if (!index) {
	*zoomx = (int) (72 * zx + 0.5);
	*zoomy = (int) (72 * zy + 0.5);
    }
    return index;
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

    return CommonMatchPS(&handle, format, widthPtr, heightPtr);
}

static int
ObjMatch(interp, data, format, widthPtr, heightPtr)
    Tcl_Interp *interp;
    Tcl_Obj *data;
    Tcl_Obj *format;
    int *widthPtr, *heightPtr;
{
    tkimg_MFile handle;

    tkimg_FixObjMatchProc(&interp, &data, &format, &widthPtr, &heightPtr);

    handle.data = (char *)tkimg_GetStringFromObj(data, &handle.length);
    handle.state = IMG_STRING;

    return CommonMatchPS(&handle, format, widthPtr, heightPtr);
}

static int
CommonMatchPS(handle, format, widthPtr, heightPtr)
    tkimg_MFile *handle;
    Tcl_Obj *format;
    int *widthPtr, *heightPtr;
{
    char buf[41];

    if ((tkimg_Read(handle, buf, 11) != 11)
	    || (strncmp("%!PS-Adobe-", buf, 11) != 0)) {
	return 0;
    }
    while (tkimg_Read(handle,buf, 1) == 1) {
	if (buf[0] == '%' &&
		(tkimg_Read(handle, buf, 2) == 2) &&
		(!memcmp(buf, "%B", 2) &&
		(tkimg_Read(handle, buf, 11) == 11) &&
		(!memcmp(buf, "oundingBox:", 11)) &&
		(tkimg_Read(handle, buf, 40) == 40))) {
	    int w, h, zoomx, zoomy;
	    char *p = buf;
	    buf[40] = 0;
	    w = - (int) strtoul(p, &p, 0);
	    h = - (int) strtoul(p, &p, 0);
	    w += strtoul(p, &p, 0);
	    h += strtoul(p, &p, 0);
	    if (parseFormat(format, &zoomx, &zoomy) >= 0) {
		w = (w * zoomx + 36) / 72;
		h = (h * zoomy + 36) / 72;
	    }
	    if ((w <= 0) || (h <= 0)) return 0;
	    *widthPtr = w;
	    *heightPtr = h;
	    return 1;
	}
    }
    return 0;
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
    tkimg_MFile handle;

    handle.data = (char *) chan;
    handle.state = IMG_CHAN;

    return CommonRead(interp, &handle, format, imageHandle, destX, destY,
	    width, height, srcX, srcY);
}

static int
ObjRead(interp, data, format, imageHandle,
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

    tkimg_ReadInit(data,'%',&handle);

    return CommonRead(interp, &handle, format, imageHandle,
	    destX, destY, width, height, srcX, srcY);
}

typedef struct myblock {
    Tk_PhotoImageBlock ck;
    int dummy; /* extra space for offset[3], in case it is not
		  included already in Tk_PhotoImageBlock */
} myblock;

#define block bl.ck

static int
CommonRead(interp, handle, format, imageHandle,
	destX, destY, width, height, srcX, srcY)
    Tcl_Interp *interp;
    tkimg_MFile *handle;
    Tcl_Obj *format;
    Tk_PhotoHandle imageHandle;
    int destX, destY;
    int width, height;
    int srcX, srcY;
{
#ifndef MAC_TCL
    CONST84 char *argv[10];
    int len, i, j, fileWidth, fileHeight, maxintensity, index;
    char *p, type;
    char buffer[1025];
    unsigned char *line = NULL, *line3 = NULL;
	char zoom[64], papersize[64];
    Tcl_Channel chan;
    Tcl_DString dstring;
    myblock bl;
    int zoomx, zoomy;

    index = parseFormat(format, &zoomx, &zoomy);
    if (index < 0) {
	Tcl_AppendResult(interp, "invalid format: \"",
		tkimg_GetStringFromObj(format, NULL), "\"", (char *) NULL);
	return TCL_ERROR;
    }
    sprintf(zoom, "-r%dx%d", zoomx, zoomy);

    len = tkimg_Read(handle, buffer, 1024);
    buffer[1024] = 0;
    p = strstr(buffer,"%%BoundingBox:");
    fileHeight = height + srcY;
    if (p) {
	/* postscript */
	p += 14;
	srcX += (strtoul(p, &p, 0) * zoomx + 36) / 72;
	fileHeight += (strtoul(p, &p, 0) * zoomy + 36) / 72;
	strtoul(p, &p, 0);
	srcY -= (strtoul(p, &p, 0) * zoomy + 36) / 72;
    } else {
	/* pdf */

	/*
	 * Extract the pixel position of the upper left corner
	 * of the image from the file. How to do that????
	 * For now I just assume A4-size with 72 pixels/inch.
	 */
	srcX += (0 * zoomx + 36) / 72;
	srcY -= (792 * zoomy + 36) /72;
    }

    sprintf(papersize, "-g%dx%d", srcX+width, fileHeight);

    argv[0] = "gs";
    argv[1] = "-sDEVICE=ppmraw";
    argv[2] = zoom;
    argv[3] = papersize;
    argv[4] = "-q";
    argv[5] = "-dNOPAUSE";
    argv[6] = "-sOutputFile=-";
    argv[7] = "-";

    chan = Tcl_OpenCommandChannel(interp, 8, argv,
	    TCL_STDIN|TCL_STDOUT|TCL_STDERR|TCL_ENFORCE_MODE);
    if (!chan) {
	return TCL_ERROR;
    }
    if (Tcl_SetChannelOption(interp, chan, "-translation", "binary") != TCL_OK) {
	return TCL_ERROR;
    }

    while (len > 0) {
	Tcl_Write(chan, (char *) buffer, 1024);
	len = tkimg_Read(handle, buffer, 1024);
    }
    Tcl_Write(chan,"\nquit\n", 6);
    Tcl_Flush(chan);

    Tcl_DStringInit(&dstring);
    len = Tcl_Gets(chan, &dstring);
    p = Tcl_DStringValue(&dstring);
    type = p[1];
    if ((p[0] != 'P') || (type < '4') || (type > '6')) {
	Tcl_AppendResult(interp, "gs error: \"",
		p, "\"",(char *) NULL);
	return TCL_ERROR;
    }
    do {
	Tcl_DStringSetLength(&dstring, 0);
	Tcl_Gets(chan, &dstring);
	p = Tcl_DStringValue(&dstring);
    } while (p[0] == '#');
    fileWidth = strtoul(p, &p, 0);
    srcY += (fileHeight = strtoul(p, &p, 0));

    if ((srcX + width) > fileWidth) {
	width = fileWidth - srcX;
    }
    if ((srcY + height) > fileHeight) {
	height = fileHeight - srcY;
    }
    if ((width <= 0) || (height <= 0)) {
	Tcl_Close(interp, chan);
	Tcl_DStringFree(&dstring);
	return TCL_OK;
    }
    Tk_PhotoExpand(imageHandle, destX + width, destY + height);

    maxintensity = strtoul(p, &p, 0);
    if ((type != '4') && !maxintensity) {
	Tcl_DStringSetLength(&dstring, 0);
	Tcl_Gets(chan, &dstring);
	p = Tcl_DStringValue(&dstring);
	maxintensity = strtoul(p, &p, 0);
    }
    Tcl_DStringFree(&dstring);
    line3 = (unsigned char *) ckalloc(3 * fileWidth);
    block.pixelSize = 1;
    block.pitch = block.width = width;
    block.height = 1;
    block.offset[0] = 0;
    block.offset[1] = 0;
    block.offset[2] = 0;
    block.offset[3] = 0;
    switch(type) {
	case '4':
	    i = (fileWidth+7)/8;
	    line = (unsigned char *) ckalloc(i);
	    while (srcY-- > 0) {
		Tcl_Read(chan,(char *) line, i);
	    }
	    block.pixelPtr = line3;
	    while (height--) {
	        Tcl_Read(chan, (char *) line, i);
	        for (j = 0; j < width; j++) {
		    line3[j] = ((line[(j+srcX)/8]>>(7-(j+srcX)%8) & 1)) ? 0 : 255;
	        }
		tkimg_PhotoPutBlock(interp, imageHandle, &block, destX, destY++, width, 1, TK_PHOTO_COMPOSITE_SET);
	    }
	    break;
	case '5':
	    line = (unsigned char *) ckalloc(fileWidth);
	    while (srcY-- > 0) {
		Tcl_Read(chan, (char *) line, fileWidth);
	    }
	    block.pixelPtr = line + srcX;
	    while (height--) {
		unsigned char *c = block.pixelPtr;
		Tcl_Read(chan, (char *) line, fileWidth);
		if (maxintensity != 255) {
		    for (j = width; j > 0; j--) {
			*c = (((int)*c) * maxintensity) / 255;
			c++;
		    }
		}
		tkimg_PhotoPutBlock(interp, imageHandle, &block, destX, destY++, width, 1, TK_PHOTO_COMPOSITE_SET);
	    }
	    break;
	case '6':
	    i = 3 * fileWidth;
	    line = NULL;
	    while (srcY-- > 0) {
		Tcl_Read(chan, (char *) line3, i);
	    }
	    block.pixelPtr = line3 + (3 * srcX);
	    block.pixelSize = 3;
	    block.offset[1] = 1;
	    block.offset[2] = 2;
	    while (height--) {
		unsigned char *c = block.pixelPtr;
		Tcl_Read(chan, (char *) line3, i);
		if (maxintensity != 255) {
		    for (j = (3 * width - 1); j >= 0; j--) {
			*c = (((int)*c) * maxintensity) / 255;
			c++;
		    }
		}
		tkimg_PhotoPutBlock(interp, imageHandle, &block, destX, destY++, width, 1, TK_PHOTO_COMPOSITE_SET);
	    }
	    break;
    }
    if (line) {
	ckfree((char *) line);
    }
    ckfree((char *) line3);
    Tcl_Close(interp, chan);
    Tcl_ResetResult(interp);
    return TCL_OK;
#else
    Tcl_AppendResult(interp, "Cannot read postscript file: not implemented",
	    (char *) NULL);
    return TCL_ERROR;
#endif
}

static int
ChnWrite(interp, filename, format, blockPtr)
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
    tkimg_MFile handle;
    int result;
    Tcl_DString data;
    tkimg_FixStringWriteProc(&data, &interp, &dataPtr, &format, &blockPtr);
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
    return TCL_OK;
}


static int
ChnMatchBeta (interp, chan, fileName, format, widthPtr, heightPtr) /* PDF */
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

    return CommonMatchPDF(&handle, format, widthPtr, heightPtr);
}

static int
ObjMatchBeta(interp, data, format, widthPtr, heightPtr) /* PDF */
    Tcl_Interp *interp;
    Tcl_Obj *data;
    Tcl_Obj *format;
    int *widthPtr, *heightPtr;
{
    tkimg_MFile handle;

    tkimg_FixObjMatchProc(&interp, &data, &format, &widthPtr, &heightPtr);

    if (!tkimg_ReadInit(data, '%', &handle)) {
	return 0;
    }

    return CommonMatchPDF(&handle, format, widthPtr, heightPtr);
}

static int
CommonMatchPDF(handle, format, widthPtr, heightPtr)
    tkimg_MFile *handle;
    Tcl_Obj *format;
    int *widthPtr, *heightPtr;
{
    unsigned char buf[41];
    int zoomx, zoomy, w, h;

    if ((tkimg_Read(handle, (char *) buf, 5) != 5)
	    || (strncmp("%PDF-", (char *) buf, 5) != 0)) {
	return 0;
    }

    /* Here w and h should be set to the bounding box of the pdf
     * data. But I don't know how to extract that from the file.
     * For now I just assume A4-size with 72 pixels/inch. If anyone
     * has a better idea, please mail to <nijtmans@users.sourceforge.net>.
     */

    w = 612/10;
    h = 792/10;

    if (parseFormat(format, &zoomx, &zoomy) >= 0) {
	w = (w * zoomx + 36) / 72;
	h = (h * zoomy + 36) / 72;
    }
    if ((w <= 0) || (h <= 0)) return 0;
    *widthPtr = w;
    *heightPtr = h;
    return 1;
}
