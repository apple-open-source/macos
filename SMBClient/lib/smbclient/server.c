/*
 * Copyright (c) 2009 - 2020 Apple Inc. All rights reserved.
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

#include "ntstatus.h"
#include "smbclient.h"
#include "smbclient_private.h"
#include "smbclient_internal.h"

#include <stdlib.h>
#include <time.h>
#include <pwd.h>
#include <unistd.h>
#include <readpassphrase.h>
#include <libkern/OSAtomic.h>
#include <sys/param.h>
#include <sys/mount.h>
#include <netsmb/smb_lib.h>
#include <netsmb/smb_conn.h>
#include <CoreFoundation/CoreFoundation.h>
#include <NetFS/NetFS.h>
#include <NetFS/NetFSPrivate.h>
#include <netsmb/smb_dev_2.h>

typedef int32_t refcount_t;

struct smb_server_handle
{
    volatile refcount_t refcount;
    struct smb_ctx * context;
};

static NTSTATUS
SMBLibraryInit(void)
{
    if (smb_load_library() != 0) {
		errno = ENXIO;
        return STATUS_NO_SUCH_DEVICE;
    }

    return STATUS_SUCCESS;
}

/* Allocate a new SMBHANDLE. */
static NTSTATUS
SMBAllocateServer(
    SMBHANDLE * outConnection,
	const char * targetServer)
{
    SMBHANDLE inConnection;
	int error;

    inConnection = calloc(1, sizeof(struct smb_server_handle));
    if (inConnection == NULL) {
		/* calloc sets errno for us */ 
        return STATUS_NO_MEMORY;
    }

	error = create_smb_ctx_with_url(&inConnection->context, targetServer);
	if (error || (inConnection->context == NULL)) {
		free(inConnection); /* We alocate, free it on error */
		if (error) {
			errno = error;
			return STATUS_INVALID_PARAMETER;
		} else {
			errno = ENOMEM;
		}
        return STATUS_NO_MEMORY;
    }

    SMBRetainServer(inConnection);
    *outConnection = inConnection;
    return STATUS_SUCCESS;
}

SMBHANDLE
SMBAllocateAndSetContext(
	void * phContext)
{
    SMBHANDLE inConnection = calloc(1, sizeof(struct smb_server_handle));
    if (inConnection == NULL) {
		/* calloc sets errno for us */ 
        return NULL;
    }
    SMBRetainServer(inConnection);
	inConnection->context = phContext;
    return inConnection;
}

static CFMutableDictionaryRef
SMBCreateDefaultOptions( uint64_t options)
{
    CFMutableDictionaryRef optionDict;

    optionDict = CFDictionaryCreateMutable(kCFAllocatorDefault, 0 /* capacity */,
            &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks);
    if (!optionDict) {
		errno = ENOMEM;
        return NULL;
    }

	/* We always allow loopback connections, from the framework */
	CFDictionarySetValue(optionDict, kNetFSAllowLoopbackKey, kCFBooleanTrue);
	
	if (options & kSMBOptionNoUserPreferences) {
		CFDictionarySetValue(optionDict, kNetFSNoUserPreferencesKey, kCFBooleanTrue);
	} else {
		CFDictionarySetValue(optionDict, kNetFSNoUserPreferencesKey, kCFBooleanFalse);
	}
	
	if (options & kSMBOptionForceNewSession) {
		/* Force a new session. */
		CFDictionarySetValue (optionDict, kNetFSForceNewSessionKey, kCFBooleanTrue);
	} else {
		/* Share an existing session if possible. */
		CFDictionarySetValue(optionDict, kNetFSForceNewSessionKey, kCFBooleanFalse);
	}

    if (options & kSMBOptionRequestHiFi) {
        /*
         * Requesting to use HiFi mode, so can only share an existing session
         * if the HiFi modes match.
         */
        CFDictionarySetValue (optionDict, kHighFidelityMountKey, kCFBooleanTrue);
    } else {
        /* Not requesting HiFi */
        CFDictionarySetValue(optionDict, kHighFidelityMountKey, kCFBooleanFalse);
    }

    return optionDict;
}

