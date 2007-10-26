/*
 * KLLockFile.c
 *
 * $Header$
 *
 * Copyright 2004 Massachusetts Institute of Technology.
 * All Rights Reserved.
 *
 * Export of this software from the United States of America may
 * require a specific license from the United States Government.
 * It is the responsibility of any person or organization contemplating
 * export to obtain such a license before exporting.
 *
 * WITHIN THAT CONSTRAINT, permission to use, copy, modify, and
 * distribute this software and its documentation for any purpose and
 * without fee is hereby granted, provided that the above copyright
 * notice appear in all copies and that both that copyright notice and
 * this permission notice appear in supporting documentation, and that
 * the name of M.I.T. not be used in advertising or publicity pertaining
 * to distribution of the software without specific, written prior
 * permission.  Furthermore if you modify this software you must label
 * your software as modified software and not distribute it in such a
 * fashion that it might be confused with the original M.I.T. software.
 * M.I.T. makes no representations about the suitability of
 * this software for any purpose.  It is provided "as is" without express
 * or implied warranty.
 */

#define	kCCacheLockFileFallbackTmp 	"/tmp"
#define	kCCacheLockFileDirPrefix 	"/.KerberosLogin-"
#define	kCCacheLockFileSuffix    	"/KLLCCache.lock"
#define	kLockFileDirPerms		(S_IRUSR | S_IWUSR | S_IXUSR)
#define	kLockFilePerms			(S_IRUSR | S_IWUSR)

static KLStatus __KLGetLockFile (char **outLockFileDirectory, char **outLockFile, uid_t *outUID);
static KLStatus __KLCheckLockFileDirectory (char *lockFileDirectory, uid_t lockFileUID);

static pthread_mutex_t gLockFileMutex = PTHREAD_MUTEX_INITIALIZER;
static KLLockType      gLockType = kNoLock;
static KLIndex         gLockRefCount = 0;
static int             gLockFileFD = -1;

#pragma mark -

KLStatus __KLLockCCache (KLLockType inLockType)
{
    if (__KLIsKerberosAgent()) { return klNoErr; }  // app that prompted will lock for KerberosAgent
    
    KLStatus lockErr = pthread_mutex_lock (&gLockFileMutex);
    KLStatus err = lockErr;

    if ((inLockType != kReadLock) && (inLockType != kWriteLock)) { 
        err = KLError_ (klParameterErr); 
    }

    if (err == klNoErr) {
        if (gLockRefCount == 0) {
            // file is unlocked, lock it
            char *lockFileName = NULL;
            char *lockFileDirectoryName = NULL;
            uid_t lockFileUID;

            if (err == klNoErr) {
                err = __KLGetLockFile (&lockFileDirectoryName, &lockFileName, &lockFileUID);
            }
            
            if (err == klNoErr) {
                if (mkdir (lockFileDirectoryName, kLockFileDirPerms) != 0) {
                    if (errno != EEXIST) { err = KLError_ (errno); }
                }
            }
            
            if (err == klNoErr) {
                err = __KLCheckLockFileDirectory (lockFileDirectoryName, lockFileUID);
            }
            
            if (err == klNoErr) {
                gLockFileFD = open (lockFileName, O_CREAT, kLockFilePerms);
                if (gLockFileFD < 0) { err = KLError_ (errno); }
            }

            if (lockFileName          != NULL) { KLDisposeString (lockFileName); }
            if (lockFileDirectoryName != NULL) { KLDisposeString (lockFileDirectoryName); }
        }
    }
        
    if (err == klNoErr) {
        if ((gLockRefCount == 0) ||                                       // Not locked at all
            ((inLockType == kWriteLock) && (gLockType == kReadLock))) {   // Needs upgrading to a write lock
            for (;;) {
                if (flock (gLockFileFD, ((inLockType == kWriteLock) ? LOCK_EX : LOCK_SH) | LOCK_NB) == 0) {
                    gLockType = inLockType;
                    break;
                } else {
                    if (errno == EWOULDBLOCK) {
                        __KLCallIdleCallback ();
                        usleep (10);
                        continue;
                    } else {
                        if (gLockRefCount == 0) {
                            close (gLockFileFD);
                            gLockFileFD = -1;
                        }
                        err = KLError_ (errno);
                        break;
                    }
                }
            } 
        }
    }
    
    if (err == klNoErr) {
        gLockRefCount++;
    }
    
    if (lockErr == klNoErr) { pthread_mutex_unlock (&gLockFileMutex); }
    return KLError_ (err);
}

