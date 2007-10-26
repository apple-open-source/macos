/*
 * Copyright (c) 2006 - 2007 Apple Inc. All rights reserved.
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

#include <Carbon/Carbon.h>
#include <URLMount/URLMount.h>

#include <asl.h>
#include "smb_netfs.h"
#include <netsmb/smb_lib.h>
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
	error = smb_load_library(NULL);
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
#ifdef DEBUG
	smb_log_info("%s: refernce %p\n", 0, ASL_LEVEL_DEBUG, __FUNCTION__, *sessionRef);
#endif // DEBUG
	return 0;
}

/*
 * SMB_CancelSession
 */
netfsError SMB_Cancel(void *sessionRef) 
{
#ifdef DEBUG
	smb_log_info("%s: refernce %p\n", 0, ASL_LEVEL_DEBUG, __FUNCTION__, sessionRef);
#endif // DEBUG
	smb_ctx_cancel_connection((struct smb_ctx *)sessionRef);
	return 0;
}

/*
 * SMB_CloseSession
 */
netfsError SMB_CloseSession(void *sessionRef) 
{
#ifdef DEBUG
	smb_log_info("%s: refernce %p\n", 0, ASL_LEVEL_DEBUG, __FUNCTION__, sessionRef);
#endif // DEBUG
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
	error = smb_open_session(sessionRef, url, openOptions, sessionInfo);
	pthread_mutex_unlock(&ctx->ctx_mutex);
	if (error)
		smb_log_info("%s: - error = %d!", 0, ASL_LEVEL_DEBUG, __FUNCTION__, error);		
#ifdef DEBUG
	else 
		smb_log_info("%s: refernce %p\n", 0, ASL_LEVEL_DEBUG, __FUNCTION__, sessionRef);
#endif // DEBUG
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
#ifdef DEBUG
	else 
		smb_log_info("%s: refernce %p\n", 0, ASL_LEVEL_DEBUG, __FUNCTION__, sessionRef);
#endif // DEBUG
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
#ifdef DEBUG
	else 
		smb_log_info("%s: refernce %p\n", 0, ASL_LEVEL_DEBUG, __FUNCTION__, sessionRef);
#endif // DEBUG
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
#ifdef DEBUG
	else 
		smb_log_info("%s: refernce %p\n", 0, ASL_LEVEL_DEBUG, __FUNCTION__, sessionRef);
#endif // DEBUG
	return error;
}
