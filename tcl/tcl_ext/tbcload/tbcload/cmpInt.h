/*
 * cmpInt.h --
 *
 *  Internal header file for the Compiler/Loader package.
 *  This header defines a number of macros that are used by both the writer
 *  and reader package to initialize some static variables. We use macros
 *  because the writer and the reader are two separate packages, and we don't
 *  want to share code between the two.
 *
 * Copyright (c) 1998-2000 Ajuba Solutions
 * Copyright (c) 2002      ActiveState SRL
 *
 * See the file "license.terms" for information on usage and redistribution
 * of this file, and for a DISCLAIMER OF ALL WARRANTIES.
 *
 * RCS: @(#) $Id: cmpInt.h,v 1.6 2002/12/02 17:42:02 andreas_kupries Exp $
 */

#ifndef _CMPINT_H
# define _CMPINT_H

# include "tclInt.h"
# include "tclPort.h"
# include "tclCompile.h"

/*
 * Activate features specific to 8.5 and higher.
 * JumpTableInfo AuxData (for compiled 'switch').
 */

#if (TCL_MAJOR_VERSION > 8) || ((TCL_MAJOR_VERSION == 8) && (TCL_MINOR_VERSION >= 5))
#define TCL_85_PLUS
#endif

/*
 * USE_CATCH_WRAPPER controls whether the emitted code has a catch around
 * the call to loader::bceval and code to strip off the additional back trace
 * from the error info
 */
# define USE_CATCH_WRAPPER 0

/*
 * When version numbers change here, must also go into the following files
 * and update the version numbers:
 *
 * src/configure.in
 * src/makefile.bc
 * src/makefile.vc
 *
 * The release level should be  0 for alpha, 1 for beta, and 2 for
 * final/patch.  The release serial value is the number that follows the
 * "a", "b", or "p" in the patch level; for example, if the patch level
 * is 7.6b2, TCL_RELEASE_SERIAL is 2.  It restarts at 1 whenever the
 * release level is changed, except for the final release which is 0
 * (the first patch will start at 1).
 */

# define CMP_MAJOR_VERSION   1
# define CMP_MINOR_VERSION   7
# define CMP_RELEASE_LEVEL   0
# define CMP_RELEASE_SERIAL  2

# define CMP_VERSION	     "1.7"
# define CMP_PATCH_LEVEL     "1.7.0"

/*
 * This was the version number of the .tbc file format that is current.
 * This must be set dynamically as it depends on the interpreter we are
 * tied to (8.3 or earlier == 1, 8.4+ == 2).
# define CMP_CURRENT_VERSION_NUMBER 1
 */

/*
 * This macro includes code that emits and reads the location map for a
 * ByteCode struct. The location map is useful only if the source is shipped
 * (which does not happen currently), but we need to populate a location
 * map in the ByteCode because some code in TCL core needs (for example, the
 * code that generates errorInfo uses this map to find the source of the
 * command corresponding to the pc that caused an exception).
 *
 * If EMIT_SRCMAP is 1, both the code arrays and the source arrays from the
 * location map are emitted and extracted.
 * If it is 0, only the code arrays are emitted; the source arrays are
 * generated using the dummy source noSourceCode.
 */
# define EMIT_SRCMAP 0

/*
 * structure to hold the calculated lengths of the location information
 * arrays for a ByteCode structure
 */
typedef struct LocMapSizes {
    int codeDeltaSize;		/* size of the codeDeltaStart array */
    int codeLengthSize;		/* size of the codeLengthStart array */
    int srcDeltaSize;		/* size of the srcDeltaStart array */
    int srcLengthSize;		/* size of the srcLengthStart array */
} LocMapSizes;

/*
 * Map between ExceptionRangeType enums and type codes
 */
typedef struct ExcRangeMap {
    ExceptionRangeType type;	/* The TCL enum for a given exception range
                                 * type */
    char name;			/* and its corresponding code */
} ExcRangeMap;

