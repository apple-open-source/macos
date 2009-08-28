#import <assert.h>

#import "BatterySnapshotController.h"
static NSString   *kSnapFileName = @"BatterySnapshots.xml";

@implementation BatterySnapshotController

- (id)init
{
    NSArray     *fileContents = NULL;
    NSString    *snapFilePath = NULL;
    
    int             i = 0;
    NSObject        *obj;
    NSString        *insertString = NULL;
    NSDictionary    *snapshotDictionary = NULL;
    
    
    orderedMenuTitles = [[NSMutableArray alloc] init];
    snapshotDescriptions = [[NSMutableDictionary alloc] init];
    
    /** ** ** ** ** ** ** ** ** ** ** ** ** ** ** ** ** ** ** ** ** ** ** ** **
     ** Open file and read array
     ** ** ** ** ** ** ** ** ** ** ** ** ** ** ** ** ** ** ** ** ** ** ** ** **/
    snapFilePath = [[NSBundle mainBundle] resourcePath];
    snapFilePath = [snapFilePath stringByAppendingPathComponent:kSnapFileName];

    fileContents = [NSArray arrayWithContentsOfFile:snapFilePath];
    assert(fileContents);

    /** ** ** ** ** ** ** ** ** ** ** ** ** ** ** ** ** ** ** ** ** ** ** ** **
     ** Build menu titles array
     ** And dictionary mapping titles to dictionaries.
     ** ** ** ** ** ** ** ** ** ** ** ** ** ** ** ** ** ** ** ** ** ** ** ** **/
    for (i=0; i<[fileContents count]; i++) 
    {
        insertString = NULL;
        obj = [fileContents objectAtIndex:i];
        if ([obj isKindOfClass:[NSString class]])
        {
            insertString = (NSString *)obj;
            assert( [insertString isEqualToString:@"Menu Separator"]);
        } else if ([obj isKindOfClass:[NSDictionary class]])
        {
            snapshotDictionary = (NSDictionary *)obj;
            insertString = [snapshotDictionary objectForKey:@"SnapshotTitle"];
        
            [snapshotDescriptions setObject:snapshotDictionary
                                   forKey:insertString];
        }

        if (insertString) {
            [orderedMenuTitles addObject:insertString];
        }
    }

    return [super init];
}

- (void) free
{
    [orderedMenuTitles release];
    [snapshotDescriptions release];
}

- (NSArray *)menuTitlesForSnapshots
{
    return orderedMenuTitles;
}

- (NSDictionary *)batterySnapshotForTitle:(NSString *)title
{
    NSLog(@"%@", [[snapshotDescriptions objectForKey:title] description]);
    return [snapshotDescriptions objectForKey:title];
}

@end