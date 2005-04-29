/*
 * "$Id: globals.h,v 1.4 2004/11/24 21:25:40 jlovell Exp $"
 *
 *   Global variable definitions for the Common UNIX Printing System (CUPS).
 *
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

#ifndef _CUPS_GLOBALS_H_
#  define _CUPS_GLOBALS_H_

/*
 * Include necessary headers...
 */

#include "config.h"
#include <string.h>
#include <pthread.h>
#include "http.h"
#include "ipp.h"
#include "ppd.h"
#include "language.h"

#if defined(HAVE_CDSASSL)
#include <Security/Security.h>
#  endif /* HAVE_CDSASSL */

/*
 * C++ magic...
 */

#  ifdef __cplusplus
extern "C" {
#  endif /* __cplusplus */


/*
 * To make libcups thread safe define thread safe globals (aka thread specific
 * data) for the static variables used in the library.
 */

typedef struct 
{
  /* util.c */
  http_t		*http;				/* Current server connection */
  ipp_status_t		last_error;			/* Last IPP error */
  char			def_printer[256];		/* Default printer */
  char			ppd_filename[HTTP_MAX_URI];	/* Local filename */

  /* usersys.c */
  http_encryption_t	cups_encryption;		/* Encryption setting */
  char			cups_user[65],			/* User name */
			cups_server[256];		/* Server address */
  const char		*(*cups_pwdcb)(const char *);	/* Password callback */

#ifdef HAVE_DOMAINSOCKETS
  char			cups_server_domainsocket[104];	/* Domain socket */
#endif /* HAVE_DOMAINSOCKETS */

  /* tempfile.c */
  char			*temp_fd_buf;			/* cupsTempFd buffer */
  char			temp_file_buf[1024];		/* cupsTempFile buffer */

  /* ppd.c */
  ppd_status_t		ppd_status;			/* Status of last ppdOpen*() */
  int			ppd_line;			/* Current line number */
  ppd_conform_t		ppd_conform;			/* Level of conformance required */

  /* language.c */
  cups_lang_t		*lang_cache;			/* Language string cache */

#ifdef HAVE_CF_LOCALE_ID
  char			apple_language[32];		/* Cached language */
#else
  const char		*apple_language;		/* Cached language */
#endif /* HAVE_CF_LOCALE_ID */

  /* ipp.c */
  ipp_uchar_t		date_buf[11];			/* RFC-1903 date/time data */

  /* ipp-support.c */
  int			ipp_port;			/* IPP port number */
  char			unknown[255];			/* Unknown error statuses */

  /* http.c */
  char			datetime[256];			/* date time buffer */

  /* http-addr.c */
  unsigned		packed_ip;			/* Packed IPv4 address */
  char			*packed_ptr[2];			/* Pointer to packed address */
  struct hostent	host_ip;			/* Host entry for IP address */

} cups_globals_t;


/*
 * '_cups_globals()' - Return a pointer to thread safe globals
 */

extern cups_globals_t *_cups_globals(void);


#if defined(HAVE_CDSASSL)
extern OSStatus (*_cupsSSLCloseProc)(SSLContextRef context);
extern OSStatus (*_cupsSSLDisposeContextProc)(SSLContextRef context);
extern OSStatus (*_cupsSSLGetBufferedReadSizeProc)(SSLContextRef context, size_t *bufSize);
extern OSStatus (*_cupsSSLHandshakeProc)(SSLContextRef context);
extern OSStatus (*_cupsSSLNewContextProc)(Boolean isServer, SSLContextRef *contextPtr);
extern OSStatus (*_cupsSSLReadProc)(SSLContextRef context, void *data, size_t dataLength, size_t *processed);
extern OSStatus (*_cupsSSLSetConnectionProc)(SSLContextRef context, SSLConnectionRef connection);
extern OSStatus (*_cupsSSLSetEnableCertVerifyProc)(SSLContextRef context, Boolean enableVerify);
extern OSStatus (*_cupsSSLSetIOFuncsProc)(SSLContextRef context, SSLReadFunc read, SSLWriteFunc write);
extern OSStatus (*_cupsSSLWriteProc)(SSLContextRef context, const void *data, size_t dataLength, size_t *processed);

extern OSStatus (*_cupsSecKeychainOpenProc)(const char *pathName, SecKeychainRef *keychain);
extern OSStatus (*_cupsSecIdentitySearchCreateProc)(CFTypeRef keychainOrArray, CSSM_KEYUSE keyUsage, SecIdentitySearchRef *searchRef);
extern OSStatus (*_cupsSecIdentitySearchCopyNextProc)(SecIdentitySearchRef searchRef, SecIdentityRef *identity);

void _cups_cdsa_init(void);

#endif /* HAVE_CDSASSL */


/*
 * C++ magic...
 */

#  ifdef __cplusplus
}
#  endif /* __cplusplus */
#endif /* !_CUPS_GLOBALS_H_ */

/*
 * End of "$Id: globals.h,v 1.4 2004/11/24 21:25:40 jlovell Exp $".
 */
