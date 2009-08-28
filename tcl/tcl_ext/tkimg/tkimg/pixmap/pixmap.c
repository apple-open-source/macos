/*
 * tkImgPmap.c --
 *
 *	This file implements images of type "pixmap" for Tk.
 *
 * Copyright (c) 1996, Expert Interface Technologies
 *
 * See the file "license.terms" for information on usage and redistribution
 * of this file, and for a DISCLAIMER OF ALL WARRANTIES.
 *
 */

#include <ctype.h>
#include <stdlib.h>
#include <string.h>
#include <pixmapInt.h>
#include <tkimg.h>

#if defined(__WIN32__) && !defined (__GNUC__)
#define strncasecmp strnicmp
#endif

#ifndef MAC_TCL
#include <sys/types.h>
#include <sys/stat.h>
#endif

#ifndef TCL_STUB_MAGIC
EXTERN void		panic _ANSI_ARGS_(TCL_VARARGS(char *,format));
#endif

#define UCHAR(c) ((unsigned char) (c))

/*
 * Prototypes for procedures used only locally in this file:
 */

static int		TkimgXpmCreate _ANSI_ARGS_((Tcl_Interp *interp,
			    const char *name, int argc, Tcl_Obj *objv[],
			    const Tk_ImageType *typePtr, Tk_ImageMaster master,
			    ClientData *clientDataPtr));
static ClientData	TkimgXpmGet _ANSI_ARGS_((Tk_Window tkwin,
			    ClientData clientData));
static void		TkimgXpmDisplay _ANSI_ARGS_((ClientData clientData,
			    Display *display, Drawable drawable,
			    int imageX, int imageY, int width, int height,
			    int drawableX, int drawableY));
static void		TkimgXpmFree _ANSI_ARGS_((ClientData clientData,
			    Display *display));
static void		TkimgXpmDelete _ANSI_ARGS_((ClientData clientData));
static int		TkimgXpmCmd _ANSI_ARGS_((ClientData clientData,
			    Tcl_Interp *interp, int argc, CONST84 char **argv));
static void		TkimgXpmCmdDeletedProc _ANSI_ARGS_((
			    ClientData clientData));
static void		TkimgXpmConfigureInstance _ANSI_ARGS_((
			    PixmapInstance *instancePtr));
static int		TkimgXpmConfigureMaster _ANSI_ARGS_((
			    PixmapMaster *masterPtr, int argc, CONST84 char **argv,
			    int flags));
static int		TkimgXpmGetData _ANSI_ARGS_((Tcl_Interp *interp,
			    PixmapMaster *masterPtr));
static CONST84 char **	TkimgXpmGetDataFromFile _ANSI_ARGS_((Tcl_Interp * interp,
			    char * string, int * numLines_return));
static CONST84 char **	TkimgXpmGetDataFromString _ANSI_ARGS_((Tcl_Interp*interp,
			    char * string, int * numLines_return));
static void 		TkimgXpmGetPixmapFromData _ANSI_ARGS_((
			    Tcl_Interp * interp,
			    PixmapMaster *masterPtr,
			    PixmapInstance *instancePtr));
static char *		GetType _ANSI_ARGS_((char * colorDefn,
			    int  * type_ret));
static char *		GetColor _ANSI_ARGS_((char * colorDefn,
			    char * colorName, int * type_ret));

/*
 * Information used for parsing configuration specs:
 */

static Tk_ConfigSpec configSpecs[] = {
    {TK_CONFIG_STRING, "-data", (char *) NULL, (char *) NULL,
	(char *) NULL, Tk_Offset(PixmapMaster, dataString), TK_CONFIG_NULL_OK},
    {TK_CONFIG_STRING, "-file", (char *) NULL, (char *) NULL,
	(char *) NULL, Tk_Offset(PixmapMaster, fileString), TK_CONFIG_NULL_OK},
    {TK_CONFIG_END, (char *) NULL, (char *) NULL, (char *) NULL,
	(char *) NULL, 0, 0}
};

Tk_ImageType imgPixmapImageType = {
    "pixmap",				/* name */
    (Tk_ImageCreateProc *) TkimgXpmCreate,/* createProc */
    TkimgXpmGet,				/* getProc */
    TkimgXpmDisplay,			/* displayProc */
    TkimgXpmFree,				/* freeProc */
    TkimgXpmDelete,			/* deleteProc */
#ifdef TK_CONFIG_OBJS
    (Tk_ImagePostscriptProc *) NULL,	/* postscriptProc */
#endif
    (Tk_ImageType *) NULL		/* nextPtr */
};

