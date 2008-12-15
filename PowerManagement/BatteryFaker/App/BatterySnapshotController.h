/* BatterySnapshotController */

#import <Cocoa/Cocoa.h>

@interface BatterySnapshotController : NSObject
{
    NSMutableArray         *orderedMenuTitles;
    NSMutableDictionary    *snapshotDescriptions;
}

- (NSArray *)menuTitlesForSnapshots;

- (NSDictionary *)batterySnapshotForTitle:(NSString *)title;

@end
