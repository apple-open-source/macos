/*
 * Copyright (c) 2008 Apple Inc. All rights reserved.
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

#include <crt_externs.h>

#include <unistd.h>
#include <syslog.h>
#include <spawn.h>
#include <pthread.h>

#include <sys/param.h>
#include <sys/mount.h>

#include <CoreServices/CoreServices.h>

#include <NetFS/NetFSPlugin.h>
#include <NetFS/NetFSUtil.h>
#include <NetFS/NetFSUtilPrivate.h>

/*
 * NFS plugin for NetFS.
 */
struct NFSSessionRef {
	pthread_mutex_t mtx;    /* mutex protecting the following fields */
	pid_t   mounter_pid;    /* PID of process trying to do the mount, or 0 if there's no such process */
	int     cancel_requested; /* 1 if netfs_Cancel has been called */
};

/*
 * NFS_CreateSessionRef
 *
 * Create a session reference structure; the first element must have our scheme
 * as a CFStringRef.
 */
static netfsError
NFS_CreateSessionRef(void **out_SessionRef)
{
	struct NFSSessionRef *sessionRef;

	sessionRef = malloc(sizeof *sessionRef);
	if (sessionRef == NULL) {
		return ENOMEM;
	}
	if (pthread_mutex_init(&sessionRef->mtx, NULL) == -1) {
		syslog(LOG_ERR, "NFS_CreateSessionRef: pthread_mutex_init failed!");
		free(sessionRef);
		return EINVAL;
	}
	sessionRef->mounter_pid = 0;            /* no mounter yet */
	sessionRef->cancel_requested = 0;       /* nothing canceled yet */
	*out_SessionRef = sessionRef;
	return 0;
}

/*
 * NFS_Cancel
 * Cancel an in-progress operation.  Most operations don't block, so we
 * don't do anything to cancel them.  Mounts might block; we set a flag
 * on the session structure so that if we haven't run mount_nfs yet, we
 * won't, and, if we have run it (i.e., if the mounter_pid is non-zero),
 * we kill it.
 */
static netfsError
NFS_Cancel(void *in_SessionRef)
{
	struct NFSSessionRef *sessionRef = in_SessionRef;
	int ret = 0;

	pthread_mutex_lock(&sessionRef->mtx);
	if (sessionRef->mounter_pid != 0) {
		if (kill(sessionRef->mounter_pid, SIGTERM) == -1) {
			ret = errno;
		}
	} else {
		sessionRef->cancel_requested = 1;
	}
	pthread_mutex_unlock(&sessionRef->mtx);
	return ret;
}

/*
 * NFS_CloseSession
 */
static netfsError
NFS_CloseSession(void *in_SessionRef)
{
	struct NFSSessionRef *sessionRef = in_SessionRef;

	NFS_Cancel(in_SessionRef);
	pthread_mutex_destroy(&sessionRef->mtx);
	free(in_SessionRef);
	return 0;
}

/*
 * NFS_ParseURL
 */
