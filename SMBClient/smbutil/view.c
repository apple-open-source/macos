/*
 * Copyright (c) 2000, Boris Popov
 * All rights reserved.
 *
 * Portions Copyright (C) 2001 - 2010 Apple Inc. All rights reserved.
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
#include <sys/errno.h>
#include <sys/stat.h>
#include <err.h>
#include <stdio.h>
#include <unistd.h>
#include <strings.h>
#include <stdlib.h>
#include <sysexits.h>

#include <smbclient/smbclient.h>
#include <smbclient/ntstatus.h>

#include "common.h"
#include "netshareenum.h"

/*
 * Allocate a buffer and then use CFStringGetCString to copy the c-style string 
 * into the buffer. The calling routine needs to free the buffer when done.
 */
static char *CStringCreateWithCFString(CFStringRef inStr)
{
	CFIndex maxLen;
	char *str;
	
	if (inStr == NULL) {
		return NULL;
	}
	maxLen = CFStringGetMaximumSizeForEncoding(CFStringGetLength(inStr), 
											   kCFStringEncodingUTF8) + 1;
	str = malloc(maxLen);
	if (!str) {
		return NULL;
	}
	CFStringGetCString(inStr, str, maxLen, kCFStringEncodingUTF8);
	return str;
}

/*
 * Given a share dictionary create an array that contains the share entries in
 * the dictionary.
 */
CFArrayRef createShareArrayFromShareDictionary(CFDictionaryRef shareDict)
{
	CFIndex count = 0;
	CFArrayRef keyArray = NULL;
    
    if (!shareDict)
        return NULL;
    count = CFDictionaryGetCount(shareDict);
    
    void *shareKeys = CFAllocatorAllocate(kCFAllocatorDefault, count * sizeof(CFStringRef), 0);
	if (shareKeys) {
		CFDictionaryGetKeysAndValues(shareDict, (const void **)shareKeys, NULL);
		keyArray = CFArrayCreate(kCFAllocatorDefault, (const void **)shareKeys,
                                 count, &kCFTypeArrayCallBacks);
		CFAllocatorDeallocate(kCFAllocatorDefault, shareKeys);
	}
    
	return keyArray;
}

int
cmd_view(int argc, char *argv[])
{
	const char *url = NULL;
	int			opt;
	SMBHANDLE	serverConnection = NULL;
	uint64_t	options = 0;
	NTSTATUS	status;
	int			error;
	CFDictionaryRef shareDict= NULL;
	
	while ((opt = getopt(argc, argv, "ANGgaf")) != EOF) {
		switch(opt){
			case 'A':
				options |= kSMBOptionSessionOnly;
				break;
			case 'N':
				options |= kSMBOptionNoPrompt;
				break;
			case 'G':
				options |= kSMBOptionAllowGuestAuth;
				break;
			case 'g':
				if (options & kSMBOptionOnlyAuthMask)
					view_usage();
				options |= kSMBOptionUseGuestOnlyAuth;
				options |= kSMBOptionNoPrompt;
				break;
			case 'a':
				if (options & kSMBOptionOnlyAuthMask)
					view_usage();
				options |= kSMBOptionUseAnonymousOnlyAuth;
				options |= kSMBOptionNoPrompt;
				break;
			case 'f':
				options |= kSMBOptionForceNewSession;
				break;
			default:
				view_usage();
				/*NOTREACHED*/
		}
	}
	
	if (optind >= argc)
		view_usage();
	url = argv[optind];
	argc -= optind;
	/* One more check to make sure we have the correct number of arguments */
	if (argc != 1)
		view_usage();
	
	status = SMBOpenServerEx(url, &serverConnection, options);
	/* 
	 * SMBOpenServerEx now sets errno, so err will work correctly. We change 
	 * the string based on the NTSTATUS Error.
	 */
	if (!NT_SUCCESS(status)) {
		/* This routine will exit the program */
		ntstatus_to_err(status);
	}
	if (options  & kSMBOptionSessionOnly) {
		fprintf(stdout, "Authenticate successfully with %s\n", url);
		goto done;
	}
	fprintf(stdout, "%-48s%-8s%s\n", "Share", "Type", "Comments");
	fprintf(stdout, "-------------------------------\n");

	error = smb_netshareenum(serverConnection, &shareDict, FALSE);
	if (error) {
		errno = error;
		SMBReleaseServer(serverConnection);
		err(EX_IOERR, "unable to list resources");
	} else {
		CFArrayRef shareArray = createShareArrayFromShareDictionary(shareDict);
		CFStringRef shareStr, shareTypeStr, commentStr;
		CFDictionaryRef theDict;
		CFIndex ii;
		char *share, *sharetype, *comments;
						
		for (ii=0; shareArray && (ii < CFArrayGetCount(shareArray)); ii++) {
			shareStr = CFArrayGetValueAtIndex(shareArray, ii);
			/* Should never happen, but just to be safe */
			if (shareStr == NULL) {
				continue;
			}
			theDict = CFDictionaryGetValue(shareDict, shareStr);
			/* Should never happen, but just to be safe */
			if (theDict == NULL) {
				continue;
			}
			shareTypeStr = CFDictionaryGetValue(theDict, kNetShareTypeStrKey);
			commentStr = CFDictionaryGetValue(theDict, kNetCommentStrKey);
			
			share = CStringCreateWithCFString(shareStr);
			sharetype = CStringCreateWithCFString(shareTypeStr);
			comments = CStringCreateWithCFString(commentStr);
			fprintf(stdout, "%-48s%-8s%s\n", share ? share : "",  
					sharetype ? sharetype : "", comments ? comments : "");
			free(share);
			free(sharetype);
			free(comments);
		}
		if (shareArray) {
			fprintf(stdout, "\n%ld shares listed\n", CFArrayGetCount(shareArray));
			CFRelease(shareArray);
		} else {
			fprintf(stdout, "\n0 shares listed\n");
		}
		if (shareDict) {
			CFRelease(shareDict);
		}
	}
done:
	SMBReleaseServer(serverConnection);
	return 0;
}


void
view_usage(void)
{
	fprintf(stderr, "usage: smbutil view [connection options] //"
		"[domain;][user[:password]@]"
	"server\n");
	
	fprintf(stderr, "where options are:\n"
					"    -A    authorize only\n"
					"    -N    don't prompt for a password\n"
					"    -G    allow guest access\n"
					"    -g    authorize with guest only\n"
					"    -a    authorize with anonymous only\n"
					"    -f    don't share session\n");
	exit(1);
}