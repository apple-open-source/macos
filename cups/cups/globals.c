/*
 * "$Id: globals.c,v 1.7 2005/02/13 19:02:43 jlovell Exp $"
 *
 *   Global variable access routines for the Common UNIX Printing System (CUPS).
 *
 * Contents:
 *
 *   globals_init()       - One-time initializer of globals
 *   globals_destructor() - Frees memory allocated in _cups_globals().
 *   _cups_globals()	  - Return a pointer to thread local storage
 *   cdsa_init()          - One-time initializer of CDSA SSL.
 */

/*
© Copyright 2004 Apple Computer, Inc. All rights reserved.

IMPORTANT:  This Apple software is supplied to you by Apple Computer,
Inc. ("Apple") in consideration of your agreement to the following
terms, and your use, installation, modification or redistribution of
this Apple software constitutes acceptance of these terms.  If you do
not agree with these terms, please do not use, install, modify or
redistribute this Apple software.

In consideration of your agreement to abide by the following terms, and
subject to these terms, Apple grants you a personal, non-exclusive
license, under Apple’s copyrights in this original Apple software (the
"Apple Software"), to use, reproduce, modify and redistribute the Apple
Software, with or without modifications, in source and/or binary forms;
provided that if you redistribute the Apple Software in its entirety and
without modifications, you must retain this notice and the following
text and disclaimers in all such redistributions of the Apple Software. 
Neither the name, trademarks, service marks or logos of Apple Computer,
Inc. may be used to endorse or promote products derived from the Apple
Software without specific prior written permission from Apple.  Except
as expressly stated in this notice, no other rights or licenses, express
or implied, are granted by Apple herein, including but not limited to
any patent rights that may be infringed by your derivative works or by
other works in which the Apple Software may be incorporated.

The Apple Software is provided by Apple on an "AS IS" basis.  APPLE
MAKES NO WARRANTIES, EXPRESS OR IMPLIED, INCLUDING WITHOUT LIMITATION
THE IMPLIED WARRANTIES OF NON-INFRINGEMENT, MERCHANTABILITY AND FITNESS
FOR A PARTICULAR PURPOSE, REGARDING THE APPLE SOFTWARE OR ITS USE AND
OPERATION ALONE OR IN COMBINATION WITH YOUR PRODUCTS.

IN NO EVENT SHALL APPLE BE LIABLE FOR ANY SPECIAL, INDIRECT, INCIDENTAL
OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
INTERRUPTION) ARISING IN ANY WAY OUT OF THE USE, REPRODUCTION,
MODIFICATION AND/OR DISTRIBUTION OF THE APPLE SOFTWARE, HOWEVER CAUSED
AND WHETHER UNDER THEORY OF CONTRACT, TORT (INCLUDING NEGLIGENCE),
STRICT LIABILITY OR OTHERWISE, EVEN IF APPLE HAS BEEN ADVISED OF THE
POSSIBILITY OF SUCH DAMAGE.
*/


/*
 * Include necessary headers...
 */

#include <stdlib.h>
#include "globals.h"
#include "http-private.h"

#ifdef HAVE_DLFCN_H
#  include <dlfcn.h>
#endif /* HAVE_DLFCN_H */


/*
 * Globals...
 */
#ifdef HAVE_CDSASSL
OSStatus (*_cupsSSLCloseProc)(SSLContextRef context) = NULL;
OSStatus (*_cupsSSLDisposeContextProc)(SSLContextRef context) = NULL;
OSStatus (*_cupsSSLGetBufferedReadSizeProc)(SSLContextRef context, size_t *bufSize) = NULL;
OSStatus (*_cupsSSLHandshakeProc)(SSLContextRef context) = NULL;
OSStatus (*_cupsSSLNewContextProc)(Boolean isServer, SSLContextRef *contextPtr) = NULL;
OSStatus (*_cupsSSLReadProc)(SSLContextRef context, void *data, size_t dataLength, size_t *processed) = NULL;
OSStatus (*_cupsSSLSetConnectionProc)(SSLContextRef context, SSLConnectionRef connection) = NULL;
OSStatus (*_cupsSSLSetEnableCertVerifyProc)(SSLContextRef context, Boolean enableVerify) = NULL;
OSStatus (*_cupsSSLSetIOFuncsProc)(SSLContextRef context, SSLReadFunc read, SSLWriteFunc write) = NULL;
OSStatus (*_cupsSSLWriteProc)(SSLContextRef context, const void *data, size_t dataLength, size_t *processed) = NULL;

OSStatus (*_cupsSecKeychainOpenProc)(const char *pathName, SecKeychainRef *keychain) = NULL;
OSStatus (*_cupsSecIdentitySearchCreateProc)(CFTypeRef keychainOrArray, CSSM_KEYUSE keyUsage, SecIdentitySearchRef *searchRef) = NULL;
OSStatus (*_cupsSecIdentitySearchCopyNextProc)(SecIdentitySearchRef searchRef, SecIdentityRef *identity) = NULL;

