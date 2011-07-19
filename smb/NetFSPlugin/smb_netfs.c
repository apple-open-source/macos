/*
 * Copyright (c) 2006 - 2010 Apple Inc. All rights reserved.
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
#include <asl.h>

#include <NetFS/NetFSPlugin.h>
#include <NetFS/NetFSUtil.h>
#include <NetFS/NetFSPrivate.h>
#include <NetFS/NetFSUtilPrivate.h>

#include <smbclient/smbclient.h>
#include <smbclient/smbclient_netfs.h>
#include <smbclient/smbclient_internal.h>

#include "smb_netfs.h"
#include "netshareenum.h"
#include "SetNetworkAccountSID.h"

/*
 * SMB_GetTraceMessageScheme
 *
 * See whether we were passed a scheme to use when logging failures.  If so, we 
 * log them with NetFSLogToMessageTracer. The scheme string needs to be freed
 * by the calling process.
 */
static char *SMB_GetTraceMessageScheme(CFDictionaryRef options)
{
	CFStringRef schemeRef;
	
	if (!options) {
		return NULL;
	}
	schemeRef = CFDictionaryGetValue(options, kNetFSTraceMessageSchemeKey);
	if (!schemeRef) {
		return NULL;
	}
	return NetFSCFStringtoCString(schemeRef);
}

/*
 * SMB_CreateSessionRef
 *
 * Load the smbfs kext if need, initialize anything needed by the library  and
 * create a session reference structure, the first element must have our schema
 * as a CFStringRef.
 */
static netfsError SMB_CreateSessionRef(void **outConnection)
{
	int error = SMBNetFsCreateSessionRef((SMBHANDLE *)outConnection);
		
	if (*outConnection == NULL) {
		SMBLogInfo("%s: creating smb handle failed, syserr = %s", 
					 ASL_LEVEL_ERR, __FUNCTION__, strerror(error));
	}
	return error;
}

/*
 * SMB_CancelSession
 */
static netfsError SMB_Cancel(void *inConnection) 
{
	return SMBNetFsCancel(inConnection);
}

/*
 * SMB_CloseSession
 */
static netfsError SMB_CloseSession(void *inConnection) 
{
	return SMBNetFsCloseSession(inConnection);
}

/*
 * SMB_ParseURL
 */
static netfsError SMB_ParseURL(CFURLRef url, CFDictionaryRef *urlParms)
{
	return SMBNetFsParseURL(url, urlParms);
}

/*
 * SMB_CreateURL
 */
static netfsError SMB_CreateURL(CFDictionaryRef urlParms, CFURLRef *url)
{
	return SMBNetFsCreateURL(urlParms, url);
}

/*
 * SMB_OpenSession
 */
static netfsError SMB_OpenSession(CFURLRef url, void *inConnection, 
								  CFDictionaryRef openOptions, 
								  CFDictionaryRef *sessionInfo)
{
	int error = 0;
	char *tmscheme = SMB_GetTraceMessageScheme(openOptions);	
	
	if ((inConnection == NULL) || (url == NULL))  {
		error = EINVAL;
		NetFSLogToMessageTracer(tmscheme, "checking parameters in SMB_OpenSession", error);
		goto done;
	}
	SMBNetFsLockSession(inConnection);	
	error = SMBNetFsOpenSession(url, inConnection, openOptions, sessionInfo);
	if (error) {
		SMBLogInfo("%s: - error = %d!", ASL_LEVEL_DEBUG, __FUNCTION__, error);		
		/* Tracing handled in smb_open_session */
	}
	SMBNetFsUnlockSession(inConnection);
done:
	if (tmscheme) {
		free(tmscheme);
	}
	return error;
}

/*
 * SMB_GetServerInfo
 */
static netfsError SMB_GetServerInfo(CFURLRef url, void *inConnection, 
									CFDictionaryRef openOptions, 
									CFDictionaryRef *serverParms)
{
	int error;
	
	if ((inConnection == NULL) || (url == NULL)) {
		return EINVAL;
	}
	SMBNetFsLockSession(inConnection);
	error = SMBNetFsGetServerInfo(url, inConnection, openOptions, serverParms);
	if (error) {
		SMBLogInfo("%s: - error = %d!", ASL_LEVEL_DEBUG, __FUNCTION__, error);		
	}
	SMBNetFsUnlockSession(inConnection);
	return error;
}

