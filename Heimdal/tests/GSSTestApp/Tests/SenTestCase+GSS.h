//
//  SenTestCase+GSS.h
//  GSSTestApp
//
//  Created by Love Hörnquist Åstrand on 2013-06-08.
//  Copyright (c) 2013 Apple, Inc. All rights reserved.
//

#include <SenTestingKit/SenTestingKit.h>
#include <GSS/GSS.h>

@interface SenTestCase (GSS)

- (void)STCDestroyCredential:(gss_OID)mech;
- (gss_cred_id_t)STCAcquireCredential:(NSString *)name withPassword:(NSString *)password mech:(gss_OID)mech;
- (BOOL)STCAuthenticate:(gss_cred_id_t)cred nameType:(gss_OID)nameType toServer:(NSString *)serverName;
- (void)STCOutput:(NSString *)output;

@end
