/*
 * Copyright (c) 2006 - 2009 Apple Inc. All rights reserved.
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

#include <sys/param.h>
#include <sys/mount.h>

#include <NetFS/NetFSPlugin.h>
#include <NetFS/NetFSUtil.h>
#include <NetFS/NetFSPrivate.h>

#include <asl.h>
#include "smb_netfs.h"
#include <netsmb/smb_lib.h>
#include <netsmb/smb_conn.h>
#include <parse_url.h>

/*
 * SMB_CreateSessionRef
 *
 * Load the smbfs kext if need, initialize anything needed by the library  and
 * create a session reference structure, the first element must have our schema
 * as a CFStringRef.
 */
netfsError SMB_CreateSessionRef(void **sessionRef)
{
	int error;
		
	/* Need to initialize the library and load the kext */
	error = smb_load_library();
	if (error) {
		smb_log_info("%s: loading the smb library failed!", error, ASL_LEVEL_ERR, __FUNCTION__);
		*sessionRef = NULL;
		return error;
	}
	/* smb_create_ctx can only fail because of an allocation failure */
	*sessionRef = smb_create_ctx();
	if (*sessionRef == NULL) {
		smb_log_info("%s: creating session refernce failed!\n", ENOMEM, ASL_LEVEL_ERR, __FUNCTION__);
		return ENOMEM;
	}
#ifdef SMB_DEBUG
	smb_log_info("%s: refernce %p\n", 0, ASL_LEVEL_DEBUG, __FUNCTION__, *sessionRef);
#endif // SMB_DEBUG
	return 0;
}

/*
 * SMB_CancelSession
 */
netfsError SMB_Cancel(void *sessionRef) 
{
#ifdef SMB_DEBUG
	smb_log_info("%s: refernce %p\n", 0, ASL_LEVEL_DEBUG, __FUNCTION__, sessionRef);
#endif // SMB_DEBUG
	smb_ctx_cancel_connection((struct smb_ctx *)sessionRef);
	return 0;
}

/*
 * SMB_CloseSession
 */
netfsError SMB_CloseSession(void *sessionRef) 
{
#ifdef SMB_DEBUG
	smb_log_info("%s: refernce %p\n", 0, ASL_LEVEL_DEBUG, __FUNCTION__, sessionRef);
#endif // SMB_DEBUG
	smb_ctx_done(sessionRef);
	return 0;
}

/*
 * SMB_ParseURL
 */
netfsError SMB_ParseURL(CFURLRef url, CFDictionaryRef *urlParms)
{
	*urlParms = NULL;

	if (url == NULL) {
		smb_log_info("%s: failed URL is NULL!", EINVAL, ASL_LEVEL_ERR, __FUNCTION__);
		return EINVAL;
	}
	return smb_url_to_dictionary(url, urlParms);
}

/*
 * SMB_CreateURL
 */
netfsError SMB_CreateURL(CFDictionaryRef urlParms, CFURLRef *url)
{
	*url = NULL;
	
	if (urlParms == NULL) {
		smb_log_info("%s: failed dictionary is NULL!", EINVAL, ASL_LEVEL_ERR, __FUNCTION__);
		return EINVAL;
	}
	return smb_dictionary_to_url(urlParms, url);
}

/*
 * SMB_OpenSession
 */