/*
 * SMB_EnumerateShares
 */
static netfsError SMB_EnumerateShares(void *inConnection, 
									  CFDictionaryRef enumerateOptions, 
									  CFDictionaryRef *sharePoints ) 
{
#pragma unused(enumerateOptions)
	int error;

	if (inConnection == NULL) {
		return EINVAL;
	}
	SMBNetFsLockSession(inConnection);
	/*
	 * Make sure we have a tree connect to IPC$.This is safe because all
	 * the NetAuth routines that care about the share name will reset it.
	 */
	error = SMBNetFsTreeConnectForEnumerateShares(inConnection);
	if (!error) {
		error = smb_netshareenum(inConnection, sharePoints, TRUE);
	}
	if (error) {
		SMBLogInfo("%s: - error = %d!", ASL_LEVEL_DEBUG, __FUNCTION__, error);		
	}
	SMBNetFsUnlockSession(inConnection);
	return error;
}

/*
 * SMB_Mount
 */
static netfsError SMB_Mount(void *inConnection, CFURLRef url, CFStringRef mPoint, 
					 CFDictionaryRef mOptions, CFDictionaryRef *mInfo)
{
	int error;
	char *tmscheme = SMB_GetTraceMessageScheme(mOptions);	

	if ((inConnection == NULL) || (mPoint == NULL) || (url == NULL)) {
		error = EINVAL;
		NetFSLogToMessageTracer(tmscheme, "checking parameters in SMB_Mount", error);
		goto done;
	}
	SMBNetFsLockSession(inConnection);
	error =  SMBNetFsMount(inConnection,url, mPoint, mOptions, mInfo, setNetworkAccountSID, NULL);
	if (error) {
		SMBLogInfo("%s: - error = %d!", ASL_LEVEL_DEBUG, __FUNCTION__, error);
		/* Tracing handled in smb_mount */
	}
	SMBNetFsUnlockSession(inConnection);
done:
	if (tmscheme) {
		free(tmscheme);
	}
	return error;
}

/*
 * SMB_GetMountInfo
 */
static netfsError SMB_GetMountInfo(CFStringRef in_Mountpath, CFDictionaryRef *out_MountInfo)
{
	return SMBNetFsGetMountInfo(in_Mountpath, out_MountInfo);
}




/* CIFS NetFS factory ID: 92D4EFEF-F5AA-11D5-A1EE-003065A0E6DE */
#define kCIFSNetFSInterfaceFactoryID (CFUUIDGetConstantUUIDWithBytes(NULL, 0x92, 0xd4, 0xef, 0xef, 0xf5, 0xaa, 0x11, 0xd5, 0xa1, 0xee, 0x00, 0x30, 0x65, 0xa0, 0xe6, 0xde))

/*
 *  NetFS Type implementation:
 */
static NetFSMountInterface_V1 gCIFSNetFSMountInterfaceFTbl = {
    NULL,				/* IUNKNOWN_C_GUTS: _reserved */
    NetFSQueryInterface,		/* IUNKNOWN_C_GUTS: QueryInterface */
    NetFSInterface_AddRef,		/* IUNKNOWN_C_GUTS: AddRef */
    NetFSInterface_Release,		/* IUNKNOWN_C_GUTS: Release */
    SMB_CreateSessionRef,		/* CreateSessionRef */
    SMB_GetServerInfo,			/* GetServerInfo */
    SMB_ParseURL,			/* ParseURL */
    SMB_CreateURL,			/* CreateURL */
    SMB_OpenSession,			/* OpenSession */
    SMB_EnumerateShares,		/* EnumerateShares */
    SMB_Mount,				/* Mount */
    SMB_Cancel,				/* Cancel */
    SMB_CloseSession,			/* CloseSession */
    SMB_GetMountInfo,			/* GetMountInfo */
};

void * CIFSNetFSInterfaceFactory(CFAllocatorRef allocator, CFUUIDRef typeID);

void *CIFSNetFSInterfaceFactory(CFAllocatorRef allocator, CFUUIDRef typeID)
{
#pragma unused(allocator)
	if (CFEqual(typeID, kNetFSTypeID)) {
		return NetFS_CreateInterface(kCIFSNetFSInterfaceFactoryID, &gCIFSNetFSMountInterfaceFTbl);
    } else {
		return NULL;
	}
}