/* Prompt for a password and set it on the SMB context. */
static Boolean
SMBPasswordPrompt(
    SMBHANDLE hConnection,
	uint64_t options)
{
    void *      hContext = NULL;
    NTSTATUS    status;
    char *      passwd;
    char        passbuf[SMB_MAXPASSWORDLEN + 1];
    char        prompt[128];

	/* They told us not to prompt */
	if (options & kSMBOptionNoPrompt)
		return false;

	status = SMBServerContext(hConnection, &hContext);
	if (!NT_SUCCESS(status)) {
        return false;
    }

    /* 
	 * If the password is already set, don't prompt. Since anonymous and guest
	 * both have an empty password this will protect us from prompting in
	 * those cases.
     */
	if (((struct smb_ctx *)hContext)->ct_flags & SMBCF_EXPLICITPWD) {
        return false;
    }

   /* 
	 * If the target hasn't set a user name, let's assume that we should use
     * the current username. This is almost always what the caller wants, when
	 * being prompted for a password. The above check protect us from overriding
	 * anonymouse connections.
     */
	if (((struct smb_ctx *)hContext)->ct_setup.ioc_user[0] == '\0') {
        struct passwd * pwent;
		
        pwent = getpwuid(geteuid());
        if (pwent) {
            smb_ctx_setuser(hContext, pwent->pw_name);
        }
    }
	
    snprintf(prompt, sizeof(prompt), "Password for %s: ",
            ((struct smb_ctx *)hContext)->serverName);

    /* If we have a TTY, read a password and retry ... */
    passwd = readpassphrase(prompt, passbuf, sizeof(passbuf), RPP_REQUIRE_TTY);
    if (passwd) {
        smb_ctx_setpassword(hContext, passwd, TRUE);
        memset(passbuf, 0, sizeof(passbuf));
    }
	return true;
}

static NTSTATUS
SMBServerConnect(
        SMBHANDLE   hConnection,
        CFURLRef    targetUrl,
        CFMutableDictionaryRef netfsOptions,
        SMBAuthType authType)
{
    void * hContext;
    NTSTATUS status;
    int err;

    status = SMBServerContext(hConnection, &hContext);
	if (!NT_SUCCESS(status)) {
        return status;
    }

    /* Reset all the authentication options. This puts us into the
     * state of requiring user authentication (ie. forces NTLM).
     */
    CFDictionarySetValue(netfsOptions, kNetFSUseGuestKey, kCFBooleanFalse);
    CFDictionarySetValue(netfsOptions, kNetFSUseAnonymousKey, kCFBooleanFalse);

    switch (authType) {
		case kSMBAuthTypeAuthenticated:
        case kSMBAuthTypeKerberos:
        case kSMBAuthTypeUser:
			CFDictionarySetValue(netfsOptions, kNetFSUseAuthenticationInfoKey, kCFBooleanFalse);
			break;
        case kSMBAuthTypeGuest:
			/* Don't try Kerberos */
 			CFDictionarySetValue(netfsOptions, kNetFSUseAuthenticationInfoKey, kCFBooleanFalse);
			CFDictionarySetValue(netfsOptions, kNetFSUseGuestKey, kCFBooleanTrue);
            break;
        case kSMBAuthTypeAnonymous:
			/* Don't try Kerberos */
			CFDictionarySetValue(netfsOptions, kNetFSUseAuthenticationInfoKey, kCFBooleanFalse);
            CFDictionarySetValue(netfsOptions, kNetFSUseAnonymousKey, kCFBooleanTrue);
            break;
        default:
            return STATUS_INVALID_PARAMETER;
    }

    err = smb_open_session(hContext, targetUrl, netfsOptions,
            NULL /* [OUT] session_info */);

	/* XXX map real NTSTATUS code */
	if (err) {
		if (err< 0) {
			/* 
			 * A negative error is a special NetFSAuth error. We have no method
			 * to tell us if the calling routine understands these errors, so
			 * always set errno to a number defined in sys/errno.h.
			 */
			switch (errno) {
				case SMB_ENETFSNOAUTHMECHSUPP:
					errno = ENOTSUP;
					break;
				case SMB_ENETFSNOPROTOVERSSUPP:
					errno = ENOTSUP;
					break;
				case SMB_ENETFSACCOUNTRESTRICTED:
				case SMB_ENETFSPWDNEEDSCHANGE:
				case SMB_ENETFSPWDPOLICY:
				default:
					errno = EAUTH;
					break;
			}
		} else {
			errno = err;
		}
		if (err == EAUTH) {
			return STATUS_LOGON_FAILURE;
		}
		return STATUS_CONNECTION_REFUSED;
	}

    return STATUS_SUCCESS;
}

