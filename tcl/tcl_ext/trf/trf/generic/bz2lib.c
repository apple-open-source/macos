/*
 * bz2lib.c --
 *
 *	Loader for 'bz2' compression library.
 *
 * Copyright (c) 1996 Jan Nijtmans (Jan.Nijtmans.wxs.nl)
 * All rights reserved.
 *
 * CVS: $Id: bz2lib.c,v 1.5 2008/12/11 19:04:25 andreas_kupries Exp $
 */

#include "transformInt.h"

#ifndef BZ2_LIB_NAME
#    ifdef __WIN32__
#    define BZ2_LIB_NAME "libbz2.dll"
#    endif /* __WIN32__ */
#    ifdef __APPLE__
#    define BZ2_LIB_NAME "libbz2.dylib"
#    endif /* __APPLE__ */
#    ifndef BZ2_LIB_NAME
#    define BZ2_LIB_NAME "libbz2.so"
#    endif /* BZ2_LIB_NAME */
#endif /* BZ2_LIB_NAME */


static char* symbols [] = {
  "BZ2_bzCompress",
  "BZ2_bzCompressEnd",
  "BZ2_bzCompressInit",
  "BZ2_bzDecompress",
  "BZ2_bzDecompressEnd",
  "BZ2_bzDecompressInit",
  (char *) NULL
};


/*
 * Global variable containing the vectors into the 'bz2'-library.
 */

#ifdef BZLIB_STATIC_BUILD
bzFunctions bz = {
  0,
  bzCompress,
  bzCompressEnd,
  bzCompressInit,
  bzDecompress,
  bzDecompressEnd,
  bzDecompressInit,
};
#else
bzFunctions bz = {0}; /* THREADING: serialize initialization */
#endif


int
TrfLoadBZ2lib (interp)
    Tcl_Interp* interp;
{
#ifndef BZLIB_STATIC_BUILD
  int res;

  TrfLock; /* THREADING: serialize initialization */

  res = Trf_LoadLibrary (interp, BZ2_LIB_NAME, (VOID**) &bz, symbols, 6);
  TrfUnlock;

  return res;
#else
  return TCL_OK;
#endif
}
