#ifndef TRF_INT_H
#define TRF_INT_H

/* -*- c -*-
 * transformInt.h - internal definitions
 *
 * Copyright (C) 1996, 1997 Andreas Kupries (a.kupries@westend.com)
 * All rights reserved.
 *
 * Permission is hereby granted, without written agreement and without
 * license or royalty fees, to use, copy, modify, and distribute this
 * software and its documentation for any purpose, provided that the
 * above copyright notice and the following two paragraphs appear in
 * all copies of this software.
 *
 * IN NO EVENT SHALL I BE LIABLE TO ANY PARTY FOR DIRECT, INDIRECT, SPECIAL,
 * INCIDENTAL, OR CONSEQUENTIAL DAMAGES ARISING OUT OF THE USE OF THIS
 * SOFTWARE AND ITS DOCUMENTATION, EVEN IF I HAVE BEEN ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 *
 * I SPECIFICALLY DISCLAIM ANY WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE.  THE SOFTWARE PROVIDED HEREUNDER IS ON AN "AS IS" BASIS, AND
 * I HAVE NO OBLIGATION TO PROVIDE MAINTENANCE, SUPPORT, UPDATES,
 * ENHANCEMENTS, OR MODIFICATIONS.
 *
 * CVS: $Id: transformInt.h,v 1.45 2008/12/11 19:04:25 andreas_kupries Exp $
 */

#ifdef __cplusplus
extern "C" {
#endif

#include "transform.h"
#include <errno.h>
#include <string.h>
#include <assert.h>

#ifndef ___MAKEDEPEND___
/*
 * Hide external references during runs of 'makedepend'.
 * It may fail on some systems, if the files are not installed.
 */
#ifdef MAC_TCL
#include "compat:dlfcn.h"
#include "compat:zlib.h"
#include "compat:bzlib.h"
#else
#ifdef HAVE_DLFCN_H
#   include <dlfcn.h>
#else
#   include "../compat/dlfcn.h"
#endif
#ifdef HAVE_zlibtcl_PACKAGE
#   include "zlibtcl.h"
#else
#   ifdef HAVE_ZLIB_H
#      include <zlib.h>
#   else
#      include "../compat/zlib.h"
#   endif
#endif
#endif
#ifdef HAVE_BZ2_H
#   include <bzlib.h>
#else
#   include "../compat/bzlib.h"
#endif
#ifdef HAVE_STDLIB_H
#   include <stdlib.h>
#else
#   include "../compat/stdlib.h"
#endif
#endif


/*
 * Ensure WORDS_BIGENDIAN is defined correcly:
 * Needs to happen here in addition to configure to work with fat compiles on
 * Darwin (where configure runs only once for multiple architectures).
 */

#ifdef HAVE_SYS_TYPES_H
#    include <sys/types.h>
#endif
#ifdef HAVE_SYS_PARAM_H
#    include <sys/param.h>
#endif
#ifdef BYTE_ORDER
#    ifdef BIG_ENDIAN
#        if BYTE_ORDER == BIG_ENDIAN
#            undef WORDS_BIGENDIAN
#            define WORDS_BIGENDIAN 1
#        endif
#    endif
#    ifdef LITTLE_ENDIAN
#        if BYTE_ORDER == LITTLE_ENDIAN
#            undef WORDS_BIGENDIAN
#        endif
#    endif
#endif



#ifdef TCL_STORAGE_CLASS
# undef TCL_STORAGE_CLASS
#endif
#ifdef BUILD_Trf
# define TCL_STORAGE_CLASS DLLEXPORT
#else
# define TCL_STORAGE_CLASS DLLIMPORT
#endif

/* 04/13/1999 Fileevent patch from Matt Newman <matt@novadigm.com>
 * Normally defined in tcl*Port.h
 */

#ifndef EWOULDBLOCK
#define EWOULDBLOCK EAGAIN
#endif

/* Debugging definitions.
 */

#ifdef _WIN32
#undef TRF_DEBUG
#endif

#ifdef TRF_DEBUG
extern int n;
#define BLNKS {int i; for (i=0;i<n;i++) printf (" "); }
#define IN n+=4
#define OT n-=4 ; if (n<0) {n=0;}
#define FL       fflush (stdout)
#define START(p)   BLNKS; printf ("Start %s...\n",#p); FL; IN
#define DONE(p)    OT ; BLNKS; printf ("....Done %s\n",#p); FL;
#define PRINT      BLNKS; printf
#define PRINTLN(a) BLNKS; printf ("%s\n", (a))
#define NPRINT     printf
#define PRTSTR(fmt,len,bytes) PrintString (fmt,len,bytes)
#define DUMP(len,bytes) DumpString (n, len, bytes)

extern void PrintString _ANSI_ARGS_ ((char* fmt, int len, char* bytes));
extern void DumpString  _ANSI_ARGS_ ((int level, int len, char* bytes));
#else
#define BLNKS
#define IN
#define OT
#define FL
#define START(p)
#define DONE(p)
#define PRINT  if (0) printf
#define NPRINT if (0) printf
#define PRINTLN(a)
#define PRTSTR(fmt,len,bytes)
#define DUMP(len,bytes)
#endif


/* Convenience macros
 * - construction of (nested) lists
 */

#define LIST_ADDOBJ(errlabel,list,o) \
  res = Tcl_ListObjAppendElement (interp, list, o); \
  if (res != TCL_OK) {                              \
    goto errlabel;                               \
  }

#define LIST_ADDSTR(el, list, str) \
  LIST_ADDOBJ (el, list, Tcl_NewStringObj (str, -1))

#define LIST_ADDFSTR(el, list, str, len) \
    LIST_ADDOBJ (el, list, Tcl_NewStringObj (str, len))

#define LIST_ADDINT(el, list, i) \
    LIST_ADDOBJ (el, list, Tcl_NewIntObj (i))


/* Define macro which is TRUE for tcl versions >= 8.1
 * Required as there are incompatibilities between 8.0 and 8.1
 */

#define GT81 ((TCL_MAJOR_VERSION > 8) || \
	      ((TCL_MAJOR_VERSION == 8) && \
	       (TCL_MINOR_VERSION >= 1)))