netfsError SMB_OpenSession(CFURLRef url, void *sessionRef, CFDictionaryRef openOptions, CFDictionaryRef *sessionInfo)
{
	struct smb_ctx *ctx = sessionRef;
	int error = 0;
	
	if ((sessionRef == NULL) || (url == NULL)) {
		smb_log_info("%s: - in parameters are NULL!", EINVAL, ASL_LEVEL_ERR, __FUNCTION__);
		return EINVAL;
	}
	pthread_mutex_lock(&ctx->ctx_mutex);
	/* 
	 * They didn't set any of the authentication dictionary items. Lets try 
	 * Kerberos and if that fails fallback to using user authentication. Now if
	 * the server doesn't support Kerberos then smb_open_session will check
	 * for that and return an EAUTH error. Alo if we haven't connected yet then
	 * smb_open_session will connect before checking to see if the server 
	 * supports Kerberos. So smb_open_session does most of the work for us.
	 */
	if ((openOptions == NULL) ||
		((CFDictionaryGetValue(openOptions, kNetFSUseKerberosKey) == NULL) && 
		(CFDictionaryGetValue(openOptions, kNetFSUseGuestKey) == NULL) && 
		(CFDictionaryGetValue(openOptions, kNetFSUseAnonymousKey) == NULL))) {
		CFMutableDictionaryRef openOptionsKerb;
		
		if (openOptions)
			openOptionsKerb = CFDictionaryCreateMutableCopy(kCFAllocatorDefault, 0, openOptions);
		else
			openOptionsKerb = CFDictionaryCreateMutable(kCFAllocatorDefault, 0 /* capacity */,
								&kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks);

		if (openOptionsKerb) {
			CFDictionarySetValue (openOptionsKerb, kNetFSUseKerberosKey, kCFBooleanTrue);
			error = smb_open_session(sessionRef, url, openOptionsKerb, sessionInfo);
			smb_log_info("%s: No security method selected attempting Kerberos. error = %d", 0, ASL_LEVEL_DEBUG, __FUNCTION__, error);
			CFRelease(openOptionsKerb);
		} else
			error = EAUTH;	/* Should never happen, but just to be safe */
		
		/* The Kerberos attempt failed, now attempt it with user level security */
		if (error == EAUTH)
			error = smb_open_session(sessionRef, url, openOptions, sessionInfo);

	} else
		error = smb_open_session(sessionRef, url, openOptions, sessionInfo);
	pthread_mutex_unlock(&ctx->ctx_mutex);
	if (error)
		smb_log_info("%s: - error = %d!", 0, ASL_LEVEL_DEBUG, __FUNCTION__, error);		
#ifdef SMB_DEBUG
	else 
		smb_log_info("%s: refernce %p\n", 0, ASL_LEVEL_DEBUG, __FUNCTION__, sessionRef);
#endif // SMB_DEBUG
	return error;
}

/*
 * SMB_GetServerInfo
 */
netfsError SMB_GetServerInfo(CFURLRef url, void *sessionRef, CFDictionaryRef openOptions, CFDictionaryRef *serverParms)
{
	struct smb_ctx *ctx = sessionRef;
	int error = 0;

	if ((sessionRef == NULL) || (url == NULL)) {
		smb_log_info("%s: - in parameters are NULL!", EINVAL, ASL_LEVEL_ERR, __FUNCTION__);
		return EINVAL;
	}
	pthread_mutex_lock(&ctx->ctx_mutex);
	error = smb_get_server_info(sessionRef, url, openOptions, serverParms);
	pthread_mutex_unlock(&ctx->ctx_mutex);
	if (error)
		smb_log_info("%s: - error = %d!", 0, ASL_LEVEL_DEBUG, __FUNCTION__, error);		
#ifdef SMB_DEBUG
	else 
		smb_log_info("%s: refernce %p\n", 0, ASL_LEVEL_DEBUG, __FUNCTION__, sessionRef);
#endif // SMB_DEBUG
	return error;
}

/*
 * SMB_EnumerateShares
 */
netfsError SMB_EnumerateShares(void *sessionRef, CFDictionaryRef enumerateOptions, CFDictionaryRef *sharePoints ) 
{
#pragma unused(enumerateOptions)
	struct smb_ctx *ctx = sessionRef;
	int error = 0;
	
	if (sessionRef == NULL) {
		smb_log_info("%s: failed session reference is NULL!", EINVAL, ASL_LEVEL_ERR, __FUNCTION__);
		return EINVAL;
	}
	pthread_mutex_lock(&ctx->ctx_mutex);
	error = smb_enumerate_shares(sessionRef, sharePoints);
	pthread_mutex_unlock(&ctx->ctx_mutex);
	if (error)
		smb_log_info("%s: - error = %d!", 0, ASL_LEVEL_DEBUG, __FUNCTION__, error);		
#ifdef SMB_DEBUG
	else 
		smb_log_info("%s: refernce %p\n", 0, ASL_LEVEL_DEBUG, __FUNCTION__, sessionRef);
#endif // SMB_DEBUG
	return error;
}

