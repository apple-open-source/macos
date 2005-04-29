/*
 *	imgInit.tcl
 */

#include <string.h>
#include <stdlib.h>

#include "imgInt.h"

#ifndef USE_TCL_STUBS
#undef Tcl_InitStubs
#define Tcl_InitStubs(a,b,c) Tcl_PkgRequire(a,"Tcl","8.0",1)
#endif

#ifndef USE_TK_STUBS
#undef Tk_InitStubs
#define Tk_InitStubs(a,b,c) Tcl_PkgRequire(a,"Tk","8.0",1)
#endif

/*
 * Declarations for functions defined in this file.
 */

static int char64 _ANSI_ARGS_((int c));

#ifdef ALLOW_B64
static int tob64 _ANSI_ARGS_((ClientData clientData, Tcl_Interp *interp,
		int argc, Tcl_Obj *CONST objv[]));
static int fromb64 _ANSI_ARGS_((ClientData clientData, Tcl_Interp *interp,
		int argc, Tcl_Obj *CONST objv[]));
#endif

extern Tk_ImageType		imgPixmapImageType;

extern Tk_PhotoImageFormat	imgFmtBMP;
extern Tk_PhotoImageFormat	imgFmtGIF;
extern Tk_PhotoImageFormat	imgFmtJPEG;
extern Tk_PhotoImageFormat	imgFmtPNG;
extern Tk_PhotoImageFormat	imgFmtPS;
extern Tk_PhotoImageFormat	imgFmtPDF;
extern Tk_PhotoImageFormat	imgFmtRAS;
extern Tk_PhotoImageFormat	imgFmtRAW;
extern Tk_PhotoImageFormat	imgFmtRGB;
extern Tk_PhotoImageFormat	imgFmtTIFF;
extern Tk_PhotoImageFormat	imgFmtXBM;
extern Tk_PhotoImageFormat	imgFmtXPM;
extern Tk_PhotoImageFormat	imgFmtWin;

static Tk_PhotoImageFormat *Formats[] = {
	&imgFmtTIFF,
	&imgFmtPS,
	&imgFmtPDF,
	&imgFmtXBM,
	&imgFmtXPM,
	&imgFmtBMP,
	&imgFmtJPEG,
	&imgFmtPNG,
	&imgFmtGIF,
	&imgFmtWin,
	(Tk_PhotoImageFormat *) NULL};

/*
 * The variable "initialized" contains flags indicating which
 * version of Tcl or Perl we are running:
 *
 *	IMG_TCL		Tcl
 *	IMG_OBJS	using Tcl_Obj's in stead of char* (Tk 8.3 or higher)
 *      IMG_PERL	perl
 *
 * These flags will be determined at runtime (except the IMG_PERL
 * flag, for now), so we can use the same dynamic library for all
 * Tcl/Tk versions (and for Perl/Tk in the future).
 */

static int initialized = 0;

/*
 *--------------------------------------------------------------
 *
 * Img_Init , Img_SafeInit, Img_InitStandAlone --
 *	Create Img commands.
 *
 * Results:
 *	None
 *
 * Side effects:
 *	None
 *
 *--------------------------------------------------------------
 */

int Img_Init (interp)
    Tcl_Interp *interp;
{
    Tk_PhotoImageFormat **formatPtr = Formats;
    char *version;

    if ((version = Tcl_InitStubs(interp, "8.0", 0)) == NULL) {
	return TCL_ERROR;
    }
    if (Tk_InitStubs(interp, "8.0", 0) == NULL) {
	return TCL_ERROR;
    }

    if (!initialized) {
	if (!(initialized = ImgObjInit(interp))) {
	    return TCL_ERROR;
	}
	while(*formatPtr) {
	    Tk_CreatePhotoImageFormat(*formatPtr++);
	}
#ifdef __WIN32__
	CreateMutex(NULL, FALSE, "ImgDllMutex"); 
#endif

#ifndef TCL_MAC
	Tk_CreateImageType(&imgPixmapImageType);
#endif
    }
#ifdef ALLOW_B64 /* Undocumented feature */
    Tcl_CreateObjCommand(interp,"img_to_base64", tob64, (ClientData) NULL, NULL);
    Tcl_CreateObjCommand(interp,"img_from_base64", fromb64, (ClientData) NULL, NULL);
#endif
    return Tcl_PkgProvide(interp,"Img", IMG_PATCH_LEVEL);
}

int Img_SafeInit (interp)
    Tcl_Interp *interp;
{
    return Img_Init(interp);
}

/*
 *--------------------------------------------------------------------------
 * char64 --
 *
 *	This procedure converts a base64 ascii character into its binary
 *	equivalent. This code is a slightly modified version of the
 *	char64 proc in N. Borenstein's metamail decoder.
 *
 * Results:
 *	The binary value, or an error code.
 *
 * Side effects:
 *	None.
 *--------------------------------------------------------------------------
 */