/*
 *----------------------------------------------------------------------
 *
 * TkimgXpmCreate --
 *
 *	This procedure is called by the Tk image code to create "pixmap"
 *	images.
 *
 * Results:
 *	A standard Tcl result.
 *
 * Side effects:
 *	The data structure for a new image is allocated.
 *
 *----------------------------------------------------------------------
 */

static int
TkimgXpmCreate(interp, name, argc, objv, typePtr, master, clientDataPtr)
    Tcl_Interp *interp;		/* Interpreter for application containing
				 * image. */
    const char *name;			/* Name to use for image. */
    int argc;			/* Number of arguments. */
    Tcl_Obj *objv[];		/* Argument strings for options (doesn't
				 * include image name or type). */
    const Tk_ImageType *typePtr;	/* Pointer to our type record (not used). */
    Tk_ImageMaster master;	/* Token for image, to be used by us in
				 * later callbacks. */
    ClientData *clientDataPtr;	/* Store manager's token for image here;
				 * it will be returned in later callbacks. */
{
    PixmapMaster *masterPtr;
    int i;
    char *argvbuf[10];
    CONST84 char **args = (CONST84 char **) argvbuf;

    /*
     * Convert the objc/objv arguments into string equivalent.
     */
    if (argc > 10) {
	args = (CONST84 char **) ckalloc(argc * sizeof(char *));
    }
    for (i = 0; i < argc; i++) {
	args[i] = tkimg_GetStringFromObj(objv[i], NULL);
    }

    masterPtr = (PixmapMaster *) ckalloc(sizeof(PixmapMaster));
    masterPtr->tkMaster = master;
    masterPtr->interp = interp;
    masterPtr->imageCmd = Tcl_CreateCommand(interp, name, TkimgXpmCmd,
	    (ClientData) masterPtr, TkimgXpmCmdDeletedProc);

    masterPtr->fileString = NULL;
    masterPtr->dataString = NULL;
    masterPtr->data = NULL;
    masterPtr->isDataAlloced = 0;
    masterPtr->instancePtr = NULL;

    if (TkimgXpmConfigureMaster(masterPtr, argc, args, 0) != TCL_OK) {
	TkimgXpmDelete((ClientData) masterPtr);
	if (args != ((CONST84 char **) argvbuf)) {
	    ckfree((char *) args);
	}
	return TCL_ERROR;
    }
    *clientDataPtr = (ClientData) masterPtr;
    if (args != ((CONST84 char **) argvbuf)) {
	ckfree((char *) args);
    }
    return TCL_OK;
}

/*
 *----------------------------------------------------------------------
 *
 * TkimgXpmConfigureMaster --
 *
 *	This procedure is called when a pixmap image is created or
 *	reconfigured.  It process configuration options and resets
 *	any instances of the image.
 *
 * Results:
 *	A standard Tcl return value.  If TCL_ERROR is returned then
 *	an error message is left in masterPtr->interp->result.
 *
 * Side effects:
 *	Existing instances of the image will be redisplayed to match
 *	the new configuration options.
 *
 *	If any error occurs, the state of *masterPtr is restored to
 *	previous state.
 *
 *----------------------------------------------------------------------
 */

static int
TkimgXpmConfigureMaster(masterPtr, argc, argv, flags)
    PixmapMaster *masterPtr;	/* Pointer to data structure describing
				 * overall pixmap image to (reconfigure). */
    int argc;			/* Number of entries in argv. */
    CONST84 char **argv;	/* Pairs of configuration options for image. */
    int flags;			/* Flags to pass to Tk_ConfigureWidget,
				 * such as TK_CONFIG_ARGV_ONLY. */
{
    PixmapInstance *instancePtr;
    char * oldData, * oldFile;

    oldData = masterPtr->dataString;
    oldFile = masterPtr->fileString;

    if (Tk_ConfigureWidget(masterPtr->interp, Tk_MainWindow(masterPtr->interp),
	    configSpecs, argc, argv, (char *) masterPtr, flags)
	    != TCL_OK) {
	return TCL_ERROR;
    }

    if (masterPtr->dataString != NULL ||
	masterPtr->fileString != NULL) {
	if (TkimgXpmGetData(masterPtr->interp, masterPtr) != TCL_OK) {
	    goto error;
	}
    } else {
	Tcl_AppendResult(masterPtr->interp,
	    "must specify one of -data or -file", NULL);
	goto error;
    }

    /*
     * Cycle through all of the instances of this image, regenerating
     * the information for each instance.  Then force the image to be
     * redisplayed everywhere that it is used.
     */
    for (instancePtr = masterPtr->instancePtr; instancePtr != NULL;
	instancePtr = instancePtr->nextPtr) {
	TkimgXpmConfigureInstance(instancePtr);
    }

    if (masterPtr->data) {
	Tk_ImageChanged(masterPtr->tkMaster, 0, 0,
	    masterPtr->size[0], masterPtr->size[1],
	    masterPtr->size[0], masterPtr->size[1]);
    } else {
	Tk_ImageChanged(masterPtr->tkMaster, 0, 0, 0, 0, 0, 0);
    }

    return TCL_OK;

  error:
    /* Restore it to the original (possible valid) mode */
    if (masterPtr->dataString && masterPtr->dataString != oldData) {
	ckfree(masterPtr->dataString);
    }
    if (masterPtr->fileString && masterPtr->fileString != oldFile) {
	ckfree(masterPtr->fileString);
    }
    masterPtr->dataString = oldData;
    masterPtr->fileString = oldFile;
    return TCL_ERROR;
}