static netfsError
NFS_ParseURL(CFURLRef in_URL, CFDictionaryRef *out_URLParms)
{
	CFMutableDictionaryRef mutableDict = NULL;
	SInt32 error = 0;
	CFStringRef scheme;
	CFStringRef host;
	CFStringRef port;
	Boolean isAbsolute;
	CFStringRef path, unescaped_path, options;

	*out_URLParms = NULL;

	/* check for a valid URL */
	if (!CFURLCanBeDecomposed(in_URL)) {
		error = EINVAL;
		goto exit;
	}

	/* create and return the parameters dictionary */
	mutableDict = CFDictionaryCreateMutable(NULL, 0,
	    &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks);
	if (mutableDict == NULL) {
		error = ENOMEM;
		goto exit;
	}

	/* Get the scheme */
	scheme = CFURLCopyScheme(in_URL);
	if (scheme == NULL) {
		error = ENOMEM;
		goto exit;
	}
	CFDictionarySetValue(mutableDict, kNetFSSchemeKey, scheme);
	CFRelease(scheme);

	/* Get the host name or address and the port */
	error = NetFSCopyHostAndPort(in_URL, &host, &port);
	if (error != 0) {
		goto exit;
	}

	CFDictionarySetValue(mutableDict, kNetFSHostKey, host);
	CFRelease(host);

	if (port != NULL) {
		CFDictionarySetValue(mutableDict, kNetFSAlternatePortKey,
		    port);
		CFRelease(port);
	}

	/* Get optional path */
	path = CFURLCopyStrictPath(in_URL, &isAbsolute);
	if (path != NULL) {
		unescaped_path = CFURLCreateStringByReplacingPercentEscapes(NULL,
		    path, CFSTR(""));
		if (unescaped_path == NULL) {
			CFRelease(path);
			error = ENOMEM;
			goto exit;
		}
		CFRelease(path);
		CFDictionarySetValue(mutableDict, kNetFSPathKey, unescaped_path);
		CFRelease(unescaped_path);
	}

	/* Get optional options */
	options = CFURLCopyQueryString(in_URL, NULL);
	if (options != NULL) {
		CFDictionarySetValue(mutableDict, kNetFSURLOptionsKey, options);
		CFRelease(options);
	}

	/* we have no errors, so return the properties dictionary */
	*out_URLParms = mutableDict;

exit:
	if (error) {
		if (mutableDict != NULL) {
			CFRelease(mutableDict);
		}
	}

	return error;
}

/* CFURLCreateStringByAddingPercentEscapes is the only C option for escaping URL's, so we have to keep it. */
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wdeprecated-declarations"

/*
 * NFS_CreateURL
 */
static netfsError
NFS_CreateURL(CFDictionaryRef in_URLParms, CFURLRef *out_URL)
{
	SInt32 error = 0;
	CFMutableStringRef urlStringM = NULL;
	CFURLRef myURL = NULL;
	CFStringRef scheme;
	CFStringRef host;
	CFStringRef port;
	CFStringRef path;
	CFStringRef options;
	CFStringRef escapedPath;

	*out_URL = NULL;

	/* Get the required scheme */
	scheme = (CFStringRef) CFDictionaryGetValue(in_URLParms,
	    kNetFSSchemeKey);   /* dont need to CFRelease this */
	if (scheme == NULL) {
		error = EINVAL;
		goto exit;
	}
	urlStringM = CFStringCreateMutableCopy(NULL, 0, scheme);
	if (urlStringM == NULL) {
		error = ENOMEM;
		goto exit;
	}

	CFStringAppend(urlStringM, CFSTR("://"));

	/* Get the required host */
	host = (CFStringRef) CFDictionaryGetValue(in_URLParms, kNetFSHostKey);
	if (host == NULL) {
		error = EINVAL;
		goto exit;
	}
	host = CFURLCreateStringByAddingPercentEscapes(NULL, host,
	    CFSTR("[]"), CFSTR("/@:,?=;&+$"), kCFStringEncodingUTF8);

	/* add the host */
	CFStringAppend(urlStringM, host);
	CFRelease(host);

	/* Check for optional alternate port */
	port = (CFStringRef) CFDictionaryGetValue(in_URLParms,
	    kNetFSAlternatePortKey);
	if (port != NULL) {
		port = CFURLCreateStringByAddingPercentEscapes(NULL, port,
		    NULL, NULL, kCFStringEncodingUTF8);
		CFStringAppend(urlStringM, CFSTR(":"));
		CFStringAppend(urlStringM, port);
		CFRelease(port);
	}
	CFStringAppend(urlStringM, CFSTR("/"));

	/* Check for optional path */
	path = (CFStringRef) CFDictionaryGetValue(in_URLParms, kNetFSPathKey);
	if (path != NULL) {
		/* Escape  */
		escapedPath = CFURLCreateStringByAddingPercentEscapes(NULL,
		    path, NULL, CFSTR("/?"), kCFStringEncodingUTF8);
		CFStringAppend(urlStringM, escapedPath);
		CFRelease(escapedPath);
	}

	/* Check for mount options */
	options = (CFStringRef) CFDictionaryGetValue(in_URLParms, kNetFSURLOptionsKey);
	if (options != NULL) {
		CFStringAppend(urlStringM, CFSTR("?"));
		CFStringAppend(urlStringM, options);
	}

	/* convert to CFURL */
	myURL = CFURLCreateWithString(NULL, urlStringM, NULL);
	CFRelease(urlStringM);
	urlStringM = NULL;
	if (myURL == NULL) {
		error = ENOMEM;
		goto exit;
	}

	/* we have no errors, so return the CFURL */
	*out_URL = myURL;

exit:
	if (urlStringM != NULL) {
		CFRelease(urlStringM);
	}

	return error;
}

