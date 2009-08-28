/*
 * cmpRead.c --
 *
 *  This file contains the code used by the compiled file script loader to
 *  load a compiled script. The script loader is registered in the Init
 *  procedure of the "Loader" package.
 *
 * Copyright (c) 1998-2000 Ajuba Solutions
 *
 * See the file "license.terms" for information on usage and redistribution
 * of this file, and for a DISCLAIMER OF ALL WARRANTIES.
 *
 * RCS: @(#) $Id: cmpRead.c,v 1.11 2002/11/28 16:53:10 andreas_kupries Exp $
 */

#include "cmpInt.h"
#include "proTbcLoad.h"

/*
 * This structure contains signature information from a compiled file.
 */
typedef struct ImageSignature {
    int formatNumber;		/* version number of the .tbc file format */
    int buildNumber;		/* build number of the compiler that
				 * created the .tbc file */
    int cmpMajorVersion;	/* major version of the compiler package that
				 * generated the compiled image */
    int cmpMinorVersion;	/* minor version of the compiler package that
				 * generated the compiled image */
    int tclMajorVersion;	/* major version of the interpreter that
				 * generated the compiled image */
    int tclMinorVersion;	/* minor version of the interpreter that
				 * generated the compiled image */
} ImageSignature;

/*
 * the extraction environment contains the state of an extraction for
 * a compiled ByteCode structure.
 */
typedef struct ExtractionEnv {
    char *imageBase;		/* base of the compiled image bytes */
    char *imageEnd;		/* address immediately following the last
				 * byte in the compiled image */
    char *curImagePtr;		/* pointer to the part of the compiled image
				 * we are parsing */
    ByteCode *codePtr;		/* the ByteCode structure we are populating */
    int codeSize;		/* the size of the Bytecode structure */
    LocMapSizes locMap;		/* this structure holds the sizes of the
				 * location map arrays as extracted from the
				 * image header */
    ImageSignature sig;		/* Image signature from file */
} ExtractionEnv;

/*
 * This struct holds the decoding context for a run of ExtractByteSequence
 */

typedef struct A85DecodeContext {
    int bytesToDecode;		/* number of bytes left to decode */
    unsigned char *curPtr;	/* pointer to the next available location
				 * in the decode buffer */
    int curChar;		/* index of the next base-85 digit to be
				 * stored in the decode buffer.
				 * Can be one of 0, 1, 2, 3, 4 */
    int decodeBuf[5];		/* buffer to hold the 5-tuple we are currently
				 * collecting. */
} A85DecodeContext;

/*
 * The following data structure is used to keep track of whether an
 * arbitrary block of memory has been deleted.	This is used by the
 * TclHandle code to avoid the more time-expensive algorithm of
 * Tcl_Preserve().  This mechanism is mainly used when we have lots of
 * references to a few big, expensive objects that we don't want to live
 * any longer than necessary.  This is a copy of the structure from
 * Tcl 8.1, for backwards compatibility.
 */

typedef struct OldHandleStruct {
    VOID *ptr;			/* Pointer to the memory block being
				 * tracked.  This field will become NULL when
				 * the memory block is deleted.	 This field
				 * must be the first in the structure. */
#ifdef TCL_MEM_DEBUG
    VOID *ptr2;			/* Backup copy of the above pointer used to
				 * ensure that the contents of the handle are
				 * not changed by anyone else. */
#endif
    int refCount;		/* Number of TclHandlePreserve() calls in
				 * effect on this handle. */
} OldHandleStruct;


/*
 * The current format version number.  This will be set in
 * TbcloadInit to the correct value for the current Tcl interpreter.
 */

static int formatVersion = 0;

/*
 * The current Tcl version that we are loaded in.
 * This is needed to account for differences in stubs from 8.1 to 8.2
 * and variable flags differences from 8.3 to 8.4.
 * TbcloadInit to the correct value for the current Tcl interpreter.
 */

static int tclMajorVersion = 0, tclMinorVersion = 0;

/*
 * No source available message. The source field in ByteCode structures that
 * are extracted from compiled images is initialized with this string, and
 * will be used to generate call stacks and error info.
 * Also the string representation of procedure body objects is initialized
 * with this value.
 * Must be kept to under 128 bytes in length (because of the way the source
 * code map is automatically generated in the ByteCode structs).
 * There are two parts to the "source" for a compiled script:
 *  - a comment fo humans to look at (for example in an errorInfo trace)
 *  - a bit of code that throws an error; this is used to trigger an error
 *    in some situations; for example, if the following construct is used
 *	proc A {args} { set x 5 ; return "hello world $x" }
 *	proc B {args} [info body A]
 *    a call to B will trigger the error.
 */
static char noSourceCode[] = "# Compiled -- no source code available\n\
error \"called a copy of a compiled script\"";
static int noSourceCodeSize = sizeof(noSourceCode);

#if USE_CATCH_WRAPPER
/*
 * Marker string appended by Tbcload_EvalObjCmd to the errorInfo, for use by
 * the catch code to strip out error info that we don't want.
 */
static char errorInfoMarker[] = CMP_ERRORINFO_MARKER;
#endif

/*
 * Map between ExceptionRangeType enums and type codes.
 * This map must be kept consistent with the equivalent one in cmpWrite.c.
 */
static ExcRangeMap excRangeMap[] = {
    { LOOP_EXCEPTION_RANGE,	CMP_LOOP_EXCEPTION_RANGE },
    { CATCH_EXCEPTION_RANGE,	CMP_CATCH_EXCEPTION_RANGE },

    { 0, '\0' }
};

/*
 * The list of VAR_ flag values to check when emitting. The order is
 * is important an must be kept consistent with the equivalent list in
 * cmpWrite.c
 */

static int varFlagsList[] = {
#ifdef TCL_85_PLUS
    /*
     * For 8.5+, keep the same size for compat with 8.4 written bytecodes,
     * but ignore all but VAR_ARGUMENT and VAR_TEMPORARY.
     */
    0, 0, 0, 0, 0, 0, 0, 0,
#else
    VAR_SCALAR,
    VAR_ARRAY,
    VAR_LINK,
    VAR_UNDEFINED,
    VAR_IN_HASHTABLE,
    VAR_TRACE_ACTIVE,
    VAR_ARRAY_ELEMENT,
    VAR_NAMESPACE_VAR,
#endif
    VAR_ARGUMENT,
    VAR_TEMPORARY,
    0			/* VAR_RESOLVED is always mapped as 0 */
};
static int varFlagsListSize = sizeof(varFlagsList) / sizeof(varFlagsList[0]);

/*
 * We use a modified encoding scheme which avoids the Tcl special characters
 * $, {, }, [, ], and \.
 * Because of this, we need to use a table instead of generating the character
 * codes arithmetically.
 * The decode map maps character values in the range 0 - 127 to the
 * corresponding base-85 digit's value; characters that are not base-85
 * digits have a map value of -1 if they are considered whitespace, and -2 if
 * they are illegal characters in the sequence (they should not appear).
 * The decoder skips whitespace while decoding.
 */

#define times85(x)	((((((x<<2)+x)<<2)+x)<<2)+x)

#define A85_WHITESPACE		-1
#define A85_ILLEGAL_CHAR	-2
#define A85_Z			-3

static int decodeMap[] = {
      A85_ILLEGAL_CHAR,		/* ^@ */
      A85_ILLEGAL_CHAR,		/* ^A */
      A85_ILLEGAL_CHAR,		/* ^B */
      A85_ILLEGAL_CHAR,		/* ^C */
      A85_ILLEGAL_CHAR,		/* ^D */
      A85_ILLEGAL_CHAR,		/* ^E */
      A85_ILLEGAL_CHAR,		/* ^F */
      A85_ILLEGAL_CHAR,		/* ^G */
      A85_ILLEGAL_CHAR,		/* ^H */
      A85_WHITESPACE,		/* \t */
      A85_WHITESPACE,		/* \n */
      A85_ILLEGAL_CHAR,		/* ^K */
      A85_ILLEGAL_CHAR,		/* ^L */
      A85_ILLEGAL_CHAR,		/* ^M */
      A85_ILLEGAL_CHAR,		/* ^N */
      A85_ILLEGAL_CHAR,		/* ^O */
      A85_ILLEGAL_CHAR,		/* ^P */
      A85_ILLEGAL_CHAR,		/* ^Q */
      A85_ILLEGAL_CHAR,		/* ^R */
      A85_ILLEGAL_CHAR,		/* ^S */
      A85_ILLEGAL_CHAR,		/* ^T */
      A85_ILLEGAL_CHAR,		/* ^U */
      A85_ILLEGAL_CHAR,		/* ^V */
      A85_ILLEGAL_CHAR,		/* ^W */
      A85_ILLEGAL_CHAR,		/* ^X */
      A85_ILLEGAL_CHAR,		/* ^Y */
      A85_ILLEGAL_CHAR,		/* ^Z */
      A85_ILLEGAL_CHAR,		/* ^[ */
      A85_ILLEGAL_CHAR,		/* ^\ */
      A85_ILLEGAL_CHAR,		/* ^] */
      A85_ILLEGAL_CHAR,		/* ^^ */
      A85_ILLEGAL_CHAR,		/* ^_ */
      A85_WHITESPACE,		/*   */
       0,			/* ! */
      A85_ILLEGAL_CHAR,		/* " (for hilit: ") */
       2,			/* # */
      A85_ILLEGAL_CHAR,		/* $ */
       4,			/* % */
       5,			/* & */
       6,			/* ' */
       7,			/* ( */
       8,			/* ) */
       9,			/* * */
      10,			/* + */
      11,			/* , */
      12,			/* - */
      13,			/* . */
      14,			/* / */
      15,			/* 0 */
      16,			/* 1 */
      17,			/* 2 */
      18,			/* 3 */
      19,			/* 4 */
      20,			/* 5 */
      21,			/* 6 */
      22,			/* 7 */
      23,			/* 8 */
      24,			/* 9 */
      25,			/* : */
      26,			/* ; */
      27,			/* < */
      28,			/* = */
      29,			/* > */
      30,			/* ? */
      31,			/* @ */
      32,			/* A */
      33,			/* B */
      34,			/* C */
      35,			/* D */
      36,			/* E */
      37,			/* F */
      38,			/* G */
      39,			/* H */
      40,			/* I */
      41,			/* J */
      42,			/* K */
      43,			/* L */
      44,			/* M */
      45,			/* N */
      46,			/* O */
      47,			/* P */
      48,			/* Q */
      49,			/* R */
      50,			/* S */
      51,			/* T */
      52,			/* U */
      53,			/* V */
      54,			/* W */
      55,			/* X */
      56,			/* Y */
      57,			/* Z */
      A85_ILLEGAL_CHAR,		/* [ */
      A85_ILLEGAL_CHAR,		/* \ */
      A85_ILLEGAL_CHAR,		/* ] */
      61,			/* ^ */
      62,			/* _ */
      63,			/* ` */
      64,			/* a */
      65,			/* b */
      66,			/* c */
      67,			/* d */
      68,			/* e */
      69,			/* f */
      70,			/* g */
      71,			/* h */
      72,			/* i */
      73,			/* j */
      74,			/* k */
      75,			/* l */
      76,			/* m */
      77,			/* n */
      78,			/* o */
      79,			/* p */
      80,			/* q */
      81,			/* r */
      82,			/* s */
      83,			/* t */
      84,			/* u */
       1,			/* v (replaces ") " */
       3,			/* w (replaces $) */
      58,			/* x (replaces [) */
      59,			/* y (replaces \) */
      A85_Z,			/* z */
      A85_ILLEGAL_CHAR,		/* { */
      60,			/* | (replaces ]) */
      A85_ILLEGAL_CHAR,		/* } */
      A85_ILLEGAL_CHAR,		/* ~ */
};

/*
 * These Tcl_ObjType pointers are initialized the first time that the package
 * is loaded; we do it this way because the actual object types are not
 * exported by the TCL DLL, and therefore if we use the address of the
 * standard types we get an undefined symbol at link time.
 */

static Tcl_ObjType *cmpTclProProcBodyType = 0;
static Tcl_ObjType *cmpByteCodeType = 0;
static Tcl_ObjType *cmpDoubleType = 0;
static Tcl_ObjType *cmpIntType = 0;

/*
 * Same thing for AuxDataTypes.
 */