/* Define macro which is TRUE for tcl versions >= 8.3.2
 */

#define GT832 ((TCL_MAJOR_VERSION > 8) || \
	      ((TCL_MAJOR_VERSION == 8) && \
	       ((TCL_MINOR_VERSION > 3) || \
		((TCL_MINOR_VERSION == 3) && \
		 (TCL_RELEASE_LEVEL == TCL_FINAL_RELEASE) && \
		 (TCL_RELEASE_SERIAL >= 2)))))

#if ! (GT81)
/*
 * Tcl version 8.0.x don't export their 'panic' procedure. Here we
 * define the necessary interface and map it to the name exported and
 * used by the higher versions.
 */

EXTERN void
panic _ANSI_ARGS_ (TCL_VARARGS(CONST char*, format));

#undef  Tcl_Panic
#define Tcl_Panic panic
#endif

/*
 * A single structure of the type below is created and maintained for
 * every interpreter. Beyond the table of registered transformers it
 * contains version information about the interpreter used to switch
 * runtime behaviour.
 */

typedef struct _Trf_Registry_ {
  Tcl_HashTable* registry;        /* Table containing all registered
				   * transformers. */
#ifdef USE_TCL_STUBS
  int            patchVariant;   /* Defined only for versions of Tcl
				  * supporting stubs, and thus enable
				  * the extension to switch its
				  * runtime behaviour depending on the
				  * version of the core loading
				  * it. The possible values are
				  * defined below. This information is
				  * propagated into the state of
				  * running transformers as well. */
#endif
} Trf_Registry;

#ifdef USE_TCL_STUBS
#define PATCH_ORIG (0) /* Patch as used in 8.0.x and 8.1.x */
#define PATCH_82   (1) /* Patch as included into 8.2. Valid till 8.3.1 */
#define PATCH_832  (2) /* Patch as rewritten for 8.3.2 and beyond */
#endif