#pragma clang diagnostic pop

/*
 * NFS_OpenSession
 */
static netfsError
NFS_OpenSession(CFURLRef in_URL, __unused void *in_SessionRef,
    __unused CFDictionaryRef openOptions, CFDictionaryRef *out_sessionInfo)
{
	netfsError error = 0;
	CFMutableDictionaryRef mutableDict;

	if (out_sessionInfo != NULL) {
		/* No dictionary yet. */
		*out_sessionInfo = NULL;
	}

	/* check for a valid URL */
	if (!CFURLCanBeDecomposed(in_URL)) {
		error = EINVAL;
		goto exit;
	}

	if (out_sessionInfo != NULL) {
		/* create and return the parameters dictionary */
		mutableDict = CFDictionaryCreateMutable(NULL, 0,
		    &kCFTypeDictionaryKeyCallBacks,
		    &kCFTypeDictionaryValueCallBacks);
		if (mutableDict == NULL) {
			error = ENOMEM;
			goto exit;
		}
#if 1
		/*
		 * XXX - the NetAuthAgent gets upset if this dictionary is
		 * empty.
		 */
		CFDictionarySetValue(mutableDict, kNetFSConnectedMultiUserKey,
		    kCFBooleanTrue);
#endif

		/* we have no errors, so return the properties dictionary */
		*out_sessionInfo = mutableDict;
	}

	/*
	 * No need to do something more here; we don't support getting
	 * server information or getting a list of shares, so we don't
	 * connect to the server until we do a mount.
	 */
exit:
	return error;
}

/*
 * NFS_GetServerInfo
 */
static netfsError
NFS_GetServerInfo(CFURLRef in_URL, __unused void *in_SessionRef,
    __unused CFDictionaryRef in_GetInfoOptions,
    CFDictionaryRef *out_serverParms)
{
	netfsError error = 0;
	CFMutableDictionaryRef mutableDict;
	CFStringRef host;
	CFStringRef port;

	/* No dictionary yet. */
	*out_serverParms = NULL;

	/* check for a valid URL */
	if (!CFURLCanBeDecomposed(in_URL)) {
		error = EINVAL;
		goto exit;
	}

	/* create and return the parameters dictionary */
	mutableDict = CFDictionaryCreateMutable(NULL, 0,
	    &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks);
	if (mutableDict == NULL) {
		error = ENOMEM;
		goto exit;
	}

	/* Get the host name or address and the port */
	error = NetFSCopyHostAndPort(in_URL, &host, &port);
	if (error != 0) {
		goto exit;
	}

	/* Make that the display name for the host. */
	CFDictionarySetValue(mutableDict, kNetFSServerDisplayNameKey, host);
	CFRelease(host);
	if (port != NULL) {
		CFRelease(port);
	}

	/*
	 * No notion of "guest" with NFS.
	 */
	CFDictionarySetValue(mutableDict, kNetFSSupportsGuestKey,
	    kCFBooleanFalse);

	/*
	 * NOTE: there's no way to determine whether the server
	 * supports Kerberos or not (and whether it does is per-
	 * export in any case).
	 */

	/*
	 * No notion of authenticated mounts with NFS; multiple
	 * identities can use the same mount, and the authentication
	 * is done as needed.
	 */
	CFDictionarySetValue(mutableDict, kNetFSNoMountAuthenticationKey,
	    kCFBooleanTrue);
#if 0
	CFDictionarySetValue(mutableDict, kNetFSConnectedMultiUserKey,
	    kCFBooleanTrue);
#endif

	/* we have no errors, so return the properties dictionary */
	*out_serverParms = mutableDict;

exit:
	return error;
}

