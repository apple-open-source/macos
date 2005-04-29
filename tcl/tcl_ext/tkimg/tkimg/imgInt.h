/* imgInt.h */

#ifndef _IMGINT
#define _IMGINT

#define NEED_REAL_STDIO 

#include "tcl.h"
#include "tk.h"

#ifdef _LANG
#include "tkVMacro.h"
#endif

#ifndef RESOURCE_INCLUDED

#ifdef HAVE_FCNTL_H
#include <fcntl.h>
#endif

#include "img.h"

typedef struct {
    Tcl_DString *buffer;/* pointer to dynamical string */
    char *data;		/* mmencoded source string */
    int c;		/* bits left over from previous char */
    int state;		/* decoder state (0-4 or IMG_DONE) */
    int length;		/* length of physical line already written */
} MFile;

#define IMG_SPECIAL	 (1<<8)
#define IMG_PAD		(IMG_SPECIAL+1)
#define IMG_SPACE	(IMG_SPECIAL+2)
#define IMG_BAD		(IMG_SPECIAL+3)
#define IMG_DONE	(IMG_SPECIAL+4)
#define IMG_CHAN        (IMG_SPECIAL+5)
#define IMG_STRING	(IMG_SPECIAL+6)

#define IMG_TCL		(1<<9)
#define IMG_OBJS	(1<<10)
#define IMG_PERL	(1<<11)
#define IMG_UTF		(1<<12)

TCL_EXTERN(int) ImgPhotoPutBlock _ANSI_ARGS_((Tk_PhotoHandle handle,
	Tk_PhotoImageBlock *blockPtr, int x, int y, int width, int height));

TCL_EXTERN(int) ImgLoadLib _ANSI_ARGS_((Tcl_Interp *interp, CONST char *libName,
	VOID **handlePtr, char **symbols, int num));
TCL_EXTERN(void) ImgLoadFailed _ANSI_ARGS_((VOID **handlePtr));

TCL_EXTERN(int) ImgObjInit _ANSI_ARGS_((Tcl_Interp *interp));
TCL_EXTERN(char*) ImgGetStringFromObj _ANSI_ARGS_((Tcl_Obj *objPtr,
	int *lengthPtr));
TCL_EXTERN(char*) ImgGetByteArrayFromObj _ANSI_ARGS_((Tcl_Obj *objPtr,
	int *lengthPtr));
TCL_EXTERN(int) ImgListObjGetElements _ANSI_ARGS_((Tcl_Interp *interp,
	Tcl_Obj *objPtr, int *argc, Tcl_Obj ***argv));

TCL_EXTERN(int) ImgGetc _ANSI_ARGS_((MFile *handle));
TCL_EXTERN(int) ImgRead _ANSI_ARGS_((MFile *handle, char *dst, int count));
TCL_EXTERN(int) ImgPutc _ANSI_ARGS_((int c, MFile *handle));
TCL_EXTERN(int) ImgWrite _ANSI_ARGS_((MFile *handle, CONST char *src, int count));
TCL_EXTERN(void) ImgWriteInit _ANSI_ARGS_((Tcl_DString *buffer, MFile *handle));
TCL_EXTERN(int) ImgReadInit _ANSI_ARGS_((Tcl_Obj *data, int c, MFile *handle));
TCL_EXTERN(Tcl_Channel) ImgOpenFileChannel _ANSI_ARGS_((Tcl_Interp *interp, 
	CONST char *fileName, int permissions));
TCL_EXTERN(void) ImgFixChanMatchProc _ANSI_ARGS_((Tcl_Interp **interp, Tcl_Channel *chan,
	CONST char **file, Tcl_Obj **format, int **width, int **height));
TCL_EXTERN(void) ImgFixObjMatchProc _ANSI_ARGS_((Tcl_Interp **interp, Tcl_Obj **data,
	Tcl_Obj **format, int **width, int **height));
TCL_EXTERN(void) ImgFixStringWriteProc _ANSI_ARGS_((Tcl_DString *data, Tcl_Interp **interp,
	Tcl_DString **dataPtr, Tcl_Obj **format, Tk_PhotoImageBlock **blockPtr));

TCL_EXTERN(int) ImgInitTIFFzip _ANSI_ARGS_((VOID *, int));
TCL_EXTERN(int) ImgInitTIFFjpeg _ANSI_ARGS_((VOID *, int));
TCL_EXTERN(int) ImgInitTIFFpixar _ANSI_ARGS_((VOID *, int));
TCL_EXTERN(int) ImgLoadJpegLibrary _ANSI_ARGS_((void));

#endif /* RESOURCE_INCLUDED */

#endif /* _IMGINT */
