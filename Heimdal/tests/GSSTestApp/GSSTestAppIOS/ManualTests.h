//
//  ManualTests.h
//  GSSTestApp
//
//  Created by Love Hörnquist Åstrand on 2013-07-01.
//  Copyright (c) 2013 Apple, Inc. All rights reserved.
//

#import <Foundation/Foundation.h>

@interface ManualTests : UIViewController

@property (weak) IBOutlet UITextField *username;
@property (weak) IBOutlet UISwitch *doPassword;
@property (weak) IBOutlet UITextField *password;
@property (weak) IBOutlet UISwitch *doCertificate;
@property (weak) IBOutlet UILabel *certificateLabel;
@property (weak) IBOutlet UILabel *statusLabel;
@property (weak) IBOutlet UITextField *kdchostname;


@end