/*
 * NFS_EnumerateShares
 */
static netfsError
NFS_EnumerateShares(__unused void *in_SessionRef,
    __unused CFDictionaryRef enumerateOptions,
    __unused CFDictionaryRef *sharePoints)
{
	/*
	 * XXX - for NFSv2 and NFSv3, we could get a list of exports
	 * from the server.  For NFSv4, there's no such notion.
	 */
	return ENOTSUP;
}

static void
ArgsArrayRelease(__unused CFAllocatorRef allocator, const void *value)
{
	if (value) {
		free((void *)value);
	}
}

const CFArrayCallBacks ArgsArrayCallBacks = {
	0, /* version */
	NULL, /* retain */
	ArgsArrayRelease, /* release */
	NULL, /* copyDescription /*/
	NULL, /* equal */
};

static int
NFS_Create_MountOption_From_URL(CFStringRef urlOption, char **result)
{
	int error = 0;
	CFRange range;
	char *buff = NULL;
	CFStringRef suffix = NULL;
	CFMutableStringRef option = NULL;

	// Sanity check
	if (urlOption == NULL || CFStringHasPrefix(urlOption, CFSTR("options=")) == FALSE) {
		error = EINVAL;
		goto out_free;
	}

	// Remove prefix
	range = CFRangeMake(strlen("options="), CFStringGetLength(urlOption));
	suffix = CFStringCreateWithSubstring(kCFAllocatorDefault, urlOption, range);
	if (!suffix) {
		error = ENOMEM;
		goto out_free;
	}

	// create nfs mount option
	option = CFStringCreateMutable(kCFAllocatorDefault, 0);
	if (!option) {
		error = ENOMEM;
		goto out_free;
	}

	// Replace ":" with "="
	CFStringAppend(option, CFSTR("-o"));
	CFStringAppend(option, suffix);
	range = CFRangeMake(0, CFStringGetLength(option));
	CFStringFindAndReplace(option, CFSTR(":"), CFSTR("="), range, kCFCompareEqualTo);

	buff = malloc(NAME_MAX);
	if (buff == NULL) {
		error = ENOMEM;
		goto out_free;
	}

	if (CFStringGetCString(option, buff, NAME_MAX, kCFStringEncodingUTF8) == FALSE) {
		error = ENOMEM;
		goto out_free;
	}

	*result = buff;
	buff = NULL;

out_free:
	if (buff) {
		free(buff);
	}
	if (suffix) {
		CFRelease(suffix);
	}
	if (option) {
		CFRelease(option);
	}
	return error;
}

#define ARGS_SIZE 100

#define AddArg(arg) \
	do { \
	        if (i >= ARGS_SIZE) { \
	                error = ENOMEM; \
	                goto out_free; \
	        } \
	        args[i] = arg; \
	        i++; \
} while (0)

/*
 * NFS_Mount
 *
 * We aren't necessarily running as root, but we might have to mount from
 * a server that requires that requests come from reserved ports
 * (because somebody's managed to convince themselves that said requirement
 * somehow increases security), and the NFS client might be configured
 * to default to using reserved ports, so the mount might have to be
 * done as root.
 *
 * mount_nfs is set-UID root so that it can get a reserved port; we run
 * it, rather than doing the mount ourselves.
 */