static int
char64(c)
    int c;
{
    switch(c) {
	case 'A': return 0;	case 'B': return 1;	case 'C': return 2;
	case 'D': return 3;	case 'E': return 4;	case 'F': return 5;
	case 'G': return 6;	case 'H': return 7;	case 'I': return 8;
	case 'J': return 9;	case 'K': return 10;	case 'L': return 11;
	case 'M': return 12;	case 'N': return 13;	case 'O': return 14;
	case 'P': return 15;	case 'Q': return 16;	case 'R': return 17;
	case 'S': return 18;	case 'T': return 19;	case 'U': return 20;
	case 'V': return 21;	case 'W': return 22;	case 'X': return 23;
	case 'Y': return 24;	case 'Z': return 25;	case 'a': return 26;
	case 'b': return 27;	case 'c': return 28;	case 'd': return 29;
	case 'e': return 30;	case 'f': return 31;	case 'g': return 32;
	case 'h': return 33;	case 'i': return 34;	case 'j': return 35;
	case 'k': return 36;	case 'l': return 37;	case 'm': return 38;
	case 'n': return 39;	case 'o': return 40;	case 'p': return 41;
	case 'q': return 42;	case 'r': return 43;	case 's': return 44;
	case 't': return 45;	case 'u': return 46;	case 'v': return 47;
	case 'w': return 48;	case 'x': return 49;	case 'y': return 50;
	case 'z': return 51;	case '0': return 52;	case '1': return 53;
	case '2': return 54;	case '3': return 55;	case '4': return 56;
	case '5': return 57;	case '6': return 58;	case '7': return 59;
	case '8': return 60;	case '9': return 61;	case '+': return 62;
	case '/': return 63;

	case ' ': case '\t': case '\n': case '\r': case '\f': return IMG_SPACE;
	case '=': return IMG_PAD;
	case '\0': return IMG_DONE;
	default: return IMG_BAD;
    }
}

/*
 *--------------------------------------------------------------------------
 * ImgRead --
 *
 *  This procedure returns a buffer from the stream input. This stream
 *  could be anything from a base-64 encoded string to a Channel.
 *
 * Results:
 *  The number of characters successfully read from the input
 *
 * Side effects:
 *  The MFile state could change.
 *--------------------------------------------------------------------------
 */

int
ImgRead(handle, dst, count)
    MFile *handle;	/* mmdecode "file" handle */
    char *dst;		/* where to put the result */
    int count;		/* number of bytes */
{
    register int i, c;
    switch (handle->state) {
      case IMG_STRING:
	if (count > handle->length) {
	    count = handle->length;
	}
	if (count) {
	    memcpy(dst, handle->data, count);
	    handle->length -= count;
	    handle->data += count;
	}
	return count;
      case IMG_CHAN:
	return Tcl_Read((Tcl_Channel) handle->data, dst, count);
    }

    for(i=0; i<count && (c=ImgGetc(handle)) != IMG_DONE; i++) {
	*dst++ = c;
    }
    return i;
}
/*
 *--------------------------------------------------------------------------
 *
 * ImgGetc --
 *
 *  This procedure returns the next input byte from a stream. This stream
 *  could be anything from a base-64 encoded string to a Channel.
 *
 * Results:
 *  The next byte (or IMG_DONE) is returned.
 *
 * Side effects:
 *  The MFile state could change.
 *
 *--------------------------------------------------------------------------
 */

int
ImgGetc(handle)
   MFile *handle;			/* Input stream handle */
{
    int c;
    int result = 0;			/* Initialization needed only to prevent
					 * gcc compiler warning */
    if (handle->state == IMG_DONE) {
	return IMG_DONE;
    }

    if (handle->state == IMG_STRING) {
	if (!handle->length--) {
	    handle->state = IMG_DONE;
	    return IMG_DONE;
	}
	return *handle->data++;
    }

    do {
	if (!handle->length--) {
	    handle->state = IMG_DONE;
	    return IMG_DONE;
	}
	c = char64(*handle->data++);
    } while (c == IMG_SPACE);

    if (c > IMG_SPECIAL) {
	handle->state = IMG_DONE;
	return IMG_DONE;
    }

    switch (handle->state++) {
	case 0:
	    handle->c = c<<2;
	    result = ImgGetc(handle);
	    break;
	case 1:
	    result = handle->c | (c>>4);
	    handle->c = (c&0xF)<<4;
	    break;
	case 2:
	    result = handle->c | (c>>2);
	    handle->c = (c&0x3)<<6;
	    break;
	case 3:
	    result = handle->c | c;
	    handle->state = 0;
	    break;
    }
    return result;
}

