/*
 * loadman.c --
 *
 *	Loader for various crypto libraries.
 *
 * Copyright (c) 1997 Andreas Kupries (a.kupries@westend.com)
 * All rights reserved.
 *
 * CVS: $Id: loadman.c,v 1.10 2002/11/05 00:20:20 andreas_kupries Exp $
 */

#include "loadman.h"

/*
 * Allow the Makefile to define this value
 */

#ifndef SSL_LIB_NAME
#    ifdef __WIN32__
#    define SSL_LIB_NAME "crypto32.dll"
#    endif /* __WIN32__ */
#    ifndef SSL_LIB_NAME
#    define SSL_LIB_NAME "libcrypto.so"
#    endif /* SSL_LIB_NAME */
#endif /* SSL_LIB_NAME */


#ifndef CRYPT_LIB_NAME
#    ifdef __WIN32__
#    define CRYPT_LIB_NAME "crypt.dll"
#    endif /* __WIN32__ */
#    ifndef CRYPT_LIB_NAME
#    define CRYPT_LIB_NAME "libcrypt.so"
#    endif /* SSL_LIB_NAME */
#endif /* SSL_LIB_NAME */


typedef struct SslLibFunctions {
  void* handle;
  /* MD2 */
  void (* md2_init)        _ANSI_ARGS_ ((MD2_CTX* c));
  void (* md2_update)      _ANSI_ARGS_ ((MD2_CTX* c, unsigned char* data, unsigned long length));
  void (* md2_final)       _ANSI_ARGS_ ((unsigned char* digest, MD2_CTX* c));
  /* SHA1 */
  void (* sha1_init)        _ANSI_ARGS_ ((SHA_CTX* c));
  void (* sha1_update)      _ANSI_ARGS_ ((SHA_CTX* c, unsigned char* data, unsigned long length));
  void (* sha1_final)       _ANSI_ARGS_ ((unsigned char* digest, SHA_CTX* c));
} sslLibFunctions;


static char* ssl_symbols [] = {
  /* md2 */
  "MD2_Init",
  "MD2_Update",
  "MD2_Final",
  /* sha1 */
  "SHA1_Init",
  "SHA1_Update",
  "SHA1_Final",
  /* -- */
  (char *) NULL,
};

static char* crypt_symbols [] = {
  /* md5 */
  "md5_init_ctx",
  "md5_process_bytes",
  "md5_finish_ctx",
  "crypt",
  /* -- */
  (char *) NULL,
};




/*
 * Global variables containing the vectors to DES, MD2, ...
 */

md2Functions  md2f  = {0}; /* THREADING: serialize initialization */
sha1Functions sha1f = {0}; /* THREADING: serialize initialization */
md5Functions  md5f  = {0}; /* THREADING: serialize initialization */

#ifdef MD5_STATIC_BUILD
#include "../md5-crypt/md5.h" /* THREADING: import of one constant var, read-only => safe */
#endif

/*
 * Internal global var's, contains all vectors loaded from SSL's 'cryptlib'.
 *                        contains all vectors loaded from 'libdes' library.
 */

static sslLibFunctions ssl; /* THREADING: serialize initialization */

/*
 *------------------------------------------------------*
 *
 *	TrfLoadMD2 --
 *
 *	------------------------------------------------*
 *	Makes MD2 functionality available.
 *	------------------------------------------------*
 *
 *	Sideeffects:
 *		Loads the required shared library and
 *		makes the addresses of MD2 functionality
 *		available. In case of failure an error
 *		message is left in the result area of
 *		the specified interpreter.
 *
 *	Result:
 *		A standard tcl error code.
 *
 *------------------------------------------------------*
 */

int
TrfLoadMD2 (interp)
    Tcl_Interp* interp;
{
  int res;

  TrfLock; /* THREADING: serialize initialization */

  if (md2f.loaded) {
    TrfUnlock;
    return TCL_OK;
  }

  res = Trf_LoadLibrary (interp, SSL_LIB_NAME, (VOID**) &ssl, ssl_symbols, 0);

  if ((res == TCL_OK) &&
      (ssl.md2_init   != NULL) &&
      (ssl.md2_update != NULL) &&
      (ssl.md2_final  != NULL)) {

    md2f.loaded = 1;
    md2f.init   = ssl.md2_init;
    md2f.update = ssl.md2_update;
    md2f.final  = ssl.md2_final;

    TrfUnlock;
    return TCL_OK;
  }

  TrfUnlock;
  return TCL_ERROR;
}

/*
 *------------------------------------------------------*
 *
 *	TrfLoadMD5 --
 *
 *	------------------------------------------------*
 *	Makes MD5 functionality available.
 *	------------------------------------------------*
 *
 *	Sideeffects:
 *		Loads the required shared library and
 *		makes the addresses of MD5 functionality
 *		available. In case of failure an error
 *		message is left in the result area of
 *		the specified interpreter.
 *
 *	Result:
 *		A standard tcl error code.
 *
 *------------------------------------------------------*
 */
int
TrfLoadMD5 (interp)
    Tcl_Interp* interp;
{
#ifdef MD5_STATIC_BUILD
  md5f.loaded = 1;
  md5f.init   = md5_init_ctx;
  md5f.update = md5_process_bytes;
  md5f.final  = md5_finish_ctx;
  md5f.crypt  = NULL;
  return TCL_OK;
#else
  int res;

  TrfLock; /* THREADING: serialize initialization */
  res = Trf_LoadLibrary (interp, CRYPT_LIB_NAME, (VOID**) &md5f,
			 crypt_symbols, 0);
  TrfUnlock;

  return res;
#endif
}

/*
 *------------------------------------------------------*
 *
 *	TrfLoadSHA1 --
 *
 *	------------------------------------------------*
 *	Makes SHA-1 functionality available.
 *	------------------------------------------------*
 *
 *	Sideeffects:
 *		Loads the required shared library and
 *		makes the addresses of SHA-1 functionality
 *		available. In case of failure an error
 *		message is left in the result area of
 *		the specified interpreter.
 *		
 *
 *	Result:
 *		A standard tcl error code.
 *
 *------------------------------------------------------*
 */

int
TrfLoadSHA1 (interp)
    Tcl_Interp* interp;
{
  int res;

  TrfLock; /* THREADING: serialize initialization */

  if (sha1f.loaded) {
    TrfUnlock;
    return TCL_OK;
  }

  res = Trf_LoadLibrary (interp, SSL_LIB_NAME, (VOID**) &ssl, ssl_symbols, 0);

  if ((res == TCL_OK) &&
      (ssl.sha1_init   != NULL) &&
      (ssl.sha1_update != NULL) &&
      (ssl.sha1_final  != NULL)) {

    sha1f.loaded = 1;
    sha1f.init   = ssl.sha1_init;
    sha1f.update = ssl.sha1_update;
    sha1f.final  = ssl.sha1_final;

    TrfUnlock;
    return TCL_OK;
  }

  TrfUnlock;
  return TCL_ERROR;
}