NTSTATUS
SMBServerContext(
    SMBHANDLE hConnection,
    void ** phContext)
{
    if (hConnection == NULL || hConnection->context == NULL) {
		errno = EINVAL;
        return STATUS_INVALID_HANDLE;
    }

    *phContext = hConnection->context;
    return STATUS_SUCCESS;
}

NTSTATUS
SMBMountShare(
	SMBHANDLE inConnection,
	const char * targetShare,
	const char * mountPoint)
{
    return SMBMountShareEx(inConnection, targetShare, mountPoint, 0, 0, 0, 0, NULL, NULL);
}


NTSTATUS
SMBMountShareEx(
	SMBHANDLE	inConnection,
	const char	*targetShare,
	const char	*mountPoint,
	unsigned	mountFlags,
	uint64_t	mountOptions,
	mode_t 		fileMode,
	mode_t 		dirMode,
	void (*callout)(void  *, void *), 
	void *args)
{
    NTSTATUS    status = STATUS_SUCCESS;
	CFMutableDictionaryRef mOptions = NULL;
	CFNumberRef numRef = NULL;
	
	mOptions = CFDictionaryCreateMutable(kCFAllocatorDefault, 0, &kCFTypeDictionaryKeyCallBacks, 
										 &kCFTypeDictionaryValueCallBacks);
	if (mOptions == NULL) {
		/* Couldn't create the mount option dictionary, error out */
		errno = ENOMEM;
		status = STATUS_NO_MEMORY;
        goto done;
	}
	
	numRef = CFNumberCreate (NULL, kCFNumberSInt32Type, &mountFlags);
	if (numRef) {
		/* Put the mount flags into the dictionary */
		CFDictionarySetValue (mOptions, kNetFSMountFlagsKey, numRef);
		CFRelease(numRef);
	}
	
	if (mountOptions & kSMBMntOptionNoStreams) {
		/* Don't use NTFS Streams even if they are supported by the server.  */
		CFDictionarySetValue (mOptions, kStreamstMountKey, kCFBooleanFalse);
	}
		
	if (mountOptions & kSMBMntOptionNoNotifcations) {
		/* Don't use Remote Notifications even if they are supported by the server. */
		CFDictionarySetValue (mOptions, kNotifyOffMountKey, kCFBooleanTrue);
	}
		
	if (mountOptions & kSMBMntOptionSoftMount) {
		/* Mount the volume soft, return time out error durring reconnect. */
		CFDictionarySetValue (mOptions, kNetFSSoftMountKey, kCFBooleanTrue);
	}
	
	if (mountOptions & kSMBReservedTMMount) {
		/* Mount the volume as a Time Machine mount. */
		CFDictionarySetValue (mOptions, kTimeMachineMountKey, kCFBooleanTrue);
	}
    
	if (mountOptions & kSMBMntForceNewSession) {
		/* Force a new session */
		CFDictionarySetValue (mOptions, kNetFSForceNewSessionKey, kCFBooleanTrue);
	}
    
    if (mountOptions & kSMBHighFidelityMount) {
        /* High Fidelity mount */
        CFDictionarySetValue (mOptions, kHighFidelityMountKey, kCFBooleanTrue);
    }

    if (mountOptions & kSMBDataCacheOffMount) {
        /* Disable data caching */
        CFDictionarySetValue (mOptions, kDataCacheOffMountKey, kCFBooleanTrue);
    }

    if (mountOptions & kSMBMDataCacheOffMount) {
        /* Disable meta data caching */
        CFDictionarySetValue (mOptions, kMDataCacheOffMountKey, kCFBooleanTrue);
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
				CFDictionarySetValue (mOptions, kfileModeKey, numRef);
				CFRelease(numRef);
			}
		}
		if (dirMode) {
			numRef = CFNumberCreate(kCFAllocatorDefault, kCFNumberSInt16Type, &dirMode);
			if (numRef) {
				CFDictionarySetValue (mOptions, kdirModeKey, numRef);
				CFRelease(numRef);
			}
		}
	}

    status = SMBMountShareExDict(inConnection, targetShare, mountPoint,
                                 mOptions, NULL, callout, args);

done:
	if (mOptions) {
		CFRelease(mOptions);
	}
	return status;
}