/*
 *----------------------------------------------------------------------
 *
 * TkimgXpmGetData --
 *
 *	Given a file name or ASCII string, this procedure parses the
 *	file or string contents to produce binary data for a pixmap.
 *
 * Results:
 *	If the pixmap description was parsed successfully then the data
 *	is read into an array of strings. This array will later be used
 *	to create X Pixmaps for each instance.
 *
 * Side effects:
 *	The masterPtr->data array is allocated when successful. Contents of
 *	*masterPtr is changed only when successful.
 *----------------------------------------------------------------------
 */

static int
TkimgXpmGetData(interp, masterPtr)
    Tcl_Interp *interp;			/* For reporting errors. */
    PixmapMaster *masterPtr;
{
    CONST84 char ** data = NULL;
    int  isAllocated = 0;		/* do we need to free "data"? */
    int listArgc;
    CONST84 char ** listArgv = NULL;
    int numLines;
    int size[2];
    int cpp;
    int ncolors;
    int code = TCL_OK;

    if (masterPtr->fileString != NULL) {
	if (Tcl_IsSafe(interp)) {
	    Tcl_AppendResult(interp, "can't get image from a file in a",
		    " safe interpreter", (char *) NULL);
	    return TCL_ERROR;
	}
	data = TkimgXpmGetDataFromFile(interp, masterPtr->fileString, &numLines);
	isAllocated = 1;
    }
    else if (masterPtr->dataString != NULL) {
	data = TkimgXpmGetDataFromString(interp,masterPtr->dataString,&numLines);
	isAllocated = 1;
    }
    else {
	/* Should have been enforced by TkimgXpmConfigureMaster() */
	panic("TkimgXpmGetData(): -data and -file are all NULL");
    }

    if (data == NULL) {
	/* nothing has been allocated yet. Don't need to goto done */
	return TCL_ERROR;
    }

    /* Parse the first line of the data and get info about this pixmap */
    if (Tcl_SplitList(interp, data[0], &listArgc, &listArgv) != TCL_OK) {
	code = TCL_ERROR; goto done;
    }

    if (listArgc < 4) {	/* file format error */
	code = TCL_ERROR; goto done;
    }

    if (Tcl_GetInt(interp, listArgv[0], &size[0]) != TCL_OK) {
	code = TCL_ERROR; goto done;
    }
    if (Tcl_GetInt(interp, listArgv[1], &size[1]) != TCL_OK) {
	code = TCL_ERROR; goto done;
    }
    if (Tcl_GetInt(interp, listArgv[2], &ncolors) != TCL_OK) {
	code = TCL_ERROR; goto done;
    }
    if (Tcl_GetInt(interp, listArgv[3], &cpp) != TCL_OK) {
	code = TCL_ERROR; goto done;
    }

    if (isAllocated) {
	if (numLines != size[1] + ncolors + 1) {
	    /* the number of lines read from the file/data
	     * is not the same as specified in the data
	     */
	    code = TCL_ERROR; goto done;
	}
    }

  done:
    if (code == TCL_OK) {
	if (masterPtr->isDataAlloced && masterPtr->data) {
	    ckfree((char*)masterPtr->data);
	}
	masterPtr->isDataAlloced = isAllocated;
	masterPtr->data = (char **) data;
	masterPtr->size[0] = size[0];
	masterPtr->size[1] = size[1];
	masterPtr->cpp = cpp;
	masterPtr->ncolors = ncolors;
    } else {
	if (isAllocated && data) {
	    ckfree((char*)data);
	}

	Tcl_ResetResult(interp);
	Tcl_AppendResult(interp, "File format error", NULL);
    }

    if (listArgv) {
	ckfree((char*)listArgv);
    }

    return code;
}