KLStatus __KLUnlockCCache (void)
{
    if (__KLIsKerberosAgent()) { return klNoErr; }  // app that prompted will lock for KerberosAgent
    
    KLStatus lockErr = pthread_mutex_lock (&gLockFileMutex);
    KLStatus err = lockErr;

    if (gLockRefCount < 1) { err = KLError_ (klParameterErr); }
    if (gLockFileFD   < 0) { err = KLError_ (klParameterErr); }

    if (err == klNoErr) {
        if ((gLockRefCount == 1) && (gLockFileFD >= 0)) {
            err = flock (gLockFileFD, LOCK_UN);
            
            if (err == klNoErr) {
                close (gLockFileFD);
                gLockFileFD = -1;
                gLockType = kNoLock;
            }
        }
    }
    
    if (err == klNoErr) {
        gLockRefCount--;
    }   
    
    if (lockErr == klNoErr) { pthread_mutex_unlock (&gLockFileMutex); }
    return KLError_ (err);
}

#pragma mark -

static KLStatus __KLGetLockFile (char **outLockFileDirectory, char **outLockFile, uid_t *outUID)
{
    KLStatus err = klNoErr;
    char tmpDir[MAXPATHLEN];
    char uidString[64];
    char *fileName = NULL;
    char *directoryName = NULL;

    if (outLockFileDirectory == NULL) { err = KLError_ (klParameterErr); }
    if (outLockFile          == NULL) { err = KLError_ (klParameterErr); }
    if (outUID               == NULL) { err = KLError_ (klParameterErr); }
    
    if (err == klNoErr) {
        size_t size = 0;
        
#ifdef _CS_DARWIN_USER_TEMP_DIR
        size = confstr (_CS_DARWIN_USER_TEMP_DIR, tmpDir, sizeof (tmpDir));
#else
#warning _CS_DARWIN_USER_TEMP_DIR not defined
#endif
        
        if (size < 1 || size >= sizeof (tmpDir)) { 
            /* no temporary directory defined */
            strlcpy (tmpDir, kCCacheLockFileFallbackTmp, sizeof (tmpDir));
        }
    }
    
    if (err == klNoErr) {
        uid_t uid = kipc_session_get_session_uid ();
        unsigned int count = snprintf (uidString, sizeof (uidString), "%d", uid);
        if (count > sizeof (uidString)) {
            dprintf ("__KLGetLockFile: WARNING! UID %d needs %d bytes and buffer is %ld bytes",
                     uid, count, sizeof (uidString));
            err = KLError_ (klParameterErr);
        }
    }
    
    // Create the lock file directory path
    
    if (err == klNoErr) {
        err = __KLCreateString (tmpDir, &directoryName);
    }
    
    if (err == klNoErr) {
        err = __KLAppendToString (kCCacheLockFileDirPrefix, &directoryName);
    }
    
    if (err == klNoErr) {
        err = __KLAppendToString (uidString, &directoryName);
    }
    
    if (err == klNoErr) {
        err = __KLAppendToString ("-", &directoryName);
    }
    
    if (err == klNoErr) {
        err = __KLAppendToString (kipc_get_session_id_string (), &directoryName);
    }
    
    // Create the lock file path

    if (err == klNoErr) {
        err = __KLCreateString (directoryName, &fileName);
    }

    if (err == klNoErr) {
        err = __KLAppendToString (kCCacheLockFileSuffix, &fileName);
    }    

    if (err == klNoErr) {
        *outUID = geteuid ();
        *outLockFile = fileName;
        *outLockFileDirectory = directoryName;
    } else {
        if (fileName      != NULL) { KLDisposeString (fileName); }
        if (directoryName != NULL) { KLDisposeString (directoryName); }
    }

    return KLError_ (err);
}


static KLStatus __KLCheckLockFileDirectory (char *lockFileDirectory, uid_t lockFileUID)
{
    KLStatus    err = klNoErr;
    struct stat sb;
    
    if (err == klNoErr) {
        err = stat (lockFileDirectory, &sb);
        if (err != 0) {
            dprintf ("__KLCheckLockFileDirectory(): stat(%s) returned %d (%s)\n",
                     lockFileDirectory, errno, strerror (errno));
            err = KLError_ (errno);
        }
    }
    
    if (err == klNoErr) {
        if (!S_ISDIR (sb.st_mode)) {
            dprintf ("__KLCheckLockFileDirectory(): %s is not a directory\n", lockFileDirectory);
            err = KLError_ (ENOTDIR);
        }
    }
    
    if (err == klNoErr) {
        if (sb.st_uid != lockFileUID) {
            // Just warn, since there's nothing we can do about this
            dprintf ("__KLCheckLockFileDirectory(): %s is owned by uid %d, not uid %d\n",
                     lockFileDirectory, sb.st_uid, lockFileUID);
        }
    }
    
    if (err == klNoErr) {
        if ((sb.st_mode & ALLPERMS) != kLockFileDirPerms) {
            if (chmod (lockFileDirectory, kLockFileDirPerms) != 0) {
                err = KLError_ (errno);
                dprintf ("__KLCheckLockFileDirectory(): %s has permission bits 0x%x, not 0x%x\n",
                         lockFileDirectory, (sb.st_mode & ALLPERMS), kLockFileDirPerms);
            }
        }
    }
    
    return KLError_ (err);
}

