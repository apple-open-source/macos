/*
 * KLLockFile.c
 *
 * $Header: /cvs/kfm/KerberosFramework/KerberosLogin/Sources/KerberosLogin/KLLockFile.c,v 1.5 2003/06/06 14:28:55 lxs Exp $
 *
 * Copyright 2003 Massachusetts Institute of Technology.
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

#define	kCCacheLockFileDirPrefix 	"/tmp/.KerberosLogin-"
#define	kCCacheLockFileSuffix    	"/KLLCCache.lock"
#define	kLockFileDirPerms		(S_IRUSR | S_IWUSR | S_IXUSR)
#define	kLockFilePerms			(S_IRUSR | S_IWUSR)

static KLStatus __KLGetLockFile (char **outLockFileDirectory, char **outLockFile, uid_t *outUID);
static KLStatus __KLCheckLockFileDirectory (char *lockFileDirectory, uid_t lockFileUID);

static KLLockType gLockType = kNoLock;
static KLIndex    gLockRefCount = 0;
static int        gLockFileFD = -1;

#pragma mark -

KLStatus __KLLockCCache (KLLockType inLockType)
{
    KLStatus err = klNoErr;

    if (__KLIsKerberosLoginServer()) { return klNoErr; }  // app that prompted will lock for KLS
    
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

            if (err == klNoErr) {
                if (fchown (gLockFileFD, lockFileUID, getegid ()) != 0) {
                    err = KLError_ (errno);
                }
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
    
    return KLError_ (err);
}

KLStatus __KLUnlockCCache (void)
{
    KLStatus err = klNoErr;

    if (__KLIsKerberosLoginServer()) { return klNoErr; }  // app that prompted will lock for KLS
    
    if (gLockRefCount < 1) { err = KLError_ (klParameterErr); }
    if (gLockFileFD   < 0) { err = KLError_ (klParameterErr); }

    if ((gLockRefCount == 1) && (gLockFileFD >= 0)) {
        if (err == klNoErr) {
            err = flock (gLockFileFD, LOCK_UN);
        }
 
        if (err == klNoErr) {
            close (gLockFileFD);
            gLockFileFD = -1;
            gLockType = kNoLock;
        }
    }
    
    if (err == klNoErr) {
        gLockRefCount--;
    }   
    
    return KLError_ (err);
}

#pragma mark -

static KLStatus __KLGetLockFile (char **outLockFileDirectory, char **outLockFile, uid_t *outUID)
{
    KLStatus err = klNoErr;
    uid_t uid = 0; 
    char uidString[64];
    char *fileName = NULL;
    char *directoryName = NULL;

    if (outLockFileDirectory == NULL) { err = KLError_ (klParameterErr); }
    if (outLockFile          == NULL) { err = KLError_ (klParameterErr); }
    if (outUID               == NULL) { err = KLError_ (klParameterErr); }
    
    if (err == klNoErr) {
        uid = geteuid ();  // The uid the file will be created as if we create it
    }

    if (err == klNoErr) {
        int count = snprintf (uidString, sizeof (uidString), "%d", uid);
        if (count > sizeof (uidString)) {
            dprintf ("__KLGetLockFile: WARNING! UID %d needs %ld bytes and buffer is %ld bytes",
                     uid, count, sizeof (uidString));
            err = KLError_ (klParameterErr);
        }
    }

    if (err == klNoErr) {
        err = __KLCreateString (uidString, &directoryName);
    }

    if (err == klNoErr) {
        err = __KLAddPrefixToString (kCCacheLockFileDirPrefix, &directoryName);
    }

    if (err == klNoErr) {
        err = __KLCreateString (kCCacheLockFileSuffix, &fileName);
    }

    if (err == klNoErr) {
        err = __KLAddPrefixToString (directoryName, &fileName);
    }    

    if (err == klNoErr) {
        *outUID = uid;
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
            dprintf ("__KLCheckLockFileDirectory(): stat(%s) returned %ld (%s)\n",
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
            if (chown (lockFileDirectory, lockFileUID, sb.st_gid) != 0) {
                err = KLError_ (errno);
                dprintf ("__KLCheckLockFileDirectory(): %s is owned by uid %ld, not uid %ld\n",
                         lockFileDirectory, sb.st_uid, lockFileUID);
            }
        }
    }
    
    if (err == klNoErr) {
        if ((sb.st_mode & ALLPERMS) != kLockFileDirPerms) {
            if (chmod (lockFileDirectory, kLockFileDirPerms) != 0) {
                err = KLError_ (errno);
                dprintf ("__KLCheckLockFileDirectory(): %s has permission bits 0x%lx, not 0x%lx\n",
                         lockFileDirectory, (sb.st_mode & ALLPERMS), kLockFileDirPerms);
            }
        }
    }
    
    return KLError_ (err);
}