static CONST84 char **
TkimgXpmGetDataFromString(interp, string, numLines_return)
    Tcl_Interp * interp;
    char * string;
    int * numLines_return;
{
    int quoted;
    char * p, * list;
    int numLines;
    CONST84 char ** data;

    /* skip the leading blanks (leading blanks are not defined in the
     * the XPM definition, but skipping them shouldn't hurt. Also, the ability
     * to skip the leading blanks is good for using in-line XPM data in TCL
     * scripts
     */
    while (isspace(UCHAR(*string))) {
	++ string;
    }

    /* parse the header */
    if (strncmp("/* XPM", string, 6) != 0) {
	goto error;
    }

    /* strip the comments */
    for (quoted = 0, p=string; *p;) {
	if (!quoted) {
	    if (*p == '"') {
		quoted = 1;
		++ p;
		continue;
	    }

	    if (*p == '/' && *(p+1) == '*') {
		*p++ = ' ';
		*p++ = ' ';
		while (1) {
		    if (*p == 0) {
			break;
		    }
		    if (*p == '*' && *(p+1) == '/') {
			*p++ = ' ';
			*p++ = ' ';
			break;
		    }
		    *p++ = ' ';
		}
		continue;
	    }
	    ++ p;
	} else {
	    if (*p == '"') {
		quoted = 0;
	    }
	    ++ p;
	}
    }

    /* Search for the opening brace */
    for (p=string; *p;) {
	if (*p != '{') {
	    ++ p;
	} else {
	    ++p;
	    break;
	}
    }

    /* Change the buffer in to a proper TCL list */
    quoted = 0;
    list = p;

    while (*p) {
	if (!quoted) {
	    if (*p == '"') {
		quoted = 1;
		++ p;
		continue;
	    }

	    if (isspace(UCHAR(*p))) {
		*p = ' ';
	    }
	    else if (*p == ',') {
		*p = ' ';
	    }
	    else if (*p == '}') {
		*p = 0;
		break;
	    }
	    ++p;
	}
	else {
	    if (*p == '"') {
		quoted = 0;
	    }
	    ++ p;
	}
    }

    /* The following code depends on the fact that Tcl_SplitList
     * strips away double quoates inside a list: ie:
     * if string == "\"1\" \"2\"" then
     *		list[0] = "1"
     *		list[1] = "2"
     * and NOT
     *
     *		list[0] = "\"1\""
     *		list[1] = "\"2\""
     */
    if (Tcl_SplitList(interp, list, &numLines, &data) != TCL_OK) {
	goto error;
    } else {
	if (numLines == 0) {
	    /* error: empty data? */
	    if (data != NULL) {
		ckfree((char*)data);
		goto error;
	    }
	}
	* numLines_return = numLines;
	return data;
    }

  error:
    Tcl_AppendResult(interp, "File format error", NULL);
    return (CONST84 char**) NULL;
}

static CONST84 char **
TkimgXpmGetDataFromFile(interp, fileName, numLines_return)
    Tcl_Interp * interp;
    char * fileName;
    int * numLines_return;
{
    Tcl_Channel chan;
    int size;
    CONST84 char ** data = (CONST84 char **) NULL;
    char *cmdBuffer = NULL;

    chan = tkimg_OpenFileChannel(interp, fileName, 0);
    if (!chan) {
	return (CONST84 char **) NULL;
    }

    size = Tcl_Seek(chan, 0, SEEK_END);
    if (size > 0) {
	Tcl_Seek(chan, 0, SEEK_SET);
	cmdBuffer = (char *) ckalloc(size+1);
	size = Tcl_Read(chan, cmdBuffer, size);
    }
    if (Tcl_Close(interp, chan) != TCL_OK) {
	goto error;
    }
    if (size < 0) {
	Tcl_AppendResult(interp, fileName, ": ",
		Tcl_PosixError(interp), (char *)NULL);
	goto error;
    }
    cmdBuffer[size] = 0;

    data = TkimgXpmGetDataFromString(interp, cmdBuffer, numLines_return);
    error:
    if (cmdBuffer) {
	ckfree(cmdBuffer);
    }
    return data;
}


static char *
GetType(colorDefn, type_ret)
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

/*
 * colorName is guaranteed to be big enough
 */

static char *
GetColor(colorDefn, colorName, type_ret)
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

/*----------------------------------------------------------------------
 * TkimgXpmGetPixmapFromData --
 *
 *	Creates a pixmap for an image instance.
 *----------------------------------------------------------------------
 */

