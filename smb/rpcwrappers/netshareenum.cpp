/*
 * Copyright (c) 2010 Apple Inc. All rights reserved.
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
#include <sys/errno.h>
#include <sys/stat.h>
#include <err.h>
#include <stdio.h>
#include <unistd.h>
#include <strings.h>
#include <stdlib.h>
#include <sysexits.h>

#include <stdint.h>
#include <netsmb/smb.h>

#include <smbclient/smbclient.h>
#include <smbclient/smbclient_internal.h>
#include <smbclient/smbclient_netfs.h>
#include <smbclient/ntstatus.h>

#include "common.h"

#include "lmshare.h"
#include "netshareenum.h"
#include "rap.h"

#include <NetFS/NetFS.h>

/*
 * Get a list of all mount volumes. The calling routine will need to free the memory.
 */
static struct statfs *smb_getfsstat(int *fs_cnt)
{
	struct statfs *fs;
	int bufsize = 0;
	
	/* See what we need to allocate */
	*fs_cnt = getfsstat(NULL, bufsize, MNT_NOWAIT);
	if (*fs_cnt <=  0)
		return NULL;
	bufsize = *fs_cnt * (int)sizeof(*fs);
	fs = (struct statfs *)malloc(bufsize);
	if (fs == NULL)
		return NULL;
	
	*fs_cnt = getfsstat(fs, bufsize, MNT_NOWAIT);
	if (*fs_cnt < 0) {
		*fs_cnt = 0;
		free (fs);
		fs = NULL;
	}
	return fs;
}

/*
 * Does the same thing as strlen, except only looks up
 * to max chars inside the buffer. 
 * Taken from the darwin osfmk/device/subrs.c file. 
 * inputs:
 *      s       string whose length is to be measured
 *      max     maximum length of string to search for null
 * outputs:
 *      length of s or max; whichever is smaller
 */
static size_t smb_strnlen(const char *s, size_t max) 
{
	const char *es = s + max, *p = s;
	while(*p && p != es)
		p++;
	
	return p - s;
}

/*
 * Convert the input value into a CFString Ref. 
 */
static CFStringRef convertToStringRef(const void *inStr, size_t maxLen, int unicode)
{
	char *cStr = NULL;
	CFStringRef strRef = NULL;
	
	if (inStr == NULL) {
		return NULL;
	}
	if (unicode) {
		cStr = SMBConvertFromUTF16ToUTF8((const uint16_t *)inStr, maxLen, 0);
	} else {
		cStr = SMBConvertFromCodePageToUTF8((const char *)inStr);
	}
	if (cStr) {
		strRef = CFStringCreateWithCString(NULL, cStr, kCFStringEncodingUTF8);
		free(cStr); 
	}
	return strRef;
}

static void addShareToDictionary(SMBHANDLE inConnection, 
								 CFMutableDictionaryRef shareDict, 
								 CFStringRef shareName,  CFStringRef comments, 
								 u_int16_t shareType, struct statfs *fs, int fs_cnt)
{
	CFMutableDictionaryRef currDict = NULL;
	
	if (shareName == NULL) {
		return;
	}
	currDict = CFDictionaryCreateMutable(kCFAllocatorDefault, 0, 
										 &kCFTypeDictionaryKeyCallBacks, 
										 &kCFTypeDictionaryValueCallBacks);
	if (currDict == NULL) {
		/* Log error here, but keep going */
		SMBLogInfo("addShareToDictionary: Couldn't create the dictionary!", ASL_LEVEL_DEBUG);
		return;
	}
	
	if (CFStringHasSuffix(shareName, CFSTR("$"))) {
		CFDictionarySetValue (currDict, kNetFSIsHiddenKey, kCFBooleanTrue);
	}
		
	if (comments) {
		CFDictionarySetValue (currDict, kNetCommentStrKey, comments);
	}

	switch (shareType) {
		case SMB_ST_DISK:
			CFDictionarySetValue (currDict, kNetShareTypeStrKey, CFSTR("Disk"));
			/* Now check to see if this share is already mounted */
			if (fs) {
				/* We only care if its already mounted ignore any other errors for now */
				if (SMBCheckForAlreadyMountedShare(inConnection, shareName, currDict, fs, fs_cnt) == EEXIST) {
					CFDictionarySetValue (currDict, kNetFSAlreadyMountedKey, kCFBooleanTrue);
				} else {
					CFDictionarySetValue (currDict, kNetFSAlreadyMountedKey, kCFBooleanFalse);
				}

			}
			break;
		case SMB_ST_PRINTER:
			CFDictionarySetValue (currDict, kNetShareTypeStrKey, CFSTR("Printer"));
			CFDictionarySetValue (currDict, kNetFSPrinterShareKey, kCFBooleanTrue);				
			CFDictionarySetValue (currDict, kNetFSAlreadyMountedKey, kCFBooleanFalse);
			break;
		case SMB_ST_PIPE:
			CFDictionarySetValue (currDict, kNetShareTypeStrKey, CFSTR("Pipe"));
			CFDictionarySetValue (currDict, kNetFSAlreadyMountedKey, kCFBooleanFalse);
			break;
		case SMB_ST_COMM:
			CFDictionarySetValue (currDict, kNetShareTypeStrKey, CFSTR("Comm"));
			CFDictionarySetValue (currDict, kNetFSAlreadyMountedKey, kCFBooleanFalse);
			break;
		default:
			CFDictionarySetValue (currDict, kNetShareTypeStrKey, CFSTR("Unknown"));
			CFDictionarySetValue (currDict, kNetFSAlreadyMountedKey, kCFBooleanFalse);
			break;
	}
	CFDictionarySetValue (currDict, kNetFSHasPasswordKey, kCFBooleanFalse);
	CFDictionarySetValue (shareDict, shareName, currDict);
	CFRelease (currDict);
}