/*
 *-----------------------------------------------------------------------
 * ImgWrite --
 *
 *  This procedure is invoked to put imaged data into a stream
 *  using ImgPutc.
 *
 * Results:
 *  The return value is the number of characters "written"
 *
 * Side effects:
 *  The base64 handle will change state.
 *
 *-----------------------------------------------------------------------
 */

int
ImgWrite(handle, src, count)
    MFile *handle;	/* mmencode "file" handle */
    CONST char *src;	/* where to get the data */
    int count;		/* number of bytes */
{
    register int i;
    int curcount, bufcount;

    if (handle->state == IMG_CHAN) {
	return Tcl_Write((Tcl_Channel) handle->data, (char *) src, count);
    }
    curcount = handle->data - Tcl_DStringValue(handle->buffer);
    bufcount = curcount + count + count/3 + count/52 + 1024;

    /* make sure that the DString contains enough space */
    if (bufcount >= (handle->buffer->spaceAvl)) {
	Tcl_DStringSetLength(handle->buffer, bufcount + 4096);
	handle->data = Tcl_DStringValue(handle->buffer) + curcount;
    }
    /* write the data */
    for (i=0; (i<count) && (ImgPutc(*src++, handle) != IMG_DONE); i++) {
	/* empty loop body */
    }
    return i;
}
/*
 *-----------------------------------------------------------------------
 *
 * ImgPutc --
 *
 *  This procedure encodes and writes the next byte to a base64
 *  encoded string.
 *
 * Results:
 *  The written byte is returned.
 *
 * Side effects:
 *  the base64 handle will change state.
 *
 *-----------------------------------------------------------------------
 */

static char base64_table[64] = {
    'A', 'B', 'C', 'D', 'E', 'F', 'G', 'H',
    'I', 'J', 'K', 'L', 'M', 'N', 'O', 'P',
    'Q', 'R', 'S', 'T', 'U', 'V', 'W', 'X',
    'Y', 'Z', 'a', 'b', 'c', 'd', 'e', 'f',
    'g', 'h', 'i', 'j', 'k', 'l', 'm', 'n',
    'o', 'p', 'q', 'r', 's', 't', 'u', 'v',
    'w', 'x', 'y', 'z', '0', '1', '2', '3',
    '4', '5', '6', '7', '8', '9', '+', '/'
};

int
ImgPutc(c, handle)
    register int c;		/* character to be written */
    register MFile *handle;	/* handle containing decoder data and state */
{
    /* In fact, here should be checked first if the dynamic
     * string contains enough space for the next character.
     * This would be very expensive to do for each character.
     * Therefore we just allocate 1024 bytes immediately in
     * the beginning and also take a 1024 bytes margin inside
     * every ImgWrite. At least this check is done then only
     * every 256 bytes, which is much faster. Because the GIF
     * header is less than 1024 bytes and pixel data is
     * written in 256 byte portions, this should be safe.
     */

    if (c == IMG_DONE) {
	switch(handle->state) {
	    case 0:
		break;
	    case 1:
		*handle->data++ = base64_table[(handle->c<<4)&63];
		*handle->data++ = '='; *handle->data++ = '='; break;
	    case 2:
		*handle->data++ = base64_table[(handle->c<<2)&63];
		*handle->data++ = '='; break;
	    default:
		handle->state = IMG_DONE;
		return IMG_DONE;
	}
	Tcl_DStringSetLength(handle->buffer,
		(handle->data) - Tcl_DStringValue(handle->buffer));
	handle->state = IMG_DONE;
	return IMG_DONE;
    }

    if (handle->state == IMG_CHAN) {
	char ch = (char) c;
	return (Tcl_Write((Tcl_Channel) handle->data, &ch, 1)>0) ? c : IMG_DONE;
    }

    c &= 0xff;
    switch (handle->state++) {
	case 0:
	    *handle->data++ = base64_table[(c>>2)&63]; break;
	case 1:
	    c |= handle->c << 8;
	    *handle->data++ = base64_table[(c>>4)&63]; break;
	case 2:
	    handle->state = 0;
	    c |= handle->c << 8;
	    *handle->data++ = base64_table[(c>>6)&63];
	    *handle->data++ = base64_table[c&63]; break;
    }
    handle->c = c;
    if (handle->length++ > 52) {
	handle->length = 0;
	*handle->data++ = '\n';
    }
    return c & 0xff;
};

/*
 *-------------------------------------------------------------------------
 * ImgWriteInit --
 *  This procedure initializes a base64 decoder handle for writing
 *
 * Results:
 *  none
 *
 * Side effects:
 *  the base64 handle is initialized
 *
 *-------------------------------------------------------------------------
 */