static void
TkimgXpmGetPixmapFromData(interp, masterPtr, instancePtr)
    Tcl_Interp * interp;
    PixmapMaster *masterPtr;
    PixmapInstance *instancePtr;
{
    XImage * image = NULL, * mask = NULL;
    int depth, i, j, k, lOffset, isTransp = 0, isMono;
    ColorStruct * colors;

    depth = Tk_Depth(instancePtr->tkwin);

    switch ((Tk_Visual(instancePtr->tkwin))->class) {
      case StaticGray:
      case GrayScale:
	isMono = 1;
	break;
      default:
	isMono = 0;
    }

    TkimgXpmAllocTmpBuffer(masterPtr, instancePtr, &image, &mask);

    /*
     * Parse the colors
     */
    lOffset = 1;
    colors = (ColorStruct*)ckalloc(sizeof(ColorStruct)*masterPtr->ncolors);

    /*
     * Initialize the color structures
     */
    for (i=0; i<masterPtr->ncolors; i++) {
	colors[i].colorPtr = NULL;
	if (masterPtr->cpp == 1) {
	    colors[i].c = 0;
	} else {
	    colors[i].cstring = (char*)ckalloc(masterPtr->cpp);
	    colors[i].cstring[0] = 0;
	}
    }

    for (i=0; i<masterPtr->ncolors; i++) {
	char * colorDefn;		/* the color definition line */
	char * colorName;		/* temp place to hold the color name
					 * defined for one type of visual */
	char * useName;			/* the color name used for this
					 * color. If there are many names
					 * defined, choose the name that is
					 * "best" for the target visual
					 */
	int found;

	colorDefn = masterPtr->data[i+lOffset]+masterPtr->cpp;
	colorName = (char*)ckalloc(strlen(colorDefn));
	useName   = (char*)ckalloc(strlen(colorDefn));
	found     = 0;

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
	if (masterPtr->cpp == 1) {
	    colors[i].c = masterPtr->data[i+lOffset][0];
	} else {
	    strncpy(colors[i].cstring, masterPtr->data[i+lOffset],
		(size_t)masterPtr->cpp);
	}

	if (found) {
	    if (strncasecmp(useName, "none", 5) != 0) {
		colors[i].colorPtr = Tk_GetColor(interp,
		    instancePtr->tkwin, Tk_GetUid(useName));
		if (colors[i].colorPtr == NULL) {
		    colors[i].colorPtr = Tk_GetColor(interp,
			instancePtr->tkwin, Tk_GetUid("black"));
		}
	    }
	} else {
	    colors[i].colorPtr = Tk_GetColor(interp,
		instancePtr->tkwin, Tk_GetUid("black"));
	}

	ckfree(colorName);
	ckfree(useName);
    }

    lOffset += masterPtr->ncolors;

    /*
     * Parse the main body of the image
     */
    for (i=0; i<masterPtr->size[1]; i++) {
	char * p = masterPtr->data[i+lOffset];

	for (j=0; j<masterPtr->size[0]; j++) {
	    if (masterPtr->cpp == 1) {
		for (k=0; k<masterPtr->ncolors; k++) {
		    if (*p == colors[k].c) {
			TkimgXpmSetPixel(instancePtr, image, mask, j, i,
			        colors[k].colorPtr, &isTransp);
			break;
		    }
		}
		if (*p) {
		    p++;
		}
	    } else {
		for (k=0; k<masterPtr->ncolors; k++) {
		    if (strncmp(p, colors[k].cstring,
			    (size_t)masterPtr->cpp) == 0) {
			TkimgXpmSetPixel(instancePtr, image, mask, j, i,
			        colors[k].colorPtr, &isTransp);
			break;
		    }
		}
		for (k=0; *p && k<masterPtr->cpp; k++) {
		    p++;
		}
	    }
	}
    }

    instancePtr->colors = colors;

    TkimgXpmRealizePixmap(masterPtr, instancePtr, image, mask, isTransp);
    TkimgXpmFreeTmpBuffer(masterPtr, instancePtr, image, mask);
}

/*
 *----------------------------------------------------------------------
 *
 * TkimgXpmConfigureInstance --
 *
 *	This procedure is called to create displaying information for
 *	a pixmap image instance based on the configuration information
 *	in the master.  It is invoked both when new instances are
 *	created and when the master is reconfigured.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	Generates errors via Tk_BackgroundError if there are problems
 *	in setting up the instance.
 *
 *----------------------------------------------------------------------
 */

