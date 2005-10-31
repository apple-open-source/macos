/*
 * KerberosAgentController.m
 *
 * $Header: /cvs/kfm/KerberosFramework/KerberosLogin/Sources/KerberosLoginServer/KerberosAgentController.m,v 1.10 2005/06/07 23:42:01 lxs Exp $
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
#include <Carbon/Carbon.h>
#include <CoreFoundation/CoreFoundation.h>
#include <SystemConfiguration/SystemConfiguration.h>

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

    // Delete the services menu
    NSMenu *mainMenu = [NSApp mainMenu];
    if ((mainMenu != NULL) && ([mainMenu numberOfItems] > 1)) {
        NSMenuItem *appMenuItem = [mainMenu itemAtIndex: 0];
        if ((appMenuItem != NULL) && [appMenuItem hasSubmenu]) {
            NSMenu *appMenu = [appMenuItem submenu];
            if ((appMenu != NULL) && ([appMenu numberOfItems] == 7)) {
                [appMenu removeItemAtIndex: 2]; // services
                [appMenu removeItemAtIndex: 2]; // separator after services
            }
        }
    }
}

// ---------------------------------------------------------------------------

- (void) applicationDidFinishLaunching: (NSNotification *) notification
{
    OSStatus err = 0;
    uid_t uid = 0;
    uid_t gid = 0;
    
    NSString *consoleUserName = (NSString *) SCDynamicStoreCopyConsoleUser(NULL, &uid, &gid);
    dprintf ("applicationDidFinishLaunching: SCDynamicStoreCopyConsoleUser returned '%s' (uid = %d, gid = %d)", 
             ((consoleUserName == NULL) ? "(null)" : [consoleUserName UTF8String]), uid, gid);
    
    if (consoleUserName == NULL) {
        // We are running under the loginwindow
        err = SetSystemUIMode (kUIModeAllHidden, 
                               kUIOptionDisableProcessSwitch|kUIOptionDisableForceQuit|kUIOptionDisableSessionTerminate);
    } else {
        err = SetSystemUIMode (kUIModeNormal, kUIOptionDisableAppleMenu);
    }
    dprintf ("applicationDidFinishLaunching: SetSystemUIMode returned err %ld", err);
}

// ---------------------------------------------------------------------------

- (IBAction) showAboutBox: (id) sender
{
    [aboutWindow center];
    [aboutWindow makeKeyAndOrderFront: self];
    [NSApp mainMenu];
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