static AuxDataType *cmpForeachInfoType = 0;
#ifdef TCL_85_PLUS
static AuxDataType *cmpJumptableInfoType = 0;
#endif

static int didLoadTypes = 0;

/*
 * error message to generate when we run into the end of the buffer and we
 * are expecting more stuff
 */

static char prematureEnd[] = "bytecode terminated prematurely";

/*
 * Compatibility layer declarations and static storage follow.
 */

static int compatibilityLayerInit = 0;

/*
 * This procedure is used internally to look up symbols in the current
 * executable, to initialize the compatibility layer properly
 */

extern VOID *	TbcloadGetSymbolAddress _ANSI_ARGS_((CONST char* symbolName));

/*
 * The factory for procbody objects.
 */

typedef Tcl_Obj * (ProcBodyFactory) _ANSI_ARGS_((Proc *procPtr));
static ProcBodyFactory *procBodyFactory = 0;

/*
 * The cleanup proc for procbody objects.
 */

typedef void (ProcBodyCleanup) _ANSI_ARGS_((Proc *procPtr));
static ProcBodyCleanup *procBodyCleanup = 0;

/*
 *
 * The real tbcload::bcproc command implementation. This is either the old
 * (1.0) implementation if the interpreter is 8.0.3, or Tcl_ProcObjCmd if
 * the interpreter is 8.0.4 or later.
 */

static Tcl_ObjCmdProc *bcprocCmdProc = NULL;

/*
 * Prototypes for procedures defined later in this file:
 */

static int	A85DecodeByte _ANSI_ARGS_((Tcl_Interp *interp, int code,
			A85DecodeContext *ctxPtr));
static void	A85InitDecodeContext _ANSI_ARGS_((int numBytes,
			unsigned char *decodeBuf, A85DecodeContext *ctxPtr));
static int	AllocAndExtractByteSequence _ANSI_ARGS_((Tcl_Interp *interp,
			ExtractionEnv *envPtr, int addNull,
			unsigned char **seqPtrPtr, int *seqSizePtr));
static int	AllocAndExtractString _ANSI_ARGS_((Tcl_Interp *interp,
			ExtractionEnv *envPtr, int addNull,
			char **stringPtrPtr, int *stringSizePtr));
static void	AppendErrorLocation _ANSI_ARGS_((Tcl_Interp *interp,
			ExtractionEnv* envPtr));
static void	CleanupByteCode _ANSI_ARGS_((ExtractionEnv* envPtr));
static void	CleanupExtractEnv _ANSI_ARGS_((ExtractionEnv *envPtr));
static Tcl_Obj *CreateSimpleObject _ANSI_ARGS_((ExtractionEnv *envPtr));
static int	ExcRangeFromName _ANSI_ARGS_((int name,
			ExceptionRangeType* typePtr));
static int	ExtractAuxDataArray _ANSI_ARGS_((Tcl_Interp *interp,
			int numAuxDataItems, ExtractionEnv* envPtr,
			AuxData *auxDataArray, int auxDataArraySize));
static int	ExtractByteCode _ANSI_ARGS_((Tcl_Interp *interp,
			ExtractionEnv* envPtr));
static int	ExtractByteSequence _ANSI_ARGS_((Tcl_Interp *interp,
			int length, ExtractionEnv *envPtr,
			unsigned char *seqPtr, int seqSize));
static Tcl_Obj*	ExtractCompiledFile _ANSI_ARGS_((Tcl_Interp *interp,
			char* codeBuffer, int codeLength));
static CompiledLocal *
		ExtractCompiledLocal _ANSI_ARGS_((Tcl_Interp *interp,
			ExtractionEnv *envPtr));
static int	ExtractExcRangeArray _ANSI_ARGS_((Tcl_Interp *interp,
			int numExceptRanges, ExtractionEnv* envPtr,
			ExceptionRange *excArray, int excArraySize));
static int	ExtractForeachInfo _ANSI_ARGS_((Tcl_Interp *interp,
			ExtractionEnv *envPtr, AuxData *auxDataPtr));
#ifdef TCL_85_PLUS
static int	ExtractJumptableInfo _ANSI_ARGS_((Tcl_Interp *interp,
			ExtractionEnv *envPtr, AuxData *auxDataPtr));
#endif
static int	ExtractInteger _ANSI_ARGS_((Tcl_Interp *interp,
			ExtractionEnv *envPtr, int *valuePtr));
static int	ExtractObjArray _ANSI_ARGS_((Tcl_Interp *interp,
			int numLitObjects, ExtractionEnv* envPtr,
			Tcl_Obj **objArray, int objArraySize));
static Tcl_Obj *ExtractObject _ANSI_ARGS_((Tcl_Interp *interp,
			ExtractionEnv *envPtr));
static Tcl_Obj *ExtractProcBody _ANSI_ARGS_((Tcl_Interp *interp,
			ByteCode *codeptr, ExtractionEnv *envPtr));
static char*	ExtractSignature _ANSI_ARGS_((Tcl_Interp *interp,
			char *codePtr, char *codeEnd,
			ImageSignature *signaturePtr));
static int	ExtractString _ANSI_ARGS_((Tcl_Interp *interp,
			int length, ExtractionEnv *envPtr,
			char *strPtr, int strSize));
static char *	FindEnd _ANSI_ARGS_((char *first, char *last));
static int	CheckSignature _ANSI_ARGS_((Tcl_Interp *interp,
			ImageSignature *signaturePtr));
static int	InitByteCode _ANSI_ARGS_((Tcl_Interp *interp,
			ExtractionEnv* envPtr));
static int	InitCompatibilityLayer _ANSI_ARGS_((Tcl_Interp *interp));
static void	InitExtractEnv _ANSI_ARGS_((char *codeBase, char *codeEnd,
			ExtractionEnv *envPtr));
static void	InitTypes _ANSI_ARGS_(());
static TclHandle OldHandlePreserve _ANSI_ARGS_((TclHandle handle));


/*
 *----------------------------------------------------------------------
 *
 * Tbcload_EvalObjCmd --
 *
 *  Eval the Tcl_Obj given as its first argument. If this Tcl_Obj contains
 *  the string representation of a compiled script, it builds a ByteCode
 *  from it, and executes it.
 *
 * Results:
 *  Returns a standard TCL result code.
 *
 * Side effects:
 *  None.
 *
 *----------------------------------------------------------------------
 */

int
Tbcload_EvalObjCmd(dummy, interp, objc, objv)
    ClientData dummy;		/* Not used. */
    Tcl_Interp *interp;		/* Current interpreter. */
    int objc;			/* Number of arguments. */
    Tcl_Obj *CONST objv[];	/* Argument objects. */
{
    ImageSignature sig;
    Tcl_Obj *cmdObjPtr;
    char *scriptPtr;
    int scriptLength;
    int result;

    if (objc < 2) {
	Tcl_WrongNumArgs(interp, 1, objv, "bytestream");
	return TCL_ERROR;
    }

    Tcl_ResetResult(interp);

    /*
     * Check the signature. If OK, then extract the whole file.
     * If the script is empty, return TCL_OK here.
     */

    scriptPtr = objv[1]->bytes;
    scriptLength = objv[1]->length;

    if ((scriptLength < 1) || (scriptPtr == NULL)) {
	return TCL_OK;
    }

    if ((ExtractSignature(interp, scriptPtr, (scriptPtr + scriptLength),
	    &sig) == NULL) || (CheckSignature(interp, &sig) != TCL_OK)) {
	return TCL_ERROR;
    }

    cmdObjPtr = ExtractCompiledFile(interp, scriptPtr, scriptLength);
    if (cmdObjPtr == NULL) {
	return TCL_ERROR;
    }

    result = Tcl_EvalObj(interp, cmdObjPtr);
    Tcl_DecrRefCount(cmdObjPtr);

#if USE_CATCH_WRAPPER
    if (result == TCL_ERROR) {
	Tcl_AddObjErrorInfo(interp, "\n", -1);
	Tcl_AddObjErrorInfo(interp, errorInfoMarker, -1);
    }
#endif

    return result;
}

/*
 *----------------------------------------------------------------------
 *
 * Tbcload_ProcObjCmd --
 *
 *  Wrapper procedure, dispatches the call to the appropriate implementation.
 *
 * Results:
 *  A standard Tcl object result value.
 *
 * Side effects:
 *  A new procedure gets created.
 *
 *----------------------------------------------------------------------
 */

int
Tbcload_ProcObjCmd(dummy, interp, objc, objv)
    ClientData dummy;		/* Not used. */
    Tcl_Interp *interp;		/* Current interpreter. */
    int objc;			/* Number of arguments. */
    Tcl_Obj *CONST objv[];	/* Argument objects. */
{
    return (*bcprocCmdProc)(dummy, interp, objc, objv);
}

/*
 *----------------------------------------------------------------------
 *
 * ExtractByteCode --
 *
 *  Extracts the contents of a ByteCode structure from a compiled image.
 *
 * Results:
 *  Returns TCL_OK on success, TCL_ERROR on failure.
 *
 * Side effects:
 *  Populates the ByteCode with the value extracted from the file.
 *  Updates the contents of the ExtractionEnv structure: stores the new
 *  address of the next available byte.
 *  May also modify the interpreter's result in case of failure.
 *
 *----------------------------------------------------------------------
 */

static int
ExtractByteCode(interp, envPtr)
    Tcl_Interp *interp;			/* the current interpreter */
    ExtractionEnv *envPtr;		/* the extraction environment */
{
    ByteCode *codePtr;
    LocMapSizes *locMapPtr = &envPtr->locMap;

    if (InitByteCode(interp, envPtr) != TCL_OK) {
	goto error;
    }

    codePtr = envPtr->codePtr;

    if ((ExtractByteSequence(interp, codePtr->numCodeBytes, envPtr,
	    codePtr->codeStart, codePtr->numCodeBytes) != TCL_OK)
	    || (ExtractByteSequence(interp, locMapPtr->codeDeltaSize, envPtr,
		    codePtr->codeDeltaStart,
		    locMapPtr->codeDeltaSize) != TCL_OK)
	    || (ExtractByteSequence(interp, locMapPtr->codeLengthSize, envPtr,
		    codePtr->codeLengthStart,
		    locMapPtr->codeLengthSize) != TCL_OK)) {
	goto error;
    }

    if ((locMapPtr->srcDeltaSize >= 0)
	    && (ExtractByteSequence(interp, locMapPtr->srcDeltaSize, envPtr,
		    codePtr->srcDeltaStart,
		    locMapPtr->srcDeltaSize) != TCL_OK)) {
	goto error;
    }

    if ((locMapPtr->srcLengthSize >= 0)
	    && (ExtractByteSequence(interp, locMapPtr->srcLengthSize, envPtr,
		    codePtr->srcLengthStart,
		    locMapPtr->srcLengthSize) != TCL_OK)) {
	goto error;
    }

    if ((ExtractObjArray(interp, codePtr->numLitObjects, envPtr,
		codePtr->objArrayPtr, codePtr->numLitObjects) != TCL_OK)
	    || (ExtractExcRangeArray(interp, codePtr->numExceptRanges, envPtr,
		    codePtr->exceptArrayPtr,
		    codePtr->numExceptRanges) != TCL_OK)
	    || (ExtractAuxDataArray(interp, codePtr->numAuxDataItems, envPtr,
		    codePtr->auxDataArrayPtr,
		    codePtr->numAuxDataItems) != TCL_OK)) {
	goto error;
    }

    /*
     * If the source map arrays were not included in the .tbc file (which is
     * typically the case), generate them here; each command will start at 0
     * and have length noSourceCodeSize
     */

    if (locMapPtr->srcDeltaSize < 0) {
	memset(codePtr->srcDeltaStart, 0, (size_t) codePtr->numCommands);
    }

    if (locMapPtr->srcLengthSize < 0) {
	memset(codePtr->srcLengthStart, (char) noSourceCodeSize,
		(size_t) codePtr->numCommands);
    }

    return TCL_OK;

    error:
    CleanupByteCode(envPtr);
    return TCL_ERROR;
}