void
ImgWriteInit(buffer, handle)
    Tcl_DString *buffer;
    MFile *handle;		/* mmencode "file" handle */
{
    Tcl_DStringSetLength(buffer, buffer->spaceAvl);
    handle->buffer = buffer;
    handle->data = Tcl_DStringValue(buffer);
    handle->state = 0;
    handle->length = 0;
}

/*
 *-------------------------------------------------------------------------
 * ImgReadInit --
 *  This procedure initializes a base64 decoder handle for reading.
 *
 * Results:
 *  none
 *
 * Side effects:
 *  the base64 handle is initialized
 *
 *-------------------------------------------------------------------------
 */


int
ImgReadInit(data, c, handle)
    Tcl_Obj *data;		/* string containing initial mmencoded data */
    int c;
    MFile *handle;		/* mmdecode "file" handle */
{
    handle->data = ImgGetByteArrayFromObj(data, &handle->length);
    if (*handle->data == c) {
	handle->state = IMG_STRING;
	return 1;
    }
    c = base64_table[(c>>2)&63];

    while((handle->length) && (char64(*handle->data) == IMG_SPACE)) {
	handle->data++;
	handle->length--;
    }
    if (c != *handle->data) {
	handle->state = IMG_DONE;
	return 0;
    }
    handle->state = 0;
    return 1;
}

/*
 *-------------------------------------------------------------------------
 * tob64 --
 *  This function converts the contents of a file into a base-64
 *  encoded string.
 *
 * Results:
 *  none
 *
 * Side effects:
 *  none
 *
 *-------------------------------------------------------------------------
 */

#ifdef ALLOW_B64
int tob64(clientData, interp, argc, objv)
    ClientData clientData;
    Tcl_Interp *interp;
    int argc;
    Tcl_Obj *CONST objv[];
{
    Tcl_DString dstring;
    MFile handle;
    Tcl_Channel chan;
    char buffer[1024];
    int len;

    if (argc != 2) {
	Tcl_WrongNumArgs(interp, 1, objv, "filename");
	return TCL_ERROR;
    }

    chan = ImgOpenFileChannel(interp, Tcl_GetStringFromObj(objv[1], &len), 0);
    if (!chan) {
	return TCL_ERROR;
    }

    Tcl_DStringInit(&dstring);
    ImgWriteInit(&dstring, &handle);

    while ((len = Tcl_Read(chan, buffer, 1024)) == 1024) {
	ImgWrite(&handle, buffer, 1024);
    }
    if (len > 0) {
	ImgWrite(&handle, buffer, len);
    }
    if ((Tcl_Close(interp, chan) == TCL_ERROR) || (len < 0)) {
	Tcl_DStringFree(&dstring);
	Tcl_AppendResult(interp, Tcl_GetStringFromObj(objv[1], &len),
		": ", Tcl_PosixError(interp), (char *)NULL);
	return TCL_ERROR;
    }
    ImgPutc(IMG_DONE, &handle);

    Tcl_DStringResult(interp, &dstring);
    return TCL_OK;
}

/*
 *-------------------------------------------------------------------------
 * fromb64 --
 *  This function converts a base-64 encoded string into binary form,
 *  which is written to a file.
 *
 * Results:
 *  none
 *
 * Side effects:
 *  none
 *
 *-------------------------------------------------------------------------
 */

int fromb64(clientData, interp, argc, objv)
    ClientData clientData;
    Tcl_Interp *interp;
    int argc;
    Tcl_Obj *CONST objv[];
{
    MFile handle;
    Tcl_Channel chan;
    char buffer[1024];
    int len;

    if (argc != 3) {
	Tcl_WrongNumArgs(interp, 1, objv, "filename data");
	return TCL_ERROR;
    }

    chan = ImgOpenFileChannel(interp, Tcl_GetStringFromObj(objv[1], &len), 0644);
    if (!chan) {
	return TCL_ERROR;
    }

    handle.data = Tcl_GetStringFromObj(objv[2], &handle.length);
    handle.state = 0;

    while ((len = ImgRead(&handle, buffer, 1024)) == 1024) {
	if (Tcl_Write(chan, buffer, 1024) != 1024) {
	    goto writeerror;
	}
    }
    if (len > 0) {
	if (Tcl_Write(chan, buffer, len) != len) {
	    goto writeerror;
	}
    }
    if (Tcl_Close(interp, chan) == TCL_ERROR) {
	return TCL_ERROR;
    }
    return TCL_OK;

    writeerror:
    Tcl_AppendResult(interp, Tcl_GetStringFromObj(objv[1], &len), ": ",
	    Tcl_PosixError(interp), (char *)NULL);
    return TCL_ERROR;
}
#endif
