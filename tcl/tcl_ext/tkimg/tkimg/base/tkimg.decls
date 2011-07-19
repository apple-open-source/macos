# tkimg.decls -- -*- tcl -*-
#
# This file contains the declarations for all supported public functions
# that are exported by the TKIMG library via the stubs table. This file
# is used to generate the tkimgDecls.h/tkimgStubsLib.c/tkimgStubsInit.c
# files.
#

# Declare each of the functions in the public BLT interface.  Note that
# the an index should never be reused for a different function in order
# to preserve backwards compatibility.

library tkimg

# Define the TKIMG interface:

interface tkimg
scspec TKIMGAPI

#########################################################################
###  Reading and writing image data from channels and/or strings.

declare 0 {
    Tcl_Channel tkimg_OpenFileChannel(Tcl_Interp *interp,
	const char *fileName, int permissions)
}
declare 1 {
    int tkimg_ReadInit(Tcl_Obj *data, int c, tkimg_MFile *handle)
}
declare 2 {
    void tkimg_WriteInit(Tcl_DString *buffer, tkimg_MFile *handle)
}
declare 3 {
    int tkimg_Getc(tkimg_MFile *handle)
}
declare 4 {
    int tkimg_Read(tkimg_MFile *handle, char *dst, int count)
}
declare 5 {
    int tkimg_Putc(int c, tkimg_MFile *handle)
}
declare 6 {
    int tkimg_Write(tkimg_MFile *handle, const char *src, int count)
}
declare 7 {
    void tkimg_ReadBuffer(int onOff)
}

#########################################################################
###  Specialized put block handling transparency

declare 10 {
    int tkimg_PhotoPutBlock(Tcl_Interp *interp, Tk_PhotoHandle handle,
	Tk_PhotoImageBlock *blockPtr, int x, int y, int width, int height, int flags)
}
declare 11 {
    int tkimg_PhotoExpand(Tcl_Interp *interp, Tk_PhotoHandle handle,
	int width, int height)
}
declare 12 {
    int tkimg_PhotoSetSize(Tcl_Interp *interp, Tk_PhotoHandle handle,
	int width, int height)
}

#########################################################################
###  Utilities to help handling the differences in 8.3.2 and 8.2 image
###  types. Not used any more.

declare 20 {
    void tkimg_FixChanMatchProc(Tcl_Interp **interp, Tcl_Channel *chan,
	const char **file, Tcl_Obj **format, int **width, int **height)
}
declare 21 {
    void tkimg_FixObjMatchProc(Tcl_Interp **interp, Tcl_Obj **data,
	Tcl_Obj **format, int **width, int **height)
}
declare 22 {
    void tkimg_FixStringWriteProc(Tcl_DString *data, Tcl_Interp **interp,
	Tcl_DString **dataPtr, Tcl_Obj **format, Tk_PhotoImageBlock **blockPtr)
}

#########################################################################
###  Like the core functions, except that they accept objPtr == NULL.
###  The byte array function also handles both UTF and non-UTF cores.

declare 30 {
    const char *tkimg_GetStringFromObj(Tcl_Obj *objPtr, int *lengthPtr)
}
declare 31 {
    unsigned char *tkimg_GetByteArrayFromObj(Tcl_Obj *objPtr, int *lengthPtr)
}
declare 32 {
    int tkimg_ListObjGetElements(Tcl_Interp *interp, Tcl_Obj *objPtr, int *argc, Tcl_Obj ***argv)
}

#########################################################################