/*
 *----------------------------------------------------------------------
 *
 * InitByteCode --
 *
 *  Allocates and initializes a ByteCode structure using the values stored
 *  in a compiled image.
 *
 * Results:
 *  Returns TCL_OK on success, TCL_ERROR on failure.
 *
 * Side effects:
 *  Populates parts of the ByteCode with the value extracted from the file;
 *  also sets it up so that CleanupByteCode can clean it up even if the
 *  structure was only partially built.
 *  Updates the contents of the ExtractionEnv structure: stores the new
 *  address of the next available byte and the pointer to the newly allocated
 *  ByteCode struct.
 *  May also modify the interpreter's result in case of failure.
 *
 *----------------------------------------------------------------------
 */

static int
InitByteCode(interp, envPtr)
    Tcl_Interp *interp;			/* the current interpreter */
    ExtractionEnv *envPtr;		/* the extraction environment */
{
    Interp *iPtr = (Interp *) interp;
    ByteCode *byteCodePtr;
    unsigned char *p;
    int size;
    int numCommands, numSrcBytes, numCodeBytes, numLitObjects, numExceptRanges;
    int numAuxDataItems, numCmdLocBytes, maxExceptDepth, maxStackDepth;
    int objArrayBytes, exceptArrayBytes, auxDataArrayBytes;
    LocMapSizes *locMapPtr = &envPtr->locMap;
    Namespace *namespacePtr;

    CleanupByteCode(envPtr);

    /*
     * Determine the size of the ByteCode struct, malloc it, then initialize
     * the components that were not saved with the file image because it
     * doesn't make sense to do so: pointer to the interpreter, epoch, etc...
     * To determine the size of the ByteCode struct, we read in the .tbc
     * control line, which has enough enformation to calculate the required
     * size.
     */

    if ((ExtractInteger(interp, envPtr, &numCommands) != TCL_OK)
	    || (ExtractInteger(interp, envPtr, &numSrcBytes) != TCL_OK)
	    || (ExtractInteger(interp, envPtr, &numCodeBytes) != TCL_OK)
	    || (ExtractInteger(interp, envPtr, &numLitObjects) != TCL_OK)
	    || (ExtractInteger(interp, envPtr, &numExceptRanges) != TCL_OK)
	    || (ExtractInteger(interp, envPtr, &numAuxDataItems) != TCL_OK)
	    || (ExtractInteger(interp, envPtr, &numCmdLocBytes) != TCL_OK)
	    || (ExtractInteger(interp, envPtr, &maxExceptDepth) != TCL_OK)
	    || (ExtractInteger(interp, envPtr, &maxStackDepth) != TCL_OK)
	    || (ExtractInteger(interp, envPtr,
		    &(locMapPtr->codeDeltaSize)) != TCL_OK)
	    || (ExtractInteger(interp, envPtr,
		    &(locMapPtr->codeLengthSize)) != TCL_OK)
	    || (ExtractInteger(interp, envPtr,
		    &(locMapPtr->srcDeltaSize)) != TCL_OK)
	    || (ExtractInteger(interp, envPtr,
		    &(locMapPtr->srcLengthSize)) != TCL_OK)) {
	return TCL_ERROR;
    }

    objArrayBytes = (numLitObjects * sizeof(Tcl_Obj *));
    exceptArrayBytes = (numExceptRanges * sizeof(ExceptionRange));
    auxDataArrayBytes = (numAuxDataItems * sizeof(AuxData));

    /*
     * generate the numCmdLocBytes from the values of the location and
     * source arrays, which could be different here from the original,
     * because the .tbc file was written without sources
     */

    numCmdLocBytes = locMapPtr->codeDeltaSize + locMapPtr->codeLengthSize;
    if (locMapPtr->srcDeltaSize < 0) {
	/*
	 * The source arrays have as many entries as the number of commands,
	 * because both start (0 for all) and delta (noSourceCodeSize for all)
	 * values fit in one byte
	 */

	numCmdLocBytes += numCommands;
    } else {
	numCmdLocBytes += locMapPtr->srcDeltaSize;
    }
    if (locMapPtr->srcLengthSize < 0) {
	numCmdLocBytes += numCommands;
    } else {
	numCmdLocBytes += locMapPtr->srcLengthSize;
    }

    size = sizeof(ByteCode);
    size += TCL_ALIGN(numCodeBytes);	  /* align object array */
    size += TCL_ALIGN(objArrayBytes);	  /* align exception range array */
    size += TCL_ALIGN(exceptArrayBytes);  /* align AuxData array */
    size += auxDataArrayBytes;
    size += numCmdLocBytes;

    /*
     * initialize the reference count on the ByteCode to 1 because we have a
     * reference to it in the extraction environment struct.
     */

    if (iPtr->varFramePtr != NULL) {
	namespacePtr = iPtr->varFramePtr->nsPtr;
    } else {
	namespacePtr = iPtr->globalNsPtr;
    }

    p = (unsigned char *) ckalloc((size_t) size);
    byteCodePtr = (ByteCode *) p;
    memset(byteCodePtr, 0, (size_t) size);
#ifdef tcl8_0
    byteCodePtr->iPtr = iPtr;
#else
    if ((tclMajorVersion >= 8) && (tclMinorVersion == 1)) {
	/*
	 * Need to use backwards compatibility, since
	 * TclHandlePreserve was added to the stubs table
	 * in 8.2b1.
	 */

	byteCodePtr->interpHandle = OldHandlePreserve(iPtr->handle);
    } else {
	byteCodePtr->interpHandle = TclHandlePreserve(iPtr->handle);
    }
#endif
    byteCodePtr->compileEpoch = iPtr->compileEpoch;
    byteCodePtr->nsPtr = namespacePtr;
    byteCodePtr->nsEpoch = namespacePtr->resolverEpoch;
    byteCodePtr->refCount = 1;
    byteCodePtr->flags = TCL_BYTECODE_PRECOMPILED;
    byteCodePtr->procPtr = NULL;

    envPtr->codeSize = size;
    envPtr->codePtr = byteCodePtr;

    byteCodePtr->structureSize = 0;
    byteCodePtr->numCommands = numCommands;
    byteCodePtr->numSrcBytes = numSrcBytes;
    byteCodePtr->numCodeBytes = numCodeBytes;
    byteCodePtr->numLitObjects = numLitObjects;
    byteCodePtr->numExceptRanges = numExceptRanges;
    byteCodePtr->numAuxDataItems = numAuxDataItems;
    byteCodePtr->numCmdLocBytes = numCmdLocBytes;
    byteCodePtr->maxExceptDepth = maxExceptDepth;
    byteCodePtr->maxStackDepth = maxStackDepth;
    byteCodePtr->source = noSourceCode;
    byteCodePtr->numSrcBytes = noSourceCodeSize;

    /*
     * The assignements to p must be kept consistent with the ones in
     * TclInitByteCodeObj, so that the arrays are aligned as expected.
     */

    p += sizeof(ByteCode);
    byteCodePtr->codeStart = p;

    p += TCL_ALIGN(numCodeBytes);
    if (numLitObjects > 0) {
	byteCodePtr->objArrayPtr = (Tcl_Obj **) p;
    }

    p += TCL_ALIGN(objArrayBytes);
    if (numExceptRanges > 0) {
	byteCodePtr->exceptArrayPtr = (ExceptionRange *) p;
    }

    p += TCL_ALIGN(exceptArrayBytes);
    if (numAuxDataItems > 0) {
	byteCodePtr->auxDataArrayPtr = (AuxData *) p;
    }

    p += auxDataArrayBytes;
    byteCodePtr->codeDeltaStart = p;
    p += locMapPtr->codeDeltaSize;
    byteCodePtr->codeLengthStart = p;
    p += locMapPtr->codeLengthSize;
    byteCodePtr->srcDeltaStart = p;
    if (locMapPtr->srcDeltaSize < 0) {
	p += byteCodePtr->numCommands;
    } else {
	p += locMapPtr->srcDeltaSize;
    }
    byteCodePtr->srcLengthStart = p;

    return TCL_OK;
}

/*
 *----------------------------------------------------------------------
 *
 * CleanupByteCode --
 *
 *  Cleans up and frees the ByteCode structure in the ExtractionEnv.
 *  Generally this will only be called in the case when an error
 *  occurs extracting the ByteCode, while the ByteCodes will generally
 *  get cleaned up by tclCompile.c:TclCleanupByteCode, which has a
 *  special case for precompiled flagged ByteCode structures.
 *
 * Results:
 *  None.
 *
 * Side effects:
 *  Frees a number of arrays and TCL objects that may have been allocated
 *  while the ByteCode was assembled from the compiled image..
 *
 *----------------------------------------------------------------------
 */

static void
CleanupByteCode(envPtr)
    ExtractionEnv *envPtr;	/* the structure to clean up */
{
    ByteCode *byteCodePtr = envPtr->codePtr;

    if (byteCodePtr) {
	byteCodePtr->refCount--;
	if (byteCodePtr->refCount < 1) {
	    if (byteCodePtr->numLitObjects > 0) {
		Tcl_Obj **objArrayPtr = byteCodePtr->objArrayPtr;
		Tcl_Obj *objPtr;
		int i;

		for (i = 0; i < byteCodePtr->numLitObjects; i++) {
		    objPtr = *objArrayPtr;
		    objArrayPtr += 1;
		    if (objPtr) {
			Tcl_DecrRefCount(objPtr);
		    }
		}
	    }

	    ckfree((char *) byteCodePtr);
	}
	envPtr->codePtr = NULL;
    }
}

/*
 *----------------------------------------------------------------------
 *
 * ExtractInteger --
 *
 *  Extracts an integer value from a compiled image. Skips whitespace, then
 *  gets the int from the next word. If successful, places the value in the
 *  location pointed to by valuePtr.
 *
 * Results:
 *  Returns TCL_OK on success, TCL_ERROR on failure.
 *
 * Side effects:
 *  Loads the int at valuePtr with the value extracted from the file.
 *  Updates the contents of the ExtractionEnv structure: stores the new
 *  address of the next available byte.
 *  May also modify the interpreter's result in case of failure.
 *
 *----------------------------------------------------------------------
 */

static int
ExtractInteger(interp, envPtr, valuePtr)
    Tcl_Interp *interp;			/* current interpreter */
    ExtractionEnv *envPtr;		/* the extraction environment */
    int *valuePtr;			/* address of int variable to load */
{
    char *endPtr;
    char savedChar;
    char *codePtr = envPtr->curImagePtr;
    char *codeEnd = envPtr->imageEnd;

    /*
     * Skip to the first non-whitespace character.
     */

    for (;;) {
	if (codePtr == codeEnd) {
	    Tcl_AppendResult(interp, prematureEnd, (char *) NULL);
	    return TCL_ERROR;
	}
	if (!isspace(UCHAR(*codePtr))) {
	    break;
	}
	codePtr += 1;
    }

    /*
     * Find the end of the current word.
     */

    endPtr = FindEnd(codePtr, codeEnd);
    savedChar = *endPtr;
    *endPtr = '\0';

    if (Tcl_GetInt(interp, codePtr, valuePtr) != TCL_OK) {
	*endPtr = savedChar;
	AppendErrorLocation(interp, envPtr);
	return TCL_ERROR;
    }

    *endPtr = savedChar;
    envPtr->curImagePtr = endPtr;

    return TCL_OK;
}

/*
 *----------------------------------------------------------------------
 *
 * ExtractByteSequence --
 *
 *  Extracts a byte sequence from the compiled image, place it into the array
 *  at seqPtr. The length argument can be specified as -1, in which case the
 *  procedure extracts the length from the byte sequence header, or as a
 *  number that specifies the expected length of the sequence. In the latter
 *  case, the length obtained from the byte sequence header is compared to
 *  the value passed in, and an error is returned if the two don't match.
 *  The seqSize argument specifies the size of the seqPtr array.
 *  The byte sequence is assumed to have been encoded with a modified
 *  ASCII85 filter, as described in cmpWrite.c.
 *
 * Results:
 *  Returns TCL_OK on success, TCL_ERROR on failure.
 *
 * Side effects:
 *  Loads the byte sequence into the array starting at seqPtr.
 *  Updates the contents of the ExtractionEnv structure: stores the new
 *  address of the next available byte.
 *  May also modify the interpreter's result in case of failure.
 *
 *----------------------------------------------------------------------
 */

