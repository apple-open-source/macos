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

#import <KerberosHelper/NetworkAuthenticationHelper.h>
#import <KerberosHelper/NetworkAuthenticationHelperGSS.h>
#import <KerberosHelper/KerberosHelper.h>
#import <Foundation/Foundation.h>
#import <GSS/gssapi.h>

#include <unistd.h>
#include <err.h>


/* XXX application protocol code here
 sample code only using GSS-API directly
 */

static void
tryGSSAPI(gss_OID mech, gss_cred_id_t cred, gss_name_t servername, CFTypeRef name)
{
    OM_uint32 maj, junk;
    
    gss_buffer_desc buffer;
    gss_ctx_id_t ctx = NULL;
    
    maj = gss_init_sec_context(&junk, cred, &ctx, servername, mech, 0, GSS_C_INDEFINITE, NULL, NULL,
			       NULL, &buffer, NULL, NULL);
    if (maj)
	NSLog(@"ISC failed with %d/%d for %@", maj, junk, name);
    else {
	printf("ISC success, got first token\n");
	gss_delete_sec_context(&junk, &ctx, NULL);
	gss_release_buffer(&junk, &buffer);
    }
    gss_release_cred(&junk, &cred);
    gss_release_name(&junk, &servername);
}


static int
doit(CFStringRef username, CFStringRef password,
     CFStringRef hostname, CFStringRef protocol)
{
    CFMutableDictionaryRef info = NULL;
    CFDictionaryRef krbtoken = NULL;

    if (protocol == NULL)
	protocol = CFSTR("host");

    info = CFDictionaryCreateMutable(NULL, 0,
				     &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks);


    krbtoken = KRBCreateNegTokenLegacyKerberos(NULL);
    CFDictionaryAddValue(info, kNAHNegTokenInit, krbtoken);
    CFRelease(krbtoken);

    if (username)
	CFDictionaryAddValue(info, kNAHUserName, username);

    if (password)
	CFDictionaryAddValue(info, kNAHPassword, password);


    NAHRef nah = NAHCreate(NULL, hostname, protocol, info);
    if (nah == NULL)
	errx(1, "NAHCreate");

    CFArrayRef selections = NAHGetSelections(nah);
    if (selections == NULL)
	errx(1, "NAHGetSelections");
    
    CFIndex idx, count = CFArrayGetCount(selections);

    printf("num selection: %ld\n", count);

    for (idx = 0; idx < count; idx++) {
	NAHSelectionRef sel = 
	    (NAHSelectionRef)CFArrayGetValueAtIndex(selections, idx);
	CFErrorRef error;

	CFStringRef clientPrincipal = NAHSelectionGetInfoForKey(sel, kNAHClientPrincipal);
	CFStringRef serverPrincipal = NAHSelectionGetInfoForKey(sel, kNAHServerPrincipal);
	
	NSLog(@"trying to get credential: client: %@ server: %@",
	      (NSString *)clientPrincipal, (NSString *)serverPrincipal);
	
	if (!NAHSelectionAcquireCredential(sel, NULL, &error)) {
	    NSLog(@"failed to acquire cred: %@", error);
	    if (error)
		CFRelease(error);
	    continue;
	}

	{
	    gss_name_t servername = NAHSelectionGetGSSAcceptorName(sel, NULL);
	    if (servername == NULL)
		errx(1, "failed to get server name");
	    
	    gss_cred_id_t cred = NAHSelectionGetGSSCredential(sel, NULL);
	    if (cred == NULL)
		errx(1, "failed to get client credential");
	    
	    gss_OID mech = NAHSelectionGetGSSMech(sel);
	    if (mech == NULL)
		errx(1, "failed to get mech oid");
	    
	    
	    tryGSSAPI(mech, cred, servername, sel);
	}

	{
	    CFDictionaryRef authInfo = NAHSelectionCopyAuthInfo(sel);
	    
	    gss_cred_id_t cred = NAHAuthenticationInfoCopyClientCredential(authInfo, NULL);
	    gss_name_t servername = NAHAuthenticationInfoCopyServerName(authInfo, NULL);
	    gss_OID mech = NAHAuthenticationInfoGetGSSMechanism(authInfo, NULL);
	    
	    tryGSSAPI(mech, cred, servername, @"mech");
	}
    
    }


    if (selections)
	CFRelease(selections);
    if (nah)
	CFRelease(nah);

    return 0;
}

static void
usage(int exit_code)
{
    printf("%s [-u user] [-p password] [-s service] hostname1...\n", getprogname());
    exit(exit_code);
}

int
main(int argc, char **argv)
{
    CFStringRef username = NULL, password = NULL;
    CFStringRef hostname = NULL, service = NULL;
    int ch, i;

    while ((ch = getopt(argc, argv, "u:p:s:")) != -1) {
	switch (ch) {
	case 'u':
	    username = CFStringCreateWithCString(NULL, optarg, kCFStringEncodingUTF8);
	    break;
	case 'p':
	    password = CFStringCreateWithCString(NULL, optarg, kCFStringEncodingUTF8);
	    break;
	case 's':
	    service = CFStringCreateWithCString(NULL, optarg, kCFStringEncodingUTF8);
	    break;
	case '?':
	    usage(0);
	    break;
	default:
	    usage(1);
	    break;
	}
    }
    argc -= optind;
    argv += optind;
    
    if (argc < 0)
	usage(1);
    
    for (i = 0; i < argc; i++) {
	hostname = CFStringCreateWithCString(NULL, argv[i], kCFStringEncodingUTF8);
	
	doit(username, password, hostname, service);
	
	CFRelease(hostname);
    }

    return 0;
}
