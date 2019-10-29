//
//  mockaksxcbase.h
//  Security
//

#ifndef mockaksxcbase_h
#define mockaksxcbase_h

#import <XCTest/XCTest.h>

@interface mockaksxcbase : XCTestCase
@property NSString *testHomeDirectory;
@property long lockCounter;

- (NSString*)createKeychainDirectoryWithSubPath:(NSString*)subpath;

- (void)addAccessGroup:(NSString *)accessGroup;

@end

#endif /* mockaksxcbase_h */
