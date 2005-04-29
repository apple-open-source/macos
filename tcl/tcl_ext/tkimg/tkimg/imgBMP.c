/*
 * tkImgBMP.c --
 *
 * A photo image file handler for BMP files.
 *
 */

/* Author : Jan Nijtmans */
/* Date   : 7/22/97        */

#include <string.h>
#include <stdlib.h>

#include "imgInt.h"

/*
 * The format record for the BMP file format:
 */

static int ChnMatchBMP _ANSI_ARGS_((Tcl_Interp *interp, Tcl_Channel chan,
	CONST char *fileName, Tcl_Obj *format, int *widthPtr,
	int *heightPtr));
static int ObjMatchBMP _ANSI_ARGS_((Tcl_Interp *interp, Tcl_Obj *dataObj,
	Tcl_Obj *format, int *widthPtr, int *heightPtr));
static int ChnReadBMP _ANSI_ARGS_((Tcl_Interp *interp, Tcl_Channel chan,
	CONST char *fileName, Tcl_Obj *format, Tk_PhotoHandle imageHandle,
	int destX, int destY, int width, int height, int srcX, int srcY));
static int ObjReadBMP _ANSI_ARGS_((Tcl_Interp *interp, Tcl_Obj *dataObj,
	Tcl_Obj *format, Tk_PhotoHandle imageHandle,
	int destX, int destY, int width, int height, int srcX, int srcY));
static int ChnWriteBMP _ANSI_ARGS_((Tcl_Interp *interp, CONST char *filename,
	Tcl_Obj *format, Tk_PhotoImageBlock *blockPtr));
static int StringWriteBMP _ANSI_ARGS_((Tcl_Interp *interp,
	Tcl_DString *data, Tcl_Obj *format,
	Tk_PhotoImageBlock *blockPtr));

Tk_PhotoImageFormat imgFmtBMP = {
    "bmp",					/* name */
    (Tk_ImageFileMatchProc *) ChnMatchBMP,	/* fileMatchProc */
    (Tk_ImageStringMatchProc *) ObjMatchBMP,	/* stringMatchProc */
    (Tk_ImageFileReadProc *) ChnReadBMP,	/* fileReadProc */
    (Tk_ImageStringReadProc *) ObjReadBMP,	/* stringReadProc */
    (Tk_ImageFileWriteProc *) ChnWriteBMP,	/* fileWriteProc */
    (Tk_ImageStringWriteProc *) StringWriteBMP,	/* stringWriteProc */
};

/*
 * Prototypes for local procedures defined in this file:
 */

static int CommonMatchBMP _ANSI_ARGS_((MFile *handle, int *widthPtr,
	int *heightPtr, unsigned char **colorMap, int *numBits,
	int *numCols, int *comp));
static int CommonReadBMP _ANSI_ARGS_((Tcl_Interp *interp, MFile *handle,
	Tk_PhotoHandle imageHandle, int destX, int destY, int width,
	int height, int srcX, int srcY));
static int CommonWriteBMP _ANSI_ARGS_((Tcl_Interp *interp, MFile *handle,
	Tk_PhotoImageBlock *blockPtr));
static void putint _ANSI_ARGS_((MFile *handle, int i));

static int ChnMatchBMP(interp, chan, fileName, format, widthPtr, heightPtr)
    Tcl_Interp *interp;
    Tcl_Channel chan;
    CONST char *fileName;
    Tcl_Obj *format;
    int *widthPtr, *heightPtr;
{
    MFile handle;

    ImgFixChanMatchProc(&interp, &chan, &fileName, &format, &widthPtr, &heightPtr);

    handle.data = (char *) chan;
    handle.state = IMG_CHAN;

    return CommonMatchBMP(&handle, widthPtr, heightPtr, NULL, NULL, NULL, NULL);
}