NTSTATUS
SMBMountShareExDict(
    SMBHANDLE inConnection,
    const char *targetShare,
    const char *mountPoint,
    CFDictionaryRef mountOptions,
    CFDictionaryRef *retMountInfo,
    void (*callout)(void *, void *),
    void *args)
{
    NTSTATUS    status = STATUS_SUCCESS;
    int         err = 0;
    void *      hContext = NULL;
    CFStringRef mountPtRef = NULL;

    status = SMBServerContext(inConnection, &hContext);
    if (!NT_SUCCESS(status)) {
        /* Couldn't get the context? */
        goto done;
    }

    /* Get the mount point */
    if (mountPoint) {
        mountPtRef = CFStringCreateWithCString(kCFAllocatorDefault, mountPoint,
                                               kCFStringEncodingUTF8);
    }

    if (mountPtRef == NULL) {
        /* No mount point */
        errno = ENOMEM;
        status = STATUS_NO_MEMORY;
        goto done;
    }

    /* Set the share if they gave us one */
    if (targetShare) {
        err = smb_ctx_setshare(hContext, targetShare);
    }

    if (err == 0) {
        err = smb_mount(hContext, mountPtRef, mountOptions, retMountInfo,
                        callout, args);
    }

    if (err) {
        errno = err;
        status = STATUS_UNSUCCESSFUL;
        goto done;
    }

done:
    if (mountPtRef) {
        CFRelease(mountPtRef);
    }
    return status;
}

NTSTATUS
SMBOpenServer(
    const char * targetServer,
    SMBHANDLE * outConnection)
{
    return SMBOpenServerEx(targetServer, outConnection, 0);
}

NTSTATUS 
SMBOpenServerEx(
    const char * targetServer,
    SMBHANDLE * outConnection,
    uint64_t    options)
{
    NTSTATUS    status;
    int         err;
    void *      hContext;

    CFMutableDictionaryRef netfsOptions = NULL;
	CFDictionaryRef ServerParams = NULL;

    *outConnection = NULL;

    status = SMBLibraryInit();
	if (!NT_SUCCESS(status)) {
        goto done;
    }

    netfsOptions = SMBCreateDefaultOptions(options);
    if (netfsOptions == NULL) {
        status = STATUS_NO_MEMORY;
        goto done;
    }

    status = SMBAllocateServer(outConnection, targetServer);
	if (!NT_SUCCESS(status)) {
        goto done;
    }

    status = SMBServerContext(*outConnection, &hContext);
	if (!NT_SUCCESS(status)) {
        goto done;
    }
	
	err = smb_get_server_info(hContext, NULL, netfsOptions, &ServerParams);
	if (err) {
        /* XXX map real NTSTATUS code */
        status = STATUS_CONNECTION_REFUSED;
		errno = err;
		goto done;
    }
	/*
	 * They didn't set the force new session option and we have a shared session,
	 * then we are done. We have a connection and we are authenticated.
	 */
	if (!(options & kSMBOptionForceNewSession) && (((struct smb_ctx *)hContext)->ct_session_shared)) {
		goto authDone;
	}
	/*
	 * They have guest as the username in the url, then they want us to 
	 * force guest access.
	 */
	if (((struct smb_ctx *)hContext)->ct_setup.ioc_userflags & SMBV_GUEST_ACCESS) {
		options |= kSMBOptionUseGuestOnlyAuth;
	}
	
	/* Connect using Guest Access only */
	if (options & kSMBOptionUseGuestOnlyAuth) {
		status = SMBServerConnect(*outConnection, NULL, netfsOptions, kSMBAuthTypeGuest);
		goto authDone;
	}

	/* Connect using Anonymous Access only  */
	if (options & kSMBOptionUseAnonymousOnlyAuth) {
		status = SMBServerConnect(*outConnection, NULL, netfsOptions, kSMBAuthTypeAnonymous);
		goto authDone;
	}
	
	/* Attempt an authenticated connect, could be kerberos or ntlm. */
	status = SMBServerConnect(*outConnection, NULL, netfsOptions, kSMBAuthTypeAuthenticated);
	if (NT_SUCCESS(status)) {
		goto authDone;
	}
	
	/* See if we need to prompt for a password */
	if (SMBPasswordPrompt(*outConnection, options)) {
		/* Attempt an authenticated connect again , could be kerberos or ntlm. */
		status = SMBServerConnect(*outConnection, NULL, netfsOptions, kSMBAuthTypeAuthenticated);
		if (NT_SUCCESS(status)) {
			goto authDone;
		}
	}

	/* Kerberos and NTLM failed, attempt Guest access if option set */
	if (options & kSMBOptionAllowGuestAuth) {
		status = SMBServerConnect(*outConnection, NULL, netfsOptions, kSMBAuthTypeGuest);
		if (NT_SUCCESS(status)) {
			goto authDone;
		}
	}
	
	/* Kerberos and NTLM failed, attempt Anonymous access if option set */
	if (options & kSMBOptionAllowAnonymousAuth) {
		status = SMBServerConnect(*outConnection, NULL, netfsOptions, kSMBAuthTypeAnonymous);
		if (NT_SUCCESS(status)) {
			goto authDone;
		}
	}

authDone:

	if (!NT_SUCCESS(status)) {
		goto done;
	}
		
	if (options & kSMBOptionSessionOnly) {
		goto done;
	}
	
    /* 
	 * If the target doesn't contain a share name, let's assume that the
     * caller means IPC$, unless a share name is required.
     */
    if (!((struct smb_ctx *)hContext)->ct_origshare) {
        err = smb_ctx_setshare(hContext, "IPC$");
        if (err) {
			if (err == ENAMETOOLONG) {
				status = STATUS_NAME_TOO_LONG;
			} else {
				status = STATUS_NO_MEMORY;
			}
			errno = err;
            goto done;
        }
    }
	
    /* OK, now we have a session but no tree connection yet. */
    err = smb_share_connect((*outConnection)->context);
    if (err) {
        status = STATUS_BAD_NETWORK_NAME;
		errno = err;
        goto done;
    }
    
    status = STATUS_SUCCESS;

done:
    if (netfsOptions) {
        CFRelease(netfsOptions);
    }
	if (ServerParams) {
		CFRelease(ServerParams);
	}
    if ((!NT_SUCCESS(status)) && *outConnection) {
        SMBReleaseServer(*outConnection);
        *outConnection = NULL;
    }

    return status;
}