static int
ExtractByteSequence(interp, length, envPtr, seqPtr, seqSize)
    Tcl_Interp *interp;		/* current interpreter */
    int length;			/* the length in bytes of the byte sequence to
				 * extract, -1 to get it from the header */
    ExtractionEnv *envPtr;	/* the extraction environment */
    unsigned char *seqPtr;	/* where the bytes will be written */
    int seqSize;		/* the size in bytes of the array at seqPtr */
{
    char *imagePtr;
    char *imageEnd;
    int hLen, code;
    A85DecodeContext decodeCtx;
    A85DecodeContext *ctxPtr = &decodeCtx;

    /*
     * read the length from the header; we need to do this even in cases where
     * the length is passed in.
     * Then do the length checks if necessary.
     */

    if (ExtractInteger(interp, envPtr, &hLen) != TCL_OK) {
	return TCL_ERROR;
    }

    if (length < 0) {
	length = hLen;
    } else if (length != hLen) {
	Tcl_AppendResult(interp, "inconsistent byte sequence length",
		(char *) NULL);
	AppendErrorLocation(interp, envPtr);
	return TCL_ERROR;
    }

    if (length > seqSize) {
	Tcl_AppendResult(interp, "byte sequence too big for storage",
		(char *) NULL);
	AppendErrorLocation(interp, envPtr);
	return TCL_ERROR;
    }

    imagePtr = envPtr->curImagePtr;
    imageEnd = envPtr->imageEnd;
    A85InitDecodeContext(length, seqPtr, ctxPtr);

    while (decodeCtx.bytesToDecode > 0) {
	if (imagePtr == imageEnd) {
	    envPtr->curImagePtr = imagePtr;
	    Tcl_AppendResult(interp, prematureEnd, (char *) NULL);
	    return TCL_ERROR;
	}

	code = decodeMap[(int) *imagePtr];
	imagePtr += 1;

	if (code == A85_ILLEGAL_CHAR) {
	    envPtr->curImagePtr = imagePtr - 1;
	    Tcl_AppendResult(interp, "malformed byte sequence", (char *) NULL);
	    AppendErrorLocation(interp, envPtr);
	    return TCL_ERROR;
	} else if (code != A85_WHITESPACE) {
	    if (A85DecodeByte(interp, code, ctxPtr) != TCL_OK) {
		envPtr->curImagePtr = imagePtr - 1;
		AppendErrorLocation(interp, envPtr);
		return TCL_ERROR;
	    }
	}
    }

    envPtr->curImagePtr = imagePtr;

    return TCL_OK;
}

/*
 *----------------------------------------------------------------------
 *
 * ExtractString --
 *
 *  Extracts a string from the compiled image, place it into the array
 *  at strPtr. The length argument must be specified, because strings are
 *  written out without count fields.
 *
 * Results:
 *  Returns TCL_OK on success, TCL_ERROR on failure.
 *
 * Side effects:
 *  Loads the string into the array starting at strPtr.
 *  Updates the contents of the ExtractionEnv structure: stores the new
 *  address of the next available byte.
 *  May also modify the interpreter's result in case of failure.
 *
 *----------------------------------------------------------------------
 */

static int
ExtractString(interp, length, envPtr, strPtr, strSize)
    Tcl_Interp *interp;		/* current interpreter */
    int length;			/* the length in bytes of the byte sequence to
				 * extract, -1 to get it from the header */
    ExtractionEnv *envPtr;	/* the extraction environment */
    char *strPtr;		/* where the bytes will be written */
    int strSize;		/* the size in bytes to read. Also the minimum
				 * size of the strptr array */
{
    char *imagePtr;

    imagePtr = envPtr->curImagePtr;

    if ((imagePtr + strSize) > envPtr->imageEnd) {
	Tcl_AppendResult(interp, prematureEnd, (char *) NULL);
	return TCL_ERROR;
    }

    memcpy(strPtr, imagePtr, (size_t) strSize);

    envPtr->curImagePtr += strSize;

    return TCL_OK;
}

/*
 *----------------------------------------------------------------------
 *
 * AllocAndExtractByteSequence --
 *
 *  Get the size of the byte sequence from the compiled image, alloc a buffer
 *  large enough for it, then extract it. Places the allocated pointer in
 *  *seqPtrPtr, and the size of the byte sequence in *seqSizePtr.
 *  If 'addNull' is true, then the procedure adds a terminating NULL char
 *  to the sequence; this terminating char is not counted in the size value
 *  that is returned.
 *
 * Results:
 *  Returns TCL_OK on success, TCL_ERROR on failure.
 *
 * Side effects:
 *  Loads the byte sequence into the array starting at *seqPtr.
 *  Updates the contents of the ExtractionEnv structure: stores the new
 *  address of the next available byte.
 *  May also modify the interpreter's result in case of failure.
 *
 *----------------------------------------------------------------------
 */

static int
AllocAndExtractByteSequence(interp, envPtr, addNull, seqPtrPtr, seqSizePtr)
    Tcl_Interp *interp;		/* current interpreter */
    ExtractionEnv *envPtr;	/* the extraction environment */
    int addNull;		/* if true, add a NULL char at the end of
				 * the sequence */
    unsigned char **seqPtrPtr;	/* where the bytes will be written */
    int *seqSizePtr;		/* the size in bytes of the array at seqPtr */
{
    char *curImagePtr;
    int hLen, allocLen;
    unsigned char *seqBuf;

    /*
     * read the length from the header; we need to do this so that we can
     * allocate the buffer. Then, let's move the extraction environment back
     * to where it was at the start of the call, so that we can call
     * ExtractByteSequence.
     */

    curImagePtr = envPtr->curImagePtr;
    if (ExtractInteger(interp, envPtr, &hLen) != TCL_OK) {
	return TCL_ERROR;
    }
    envPtr->curImagePtr = curImagePtr;

    allocLen = (addNull) ? hLen + 1 : hLen;
    seqBuf = (unsigned char *) ckalloc((size_t) allocLen);
    if (ExtractByteSequence(interp, hLen, envPtr, seqBuf, hLen) != TCL_OK) {
	ckfree((char *) seqBuf);
	return TCL_ERROR;
    }

    *seqPtrPtr = seqBuf;
    *seqSizePtr = hLen;

    if (addNull) {
	seqBuf[hLen] = 0;
    }

    return TCL_OK;
}

/*
 *----------------------------------------------------------------------
 *
 * AllocAndExtractString --
 *
 *  Get the size of the string from the compiled image, alloc a buffer
 *  large enough for it, then extract it. Places the allocated pointer in
 *  *strPtrPtr, and the size of the string in *strSizePtr.
 *  If 'addNull' is true, then the procedure adds a terminating NULL char
 *  to the sequence; this terminating char is not counted in the size value
 *  that is returned.
 *
 * Results:
 *  Returns TCL_OK on success, TCL_ERROR on failure.
 *
 * Side effects:
 *  Loads the string into the array starting at *seqPtrPtr.
 *  Updates the contents of the ExtractionEnv structure: stores the new
 *  address of the next available byte.
 *  May also modify the interpreter's result in case of failure.
 *
 *----------------------------------------------------------------------
 */

static int
AllocAndExtractString(interp, envPtr, addNull, strPtrPtr, strSizePtr)
    Tcl_Interp *interp;		/* current interpreter */
    ExtractionEnv *envPtr;	/* the extraction environment */
    int addNull;		/* if true, add a NULL char at the end of
				 * the sequence */
    char **strPtrPtr;		/* where the bytes will be written */
    int *strSizePtr;		/* the size in bytes of the array at seqPtr */
{
    int hLen, allocLen;
    char *strBuf;
    char *imagePtr;
    char *imageEnd;

    /*
     * read the length from the header; we don't need to move the extraction
     * environment back, because ExtractString does not expect a count field.
     * But make sure we skip to just past end-of-line
     */

    if (ExtractInteger(interp, envPtr, &hLen) != TCL_OK) {
	return TCL_ERROR;
    }

    /*
     * skip to EOL, but not beyond, because that belongs to the string
     */

    imagePtr = envPtr->curImagePtr;
    imageEnd = envPtr->imageEnd;
    for (;;) {
	if (imagePtr == imageEnd) {
	    Tcl_AppendResult(interp, prematureEnd, (char *) NULL);
	    return TCL_ERROR;
	}
	if (*imagePtr == '\n') {
	    break;
	}
	imagePtr += 1;
    }

    envPtr->curImagePtr = imagePtr + 1;

    allocLen = (addNull) ? hLen + 1 : hLen;
    strBuf = (char *) ckalloc((size_t) allocLen);
    if (ExtractString(interp, hLen, envPtr, strBuf, hLen) != TCL_OK) {
	ckfree(strBuf);
	return TCL_ERROR;
    }

    *strPtrPtr = strBuf;
    *strSizePtr = hLen;

    if (addNull) {
	strBuf[hLen] = '\0';
    }

    return TCL_OK;
}

/*
 *----------------------------------------------------------------------
 *
 * ExtractObjArray --
 *
 *  Extracts a number of objects from the compiled image, instantiate them,
 *  and place pointers to them in the array at objArray. The numLitObjects
 *  argument can be specified as -1, in which case the procedure extracts
 *  the number of objects from the array header, or as a number that specifies
 *  the expected number of objects in the array. In the latter case, the value
 *  obtained from the array header is compared to the value passed in, and an
 *  error is returned if the two don't match.
 *  The objArraySize argument specifies the size of the objArray array.
 *
 * Results:
 *  Returns TCL_OK on success, TCL_ERROR on failure.
 *
 * Side effects:
 *  Populates the object array with pointers to objects extracted from the
 *  file.
 *  Creates the objects listed in the compiled image.
 *  Updates the contents of the ExtractionEnv structure: stores the new
 *  address of the next available byte.
 *  May also modify the interpreter's result in case of failure.
 *
 *----------------------------------------------------------------------
 */

static int
ExtractObjArray(interp, numLitObjects, envPtr, objArray, objArraySize)
    Tcl_Interp *interp;		/* the current interpreter */
    int numLitObjects;		/* how many objects are expected to be
				 * in the list following */
    ExtractionEnv *envPtr;	/* the extraction environment */
    Tcl_Obj **objArray;		/* array to be filled with pointers to the
				 * objects extracted from the image */
    int objArraySize;		/* size (in number of entries) of objArray */
{
    int hnumLitObjects;
    int iObj;
    Tcl_Obj *objPtr;

    /*
     * read the number of objects from the header; we need to do this even
     * in cases where the number is passed in.
     * Then do the checks if necessary.
     */

    if (ExtractInteger(interp, envPtr, &hnumLitObjects) != TCL_OK) {
	return TCL_ERROR;
    }

    if (numLitObjects < 0) {
	numLitObjects = hnumLitObjects;
    } else if (numLitObjects != hnumLitObjects) {
	Tcl_AppendResult(interp, "inconsistent object array size",
		(char *) NULL);
	return TCL_ERROR;
    }

    if (numLitObjects > objArraySize) {
	Tcl_AppendResult(interp, "object array too big for storage",
		(char *) NULL);
	return TCL_ERROR;
    }

    for (iObj=0 ; iObj < numLitObjects ; iObj++) {
	objPtr = ExtractObject(interp, envPtr);
	if (!objPtr) {
	    return TCL_ERROR;
	}
	objArray[iObj] = objPtr;
    }

    return TCL_OK;
}

/*
 *----------------------------------------------------------------------
 *
 * ExtractObject --
 *
 *  Extracts an object from the compiled image, instantiate it, and returns it.
 *  Note that the object's reference count is initialized to 1.
 *
 * Results:
 *  Returns a Tcl_Obj pointer, NULL on error.
 *
 * Side effects:
 *  Creates the objects listed in the compiled image.
 *  Updates the contents of the ExtractionEnv structure: stores the new
 *  address of the next available byte.
 *  May also modify the interpreter's result in case of failure.
 *
 *----------------------------------------------------------------------
 */

