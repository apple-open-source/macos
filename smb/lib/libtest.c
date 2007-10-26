/*
 * Copyright (c) 2007 Apple Inc. All rights reserved.
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

#include "libtest.h"

#include <URLMount/URLMount.h>
#include <sys/stat.h>
#include <asl.h>
#include "smb_netfs.h"
#include <netsmb/smb_lib.h>
#include <netsmb/smb_lib.h>
#include <parse_url.h>
#include <netsmb/smb_conn.h>

#include <unistd.h>
#include <sys/stat.h>

#include <CoreFoundation/CoreFoundation.h>
#include <URLMount/URLMount.h>

#include <asl.h>
#include "smb_netfs.h"
#include <netsmb/smb_lib.h>
#include <netsmb/smb_lib.h>
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
	return 0;
}

/*
 * SMB_CloseSession
 */
netfsError SMB_CloseSession(void *sessionRef) 
{
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
	if ((sessionRef == NULL) || (url == NULL)) {
		smb_log_info("%s: - in parameters are NULL!", EINVAL, ASL_LEVEL_ERR, __FUNCTION__);
		return EINVAL;
	}
	return smb_open_session(sessionRef, url, openOptions, sessionInfo);
}

/*
 * SMB_GetServerInfo
 */
netfsError SMB_GetServerInfo(CFURLRef url, void *sessionRef, CFDictionaryRef openOptions, CFDictionaryRef *serverParms)
{
	if ((sessionRef == NULL) || (url == NULL)) {
		smb_log_info("%s: - in parameters are NULL!", EINVAL, ASL_LEVEL_ERR, __FUNCTION__);
		return EINVAL;
	}
	return smb_get_server_info(sessionRef, url, openOptions, serverParms);	
}

/*
 * SMB_EnumerateShares
 */
netfsError SMB_EnumerateShares(void *sessionRef, CFDictionaryRef in_EnumerateOptions, CFDictionaryRef *sharePoints ) 
{
	if (sessionRef == NULL) {
		smb_log_info("%s: failed session reference is NULL!", EINVAL, ASL_LEVEL_ERR, __FUNCTION__);
		return EINVAL;
	}
	return smb_enumerate_shares(sessionRef, sharePoints);
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
	
	/* Now deal with the URL */
	if (ctx->ct_url)
		CFRelease(ctx->ct_url);
	ctx->ct_url =  CFURLCopyAbsoluteURL(url);
	if (ctx->ct_url)
		error = ParseSMBURL(ctx, SMB_ST_DISK);
	else
		error = ENOMEM;
	
	if (error) {
		smb_log_info("%s: Parsing URL failed!", error, ASL_LEVEL_ERR, __FUNCTION__);
		return error;
	}
	
	return smb_mount(sessionRef, mPoint, mOptions, mInfo);
}

static CFURLRef create_url_with_share(CFURLRef url, CFStringRef sharePoint)
{
	CFMutableDictionaryRef urlParms = NULL;
	
	errno = SMB_ParseURL(url, (CFDictionaryRef *)&urlParms);
	if (errno)
		return NULL;
	
	CFDictionarySetValue (urlParms, kPathKey, sharePoint);
	url = NULL;
	errno = SMB_CreateURL(urlParms, &url);
	if (errno)
		return NULL;
	return url;

}
#ifdef TEST_ONE
static int netfs_test_mounts(void *sessionRef, CFURLRef url)
{
	CFURLRef mount_url = NULL;
	CFMutableDictionaryRef mountOptions = NULL;
	CFDictionaryRef mountInfo = NULL;
	int error;
	
	if ((mkdir("/Volumes/george/TestMountPts/mp", S_IRWXU | S_IRWXG) == -1) && (errno != EEXIST))
		return errno;
	
	mountOptions = CFDictionaryCreateMutable(kCFAllocatorDefault, 0, &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks);
	if (!mountOptions)
		return errno;
	CFDictionarySetValue (mountOptions, kForceNewSessionKey, kCFBooleanFalse);
		
	mount_url = create_url_with_share(url, CFSTR("TestShare"));
	if (mount_url == NULL) {
		error = errno;
		goto WeAreDone;
	}
	
	error = SMB_Mount(sessionRef, mount_url, CFSTR("/Volumes/george/TestMountPts/mp"), mountOptions, &mountInfo);
	if (error)
		goto WeAreDone;
	if (mountInfo) {
		CFShow(mountInfo);
		CFRelease(mountInfo);
	}
WeAreDone:
	if (mountOptions)
		CFRelease(mountOptions);
	if (mount_url)
		CFRelease(mount_url);
	return error;
}
#else // TEST_ONE