/*
 * Get a statfs buffer for the volume at mntpoint.
 * Use getfsstat so we can specify MNT_NOWAIT.
 * WARNING: The incoming arg of mntpoint must be in the form of
 *          /Volumes/<blah> and not /System/Volumes/Data/Volumes/<blah>
 *          because getfsstat(2) returns f_mntonname in /Volumes/<blah> form
 */
static int
getfsstat_by_mount(const char* mntpoint, struct statfs *fs)
{
    int res, count, i;
    size_t bufsize = 0;
    struct statfs *bufp, *bp;
    int found = 0;

    /* See what we need to allocate */
    count = getfsstat(NULL, 0, MNT_NOWAIT);
    if (count <= 0) {
        /* getfsstat() sets errno for us */
        return -1;
    }

    bufsize = count * sizeof(*fs);
    bufp = malloc(bufsize);
    if (bufp == NULL) {
        /* malloc() sets errno for us */
        return -1;
    }

    /* getfsstat takes in an int as a size parameter */
    res = getfsstat(bufp, (int) bufsize, MNT_NOWAIT);

    /* Find the statfs buffer that matches the mntpoint */
    if (res > 0) {
        res = -1;
        for (i = 0; i < count; i++) {
            bp = &bufp[i];

            if (strncmp(bp->f_mntonname, mntpoint, strlen(bp->f_mntonname) + 1) == 0) {
                *fs = *bp; /* struct copy */
                res = 0;
                found = 1;
                break;
            }
        }
    }
    else {
        /* getfsstat() sets errno for us */
    }

    if ((res > 0) && (found == 0)) {
        /* no matching mntpoint was found, need to set errno */
        res = -1;
        errno = ENOENT;
    }

    free(bufp);
    return res;
}

