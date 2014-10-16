//
//  KCATableViewController.h
//  Security
//
//  Created by John Hurley on 10/22/12.
//
//

#import <UIKit/UIKit.h>

@class KeychainItemCell;

@interface KCATableViewController : UITableViewController
{
    int notificationToken;
    bool hasUnlockedRecently;
}
//@property (assign, nonatomic) IBOutlet KeychainItemCell *kcitemCell;
@property (strong, nonatomic) NSArray *kcItemStatuses;
@property (strong, nonatomic) NSArray *kcItemNames;
@property (strong, nonatomic) NSTimer *lockTimer;
@property (strong, nonatomic) NSTimer *recentTimer;

- (NSArray *)getItems;

@end
