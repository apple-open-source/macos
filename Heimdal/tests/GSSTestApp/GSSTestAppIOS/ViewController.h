//
//  ViewController.h
//  GSSTestApp
//
//  Copyright (c) 2013 Apple, Inc. All rights reserved.
//

#import <UIKit/UIKit.h>
#import <TargetConditionals.h>

#import "CredentialTableController.h"

@interface ViewController : UIViewController <GSSCredentialsChangeNotification>

@property (weak) IBOutlet UITableView *credentialsTableView;
@property (assign) CredentialTableController *credentialsTableController;

@end