/*
 * A structure of the type below is created and maintained
 * for every registered transformer (and every interpreter).
 */

typedef struct _Trf_RegistryEntry_ {
  Trf_Registry*       registry;   /* Backpointer to the registry */

  Trf_TypeDefinition* trfType;    /* reference to transformer specification */
  Tcl_ChannelType*    transType;  /* reference to derived channel type
				   * specification */
  Tcl_Command         trfCommand; /* command associated to the transformer */
  Tcl_Interp*         interp;     /* interpreter the command is registered
				   * in. */
} Trf_RegistryEntry;


/*
 * Procedures to access the registry of transformers for a specified
 * interpreter. The registry is a hashtable mapping from transformer
 * names to structures of type 'Trf_RegistryEntry' (see above).
 */

EXTERN Trf_Registry*
TrfGetRegistry  _ANSI_ARGS_ ((Tcl_Interp* interp));

EXTERN Trf_Registry*
TrfPeekForRegistry _ANSI_ARGS_ ((Tcl_Interp* interp));

EXTERN int
Trf_Unregister _ANSI_ARGS_ ((Tcl_Interp*        interp,
			     Trf_RegistryEntry* entry));


/*
 * Procedures used by 3->4 encoders (uu, base64).
 */

EXTERN void TrfSplit3to4 _ANSI_ARGS_ ((CONST unsigned char* in,
				       unsigned char* out, int length));

EXTERN void TrfMerge4to3 _ANSI_ARGS_ ((CONST unsigned char* in,
				       unsigned char* out));

EXTERN void TrfApplyEncoding   _ANSI_ARGS_ ((unsigned char* buf, int length,
					     CONST char* map));

EXTERN int  TrfReverseEncoding _ANSI_ARGS_ ((unsigned char* buf, int length,
					     CONST char* reverseMap,
					     unsigned int padChar,
					     int* hasPadding));

/*
 * Definition of option information for message digests and accessor
 * to set of vectors processing these.
 */


typedef struct _TrfMDOptionBlock {
  int         behaviour; /* IMMEDIATE vs. ATTACH, got from checkProc */
  int         mode;      /* what to to with the generated hashvalue */

  char*       readDestination;	/* Name of channel (or global variable)
				 * to write the hash of read data to
				 * (mode = TRF_WRITE_HASH / ..TRANSPARENT) */
  char*       writeDestination;	/* Name of channel (or global variable)
				 * to write the hash of written data to
				 * (mode = TRF_WRITE_HASH / ..TRANSPARENT) */

  int        rdIsChannel; /* typeflag for 'readDestination',  true for a channel */
  int        wdIsChannel; /* typeflag for 'writeDestination', true for a channel */

  char*       matchFlag; /* Name of global variable to write the match-
			  * result into (TRF_ABSORB_HASH) */

  Tcl_Interp* vInterp;	/* Interpreter containing the variable named in
			 * 'matchFlag', or '*Destination'. */

  /* derived information */

  Tcl_Channel rdChannel;  /* Channel associated to 'readDestination' */
  Tcl_Channel wdChannel;  /* Channel associated to 'writeDestination' */
} TrfMDOptionBlock;

#define TRF_IMMEDIATE (1)
#define TRF_ATTACH    (2)

#define TRF_ABSORB_HASH (1)
#define TRF_WRITE_HASH  (2)
#define TRF_TRANSPARENT (3)

EXTERN Trf_OptionVectors*
TrfMDOptions _ANSI_ARGS_ ((void));





/*
 * Definition of option information for general transformation (reflect.c, ref_opt.c)
 * to set of vectors processing these.
 */

typedef struct _TrfTransformOptionBlock {
  int mode; /* operation to execute (transform for read or write) */

#if (TCL_MAJOR_VERSION >= 8)
  Tcl_Obj*       command; /* tcl code to execute for a buffer */
#else
  unsigned char* command; /* tcl code to execute for a buffer */
#endif
} TrfTransformOptionBlock;

