/*
 * Copyright (c) 2000-2020, Boris Popov
 * All rights reserved.
 *
 * Portions Copyright (C) 2001 - 2014 Apple Inc. All rights reserved. 
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *    This product includes software developed by Boris Popov.
 * 4. Neither the name of the author nor the names of any co-contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 */
#include <sys/param.h>
#include <sys/stat.h>
#include <sys/errno.h>
#include <sys/mount.h>
#include <time.h>

#include <stdio.h>
#include <string.h>
#include <pwd.h>
#include <grp.h>
#include <unistd.h>
#include <ctype.h>
#include <stdlib.h>
#include <err.h>
#include <sysexits.h>
#include <smbclient/smbclient.h>
#include <smbclient/smbclient_internal.h>
#include <smbclient/ntstatus.h>
#include <netsmb/smb_lib.h>

#include "SetNetworkAccountSID.h"

#include <mntopts.h>

#if !defined(kNetFSMountFlagsKey)
    #define kNetFSMountFlagsKey        CFSTR("MountFlags")
#endif

#if !defined(kNetFSSoftMountKey)
    #define kNetFSSoftMountKey        CFSTR("SoftMount")
#endif

#if !defined(kNetFSForceNewSessionKey)
    #define kNetFSForceNewSessionKey    CFSTR("ForceNewSession")
#endif


static void usage(void);
static void
handle_options(int argc, char **argv, CFMutableDictionaryRef *mOptions,
               char *mountPoint, uint64_t *options, const char **url);

static struct mntopt mopts[] = {
	MOPT_STDOPTS,
	{ "streams",        0, SMBFS_MNT_STREAMS_ON, 1 },
	{ "forcenotify",    1, SMBFS_MNT_NOTIFY_OFF, 1 }, /* negative flag */
	{ "soft",           0, SMBFS_MNT_SOFT, 1 },
    { "timemachine",    0, SMBFS_MNT_TIME_MACHINE, 1 },
    { "hifi",           0, SMBFS_MNT_HIGH_FIDELITY, 1 },
    { "datacache",      1, SMBFS_MNT_DATACACHE_OFF, 1 }, /* negative flag */
    { "mdatacache",     1, SMBFS_MNT_MDATACACHE_OFF, 1 }, /* negative flag */
    { "sessionencrypt", 0, SMBFS_MNT_SESSION_ENCRYPT, 1 },
    { "shareencrypt",   0, SMBFS_MNT_SHARE_ENCRYPT, 1 },
    { "filemode",       0, SMBFS_MNT_FILE_MODE, 1 },
    { "dirmode",        0, SMBFS_MNT_DIR_MODE, 1 },
    { "snapshot",       0, SMBFS_MNT_SNAPSHOT, 1 },
    { "passprompt",     1, SMBFS_MNT_NO_PASSWORD_PROMPT, 1 },
    { "forcenewsession",0, SMBFS_MNT_FORCE_NEW_SESSION, 1 },
	{ NULL, 0, 0, 0 }
};


static unsigned xtoi(unsigned u)
{
	if (isdigit(u))
		return (u - '0'); 
	else if (islower(u))
		return (10 + u - 'a'); 
	else if (isupper(u))
		return (10 + u - 'A'); 
	return (16);
}

/* Removes the "%" escape sequences from a URL component.
 * See IETF RFC 2396.
 *
 * Someday we should convert this to use CFURLCreateStringByReplacingPercentEscapesUsingEncoding
 */
static char * unpercent(char * component)
{
	unsigned char c, *s;
	unsigned hi, lo; 
	
	if (component)
		for (s = (unsigned char *)component; (c = (unsigned char)*s); s++) {
			if (c != '%') 
				continue;
			if ((hi = xtoi(s[1])) > 15 || (lo = xtoi(s[2])) > 15)
				continue; /* ignore invalid escapes */
			s[0] = hi*16 + lo;
			/*      
			 * This was strcpy(s + 1, s + 3);
			 * But nowadays leftward overlapping copies are
			 * officially undefined in C.  Ours seems to
			 * work or not depending upon alignment.
			 */      
			memmove(s+1, s+3, (strlen((char *)(s+3))) + 1);
		}       
	return (component);
}