static Tcl_Obj *
ExtractObject(interp, envPtr)
    Tcl_Interp *interp;		/* the current interpreter */
    ExtractionEnv *envPtr;	/* the extraction environment */
{
    Tcl_Obj *objPtr = NULL;
    char *imagePtr;
    char *imageEnd;
    char *objString;
    char typeCode;
    int objStringLength;
    Tcl_ObjType *objTypePtr = NULL;

    imagePtr = envPtr->curImagePtr;
    imageEnd = envPtr->imageEnd;

    /*
     * skip whitespace, get the typecode
     */

    for (;;) {
	if (imagePtr == imageEnd) {
	    Tcl_AppendResult(interp, prematureEnd, (char *) NULL);
	    return NULL;
	}
	if (!isspace(UCHAR(*imagePtr))) {
	    break;
	}
	imagePtr += 1;
    }

    typeCode = *imagePtr;
    imagePtr += 1;
    envPtr->curImagePtr = imagePtr;

    /*
     * process by object type.
     */

    if (typeCode == CMP_STRING_CODE) {
	if (AllocAndExtractString(interp, envPtr, 1,
		&objString, &objStringLength) != TCL_OK) {
	    return NULL;
	}

	objPtr = Tcl_NewObj();

	if ((envPtr->sig.tclMajorVersion <= 8)
		&& (envPtr->sig.tclMinorVersion < 1)) {

	    /*
	     * if compiled with 8.0-based compiler,
	     * need to make the object a byte array
	     */

	    Tcl_DString utfString;

	    Tcl_ExternalToUtfDString(NULL, objString, objStringLength,
		    &utfString);

	    Tcl_SetStringObj(objPtr, Tcl_DStringValue(&utfString),
		    Tcl_DStringLength(&utfString));

	    Tcl_DStringFree(&utfString);

	} else {
	    objPtr->bytes = objString;
	    objPtr->length = objStringLength;
	}
	Tcl_IncrRefCount(objPtr);

    } else if (typeCode == CMP_XSTRING_CODE) {
	if (AllocAndExtractByteSequence(interp, envPtr, 1,
		(unsigned char **) &objString, &objStringLength) != TCL_OK) {
	    return NULL;
	}

	objPtr = Tcl_NewObj();

	if ((envPtr->sig.tclMajorVersion <= 8)
		&& (envPtr->sig.tclMinorVersion < 1)) {
	    /*
	     * if compiled with 8.0-based compiler,
	     * need to make the object a byte array
	     */

	    Tcl_DString utfString;

	    Tcl_ExternalToUtfDString(NULL, objString, objStringLength,
		    &utfString);

	    Tcl_SetStringObj(objPtr, Tcl_DStringValue(&utfString),
		    Tcl_DStringLength(&utfString));

	    Tcl_DStringFree(&utfString);

	} else {
	    objPtr->bytes = objString;
	    objPtr->length = objStringLength;
	}
	Tcl_IncrRefCount(objPtr);

    } else {
	/*
	 * skip whitespace
	 */

	for (;;) {
	    if (imagePtr == imageEnd) {
		Tcl_AppendResult(interp, prematureEnd, (char *) NULL);
		return NULL;
	    }
	    if (!isspace(UCHAR(*imagePtr))) {
		break;
	    }
	    imagePtr += 1;
	}

	envPtr->curImagePtr = imagePtr;

	if (typeCode == CMP_INT_CODE) {
	    objPtr = CreateSimpleObject(envPtr);
	    objTypePtr = cmpIntType;
	} else if (typeCode == CMP_DOUBLE_CODE) {
	    objPtr = CreateSimpleObject(envPtr);
	    objTypePtr = cmpDoubleType;
	} else if (typeCode == CMP_BOOLEAN_CODE) {
	    objPtr = CreateSimpleObject(envPtr);
	    objTypePtr = NULL;
	} else if (typeCode == CMP_BYTECODE_CODE) {
	    /*
	     * This block is a copy of most of the code in
	     * ExtractCompiledFile; unfortunately, we can't share the two
	     * because here I need the final values of the local extraction
	     * environment so that I can update the outer one.
	     */

	    ExtractionEnv localExEnv;

	    InitExtractEnv(imagePtr, imageEnd, &localExEnv);
	    localExEnv.sig = envPtr->sig;

	    if (ExtractByteCode(interp, &localExEnv) != TCL_OK) {
		CleanupExtractEnv(&localExEnv);
		return NULL;
	    }

	    /*
	     * create the new object.
	     * We give its string representation a dummy value, so that
	     * commands like "info body" don't cause a panic. The reason
	     * "info body" does is this: "info body" simply looks up the
	     * body object and returns on the stack. Later, when we access
	     * the object as a string, Tcl_GetStringFromObj calls the
	     * type's updateStringProc, which for bytecodes panics.
	     * So put a dummy to avoid the call.
	     * The side effects of this need to be investigated further.
	     */

	    objPtr = Tcl_NewStringObj(noSourceCode, -1);
	    Tcl_IncrRefCount(objPtr);

	    objPtr->internalRep.otherValuePtr = (VOID *) localExEnv.codePtr;
	    objPtr->typePtr = cmpByteCodeType;

	    localExEnv.codePtr->refCount++;

	    /*
	     * skip over the ByteCode representation we just read in
	     */

	    envPtr->curImagePtr = localExEnv.curImagePtr;

	    CleanupExtractEnv(&localExEnv);
	} else if (typeCode == CMP_PROCBODY_CODE) {
	    /*
	     * A ProcBodyType Tcl_Obj contains a ByteCode dump and a
	     * number of fields in a Proc struct.
	     *
	     * First, extract the ByteCode.
	     */

	    ExtractionEnv localExEnv;

	    InitExtractEnv(imagePtr, imageEnd, &localExEnv);
	    localExEnv.sig = envPtr->sig;

	    if (ExtractByteCode(interp, &localExEnv) != TCL_OK) {
		CleanupExtractEnv(&localExEnv);
		return NULL;
	    }

	    /*
	     * skip over the ByteCode representation we just read in, then
	     * finish reading in the object.
	     */

	    envPtr->curImagePtr = localExEnv.curImagePtr;

	    objPtr = ExtractProcBody(interp, localExEnv.codePtr, envPtr);
	    if (objPtr) {
		Tcl_IncrRefCount(objPtr);
	    }

	    CleanupExtractEnv(&localExEnv);
	} else {
	    char errBuf[2];
	    errBuf[0] = typeCode;
	    errBuf[1] = '\0';
	    Tcl_AppendResult(interp, "unknown object type \"",
		    errBuf, "\"", (char *) NULL);
	    AppendErrorLocation(interp, envPtr);
	    return NULL;
	}

	if (objTypePtr
		&& (Tcl_ConvertToType(interp, objPtr, objTypePtr) != TCL_OK)) {
	    Tcl_DecrRefCount(objPtr);
	    return NULL;
	}
    }

    return objPtr;
}

/*
 *----------------------------------------------------------------------
 *
 * CreateSimpleObject --
 *
 *  Creates a Tcl object whose string representation is the Tcl word at the
 *  current extraction pointer.
 *  Note that the procedure increases the reference count on the object.
 *
 * Results:
 *  Returns a newly created Tcl object.
 *
 * Side effects:
 *  Updates the contents of the extraction environment.
 *
 *----------------------------------------------------------------------
 */

static Tcl_Obj *
CreateSimpleObject(envPtr)
    ExtractionEnv *envPtr;	/* the extraction environment */
{
    char *endPtr;
    char *imagePtr = envPtr->curImagePtr;
    Tcl_Obj *objPtr;

    endPtr = FindEnd(imagePtr, envPtr->imageEnd);

    objPtr = Tcl_NewStringObj(imagePtr, endPtr - imagePtr);
    Tcl_IncrRefCount(objPtr);

    envPtr->curImagePtr = endPtr;

    return objPtr;
}

/*
 *----------------------------------------------------------------------
 *
 * ExtractExcRangeArray --
 *
 *  Extracts the exception range array for a ByteCode struct from a compiled
 *  image. The numExceptRanges argument can be specified as -1, in which case the
 *  procedure extracts the number of exception ranges from the array header,
 *  or as a number that specifies the expected number of exception ranges in
 *  the array. In the latter case, the value obtained from the array header is
 *  compared to the value passed in, and an error is returned if the two don't
 *  match.
 *  The excArraySize argument specifies the size of the excArray array.
 *
 * Results:
 *  Returns TCL_OK on success, TCL_ERROR on failure.
 *
 * Side effects:
 *  Populates the exception ranges array with values extracted from the image.
 *  Updates the contents of the ExtractionEnv structure: stores the new
 *  address of the next available byte.
 *  May also modify the interpreter's result in case of failure.
 *
 *----------------------------------------------------------------------
 */

static int
ExtractExcRangeArray(interp, numExceptRanges, envPtr, excArray, excArraySize)
    Tcl_Interp *interp;		/* the current interpreter */
    int numExceptRanges;		/* how many exception ranges are expected to
				 * be in the list following */
    ExtractionEnv *envPtr;	/* the extraction environment */
    ExceptionRange *excArray;	/* array to be filled with exception ranges
				 * extracted from the image */
    int excArraySize;		/* size (in number of entries) of excArray */
{
    char *imagePtr;
    char *imageEnd;
    int hnumExceptRanges;
    int iRange;
    char codeType;
    ExceptionRange *excPtr = excArray;

    /*
     * read the number of exception ranges from the header; we need to do
     * this even in cases where the number is passed in.
     * Then do the checks if necessary.
     */

    if (ExtractInteger(interp, envPtr, &hnumExceptRanges) != TCL_OK) {
	return TCL_ERROR;
    }

    if (numExceptRanges < 0) {
	numExceptRanges = hnumExceptRanges;
    } else if (numExceptRanges != hnumExceptRanges) {
	Tcl_AppendResult(interp, "inconsistent exception ranges array size",
		(char *) NULL);
	AppendErrorLocation(interp, envPtr);
	return TCL_ERROR;
    }

    if (numExceptRanges > excArraySize) {
	Tcl_AppendResult(interp, "exception ranges array too big for storage",
		(char *) NULL);
	AppendErrorLocation(interp, envPtr);
	return TCL_ERROR;
    }

    imagePtr = envPtr->curImagePtr;
    imageEnd = envPtr->imageEnd;

    for (iRange=0 ; iRange < numExceptRanges ; iRange++) {
	/*
	 * skip whitespace.
	 */

	for (;;) {
	    if (imagePtr == imageEnd) {
		Tcl_AppendResult(interp, prematureEnd, (char *) NULL);
		return TCL_ERROR;
	    }
	    if (!isspace(UCHAR(*imagePtr))) {
		break;
	    }
	    imagePtr += 1;
	}

	codeType = *imagePtr;
	imagePtr += 1;
	envPtr->curImagePtr = imagePtr;

	/*
	 * look up the type, then read in all the int values.
	 */

	if (ExcRangeFromName((int) codeType, &excPtr->type) < 0) {
	    char errBuf[2];
	    errBuf[0] = codeType;
	    errBuf[1] = '\0';
	    Tcl_AppendResult(interp, "unknown exception range type: ",
		    errBuf, (char *) NULL);
	    AppendErrorLocation(interp, envPtr);
	    return TCL_ERROR;
	}

	if ((ExtractInteger(interp, envPtr, &excPtr->nestingLevel) != TCL_OK)
		|| (ExtractInteger(interp, envPtr,
			&excPtr->codeOffset) != TCL_OK)
		|| (ExtractInteger(interp, envPtr,
			&excPtr->numCodeBytes) != TCL_OK)
		|| (ExtractInteger(interp, envPtr,
			&excPtr->breakOffset) != TCL_OK)
		|| (ExtractInteger(interp, envPtr,
			&excPtr->continueOffset) != TCL_OK)
		|| (ExtractInteger(interp, envPtr,
			&excPtr->catchOffset) != TCL_OK)) {
	    return TCL_ERROR;
	}

	excPtr += 1;
	imagePtr = envPtr->curImagePtr;
    }

    return TCL_OK;
}

/*
 *----------------------------------------------------------------------
 *
 * ExtractAuxDataArray --
 *
 *  Extracts the AuxData array for a ByteCode struct from a compiled image.
 *  The numAuxDataItems argument can be specified as -1, in which case the
 *  procedure extracts the number of AuxData items from the array header,
 *  or as a number that specifies the expected number of AuxData items in
 *  the array. In the latter case, the value obtained from the array header is
 *  compared to the value passed in, and an error is returned if the two don't
 *  match.
 *  The auxDataArraySize argument specifies the size of the auxDataArray array.
 *
 * Results:
 *  Returns TCL_OK on success, TCL_ERROR on failure.
 *
 * Side effects:
 *  Populates the AuxData array with values extracted from the image.
 *  Updates the contents of the ExtractionEnv structure: stores the new
 *  address of the next available byte.
 *  May also modify the interpreter's result in case of failure.
 *
 *----------------------------------------------------------------------
 */