/*#define TRF_UNKNOWN_MODE (0) -- transform.h */
#define TRF_WRITE_MODE (1)
#define TRF_READ_MODE  (2)



EXTERN Trf_OptionVectors*
TrfTransformOptions _ANSI_ARGS_ ((void));


/*
 * Definition of option information for ZIP compressor
 * + accessor to set of vectors processing them
 */

typedef struct _TrfZipOptionBlock {
  int mode;   /* compressor mode: compress/decompress */
  int level;  /* compression level (1..9, -1 = default) */
  int nowrap; /* pkzip-compatibility (0..1, 0 = default) */
} TrfZipOptionBlock;

EXTERN Trf_OptionVectors*
TrfZIPOptions _ANSI_ARGS_ ((void));

/*
 * Definition of option information for BZ2 compressor
 * + accessor to set of vectors processing them
 */

typedef struct _TrfBz2OptionBlock {
  int mode;   /* compressor mode: compress/decompress */
  int level;  /* compression level (1..9, 9 = default) */
} TrfBz2OptionBlock;

EXTERN Trf_OptionVectors*
TrfBZ2Options _ANSI_ARGS_ ((void));

#define TRF_COMPRESS   (1)
#define TRF_DECOMPRESS (2)

#define TRF_MIN_LEVEL      (1)
#define TRF_MAX_LEVEL      (9)
#define TRF_DEFAULT_LEVEL (-1)

#define TRF_MIN_LEVEL_STR "1"
#define TRF_MAX_LEVEL_STR "9"

#ifndef WINAPI
#define WINAPI
#endif

#ifdef ZLIB_STATIC_BUILD
#undef  ZEXPORT
#define ZEXPORT
#else
#undef  ZEXPORT
#define ZEXPORT WINAPI
#endif
#ifdef HAVE_zlibtcl_PACKAGE
#undef  ZEXPORT
#define ZEXPORT
#endif

/*
 * 'zlib' will be dynamically loaded. Following a structure to
 * contain the addresses of all functions required by this extension.
 *
 * Affected commands are: zip, adler, crc-zlib.
 * They will fail, if the library could not be loaded.
 */

typedef struct ZFunctions {
  VOID *handle;
  int (ZEXPORT * zdeflate)           _ANSI_ARGS_ ((z_streamp strm, int flush));
  int (ZEXPORT * zdeflateEnd)        _ANSI_ARGS_ ((z_streamp strm));

  int (ZEXPORT * zdeflateInit2_)     _ANSI_ARGS_ ((z_streamp strm, int level,
						  int method, int windowBits,
						  int memLevel, int strategy,
						  CONST char *version,
						 int stream_size));
  int (ZEXPORT * zdeflateReset)      _ANSI_ARGS_ ((z_streamp strm));
  int (ZEXPORT * zinflate)           _ANSI_ARGS_ ((z_streamp strm, int flush));
  int (ZEXPORT * zinflateEnd)        _ANSI_ARGS_ ((z_streamp strm));
  int (ZEXPORT * zinflateInit2_)     _ANSI_ARGS_ ((z_streamp strm,
						  int windowBits,
						  CONST char *version,
						  int stream_size));
  int (ZEXPORT * zinflateReset)      _ANSI_ARGS_ ((z_streamp strm));
  unsigned long (ZEXPORT * zadler32) _ANSI_ARGS_ ((unsigned long adler,
						  CONST unsigned char *buf,
						  unsigned int len));
  unsigned long (ZEXPORT * zcrc32)   _ANSI_ARGS_ ((unsigned long crc,
						  CONST unsigned char *buf,
						  unsigned int len));
} zFunctions;


EXTERN zFunctions zf; /* THREADING: serialize initialization */

EXTERN int
TrfLoadZlib _ANSI_ARGS_ ((Tcl_Interp *interp));

/*
 * 'libbz2' will be dynamically loaded. Following a structure to
 * contain the addresses of all functions required by this extension.
 *
 * Affected commands are: bzip.
 * They will fail, if the library could not be loaded.
 */

