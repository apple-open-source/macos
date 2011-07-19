/*
 * Copyright (c) 2009 - 2010 Apple Inc. All rights reserved.
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

#include <netsmb/smb_lib.h>
#include <netsmb/smb_conn.h>
#include "parse_url.h"
#include "smbclient.h"
#include "smbclient_netfs.h"
#include "smbclient_internal.h"
#include "smbclient_private.h"
#include "ntstatus.h"

#define SMB_PREFIX "smb://"


/*
 * SMBNetFsUnlockSession
 */
void 
SMBNetFsLockSession(SMBHANDLE inConnection)
{
    void *hContext = NULL;
	
	SMBServerContext(inConnection, &hContext);
	pthread_mutex_lock(&((struct smb_ctx *)hContext)->ctx_mutex);
}

/*
 * SMBNetFsUnlockSession
 */
void 
SMBNetFsUnlockSession(SMBHANDLE inConnection)
{
    void *hContext = NULL;
	
	SMBServerContext(inConnection, &hContext);
	pthread_mutex_unlock(&((struct smb_ctx *)hContext)->ctx_mutex);
}

/*
 * SMBNetFsCreateSessionRef
 *
 * Load the smbfs kext if need, initialize anything needed by the library  and
 * create a session reference structure, the first element must have our schema
 * as a CFStringRef.
 */
int32_t 
SMBNetFsCreateSessionRef(SMBHANDLE *outConnection)
{
	void *hContext;
	int error;
	
	*outConnection = NULL;
	/* Need to initialize the library and load the kext */
	error = smb_load_library();
	if (error) {
		return error;
	}
	/* create_smb_ctx can only fail because of an allocation failure */
	hContext = create_smb_ctx();
	if (hContext == NULL) {
		return ENOMEM;
	}
	*outConnection = SMBAllocateAndSetContext(hContext);
	if (*outConnection == NULL) {
        smb_ctx_done(hContext);
		return ENOMEM;
	}
	return 0;
}

/*
 * SMBNetFsCancel
 */
int32_t 
SMBNetFsCancel(SMBHANDLE inConnection) 
{
    void *hContext = NULL;
	NTSTATUS status = SMBServerContext(inConnection, &hContext);

	if (NT_SUCCESS(status)) {
		smb_ctx_cancel_connection(hContext);
		return 0;
	}
	return errno;
}

/*
 * SMBNetFsCloseSession
 */
int32_t 
SMBNetFsCloseSession(SMBHANDLE inConnection) 
{
	NTSTATUS status = SMBReleaseServer(inConnection);
	
	if (NT_SUCCESS(status)) {
		return 0;
	}
	return errno;
}

/*
 * SMBNetFsParseURL
 */
int32_t 
SMBNetFsParseURL(CFURLRef url, CFDictionaryRef *urlParms)
{
	*urlParms = NULL;
	
	if (url == NULL) {
		return EINVAL;
	}
	return smb_url_to_dictionary(url, urlParms);
}

/*
 * SMBNetFsCreateURL
 */
int32_t 
SMBNetFsCreateURL(CFDictionaryRef urlParms, CFURLRef *url)
{
	*url = NULL;
	
	if (urlParms == NULL) {
		return EINVAL;
	}
	return smb_dictionary_to_url(urlParms, url);
}

/*
 * SMBNetFsOpenSession
 */
int32_t 
SMBNetFsOpenSession(CFURLRef url, SMBHANDLE inConnection, CFDictionaryRef 
							openOptions, CFDictionaryRef *sessionInfo)
{
	int error = 0;
    void *hContext = NULL;
	
	SMBServerContext(inConnection, &hContext);
	error = smb_open_session(hContext, url, openOptions, sessionInfo);
	return error;
}

/*
 * SMBNetFsGetServerInfo
 */
int32_t 
SMBNetFsGetServerInfo(CFURLRef url, SMBHANDLE inConnection, 
							  CFDictionaryRef openOptions, CFDictionaryRef *serverParms)
{
	int error = 0;
    void *hContext = NULL;
	
	SMBServerContext(inConnection, &hContext);	
	error = smb_get_server_info(hContext, url, openOptions, serverParms);
	return error;
}

/*
 * SMBNetFsTreeConnectForEnumerateShares
 *
 * When enumerating shares we need a IPC$ tree connection. This is really only 
 * need by the old RAP calls, since the DCE/RPC code will create a whole new
 * session.
 */
int32_t
SMBNetFsTreeConnectForEnumerateShares(SMBHANDLE inConnection)
{
	int error = 0;
    void *hContext = NULL;
	
	SMBServerContext(inConnection, &hContext);
	error = smb_ctx_setshare(hContext, "IPC$");
	if (!error) {
		error = smb_share_connect(hContext);
	}
	return error;
}

/*
 * SMB_Mount
 */
int32_t 
SMBNetFsMount(SMBHANDLE inConnection, CFURLRef url, CFStringRef mPoint, 
					  CFDictionaryRef mOptions, CFDictionaryRef *mInfo,
					  void (*callout)(void  *, void *), void *args)
{
	int error = 0;
    struct smb_ctx *hContext = NULL;
	
	SMBServerContext(inConnection, (void **)&hContext);
	/* Now deal with the URL, need a better way of handling this in the future */
	if (hContext->ct_url) {
		CFRelease(hContext->ct_url);
	}
	hContext->ct_url = CFURLCopyAbsoluteURL(url);
	if (hContext->ct_url) {
		error = ParseSMBURL(hContext);
	} else {
		error = ENOMEM;
	}
	if (error) {
		return error;
	}
	return smb_mount(hContext, mPoint, mOptions, mInfo, callout, args);
}

/*
 * %%% Need to clean this routine up in the future.
 */
int32_t SMBNetFsGetMountInfo(CFStringRef in_Mountpath, CFDictionaryRef *out_MountInfo)
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
	
	mountpath = CStringCreateWithCFString(in_Mountpath);
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