static int
ExtractAuxDataArray(interp, numAuxDataItems, envPtr,
	auxDataArray, auxDataArraySize)
    Tcl_Interp *interp;		/* the current interpreter */
    int numAuxDataItems;	/* how many AuxData items are expected to
				 * be in the list following */
    ExtractionEnv *envPtr;	/* the extraction environment */
    AuxData *auxDataArray;	/* array to be filled with AuxData items
				 * extracted from the image */
    int auxDataArraySize;	/* size (in number of entries) of
				 * auxDataArray */
{
    char *imagePtr;
    char *imageEnd;
    int hNumAuxDataItems;
    AuxData *auxPtr = auxDataArray;
    int iAuxData;
    char typeCode;

    /*
     * read the number of AuxData items from the header; we need to do
     * this even in cases where the number is passed in.
     * Then do the checks if necessary.
     */

    if (ExtractInteger(interp, envPtr, &hNumAuxDataItems) != TCL_OK) {
	return TCL_ERROR;
    }

    if (numAuxDataItems < 0) {
	numAuxDataItems = hNumAuxDataItems;
    } else if (numAuxDataItems != hNumAuxDataItems) {
	Tcl_AppendResult(interp, "inconsistent aux data array size",
		(char *) NULL);
	AppendErrorLocation(interp, envPtr);
	return TCL_ERROR;
    }

    if (numAuxDataItems > auxDataArraySize) {
	Tcl_AppendResult(interp, "aux data array too big for storage",
		(char *) NULL);
	AppendErrorLocation(interp, envPtr);
	return TCL_ERROR;
    }

    imagePtr = envPtr->curImagePtr;
    imageEnd = envPtr->imageEnd;

    for (iAuxData=0 ; iAuxData < numAuxDataItems ; iAuxData++) {
	/*
	 * skip whitespace
	 */

	for (;;) {
	    if (imagePtr == imageEnd) {
		Tcl_AppendResult(interp, prematureEnd, (char *) NULL);
		return TCL_ERROR;
	    }
	    if (!isspace(UCHAR(*imagePtr))) {
		break;
	    }
	    imagePtr += 1;
	}

	typeCode = *imagePtr;
	imagePtr += 1;
	envPtr->curImagePtr = imagePtr;

	/*
	 * look up the type, then dispatch to an extractor routine based on it
	 */

	if (typeCode == CMP_FOREACH_INFO) {
	    int result = ExtractForeachInfo(interp, envPtr, auxPtr);
	    if (result != TCL_OK) {
		return result;
	    }
#ifdef TCL_85_PLUS
	} else if (typeCode == CMP_JUMPTABLE_INFO) {
	  int result = ExtractJumptableInfo(interp, envPtr, auxPtr);
	  if (result != TCL_OK) {
	    return result;
	  }
#endif
	} else {
	    char errBuf[2];
	    errBuf[0] = typeCode;
	    errBuf[1] = '\0';
	    Tcl_AppendResult(interp, "unknown aux data type: ",
		    errBuf, (char *) NULL);
	    AppendErrorLocation(interp, envPtr);
	    return TCL_ERROR;
	}

	auxPtr += 1;
	imagePtr = envPtr->curImagePtr;
    }

    return TCL_OK;
}

/*
 *----------------------------------------------------------------------
 *
 * ExtractSignature --
 *
 *  Extracts the signature data from the image at codePtr.
 *
 * Results:
 *  Returns the next available address in the image after the signature,
 *  0 on failure.
 *
 * Side effects:
 *  Populates the ImageSignature at signaturePtr with values extracted from
 *  the file. May also modify the interpreter's result in case of failure.
 *
 *----------------------------------------------------------------------
 */

static char*
ExtractSignature(interp, codePtr, codeEnd, signaturePtr)
    Tcl_Interp *interp;			/* Current interpreter. */
    char *codePtr;			/* pointer to start of the image */
    char *codeEnd;			/* pointer to end of the image */
    ImageSignature *signaturePtr;	/* pointer to a struct that will be
					 * populated with signature values
					 * extracted from the image */
{
    static char badMsg[] = "bad image signature in bytecode";

    char savedChar, *savedCharPtr;
    int numScanned;

    /*
     * skip whitespace to the first nonempty line in the image; this skips
     * over whitespace that could have been inserted by the construct
     *	loader::bceval {
     *	...
     *	}
     */

    for (;;) {
	if (codePtr == codeEnd) {
	    return NULL;
	}
	if (!isspace(UCHAR(*codePtr))) {
	    break;
	}
	codePtr += 1;
    }

    /*
     * Find the end the string at the first EOL, which delimits the
     * signature line. The saved pointer will also be used for the return
     * value.
     */

    savedCharPtr = strchr(codePtr, '\n');
    if (!savedCharPtr) {
	Tcl_AppendResult(interp, badMsg, (char *) NULL);
	return NULL;
    }
    numScanned = sscanf(codePtr, CMP_SIGNATURE_HEADER " %d %d %d.%d %d.%d%c",
	    &signaturePtr->formatNumber,    &signaturePtr->buildNumber,
	    &signaturePtr->cmpMajorVersion, &signaturePtr->cmpMinorVersion,
	    &signaturePtr->tclMajorVersion, &signaturePtr->tclMinorVersion,
	    &savedChar);
    if ((numScanned != 7) || !isspace(UCHAR(savedChar))) {
	Tcl_AppendResult(interp, badMsg, (char *) NULL);
	return NULL;
    }
    return (savedCharPtr + 1);
}

/*
 *----------------------------------------------------------------------
 *
 * CheckSignature --
 *
 *  Check the signature struct to see if we support this bytecode version.
 *
 * Results:
 *  Returns TCL_OK if we think that we can handle this bytecode, TCL_ERROR
 *  otherwise.
 *
 * Side effects:
 *  Loads an error message in the result if there was an error.
 *
 *----------------------------------------------------------------------
 */

static int
CheckSignature(interp, signaturePtr)
    Tcl_Interp *interp;			/* Current interpreter. */
    ImageSignature *signaturePtr;	/* pointer to a signature struct to
					 * be checked */
{
    /*
     * Currently, we support bytecodes that were generated by all earlier
     * versions of the compiler
     */

    if (signaturePtr->formatNumber > formatVersion) {
	char buf[32];
	sprintf(buf, "%d", signaturePtr->formatNumber);
	Tcl_AppendResult(interp, "unsupported bytecode version: ",
		buf, (char *) NULL);
	return TCL_ERROR;
    }

    return TCL_OK;
}

/*
 *----------------------------------------------------------------------
 *
 * ExtractCompiledFile --
 *
 *  Parses the compiled image in the codePtr buffer (of length codeLength),
 *  creates a Tcl_Obj whose internal representation is a ByteCode structure
 *  and whose string representation is invalid.
 *
 * Results:
 *  A Tcl_Obj that holds the generated ByteCode structure. Returns 0 on
 *  failure.
 *
 * Side effects:
 *  May create othe Tcl_Objs, depending on the input. Also will modify the
 *  interprete's result on failure.
 *
 *----------------------------------------------------------------------
 */

static Tcl_Obj*
ExtractCompiledFile(interp, codePtr, codeLength)
    Tcl_Interp *interp;		/* Current interpreter. */
    char *codePtr;		/* the compiled image that is to be parsed */
    int codeLength;		/* length of the compiled image */
{
    ExtractionEnv exEnv;
    Tcl_Obj *objPtr;
    char *newCodePtr;

    /*
     * The signature line contains the version of the interpreter that was
     * used to generate the compiled script, and that value should be
     * known to the extraction routines, because formats may change between
     * releases. For the time being, disregard it.
     */

    newCodePtr = ExtractSignature(interp, codePtr, (codePtr + codeLength),
	    &(exEnv.sig));
    if (newCodePtr == NULL) {
	return NULL;
    }

    InitExtractEnv(newCodePtr, (codePtr + codeLength), &exEnv);

    /*
     * Do not allow loading newer bytecodes into older interps
     */
    if ((exEnv.sig.tclMajorVersion > tclMajorVersion) ||
	    ((exEnv.sig.tclMajorVersion == tclMajorVersion) &&
		    (exEnv.sig.tclMinorVersion > tclMinorVersion))) {
	char buf[128];

	CleanupExtractEnv(&exEnv);
	sprintf(buf, "unable to load bytecode generated for Tcl %d.%d"
		" into Tcl %d.%d",
		exEnv.sig.tclMajorVersion, exEnv.sig.tclMinorVersion,
		tclMajorVersion, tclMinorVersion);
	Tcl_AppendResult(interp, buf, (char *) NULL);
	return NULL;
    }

    if (ExtractByteCode(interp, &exEnv) != TCL_OK) {
	CleanupExtractEnv(&exEnv);
	return NULL;
    }

    /*
     * See the comment above Tcl_NewStringObj in ExtractObjArray.
     */

    objPtr = Tcl_NewStringObj(noSourceCode, -1);
    Tcl_IncrRefCount(objPtr);

    objPtr->internalRep.otherValuePtr = (VOID *) exEnv.codePtr;
    objPtr->typePtr = cmpByteCodeType;
    exEnv.codePtr->refCount++;

    CleanupExtractEnv(&exEnv);

    return objPtr;
}

/*
 *----------------------------------------------------------------------
 *
 * InitExtractEnv --
 *
 *  Initializes a ExtractionEnv structure.
 *
 * Results:
 *  None.
 *
 * Side effects:
 *  Populates the ExtractionEnv structure with appropriate initial values.
 *
 *----------------------------------------------------------------------
 */

static void
InitExtractEnv(codeBase, codeEnd, envPtr)
    char *codeBase;		/* the base pointer for the image */
    char *codeEnd;		/* pointer to first byte past the image */
    ExtractionEnv *envPtr;	/* the structure to initialize */
{
    envPtr->imageBase = codeBase;
    envPtr->imageEnd = codeEnd;
    envPtr->curImagePtr = codeBase;

    envPtr->codePtr = NULL;
    envPtr->codeSize = 0;
}

/*
 *----------------------------------------------------------------------
 *
 * CleanupExtractEnv --
 *
 *  Cleans up a ExtractionEnv structure.
 *
 * Results:
 *  None.
 *
 * Side effects:
 *  If owned, frees the ByteCode struct.
 *
 *----------------------------------------------------------------------
 */

static void
CleanupExtractEnv(envPtr)
    ExtractionEnv *envPtr;	/* the structure to clean up */
{
    CleanupByteCode(envPtr);
}

/*
 *----------------------------------------------------------------------
 *
 * ExcRangeFromName --
 *
 *  Given a name, place the corresponding ExceptionRangeType enum in *typePtr.
 *  The name is really a one-character type code.
 *
 * Results:
 *  Returns 0 if it was successful at converting the name to an enum value,
 *  -1 otherwise.
 *
 * Side effects:
 *  None.
 *
 *----------------------------------------------------------------------
 */

static int
ExcRangeFromName(name, typePtr)
    int name;
    ExceptionRangeType* typePtr;
{
    CONST ExcRangeMap *mapPtr;

    for (mapPtr=&excRangeMap[0] ; mapPtr->name != 0 ; mapPtr++) {
	if (mapPtr->name == (char) name) {
	    *typePtr = mapPtr->type;
	    return 0;
	}
    }

    return -1;
}

/*
 *----------------------------------------------------------------------
 *
 * InitTypes --
 *
 *  Uses Tcl_GetObjType to load pointers to known object types into static
 *  variables, which can then be used instead of the known objects themselves.
 *
 * Results:
 *  None.
 *
 * Side effects:
 *  None.
 *
 *----------------------------------------------------------------------
 */