#ifdef BZLIB_STATIC_BUILD
#undef  BZEXPORT
#define BZEXPORT
#else
#undef  BZEXPORT
#define BZEXPORT WINAPI
#endif

typedef struct BZFunctions {
  VOID *handle;
  int (BZEXPORT * bcompress)           _ANSI_ARGS_ ((bz_stream* strm,
						    int action));
  int (BZEXPORT * bcompressEnd)        _ANSI_ARGS_ ((bz_stream* strm));
  int (BZEXPORT * bcompressInit)       _ANSI_ARGS_ ((bz_stream* strm,
						    int blockSize100k,
						    int verbosity,
						    int workFactor));
  int (BZEXPORT * bdecompress)         _ANSI_ARGS_ ((bz_stream* strm));
  int (BZEXPORT * bdecompressEnd)      _ANSI_ARGS_ ((bz_stream* strm));
  int (BZEXPORT * bdecompressInit)     _ANSI_ARGS_ ((bz_stream* strm,
						    int verbosity, int small));
} bzFunctions;


EXTERN bzFunctions bz; /* THREADING: serialize initialization */

EXTERN int
TrfLoadBZ2lib _ANSI_ARGS_ ((Tcl_Interp *interp));

/*
 * The following definitions have to be usable for 8.0.x, 8.1.x, 8.2.x,
 * 8.3.[01], 8.3.2 and beyond. The differences between these versions:
 *
 * 8.0.x:      Trf usable only if core is patched, to check at compile time
 *             (Check = Fails to compile, for now).
 *
 * 8.1:        Trf usable with unpatched core, but restricted, check at
 *             compile time for missing definitions, check at runtime to
 *             disable the missing features.
 *
 * 8.2.x:      Changed semantics for Tcl_StackChannel (Tcl_ReplaceChannel).
 * 8.3.[01]:   Check at runtime to switch the behaviour. The patch is part
 *             of the core from now on.
 *
 * 8.3.2+:     Stacked channels rewritten for better behaviour in some
 *             situations (closing). Some new API's, semantic changes.
 */

#ifdef USE_TCL_STUBS
#ifndef Tcl_StackChannel
/* The core we are compiling against is not patched, so supply the
 * necesssary definitions here by ourselves. The form chosen for
 * the procedure macros (reservedXXX) will notify us if the core
 * does not have these reserved locations anymore.
 *
 * !! Synchronize the procedure indices in their definitions with
 *    the patch to tcl.decls, as they have to be the same.
 */

/* 281 */
typedef Tcl_Channel (trf_StackChannel) _ANSI_ARGS_((Tcl_Interp* interp,
						    Tcl_ChannelType* typePtr,
						    ClientData instanceData,
						    int mask,
						    Tcl_Channel prevChan));
/* 282 */
typedef void (trf_UnstackChannel) _ANSI_ARGS_((Tcl_Interp* interp,
					       Tcl_Channel chan));

#define Tcl_StackChannel     ((trf_StackChannel*) tclStubsPtr->reserved281)
#define Tcl_UnstackChannel ((trf_UnstackChannel*) tclStubsPtr->reserved282)

#endif /* Tcl_StackChannel */


#ifndef Tcl_GetStackedChannel
/*
 * Separate definition, available in 8.2, but not 8.1 and before !
 */

/* 283 */
typedef Tcl_Channel (trf_GetStackedChannel) _ANSI_ARGS_((Tcl_Channel chan));

#define Tcl_GetStackedChannel ((trf_GetStackedChannel*) tclStubsPtr->reserved283)

#endif /* Tcl_GetStackedChannel */


#ifndef Tcl_WriteRaw
/* Core is older than 8.3.2., so supply the missing definitions for
 * the new API's in 8.3.2.
 */

/* 394 */
typedef int (trf_ReadRaw)  _ANSI_ARGS_((Tcl_Channel chan,
					char*       dst,
					int         bytesToRead));
/* 395 */
typedef int (trf_WriteRaw) _ANSI_ARGS_((Tcl_Channel chan,
					char*       src,
					int         srcLen));