static netfsError
NFS_Mount(void *in_SessionRef, CFURLRef in_URL,
    CFStringRef in_Mountpoint, CFDictionaryRef in_MountOptions,
    CFDictionaryRef *out_MountInfo)
{
	struct NFSSessionRef *sessionRef = in_SessionRef;
	CFMutableDictionaryRef mutableDict = NULL;
	netfsError error = 0;
	CFDictionaryRef urlParms = NULL;
	CFMutableStringRef mountObject = NULL;
	CFStringRef host;
	CFStringRef path;
	char *mount_object = NULL;
	CFNumberRef mountFlagsRef;
	SInt32 mountFlags;
	struct statfs *mntbuf = NULL;
	int mntsize;
	size_t bufsize;
	CFStringRef out_Mountpoint = NULL;
	char *args[ARGS_SIZE];
	int i;
	int is_ftp;
	char *mount_path = NULL;
	pid_t child, pid;
	int status;
	CFBooleanRef bool_ref;
	CFArrayRef optionsArray = NULL;
	CFStringRef mountOptions = NULL;
	CFMutableArrayRef argsArray = NULL;

	/* No dictionary yet. */
	*out_MountInfo = NULL;

	mountObject = CFStringCreateMutable(NULL, 0);
	if (mountObject == NULL) {
		error = ENOMEM;
		goto out_free;
	}
	error = NFS_ParseURL(in_URL, &urlParms);
	if (error != 0) {
		goto out_free;
	}

	/* Get the required host */
	host = (CFStringRef) CFDictionaryGetValue(urlParms, kNetFSHostKey);
	if (host == NULL) {
		error = EINVAL;
		goto out_free;
	}
	CFStringAppend(mountObject, host);
	CFStringAppend(mountObject, CFSTR(":"));

	/* Check for optional path */
	CFStringAppend(mountObject, CFSTR("/"));
	path = (CFStringRef) CFDictionaryGetValue(urlParms, kNetFSPathKey);
	if (path != NULL) {
		/*
		 * Un-escape characters in the path.
		 */
		CFStringRef unescaped_path;

		unescaped_path = CFURLCreateStringByReplacingPercentEscapes(NULL,
		    path, CFSTR(""));
		if (unescaped_path == NULL) {
			error = EINVAL;
			goto out_free;
		}
		CFStringAppend(mountObject, unescaped_path);
		CFRelease(unescaped_path);
	}

	/*
	 * Convert the item to mount to a C string, so we can check
	 * whether it's already mounted.
	 */
	mount_object = NetFSCFStringtoCString(mountObject);

	mutableDict = CFDictionaryCreateMutable(NULL, 0,
	    &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks);
	if (mutableDict == NULL) {
		error = ENOMEM;
		goto out_free;
	}

	/*
	 * Get the mount flags, so we can see whether we *should* check
	 * whether it's already mounted - we only do so if this will
	 * be a "Finder-visible" mount (i.e., a mount that's not an
	 * automount or a "don't browse this from the Finder" mount).
	 */
	mountFlagsRef = (CFNumberRef) CFDictionaryGetValue(in_MountOptions,
	    kNetFSMountFlagsKey);
	if (mountFlagsRef != NULL) {
		(void)CFNumberGetValue(mountFlagsRef,
		    kCFNumberSInt32Type, &mountFlags);
	} else {
		mountFlags = 0; /* none specified, default to 0 */
	}
	if (!(mountFlags & (MNT_DONTBROWSE | MNT_AUTOMOUNTED))) {
		/*
		 * This will be a "Finder-visible" mount.
		 * See what we have mounted.
		 */
		mntsize = getfsstat(NULL, 0, MNT_NOWAIT);
		if (mntsize == -1) {
			error = errno;
			goto out_free;
		}
		bufsize = 0;
		mntbuf = NULL;
		while (bufsize <= mntsize * sizeof(struct statfs)) {
			if (mntbuf != NULL) {
				free(mntbuf);
			}
			bufsize = (mntsize + 1) * sizeof(struct statfs);
			mntbuf = (struct statfs *)malloc(bufsize);
			if (mntbuf == NULL) {
				error = ENOMEM;
				goto out_free;
			}
			/* getfsstat takes in an int as a size */
			mntsize = getfsstat(mntbuf, (int) bufsize, MNT_NOWAIT);
			if (mntsize == -1) {
				error = errno;
				goto out_free;
			}
		}

		/*
		 * See if any of the existing "Finder-visible" mounts
		 * are NFS mounts of the same object.
		 *
		 * XXX - this won't match different host names that map
		 * to the same host.
		 *
		 * Allow multiple mounts for ftp.
		 */
		is_ftp = strncmp(mount_object, "ftp://", strlen("ftp://")) == 0;
		for (i = 0; i < mntsize && !is_ftp; i++) {
			if (strcmp(mntbuf[i].f_fstypename, "nfs") != 0) {
				continue;
			}
			if (mntbuf[i].f_flags & (MNT_DONTBROWSE | MNT_AUTOMOUNTED)) {
				continue;
			}
			if (strcmp(mntbuf[i].f_mntfromname, mount_object) == 0) {
				/*
				 * Yup, it's already mounted.
				 * Return the path it's mounted on.
				 */
				out_Mountpoint = CFStringCreateWithCString(kCFAllocatorDefault,
				    mntbuf[i].f_mntonname,
				    kCFStringEncodingUTF8);
				if (out_Mountpoint == NULL) {
					error = ENOMEM;
					goto out_free;
				}
				CFDictionarySetValue(mutableDict,
				    kNetFSMountPathKey, out_Mountpoint);
				*out_MountInfo = mutableDict;
				mutableDict = NULL; // Don't release this object
				error = EEXIST;
				goto out_free;
			}
		}
	}

	/*
	 * They're not, or we don't care whether they are; do the mount.
	 */
	i = 0;
	AddArg("mount_nfs");
	if (in_MountOptions != NULL) {
		if (mountFlags & MNT_RDONLY) {
			AddArg("-ordonly");
		}
		if (mountFlags & MNT_SYNCHRONOUS) {
			AddArg("-osync");
		}
		if (mountFlags & MNT_NOEXEC) {
			AddArg("-onoexec");
		}
		if (mountFlags & MNT_NOSUID) {
			AddArg("-onosuid");
		}
		if (mountFlags & MNT_NODEV) {
			AddArg("-onodev");
		}
		if (mountFlags & MNT_ASYNC) {
			AddArg("-oasync");
		}
		if (mountFlags & MNT_DONTBROWSE) {
			AddArg("-onobrowse");
		}
		if (mountFlags & MNT_IGNORE_OWNERSHIP) {
			AddArg("-onoowners");
		}
		if (mountFlags & MNT_AUTOMOUNTED) {
			AddArg("-oautomounted");
		}
		if (mountFlags & MNT_NOATIME) {
			AddArg("-onoatime");
		}
		if (mountFlags & MNT_QUARANTINE) {
			AddArg("-oquarantine");
		}
		if (mountFlags & MNT_MULTILABEL) {
			AddArg("-omultilabel");
		}

		/*
		 * If we've been asked to softmount by netfs, then
		 * we set the deadtimeout to 45 seconds to mimic AFP.
		 * When we get a "squishy" mount option we should add
		 * that as well. We don't actually use the softmount option
		 * since most programs don't seem to handle ETIMEDOUT and
		 * afp "softmounts" don't generate ETIMEDOUT either.
		 */
		bool_ref = (CFBooleanRef) CFDictionaryGetValue(in_MountOptions, kNetFSSoftMountKey);
		if (bool_ref != NULL) {
			if (CFBooleanGetValue(bool_ref)) {
				AddArg("-odeadtimeout=45");
			}
		}
	}

	/* Parse Mount options provided by the URL */
	mountOptions = (CFStringRef) CFDictionaryGetValue(urlParms, kNetFSURLOptionsKey);
	if (mountOptions) {
		argsArray = CFArrayCreateMutable(NULL, 0, &ArgsArrayCallBacks);
		if (argsArray == NULL) {
			error = ENOMEM;
			goto out_free;
		}
		optionsArray = CFStringCreateArrayBySeparatingStrings(NULL, mountOptions, CFSTR("?"));
		if (optionsArray == NULL) {
			error = ENOMEM;
			goto out_free;
		}
		for (int j = 0; j < CFArrayGetCount(optionsArray); j++) {
			char *arg = NULL;
			error = NFS_Create_MountOption_From_URL(CFArrayGetValueAtIndex(optionsArray, j), &arg);
			if (error) {
				goto out_free;
			}
			if (arg == NULL) {
				continue;
			}
			CFArrayAppendValue(argsArray, arg);
			AddArg(arg);
		}
	}

	AddArg(mount_object);
	mount_path = NetFSCFStringtoCString(in_Mountpoint);
	if (mount_path == NULL) {
		error = ENOMEM;
		goto out_free;
	}
	AddArg(mount_path);
	AddArg(NULL);

	pthread_mutex_lock(&sessionRef->mtx);

	/*
	 * Has somebody tried to cancel this operation?
	 */
	if (sessionRef->cancel_requested) {
		/*
		 * Yes.  Don't start the mount, just quit.
		 */
		sessionRef->cancel_requested = 0;
		pthread_mutex_unlock(&sessionRef->mtx);
		error = ECANCELED;
		goto out_free;
	}
	error = posix_spawn(&child, "/sbin/mount_nfs", NULL, NULL,
	    args, (*_NSGetEnviron()));
	if (error != 0) {
		goto out_free;
	}
	sessionRef->mounter_pid = child;
	pthread_mutex_unlock(&sessionRef->mtx);

	/*
	 * Wait for the child to complete.
	 */
	for (;;) {
		pid = waitpid(child, &status, 0);
		if (pid == child) {
			break;
		}
		if (pid == -1 && errno != EINTR) {
			syslog(LOG_ERR, "Error %m while waiting for mount");
			error = EIO;
			goto out_free;
		}
	}
	pthread_mutex_lock(&sessionRef->mtx);
	sessionRef->cancel_requested = 0;
	sessionRef->mounter_pid = 0;
	pthread_mutex_unlock(&sessionRef->mtx);
	if (WIFEXITED(status)) {
		error = WEXITSTATUS(status);
	} else if (WIFSIGNALED(status)) {
		if (WTERMSIG(status) == SIGTERM || WTERMSIG(status) == SIGKILL) {
			error = ECANCELED;      /* assume somebody killed a hanging mount */
		} else {
			error = EIO;
		}
	} else {
		error = EIO;
	}

	if (error == 0) {
		/*
		 * Mount succeeded.
		 */
		CFDictionarySetValue(mutableDict, kNetFSMountPathKey,
		    in_Mountpoint);
		*out_MountInfo = mutableDict;
		mutableDict = NULL; // Don't release this object
	}

out_free:
	if (urlParms) {
		CFRelease(urlParms);
	}
	if (mutableDict) {
		CFRelease(mutableDict);
	}
	if (mountObject) {
		CFRelease(mountObject);
	}
	if (optionsArray) {
		CFRelease(optionsArray);
	}
	if (out_Mountpoint) {
		CFRelease(out_Mountpoint);
	}
	if (argsArray) {
		CFRelease(argsArray);
	}
	if (mount_object) {
		free(mount_object);
	}
	if (mount_path) {
		free(mount_path);
	}
	if (mntbuf) {
		free(mntbuf);
	}
	return error;
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
	} else {
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

#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wdeprecated-declarations"

static netfsError
NFS_GetMountInfo(CFStringRef in_Mountpath, CFDictionaryRef *out_MountInfo)
{
	int error;
	char *mountpath;
	struct statfs statbuf = {0};
	char *authority, *path;
	char *url;
	CFStringRef url_CFString, tmp_CFString;
	const void *keys[1], *values[1];

	*out_MountInfo = NULL;

	mountpath = NetFSCFStringtoCString(in_Mountpath);
	if (mountpath == NULL) {
		return ENOMEM;
	}

	if (getfsstat_by_mount(mountpath, &statbuf) == -1) {
		error = errno;
		free(mountpath);
		return error;
	}
	free(mountpath);

	path = statbuf.f_mntfromname;
	if (*path == '[') {
		/*
		 * If the server has an IPv6 address enclosed in brackets,
		 * e.g. [2620:149:4:f01:21e:c2ff:fe06:19ab]:/Volumes/Stuff
		 * then skip to the closing bracket.
		 */
		path = strchr(path, ']');
		if (path == NULL) {
			return EINVAL;
		}
		path++; // now should be on the colon
	}

	/*
	 * Look for the ':' separating the host name from the path.
	 */
	path = strchr(path, ':');
	if (path == NULL) {
		return EINVAL;  /* *that's* not good */
	}
	*path++ = '\0'; /* split host from path */
	authority = statbuf.f_mntfromname;

	error = asprintf(&url, "nfs://%s%s", authority, path);
	if (error == -1) {
		return errno;
	}
	tmp_CFString = CFStringCreateWithCString(kCFAllocatorDefault, url,
	    kCFStringEncodingUTF8);
	free(url);
	if (tmp_CFString == NULL) {
		return ENOMEM;
	}
	url_CFString = CFURLCreateStringByAddingPercentEscapes(kCFAllocatorDefault, tmp_CFString,
	    NULL, NULL, kCFStringEncodingUTF8);
	CFRelease(tmp_CFString);
	if (url_CFString == NULL) {
		return ENOMEM;
	}

	keys[0] = kNetFSMountedURLKey;
	values[0] = url_CFString;

	*out_MountInfo = CFDictionaryCreate(NULL, keys, values, 1,
	    &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks);
	CFRelease(url_CFString);
	if (*out_MountInfo == NULL) {
		return ENOMEM;
	}
	return 0;
}

#pragma clang diagnostic pop

/* NFS URLMount factory ID: 746BAEA4-F34F-11D5-A562-003065A0E6DE */
#define kNFSNetFSInterfaceFactoryID (CFUUIDGetConstantUUIDWithBytes(NULL, 0x74, 0x6b, 0xae, 0xa4, 0xf3, 0x4f, 0x11, 0xd5, 0xa5, 0x62, 0x00, 0x30, 0x65, 0xa0, 0xe6, 0xde))

/*
 *  NetFS Type implementation:
 */
static NetFSMountInterface_V1 gNFSNetFSInterfaceFTbl = {
	NULL,                           /* IUNKNOWN_C_GUTS: _reserved */
	NetFSQueryInterface,            /* IUNKNOWN_C_GUTS: QueryInterface */
	NetFSInterface_AddRef,          /* IUNKNOWN_C_GUTS: AddRef */
	NetFSInterface_Release,         /* IUNKNOWN_C_GUTS: Release */
	NFS_CreateSessionRef,           /* CreateSessionRef */
	NFS_GetServerInfo,              /* GetServerInfo */
	NFS_ParseURL,                   /* ParseURL */
	NFS_CreateURL,                  /* CreateURL */
	NFS_OpenSession,                /* OpenSession */
	NFS_EnumerateShares,            /* EnumerateShares */
	NFS_Mount,                      /* Mount */
	NFS_Cancel,                     /* Cancel */
	NFS_CloseSession,               /* CloseSession */
	NFS_GetMountInfo,               /* GetMountInfo */
};

void *NFSNetFSInterfaceFactory(CFAllocatorRef allocator, CFUUIDRef typeID);

void *
NFSNetFSInterfaceFactory(__unused CFAllocatorRef allocator, CFUUIDRef typeID)
{
	if (CFEqual(typeID, kNetFSTypeID)) {
		NetFSInterface *result = NetFS_CreateInterface(kNFSNetFSInterfaceFactoryID, &gNFSNetFSInterfaceFTbl);
		return result;
	} else {
		return NULL;
	}
}