static int ObjMatchBMP(interp, data, format, widthPtr, heightPtr)
    Tcl_Interp *interp;
    Tcl_Obj *data;
    Tcl_Obj *format;
    int *widthPtr, *heightPtr;
{
    MFile handle;

    ImgFixObjMatchProc(&interp, &data, &format, &widthPtr, &heightPtr);

    if (!ImgReadInit(data,'B',&handle)) {
	return 0;
    }
    return CommonMatchBMP(&handle, widthPtr, heightPtr, NULL, NULL, NULL, NULL);
}

static int CommonMatchBMP(handle, widthPtr, heightPtr, colorMap, numBits, numCols, comp)
    MFile *handle;
    int *widthPtr, *heightPtr;
    unsigned char **colorMap;
    int *numBits, *numCols, *comp;
{
    unsigned char buf[28];
    int c,i, compression, nBits, clrUsed, offBits;

    if ((ImgRead(handle, (char *) buf, 2) != 2)
	    || (strncmp("BM", (char *) buf, 2) != 0)
	    || (ImgRead(handle, (char *) buf, 24) != 24)
	    || buf[13] || buf[14] || buf[15]) {
	return 0;
    }

    offBits = (buf[11]<<24) + (buf[10]<<16) + (buf[9]<<8) + buf[8];
    c = buf[12];
    if ((c == 40) || (c == 64)) {
	*widthPtr = (buf[19]<<24) + (buf[18]<<16) + (buf[17]<<8) + buf[16];
	*heightPtr = (buf[23]<<24) + (buf[22]<<16) + (buf[21]<<8) + buf[20];
	if (ImgRead(handle, (char *) buf, 24) != 24) {
	    return 0;
	}
	nBits = buf[2];
	compression = buf[4];
	clrUsed = (buf[21]<<8) + buf[20];
	offBits -= c+14;
    } else if (c == 12) {
	*widthPtr = (buf[17]<<8) + buf[16];
	*heightPtr = (buf[19]<<8) + buf[18];
	nBits = buf[22];
	compression = 0;
	clrUsed = 0;
    } else {
	return 0;
    }
    if (colorMap) {
	if (c > 36) ImgRead(handle, (char *) buf, c - 36);
	if (!clrUsed && nBits != 24) {
	    clrUsed = 1 << nBits;
	}
	if (nBits<24) {
	    unsigned char colbuf[4], *ptr;
	    offBits -= (3+(c!=12)) * clrUsed;
	    *colorMap = ptr = (unsigned char *) ckalloc(3*clrUsed);
	    for (i = 0; i < clrUsed; i++) {
		ImgRead(handle, (char *) colbuf, 3+(c!=12));
		*ptr++ = colbuf[0]; *ptr++ = colbuf[1]; *ptr++ = colbuf[2];
		/*printf("color %d: %d %d %d\n", i, colbuf[2], colbuf[1], colbuf[0]);*/
	    }
	}
	while (offBits>28) {
	    offBits -= 28;
	    ImgRead(handle, (char *) buf, 28);
	}
	if (offBits) ImgRead(handle, (char *) buf, offBits);
	if (numCols) {
	    *numCols = clrUsed;
	}
    }
    if (numBits) {
	*numBits = nBits;
    }
    if (comp) {
	*comp = compression;
    }
    return 1;
}

static int ChnReadBMP(interp, chan, fileName, format, imageHandle,
	destX, destY, width, height, srcX, srcY)
    Tcl_Interp *interp;
    Tcl_Channel chan;
    CONST char *fileName;
    Tcl_Obj *format;
    Tk_PhotoHandle imageHandle;
    int destX, destY;
    int width, height;
    int srcX, srcY;
{
    MFile handle;

    handle.data = (char *) chan;
    handle.state = IMG_CHAN;

    return CommonReadBMP(interp, &handle, imageHandle, destX, destY,
	    width, height, srcX, srcY);
}

