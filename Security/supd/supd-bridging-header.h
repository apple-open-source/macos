//
//  supd-bridging-header.h
//

#import "supd.h"
#import <Security/SFAnalyticsSQLiteStore.h>

NS_ASSUME_NONNULL_BEGIN

@interface SFAnalyticsSQLiteStore (glueWhileBNICatchesUp)
@property (readonly, strong) NSString* databaseBasename;
- (void)streamEventsWithLimit:(NSNumber *_Nullable)limit
                    fromTable:(NSString *)table
                 eventHandler:(bool (^)(NSData *event))eventHandler;
- (void)clearAllData;
@end

NS_ASSUME_NONNULL_END