int main(int argc, char *argv[])
{
	SMBHANDLE serverConnection = NULL;
	uint64_t options = kSMBOptionSessionOnly;
	NTSTATUS status;
	char mountPoint[MAXPATHLEN];
	const char * url = NULL;
    CFMutableDictionaryRef mOptions = NULL;

    handle_options(argc, argv, &mOptions, mountPoint, &options, &url);

	status = SMBOpenServerEx(url, &serverConnection, options);
	if (NT_SUCCESS(status)) {
		status = SMBMountShareExDict(serverConnection, NULL, mountPoint,
                                     mOptions, NULL,
                                     setNetworkAccountSID, NULL);
	}

	/* 
	 * SMBOpenServerEx now sets errno, so err will work correctly. We change 
	 * the string based on the NTSTATUS Error.
	 */
	if (!NT_SUCCESS(status)) {
		switch (status) {
			case STATUS_NO_SUCH_DEVICE:
				err(EX_UNAVAILABLE, "failed to intitialize the smb library");
				break;
			case STATUS_LOGON_FAILURE:
				err(EX_NOPERM, "server rejected the connection");
				break;
			case STATUS_CONNECTION_REFUSED:
				err(EX_NOHOST, "server connection failed");
			break;
			case STATUS_INVALID_HANDLE:
			case STATUS_NO_MEMORY:
				err(EX_UNAVAILABLE, "internal error");
				break;
			case STATUS_UNSUCCESSFUL:
				err(EX_USAGE, "mount error: %s", mountPoint);
				break;
			case STATUS_INVALID_PARAMETER:
				err(EX_USAGE, "URL parsing failed, please correct the URL and try again");
				break;
			case STATUS_BAD_NETWORK_NAME:
				err(EX_NOHOST, "share connection failed");
				break;
			default:
				err(EX_OSERR, "unknown status %d", status);
				break;
		}
	}

	/* We are done clean up anything left around */
    if (serverConnection) {
		SMBReleaseServer(serverConnection);
    }

	return 0;
}

/*
 * Convert all 'notification' occurrences with 'forcenotify,' so it can
 * get parsed by getmntopts().
 */
static char *
handle_notification_opt(char *opt)
{
    char *ptr = NULL;
    const char *notification = "notification";
    const char *forcenotify = "forcenotify,";

    while ((ptr = strstr(opt, notification))) {
        memcpy(ptr, forcenotify, strlen(notification));
    }

    return opt;
}

static void
handle_mntopts(mntoptparse_t mp, int altflags, mode_t *fileMode,
               mode_t *dirMode, char *snapshot_time, int snapshot_time_len,
               uint64_t *options, uint64_t *mntOptions)
{
    const char *str;
    char *next;

    if ((altflags & SMBFS_MNT_FILE_MODE) == SMBFS_MNT_FILE_MODE) {
        str = getmntoptstr(mp, "filemode");
        if (str) {
            errno = 0;
            *fileMode = strtol(str, &next, 8);
            if (errno || *next != 0)
                errx(EX_DATAERR, "invalid value for file mode");
        } else {
            errx(EX_DATAERR, "file mode needs a value");
        }
    }
    
    if ((altflags & SMBFS_MNT_DIR_MODE) == SMBFS_MNT_DIR_MODE) {
        str = getmntoptstr(mp, "dirmode");
        if (str) {
            errno = 0;
            *dirMode = strtol(str, &next, 8);
            if (errno || *next != 0)
                errx(EX_DATAERR, "invalid value for dir mode");
        } else {
            errx(EX_DATAERR, "file mode needs a value");
        }
    }

    if ((altflags & SMBFS_MNT_SNAPSHOT) == SMBFS_MNT_SNAPSHOT) {
        str = getmntoptstr(mp, "snapshot");
        if (str) {
            strncpy(snapshot_time, str, snapshot_time_len);

            *options |= kSMBOptionForceNewSession;
            *mntOptions |= kSMBMntForceNewSession;
        } else {
            errx(EX_DATAERR, "snapshot needs a value");
        }
    }
    
    if ((altflags & SMBFS_MNT_NO_PASSWORD_PROMPT) == SMBFS_MNT_NO_PASSWORD_PROMPT) {
        *options |= kSMBOptionNoPrompt;
    }

    if ((altflags & SMBFS_MNT_FORCE_NEW_SESSION) == SMBFS_MNT_FORCE_NEW_SESSION) {
        *options |= kSMBOptionForceNewSession;
        *mntOptions |= kSMBMntForceNewSession;
    }
}


