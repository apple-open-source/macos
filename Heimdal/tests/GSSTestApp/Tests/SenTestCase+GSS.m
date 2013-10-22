//
//  SenTestCase+GSS.m
//  GSSTestApp
//
//  Created by Love Hörnquist Åstrand on 2013-06-08.
//  Copyright (c) 2013 Apple, Inc. All rights reserved.
//

#import "SenTestCase+GSS.h"
#import "TestHarness.h"

@implementation SenTestCase (GSS)

- (void)STCDestroyCredential:(gss_OID)mech {
    
    gss_iter_creds(NULL, 0, mech, ^(gss_OID mechanism, gss_cred_id_t cred) {
        OM_uint32 min_stat;
        gss_destroy_cred(&min_stat, &cred);
    });
}

- (gss_cred_id_t)STCAcquireCredential:(NSString *)name withPassword:(NSString *)password mech:(gss_OID)mech {
    CFErrorRef error = NULL;
    gss_cred_id_t cred = NULL;
    OM_uint32 maj_stat;

    gss_name_t gname = GSSCreateName((__bridge CFTypeRef)name, GSS_C_NT_USER_NAME, &error);
    if (gname == NULL) {
        NSLog(@"CreateName failed with: %@", error);
        if (error) CFRelease(error);
        return NULL;
    }
    
    NSDictionary *options = @{ (id)kGSSICPassword : password } ;
    
    maj_stat = gss_aapl_initial_cred(gname, mech, (__bridge CFDictionaryRef)options, &cred, &error);
    CFRelease(gname);
    if (maj_stat) {
        NSLog(@"gss_aapl_initial_cred failed with: %@", error);
        if (error) CFRelease(error);
        return NULL;
    }
    
    
    return cred;
}


- (BOOL)STCAuthenticate:(gss_cred_id_t)cred nameType:(gss_OID)nameType toServer:(NSString *)serverName
{
    gss_ctx_id_t ctx = GSS_C_NO_CONTEXT;
    gss_name_t server_name = GSS_C_NO_NAME;
    gss_buffer_desc buffer;
    OM_uint32 maj_stat, min_stat;
    CFErrorRef error = NULL;
	BOOL res;
    
    server_name = GSSCreateName((__bridge CFStringRef)serverName, nameType, &error);
    STAssertTrue(server_name, @"failed to import %@", serverName);

    maj_stat = gss_init_sec_context(&min_stat, cred,
                                    &ctx, server_name, GSS_KRB5_MECHANISM,
                                    GSS_C_REPLAY_FLAG|GSS_C_INTEG_FLAG, 0, GSS_C_NO_CHANNEL_BINDINGS,
                                    NULL, NULL, &buffer, NULL, NULL);
    STAssertTrue(maj_stat == GSS_S_COMPLETE, @"failed init_sec_context to %@: %d", serverName, (int)min_stat);
    if (maj_stat) {
        NSLog(@"FAIL init_sec_context maj_stat: %d", (int)maj_stat);
		res = FALSE;
    } else {
        NSLog(@"have a buffer of length: %d, success", (int)buffer.length);
		res = TRUE;
	}
    
    gss_delete_sec_context(&min_stat, &ctx, NULL);
    gss_release_name(&min_stat, &server_name);
    gss_release_buffer(&min_stat, &buffer);
    
	return res;
}

- (void)STCOutput:(NSString *)output
{
    [TestHarness TestHarnessOutput:output];
}


@end
