//
//  KeychainItemCell.h
//  Security
//
//  Created by John Hurley on 10/22/12.
//
//

#import <UIKit/UIKit.h>
#import "CircleStatusView.h"

@interface KeychainItemCell : UITableViewCell
{
    uint32_t flashCounter;
    NSTimer *flashTimer;
}
@property (weak, nonatomic) IBOutlet ItemStatusView *itemStatus;
@property (weak, nonatomic) IBOutlet UILabel *itemName;
@property (weak, nonatomic) IBOutlet UILabel *itemAccount;

- (void)startCellFlasher;

@end
