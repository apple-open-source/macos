//
//  KeychainAnalytics-bridging-header.h
//  Security
//

#import "supd/supd.h"
#import "Analytics/SFAnalyticsSQLiteStore.h"
#import "supd/Tests/SupdTests.h"

NS_ASSUME_NONNULL_BEGIN

@interface SFAnalyticsSQLiteStore (glueWhileBNICatchesUp)
@property (readonly, strong) NSString* databaseBasename;
- (void)streamEventsWithLimit:(NSNumber *_Nullable)limit
                    fromTable:(NSString *)table
                 eventHandler:(bool (^)(NSData *event))eventHandler;
- (void)clearAllData;
@end

NS_ASSUME_NONNULL_END
