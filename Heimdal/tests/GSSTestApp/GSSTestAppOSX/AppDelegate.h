//
//  AppDelegate.h
//  GSSTestAppOSX
//
//  Created by Love Hörnquist Åstrand on 2013-06-08.
//  Copyright (c) 2013 Apple, Inc. All rights reserved.
//

#import <Cocoa/Cocoa.h>
#import "TestHarness.h"

@interface AppDelegate : NSObject <NSApplicationDelegate, TestHarnessProtocol>

@property (assign) IBOutlet NSWindow *window;
@property (assign) IBOutlet NSTabView *tabView;

@property (assign) IBOutlet NSButton *runTestsButton;
@property (assign) IBOutlet NSTextField *statusLabel;
@property (assign) IBOutlet NSTextView *progressTextView;
@property (assign) IBOutlet NSArrayController *certificateArrayController;

@property (assign) IBOutlet NSTextField *username;
@property (assign) IBOutlet NSSecureTextField *password;
@property (assign) IBOutlet NSMatrix *credentialRadioButton;
@property (assign) IBOutlet NSPopUpButton *certificiatePopUp;
@property (assign) IBOutlet NSTextField *kdchostname;

@end