static int netfs_test_mounts(void *sessionRef, CFURLRef url)
{
	CFURLRef mount_url = NULL;
	CFMutableDictionaryRef mountOptions = NULL;
	CFDictionaryRef mountInfo = NULL;
	int error;
	
	if ((mkdir("/Volumes/george/TestMountPts/mp", S_IRWXU | S_IRWXG) == -1) && (errno != EEXIST))
		return errno;
	if ((mkdir("/Volumes/george/TestMountPts/mp1", S_IRWXU | S_IRWXG) == -1) && (errno != EEXIST))
		return errno;
	if ((mkdir("/Volumes/george/TestMountPts/mp2", S_IRWXU | S_IRWXG) == -1) && (errno != EEXIST))
		return errno;
	
	mountOptions = CFDictionaryCreateMutable(kCFAllocatorDefault, 0, &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks);
	if (!mountOptions)
		return errno;
	CFDictionarySetValue (mountOptions, kForceNewSessionKey, kCFBooleanFalse);
	
	mount_url = create_url_with_share(url, CFSTR("TestShare"));
	if (mount_url == NULL) {
		error = errno;
		goto WeAreDone;
	}
	
	error = SMB_Mount(sessionRef, mount_url, CFSTR("/Volumes/george/TestMountPts/mp"), mountOptions, &mountInfo);
	if (error)
		goto WeAreDone;
	if (mountInfo) {
		CFShow(mountInfo);
		CFRelease(mountInfo);
	}
	mountInfo = NULL;
	CFRelease(mount_url);
	mount_url = NULL;
	mount_url = create_url_with_share(url, CFSTR("ntfsshare"));
	if (mount_url == NULL) {
		error = errno;
		goto WeAreDone;
	}
	
	error = SMB_Mount(sessionRef,mount_url, CFSTR("/Volumes/george/TestMountPts/mp1"), mountOptions, &mountInfo);
	if (error)
		goto WeAreDone;
	if (mountInfo) {
		CFShow(mountInfo);
		CFRelease(mountInfo);
	}
	mountInfo = NULL;
	CFRelease(mount_url);
	mount_url = NULL;
	mount_url = create_url_with_share(url, CFSTR("fat32share"));
	if (mount_url == NULL) {
		error = errno;
		goto WeAreDone;
	}
	error = SMB_Mount(sessionRef, mount_url, CFSTR("/Volumes/george/TestMountPts/mp2"), mountOptions, &mountInfo);
	if (error)
		goto WeAreDone;
	if (mountInfo) {
		CFShow(mountInfo);
		CFRelease(mountInfo);
	}
WeAreDone:
	if (mountOptions)
		CFRelease(mountOptions);
	if (mount_url)
		CFRelease(mount_url);
	return error;
}
#endif // TEST_ONE

static int do_user_test(CFStringRef urlString)
{
	CFURLRef	url;
	void		*ref;
	int error;
	CFMutableDictionaryRef OpenOptions = NULL;
	CFDictionaryRef shares = NULL;
	CFDictionaryRef ServerParams = NULL;
	
	error = smb_load_library(NULL);
	if (error)
		return error;
	url = CFURLCreateWithString (NULL, urlString, NULL);
	if (error)
		return errno;
	OpenOptions = CFDictionaryCreateMutable(kCFAllocatorDefault, 0, &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks);
	if (OpenOptions == NULL)
		return errno;
	CFDictionarySetValue (OpenOptions, kUseKerberosKey, kCFBooleanTrue);
	
	ref = smb_create_ctx();
	if (ref == NULL)
		exit(-1);
	error = smb_get_server_info(ref, url, OpenOptions, &ServerParams);
	if ((error == 0) && ServerParams)
		CFShow(ServerParams);
	
	CFDictionarySetValue (OpenOptions, kUseKerberosKey, kCFBooleanFalse);
	if (error == 0)
		error = smb_open_session(ref, url, OpenOptions, NULL);
	if (error == 0)
		error = smb_enumerate_shares(ref, &shares);
	if (error == 0)
		CFShow(shares);
	if (error == 0)
		error = mkdir("/Volumes/george/TestShare", S_IRWXU | S_IRWXG);
	if (error == 0)
		error = smb_mount(ref, CFSTR("/Volumes/george/TestShare"), NULL, NULL);
	
	if (ServerParams)
		CFRelease(ServerParams);
	if (url)
		CFRelease(url);
	if (OpenOptions)
		CFRelease(OpenOptions);
	if (shares)
		CFRelease(shares);
	smb_ctx_done(ref);
	return 0;
}

