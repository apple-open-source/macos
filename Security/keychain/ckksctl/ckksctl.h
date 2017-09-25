//
//  Security
//

#import <Foundation/Foundation.h>

@interface CKKSControl : NSObject
- (NSDictionary<NSString *, id> *)printPerformanceCounters;
- (NSDictionary<NSString *, id> *)status: (NSString*) view;
- (void)status_custom: (NSString*) view;
- (void)resync: (NSString*) view;

- (void)resetLocal:    (NSString*)view;
- (void)resetCloudKit: (NSString*) view;

- (void)getAnalyticsJSON;
- (void)forceAnalyticsUpload;
@end