static int ObjReadBMP(interp, data, format, imageHandle,
	destX, destY, width, height, srcX, srcY)
    Tcl_Interp *interp;
    Tcl_Obj *data;
    Tcl_Obj *format;
    Tk_PhotoHandle imageHandle;
    int destX, destY;
    int width, height;
    int srcX, srcY;
{
    MFile handle;

    ImgReadInit(data,'B',&handle);

    return CommonReadBMP(interp, &handle, imageHandle, destX, destY,
	    width, height, srcX, srcY);
}

typedef struct myblock {
    Tk_PhotoImageBlock ck;
    int dummy; /* extra space for offset[3], in case it is not
		  included already in Tk_PhotoImageBlock */
} myblock;

#define block bl.ck

static int CommonReadBMP(interp, handle, imageHandle, destX, destY,
	width, height, srcX, srcY)
    Tcl_Interp *interp;
    MFile *handle;
    Tk_PhotoHandle imageHandle;
    int destX, destY;
    int width, height;
    int srcX, srcY;
{
    myblock bl;
    int numBits, bytesPerLine, numCols, comp, x, y;
    int fileWidth, fileHeight;
    unsigned char *colorMap = NULL;
    char buf[10];
    unsigned char *line = NULL, *expline = NULL;

    CommonMatchBMP(handle, &fileWidth, &fileHeight, &colorMap, &numBits,
	    &numCols, &comp);

    /* printf("reading %d-bit BMP %dx%d\n", numBits, width, height); */
    if (comp != 0) {
	Tcl_AppendResult(interp,
		"Compressed BMP files not (yet) supported", (char *) NULL);
	goto error;
	return TCL_ERROR;
    }

    Tk_PhotoExpand(imageHandle, destX + width, destY + height);

    bytesPerLine = ((numBits * fileWidth + 31)/32)*4;
    line = (unsigned char *) ckalloc(bytesPerLine);

    for(y=srcY+height; y<fileHeight; y++) {
	ImgRead(handle, line, bytesPerLine);
    }
    block.pixelSize = 3;
    block.pitch = bytesPerLine;
    block.width = width;
    block.height = 1;
    block.offset[0] = 2;
    block.offset[1] = 1;
    block.offset[2] = 0;
    block.offset[3] = block.offset[0];
    switch (numBits) {
	case 24:
	    block.pixelPtr = line + srcX*3;
	    for( y = height-1; y>=0; y--) {
		ImgRead(handle, line, bytesPerLine);
#if (TK_MAJOR_VERSION > 8) || ((TK_MAJOR_VERSION == 8) && (TK_MINOR_VERSION > 3))
		Tk_PhotoPutBlock(imageHandle, &block, destX, destY+y,
			width,1, TK_PHOTO_COMPOSITE_OVERLAY);
#else
		Tk_PhotoPutBlock(imageHandle, &block, destX, destY+y,
			width,1);
#endif
	    }
	    break;
	case 8:
	    block.pixelPtr = expline = (unsigned char *) ckalloc(3*width);
	    for( y = height-1; y>=0; y--) {
		ImgRead(handle, line, bytesPerLine);
		for (x = srcX; x < (srcX+width); x++) {
		    memcpy(expline, colorMap+(3*line[x]),3);
		    expline += 3;
		}
#if (TK_MAJOR_VERSION > 8) || ((TK_MAJOR_VERSION == 8) && (TK_MINOR_VERSION > 3))
		Tk_PhotoPutBlock(imageHandle, &block, destX, destY+y,
			width,1, TK_PHOTO_COMPOSITE_OVERLAY);
#else
		Tk_PhotoPutBlock(imageHandle, &block, destX, destY+y,
			width,1);
#endif
		expline = block.pixelPtr;
	    }
	    break;
	case 4:
	    block.pixelPtr = expline = (unsigned char *) ckalloc(3*width);
	    for( y = height-1; y>=0; y--) {
	    	int c;
		ImgRead(handle, line, bytesPerLine);
		for (x = srcX; x < (srcX+width); x++) {
		    if (x&1) {
		    	c = line[x/2] & 0x0f;
		    } else {
		    	c = line[x/2] >> 4;
		    }
		    memcpy(expline, colorMap+(3*c),3);
		    expline += 3;
		}
#if (TK_MAJOR_VERSION > 8) || ((TK_MAJOR_VERSION == 8) && (TK_MINOR_VERSION > 3))
		Tk_PhotoPutBlock(imageHandle, &block, destX, destY+y,
			width,1, TK_PHOTO_COMPOSITE_OVERLAY);
#else
		Tk_PhotoPutBlock(imageHandle, &block, destX, destY+y,
			width,1);
#endif
		expline = block.pixelPtr;
	    }
	    break;
	case 1:
	    block.pixelPtr = expline = (unsigned char *) ckalloc(3*width);
	    for( y = height-1; y>=0; y--) {
	    	int c;
		ImgRead(handle, line, bytesPerLine);
		for (x = srcX; x < (srcX+width); x++) {
		    c = (line[x/8] >> (7-(x%8))) & 1;
		    memcpy(expline, colorMap+(3*c),3);
		    expline += 3;
		}
#if (TK_MAJOR_VERSION > 8) || ((TK_MAJOR_VERSION == 8) && (TK_MINOR_VERSION > 3))
		Tk_PhotoPutBlock(imageHandle, &block, destX, destY+y,
			width,1, TK_PHOTO_COMPOSITE_OVERLAY);
#else
		Tk_PhotoPutBlock(imageHandle, &block, destX, destY+y,
			width,1);
#endif
		expline = block.pixelPtr;
	    }
	    break;
	default:
	    sprintf(buf,"%d", numBits);
	    Tcl_AppendResult(interp, buf,
		    "-bits BMP file not (yet) supported", (char *) NULL);
	    goto error;
    }

    if (colorMap) {
	ckfree((char *) colorMap);
    }
    if (line) {
	ckfree((char *) line);
    }
    if (expline) {
	ckfree((char *) expline);
    }
    return TCL_OK ;

error:
    if (colorMap) {
	ckfree((char *) colorMap);
    }
    if (line) {
	ckfree((char *) line);
    }
    if (expline) {
	ckfree((char *) expline);
    }
    return TCL_ERROR;
}

