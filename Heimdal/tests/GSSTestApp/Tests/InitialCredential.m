//
//  InitialCredential.m
//  GSSTestApp
//
//  Created by Love Hörnquist Åstrand on 2013-06-07.
//  Copyright (c) 2013 Apple, Inc. All rights reserved.
//

#import "InitialCredential.h"
#import "TestUsers.h"

@implementation InitialCredential

- (void)testAcquireQAD {
    gss_cred_id_t cred = NULL;
    
    [self STCDestroyCredential:GSS_C_NO_OID];
    
    cred = [self STCAcquireCredential:@"ktestuser@QAD.APPLE.COM" withPassword:passwordKtestuserQAD mech:GSS_KRB5_MECHANISM];
    STAssertTrue(cred, @"Failed to acquire credential from QAD");
    if (cred == NULL)
        return;

    CFRelease(cred);
}


#if TARGET_OS_IPHONE
- (void)testAcquireWithSpecificName {
    [self STCDestroyCredential:GSS_C_NO_OID];

    OM_uint32 maj_stat;
    gss_name_t gname = GSS_C_NO_NAME;
    gss_cred_id_t cred = NULL;
    CFErrorRef error = NULL;
    
    
    gname = GSSCreateName(@"ktestuser@QAD.APPLE.COM", GSS_C_NT_USER_NAME, &error);
    STAssertTrue(gname, @"failed to create name");
    if (gname == NULL) {
        [self STCOutput:@"GSSCreateName error: %@", error];
        return;
    }
    
    CFUUIDRef uuid = CFUUIDCreateFromString(NULL, CFSTR("E5ECDD5B-1348-4452-A31A-A0A791F94114"));
    
    NSDictionary *attrs = @{
        (id)kGSSICKerberosCacheName : @"XCACHE:E5ECDD5B-1348-4452-A31A-A0A791F94114",
        (id)kGSSICPassword : passwordKtestuserQAD,
    };

    
    maj_stat = gss_aapl_initial_cred(gname, GSS_KRB5_MECHANISM, (__bridge CFDictionaryRef)attrs, &cred, &error);
    CFRelease(gname);
    STAssertTrue(maj_stat == 0, @"failed to acquire cred");
    if (maj_stat) {
        [self STCOutput:@"error: %d %@", (int)maj_stat, error];
        return;
    }
    
    CFUUIDRef creduuid = GSSCredentialCopyUUID(cred);
    
    STAssertTrue(CFEqual(creduuid, uuid), @"credential uuid not the same after acquire");
    
    CFRelease(cred);
    CFRelease(creduuid);
    CFRelease(uuid);
}
#endif

- (void)testAcquireCaseInsensitive {
    [self STCDestroyCredential:GSS_C_NO_OID];
    
    OM_uint32 maj_stat;
    gss_name_t gname = GSS_C_NO_NAME;
    gss_cred_id_t cred = NULL;
    CFErrorRef error = NULL;
    
    
    gname = GSSCreateName(@"ktestuser@qad.apple.com", GSS_C_NT_USER_NAME, NULL);
    STAssertTrue(gname, @"failed to create name");
    if (gname == NULL)
        return;
    
    NSDictionary *attrs = @{
        (id)kGSSICPassword : passwordKtestuserQAD
    };
    
    
    maj_stat = gss_aapl_initial_cred(gname, GSS_KRB5_MECHANISM, (__bridge CFDictionaryRef)attrs, &cred, &error);
    CFRelease(gname);
    STAssertTrue(maj_stat == 0, @"failed to acquire cred");
    if (maj_stat) {
        [self STCOutput:@"error: %d %@", (int)maj_stat, error];
        return;
    }
    
    CFRelease(cred);
}



- (void)testAcquireQAD_ISC_HTTP {
    gss_cred_id_t cred = NULL;
    
    [self STCDestroyCredential:GSS_C_NO_OID];
    
    cred = [self STCAcquireCredential:@"ktestuser@QAD.APPLE.COM" withPassword:passwordKtestuserQAD mech:GSS_KRB5_MECHANISM];
    STAssertTrue(cred, @"Failed to acquire credential from QAD");
    if (cred == NULL)
        return;
    
    STAssertTrue([self STCAuthenticate:cred nameType:GSS_C_NT_HOSTBASED_SERVICE toServer:@"HTTP@dc01qad.qad.apple.com"],
                 @"Failed to get HTTP/dc01.qad.apple.com credentials");
    
    CFRelease(cred);
}





@end