/* 396 */
typedef int (trf_GetTopChannel) _ANSI_ARGS_((Tcl_Channel chan));

/* 397 */
typedef int (trf_ChannelBuffered) _ANSI_ARGS_((Tcl_Channel chan));

/*
 * Generating code for accessing these parts of the stub table when
 * compiling against a core older than 8.3.2 is a hassle because even
 * the 'reservedXXX' fields of the structure are not defined yet. So
 * we have to write up some macros hiding some very hackish pointer
 * arithmetics to get at these fields. We assume that pointer to
 * functions are always of the same size.
 */

#define STUB_BASE   ((char*)(&(tclStubsPtr->tcl_UtfNcasecmp))) /* field 370 */
#define procPtrSize (sizeof (Tcl_DriverBlockModeProc *))
#define IDX(n)      (((n)-370) * procPtrSize)
#define SLOT(n)     (STUB_BASE + IDX (n))

#define Tcl_ReadRaw         (*((trf_ReadRaw**)         (SLOT (394))))
#define Tcl_WriteRaw        (*((trf_WriteRaw**)        (SLOT (395))))
#define Tcl_GetTopChannel   (*((trf_GetTopChannel**)   (SLOT (396))))
#define Tcl_ChannelBuffered (*((trf_ChannelBuffered**) (SLOT (397))))

/*
#define Tcl_ReadRaw         ((trf_ReadRaw*)         tclStubsPtr->reserved394)
#define Tcl_WriteRaw        ((trf_WriteRaw*)        tclStubsPtr->reserved395)
#define Tcl_GetTopChannel   ((trf_GetTopChannel*)   tclStubsPtr->reserved396)
#define Tcl_ChannelBuffered ((trf_ChannelBuffered*) tclStubsPtr->reserved397)
*/

/* Always required, easy emulation.
 */
#define Tcl_ChannelWatchProc(chanDriver)     ((chanDriver)->watchProc)
#define Tcl_ChannelSetOptionProc(chanDriver) ((chanDriver)->setOptionProc)
#define Tcl_ChannelGetOptionProc(chanDriver) ((chanDriver)->getOptionProc)
#define Tcl_ChannelSeekProc(chanDriver)      ((chanDriver)->seekProc)

#endif /* Tcl_WriteRaw */
#endif /* USE_TCL_STUBS */


/*
 * Internal initialization procedures for all transformers implemented here.
 */

EXTERN int TrfInit_Bin       _ANSI_ARGS_ ((Tcl_Interp* interp));
EXTERN int TrfInit_Oct       _ANSI_ARGS_ ((Tcl_Interp* interp));
EXTERN int TrfInit_Hex       _ANSI_ARGS_ ((Tcl_Interp* interp));
EXTERN int TrfInit_UU        _ANSI_ARGS_ ((Tcl_Interp* interp));
EXTERN int TrfInit_B64       _ANSI_ARGS_ ((Tcl_Interp* interp));
EXTERN int TrfInit_Ascii85   _ANSI_ARGS_ ((Tcl_Interp* interp));
EXTERN int TrfInit_OTP_WORDS _ANSI_ARGS_ ((Tcl_Interp* interp));
EXTERN int TrfInit_QP        _ANSI_ARGS_ ((Tcl_Interp* interp));

EXTERN int TrfInit_CRC       _ANSI_ARGS_ ((Tcl_Interp* interp));
EXTERN int TrfInit_MD5       _ANSI_ARGS_ ((Tcl_Interp* interp));
EXTERN int TrfInit_MD2       _ANSI_ARGS_ ((Tcl_Interp* interp));
EXTERN int TrfInit_HAVAL     _ANSI_ARGS_ ((Tcl_Interp* interp));
EXTERN int TrfInit_SHA       _ANSI_ARGS_ ((Tcl_Interp* interp));
EXTERN int TrfInit_SHA1      _ANSI_ARGS_ ((Tcl_Interp* interp));
EXTERN int TrfInit_OTP_SHA1  _ANSI_ARGS_ ((Tcl_Interp* interp));
EXTERN int TrfInit_ADLER     _ANSI_ARGS_ ((Tcl_Interp* interp));
EXTERN int TrfInit_CRC_ZLIB  _ANSI_ARGS_ ((Tcl_Interp* interp));
EXTERN int TrfInit_RIPEMD128 _ANSI_ARGS_ ((Tcl_Interp* interp));
EXTERN int TrfInit_RIPEMD160 _ANSI_ARGS_ ((Tcl_Interp* interp));
EXTERN int TrfInit_OTP_MD5   _ANSI_ARGS_ ((Tcl_Interp* interp));

