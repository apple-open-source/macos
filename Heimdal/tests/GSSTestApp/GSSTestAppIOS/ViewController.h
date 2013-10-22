//
//  ViewController.h
//  GSSTestApp
//
//  Created by Love Hörnquist Åstrand on 2013-06-07.
//  Copyright (c) 2013 Apple, Inc. All rights reserved.
//

#import <UIKit/UIKit.h>
#import "TestHarness.h"

@interface ViewController : UIViewController <TestHarnessProtocol>

@property (weak) IBOutlet UIBarButtonItem *runTestsButton;
@property (weak) IBOutlet UILabel *statusLabel;
@property (weak) IBOutlet UITextView *progressTextView;

- (IBAction)runTests:(id)sender;


@end
