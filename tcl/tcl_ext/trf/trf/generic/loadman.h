#ifndef TRF_LOADMANAGER_H
#define TRF_LOADMANAGER_H

/* -*- c -*-
 * loadman.h -
 *
 * internal definitions for loading of shared libraries required by Trf.
 *
 * Copyright (c) 1996-1999 Andreas Kupries (a.kupries@westend.com)
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
 * CVS: $Id: loadman.h,v 1.11 2008/12/11 19:04:25 andreas_kupries Exp $
 */

/*
 * The procedures defined here manage the loading of libraries required
 * by various glue-code for crytographic algorithms. Dependent on the
 * functionality requested more than one library will be tried before
 * giving up entirely.
 *
 * All following sections define a structure for each algorithm to fill
 * with the addresses of the functions required here, plus a procedure
 * to do the filling.
 */

#ifdef __cplusplus
extern "C" {
#endif

#include "transformInt.h"

#ifdef HAVE_MD2_H
#   ifdef OPENSSL_SUB
#       include <openssl/md2.h>
#   else
#       include <md2.h>
#   endif
#else
#   include "../compat/md2.h"
#endif

#ifdef HAVE_SHA_H
#   ifdef OPENSSL_SUB
#       include <openssl/sha.h>
#   else
#       include <sha.h>
#   endif
#else
#   include "../compat/sha.h"
#endif

#if defined(HAVE_MD5_H) && !defined(MD5_STATIC_BUILD)
#   ifdef OPENSSL_SUB
#       include <openssl/md5.h>
#   else
#       include <md5.h>
#   endif
#   ifdef HAVE_UNISTD_H
#       include <unistd.h>
#   endif
#else
#   include "../md5-crypt/md5.h"
#   include "../md5-crypt/trf_crypt.h"
#   define MD5_CTX struct md5_ctx
#endif


#ifdef TCL_STORAGE_CLASS
# undef TCL_STORAGE_CLASS
#endif
#ifdef BUILD_Trf
# define TCL_STORAGE_CLASS DLLEXPORT
#else
# define TCL_STORAGE_CLASS DLLIMPORT
#endif

/* Structures, variables and functions to load and access the functionality
 * required for MD2 and SHA1. Affected command in case of failure: md2, sha1.
 */

/* Structures containing the vectors to jump through to the implementation
 * of the functionality.
 */

typedef struct Md2Functions {
  long loaded;
  void (* init)   _ANSI_ARGS_ ((MD2_CTX* c));
  void (* update) _ANSI_ARGS_ ((MD2_CTX* c, unsigned char* data,
				unsigned long length));
  void (* final)  _ANSI_ARGS_ ((unsigned char* digest, MD2_CTX* c));
} md2Functions;

typedef struct Md5Functions {
  long loaded;
  void (* init)   __P ((MD5_CTX* c));
  void (* update) __P ((MD5_CTX* c, unsigned char* data,
				unsigned long length));
  void* (* final)  __P ((unsigned char* digest, MD5_CTX* c));

  const char* (* crypt) _ANSI_ARGS_ ((const char* key, const char* salt));

} md5Functions;

typedef struct Sha1Functions {
  long loaded;
  void (* init)   _ANSI_ARGS_ ((SHA_CTX* c));
  void (* update) _ANSI_ARGS_ ((SHA_CTX* c, unsigned char* data,
				unsigned long length));
  void (* final)  _ANSI_ARGS_ ((unsigned char* digest, SHA_CTX* c));
} sha1Functions;




/* Global variables containing the vectors declared above. 99% of the time they
 * are read, but during load a write is required, which has to be protected by
 * a mutex in case of a thread-enabled Tcl.
 */

EXTERN md2Functions  md2f;  /* THREADING: serialize initialization */
EXTERN md5Functions  md5f;  /* THREADING: serialize initialization */
EXTERN sha1Functions sha1f; /* THREADING: serialize initialization */


EXTERN int
TrfLoadMD2 _ANSI_ARGS_ ((Tcl_Interp *interp));

EXTERN int
TrfLoadMD5 _ANSI_ARGS_ ((Tcl_Interp *interp));

EXTERN int
TrfLoadSHA1 _ANSI_ARGS_ ((Tcl_Interp *interp));


#undef TCL_STORAGE_CLASS
#define TCL_STORAGE_CLASS DLLIMPORT

#ifdef __cplusplus
}
#endif /* C++ */
#endif /* TRF_LOADMANAGER_H */
