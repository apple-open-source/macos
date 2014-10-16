//
//  KCATableViewController.h
//  Security
//
//  Created by John Hurley on 10/22/12.
//
//

#import <UIKit/UIKit.h>

@class DeviceItemCell;

@interface DeviceTableViewController : UITableViewController
//@property (assign, nonatomic) IBOutlet KeychainItemCell *kcitemCell;
@property (strong, nonatomic) NSArray *kcItemStatuses;
@property (strong, nonatomic) NSArray *kcItemNames;

- (NSArray *)getItems;

@end
