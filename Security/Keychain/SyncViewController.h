//
//  SyncViewController.h
//  Keychain
//
//  Created by john on 10/22/12.
//

#import <UIKit/UIKit.h>
#import <PeerListCell.h>
#import "CircleStatusView.h"

@interface SyncViewController : UIViewController <UITableViewDataSource> {
    int     notificationToken;
    int     notificationCount;
}
@property (weak, nonatomic) IBOutlet UILabel *statusMessage;
@property (weak, nonatomic) IBOutlet UISwitch *syncingEnabled;
@property (weak, nonatomic) IBOutlet UIProgressView *circleProgress;
@property (weak, nonatomic) IBOutlet UILabel *circleStatus;
@property (weak, nonatomic) IBOutlet UILabel *updateCount;
@property (weak, nonatomic) IBOutlet UILabel *peerCount;
@property (weak, nonatomic) IBOutlet UILabel *applicantCount;
@property (weak, nonatomic) IBOutlet UITableView *peerList;
@property (weak, nonatomic) IBOutlet UITableView *applicantList;
@property (weak, nonatomic) IBOutlet UILabel *stateChanged;
@property (weak, nonatomic) IBOutlet UIButton *acceptButton;
@property (weak, nonatomic) IBOutlet CircleStatusView *stateChangedC;

- (void)flashChangeLight;
- (void)setStatus:(NSString *)message;
- (void)updateSyncingEnabledSwitch;
+ (void)requestToJoinCircle;

@end