static int mount_one_volume(CFStringRef urlString)
{
	CFURLRef	url;
	void		*ref;
	int error;
	CFMutableDictionaryRef OpenOptions = NULL;
	
	
	if ((mkdir("/Volumes/george/TestMountPts/mp", S_IRWXU | S_IRWXG) == -1) && (errno != EEXIST))
		return errno;
	error = smb_load_library(NULL);
	if (error)
		return error;
	url = CFURLCreateWithString (NULL, urlString, NULL);
	if (url == NULL)
		return errno;
	OpenOptions = CFDictionaryCreateMutable(kCFAllocatorDefault, 0, &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks);
	if (OpenOptions == NULL)
		return errno;
	CFDictionarySetValue (OpenOptions, kUseKerberosKey, kCFBooleanFalse);
	
	ref = smb_create_ctx();
	if (ref == NULL)
		return -1;
	error = smb_open_session(ref, url, OpenOptions, NULL);
	if(error == 0)
		error = smb_mount(ref, CFSTR("/Volumes/george/TestMountPts/mp"), NULL, NULL);
	
	if (url)
		CFRelease(url);
	if (OpenOptions)
		CFRelease(OpenOptions);
	smb_ctx_done(ref);
	return error;
	
}

/* 
 * This routine will percent escape out the input string by making a copy. It will release the input string
 * if the copy succeeds otherwise it will just return the input string.
 *
 * NOTE:  We always use kCFStringEncodingUTF8 and kCFAllocatorDefault.
 */
static CFStringRef TestCreateStringByReplacingPercentEscapesUTF8(CFStringRef theStr, CFStringRef LeaveEscaped)
{
	CFStringRef outStr;
	
	if (!theStr)	/* Just a safety check */
		return NULL;
	
	outStr = CFURLCreateStringByReplacingPercentEscapesUsingEncoding(kCFAllocatorDefault, theStr, 
																	 LeaveEscaped, kCFStringEncodingUTF8);
	if (outStr)
		CFRelease(theStr);
	else 
		outStr = theStr;
	return outStr;
}

/* 
 * This routine will  percent escape the input string by making a copy. It will release the input string
 * if the copy succeeds otherwise it will just return the input string.
 *
 * NOTE:  We always use kCFStringEncodingUTF8 and kCFAllocatorDefault.
 */
static CFStringRef TestCreateStringByAddingPercentEscapesUTF8(CFStringRef theStr)
{
	CFStringRef outStr;
	
	if (!theStr)	/* Just a safety check */
		return NULL;
	
	outStr = CFURLCreateStringByAddingPercentEscapes(NULL, theStr, NULL, NULL, kCFStringEncodingUTF8);
	if (outStr)
		CFRelease(theStr);
	else 
		outStr = theStr;
	return outStr;
}

static int list_shares_once(CFStringRef urlString)
{
	CFURLRef	url;
	void		*ref;
	int error;
	CFMutableDictionaryRef OpenOptions = NULL;
	CFDictionaryRef shares = NULL;
	
	error = smb_load_library(NULL);
	if (error)
		return error;
	urlString = TestCreateStringByReplacingPercentEscapesUTF8(urlString, CFSTR(""));
	urlString = TestCreateStringByAddingPercentEscapesUTF8(urlString);
	url = CFURLCreateWithString (NULL, urlString, NULL);
	if (error)
		return errno;
	OpenOptions = CFDictionaryCreateMutable(kCFAllocatorDefault, 0, &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks);
	if (OpenOptions == NULL)
		return errno;
	CFDictionarySetValue (OpenOptions, kUseKerberosKey, kCFBooleanFalse);
	
	ref = smb_create_ctx();
	if (ref == NULL)
		return errno;
	error = smb_open_session(ref, url, OpenOptions, NULL);
	if (error == 0)
		error = smb_enumerate_shares(ref, &shares);
	if (error == 0)
		CFShow(shares);
	else
		CFShow(CFSTR("smb_enumerate_shares returned and error"));
	if (url)
		CFRelease(url);
	if (OpenOptions)
		CFRelease(OpenOptions);
	if (shares)
		CFRelease(shares);
	smb_ctx_done(ref);
	return 0;
}