static void
InitTypes()
{
    if (didLoadTypes == 0) {
	ProcBodyRegisterTypes();

	cmpTclProProcBodyType = Tcl_GetObjType(CMP_PROCBODY_OBJ_TYPE_OLD);
	if (!cmpTclProProcBodyType) {
	    panic("InitTypes: failed to find the %s type",
		    CMP_PROCBODY_OBJ_TYPE_OLD);
	}

	cmpByteCodeType = Tcl_GetObjType("bytecode");
	if (!cmpByteCodeType) {
	    panic("InitTypes: failed to find the bytecode type");
	}

	cmpDoubleType = Tcl_GetObjType("double");
	if (!cmpDoubleType) {
	    panic("InitTypes: failed to find the double type");
	}

	cmpIntType = Tcl_GetObjType("int");
	if (!cmpIntType) {
	    panic("InitTypes: failed to find the int type");
	}

	cmpForeachInfoType = TclGetAuxDataType("ForeachInfo");
	if (!cmpForeachInfoType) {
	    panic("InitTypes: failed to find the ForeachInfo AuxData type");
	}
#ifdef TCL_85_PLUS
	cmpJumptableInfoType = TclGetAuxDataType("JumptableInfo");
	if (!cmpJumptableInfoType) {
	    panic("InitTypes: failed to find the JumptableInfo AuxData type");
	}
#endif
	didLoadTypes += 1;
    }
}

/*
 *----------------------------------------------------------------------
 *
 * InitCompatibilityLayer --
 *
 *  Initializes the internal structures used by the compatibility layer.
 *  Must be called before the public interfaces to the Loader package.
 *
 * Results:
 *  Returns TCL_OK on success, otherwise TCL_ERROR and leaves an error
 *  result in the interpreter.
 *
 * Side effects:
 *  Initializes a number of internal data structures used by the Loader
 *  implementation.
 *
 *----------------------------------------------------------------------
 */

static int
InitCompatibilityLayer(interp)
    Tcl_Interp *interp;		/* the Tcl interpreter for which the package
				 * is initialized */
{
    Tcl_CmdInfo info;

    if (compatibilityLayerInit) {
	return TCL_OK;
    }

    /*
     * Extract a pointer to the proc object command so we can wrap the API.
     * By extracting the routine, we do not have to add Tcl_ProcObjCmd to
     * the stubs list.
     *
     * Bug #3826: if the proc command is renamed, then this extraction will
     * yeild the new proc def rather than the Tcl_ProcObjCmd.  HACK: To fix
     * this problem in the case of the debugger (bug #3089), we check for the
     * existence of DbgNub_procCmd, which holds the original proc command.
     */

    if ((!Tcl_GetCommandInfo(interp, "DbgNub_procCmd", &info))
	    || (info.objProc == NULL)) {
	if ((!Tcl_GetCommandInfo(interp, "proc", &info))
		|| (info.objProc == NULL)) {
	    Tcl_AppendResult(interp,
		    "proc command could not be located.", NULL);
	    return TCL_ERROR;
	}
    }
    bcprocCmdProc = info.objProc;

    /*
     * Determine what interpreter interface to use.
     */

    procBodyFactory = (ProcBodyFactory *) TclNewProcBodyObj;
    procBodyCleanup = TclProcCleanupProc;

    compatibilityLayerInit = 1;

    return TCL_OK;
}

/*
 *----------------------------------------------------------------------
 *
 * TbcloadInit --
 *
 *  Initializes the internal structures used by the Loader package.
 *  Must be called before the public interfaces to the Loader package.
 *
 * Results:
 *  Returns TCL_OK on success, otherwise TCL_ERROR and leaves an error
 *  result in the interpreter.
 *
 * Side effects:
 *  Initializes a number of internal data structures used by the Loader
 *  implementation.
 *
 *----------------------------------------------------------------------
 */

int
TbcloadInit(interp)
    Tcl_Interp *interp;		/* the Tcl interpreter for which the package
				 * is initialized */
{
#ifdef TCL_85_PLUS
    /*
     * We need to assume 8.5a7, when variable reform (Sofer) took effect.
     */
    if (!Tcl_InitStubs(interp, "8.5a7", 0)) {
	return TCL_ERROR;
    }
#else
    if (!Tcl_InitStubs(interp, "8.1", 0)) {
	return TCL_ERROR;
    }
#endif

    /*
     * Initialize the compatibility layer.
     */

    if (InitCompatibilityLayer(interp) != TCL_OK) {
	return TCL_ERROR;
    }

    /*
     * Determine the format version of compiled code.  8.0-8.3 is v1,
     * 8.4+ is v2 (new bytecode instructions, different var flags).
     */

    Tcl_GetVersion(&tclMajorVersion, &tclMinorVersion, NULL, NULL);

    if ((tclMajorVersion == 8) && (tclMinorVersion <= 3)) {
	formatVersion = 1;
    } else {
	formatVersion = 2;
    }

    /*
     * Initialize the local copies of pointers to some built-in object types.
     * We need to do it because the built-in types are not exported by the
     * windows Tcl DLL.
     */

    InitTypes();

    return TCL_OK;
}

/*
 *----------------------------------------------------------------------
 *
 * ExtractForeachInfo --
 *
 *  Extract a ForeachInfo struct from the extraction environment..
 *
 * Results:
 *  Returns a TCL error code.
 *
 * Side effects:
 *  Creates a ForeachInfo AuxData at *auxPtr.
 *  Sets the TCL result on error.
 *
 *----------------------------------------------------------------------
 */

static int
ExtractForeachInfo(interp, envPtr, auxDataPtr)
    Tcl_Interp *interp;		/* the current interpreter */
    ExtractionEnv *envPtr;	/* the extraction environment */
    AuxData *auxDataPtr;	/* pointer to the AuxData item to be filled
				 * with the ForeachInfo extracted from the
				 * image */
{
    int i, j, result;
    int numLists, firstValueTemp, loopCtTemp, numVars;
    ForeachInfo *infoPtr = NULL;
    ForeachVarList *varListPtr = NULL;
    int *varPtr;

    /*
     * read in the control variables, allocate and initialize the
     * ForeachInfo struct.
     */

    result = ExtractInteger(interp, envPtr, &numLists);
    if (result != TCL_OK) {
	return result;
    }

    result = ExtractInteger(interp, envPtr, &firstValueTemp);
    if (result != TCL_OK) {
	return result;
    }

    result = ExtractInteger(interp, envPtr, &loopCtTemp);
    if (result != TCL_OK) {
	return result;
    }

    infoPtr = (ForeachInfo *) ckalloc((unsigned)
	    (sizeof(ForeachInfo) + (numLists * sizeof(ForeachVarList *))));
    infoPtr->numLists = numLists;
    infoPtr->firstValueTemp = firstValueTemp;
    infoPtr->loopCtTemp = loopCtTemp;
    for (i=0 ; i < numLists ; i++) {
	infoPtr->varLists[i] = (ForeachVarList *) NULL;
    }

    /*
     * now load the ForeachVarList structs
     */

    for (i=0 ; i < numLists ; i++) {
	result = ExtractInteger(interp, envPtr, &numVars);
	if (result != TCL_OK) {
	    goto errorReturn;
	}

	varListPtr = (ForeachVarList *) ckalloc((unsigned)
		sizeof(ForeachVarList) + numVars*sizeof(int));
	infoPtr->varLists[i] = varListPtr;
	varListPtr->numVars = numVars;

	varPtr = &varListPtr->varIndexes[0];
	for (j=0 ; j < numVars ; j++) {
	    result = ExtractInteger(interp, envPtr, varPtr);
	    if (result != TCL_OK) {
		goto errorReturn;
	    }
	    varPtr++;
	}
    }

    /*
     * finally! Assign the ForeachInfo to the AuxData.
     */

    auxDataPtr->type = cmpForeachInfoType;
    auxDataPtr->clientData = (ClientData) infoPtr;

    return TCL_OK;

    errorReturn:

    if (infoPtr) {
	for (i=0 ; i < infoPtr->numLists ; i++) {
	    ckfree((char *) infoPtr->varLists[i]);
	}
	ckfree((char *) infoPtr);
    }

    return result;
}

#ifdef TCL_85_PLUS
/*
 *----------------------------------------------------------------------
 *
 * ExtractJumptableInfo --
 *
 *  Extract a JumptableInfo struct from the extraction environment..
 *
 * Results:
 *  Returns a TCL error code.
 *
 * Side effects:
 *  Creates a ForeachInfo AuxData at *auxPtr.
 *  Sets the TCL result on error.
 *
 *----------------------------------------------------------------------
 */

static int
ExtractJumptableInfo(interp, envPtr, auxDataPtr)
    Tcl_Interp *interp;		/* the current interpreter */
    ExtractionEnv *envPtr;	/* the extraction environment */
    AuxData *auxDataPtr;	/* pointer to the AuxData item to be filled
				 * with the JumptableInfo extracted from the
				 * image */
{
    int i, result, numJmp, value, keyLength, new;
    JumptableInfo *infoPtr = NULL;
    Tcl_HashEntry *hEntry;
    unsigned char *key;

    /*
     * read in the control variables, allocate and initialize the
     * JumptableInfo struct.
     */

    result = ExtractInteger(interp, envPtr, &numJmp);
    if (result != TCL_OK) {
	return result;
    }

    infoPtr = (JumptableInfo *) ckalloc((unsigned)(sizeof(JumptableInfo)));
    Tcl_InitHashTable(&infoPtr->hashTable,TCL_STRING_KEYS);

    for(i=0;i<numJmp;i++) {
      result = ExtractInteger(interp, envPtr, &value);
      if (result != TCL_OK) {
	goto errorReturn;
      }

      result = AllocAndExtractByteSequence(interp, envPtr, 1,
					   &key, &keyLength);
      if (result != TCL_OK) {
	goto errorReturn;
      }
      hEntry = Tcl_CreateHashEntry(&infoPtr->hashTable,key,&new);
      ckfree(key);
      Tcl_SetHashValue(hEntry,(char*)value);
    }


    /*
     * finally! Assign the JumptableInfo to the AuxData.
     */

    auxDataPtr->type = cmpJumptableInfoType;
    auxDataPtr->clientData = (ClientData) infoPtr;

    return TCL_OK;

    errorReturn:

    if (infoPtr) {
      /* free hashtable + JumpTable Structure */
      Tcl_HashSearch hSearch;

      hEntry = Tcl_FirstHashEntry(&infoPtr->hashTable,&hSearch);
      while(hEntry) {
	Tcl_DeleteHashEntry(hEntry);
	hEntry = Tcl_NextHashEntry(&hSearch);
      }
      Tcl_DeleteHashTable(&infoPtr->hashTable);
      ckfree((char *) infoPtr);
    }

    return result;
}
#endif

/*
 *----------------------------------------------------------------------
 *
 * ExtractProcBody --
 *
 *  Extracts the fields in a Proc struct (minus ByteCode, which has already
 *  been extracted), then populate a new Proc with it, create a
 *  "procbody" Tcl_Obj and return it.
 *
 * Results:
 *  Returns a Tcl_Obj of type "procbody", NULL on error.
 *
 * Side effects:
 *  Sets the TCL result on error.
 *
 *----------------------------------------------------------------------
 */

static Tcl_Obj *
ExtractProcBody(interp, codePtr, envPtr)
    Tcl_Interp *interp;		/* the current interpreter */
    ByteCode *codePtr;		/* the ByteCode associated with this Proc */
    ExtractionEnv *envPtr;	/* the extraction environment */
{
    Proc *procPtr;
    Tcl_Obj *bodyPtr;
    int i;
    CompiledLocal *localPtr;

    /*
     * we need a bytecode Tcl_Obj to place in the proc. We bump its reference
     * count because there will be a reference to it in the Proc.
     * We also bump the reference count on the ByteCode because the object
     * contains a reference to it.
     */

    bodyPtr = Tcl_NewStringObj(noSourceCode, -1);
    Tcl_IncrRefCount(bodyPtr);
    bodyPtr->internalRep.otherValuePtr = (VOID *) codePtr;
    bodyPtr->typePtr = cmpByteCodeType;
    codePtr->refCount++;

    /*
     * allocate the proc struct and start populating it.
     * We initialize the reference count on the Proc to 0 because
     * ProcBodyNewObj will bump it when it creates a TclProProcBody Tcl_Obj.
     */

    procPtr = (Proc *) ckalloc(sizeof(Proc));
    procPtr->iPtr = NULL;
    procPtr->refCount = 0;
    procPtr->cmdPtr = NULL;
    procPtr->bodyPtr = bodyPtr;
    procPtr->numArgs = 0;
    procPtr->numCompiledLocals = 0;
    procPtr->firstLocalPtr = NULL;
    procPtr->lastLocalPtr = NULL;

    if ((ExtractInteger(interp, envPtr, &procPtr->numArgs) != TCL_OK)
	    || (ExtractInteger(interp, envPtr,
		    &procPtr->numCompiledLocals) != TCL_OK)) {
	goto cleanAndError;
    }

    /*
     * load the compiled locals info
     */

    for (i=0 ; i < procPtr->numCompiledLocals ; i++) {
	localPtr = ExtractCompiledLocal(interp, envPtr);
	if (!localPtr) {
	    goto cleanAndError;
	}

	if (procPtr->firstLocalPtr == NULL) {
	    procPtr->firstLocalPtr = procPtr->lastLocalPtr = localPtr;
	} else {
	    procPtr->lastLocalPtr->nextPtr = localPtr;
	    procPtr->lastLocalPtr = localPtr;
	}
    }

    return (*procBodyFactory)(procPtr);

    cleanAndError:
    (*procBodyCleanup)(procPtr);
    return NULL;
}

