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
#import "AppController.h"

@implementation AppController

+ (void)initialize {
    /*
    Initalize executes before the first message to the class is
    serviced therefore, we get our factory settings registered.
    Note that we do this here rather than in the Preferences as the
    Preferences instance is loaded lazily, so the Prefernces class
    may not be initialized before we need our first default
    (see applicationShouldOpenUntitledFile).
    */
    NSString		*path			= [[NSBundle mainBundle] pathForResource:@"Defaults" ofType:@"plist"];
    NSDictionary	*defaultValues	= [NSDictionary dictionaryWithContentsOfFile: path];
   [[NSUserDefaults standardUserDefaults] registerDefaults: defaultValues];
}


- (IBAction)showPreferences:(id)sender {
    [[self preferences] show:self];
}

- (Preferences *)preferences {
    // load preferences lazily
    if (preferences == nil) {
        Preferences *p = [[Preferences alloc] init];
        [self setPreferences:p];
        [p release];
    }
    return preferences;
}
- (void)setPreferences:(Preferences *)newPreferences {
    [newPreferences retain];
    [preferences release];
    preferences = newPreferences;
}

- (void)dealloc {
    [self setPreferences:nil];
    [super dealloc];
}


@end