static pthread_once_t	cdsa_key_once = PTHREAD_ONCE_INIT;
					/* One-time initialization object */
#endif /* HAVE_CDSASSL */

static pthread_key_t	globals_key = -1;/* Thread local storage key */
static pthread_once_t	globals_key_once = PTHREAD_ONCE_INIT;
					/* One-time initialization object */

/*
 * Local functions...
 */

static void	globals_init();
static void	globals_destructor(void *value);
#ifdef HAVE_CDSASSL
static void	cdsa_init();
#endif /* HAVE_CDSASSL */


/*
 * 'globals_init()' - One-time initializer of globals.
 */

static
void globals_init()
{
    pthread_key_create(&globals_key, globals_destructor);
}


/*
 * 'globals_destructor()' - Frees memory allocated in _cups_globals().
 */

static
void globals_destructor(void *value)
{
    free(value);
}


/*
 * '_cups_globals()' - Return a pointer to thread local storage
 */

cups_globals_t *_cups_globals(void)
{
  cups_globals_t *globals;

  pthread_once(&globals_key_once, globals_init);

  if ((globals = (cups_globals_t *) pthread_getspecific(globals_key)) == NULL)
  {
    globals = calloc(1, sizeof(cups_globals_t));
    pthread_setspecific(globals_key, globals);

   /*
    * Initialize variables that have non-zero values
    */

    globals->cups_encryption = (http_encryption_t)-1;
    globals->cups_pwdcb = cups_get_password;

    strlcpy(globals->cups_server_domainsocket, CUPS_DEFAULT_DOMAINSOCKET, sizeof(globals->cups_server_domainsocket));
  }

  return globals;
}


#if defined(HAVE_CDSASSL)
/*
 * 'cdsa_init()' - One-time initializer of CDSA SSL.
 */

static const char SecurityLibPath[]	    = "/System/Library/Frameworks/Security.framework/Security";

static
void cdsa_init()
{
#ifdef HAVE_DLFCN_H
 /*
  * If we have dlopen then weak-link the library and load it when needed.
  */

  void *cdsa_lib = NULL;

  cdsa_lib = dlopen(SecurityLibPath, RTLD_LAZY);

  _cupsSSLGetBufferedReadSizeProc	= dlsym(cdsa_lib, "SSLGetBufferedReadSize");
  _cupsSSLNewContextProc			= dlsym(cdsa_lib, "SSLNewContext");
  _cupsSSLSetIOFuncsProc			= dlsym(cdsa_lib, "SSLSetIOFuncs");
  _cupsSSLSetConnectionProc		= dlsym(cdsa_lib, "SSLSetConnection");
  _cupsSSLSetEnableCertVerifyProc	= dlsym(cdsa_lib, "SSLSetEnableCertVerify");
  _cupsSSLHandshakeProc			= dlsym(cdsa_lib, "SSLHandshake");
  _cupsSSLDisposeContextProc		= dlsym(cdsa_lib, "SSLDisposeContext");
  _cupsSSLCloseProc			= dlsym(cdsa_lib, "SSLClose");
  _cupsSSLReadProc			= dlsym(cdsa_lib, "SSLRead");
  _cupsSSLWriteProc			= dlsym(cdsa_lib, "SSLWrite");

  _cupsSecKeychainOpenProc		= dlsym(cdsa_lib, "SecKeychainOpen");
  _cupsSecIdentitySearchCreateProc	= dlsym(cdsa_lib, "SecIdentitySearchCreate");
  _cupsSecIdentitySearchCopyNextProc	= dlsym(cdsa_lib, "SecIdentitySearchCopyNext");
#else
  _cupsSSLGetBufferedReadSizeProc	= SSLGetBufferedReadSize;
  _cupsSSLNewContextProc			= SSLNewContext;
  _cupsSSLSetIOFuncsProc			= SSLSetIOFuncs;
  _cupsSSLSetConnectionProc		= SSLSetConnection;
  _cupsSSLSetEnableCertVerifyProc	= SSLSetEnableCertVerify;
  _cupsSSLHandshakeProc			= SSLHandshake;
  _cupsSSLDisposeContextProc		= SSLDisposeContext;
  _cupsSSLCloseProc			= SSLClose;
  _cupsSSLReadProc			= SSLRead;
  _cupsSSLWriteProc			= SSLWrite;

  _cupsSecKeychainOpenProc		= SecKeychainOpen;
  _cupsSecIdentitySearchCreateProc	= SecIdentitySearchCreate;
  _cupsSecIdentitySearchCopyNextProc	= SecIdentitySearchCopyNext;
#endif /* HAVE_DLFCN_H */
}


/*
 * '_cups_cdsa_init()' - Initialize cdsa ssl.
 */

void _cups_cdsa_init(void)
{
  pthread_once(&cdsa_key_once, cdsa_init);

  return;
}

#endif	/* HAVE_CDSASSL */

/*
 * End of "$Id: globals.c,v 1.7 2005/02/13 19:02:43 jlovell Exp $".
 */
