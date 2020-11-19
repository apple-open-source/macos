#ifndef SecDbBackupTestsBase_h
#define SecDbBackupTestsBase_h

#import <XCTest/XCTest.h>

// This isn't inheritance-based data hiding, this whole class is for convenience building tests
#import "keychain/securityd/CheckV12DevEnabled.h"
#import "CKKS.h"
#import <utilities/SecFileLocations.h>
#import "spi.h"
#import "SecItemServer.h"

@interface SecDbBackupTestsBase : XCTestCase

+ (void)setV12Development:(BOOL)newState;

@end

#endif /* SecDbBackupTestsBase_h */
