//
//  jabber_od_auth_digest_md5_test.m
//  ChatServer2
//
//  Created by korver@apple.com on 2/3/09.
//  Copyright 2009 Apple, Inc.. All rights reserved.
//

static char *argv0 = 0;

#include <stdio.h>
#include <ctype.h>
#include "../apple_authenticate.c"

/* override some of the stuff from the jabber files that we pull in */
#define log_debug if(0)(void)
#define ZONE 0
#include "jabberd/pool.h"
#include "jabberd/util.h"
#include "jabberd/hex.c"
#include "jabberd/md5.c"
#include "jabberd/sha1.c"
#include "jabberd/str.c"
#include "jabberd/xhash.c"
#include "jabberd/pool.c"
#include "jabberd/mech_digest_md5.c"
#include "jabberd/scod.c"


#define kAuthMethod				"DIGEST-MD5"
#define CFSafeRelease( cfobj )                if ( cfobj != NULL )        CFRelease( cfobj );


int od_sasl_digest_auth(const char *inChallenge, const char *inResponse, char **outResponse, char *inUsername)
{
    int result = 0;
    char buf[1024];
    unsigned int len = 0;
    CFMutableArrayRef cfArrayBuf = CFArrayCreateMutable(NULL, 3, &kCFTypeArrayCallBacks);
    CFStringRef cfUsername;
    CFStringRef cfServerChal;
    CFStringRef cfClientResp;
    bool success = false;
    CFErrorRef outError = NULL;
    CFArrayRef outItems = NULL;
    int respLen = 0;
    
    cfUsername = CFStringCreateWithCString(kCFAllocatorDefault, inUsername, kCFStringEncodingUTF8);
    cfServerChal = CFStringCreateWithCString(kCFAllocatorDefault, inChallenge, kCFStringEncodingUTF8);
    cfClientResp = CFStringCreateWithCString(kCFAllocatorDefault, inResponse, kCFStringEncodingUTF8);

    // Get user record
    ODNodeRef cfNode = ODNodeCreateWithNodeType(kCFAllocatorDefault, kODSessionDefault, kODNodeTypeAuthentication, NULL);    
    CFTypeRef cfVals[] = { CFSTR(kDSAttributesStandardAll) };
    CFArrayRef cfReqAttrs = CFArrayCreate(NULL, cfVals,1, &kCFTypeArrayCallBacks);
    ODRecordRef cfUserRecord = ODNodeCopyRecord(cfNode,  CFSTR(kDSStdRecordTypeUsers), cfUsername, cfReqAttrs, &outError );

    CFSafeRelease(cfNode);
    CFSafeRelease(cfReqAttrs);

    if (outError != NULL) {
        CFStringRef cfErr = CFErrorCopyFailureReason(outError);
        CFStringGetCString(cfErr, buf, 1024, kCFStringEncodingUTF8);
        syslog(LOG_USER | LOG_ERR, "%s: Error: ODNodeCopyRecord returned: %s", __PRETTY_FUNCTION__, buf);
        CFSafeRelease(cfUsername);
        CFSafeRelease(cfServerChal);
        CFSafeRelease(cfClientResp);
        CFSafeRelease(cfArrayBuf);
        CFSafeRelease(cfUserRecord);
        return 1;
    }

    // Is the user from OD or AD?
    //NSArray *metaNodeValues = [odRecord valuesForAttribute:@kDSNAttrMetaNodeLocation error:&odError];
	//if ( [[metaNodeValues objectAtIndex:0] hasPrefix:@"/Active Directory/"] )

    CFArrayAppendValue(cfArrayBuf, cfUsername);
    CFArrayAppendValue(cfArrayBuf, cfServerChal);
    CFArrayAppendValue(cfArrayBuf, cfClientResp);
    
    CFSafeRelease(cfUsername);
    CFSafeRelease(cfServerChal);
    CFSafeRelease(cfClientResp);
    
    success = ODRecordVerifyPasswordExtended(cfUserRecord, CFSTR(kDSStdAuthDIGEST_MD5), cfArrayBuf, &outItems, NULL, &outError );
    
    CFSafeRelease(cfArrayBuf);
    CFSafeRelease(cfUserRecord);

    if ( !success ) {
        CFStringRef cfErr = CFErrorCopyFailureReason(outError);
        CFStringGetCString(cfErr, buf, 1024, kCFStringEncodingUTF8);
        syslog(LOG_USER | LOG_ERR, "%s: auth error: %s", __PRETTY_FUNCTION__, buf);
        return 1;
    }
    
    CFDataRef cfServerResp = CFArrayGetValueAtIndex(outItems, 0);
    if (cfServerResp == NULL) {
        CFSafeRelease(outItems);
        return 1;
    }

    respLen = CFDataGetLength(cfServerResp);
    if (respLen <= 0) {
        CFSafeRelease(outItems);
        CFSafeRelease(cfServerResp);
        return 1;
    }
    
    *outResponse = (char *)malloc(respLen+1);
    memset(*outResponse, 0, respLen+1);

    CFDataGetBytes(cfServerResp, CFRangeMake(0,respLen), (UInt8 *)*outResponse);

    CFSafeRelease(cfServerResp);
    CFSafeRelease(outItems);
        
    //syslog(LOG_USER | LOG_NOTICE, "%s: Server response: %s", __PRETTY_FUNCTION__, *outResponse);
    syslog(LOG_USER | LOG_NOTICE, "%s: auth success", __PRETTY_FUNCTION__);

    return 0;
}


