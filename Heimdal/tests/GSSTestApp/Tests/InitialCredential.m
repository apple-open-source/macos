//
//  InitialCredential.m
//  GSSTestApp
//
//  Created by Love Hörnquist Åstrand on 2013-06-07.
//  Copyright (c) 2013 Apple, Inc. All rights reserved.
//

#import "InitialCredential.h"

@implementation InitialCredential

- (void)testAcquireADS {
    gss_cred_id_t cred = NULL;
    
    [self STCDestroyCredential:GSS_C_NO_OID];
    
    cred = [self STCAcquireCredential:@"ktestuser@ADS.APPLE.COM" withPassword:@"foobar" mech:GSS_KRB5_MECHANISM];
    STAssertTrue(cred, @"Failed to acquire credential from ADS");
    if (cred == NULL)
        return;

    CFRelease(cred);
}


- (void)testAcquireWithSpecificName {
#if TARGET_OS_IPHONE
    [self STCDestroyCredential:GSS_C_NO_OID];

    OM_uint32 maj_stat;
    gss_name_t gname = GSS_C_NO_NAME;
    gss_cred_id_t cred = NULL;
    CFErrorRef error = NULL;
    
    
    gname = GSSCreateName(@"ktestuser@ADS.APPLE.COM", GSS_C_NT_USER_NAME, NULL);
    STAssertTrue(gname, @"failed to create name");
    if (gname == NULL)
        return;
    
    NSString *password = @"foobar";
    
    CFUUIDRef uuid = CFUUIDCreateFromString(NULL, CFSTR("E5ECDD5B-1348-4452-A31A-A0A791F94114"));
    
    NSDictionary *attrs = @{
                            (id)kGSSICKerberosCacheName : @"XCACHE:E5ECDD5B-1348-4452-A31A-A0A791F94114",
                            (id)kGSSICPassword : password
                            };

    
    maj_stat = gss_aapl_initial_cred(gname, GSS_KRB5_MECHANISM, (__bridge CFDictionaryRef)attrs, &cred, &error);
    CFRelease(gname);
    STAssertTrue(maj_stat == 0, @"failed to acquire cred");
    if (maj_stat) {
        NSLog(@"error: %d %@", (int)maj_stat, error);
        return;
    }
    
    CFUUIDRef creduuid = GSSCredentialCopyUUID(cred);
    
    STAssertTrue(CFEqual(creduuid, uuid), @"credential uuid not the same after acquire");
    
    CFRelease(cred);
    CFRelease(creduuid);
    CFRelease(uuid);
#endif
}

- (void)testAcquireCaseInsensitive {
    [self STCDestroyCredential:GSS_C_NO_OID];
    
    OM_uint32 maj_stat;
    gss_name_t gname = GSS_C_NO_NAME;
    gss_cred_id_t cred = NULL;
    CFErrorRef error = NULL;
    
    
    gname = GSSCreateName(@"ktestuser@ads.apple.com", GSS_C_NT_USER_NAME, NULL);
    STAssertTrue(gname, @"failed to create name");
    if (gname == NULL)
        return;
    
    NSString *password = @"foobar";
    
    NSDictionary *attrs = @{
                            (id)kGSSICPassword : password
                            };
    
    
    maj_stat = gss_aapl_initial_cred(gname, GSS_KRB5_MECHANISM, (__bridge CFDictionaryRef)attrs, &cred, &error);
    CFRelease(gname);
    STAssertTrue(maj_stat == 0, @"failed to acquire cred");
    if (maj_stat) {
        NSLog(@"error: %d %@", (int)maj_stat, error);
        return;
    }
    
    CFRelease(cred);
}



- (void)testAcquireADS_ISC_HTTPS {
    gss_cred_id_t cred = NULL;
    
    [self STCDestroyCredential:GSS_C_NO_OID];
    
    cred = [self STCAcquireCredential:@"ktestuser@ADS.APPLE.COM" withPassword:@"foobar" mech:GSS_KRB5_MECHANISM];
    STAssertTrue(cred, @"Failed to acquire credential from ADS");
    if (cred == NULL)
        return;
    
    STAssertTrue([self STCAuthenticate:cred nameType:GSS_C_NT_HOSTBASED_SERVICE toServer:@"HTTP@dc03.ads.apple.com"],
                 @"Failed to gt HTTP/dc03 credentials");
    
    CFRelease(cred);
}





@end
