
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
 * mslp_macosx.c : System dependent definitions for MacOS X.
 *
 * Version: 1.0
 * Date:    12/03/99
 *
 */

#include <sys/utsname.h>
#include <sys/time.h>
#include <assert.h>
#include <stdarg.h>
#include <sys/file.h>
#include <sys/stat.h>
#include <signal.h>

#include <pthread.h>	// for pthread_*_t
#include <unistd.h>		// for _POSIX_THREADS

#include "mslp_sd.h"
#include "slp.h"
#include "mslp.h"     /* all these includes are for the definition of LOG */

pthread_mutex_t		gClientMutex;
pthread_mutex_t		gServerMutex;
static int			gMutextInitialized = 0;

EXPORT int LinOpenNetworking()
{
    if ( !gMutextInitialized )
    {
        pthread_mutex_init( &gClientMutex, NULL );
        pthread_mutex_init( &gServerMutex, NULL );
        
        gMutextInitialized = 1;
    }
    
    return 0;
}

EXPORT long LinGetTime()
{
  /* normally time() returns time_t, which is defined in sys/types.h to be  */
  /* the time of day in seconds, and typedefed to a long                    */
  long lResult;

    lResult = time(NULL);
    
    return lResult;
}

EXPORT int Linstrcasecmp(const char *pc1, const char *pc2) {
	if(pc1 && pc2)
		return strcasecmp(pc1,pc2);
	else
		return -1;
}

EXPORT int Linstrncasecmp(const char *pc1, const char *pc2, int i) {
  return strncasecmp(pc1,pc2,i);
}

EXPORT int Linchmod_writable(const char *pcPath) {

  return chmod(pcPath, FILE_MODE);

}

EXPORT void Linatexit(void (*exit_handler)(int)) {
  signal(SIGHUP,exit_handler);
  signal(SIGINT,exit_handler);
  signal(SIGQUIT,exit_handler);
}

#ifdef EXTRA_MSGS

/* as client, returns NULL unless file exists
 * as server, removes the file and creates it afresh
 *    log problem if the file exists initially (should not)
 */
 
EXPORT void * LinGetMutex(int iMode) 
{
#ifdef MAC_OS_X
    if (iMode == MSLP_CLIENT)
        return &gClientMutex;
    else
        return &gServerMutex;
#else
    int   iLockFileExists;
    
    if ((iLockFileExists = LockFileExists())<0) {
        mslplog(SLP_LOG_ERR,"LinGetMutex - fopen of lock file failed: ",
                strerror(errno));
        return NULL;
    }
    if (iMode == MSLP_CLIENT) {
    
        if (iLockFileExists) {
        return OpenLockFile(MSLP_CLIENT);
        } else {
        LOG(SLP_LOG_ERR,"LinGetMutex - lock file doesn't exist");
        return NULL;
        }
    
    } else if (iMode == MSLP_SERVER) {
    
        if (iLockFileExists) {
			unlink(LOCK_NAME); /* remove old file */
        }
    
        return OpenLockFile(MSLP_SERVER);
    
    } else {
    
        SLP_LOG( SLP_LOG_ERR, "LinGetMutex - Must be client or server...");
        return NULL;
    
    }
#endif
}

/*
 * as client, does nothing but frees per process stuff
 * as server, removes the shared file
 * Returns: 0 if OK, <0 if failed (actually SLPInternalError code)
 */
EXPORT int LinFreeMutex(void *pvLock, int iMode) 
{
#ifdef MAC_OS_X
    pthread_mutex_init( (pthread_mutex_t*)pvLock, NULL );
#else
    if (pvLock == NULL) {
        LOG_SLP_ERROR_AND_RETURN(SLP_LOG_ERR,"LinFreeMutex - warning: called with no lock parameter!",
                (int)SLP_PARAMETER_BAD);
    }
    
    if (LockFileExists() == 0) {
        LOG_SLP_ERROR_AND_RETURN(SLP_LOG_ERR,"lock doesn't exist!",(int)SLP_INTERNAL_SYSTEM_ERROR);
    }
    
    if (pvLock) {
        if (iMode == MSLP_CLIENT) { /* release lock before freeing it */
        if (flock(*(int*)pvLock,LOCK_UN)<0) perror("LinFreeMutex - flock");
        }
        if (close(*(int*)pvLock) < 0) perror("LinFreeMutex - close");
        if (iMode == MSLP_SERVER) { /* remove lock entirely - no one may use it! */
        if (unlink(LOCK_NAME) < 0) perror("LinFreeMutex - unlink");
        }
    }
    SLPFree(pvLock);
#endif
    return (int) SLP_OK;
}

/*
 * complain if there is no file
 * as client locks file
 * as server locks file
 * Returns: 0 if OK, <0 if failed (actually SLPInternalError code)
 */
EXPORT int LinLock(void *pvLock) 
{
#ifdef MAC_OS_X
    if ( pvLock )
        pthread_mutex_lock( (pthread_mutex_t*)pvLock );
#else
    if (pvLock == NULL) {
        LOG_SLP_ERROR_AND_RETURN(SLP_LOG_ERR,"LinLock - missing lock param",(int)SLP_PARAMETER_BAD);
    }
    
    if (LockFileExists() == 0) {
        LOG_SLP_ERROR_AND_RETURN(SLP_LOG_ERR,"LinLock - missing lock file",
                (int)SLP_INTERNAL_SYSTEM_ERROR);
    }
    
    if (flock(*(int*) pvLock, LOCK_EX)< 0) {
        mslplog(SLP_LOG_ERR,"LinLock - flock failed",strerror(errno));
        return (int)SLP_INTERNAL_SYSTEM_ERROR;
    }
#endif
    
    return (int)SLP_OK;
}

/*
 * Returns: 0 if OK, <0 if failed (actually SLPInternalError code)
 */
EXPORT int LinUnlock(void *pvLock) 
{
#ifdef MAC_OS_X
    if ( pvLock )
        pthread_mutex_unlock( (pthread_mutex_t*)pvLock );
#else
    if (pvLock == NULL) {
        LOG_SLP_ERROR_AND_RETURN(SLP_LOG_ERR,"LinLock - missing lock param",(int)SLP_PARAMETER_BAD);
    }
    
    if (LockFileExists() == 0) {
        LOG_SLP_ERROR_AND_RETURN(SLP_LOG_ERR,"LinLock - missing lock file",
                (int)SLP_INTERNAL_SYSTEM_ERROR);
    }
    
    if (flock(*(int*)pvLock,LOCK_UN) < 0) {
        mslplog(SLP_LOG_ERR,"LinLock - flock failed",strerror(errno));
        return (int)SLP_INTERNAL_SYSTEM_ERROR;
    }
#endif

    return (int)SLP_OK;
}

EXPORT const char *LinDefaultRegfile() {
  return DEFAULT_REGFILE;
}

EXPORT const char *LinDefaultTempfile() {
  return DEFAULT_TEMPFILE;
}

#endif /* EXTRA_MSGS */
