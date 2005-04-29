/*
 *	imgObj.tcl
 */

#include "imgInt.h"
#include <string.h>
#include <stdlib.h>
#include "tclInt.h"

/*
 * The variable "initialized" contains flags indicating which
 * version of Tcl or Perl we are running:
 *
 *      IMG_PERL	perl
 *	IMG_TCL		Tcl
 *	IMG_OBJS	using (Tcl_Obj *) in stead of (char *)
 *	IMG_UTF		Tcl supports UTF-8
 *
 * These flags will be determined at runtime (except the IMG_PERL
 * flag, for now), so we can use the same dynamic library for all
 * Tcl/Tk versions (and for Perl/Tk in the future).
 */

static int initialized = 0;
static Tcl_ObjType* byteArrayType = 0;

int
ImgObjInit(interp)
    Tcl_Interp *interp;
{
    Tcl_CmdInfo cmdInfo;
#ifdef _LANG
    return (initialized = IMG_PERL|IMG_OBJS);
#else
    char *version;
    initialized = IMG_TCL;
    if (!Tcl_GetCommandInfo(interp,"image", &cmdInfo)) {
	    Tcl_AppendResult(interp, "cannot find the \"image\" command",
		    (char *) NULL);
	    initialized = 0;
	    return TCL_ERROR;
    }
    if (cmdInfo.isNativeObjectProc == 1) {
	initialized |= IMG_OBJS; /* we use objects */
    }
    version = Tcl_PkgRequire(interp, "Tcl", "8.0", 0);
    if (version && (version[2] > '0')) {
	initialized |= IMG_UTF;
    }
    return initialized;
#endif
}

/*
 * The following structure is the internal rep for a ByteArray object.
 * Keeps track of how much memory has been used and how much has been
 * allocated for the byte array to enable growing and shrinking of the
 * ByteArray object with fewer mallocs.  The ByteArray is also guaranteed
 * to have a terminating 0 byte at the end of the used length.
 */

typedef struct ByteArray {
    int used;			/* The number of bytes used in the byte
				 * array. */
    int allocated;		/* The amount of space actually allocated
				 * minus 1 byte. */
    unsigned char bytes[4];	/* The array of bytes.  The actual size of
				 * this field depends on the 'allocated' field
				 * above. */
} ByteArray;

/*
 *----------------------------------------------------------------------
 *
 * ImgGetStringFromObj --
 *
 *	Returns the string representation's byte array pointer and length
 *	for an object.
 *
 * Results:
 *	Returns a pointer to the string representation of objPtr.  If
 *	lengthPtr isn't NULL, the length of the string representation is
 *	stored at *lengthPtr. The byte array referenced by the returned
 *	pointer must not be modified by the caller. Furthermore, the
 *	caller must copy the bytes if they need to retain them since the
 *	object's string rep can change as a result of other operations.
 *      REMARK: This function reacts a little bit different than
 *	Tcl_GetStringFromObj():
 *	- objPtr is allowed to be NULL. In that case the NULL pointer
 *	  will be returned, and the length will be reported to be 0;
 *	In the Img code there is never a distinction between en empty
 *	string and a NULL pointer, while the latter is easier to check
 *	for. That's the reason for this difference.
 *
 * Side effects:
 *	May call the object's updateStringProc to update the string
 *	representation from the internal representation.
 *
 *----------------------------------------------------------------------
 */

char *
ImgGetStringFromObj(objPtr, lengthPtr)
    register Tcl_Obj *objPtr;	/* Object whose string rep byte pointer
				 * should be returned, or NULL */
    register int *lengthPtr;	/* If non-NULL, the location where the
				 * string rep's byte array length should be
				 * stored. If NULL, no length is stored. */
{
    if (!objPtr) {
	if (lengthPtr != NULL) {
	    *lengthPtr = 0;
	}
	return (char *) NULL;
    } else
#ifdef _LANG
    {
	char *string = LangString((Arg) objPtr);
	if (lengthPtr != NULL) {
	    *lengthPtr = string ? strlen(string) : 0;
	}
	return string;
    }
#else /* _LANG */
    if (initialized & IMG_OBJS) {
	return Tcl_GetStringFromObj(objPtr, lengthPtr);
    } else {
	char *string =  (char *) objPtr;
	if (lengthPtr != NULL) {
	    *lengthPtr = string ? strlen(string) : 0;
	}
	return string;
    }
#endif /* _LANG */
}
/*
 *----------------------------------------------------------------------
 *
 * ImgGetByteArrayFromObj --
 *
 *	Returns the binary representation and length
 *	for a byte array object.
 *
 * Results:
 *	Returns a pointer to the byte representation of objPtr.  If
 *	lengthPtr isn't NULL, the length of the string representation is
 *	stored at *lengthPtr. The byte array referenced by the returned
 *	pointer must not be modified by the caller. Furthermore, the
 *	caller must copy the bytes if they need to retain them since the
 *	object's representation can change as a result of other operations.
 *
 * Side effects:
 *	May call the object's updateStringProc to update the string
 *	representation from the internal representation.
 *
 *----------------------------------------------------------------------
 */
