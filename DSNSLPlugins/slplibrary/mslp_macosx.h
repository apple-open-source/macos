/*
 * Copyright (c) 2002 Apple Computer, Inc. All rights reserved.
 *
 * @APPLE_LICENSE_HEADER_START@
 * 
 * This file contains Original Code and/or Modifications of Original Code
 * as defined in and that are subject to the Apple Public Source License
 * Version 2.0 (the 'License'). You may not use this file except in
 * compliance with the License. Please obtain a copy of the License at
 * http://www.opensource.apple.com/apsl/ and read it before using this
 * file.
 * 
 * The Original Code and all software distributed under the License are
 * distributed on an 'AS IS' basis, WITHOUT WARRANTY OF ANY KIND, EITHER
 * EXPRESS OR IMPLIED, AND APPLE HEREBY DISCLAIMS ALL SUCH WARRANTIES,
 * INCLUDING WITHOUT LIMITATION, ANY WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE, QUIET ENJOYMENT OR NON-INFRINGEMENT.
 * Please see the License for the specific language governing rights and
 * limitations under the License.
 * 
 * @APPLE_LICENSE_HEADER_END@
 */

/*
 * mslp_macosx.h : System dependent definitions for MacOS X.
 *
 * Version: 1.0
 * Date:    12/03/99
 *
 */

#ifndef _MSLP_MACOSX_
#define _MSLP_MACOSX_
#include <sys/types.h>
#include <sys/ipc.h>
#include <sys/time.h>   /* for using select          */
#include <netinet/in.h> /* for net address functions */
#include <sys/socket.h>
#include <arpa/inet.h>
#include <sys/param.h>
#include <netdb.h>
#include <sys/stat.h>
#include <pwd.h>
#include <netdb.h>
#include <stdlib.h>
#include <time.h>
#include <stdio.h>
#include <errno.h>
#include <string.h>
#include <ctype.h>

#define FILE_MODE (S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH)

#include <unistd.h>

#define EXPORT
#define TESTEXPORT
#define SOCKET           int
#define INVALID_SOCKET   -1
#define SOCKET_ERROR     -1
#define CLOSESOCKET      close
#define OPEN_NETWORKING  LinOpenNetworking
#define CLOSE_NETWORKING
#define SDGetTime        LinGetTime
#define SDstrcasecmp     Linstrcasecmp
#define SDstrncasecmp    Linstrncasecmp
#define SDread           read
#define SDwrite          write
#define SDSleep          sleep
#define SDchmod_writable Linchmod_writable
#define SDatexit         Linatexit

#ifndef LOG_IDENTITY
    #define LOG_IDENTITY	"slp"
#endif

extern EXPORT int LinOpenNetworking(void);
extern EXPORT long LinGetTime(void);
extern EXPORT int Linstrcasecmp(const char *pc1, const char *pc2);
extern EXPORT int Linstrncasecmp(const char *pc1, const char *pc2, int n);
extern EXPORT int Linchmod_writable(const char *pcPath);
extern EXPORT void Linatexit(void (*fun)(int));

#ifdef EXTRA_MSGS

#define SDLock            LinLock
#define SDUnlock          LinUnlock
#define SDGetMutex        LinGetMutex
#define SDFreeMutex       LinFreeMutex
#define SDDefaultRegfile  LinDefaultRegfile
#define SDDefaultTempfile LinDefaultTempfile

#define SLPD_VERSION	  "2.1"
#define DEFAULT_REGFILE   "/private/var/slp.regfile"	// we want this to stick around
#define DEFAULT_TEMPFILE  "/private/var/slp.tempfile"	// don't create anything in a global writable directory
#define LOCK_NAME         "/private/var/slp.lock"

#define LOG_FILE			"/private/var/log/slp.log"

#define kDAConfigFilePath	"/private/etc/slpda.conf"
#define kSAConfigFilePath	"/private/etc/slpsa.conf"

extern EXPORT void * LinGetMutex(int iMode);
extern EXPORT int LinFreeMutex(void *, int iMode);
extern EXPORT int LinLock(void *);
extern EXPORT int LinUnlock(void *);
extern EXPORT const char * LinDefaultRegfile();
extern EXPORT const char * LinDefaultTempfile();

#endif /* EXTRA_MSGS */
#endif /* ifndef _MSLP_MACOSX_ */