static int ChnWriteBMP(interp, filename, format, blockPtr)
    Tcl_Interp *interp;
    CONST char *filename;
    Tcl_Obj *format;
    Tk_PhotoImageBlock *blockPtr;
{
    Tcl_Channel chan;
    MFile handle;
    int result;

    chan = ImgOpenFileChannel(interp, filename, 0644);
    if (!chan) {
	return TCL_ERROR;
    }

    handle.data = (char *) chan;
    handle.state = IMG_CHAN;

    result = CommonWriteBMP(interp, &handle, blockPtr);
    if (Tcl_Close(interp, chan) == TCL_ERROR) {
	return TCL_ERROR;
    }
    return result;
}

static int StringWriteBMP(interp, dataPtr, format, blockPtr)
    Tcl_Interp *interp;
    Tcl_DString *dataPtr;
    Tcl_Obj *format;
    Tk_PhotoImageBlock *blockPtr;
{
    MFile handle;
    int result;
    Tcl_DString data;

    ImgFixStringWriteProc(&data, &interp, &dataPtr, &format, &blockPtr);

    ImgWriteInit(dataPtr, &handle);
    result = CommonWriteBMP(interp, &handle, blockPtr);
    ImgPutc(IMG_DONE, &handle);

    if ((result == TCL_OK) && (dataPtr == &data)) {
	Tcl_DStringResult(interp, dataPtr);
    }
    return result;
}

static void putint(handle, i)
    MFile *handle;
    int i;
{
    unsigned char buf[4];
    buf[0] = i;
    buf[1] = i>>8;
    buf[2] = i>>16;
    buf[3] = i>>24;
    ImgWrite(handle, (char *) buf, 4);
}