/*
 * An InstLocList structure holds the location in the bytecode of a PUSH
 * instruction.
 * It is used to keep track of a few different things:
 *	- the beginning of a call to "proc"
 *	- instructions that push a given object.
 *
 * This struct is exported for use by the compiler test package, otherwise
 * it could be kept local to the writer.
 */

typedef struct InstLocList {
    struct InstLocList *next;	/* next proc location in the list */
    int bytecodeOffset;		/* offset to the fist byte in the
                                 * instruction */
    int commandIndex;		/* the command to which this instruction
                                 * belongs */
} InstLocList;

/*
 * A ProcBodyInfo structure holds the information we need to
 * postprocess a procedure body.  If the indices are set to -1, then
 * the step that populated the struct detected that the body should
 * not be compiled.
 *
 * This struct is exported for use by the compiler test package, otherwise
 * it could be kept local to the writer.
 */

typedef struct ProcBodyInfo {
    int nameIndex;		/* index in the object table of the object
                                 * containing the name of the proc */
    int argsIndex;		/* index in the object table of the object
                                 * containing the argument list for the proc */
    int bodyOrigIndex;		/* the original index in the object table of
                                 * the object containing the body of the
                                 * procedure */
    int bodyNewIndex;		/* the new index in the object table of the
                                 * object containing the body of the procedure.
                                 * The index is different from the original if
                                 * the object had been shared */
    int procOffset;		/* offset to the location in the bytecodes
                                 * where the "proc" string is pushed on the
                                 * stack. This is the start of the instruction
                                 * group for a proc command execution */
    int bodyOffset;		/* offset to the location in the bytecodes
                                 * where this procedure body is pushed on the
                                 * stack */
    int commandIndex;		/* the command number for this proc; values
                                 * start at 0 for the first command in the
                                 * script. */
} ProcBodyInfo;

/*
 * The PostProcessInfo struct holds compilation info used by the compiler
 * to postprocess the compiled proc body.
 * The counters numProcs, numCompiledBodies, and numUnshared are on a
 * compilation by compilation basis (they refer to the current compilation),
 * whereas the counter in the CompilerContext struct defined below are
 * cumulative for all compilations.
 *
 * This struct is exported for use by the compiler test package, otherwise
 * it could be kept local to the writer.
 */

typedef struct PostProcessInfo {
    struct InstLocList *procs;	/* the list of proc locations */
    int numProcs;		/* how many entries in the list */
    Tcl_HashTable objTable;	/* this hash table is keyed by object
                                 * index and is used to store information
                                 * about references to this object. */
    ProcBodyInfo **infoArrayPtr;/* NULL-terminated array to pointers of
                                 * info structs that are generated for
                                 * each proc at the start of the post
                                 * processing step */
    int numCompiledBodies;	/* total number of procedure bodies that
                                 * were compiled. Not all procedure
                                 * bodies are compiled. */
    int numUnshares;		/* total number of unshares that were
                                 * performed. If 0, then there were no
                                 * shared procedure bodies */
} PostProcessInfo;

/*
 * The CompilerContext struct holds context for use by the compiler
 * code. It contains a pointer to the PostProcessInfo, counters for various
 * statistics, etc...
 * There is one such struct per interpreter.
 *
 * This struct is exported for use by the compiler test package, otherwise
 * it could be kept local to the writer.
 */

typedef struct CompilerContext {
    PostProcessInfo *ppi;	/* post-processing context for the currently
                                 * active compilation */
    int numProcs;		/* how many proc commands were seen in the
                                 * compiled script */
    int numCompiledBodies;	/* how many proc bodies were compiled */
    int numUnsharedBodies;	/* how many were unshared */
    int numUnshares;		/* how many copies were made when unsharing
                                 * proc bodies */
} CompilerContext;

/*
 * This is the start of the signature line
 */
# define CMP_SIGNATURE_HEADER "TclPro ByteCode"

/*
 * Default extension for compiled TCL files
 */
# define CMP_TC_EXTENSION ".tbc"

/*
 * Name of the eval command exported by the Loader package
 */
# define CMP_EVAL_COMMAND "bceval"

/*
 * Name of the proc command exported by the Loader package
 */
# define CMP_PROC_COMMAND "bcproc"