NTSTATUS
SMBOpenServerWithMountPoint(
	const char * pTargetMountPath,
	const char * pTargetTreeName,
	SMBHANDLE * outConnection,
	uint64_t    options)
{
	NTSTATUS	status;
	int         err;
	void *      hContext;
	struct statfs statbuf;
    char *real_mountpoint = NULL;

	*outConnection = NULL;
	
	status = SMBLibraryInit();
	if (!NT_SUCCESS(status)) {
		goto done;
	}
	
    /* Clean up any possible /System/Volumes/Data/Volumes/<blah> paths */
    real_mountpoint = realpath(pTargetMountPath, NULL);
    if (real_mountpoint == NULL) {
        status = STATUS_OBJECT_PATH_NOT_FOUND;
        goto done;
    }

    /* Need to get the mount from name, use that as the URL */
	err = getfsstat_by_mount(real_mountpoint, &statbuf);
	if (err) {
		status = STATUS_OBJECT_PATH_NOT_FOUND;
		goto done;
	}
	
	status = SMBAllocateServer(outConnection, statbuf.f_mntfromname);
	if (!NT_SUCCESS(status)) {
		goto done;
	}

	status = SMBServerContext(*outConnection, &hContext);
	if (!NT_SUCCESS(status)) {
		goto done;
	}

    /*
     * <67014611> Note that the server context here was built from just the
     * f_mntfromname which is pretty much just an URL. Specific mount options
     * like HiFi are not set in the context because those options are hidden
     * inside the mount.
     */

	/* Need to clear out the user name field */
	smb_ctx_setuser(hContext, "");
	err = findMountPointSession(hContext, pTargetMountPath);
	if (err) {
		status = STATUS_OBJECT_NAME_NOT_FOUND;
		errno = err;
		goto done;
    }

	if (options & kSMBOptionSessionOnly) {
		goto done;
	}
	
	/*  No tree name, let's assume that the caller means IPC$ */
	if (!pTargetTreeName) {
		pTargetTreeName = "IPC$";
	}
	err = smb_ctx_setshare(hContext, pTargetTreeName);
	if (err) {
		if (err == ENAMETOOLONG) {
			status = STATUS_NAME_TOO_LONG;
		} else {
			status = STATUS_NO_MEMORY;
		}
		errno = err;
		goto done;
	}
	
	/* OK, now we have a session but no tree connection yet. */
	err = smb_share_connect((*outConnection)->context);
	if (err) {
		status = STATUS_BAD_NETWORK_NAME;
		errno = err;
		goto done;
	}
	
	status = STATUS_SUCCESS;
	
done:
	if ((!NT_SUCCESS(status)) && *outConnection) {
		SMBReleaseServer(*outConnection);
		*outConnection = NULL;
	}

    if (real_mountpoint != NULL) {
        free(real_mountpoint);
    }

    return status;
}

NTSTATUS
SMBGetServerProperties(
		SMBHANDLE	inConnection,
		void		*outProperties,
		uint32_t	inVersion,
		size_t		inPropertiesSize)
{
    NTSTATUS    status;
    void *      hContext;
    uint32_t    session_flags;
	SMBServerPropertiesV1 * properties = NULL;
	
	/* 
	 * Do some sanity checking here, currently we only support version
	 * one properties, make sure they aren't null and the size matches.
	 */
	if ((outProperties == NULL) || (inVersion != kPropertiesVersion) ||
		(inPropertiesSize != sizeof(SMBServerPropertiesV1))) {
		errno = EINVAL;
		return STATUS_INVALID_LEVEL;
	}
    status = SMBServerContext(inConnection, &hContext);
	if (!NT_SUCCESS(status)) {
        return status;
    }
	properties = (SMBServerPropertiesV1 *)outProperties;
    memset(properties, 0, sizeof(*properties));
	
    /* Update the session properties to make sure we have a current verison. */
	smb_get_session_properties(hContext);
    session_flags = ((struct smb_ctx *)hContext)->ct_session_flags;
	
    if (session_flags & SMBV_GUEST_ACCESS) {
        properties->authType = kSMBAuthTypeGuest;
    } else if (session_flags & SMBV_ANONYMOUS_ACCESS) {
        properties->authType = kSMBAuthTypeAnonymous;
    } else if (session_flags & SMBV_KERBEROS_ACCESS) {
        properties->authType = kSMBAuthTypeKerberos;
    } else {
        properties->authType = kSMBAuthTypeUser;
    }
	
	if (session_flags & SMBV_NETWORK_SID) {
        properties->internalFlags |= kHasNtwrkSID;
	}
    
    /* Set the kLanmanOn flag according to the lanman_on preference */
	if (((struct smb_ctx *)hContext)->prefs.lanman_on) {
        properties->internalFlags |= kLanmanOn;
	}
	
    properties->dialect = kSMBDialectSMB;
	
    properties->capabilities = ((struct smb_ctx *)hContext)->ct_session_caps;
	
	properties->maxTransactBytes = ((struct smb_ctx *)hContext)->ct_session_txmax;	
	properties->maxReadBytes = ((struct smb_ctx *)hContext)->ct_session_rxmax;		
	properties->maxWriteBytes = ((struct smb_ctx *)hContext)->ct_session_wxmax;
    properties->treeOptionalSupport = ((struct smb_ctx *)hContext)->ct_sh.ioc_optionalSupport;
	if (((struct smb_ctx *)hContext)->serverName) {
		strlcpy(properties->serverName, ((struct smb_ctx *)hContext)->serverName, 
				sizeof(properties->serverName));
	}
    return STATUS_SUCCESS;
}