static void
TkimgXpmConfigureInstance(instancePtr)
    PixmapInstance *instancePtr;	/* Instance to reconfigure. */
{
    PixmapMaster *masterPtr = instancePtr->masterPtr;

    if (instancePtr->pixmap != None) {
	Tk_FreePixmap(Tk_Display(instancePtr->tkwin), instancePtr->pixmap);
    }
    TkimgXpmFreeInstanceData(instancePtr, 0);

    if (instancePtr->colors != NULL) {
	int i;
	for (i=0; i<masterPtr->ncolors; i++) {
	    if (instancePtr->colors[i].colorPtr != NULL) {
		Tk_FreeColor(instancePtr->colors[i].colorPtr);
	    }
	    if (masterPtr->cpp != 1) {
		ckfree(instancePtr->colors[i].cstring);
	    }
	}
	ckfree((char*)instancePtr->colors);
    }

    if (Tk_WindowId(instancePtr->tkwin) == None) {
	Tk_MakeWindowExist(instancePtr->tkwin);
    }

    /*
     * Assumption: masterPtr->data is always non NULL (enfored by
     * TkimgXpmConfigureMaster()). Also, the data must be in a valid
     * format (partially enforced by TkimgXpmConfigureMaster(), see comments
     * inside that function).
     */
    TkimgXpmGetPixmapFromData(masterPtr->interp, masterPtr, instancePtr);
}

/*
 *--------------------------------------------------------------
 *
 * TkimgXpmCmd --
 *
 *	This procedure is invoked to process the Tcl command
 *	that corresponds to an image managed by this module.
 *	See the user documentation for details on what it does.
 *
 * Results:
 *	A standard Tcl result.
 *
 * Side effects:
 *	See the user documentation.
 *
 *--------------------------------------------------------------
 */

static int
TkimgXpmCmd(clientData, interp, argc, argv)
    ClientData clientData;	/* Information about button widget. */
    Tcl_Interp *interp;		/* Current interpreter. */
    int argc;			/* Number of arguments. */
    CONST84 char **argv;	/* Argument strings. */
{
    PixmapMaster *masterPtr = (PixmapMaster *) clientData;
    int c, code;
    size_t length;

    if (argc < 2) {
	Tcl_AppendResult(interp, "wrong # args: should be \"",
		argv[0], " option ?arg arg ...?\"",
		(char *) NULL);
	return TCL_ERROR;
    }
    c = argv[1][0];
    length = strlen(argv[1]);

    if ((c == 'c') && (strncmp(argv[1], "cget", length) == 0)
	    && (length >= 2)) {
	if (argc != 3) {
	    Tcl_AppendResult(interp, "wrong # args: should be \"",
		    argv[0], " cget option\"",
		    (char *) NULL);
	    return TCL_ERROR;
	}
	return Tk_ConfigureValue(interp, Tk_MainWindow(interp), configSpecs,
		(char *) masterPtr, argv[2], 0);
    } else if ((c == 'c') && (strncmp(argv[1], "configure", length) == 0)
	    && (length >= 2)) {
	if (argc == 2) {
	    code = Tk_ConfigureInfo(interp, Tk_MainWindow(interp),
		    configSpecs, (char *) masterPtr, (char *) NULL, 0);
	} else if (argc == 3) {
	    code = Tk_ConfigureInfo(interp, Tk_MainWindow(interp),
		    configSpecs, (char *) masterPtr, argv[2], 0);
	} else {
	    code = TkimgXpmConfigureMaster(masterPtr, argc-2, argv+2,
		    TK_CONFIG_ARGV_ONLY);
	}
	return code;
    } else if ((c == 'r') && (strncmp(argv[1], "refcount", length) == 0)) {
	/*
	 * The "refcount" command is for debugging only
	 */
	PixmapInstance *instancePtr;
	int count = 0;
	char buff[30];

	if (argc != 1) {
	    Tcl_AppendResult(interp, "wrong # args: should be \"",
		    argv[0], "\"", (char *) NULL);
	    return TCL_ERROR;
	}
	for (instancePtr=masterPtr->instancePtr; instancePtr;
	     instancePtr = instancePtr->nextPtr) {
	    count += instancePtr->refCount;
	}
	sprintf(buff, "%d", count);
	Tcl_AppendResult(interp, buff, (char *) NULL);
	return TCL_OK;
    } else {
	Tcl_AppendResult(interp, "bad option \"", argv[1],
	    "\": must be cget, configure or refcount", (char *) NULL);
	return TCL_ERROR;
    }
}

