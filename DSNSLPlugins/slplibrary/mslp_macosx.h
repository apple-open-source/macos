/*
 * mslp_macosx.h : System dependent definitions for MacOS X.
 *
 * Version: 1.0
 * Date:    12/03/99
 *
 * Author: Kevin Arnold
 */

#ifndef _MSLP_MACOSX_
#define _MSLP_MACOSX_
#include <sys/types.h>
#include <sys/ipc.h>
//#include <sys/sem.h>
#include <sys/time.h>   /* for using select          */
#include <netinet/in.h> /* for net address functions */
#include <sys/socket.h>
#include <arpa/inet.h>
#include <sys/param.h>
//#include <unistd.h>     /* for close(), etc.   */
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

#ifdef	__cplusplus
//extern "C" {
#endif
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

#define SLPD_VERSION	  "2.0"
#define DEFAULT_REGFILE   "/private/var/slp.regfile"	// we want this to stick around
#define DEFAULT_TEMPFILE  "/tmp/slp.tempfile"
#define LOCK_NAME         "/tmp/slp.lock"

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
#ifdef	__cplusplus
//}
#endif
#endif /* ifndef _MSLP_MACOSX_ */
