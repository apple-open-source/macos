//
//  ViewController.h
//  GSSTestApp
//
//  Created by Love Hörnquist Åstrand on 2013-06-07.
//  Copyright (c) 2013 Apple, Inc. All rights reserved.
//

#import <UIKit/UIKit.h>
#import "CredentialTableController.h"

@interface ViewController : UIViewController <GSSCredentialsChangeNotification>

@property (weak) IBOutlet UITableView *credentialsTableView;
@property (assign) CredentialTableController *credentialsTableController;


@end
