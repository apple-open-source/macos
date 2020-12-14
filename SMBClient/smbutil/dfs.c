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
#include <pwd.h>
#include <membership.h>

#include <smbclient/smbclient.h>
#include <smbclient/smbclient_internal.h>
#include <smbclient/ntstatus.h>

#include "common.h"
#include <netsmb/smb.h>

static uint16_t
uint16FromDictionary(CFDictionaryRef dict, CFStringRef key)
{
	CFNumberRef num = CFDictionaryGetValue( dict, key);
	uint16_t value = 0;
    
	if( num ) {
		CFNumberGetValue(num, kCFNumberSInt16Type, &value);
	}
	return value;
}

static void 
fprintfCFString(CFStringRef theString, const char *preStr, Boolean newLn)
{
	char prntstr[1024];
	
	if (theString == NULL) {
		fprintf(stdout, "%s(NULL)", preStr);
	} else {
		CFStringGetCString(theString, prntstr, 1024, kCFStringEncodingUTF8);
		fprintf(stdout, "%s%s", preStr, prntstr);
	}
	if (newLn) {
		fprintf(stdout, "\n");
	}
}

static void 
displayReferralList(CFArrayRef referralList)
{
	CFIndex ii, count = (referralList) ? CFArrayGetCount(referralList) : 0;
	
	if (!referralList) {
		return;
	}
	for (ii = 0; ii < count; ii++) {
		CFDictionaryRef dict = CFArrayGetValueAtIndex(referralList, ii);

		fprintf(stdout, "     list item %-2zu: ",  ii+1);
		fprintfCFString(CFDictionaryGetValue(dict, kDFSPath), "Path: ", TRUE);
		fprintf(stdout, "     list item %-2zu: ",  ii+1);
		fprintfCFString(CFDictionaryGetValue(dict, kNetworkAddress), 
						"Network Address: ", TRUE);
		fprintf(stdout, "     list item %-2zu: ",  ii+1);
		fprintfCFString(CFDictionaryGetValue(dict, kNewReferral), 
						"New Referral: ", TRUE);
	}
}

static void 
displayDomainReferralList(CFArrayRef referralList)
{
    CFArrayRef expandedNameArray;
    CFIndex ii, count = (referralList) ? CFArrayGetCount(referralList) : 0;
	
	if (!referralList) {
		return;
	}
	for (ii = 0; ii < count; ii++) {
		CFDictionaryRef dict = CFArrayGetValueAtIndex(referralList, ii);
        CFIndex jj, exandedNameCount;
        
        expandedNameArray = CFDictionaryGetValue(dict, kExpandedNameArray);
        exandedNameCount = (expandedNameArray) ? CFArrayGetCount(expandedNameArray) : 0;
        for (jj = 0; jj < exandedNameCount; jj++) {
            fprintf(stdout, "                  ");
            fprintfCFString(CFArrayGetValueAtIndex(expandedNameArray, jj), "ExpandedName: ", TRUE);
       }
        fprintf(stdout, "                  ");
		fprintfCFString(CFDictionaryGetValue(dict, kSpecialName), "SpecialName: ", TRUE);
		fprintf(stdout, "                  NumberOfExpandedNames: %d\n", uint16FromDictionary(dict, kNumberOfExpandedNames));
		fprintf(stdout, "                  ServerType: %d\n", uint16FromDictionary(dict, kServerType));
	}
}

int
cmd_dfs(int argc, char *argv[])
{
	CFMutableDictionaryRef dfsReferralDict;
	CFArrayRef dfsServerDictArray;
	CFArrayRef dfsReferralDictArray;
	CFIndex ad_count, count, ii;
	const char *url = NULL;
	int	opt;
	
	while ((opt = getopt(argc, argv, "h")) != EOF) {
		switch(opt){
			case 'h':
			default:
				dfs_usage();
				/*NOTREACHED*/
		}
	}
    
	if (optind >= argc) {
		dfs_usage();
    }
    
	url = argv[optind];
	argc -= optind;
	/* One more check to make sure we have the correct number of arguments */
	if (argc != 1) {
		dfs_usage();
    }
	
	dfsReferralDict = CFDictionaryCreateMutable(kCFAllocatorSystemDefault, 0,
                                        &kCFTypeDictionaryKeyCallBacks, 
                                        &kCFTypeDictionaryValueCallBacks);
	if (!dfsReferralDict) {
		errno = ENOMEM;
		err(EX_UNAVAILABLE, "internal error");
	}

	SMBGetDfsReferral(url, dfsReferralDict);

    /* Did we get a list of Domain Controllers from AD? */
    dfsServerDictArray = CFDictionaryGetValue(dfsReferralDict,
                                              kDfsADServerArray);
    
	ad_count = (dfsServerDictArray) ? CFArrayGetCount(dfsServerDictArray) : 0;
    
	if (ad_count) {
        fprintf(stdout, "\n");
        fprintf(stdout, "------------- AD Domain Entries -------------\n");

        for (ii = ad_count - 1; ii >= 0; ii--) {
            CFStringRef dc = CFArrayGetValueAtIndex(dfsServerDictArray, ii);
            
            if (dc) {
                fprintfCFString(dc, "Server Name : ", TRUE);
            }
        }
    }
    
    /* 
     * Did we get a list of Domain Controllers from the server name that really 
     * was a domain name (ie GET_DFS_REFERRAL)
     */
    dfsServerDictArray = CFDictionaryGetValue(dfsReferralDict, kDfsServerArray);
    
	count = (dfsServerDictArray) ? CFArrayGetCount(dfsServerDictArray) : 0;
	if (count) {
        for (ii = count - 1; ii >= 0; ii--) {
            CFDictionaryRef dict = CFArrayGetValueAtIndex(dfsServerDictArray, ii);
            
            fprintf(stdout, "\n");
            fprintf(stdout, "------------- Domain Entry %-2zu -------------\n", count - ii);
            if (dict) {
                fprintfCFString(CFDictionaryGetValue(dict, kRequestFileName),
                                "Domain requested : ", TRUE);
                displayDomainReferralList(CFDictionaryGetValue(dict, kReferralList));
            }
        }
    }

	if (!ad_count && !count) {
		fprintf(stdout, "\nNo server entries found\n");
	}
    
    dfsReferralDictArray = CFDictionaryGetValue(dfsReferralDict, kDfsReferralArray);
    count = (dfsReferralDictArray) ? CFArrayGetCount(dfsReferralDictArray) : 0;
	if (!count) {
		fprintf(stdout, "\nNo referral entries found\n");
		goto done;
	}
	for (ii = count-1; ii >= 0; ii--) {
		CFDictionaryRef dict = CFArrayGetValueAtIndex(dfsReferralDictArray, ii);
		
		fprintf(stdout, "\n");
		fprintf(stdout, "------------- Entry %-2zu -------------\n", count - ii);
		if (dict) {
			fprintfCFString(CFDictionaryGetValue(dict, kRequestFileName),
							"Referral requested : ", TRUE);
			displayReferralList(CFDictionaryGetValue(dict, kReferralList));
		}
	}
    
done:
	fprintf(stdout, "\n");
	if (verbose) {
		CFShow(dfsReferralDict);
	}
	CFRelease(dfsReferralDict);
	return 0;
}


void
dfs_usage(void)
{
	fprintf(stderr, "usage: smbutil dfs smb://"
			"[domain;][user[:password]@]"
			"server/dfsroot/dfslink\n");
	exit(1);
}
