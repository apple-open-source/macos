//
//  DeviceItemCell.h
//  Security
//
//

#import <UIKit/UIKit.h>

@interface DeviceItemCell : UITableViewCell
@property (weak, nonatomic) IBOutlet UILabel *itemDeviceName;
@property (weak, nonatomic) IBOutlet UILabel *itemPeerID;
@property (weak, nonatomic) IBOutlet UILabel *itemDeviceStatus;

@end