NTSTATUS
SMBGetMultichannelProperties(SMBHANDLE inConnection, void *outAttrs)
{
    NTSTATUS status = STATUS_SUCCESS;
    struct smb_ctx *ctx = NULL;

    if (!inConnection || !outAttrs)
        return STATUS_INVALID_PARAMETER;

    status = SMBServerContext(inConnection, (void **)&ctx);
    if (!NT_SUCCESS(status)) {
        os_log_error(OS_LOG_DEFAULT, "%s: failed to get smb_ctx, syserr = %s",
                     __FUNCTION__, strerror(errno));
        return status;
    }

    struct smbioc_multichannel_properties *mc_props = outAttrs;
    memset(mc_props, 0, sizeof(struct smbioc_multichannel_properties));
    mc_props->ioc_version = SMB_IOC_STRUCT_VERSION;
    if (smb_ioctl_call(ctx->ct_fd, SMBIOC_MULTICHANNEL_PROPERTIES, mc_props) == -1) {
        os_log_error(OS_LOG_DEFAULT, "%s: Getting the multi-channel properties failed, syserr = %s",
                     __FUNCTION__, strerror(errno));
        return errno;
    }

    return status;
}

NTSTATUS
SMBGetNicInfoProperties(SMBHANDLE inConnection, void *outAttrs, uint8_t inClientOrServer)
{
    NTSTATUS status = STATUS_SUCCESS;
    struct smb_ctx *ctx = NULL;

    if (!inConnection || !outAttrs)
        return STATUS_INVALID_PARAMETER;

    status = SMBServerContext(inConnection, (void **)&ctx);
    if (!NT_SUCCESS(status)) {
        os_log_error(OS_LOG_DEFAULT, "%s: failed to get smb_ctx, syserr = %s",
                     __FUNCTION__, strerror(errno));
        return status;
    }

    struct smbioc_nic_info *nic_info = outAttrs;
    memset(nic_info, 0, sizeof(struct smbioc_nic_info));
    nic_info->ioc_version = SMB_IOC_STRUCT_VERSION;
    nic_info->flags = inClientOrServer;
    if (smb_ioctl_call(ctx->ct_fd, SMBIOC_NIC_INFO, nic_info) == -1) {
        os_log_error(OS_LOG_DEFAULT, "%s: Getting the network interfaces properties failed, syserr = %s",
                     __FUNCTION__, strerror(errno));
        return errno;
    }

    return status;
}

NTSTATUS
SMBGetMultichannelSessionInfoProperties(SMBHANDLE inConnection, void *outAttrs)
{
    NTSTATUS status = STATUS_SUCCESS;
    struct smb_ctx *ctx = NULL;

    if (!inConnection || !outAttrs)
        return STATUS_INVALID_PARAMETER;

    status = SMBServerContext(inConnection, (void **)&ctx);
    if (!NT_SUCCESS(status)) {
        os_log_error(OS_LOG_DEFAULT, "%s: failed to get smb_ctx, syserr = %s",
                     __FUNCTION__, strerror(errno));
        return status;
    }

    struct smbioc_session_properties *session_info = outAttrs;
    memset(session_info, 0, sizeof(struct smbioc_session_properties));
    session_info->ioc_version = SMB_IOC_STRUCT_VERSION;
    if (smb_ioctl_call(ctx->ct_fd, SMBIOC_SESSION_PROPERTIES, session_info) == -1) {
        os_log_error(OS_LOG_DEFAULT,
                     "%s: Getting the session mc info properties failed, syserr = %s",
                     __FUNCTION__, strerror(errno));
        return errno;
    }

    return status;
}