/* 
* Test based on the netfs routines 
 */
static void do_netfs_test(CFStringRef urlString, int doGuest)
{
	CFURLRef url = NULL;
	CFMutableDictionaryRef openOptions = NULL;
	CFDictionaryRef serverParms = NULL;
	void *sessionRef = NULL;
	CFDictionaryRef sharePoints = NULL;
	int error;

	url = CFURLCreateWithString (NULL, urlString, NULL);
	if (!url)
		goto WeAreDone;
	openOptions = CFDictionaryCreateMutable(kCFAllocatorDefault, 0, &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks);
	if (!openOptions)
		goto WeAreDone;
	CFDictionarySetValue (openOptions, kUseKerberosKey, kCFBooleanTrue);
	
	error = SMB_CreateSessionRef(&sessionRef);
	if (error)
		goto WeAreDone;
	error = SMB_GetServerInfo(url, sessionRef, openOptions, &serverParms);
	if (error)
		goto WeAreDone;
	CFShow(serverParms);
	if (doGuest)
		CFDictionarySetValue (openOptions, kUseGuestKey, kCFBooleanTrue);
	else {
		CFDictionarySetValue (openOptions, kUseGuestKey, kCFBooleanFalse);
		CFDictionarySetValue (openOptions, kUseKerberosKey, kCFBooleanFalse);
	}
	error = SMB_OpenSession(url, sessionRef, openOptions, NULL);
	if (error)
		goto WeAreDone;
	error = SMB_EnumerateShares(sessionRef, NULL, &sharePoints);
	if (error)
		goto WeAreDone;
	CFShow(sharePoints);
	CFRelease(sharePoints);
	sharePoints = NULL;
	error = SMB_EnumerateShares(sessionRef, NULL, &sharePoints);
	CFShow(sharePoints);
	if (error == 0)
		error = netfs_test_mounts(sessionRef, url);
	
WeAreDone:
	if (url)
		CFRelease(url);
	if (openOptions)
		CFRelease(openOptions);
	if (serverParms)
		CFRelease(serverParms);
	if (sharePoints)
		CFRelease(sharePoints);
	if (sessionRef)
		error = SMB_CloseSession(sessionRef);
}

/* 
 * Test low level smb library routines. This routine
 * will change depending on why routine is being tested.
 */
static void do_ctx_test(int type_of_test)
{
	int error = 0;
	
	switch (type_of_test) {
		case 0:
			//error = list_shares_once(CFSTR("smb://local1:local@smb-win2003.apple.com"));
			//error = list_shares_once(CFSTR("smb://gcolley:b3mGk#a!@bigpapa.apple.com"));
			error = list_shares_once(CFSTR("smb://local1:local@msfilsys.apple.com"));
			if (error)
				fprintf(stderr, " list_shares_once returned %d\n", error);
			break;
		case 1:
			error = mount_one_volume(CFSTR("smb://local1:local@smb-win2003.apple.com/TestShare"));
			if (error)
				fprintf(stderr, " mount_one_volume returned %d\n", error);
			break;
		case 2:
			error = do_user_test(CFSTR("smb://local1:local@smb-win2003.apple.com/TestShare"));
			if (error)
				fprintf(stderr, " do_user_test returned %d\n", error);
			break;
		case 3:
		{
			CFDictionaryRef dict;
			CFURLRef url = CFURLCreateWithString (NULL, CFSTR("smb://BAD%3A%3B%2FBAD@colley2/badbad"), NULL);

			error = smb_url_to_dictionary(url, &dict);
			if (error)
				fprintf(stderr, " smb_url_to_dictionary returned %d\n", error);
			else if (dict) {
				CFShow(dict);
				CFRelease(dict);
			} 
			CFRelease(url);
		}
		break;
		default:
			fprintf(stderr, " Unknown command %d\n", type_of_test);
			break;
			
	};
}
int main(int argc, char **argv)
{
	int type_of_test = 3;
	int doGuest = FALSE;


	if (type_of_test < 4)
		do_ctx_test(type_of_test);
	else if (!doGuest)
		do_netfs_test(CFSTR("smb://local1:local@smb-win2003.apple.com"), doGuest);
	else
		do_netfs_test(CFSTR("smb://smb-win2003.apple.com"), doGuest);
	return 0;
}