char *
ImgGetByteArrayFromObj(objPtr, lengthPtr)
    register Tcl_Obj *objPtr;	/* Object whose string rep byte pointer
				 * should be returned, or NULL */
    register int *lengthPtr;	/* If non-NULL, the location where the
				 * string rep's byte array length should be
				 * stored. If NULL, no length is stored. */
{
#ifdef _LANG
    char *string = LangString((Arg) objPtr);
    if (lengthPtr != NULL) {
	*lengthPtr = string ? strlen(string) : 0;
    }
    return string;
#else /* _LANG */
    if (initialized & IMG_OBJS) {
	ByteArray *baPtr;
	if (byteArrayType) {
	    if (objPtr->typePtr != byteArrayType) {
		byteArrayType->setFromAnyProc(NULL, objPtr);
	    }
        } else if (objPtr->typePtr && !strcmp(objPtr->typePtr->name, "bytearray")) {
	    byteArrayType = objPtr->typePtr;
        } else {
	    return Tcl_GetStringFromObj(objPtr, lengthPtr);
	}
	baPtr = (ByteArray *) (objPtr)->internalRep.otherValuePtr;
	if (lengthPtr != NULL) {
	    *lengthPtr = baPtr->used;
	}
	return (unsigned char *) baPtr->bytes;
    } else {
	char *string =  (char *) objPtr;
	if (lengthPtr != NULL) {
	    *lengthPtr = string ? strlen(string) : 0;
	}
	return string;
    }
#endif /* _LANG */
}

/*
 *----------------------------------------------------------------------
 *
 * ImgListObjGetElements --
 *
 *	Splits an object into its compoments.
 *
 * Results:
 *	If objPtr is a valid list (or can be converted to one),
 *	TCL_OK will be returned. The object will be split in
 *	its components.
 *	Otherwise TCL_ERROR is returned. If interp is not a NULL
 *	pointer, an error message will be left in it as well.
 *
 * Side effects:
 *	May call the object's updateStringProc to update the string
 *	representation from the internal representation.
 *
 *----------------------------------------------------------------------
 */

int
ImgListObjGetElements(interp, objPtr, objc, objv)
    Tcl_Interp *interp;
    Tcl_Obj *objPtr;
    int *objc;
    Tcl_Obj ***objv;
{
    static Tcl_Obj *staticObj = (Tcl_Obj *) NULL;

    if (objPtr == NULL) {
	*objc = 0;
	return TCL_OK;
    }
#ifndef _LANG
    if (!(initialized & IMG_OBJS)) {
	if (staticObj != (Tcl_Obj *) NULL) {
	    Tcl_DecrRefCount(staticObj);
	}
	objPtr = staticObj = Tcl_NewStringObj((char *) objPtr, -1);
	Tcl_IncrRefCount(staticObj);
    }
#endif
    return Tcl_ListObjGetElements(interp, objPtr, objc, objv);
}
/*
 *----------------------------------------------------------------------
 *
 * ImgOpenFileChannel --
 *
 *	Open a file channel in binary mode. If permissions is 0, the
 *	file will be opened in read mode, otherwise in write mode.
 *
 * Results:
 *	The same as Tcl_OpenFileChannel, only the file will
 *	always be opened in binary mode without encoding.
 *
 * Side effects:
 *	If function fails, an error message will be left in the
 *	interpreter.
 *
 *----------------------------------------------------------------------
 */

Tcl_Channel
ImgOpenFileChannel(interp, fileName, permissions)
    Tcl_Interp *interp;
    CONST char *fileName;
    int permissions;
{
    Tcl_Channel chan = Tcl_OpenFileChannel(interp, (char *) fileName,
	    permissions?"w":"r", permissions);
    if (!chan) {
	return (Tcl_Channel) NULL;
    }
    if (Tcl_SetChannelOption(interp, chan, "-translation", "binary") != TCL_OK) {
	Tcl_Close(interp, chan);
	return (Tcl_Channel) NULL;
    }
    return chan;
}

/*
 *    Various Compatibility functions
 */

void
ImgFixChanMatchProc(interp, chan, file, format, width, height)
    Tcl_Interp **interp;
    Tcl_Channel *chan;
    CONST char **file;
    Tcl_Obj **format;
    int **width;
    int **height;
{
    Tcl_Interp *tmp;

    if (initialized & IMG_PERL) {
	return;
    }
    if (initialized & IMG_OBJS) {
	tmp = (Tcl_Interp *) *height;
	if (tmp->result != ((Interp *) tmp)->resultSpace) {
	    return;
	}
    } else {
	tmp = (Tcl_Interp *) NULL;
    }

    *height = *width;
    *width = (int *) *format;
    *format = (Tcl_Obj *) *file;
    *file = (CONST char *) *chan;
    *chan = (Tcl_Channel) *interp;
    *interp = tmp;
}


void
ImgFixObjMatchProc(interp, data, format, width, height)
    Tcl_Interp **interp;
    Tcl_Obj **data;
    Tcl_Obj **format;
    int **width;
    int **height;
{
    Tcl_Interp *tmp;

    if (initialized & IMG_PERL) {
	return;
    }
    if (initialized & IMG_OBJS) {
	tmp = (Tcl_Interp *) *height;
	if (tmp->result != ((Interp *) tmp)->resultSpace) {
	    return;
	}
    } else {
	tmp = (Tcl_Interp *) NULL;
    }
    *height = *width;
    *width = (int *) *format;
    *format = (Tcl_Obj *) *data;
    *data = (Tcl_Obj *) *interp;
    *interp = tmp;
}

void
ImgFixStringWriteProc(data, interp, dataPtr, format, blockPtr)
    Tcl_DString *data;
    Tcl_Interp **interp;
    Tcl_DString **dataPtr;
    Tcl_Obj **format;
    Tk_PhotoImageBlock **blockPtr;
{
    if (!*blockPtr) {
	*blockPtr = (Tk_PhotoImageBlock *) *format;	
	*format   = (Tcl_Obj *) *dataPtr;
	*dataPtr  = data;
	Tcl_DStringInit(data);
    }
}
