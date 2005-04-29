/*
 * KerberosAgentController.m
 *
 * $Header: /cvs/kfm/KerberosFramework/KerberosLogin/Sources/KerberosLoginServer/KerberosAgentController.m,v 1.8 2005/01/23 17:53:20 lxs Exp $
 *
 * Copyright 2004 Massachusetts Institute of Technology.
 * All Rights Reserved.
 *
 * Export of this software from the United States of America may
 * require a specific license from the United States Government.
 * It is the responsibility of any person or organization contemplating
 * export to obtain such a license before exporting.
 * 
 * WITHIN THAT CONSTRAINT, permission to use, copy, modify, and
 * distribute this software and its documentation for any purpose and
 * without fee is hereby granted, provided that the above copyright
 * notice appear in all copies and that both that copyright notice and
 * this permission notice appear in supporting documentation, and that
 * the name of M.I.T. not be used in advertising or publicity pertaining
 * to distribution of the software without specific, written prior
 * permission.  Furthermore if you modify this software you must label
 * your software as modified software and not distribute it in such a
 * fashion that it might be confused with the original M.I.T. software.
 * M.I.T. makes no representations about the suitability of
 * this software for any purpose.  It is provided "as is" without express
 * or implied warranty.
 */

#import "KerberosAgentController.h"

@implementation KerberosAgentController

// ---------------------------------------------------------------------------

- (id) init
{
    if ((self = [super init])) {
        activeWindows = [[NSMutableArray alloc] init];
        if (activeWindows == NULL) {
            [self release];
            return NULL;
        }
    }
    
    return self;
}

// ---------------------------------------------------------------------------

- (void) dealloc
{
    if (activeWindows != NULL) { [activeWindows release]; }

    [super dealloc];
}

// ---------------------------------------------------------------------------

- (void) awakeFromNib 
{
    // Fill in the version field in the about dialog
    NSString *name      = [[NSBundle mainBundle] objectForInfoDictionaryKey: @"CFBundleName"];
    NSString *version   = [[NSBundle mainBundle] objectForInfoDictionaryKey: @"KfMDisplayVersion"];
    NSString *copyright = [[NSBundle mainBundle] objectForInfoDictionaryKey: @"KfMDisplayCopyright"];
    
    [aboutVersionTextField setStringValue: [NSString stringWithFormat: @"%@ %@", 
                                               (name != NULL) ? name : @"", (version != NULL) ? version : @""]];    
    [aboutCopyrightTextField setStringValue: (copyright != NULL) ? copyright : @""];
}

// ---------------------------------------------------------------------------

- (IBAction) showAboutBox: (id) sender
{
    [aboutWindow center];
    [aboutWindow makeKeyAndOrderFront: self];
}

// ---------------------------------------------------------------------------

- (void) addActiveWindow: (NSWindow *) window
{
    if (window != NULL) {
        [activeWindows addObject: window];
    }
}

// ---------------------------------------------------------------------------

- (void) removeActiveWindow: (NSWindow *) window
{
    if (window != NULL) {
        [activeWindows removeObject: window];
    }
}

// ---------------------------------------------------------------------------

- (NSWindow *) activeWindow
{
    return [activeWindows lastObject];
}

@end
