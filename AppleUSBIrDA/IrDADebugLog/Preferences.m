/*
 * Copyright (c) 2003 Apple Computer, Inc. All rights reserved.
 *
 * @APPLE_LICENSE_HEADER_START@
 * 
 * Copyright (c) 1999-2003 Apple Computer, Inc.  All Rights Reserved.
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
#import "Preferences.h"

@implementation Preferences

NSString *DefaultDriverKey = @"DefaultDriver";

static NSString *nibName = @"Preferences";

- (IBAction)update:(id)sender {
    // provide a single update action method for all defaults
    // it's a lot easier than setting each indvidually...
    NSUserDefaults *defaults = [NSUserDefaults standardUserDefaults];
    [defaults setObject: [defaultDriverField stringValue] forKey: DefaultDriverKey];
 }

- (void)windowDidResignKey:(NSNotification *)n {
    // make sure defaults are saved automatically after edits
    [self update: self];
}

- (void)windowDidClose:(NSNotification *)n {
    // make sure defaults are saved automatically when window closed
    [self update: self];
}

- (IBAction)show:(id)sender
{
    NSUserDefaults *defaults = [NSUserDefaults standardUserDefaults];
    if (!panel) {
        if (![NSBundle loadNibNamed: nibName owner: self])
            NSLog(@"Unable to load nib \"%@\"",nibName);
    }
    [defaultDriverField setStringValue: [defaults objectForKey: DefaultDriverKey]];
    [panel makeKeyAndOrderFront: self];
}

@end
