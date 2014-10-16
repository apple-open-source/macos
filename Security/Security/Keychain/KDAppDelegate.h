/*
 * Copyright (c) 2013-2014 Apple Inc. All Rights Reserved.
 *
 * @APPLE_LICENSE_HEADER_START@
 * 
 * This file contains Original Code and/or Modifications of Original Code
 * as defined in and that are subject to the Apple Public Source License
 * Version 2.0 (the 'License'). You may not use this file except in
 * compliance with the License. Please obtain a copy of the License at
 * http://www.opensource.apple.com/apsl/ and read it before using this
 * file.
 * 
 * The Original Code and all software distributed under the License are
 * distributed on an 'AS IS' basis, WITHOUT WARRANTY OF ANY KIND, EITHER
 * EXPRESS OR IMPLIED, AND APPLE HEREBY DISCLAIMS ALL SUCH WARRANTIES,
 * INCLUDING WITHOUT LIMITATION, ANY WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE, QUIET ENJOYMENT OR NON-INFRINGEMENT.
 * Please see the License for the specific language governing rights and
 * limitations under the License.
 * 
 * @APPLE_LICENSE_HEADER_END@
 */


#import <Cocoa/Cocoa.h>
#import "KDSecItems.h"
#import "KDSecCircle.h"

@interface KDAppDelegate : NSObject <NSApplicationDelegate>

@property (assign) IBOutlet NSWindow *window;
@property (assign) IBOutlet NSTableView *itemTable;
@property (assign) IBOutlet NSTextFieldCell *itemTableTitle;
@property (retain) id<NSTableViewDataSource> itemDataSource;

@property (assign) IBOutlet NSButton *enableKeychainSyncing;
@property (assign) IBOutlet NSTextFieldCell *circleStatusCell;
@property (assign) IBOutlet NSTextFieldCell *peerCountCell;
@property (assign) IBOutlet NSTextView *peerTextList;
@property (assign) IBOutlet NSTextFieldCell *applicantCountCell;
@property (assign) IBOutlet NSTextView *applicantTextList;
@property (assign) IBOutlet NSProgressIndicator *syncSpinner;

@property (retain) KDSecCircle *circle;

@property (retain) NSMutableArray *stuffNotToLeak;

-(IBAction)enableKeychainSyncingClicked:(id)sender;
@end
