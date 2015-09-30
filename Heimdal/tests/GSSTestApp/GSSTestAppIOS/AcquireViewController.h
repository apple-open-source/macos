//
//  Copyright (c) 2013 Apple, Inc. All rights reserved.
//

#import <Foundation/Foundation.h>
#import "CredentialTableController.h"

@interface AcquireViewController : UIViewController <GSSCredentialsChangeNotification>

@property (weak) IBOutlet UITextField *username;
@property (weak) IBOutlet UISwitch *doPassword;
@property (weak) IBOutlet UITextField *password;
@property (weak) IBOutlet UISwitch *doCertificate;
@property (weak) IBOutlet UILabel *certificateLabel;
@property (weak) IBOutlet UILabel *statusLabel;
@property (weak) IBOutlet UITextField *kdchostname;


@property (weak) IBOutlet UITableView *credentialsTableView;
@property (assign) CredentialTableController *credentialsTableController;

@end