/*
 * SMB_Mount
 */
netfsError SMB_Mount(void *sessionRef, CFURLRef url, CFStringRef mPoint, CFDictionaryRef mOptions, CFDictionaryRef *mInfo)
{
	struct smb_ctx *ctx = sessionRef;
	int error = 0;

	if ((sessionRef == NULL) || (mPoint == NULL) || (url == NULL)) {
		smb_log_info("%s: - in parameters are NULL!", EINVAL, ASL_LEVEL_ERR, __FUNCTION__);
		return EINVAL;
	}
	
	pthread_mutex_lock(&ctx->ctx_mutex);
	/* Now deal with the URL */
	if (ctx->ct_url)
		CFRelease(ctx->ct_url);
	ctx->ct_url = CFURLCopyAbsoluteURL(url);
	if (ctx->ct_url)
		error = ParseSMBURL(ctx, SMB_ST_DISK);
	else
		error = ENOMEM;
	
	if (error) 
		smb_log_info("%s: Parsing URL failed!", error, ASL_LEVEL_ERR, __FUNCTION__);
	else
		error = smb_mount(sessionRef, mPoint, mOptions, mInfo);
	pthread_mutex_unlock(&ctx->ctx_mutex);
	if (error)
		smb_log_info("%s: - error = %d!", 0, ASL_LEVEL_DEBUG, __FUNCTION__, error);		
#ifdef SMB_DEBUG
	else 
		smb_log_info("%s: refernce %p\n", 0, ASL_LEVEL_DEBUG, __FUNCTION__, sessionRef);
#endif // SMB_DEBUG
	return error;
}

/*
 * %%% Need to clean this routine up in the future.
 */
netfsError SMB_GetMountInfo(CFStringRef in_Mountpath, CFDictionaryRef *out_MountInfo)
{
	int error;
	char *mountpath;
	struct statfs statbuf;
	char *sharepointname;
	char *url;
	size_t url_length;
	CFStringRef url_CString;
	CFMutableDictionaryRef	mutableDict = NULL;

	*out_MountInfo = NULL;
	mutableDict = CFDictionaryCreateMutable(kCFAllocatorDefault, 0, &kCFTypeDictionaryKeyCallBacks, 
										 &kCFTypeDictionaryValueCallBacks);
	if (mutableDict == NULL)
		return ENOMEM;

	mountpath = NetFSCFStringtoCString(in_Mountpath);
	if (mountpath == NULL) {
		CFRelease(mutableDict);
		return ENOMEM;
	}

	if (statfs(mountpath, &statbuf) == -1) {
		error = errno;
		CFRelease(mutableDict);
		free(mountpath);
		return error;
	}
	free(mountpath);

	smb_ctx_get_user_mount_info(statbuf.f_mntonname, mutableDict);

	sharepointname = statbuf.f_mntfromname;
	/* Skip all leading slashes and backslashes*/
	while ((*sharepointname == '/') || (*sharepointname == '\\')) {
		++sharepointname;
	}
	url_length = sizeof(SMB_PREFIX) + strlen(sharepointname); /* sizeof(SMB_PREFIX) will include the null byte */
	url = malloc(url_length);
	if (url == NULL) {
		CFRelease(mutableDict);
		return ENOMEM;	
	}
	strlcpy(url, SMB_PREFIX, url_length);
	strlcat(url, sharepointname, url_length);
	url_CString = CFStringCreateWithCString(kCFAllocatorDefault, url,
	    kCFStringEncodingUTF8);
	free(url);
	if (url_CString == NULL) {
		CFRelease(mutableDict);		
		return ENOMEM;
	}
	CFDictionarySetValue (mutableDict, kNetFSMountedURLKey, url_CString);

	*out_MountInfo = mutableDict;
	CFRelease(url_CString);
	return 0;
}
