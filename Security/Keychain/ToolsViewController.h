//
//  ToolsViewController.h
//  Keychain
//
//  Created by john on 10/22/12.
//
//

#import <UIKit/UIKit.h>
#import <PeerListCell.h>

@interface ToolsViewController : UIViewController  {
    int     notificationToken;
    int     notificationCount;
}
@property (weak, nonatomic) IBOutlet UIButton *autopopulateButton;
@property (weak, nonatomic) IBOutlet UIButton *clearButton;
@property (weak, nonatomic) IBOutlet UIButton *clearKVS;
@property (weak, nonatomic) IBOutlet UIButton *buttonC;
@property (weak, nonatomic) IBOutlet UILabel *kvsCleared;
@property (weak, nonatomic) IBOutlet UILabel *statusMessage;

- (void)setStatus:(NSString *)message;

@end
