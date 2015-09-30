//
//  RegressionTestsViewController.h
//  GSSTestApp
//
//  Created by Love Hörnquist Åstrand on 2014-08-15.
//  Copyright (c) 2014 Apple, Inc. All rights reserved.
//

#import <UIKit/UIKit.h>

@interface RegressionTestsViewController : UIViewController

@property (weak) IBOutlet UIBarButtonItem *runTestsButton;
@property (weak) IBOutlet UILabel *statusLabel;
@property (weak) IBOutlet UITextView *progressTextView;

- (IBAction)runTests:(id)sender;
- (void)appendProgress:(NSString *)string color:(UIColor *)color;

@end