/*
 *----------------------------------------------------------------------
 *
 * ExtractCompiledLocal --
 *
 *  Creates a CompiledLocal struct, then populates it with data extracted
 *  from the extraction environment.
 *
 * Results:
 *  Returns a newly allocated CompiledLocal struct, NULL on error.
 *
 * Side effects:
 *  Modified the TCL result on error.
 *
 *----------------------------------------------------------------------
 */

static CompiledLocal *
ExtractCompiledLocal(interp, envPtr)
    Tcl_Interp *interp;		/* the current interpreter */
    ExtractionEnv *envPtr;	/* the extraction environment */
{
    char *curImagePtr;
    int i, nameLength, hasDef;
    CompiledLocal *localPtr;
    unsigned int bit, mask;

    /*
     * read the length of the name from the byte sequence header; we need to
     * do this so that we can allocate the CompiledLocal. Then, let's move
     * the extraction environment back to where it was at the start of the
     * call, so that we can call ExtractByteSequence.
     */

    curImagePtr = envPtr->curImagePtr;
    if (ExtractInteger(interp, envPtr, &nameLength) != TCL_OK) {
	return NULL;
    }
    envPtr->curImagePtr = curImagePtr;

    localPtr = (CompiledLocal *) ckalloc(sizeof(CompiledLocal)
	    - sizeof(localPtr->name)
	    + nameLength + 1);
    localPtr->nextPtr = NULL;
    localPtr->nameLength = nameLength;
    localPtr->defValuePtr = NULL;
    localPtr->flags = 0;
    localPtr->resolveInfo = NULL;

    if (ExtractByteSequence(interp, nameLength, envPtr,
	    (unsigned char *) &localPtr->name[0], nameLength) != TCL_OK) {
	ckfree((char *) localPtr);
	return NULL;
    }

    localPtr->name[nameLength] = 0;

    /*
     * extract the fields of the struct, then if necessary extract
     * the default value for the argument
     */

    if ((ExtractInteger(interp, envPtr, &localPtr->frameIndex) != TCL_OK)
	    || (ExtractInteger(interp, envPtr, &hasDef) != TCL_OK)
	    || (ExtractInteger(interp, envPtr, (int *) &mask) != TCL_OK)) {
	ckfree((char *) localPtr);
	return NULL;
    }

    /*
     * The flags are mapped to a bit sequence and written as an int.
     * We default to VAR_UNDEFINED for 8.4 when compiled prior to 8.4
     * to match the new defaults use in optimization of var lookup.
     */

#ifndef TCL_85_PLUS
    if ((envPtr->sig.tclMajorVersion == 8) &&
	    (envPtr->sig.tclMinorVersion < 4) &&
	    (tclMajorVersion == 8) && (tclMinorVersion >= 4)) {
	localPtr->flags = VAR_UNDEFINED;
    }
#endif
    bit = 1;
    for (i=0 ; i < varFlagsListSize ; i++) {
	if (mask & bit) {
	    localPtr->flags |= varFlagsList[i];
	}
	bit <<= 1;
    }

    /*
     * now we get the default value if any
     */

    if (hasDef) {
	Tcl_Obj *objPtr = ExtractObject(interp, envPtr);
	if (!objPtr) {
	    ckfree((char *) localPtr);
	    return NULL;
	}
	localPtr->defValuePtr = objPtr;
    }

    return localPtr;
}

/*
 *----------------------------------------------------------------------
 *
 * AppendErrorLocation --
 *
 *  Append a string showing the location where an error ws detected
 *  during extraction of the compiled file.
 *
 * Results:
 *  None.
 *
 * Side effects:
 *  Modifies the TCL result.
 *
 *----------------------------------------------------------------------
 */

static void
AppendErrorLocation(interp, envPtr)
    Tcl_Interp *interp;		/* the current interpreter */
    ExtractionEnv *envPtr;	/* the extraction environment */
{
    char *imagePtr, *imageEnd, *endPtr, *basePtr, *lastPtr, *p;
    char savedChar;

    /*
     * we append about 32 characters, give or take a few to make sure we
     * show a full TCL word at the end
     */

    basePtr = envPtr->curImagePtr;
    imagePtr = basePtr;
    imageEnd = envPtr->imageEnd;
    lastPtr = basePtr + 32;
    if (lastPtr > imageEnd) {
	lastPtr = imageEnd;
    }
    endPtr = lastPtr;

    for (;;) {
	p = FindEnd(imagePtr, imageEnd);
	if ((p <= imagePtr) || (p > lastPtr)) {
	    break;
	}
	endPtr = p;
	imagePtr = p + 1;
    }

    savedChar = *endPtr;
    *endPtr = '\0';

    Tcl_AppendResult(interp, " at or near \"", basePtr, "\"", (char *) NULL);
    *endPtr = savedChar;
}

/*
 *----------------------------------------------------------------------
 *
 * A85InitDecodeContext --
 *
 *  Initialize an A85DecodeContext struct.
 *
 * Results:
 *  None.
 *
 * Side effects:
 *  initializes the fields of the A85ContextStruct with appropriate values.
 *
 *----------------------------------------------------------------------
 */

static void
A85InitDecodeContext(numBytes, decodeBuf, ctxPtr)
    int numBytes;		/* how many bytes to decode */
    unsigned char *decodeBuf;	/* the buffer where the decoded bytes are
				 * to be placed */
    A85DecodeContext *ctxPtr;	/* pointer to the context to initialize */
{
    ctxPtr->bytesToDecode = numBytes;
    ctxPtr->curPtr = decodeBuf;
    ctxPtr->curChar = 0;
}

/*
 *----------------------------------------------------------------------
 *
 * A85DecodeByte --
 *
 *  Decodes a character read in from an ASCII85 encoded string.
 *  This procedure accumulates enough bytes to decode a tuple, then does the
 *  decoding and writes the bytes to the decode buffer pojnted to by the
 *  context.
 *
 * Results:
 *  A standard Tcl result value.
 *
 * Side effects:
 *  Adds to the error result in the interpreter in case of error.
 *
 *----------------------------------------------------------------------
 */

static int
A85DecodeByte(interp, code, ctxPtr)
    Tcl_Interp *interp;		/* the current interpreter */
    int code;			/* the code corresponding to the byte read in.
				 * This is either a value in the set (0, 84)
				 * or the special code A85_Z */
    A85DecodeContext *ctxPtr;	/* the decode context */
{
    int i;
    int *decodePtr;
    long int decodeWord;
    unsigned char *curPtr = ctxPtr->curPtr;

    if (code == A85_Z) {
	if (ctxPtr->curChar != 0) {
	    Tcl_AppendResult(interp, "malformed byte sequence", (char *) NULL);
	    return TCL_ERROR;
	}

	*curPtr = 0;
	curPtr += 1;

	*curPtr = 0;
	curPtr += 1;

	*curPtr = 0;
	curPtr += 1;

	*curPtr = 0;
	curPtr += 1;

	ctxPtr->bytesToDecode -= 4;
    } else {
	ctxPtr->decodeBuf[ctxPtr->curChar] = code;
	ctxPtr->curChar += 1;

	/*
	 * There are two cases here:
	 *  - if bytesToDecode > 4, then we expect that a full 5-tuple was
	 *    written to the encoded buffer.
	 *  - if bytesToDecode < 4, then the encoded buffer contains only
	 *    the first (bytesToDecode + 1) characters in the 5-tuple, since
	 *    the others can be reconstructed.
	 *
	 * Also, decoded bytes are stored in reverse order of their packing
	 * order, because that's how the encoder did it.
	 */

	if (ctxPtr->bytesToDecode >= 4) {
	    if (ctxPtr->curChar > 4) {
		/*
		 * The decode word was stored in base-85, least significant to
		 * most significant char
		 */

		decodePtr = &ctxPtr->decodeBuf[4];
		decodeWord = *decodePtr;
		for (i=1 ; i < 5 ; i++) {
		    decodePtr -= 1;
		    decodeWord = times85(decodeWord) + *decodePtr;
		}

		*curPtr = (int)(decodeWord & 0xff);
		curPtr += 1;

		*curPtr = (int)((decodeWord >> 8) & 0xff);
		curPtr += 1;

		*curPtr = (int)((decodeWord >> 16) & 0xff);
		curPtr += 1;

		*curPtr = (int)((decodeWord >> 24) & 0xff);
		curPtr += 1;

		ctxPtr->curChar = 0;
		ctxPtr->bytesToDecode -= 4;
	    }
	} else {
	    int i;
	    int bytesToDecode = ctxPtr->bytesToDecode;
	    if (ctxPtr->curChar > bytesToDecode) {
		/*
		 * reconstruct the missing characters, then extract the bytes
		 */

		for (i=bytesToDecode+1 ; i < 5 ; i++) {
		    ctxPtr->decodeBuf[i] = 0;
		}

		decodePtr = &ctxPtr->decodeBuf[4];
		decodeWord = *decodePtr;
		for (i=1 ; i < 5 ; i++) {
		    decodePtr -= 1;
		    decodeWord = times85(decodeWord) + *decodePtr;
		}

		*curPtr = (int)(decodeWord & 0xff);
		curPtr += 1;

		if (bytesToDecode > 1) {
		    *curPtr = (int)((decodeWord >> 8) & 0xff);
		    curPtr += 1;
		}

		if (bytesToDecode > 2) {
		    *curPtr = (int)((decodeWord >> 16) & 0xff);
		    curPtr += 1;
		}

		ctxPtr->curChar = 0;
		ctxPtr->bytesToDecode = 0;
	    }
	}
    }

    ctxPtr->curPtr = curPtr;
    return TCL_OK;
}

/*
 *----------------------------------------------------------------------
 *
 * FindEnd --
 *
 *	Skips to the end of the current word.
 *
 * Results:
 *	Returns a pointer to the first whitespace or null character
 *	past the end of the word.
 *
 * Side effects:
 *	None.
 *
 *----------------------------------------------------------------------
 */

static char *
FindEnd(first, last)
    char *first;		/* The first byte of the word. */
    char *last;			/* The last byte of the word. */
{
    char *p;
    for (p = first; p != last; p++) {
	if (isspace(UCHAR(*p))) {
	    break;
	}
    }
    return p;
}

/*
 *---------------------------------------------------------------------------
 *
 * OldHandlePreserve --
 *
 *	Declare an interest in the arbitrary malloc'd block associated
 *	with the handle.
 *
 * Results:
 *	The return value is the handle argument, with its ref count
 *	incremented.
 *
 * Side effects:
 *	For each call to TclHandlePreserve(), there should be a matching
 *	call to TclHandleRelease() when the caller is no longer interested
 *	in the malloc'd block associated with the handle.
 *
 *---------------------------------------------------------------------------
 */

static TclHandle
OldHandlePreserve(handle)
    TclHandle handle;		/* Declare an interest in the block of
				 * memory referenced by this handle. */
{
    OldHandleStruct *handlePtr;

    handlePtr = (OldHandleStruct *) handle;
#ifdef TCL_MEM_DEBUG
    if (handlePtr->refCount == 0x61616161) {
	panic("using previously disposed TclHandle %x", handlePtr);
    }
    if ((handlePtr->ptr != NULL)
	    && (handlePtr->ptr != handlePtr->ptr2)) {
	panic("someone has changed the block referenced by the handle %x\nfrom %x to %x",
		handlePtr, handlePtr->ptr2, handlePtr->ptr);
    }
#endif
    handlePtr->refCount++;

    return handle;
}