/*
 *----------------------------------------------------------------------
 *
 * TkimgXpmGet --
 *
 *	This procedure is called for each use of a pixmap image in a
 *	widget.
 *
 * Results:
 *	The return value is a token for the instance, which is passed
 *	back to us in calls to TkimgXpmDisplay and TkimgXpmFre.
 *
 * Side effects:
 *	A data structure is set up for the instance (or, an existing
 *	instance is re-used for the new one).
 *
 *----------------------------------------------------------------------
 */

static ClientData
TkimgXpmGet(tkwin, masterData)
    Tk_Window tkwin;		/* Window in which the instance will be
				 * used. */
    ClientData masterData;	/* Pointer to our master structure for the
				 * image. */
{
    PixmapMaster *masterPtr = (PixmapMaster *) masterData;
    PixmapInstance *instancePtr;

    /*
     * See if there is already an instance for this window.  If so
     * then just re-use it.
     */

    for (instancePtr = masterPtr->instancePtr; instancePtr != NULL;
	    instancePtr = instancePtr->nextPtr) {
	if (instancePtr->tkwin == tkwin) {
	    instancePtr->refCount++;
	    return (ClientData) instancePtr;
	}
    }

    /*
     * The image isn't already in use in this window.  Make a new
     * instance of the image.
     */
    instancePtr = (PixmapInstance *) ckalloc(sizeof(PixmapInstance));
    instancePtr->refCount = 1;
    instancePtr->masterPtr = masterPtr;
    instancePtr->tkwin = tkwin;
    instancePtr->pixmap = None;
    instancePtr->nextPtr = masterPtr->instancePtr;
    instancePtr->colors = NULL;
    masterPtr->instancePtr = instancePtr;

    TkimgInitPixmapInstance(masterPtr, instancePtr);
    TkimgXpmConfigureInstance(instancePtr);

    /*
     * If this is the first instance, must set the size of the image.
     */
    if (instancePtr->nextPtr == NULL) {
	if (masterPtr->data) {
	    Tk_ImageChanged(masterPtr->tkMaster, 0, 0,
	        masterPtr->size[0], masterPtr->size[1],
	        masterPtr->size[0], masterPtr->size[1]);
	} else {
	    Tk_ImageChanged(masterPtr->tkMaster, 0, 0, 0, 0, 0, 0);
	}
    }

    return (ClientData) instancePtr;
}

/*
 *----------------------------------------------------------------------
 *
 * TkimgXpmDisplay --
 *
 *	This procedure is invoked to draw a pixmap image.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	A portion of the image gets rendered in a pixmap or window.
 *
 *----------------------------------------------------------------------
 */

static void
TkimgXpmDisplay(clientData, display, drawable, imageX, imageY, width,
	height, drawableX, drawableY)
    ClientData clientData;	/* Pointer to PixmapInstance structure for
				 * for instance to be displayed. */
    Display *display;		/* Display on which to draw image. */
    Drawable drawable;		/* Pixmap or window in which to draw image. */
    int imageX, imageY;		/* Upper-left corner of region within image
				 * to draw. */
    int width, height;		/* Dimensions of region within image to draw.*/
    int drawableX, drawableY;	/* Coordinates within drawable that
				 * correspond to imageX and imageY. */
{
    TkimgpXpmDisplay(clientData, display, drawable, imageX, imageY, width,
	height, drawableX, drawableY);
}

/*
 *----------------------------------------------------------------------
 *
 * TkimgXpmFree --
 *
 *	This procedure is called when a widget ceases to use a
 *	particular instance of an image.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	Internal data structures get cleaned up.
 *
 *----------------------------------------------------------------------
 */

static void
TkimgXpmFree(clientData, display)
    ClientData clientData;	/* Pointer to PixmapInstance structure for
				 * for instance to be displayed. */
    Display *display;		/* Display containing window that used image.*/
{
    PixmapInstance *instancePtr = (PixmapInstance *) clientData;
    PixmapInstance *prevPtr;

    instancePtr->refCount--;
    if (instancePtr->refCount > 0) {
	return;
    }

    /*
     * There are no more uses of the image within this widget.  Free
     * the instance structure.
     */
    if (instancePtr->pixmap != None) {
	Tk_FreePixmap(display, instancePtr->pixmap);
    }
    TkimgXpmFreeInstanceData(instancePtr, 1);

    if (instancePtr->colors != NULL) {
	int i;
	for (i=0; i<instancePtr->masterPtr->ncolors; i++) {
	    if (instancePtr->colors[i].colorPtr != NULL) {
		Tk_FreeColor(instancePtr->colors[i].colorPtr);
	    }
	    if (instancePtr->masterPtr->cpp != 1) {
		ckfree(instancePtr->colors[i].cstring);
	    }
	}
	ckfree((char*)instancePtr->colors);
    }

    if (instancePtr->masterPtr->instancePtr == instancePtr) {
	instancePtr->masterPtr->instancePtr = instancePtr->nextPtr;
    } else {
	for (prevPtr = instancePtr->masterPtr->instancePtr;
		prevPtr->nextPtr != instancePtr; prevPtr = prevPtr->nextPtr) {
	    /* Empty loop body */
	}
	prevPtr->nextPtr = instancePtr->nextPtr;
    }
    ckfree((char *) instancePtr);
}

