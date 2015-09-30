//
//  SenTestCase+GSS.h
//  GSSTestApp
//
//  Created by Love Hörnquist Åstrand on 2013-06-08.
//  Copyright (c) 2013 Apple, Inc. All rights reserved.
//

#import "FakeXCTest.h"
#include <GSS/GSS.h>

@interface XCTestCase (GSS)

- (void)XTCDestroyCredential:(gss_OID)mech;
- (gss_cred_id_t)XTCAcquireCredential:(NSString *)name withOptions:(NSDictionary *)options mech:(gss_OID)mech;
- (BOOL)STCAuthenticate:(gss_cred_id_t)cred nameType:(gss_OID)nameType toServer:(NSString *)serverName;
- (void)XCTOutput:(NSString *)output, ... NS_FORMAT_FUNCTION(1,2);

@end