static int CommonWriteBMP(interp, handle, blockPtr)
    Tcl_Interp *interp;
    MFile *handle;
    Tk_PhotoImageBlock *blockPtr;
{
    int bperline, nbytes, ncolors, i, x, y, greenOffset, blueOffset, alphaOffset;
    unsigned char *imagePtr, *pixelPtr;
    unsigned char buf[4];
    int colors[256];

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
    ncolors = 0;
    if (greenOffset || blueOffset) {
	for (y = 0; ncolors <= 256 && y < blockPtr->height; y++) {
	    pixelPtr = blockPtr->pixelPtr + y*blockPtr->pitch + blockPtr->offset[0];
	    for (x=0; ncolors <= 256 && x<blockPtr->width; x++) {
		int pixel;
		if (alphaOffset && (pixelPtr[alphaOffset] == 0))
		    pixel = 0xd9d9d9;
		else
		    pixel = (pixelPtr[0]<<16) | (pixelPtr[greenOffset]<<8) | pixelPtr[blueOffset];
		for (i = 0; i < ncolors && pixel != colors[i]; i++);
		if (i == ncolors) {
		    if (ncolors < 256) {
			colors[ncolors] = pixel;
		    }
		    ncolors++;
		}
		pixelPtr += blockPtr->pixelSize;
	    }
	}
	if (ncolors <= 256 && (blockPtr->width * blockPtr->height >= 512)) {
            while (ncolors < 256) {
		colors[ncolors++] = 0;
	    }
	    nbytes = 1;
	} else {
	    nbytes = 3;
	    ncolors = 0;
	}
    } else {
        nbytes = 1;
    }

    bperline = ((blockPtr->width  * nbytes + 3) / 4) * 4;

    ImgWrite(handle,"BM", 2);
    putint(handle, 54 + (ncolors*4) + bperline * blockPtr->height);
    putint(handle, 0);
    putint(handle, 54 + (ncolors*4));
    putint(handle, 40);
    putint(handle, blockPtr->width);
    putint(handle, blockPtr->height);
    putint(handle, 1 + (nbytes<<19));
    putint(handle, 0);
    putint(handle, bperline * blockPtr->height);
    putint(handle, 75*39);
    putint(handle, 75*39);
    putint(handle, ncolors);
    putint(handle, ncolors);

    for (i = 0; i < ncolors ; i++) {
	putint(handle, colors[i]);
    }

    bperline -= blockPtr->width * nbytes;

    imagePtr =  blockPtr->pixelPtr + blockPtr->offset[0]
	    + blockPtr->height * blockPtr->pitch;
    for (y = 0; y < blockPtr->height; y++) {
	pixelPtr = imagePtr -= blockPtr->pitch;
	for (x=0; x<blockPtr->width; x++) {
	    if (ncolors) {
		int pixel;
		if (alphaOffset && (pixelPtr[alphaOffset] == 0))
		    pixel = 0xd9d9d9;
		else
		    pixel = (pixelPtr[0]<<16)|(pixelPtr[greenOffset]<<8)|pixelPtr[blueOffset];
		for (i = 0; i < ncolors && pixel != colors[i]; i += 1);
		buf[0] = i;
	    } else if (alphaOffset && (pixelPtr[alphaOffset] == 0)) {
		buf[0] = buf[1] = buf[2] = 0xd9;
	    } else {
		buf[0] = pixelPtr[blueOffset];
		buf[1] = pixelPtr[greenOffset];
		buf[2] = pixelPtr[0];
	    }
	    ImgWrite(handle, (char *) buf, nbytes);
	    pixelPtr += blockPtr->pixelSize;
	}
	if (bperline) {
	    ImgWrite(handle, "\0\0\0", bperline);
	}
    }
    return(TCL_OK);
}