EXTERN int TrfInit_RS_ECC    _ANSI_ARGS_ ((Tcl_Interp* interp));
EXTERN int TrfInit_ZIP       _ANSI_ARGS_ ((Tcl_Interp* interp));
EXTERN int TrfInit_BZ2       _ANSI_ARGS_ ((Tcl_Interp* interp));

EXTERN int TrfInit_Info      _ANSI_ARGS_ ((Tcl_Interp* interp));
EXTERN int TrfInit_Unstack   _ANSI_ARGS_ ((Tcl_Interp* interp));
EXTERN int TrfInit_Binio     _ANSI_ARGS_ ((Tcl_Interp* interp));

EXTERN int TrfInit_Transform _ANSI_ARGS_ ((Tcl_Interp* interp));
EXTERN int TrfInit_Crypt     _ANSI_ARGS_ ((Tcl_Interp* interp));



/* Compile time distinctions between various versions of Tcl.
 */

/* Do we support locking ? Has to be a version of 8.1 or
 * beyond with threading enabled.
 */

#if GT81 && defined (TCL_THREADS) /* THREADING: Lock procedures */

EXTERN void TrfLockIt   _ANSI_ARGS_ ((void));
EXTERN void TrfUnlockIt _ANSI_ARGS_ ((void));

#define TrfLock   TrfLockIt ()
#define TrfUnlock TrfUnlockIt ()
#else
/* Either older version of Tcl, or non-threaded 8.1.x.
 * Whatever, locking is not required, undefine the calls.
 */

#define TrfLock
#define TrfUnlock
#endif

/* Tcl 8.1 and beyond have better support for binary data. We have to
 * use that to avoid mangling information going through the
 * transformations.
 */

#if GT81
#define GET_DATA(in,len) (unsigned char*) Tcl_GetByteArrayFromObj ((in), (len))
#define NEW_DATA(r)      Tcl_NewByteArrayObj ((r).buf, (r).used);
#else
#define GET_DATA(in,len) (unsigned char*) Tcl_GetStringFromObj ((in), (len))
#define NEW_DATA(r)      Tcl_NewStringObj ((char*) (r).buf, (r).used)
#endif

/* Map the names of some procedures from the stubs-variant to their
 * pre-stubs names.
 */

#ifndef USE_TCL_STUBS
#define Tcl_UnstackChannel Tcl_UndoReplaceChannel
#define Tcl_StackChannel   Tcl_ReplaceChannel
#endif

/* Define the code to 'provide' this package to the loading interpreter.
 */

#if !(GT81)
#define PROVIDE(interp,stubs) Tcl_PkgProvide ((interp), PACKAGE_NAME, PACKAGE_VERSION);
#else
#ifndef __WIN32__
#define PROVIDE(interp,stubs) \
    Tcl_PkgProvideEx ((interp), PACKAGE_NAME, PACKAGE_VERSION, (ClientData) &(stubs)); \
    Trf_InitStubs    ((interp), PACKAGE_VERSION, 0);
#else
#define PROVIDE(interp,stubs) Tcl_PkgProvideEx ((interp), PACKAGE_NAME, PACKAGE_VERSION, (ClientData) &(stubs));
#endif
#endif


#include "trfIntDecls.h"

#undef TCL_STORAGE_CLASS
#define TCL_STORAGE_CLASS DLLIMPORT

#ifdef __cplusplus
}
#endif /* C++ */
#endif /* TRF_INT_H */