int smb_netshareenum(SMBHANDLE inConnection, CFDictionaryRef *outDict, 
					 int DiskAndPrintSharesOnly)
{
	int error = 0;
	NTSTATUS status;
	SMBServerPropertiesV1 properties;
	CFMutableDictionaryRef shareDict = NULL;
	uint32_t ii;
	CFStringRef shareName, comments;
	u_int16_t shareType;
	struct statfs *fs = NULL;
	int fs_cnt = 0;
	
	fs = smb_getfsstat(&fs_cnt);

	shareDict = CFDictionaryCreateMutable(kCFAllocatorDefault, 0, 
										 &kCFTypeDictionaryKeyCallBacks, 
										 &kCFTypeDictionaryValueCallBacks);
	if (shareDict == NULL) {
		error = ENOMEM;
		goto done;
	}
	
	status = SMBGetServerProperties(inConnection, &properties, kPropertiesVersion, sizeof(properties));
	if (!NT_SUCCESS(status)) {
		/* Should never happen */
		error = errno;
		goto done;
	}
	/* Only use RPC if the server supports DCE/RPC and UNICODE */
	if ((properties.capabilities & SMB_CAP_RPC_REMOTE_APIS) && 
		(properties.capabilities & SMB_CAP_UNICODE)) {
		PSHARE_ENUM_STRUCT InfoStruct = NULL;
		NET_API_STATUS api_status;
		
		/* Try getting a list of shares with the SRVSVC RPC service. */
		api_status = NetShareEnum(properties.serverName, 1, &InfoStruct);
		if (api_status == 0) {
			for (ii = 0; ii < InfoStruct->ShareInfo.Level1->EntriesRead; ii++) {
				shareType = OSSwapLittleToHostInt16(InfoStruct->ShareInfo.Level1->Buffer[ii].shi1_type);
				/* They only want the disk and printer shares */
				if (DiskAndPrintSharesOnly && (shareType != SMB_ST_DISK) && (shareType != SMB_ST_PRINTER))
					continue;
				shareName = convertToStringRef(InfoStruct->ShareInfo.Level1->Buffer[ii].shi1_netname, 1024, TRUE);
				if (shareName == NULL) {
					continue;
				}
				if (InfoStruct->ShareInfo.Level1->Buffer[ii].shi1_remark) {
					comments = convertToStringRef(InfoStruct->ShareInfo.Level1->Buffer[ii].shi1_remark, 1024, TRUE);
				} else {
					comments = NULL;
				}
				addShareToDictionary(inConnection, shareDict, shareName, comments, shareType, fs, fs_cnt);
				CFRelease(shareName);
				if (comments) {
					CFRelease(comments);
				}
			}
			NetApiBufferFree(InfoStruct);
			goto done;
		} 
		SMBLogInfo("Looking up shares with RPC failed api_status = %d", ASL_LEVEL_DEBUG, api_status);
	}
	/*
	 * OK, that didn't work - either they don't support RPC or we
	 * got an error in either case try RAP.
	 */
	{
		void *rBuffer = NULL;
		unsigned char *endBuffer;
		uint32_t rBufferSize = 0;
		struct smb_share_info_1 *shareInfo1;
		uint32_t entriesRead = 0;

		/* Try getting a list of shares with the RAP protocol. */
		error = RapNetShareEnum(inConnection, 1, &rBuffer, &rBufferSize, &entriesRead, NULL);
		if (error) {
			goto done;		
		}
		endBuffer = (unsigned char *)rBuffer + rBufferSize;

		for (shareInfo1 = (struct smb_share_info_1 *)rBuffer, ii = 0;
			 (ii < entriesRead) && (((unsigned  char *)shareInfo1 + sizeof(smb_share_info_1)) <= endBuffer); 
			 ii++, shareInfo1++) {
			
			shareInfo1->shi1_pad = 0; /* Just to be safe */
			/* Note we need to swap this item */
			shareType = OSSwapLittleToHostInt16(shareInfo1->shi1_type);
			shareName = convertToStringRef(shareInfo1->shi1_netname,  sizeof(shareInfo1->shi1_netname), FALSE);
			if (shareName == NULL) {
				continue;
			}
			/* Assume we have no comments for this entry */ 
			comments = NULL;
			/* 
			 * The shi1_remark gets swapped in the rap processing, someday we just
			 * take another look at this an make it work the same for all values.
			 */
			if ((shareInfo1->shi1_remark != 0) && (shareInfo1->shi1_remark < rBufferSize)) {
				unsigned char *remarks = (unsigned char *)rBuffer + shareInfo1->shi1_remark;
				
				/*
				 * Make sure the comments don't start pass the end of the buffer
				 * and we have a comment. 
				 */
				if ((remarks < endBuffer) && *remarks) {
					size_t maxlen = endBuffer - remarks;
					/* Now make sure the comment is a null terminate string */
					maxlen = smb_strnlen((const char *)remarks, maxlen);
					remarks[maxlen] = 0;
					comments = convertToStringRef(remarks, maxlen, FALSE);
				}

			}
			addShareToDictionary(inConnection, shareDict, shareName, comments, shareType, fs, fs_cnt);
			CFRelease(shareName);
			if (comments) {
				CFRelease(comments);
			}
		}
		RapNetApiBufferFree(rBuffer);
	}
done:
	if (fs) {
		free(fs);
	}
	if (error) {
		*outDict = NULL;
		if (shareDict) {
			CFRelease(shareDict);
		}
	} else {
		*outDict = shareDict;
	}
	return error;
}