/*
 *----------------------------------------------------------------------
 *
 * TkimgXpmDelete --
 *
 *	This procedure is called by the image code to delete the
 *	master structure for an image.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	Resources associated with the image get freed.
 *
 *----------------------------------------------------------------------
 */

static void
TkimgXpmDelete(masterData)
    ClientData masterData;	/* Pointer to PixmapMaster structure for
				 * image.  Must not have any more instances. */
{
    PixmapMaster *masterPtr = (PixmapMaster *) masterData;

    if (masterPtr->instancePtr != NULL) {
	panic("tried to delete pixmap image when instances still exist");
    }
    masterPtr->tkMaster = NULL;
    if (masterPtr->imageCmd != NULL) {
	Tcl_DeleteCommand(masterPtr->interp,
		Tcl_GetCommandName(masterPtr->interp, masterPtr->imageCmd));
    }
    if (masterPtr->isDataAlloced && masterPtr->data != NULL) {
	ckfree((char*)masterPtr->data);
	masterPtr->data = NULL;
    }

    Tk_FreeOptions(configSpecs, (char *) masterPtr, (Display *) NULL, 0);
    ckfree((char *) masterPtr);
}

/*
 *----------------------------------------------------------------------
 *
 * TkimgXpmCmdDeletedProc --
 *
 *	This procedure is invoked when the image command for an image
 *	is deleted.  It deletes the image.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	The image is deleted.
 *
 *----------------------------------------------------------------------
 */

static void
TkimgXpmCmdDeletedProc(clientData)
    ClientData clientData;	/* Pointer to PixmapMaster structure for
				 * image. */
{
    PixmapMaster *masterPtr = (PixmapMaster *) clientData;

    masterPtr->imageCmd = NULL;
    if (masterPtr->tkMaster != NULL) {
	Tk_DeleteImage(masterPtr->interp, Tk_NameOfImage(masterPtr->tkMaster));
    }
}

/*
 * Package management. Initialization of stub information.
 */

/*
 *----------------------------------------------------------------------------
 *
 * Tkimgpixmap_Init --
 *
 *  Initialisation routine for loadable module
 *
 * Results:
 *  None.
 *
 * Side effects:
 *  Creates commands in the interpreter, loads package.
 *
 *----------------------------------------------------------------------------
 */

int
Tkimgpixmap_Init (interp)
      Tcl_Interp *interp; /* Interpreter to initialise. */
{
  static int initialized = 0;

#ifdef USE_TCL_STUBS
    if (Tcl_InitStubs(interp, "8.1", 0) == NULL) {
        return TCL_ERROR;
    }
#endif
#ifdef USE_TK_STUBS
    if (Tk_InitStubs(interp, "8.1", 0) == NULL) {
        return TCL_ERROR;
    }
#endif
#ifdef USE_TKIMG_STUBS
    if (Tkimg_InitStubs(interp, TKIMG_VERSION, 0) == NULL) {
        return TCL_ERROR;
    }
#endif

#ifndef TCL_MAC
    if (!initialized) {
	Tk_CreateImageType(&imgPixmapImageType);
	initialized = 1;
    }
#endif

    /*
     * At last provide the package ...
     */

    if (Tcl_PkgProvide(interp, PACKAGE_TCLNAME, PACKAGE_VERSION) != TCL_OK) {
        return TCL_ERROR;
    }
    return TCL_OK;
}

/*
 *----------------------------------------------------------------------------
 *
 * Tkimgpixmap_SafeInit --
 *
 *  Initialisation routine for loadable module in a safe interpreter.
 *
 * Results:
 *  None.
 *
 * Side effects:
 *  Creates commands in the interpreter,
 *  loads xml package.
 *
 *----------------------------------------------------------------------------
 */

int
Tkimgpixmap_SafeInit (interp)
      Tcl_Interp *interp; /* Interpreter to initialise. */
{
    return Tkimgpixmap_Init (interp);
}
