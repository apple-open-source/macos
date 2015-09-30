//
//  SenTestCase+GSS.m
//  GSSTestApp
//
//  Created by Love Hörnquist Åstrand on 2013-06-08.
//  Copyright (c) 2013 Apple, Inc. All rights reserved.
//

#import "XCTestCase+GSS.h"

@implementation XCTestCase (GSS)

- (void)XTCDestroyCredential:(gss_OID)mech {
    OM_uint32 junk;

    gss_iter_creds(&junk, 0, mech, ^(gss_OID mechanism, gss_cred_id_t cred) {
        OM_uint32 min_stat;
        gss_destroy_cred(&min_stat, &cred);
    });
}

- (gss_cred_id_t)XTCAcquireCredential:(NSString *)name withOptions:(NSDictionary *)options mech:(gss_OID)mech {
    CFErrorRef error = NULL;
    gss_cred_id_t cred = NULL;
    OM_uint32 maj_stat;

    gss_name_t gname = GSSCreateName((__bridge CFTypeRef)name, GSS_C_NT_USER_NAME, &error);
    if (gname == NULL) {
        [self XCTOutput:@"CreateName failed with: %@", error];
        if (error) CFRelease(error);
        return NULL;
    }
    
    maj_stat = gss_aapl_initial_cred(gname, mech, (__bridge CFDictionaryRef)options, &cred, &error);
    CFRelease(gname);
    if (maj_stat) {
        [self XCTOutput:@"gss_aapl_initial_cred failed with: %@", error];
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
    XCTAssertTrue(server_name, @"failed to import %@", serverName);

    maj_stat = gss_init_sec_context(&min_stat, cred,
                                    &ctx, server_name, GSS_KRB5_MECHANISM,
                                    GSS_C_REPLAY_FLAG|GSS_C_INTEG_FLAG, 0, GSS_C_NO_CHANNEL_BINDINGS,
                                    NULL, NULL, &buffer, NULL, NULL);
    XCTAssertTrue(maj_stat == GSS_S_COMPLETE, @"failed init_sec_context to %@: %d", serverName, (int)min_stat);
    if (maj_stat) {
        CFErrorRef error = GSSCreateError(GSS_C_NO_OID, maj_stat, min_stat);
        [self XCTOutput:@"FAIL init_sec_context maj_stat: %@", error];
        if (error)
            CFRelease(error);
		res = FALSE;
    } else {
        [self XCTOutput:@"have a buffer of length: %d, success", (int)buffer.length];
		res = TRUE;
	}
    
    gss_delete_sec_context(&min_stat, &ctx, NULL);
    gss_release_name(&min_stat, &server_name);
    gss_release_buffer(&min_stat, &buffer);
    
	return res;
}

static void
printme(const char *format, ...)
{
    va_list ap;
    va_start(ap, format);
    XFakeXCTestCallback(format, ap);
    va_end(ap);
}

- (void)XCTOutput:(NSString *)format, ...
{
    va_list va;
    va_start(va, format);
    
    NSString *string = [[NSString alloc] initWithFormat:format arguments:va];

    printme("%s\n", [string UTF8String]);
}


@end