NTSTATUS
SMBGetShareAttributes(SMBHANDLE inConnection, void *outAttrs)
{
    struct smbioc_session_properties session_prop;
    struct smbioc_share_properties share_prop;
    NTSTATUS status;
    struct smb_ctx *ctx;
    SMBShareAttributes *sattrs = (SMBShareAttributes *)outAttrs;
    size_t name_len;
    
    if (!inConnection || !sattrs)
        return STATUS_INVALID_PARAMETER;
    
    status = SMBServerContext(inConnection, (void **)&ctx);
    if (!NT_SUCCESS(status)) {
        os_log_error(OS_LOG_DEFAULT, "%s: failed to get smb_ctx, syserr = %s",
					 __FUNCTION__, strerror(errno));
        return status;
    }
    
    memset(&session_prop, 0, sizeof(session_prop));
	session_prop.ioc_version = SMB_IOC_STRUCT_VERSION;
	if (smb_ioctl_call(ctx->ct_fd, SMBIOC_SESSION_PROPERTIES, &session_prop) == -1) {
		os_log_error(OS_LOG_DEFAULT, "%s: Getting the session properties failed, syserr = %s",
					 __FUNCTION__, strerror(errno));
        return errno;
    }
    else {
        sattrs->session_uid = session_prop.uid;
        sattrs->session_smb1_caps = session_prop.smb1_caps;
        sattrs->session_smb2_caps = session_prop.smb2_caps;
        sattrs->session_flags = session_prop.flags;
        sattrs->session_misc_flags = session_prop.misc_flags;
        sattrs->session_hflags = session_prop.hflags;
        sattrs->session_hflags2 = session_prop.hflags2;

        if (sattrs->session_misc_flags & SMBV_MNT_SNAPSHOT) {
            strlcpy(sattrs->snapshot_time, session_prop.snapshot_time,
                    sizeof(sattrs->snapshot_time));
        }
    }
    
    memset(&share_prop, 0, sizeof(share_prop));
	share_prop.ioc_version = SMB_IOC_STRUCT_VERSION;
	if (smb_ioctl_call(ctx->ct_fd, SMBIOC_SHARE_PROPERTIES, &share_prop) == -1) {
		os_log_error(OS_LOG_DEFAULT, "%s: Getting the share properties failed, syserr = %s",
					 __FUNCTION__, strerror(errno));
       return errno;
    }
    else {
        sattrs->ss_flags = share_prop.share_flags;
        sattrs->ss_type = share_prop.share_type;
        sattrs->ss_caps = share_prop.share_caps;
        sattrs->ss_attrs = share_prop.attributes;
    }
    
    sattrs->ss_fstype = ctx->ct_sh.ioc_fstype;
    
    memset(sattrs->server_name, 0, kMaxSrvNameLen);
    name_len = (kMaxSrvNameLen >= strlen(ctx->serverName)) ? kMaxSrvNameLen : strlen(ctx->serverName);
    strlcpy(sattrs->server_name, ctx->serverName, name_len);
    
    return STATUS_SUCCESS;
}

NTSTATUS
SMBRetainServer(
    SMBHANDLE inConnection)
{
    OSAtomicIncrement32(&inConnection->refcount);
    return STATUS_SUCCESS;
}

NTSTATUS
SMBReleaseServer(
    SMBHANDLE inConnection)
{
    refcount_t refcount;

    refcount = OSAtomicDecrement32(&inConnection->refcount);
    if (refcount == 0) {
        smb_ctx_done(inConnection->context);
        free(inConnection);
    }

    return STATUS_SUCCESS;
}

time_t
SMBConvertGMT( const char *gmt_string)
{
    struct tm gmt_time = {0};
    time_t local_time;
    int ret;

    /* Convert the @GMT token to local time */
    ret = sscanf(gmt_string, "@GMT-%d.%d.%d-%d.%d.%d",
                 &gmt_time.tm_year, &gmt_time.tm_mon, &gmt_time.tm_mday,
                 &gmt_time.tm_hour, &gmt_time.tm_min, &gmt_time.tm_sec);
    if (ret != 6) {
        os_log_error(OS_LOG_DEFAULT, "%s: sscanf failed on <%s>",
                     __FUNCTION__, gmt_string);
        return(0);
    }

    gmt_time.tm_year -= 1900;   /* tm_year is years from 1900 */
    gmt_time.tm_mon -= 1;       /* tm_mon starts at 0 for Jan */

    local_time = timegm(&gmt_time);

    return(local_time);
}


/* vim: set sw=4 ts=4 tw=79 et: */
