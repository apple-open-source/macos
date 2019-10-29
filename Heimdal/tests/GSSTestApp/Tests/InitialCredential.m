//
//  InitialCredential.m
//  GSSTestApp
//
//  Created by Love Hörnquist Åstrand on 2013-06-07.
//  Copyright (c) 2013 Apple, Inc. All rights reserved.
//

#import "FakeXCTest.h"
#import "XCTestCase+GSS.h"
#import "TestUsers.h"

@interface InitialCredential : XCTestCase

@end

@implementation InitialCredential

- (void)testAcquireCEAD {
    gss_cred_id_t cred = NULL;
    
    [self XTCDestroyCredential:GSS_C_NO_OID];

    NSDictionary *options = @{
        (id)kGSSICPassword : passwordKtestuserCEAD
    };

    cred = [self XTCAcquireCredential:@"heimdal001@CORP.CEAD.APPLE.COM" withOptions:options mech:GSS_KRB5_MECHANISM];
    XCTAssertTrue(cred, @"Failed to acquire credential from QAD");
    if (cred == NULL)
        return;

    {
	OM_uint32 maj_stat, min_stat;
	gss_cred_id_t cred2 = NULL;
	gss_buffer_desc buffer;

	maj_stat = gss_export_cred(&min_stat, cred, &buffer);
	XCTAssertTrue (maj_stat == GSS_S_COMPLETE, @"GSS export failed");

	maj_stat = gss_import_cred(&min_stat, &buffer, &cred2);
	XCTAssertTrue (maj_stat == GSS_S_COMPLETE, @"GSS import failed");

	gss_release_buffer(&min_stat, &buffer);
	if (cred2)
	    CFRelease(cred2);
    }

    CFRelease(cred);
}

#if TARGET_OS_IPHONE
- (void)testAcquireWithSpecificName {
    [self XTCDestroyCredential:GSS_C_NO_OID];

    OM_uint32 maj_stat;
    gss_name_t gname = GSS_C_NO_NAME;
    gss_cred_id_t cred = NULL;
    CFErrorRef error = NULL;
    
    
    gname = GSSCreateName(@"heimdal001@CORP.CEAD.APPLE.COM", GSS_C_NT_USER_NAME, &error);
    XCTAssertTrue(gname, @"failed to create name");
    if (gname == NULL) {
        [self XCTOutput:@"GSSCreateName error: %@", error];
        return;
    }
    
    CFUUIDRef uuid = CFUUIDCreateFromString(NULL, CFSTR("E5ECDD5B-1348-4452-A31A-A0A791F94114"));
    
    NSDictionary *attrs = @{
        (id)kGSSICKerberosCacheName : @"XCACHE:E5ECDD5B-1348-4452-A31A-A0A791F94114",
        (id)kGSSICPassword : passwordKtestuserCEAD,
    };

    
    maj_stat = gss_aapl_initial_cred(gname, GSS_KRB5_MECHANISM, (__bridge CFDictionaryRef)attrs, &cred, &error);
    CFRelease(gname);
    XCTAssertTrue(maj_stat == 0, @"failed to acquire cred");
    if (maj_stat) {
        [self XCTOutput:@"error: %d %@", (int)maj_stat, error];
        return;
    }
    
    CFUUIDRef creduuid = GSSCredentialCopyUUID(cred);

    XCTAssert(creduuid, @"No credential uuid for %@", cred);
    
    XCTAssertTrue(creduuid && CFEqual(creduuid, uuid), @"credential uuid not the same after acquire");
    
    CFRelease(cred);
    if (creduuid)
        CFRelease(creduuid);
    CFRelease(uuid);
}
#endif

- (void)testAcquireCaseInsensitive {
    [self XTCDestroyCredential:GSS_C_NO_OID];
    
    OM_uint32 maj_stat;
    gss_name_t gname = GSS_C_NO_NAME;
    gss_cred_id_t cred = NULL;
    CFErrorRef error = NULL;
    
    
    gname = GSSCreateName(@"heimdal001@corp.cead.apple.com", GSS_C_NT_USER_NAME, NULL);
    XCTAssertTrue(gname, @"failed to create name");
    if (gname == NULL)
        return;
    
    NSDictionary *attrs = @{
        (id)kGSSICPassword : passwordKtestuserCEAD
    };
    
    
    maj_stat = gss_aapl_initial_cred(gname, GSS_KRB5_MECHANISM, (__bridge CFDictionaryRef)attrs, &cred, &error);
    CFRelease(gname);
    XCTAssertTrue(maj_stat == 0, @"failed to acquire cred");
    if (maj_stat) {
        [self XCTOutput:@"error: %d %@", (int)maj_stat, error];
        return;
    }
    
    CFRelease(cred);
}



- (void)testAcquireCEAD_ISC_HTTP {
    gss_cred_id_t cred = NULL;
    
    [self XTCDestroyCredential:GSS_C_NO_OID];

    NSDictionary *options = @{
        (id)kGSSICPassword : passwordKtestuserCEAD
    };

    cred = [self XTCAcquireCredential:@"heimdal001@CORP.CEAD.APPLE.COM" withOptions:options mech:GSS_KRB5_MECHANISM];
    XCTAssertTrue(cred, @"Failed to acquire credential from CEAD");
    if (cred == NULL)
        return;
    
    XCTAssertTrue([self STCAuthenticate:cred nameType:GSS_C_NT_HOSTBASED_SERVICE toServer:@"HTTP@negotiate.cead.apple.com"],
                 @"Failed to get HTTP/negotiate.cead.apple.com credentials");
    
    CFRelease(cred);
}





@end