/*
 * Name of the writer (compiler) and reader (loader) packages
 */
# define CMP_WRITER_PACKAGE "compiler"
# define CMP_READER_PACKAGE "tbcload"

# if USE_CATCH_WRAPPER
/*
 * Marker string appended by Loader_EvalObjCmd to the errorInfo, for use by
 * the catch code to strip out error info that we don't want.
 */
#  define CMP_ERRORINFO_MARKER "----------####----------"
# endif

/*
 * The name of the Tcl_ObjType for objects that hold procedure body info.
 * _OLD is for 8.0.3 interps, _NEW for 8.0.4 and later.
 */
# define CMP_PROCBODY_OBJ_TYPE_NEW "procbody"
# define CMP_PROCBODY_OBJ_TYPE_OLD "TclProProcBody"

/*
 * The one-letter codes for various object types.
 * CMP_STRING_TYPE is an uncompressed/unencoded string,
 * CMP_XSTRING_TYPE is compressed/encoded
 */
# define CMP_INT_CODE			'i'
# define CMP_DOUBLE_CODE		'd'
# define CMP_STRING_CODE		's'
# define CMP_XSTRING_CODE		'x'
# define CMP_PROCBODY_CODE		'p'
# define CMP_BOOLEAN_CODE		'b'
# define CMP_BYTECODE_CODE		'c'

/*
 * The one-letter codes for the exception range types
 */
# define CMP_LOOP_EXCEPTION_RANGE	'L'
# define CMP_CATCH_EXCEPTION_RANGE	'C'

/*
 * The one-letter codes for the AuxData types range types
 */
# define CMP_FOREACH_INFO		'F'
#ifdef TCL_85_PLUS
# define CMP_JUMPTABLE_INFO		'J'
#endif

/*
 * the following set of procedures needs to be wrapped around a DLLEXPORT
 * macro setup, because they are exported by the Tbcload DLL
 */

# ifdef BUILD_tbcload
#  undef TCL_STORAGE_CLASS
#  define TCL_STORAGE_CLASS DLLEXPORT
# endif

/*
 *---------------------------------------------------------------
 * Procedures exported by the proc body object support, used internally by
 * both compiler and loader.
 * They are defined in the loader DLL, and exported internally to the
 * compiler.
 *---------------------------------------------------------------
 */

EXTERN void	ProcBodyCleanupProc _ANSI_ARGS_((Proc *procPtr));
EXTERN Tcl_Obj *ProcBodyNewObj _ANSI_ARGS_((Proc *procPtr));
EXTERN void	ProcBodyRegisterTypes _ANSI_ARGS_((void));

/*
 *----------------------------------------------------------------
 * Procedures exported by cmpRead.c and cmpRPkg.c
 *----------------------------------------------------------------
 */

EXTERN int	TbcloadInit _ANSI_ARGS_((Tcl_Interp *interp));

# undef TCL_STORAGE_CLASS
# define TCL_STORAGE_CLASS DLLIMPORT

/*
 *----------------------------------------------------------------
 * Procedures exported for use by the test package
 *----------------------------------------------------------------
 */

EXTERN CONST char *
		CmptestGetPackageName _ANSI_ARGS_(());
EXTERN int	Cmptest_Init _ANSI_ARGS_((Tcl_Interp *interp));

/*
 *----------------------------------------------------------------
 * Procedures exported by cmpWrite.c and cmpWPkg.c
 *----------------------------------------------------------------
 */

#undef TCL_STORAGE_CLASS
#ifdef BUILD_tclcompiler
#   define TCL_STORAGE_CLASS DLLEXPORT
#else
#   define TCL_STORAGE_CLASS DLLIMPORT
#endif

/* GetContext exported for use by Test package. */
EXTERN CompilerContext *
		CompilerGetContext _ANSI_ARGS_((Tcl_Interp *interp));

EXTERN void	CompilerInit _ANSI_ARGS_((Tcl_Interp *interp));


#undef TCL_STORAGE_CLASS
#define TCL_STORAGE_CLASS DLLIMPORT

#endif /* _CMPINT_H */