static void
handle_options(int argc, char **argv, CFMutableDictionaryRef *mOptions,
               char *mountPoint, uint64_t *options, const char **url) {
    uint64_t mntOptions = 0;
    int altflags = SMBFS_MNT_STREAMS_ON;
    mode_t fileMode = 0, dirMode = 0;
    int mntflags = 0;
    struct stat st;
    char *next;
    int opt;
    int version = SMBFrameworkVersion();
    CFNumberRef numRef = NULL;
    char snapshot_time[32] = {0};
    CFStringRef strRef = NULL;
    
    while ((opt = getopt(argc, argv, "Nvhsd:f:o:t:")) != -1) {
        switch (opt) {
            case 'd':
                errno = 0;
                dirMode = strtol(optarg, &next, 8);
                if (errno || *next != 0)
                    errx(EX_DATAERR, "invalid value for directory mode");
                break;
            case 'f':
                errno = 0;
                fileMode = strtol(optarg, &next, 8);
                if (errno || *next != 0)
                    errx(EX_DATAERR, "invalid value for file mode");
                break;
            case 'N':
                *options |= kSMBOptionNoPrompt;
                break;
            case 'o': {
                mntoptparse_t mp = getmntopts(handle_notification_opt(optarg), mopts, &mntflags, &altflags);
                if (mp == NULL)
                    err(1, NULL);
                handle_mntopts(mp, altflags, &fileMode, &dirMode, snapshot_time,
                                        sizeof(snapshot_time), options, &mntOptions);
                freemntopts(mp);
                break;
            }
            case 's':
                *options |= kSMBOptionForceNewSession;
                mntOptions |= kSMBMntForceNewSession;
                break;
            case 't': {
                strncpy(snapshot_time, optarg, sizeof(snapshot_time));
                
                /*
                 * Snapshot mounts are always read only and force new session
                 * Read only is set later on in this function.
                 */
                *options |= kSMBOptionForceNewSession;
                mntOptions |= kSMBMntForceNewSession;
                break;
            }
            case 'v':
                errx(EX_OK, "version %d.%d.%d",
                     version / 100000, (version % 10000) / 1000, (version % 1000) / 100);
                break;
            case '?':
            case 'h':
            default:
                usage();
                break;
        }
    }
    if (optind >= argc)
        usage();
    
    argc -= optind;
    /* At this point we should only have a url and a mount point */
    if (argc != 2)
        usage();
    *url = argv[optind];
    optind++;
    
    if (mntflags & MNT_NOFOLLOW) {
        size_t sc = strlcpy(mountPoint, unpercent(argv[optind]),
                            MAXPATHLEN);
        if (sc >= MAXPATHLEN)
            err(EX_USAGE, "%s: %s", mountPoint, strerror(EINVAL));
    } else {
        realpath(unpercent(argv[optind]), mountPoint);
    }
    
    if (stat(mountPoint, &st) == -1)
        err(EX_OSERR, "could not find mount point %s", mountPoint);
    
    if (!S_ISDIR(st.st_mode)) {
        errno = ENOTDIR;
        err(EX_OSERR, "can't mount on %s", mountPoint);
    }
    
    if (mntflags & MNT_AUTOMOUNTED) {
        /* Automount volume, don't look in the user home directory */
        *options |= kSMBOptionNoUserPreferences;
    }
    
    if ((altflags & SMBFS_MNT_STREAMS_ON) != SMBFS_MNT_STREAMS_ON) {
        /* They told us to turn off named streams */
        mntOptions |= kSMBMntOptionNoStreams;
    }
    
    if ((altflags & SMBFS_MNT_NOTIFY_OFF) == SMBFS_MNT_NOTIFY_OFF) {
        /* They told us to turn off remote notifications */
        mntOptions |= kSMBMntOptionNoNotifcations;
    }
    
    if ((altflags & SMBFS_MNT_SOFT) == SMBFS_MNT_SOFT) {
        /* Make this a soft mount */
        mntOptions |= kSMBMntOptionSoftMount;
    }
    
    if ((altflags & SMBFS_MNT_TIME_MACHINE) == SMBFS_MNT_TIME_MACHINE) {
        /* Make this a tm mount */
        mntOptions |= kSMBReservedTMMount;
    }
    
    if ((altflags & SMBFS_MNT_HIGH_FIDELITY) == SMBFS_MNT_HIGH_FIDELITY) {
        /* Make this a high fidelity mount */
        *options |= kSMBOptionRequestHiFi;       /* Set Open option */
        mntOptions |= kSMBHighFidelityMount;    /* Set Mount option */
    }
    
    if ((altflags & SMBFS_MNT_DATACACHE_OFF) == SMBFS_MNT_DATACACHE_OFF) {
        /* They told us to turn off data caching */
        mntOptions |= kSMBDataCacheOffMount;
    }
    
    if ((altflags & SMBFS_MNT_MDATACACHE_OFF) == SMBFS_MNT_MDATACACHE_OFF) {
        /* They told us to turn off meta data caching */
        mntOptions |= kSMBMDataCacheOffMount;
    }
    
    if ((altflags & SMBFS_MNT_SESSION_ENCRYPT) == SMBFS_MNT_SESSION_ENCRYPT) {
        /* Force session encryption */
        *options |= kSMBOptionSessionEncrypt;       /* Set Open option */
        mntOptions |= kSMBSessionEncryptMount;
    }
    
    if ((altflags & SMBFS_MNT_SHARE_ENCRYPT) == SMBFS_MNT_SHARE_ENCRYPT) {
        /* Force share encryption */
        mntOptions |= kSMBShareEncryptMount;
    }
    
    /*
     * Build the mount options dictionary
     */
    *mOptions = CFDictionaryCreateMutable(kCFAllocatorDefault, 0,
                                          &kCFTypeDictionaryKeyCallBacks,
                                          &kCFTypeDictionaryValueCallBacks);
    if (*mOptions == NULL) {
        /* Couldn't create the mount option dictionary, error out */
        err(EX_OSERR, "CFDictionaryCreateMutable failed for mount options");
    }
    
    if (strlen(snapshot_time)) {
        /*
         * Snapshot mounts are always read only and force new session
         * Force new session was set earlier in this function.
         */
        mntflags |= MNT_RDONLY;
        
        strRef = CFStringCreateWithCString(NULL, snapshot_time,
                                            kCFStringEncodingUTF8);
        if (strRef != NULL) {
            CFDictionarySetValue (*mOptions, kSnapshotTimeKey, strRef);
            CFRelease(strRef);
            strRef = NULL;
        }
    }
    
    numRef = CFNumberCreate (NULL, kCFNumberSInt32Type, &mntflags);
    if (numRef) {
        /* Put the mount flags into the dictionary */
        CFDictionarySetValue (*mOptions, kNetFSMountFlagsKey, numRef);
        CFRelease(numRef);
    }
    
    if (mntOptions & kSMBMntOptionNoStreams) {
        /* Don't use NTFS Streams even if they are supported by the server.  */
        CFDictionarySetValue (*mOptions, kStreamstMountKey, kCFBooleanFalse);
    }
    
    if (mntOptions & kSMBMntOptionNoNotifcations) {
        /* Don't use Remote Notifications even if they are supported by the server. */
        CFDictionarySetValue (*mOptions, kNotifyOffMountKey, kCFBooleanTrue);
    }
    
    if (mntOptions & kSMBMntOptionSoftMount) {
        /* Mount the volume soft, return time out error durring reconnect. */
        CFDictionarySetValue (*mOptions, kNetFSSoftMountKey, kCFBooleanTrue);
    }
    
    if (mntOptions & kSMBReservedTMMount) {
        /* Mount the volume as a Time Machine mount. */
        CFDictionarySetValue (*mOptions, kTimeMachineMountKey, kCFBooleanTrue);
    }
    
    if (mntOptions & kSMBMntForceNewSession) {
        /* Force a new session */
        CFDictionarySetValue (*mOptions, kNetFSForceNewSessionKey, kCFBooleanTrue);
    }
    
    if (mntOptions & kSMBHighFidelityMount) {
        /* High Fidelity mount */
        CFDictionarySetValue (*mOptions, kHighFidelityMountKey, kCFBooleanTrue);
    }
    
    if (mntOptions & kSMBDataCacheOffMount) {
        /* Disable data caching */
        CFDictionarySetValue (*mOptions, kDataCacheOffMountKey, kCFBooleanTrue);
    }
    
    if (mntOptions & kSMBMDataCacheOffMount) {
        /* Disable meta data caching */
        CFDictionarySetValue (*mOptions, kMDataCacheOffMountKey, kCFBooleanTrue);
    }
    
    if (mntOptions & kSMBSessionEncryptMount) {
        /* Force session encryption */
        CFDictionarySetValue (*mOptions, kSessionEncryptionKey, kCFBooleanTrue);
    }
    
    if (mntOptions & kSMBShareEncryptMount) {
        /* Force share encryption */
        CFDictionarySetValue (*mOptions, kShareEncryptionKey, kCFBooleanTrue);
    }
    
    /*
     * Specify permissions that should be assigned to files and directories. The
     * value must be specified as an octal numbers. A value of zero means use the
     * default values. Not setting these in the dictionary will force the default
     * values to be used.
     */
    if (fileMode || dirMode) {
        if (fileMode) {
            numRef = CFNumberCreate(kCFAllocatorDefault, kCFNumberSInt16Type, &fileMode);
            if (numRef) {
                CFDictionarySetValue (*mOptions, kfileModeKey, numRef);
                CFRelease(numRef);
            }
        }
        if (dirMode) {
            numRef = CFNumberCreate(kCFAllocatorDefault, kCFNumberSInt16Type, &dirMode);
            if (numRef) {
                CFDictionarySetValue (*mOptions, kdirModeKey, numRef);
                CFRelease(numRef);
            }
        }
    }
}


static void
usage(void)
{
	fprintf(stderr, "%s\n",
	"usage: mount_smbfs [-N] [-o options] [-d mode] [-f mode] [-h] [-s] [-v] [-t \"@GMT token\"] \n"
	"                   //"
	"[domain;][user[:password]@]server[/share]"
	" path");

	exit (EX_USAGE);
}
