//
//  SupdTests.h
//  Security
//

#import <XCTest/XCTest.h>

NS_ASSUME_NONNULL_BEGIN

@interface SupdTests : XCTestCase
- (SFAnalyticsTopic * _Nullable)SWTransparencyTopic;

@property (readonly) SFAnalytics* swtransparencyAnalytics;
@property (readonly) supd* supd;

+ (NSData *_Nullable)supd_gzipInflate:(NSData *_Nullable)data;

@end

NS_ASSUME_NONNULL_END