int scod_callback(scod_t sd, int cb, void *arg, void **res, void *cbarg)
{
    assert(sd != 0);
    switch (cb) {
    case sd_cb_DIGEST_MD5_CHOOSE_REALM:
        {
            xht ht = (xht)arg;
            assert(xhash_count(ht) == 1);
            *res = (void*)ht->iter_node->key;
        }
        break;
    case sd_cb_GET_PASS:
        {
            char **pass_out = (char**)res;
            char *pass_in = (char*)cbarg;
            *pass_out = pass_in;
        }
        break;
    case sd_cb_CHECK_AUTHZID:
        {
            // pretend it's ok
        }
        break;
    default:
        assert(0 /* unknown cb */);
        break;
    }

    return 0;
}


int auth_test(char *realm, char *authnid, char *pass)
{
    int r;

    scod_ctx_t ctx = scod_ctx_new(scod_callback, pass);
    assert(ctx != 0);

    scod_t sds = scod_new(ctx, sd_type_SERVER);
    assert(sds != 0);

    char *server_challenge = 0;
    int challen = 0;
    char *ignore = 0;
    int ignorelen = 0;
    r = scod_server_start(sds, "DIGEST-MD5", realm, &ignore, &ignorelen, &server_challenge, &challen);
    assert(r == sd_CONTINUE);

    if (strcasestr(server_challenge, "digest-uri") == 0)
        asprintf(&server_challenge, "%s,digest-uri=\"ldap/%s\"", server_challenge, realm);

    scod_t sdc = scod_new(ctx, sd_type_CLIENT);
    assert(sdc != 0);

    char *resp1 = 0;
    int resplen1 = 0;
    r = scod_client_start(sdc, "DIGEST-MD5", 0, authnid, pass, &resp1, &resplen1);
    assert(r == sd_CONTINUE);
    assert(resp1 == 0);
    assert(resplen1 == 0);

    char *resp2 = 0;
    int resplen2 = 0;
    r = scod_client_step(sdc, server_challenge, strlen(server_challenge), &resp2, &resplen2);
    assert(r == sd_CONTINUE);

    char *chall2;
    int challen2;
    r = scod_server_step(sds, resp2, resplen2, &chall2, &challen2);
    assert(r == sd_CONTINUE);
    assert(chall2 != 0);
    assert(challen2 != 0);

    char *out = 0;
    r = od_sasl_digest_auth(server_challenge,resp2,&out,authnid);
    if (r)
        return r;  /* failure */

    return 0;
}


int usage()
{
    fprintf(stderr, "usage: %s iterations username realm password\n", argv0);
    fprintf(stderr, "       username, realm, and password are passed to sprintf, where %%d is the index\n");
    fprintf(stderr, "       (eg %s 1000 'testuser%%d' 'od%%d.apple.com' 'pass%%d')\n", argv0);
    exit(1);
}

int main (int argc, const char * argv[])
{
    argv0 = (char*)argv[0];
    if (strrchr(argv0, '/'))
        argv0 = strrchr(argv0, '/') + 1;

    if (argc != 5)
        usage();

    if (! isdigit(argv[1][0]))
        usage();

    NSAutoreleasePool * pool = [[NSAutoreleasePool alloc] init];

    int iterations = atoi(argv[1]);
    fprintf(stderr, "Starting %d iterations\n", iterations);

    char username[1024];
    char realm[1024];
    char password[1024];
    int i;
    int auths_succeeded = 0;
    for (i = 0; i < iterations; ++i) {
        // lots of i in case %d is used multiple times
        snprintf(username, sizeof(username), argv[2], i, i, i, i, i, i);
        snprintf(realm, sizeof(realm), argv[3], i, i, i, i, i, i);
        snprintf(password, sizeof(password), argv[4], i, i, i, i, i, i);

        if (auth_test(realm, username, password) == 0)
            ++auths_succeeded;
    }

    printf("auths_succeeded: %d (of %d)\n", auths_succeeded, iterations);

    [pool drain];

    return 0;
}

