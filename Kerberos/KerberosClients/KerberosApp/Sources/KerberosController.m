/*
 * KerberosController.m
 *
 * $Header$
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

#import "KerberosController.h"
#import "KerberosErrorAlert.h"
#import "KerberosCredential.h"
#import "Utilities.h"
#import <IOKit/pwr_mgt/IOPMLib.h>
#import <IOKit/IOMessage.h>
#import <Kerberos/KerberosLoginPrivate.h>


@implementation KerberosController

#pragma mark -
    
// ---------------------------------------------------------------------------

- (id) init
{
    if ((self = [super init])) {
        cacheCollection = [[KerberosCacheCollection sharedCacheCollection] retain];
        if (!cacheCollection) {
            [self release];
            return NULL;
        }
        
        minuteTimer = NULL;
        
        preferencesController = NULL;
        ticketListController = NULL;
        realmsEditorController = NULL;
        
        ticketsKerberosIconImage = NULL;
        warningKerberosIconImage = NULL;
        noTicketsKerberosIconImage = NULL;
        kerberosAppIconImage = NULL;   
    }
    return self;
}

// ---------------------------------------------------------------------------

- (void) dealloc
{
    [[NSNotificationCenter defaultCenter] removeObserver: self];
    [[[NSWorkspace sharedWorkspace] notificationCenter] removeObserver: self];
    
    if (cacheCollection           ) { [cacheCollection release]; }
    if (minuteTimer               ) { [minuteTimer release]; }
    if (preferencesController     ) { [preferencesController release]; }
    if (ticketListController      ) { [ticketListController release]; }
    if (ticketsKerberosIconImage  ) { [ticketsKerberosIconImage release]; }
    if (warningKerberosIconImage  ) { [warningKerberosIconImage release]; }
    if (noTicketsKerberosIconImage) { [noTicketsKerberosIconImage release]; }
    if (kerberosAppIconImage      ) { [kerberosAppIconImage release]; }
    
    [super dealloc];
}

#pragma mark --- Actions --

// ---------------------------------------------------------------------------

- (IBAction) getTickets: (id) sender
{
    KLStatus err = [KerberosPrincipal getTickets];
    if (!err) {
        [cacheCollection update];
    } else if (err != klUserCanceledErr) {
        [KerberosErrorAlert alertForError: err
                                   action: KerberosGetTicketsAction];
    }        
}

// ---------------------------------------------------------------------------

- (IBAction) changePasswordForSelectedCache: (id) sender
{
   if ([self haveTicketListWindow]) {
        [ticketListController changePasswordForSelectedCache: self];
    }
}

// ---------------------------------------------------------------------------

- (IBAction) changePasswordForActiveUser: (id) sender
{
    KerberosCache *cache = [cacheCollection defaultCache];
    if (cache) {
        KLStatus err = [[cache principal] changePassword];
        if (!err) {
            [cacheCollection update];
        } else if (err != klUserCanceledErr) {
            [KerberosErrorAlert alertForError: err
                                       action: KerberosChangePasswordAction];
        }        
    }
}

// ---------------------------------------------------------------------------

- (IBAction) destroyTicketsForSelectedCache: (id) sender
{
    if ([self haveTicketListWindow]) {
        [ticketListController destroyTicketsForSelectedCache: self];
    }
}

// ---------------------------------------------------------------------------

- (IBAction) destroyTicketsForActiveUser: (id) sender
{
    KerberosCache *cache = [cacheCollection defaultCache];
    if (cache) {
        KLStatus err = [[cache principal] destroyTickets];
        if (!err) {
            [cacheCollection update];
        } else if (err != klUserCanceledErr) {
            [KerberosErrorAlert alertForError: err
                                       action: KerberosDestroyTicketsAction];
        }        
    }
}

// ---------------------------------------------------------------------------

- (IBAction) renewTicketsForSelectedCache: (id) sender
{
    if ([self haveTicketListWindow]) {
        [ticketListController renewTicketsForSelectedCache: self];
    }
}

// ---------------------------------------------------------------------------

- (IBAction) renewTicketsForActiveUser: (id) sender
{
    KerberosCache *cache = [cacheCollection defaultCache];
    if (cache) {
        KLStatus err = [[cache principal] renewTickets];
        if (!err) {
            [cacheCollection update];
        } else if (err != klUserCanceledErr) {
            [KerberosErrorAlert alertForError: err
                                       action: KerberosRenewTicketsAction];
        }        
    }
}

// ---------------------------------------------------------------------------

- (IBAction) validateTicketsForSelectedCache: (id) sender
{
    if ([self haveTicketListWindow]) {
        [ticketListController validateTicketsForSelectedCache: self];
    }
}

// ---------------------------------------------------------------------------

- (IBAction) validateTicketsForActiveUser: (id) sender
{
    KerberosCache *cache = [cacheCollection defaultCache];
    if (cache) {
        KLStatus err = [[cache principal] validateTickets];
        if (!err) {
            [cacheCollection update];
        } else if (err != klUserCanceledErr) {
            [KerberosErrorAlert alertForError: err
                                       action: KerberosValidateTicketsAction];
        }        
    }
}

// ---------------------------------------------------------------------------

- (IBAction) changeActiveUser: (id) sender
{
    if ([sender isMemberOfClass: [NSMenuItem class]]) {
        NSMenuItem *item = sender;
        NSMenu *menu = [item menu];
        int offset = 0;
        if (menu == dockMenu) {
            offset += [dockMenu indexOfItem: dockSeparatorItem] + 2; // include header item
        }
        
        if ([cacheCollection numberOfCaches] > 0) {
            KerberosCache *cache = [cacheCollection cacheAtIndex: ([menu indexOfItem: item] - offset)];
            if (![cache isDefault]) {
                KLStatus err = [[cache principal] setDefault];
                if (!err) {
                    [cacheCollection update];
                } else if (err != klUserCanceledErr) {
                    [KerberosErrorAlert alertForError: err
                                               action: KerberosChangeActiveUserAction];
                }        
            }
        }
    }
}

// ---------------------------------------------------------------------------

- (IBAction) showTicketInfo: (id) sender
{
    if ([self haveTicketListWindow]) {
        [ticketListController showTicketInfo: self];
    }
}

// ---------------------------------------------------------------------------

- (IBAction) showPreferences: (id) sender
{
    if (!preferencesController) {
        preferencesController = [[PreferencesController alloc] init];
    }
    [preferencesController showWindow: self];
}

// ---------------------------------------------------------------------------

- (IBAction) showAboutBox: (id) sender
{
    [aboutWindow center];
    [aboutWindow makeKeyAndOrderFront: self];
}

// ---------------------------------------------------------------------------

- (IBAction) showTicketList: (id) sender
{
    if (!ticketListController) {
        ticketListController = [[TicketListController alloc] init];
    }
    if (ticketListController) {
        [ticketListController showWindow: self];
        [self menuNeedsUpdate: ticketsMenu];
    }
}

// ---------------------------------------------------------------------------

- (IBAction) editRealms: (id) sender
{
    if (!realmsEditorController) {
        realmsEditorController = [[RealmsEditorController alloc] init];
    }
    [realmsEditorController showWindow: self];
}


#pragma mark --- Delegate Methods --

// ---------------------------------------------------------------------------

- (void) awakeFromNib
{
    dprintf ("Entering awakeFromNib...");

    // Make sure we are always prompted with the dialog even if we have a controlling terminal
    __KLSetPromptMechanism (klPromptMechanism_GUI);
    
    // Fill in the version field in the about dialog
    NSString *name      = [[NSBundle mainBundle] objectForInfoDictionaryKey: @"CFBundleName"];
    NSString *version   = [[NSBundle mainBundle] objectForInfoDictionaryKey: @"KfMDisplayVersion"];
    NSString *copyright = [[NSBundle mainBundle] objectForInfoDictionaryKey: @"KfMDisplayCopyright"];
    
    [aboutVersionTextField setStringValue: [NSString stringWithFormat: @"%@ %@", 
        (name != NULL) ? name : @"", (version != NULL) ? version : @""]];    
    [aboutCopyrightTextField setStringValue: (copyright != NULL) ? copyright : @""];
    
    // Set up timers
    
    minuteTimer = [[NSTimer scheduledTimerWithTimeInterval: 30 // controls the minute counter in the dock
                                                    target: self 
                                                  selector: @selector(minuteTimer:) 
                                                  userInfo: NULL
                                                   repeats: YES] retain];
   
    
    // Enable and disable menus in these lists manually 
    // since it's easier to manually enable and disable the active user list
    [ticketsMenu    setAutoenablesItems: NO];
    [activeUserMenu setAutoenablesItems: NO];
    [dockMenu       setAutoenablesItems: NO];
    
    [[NSNotificationCenter defaultCenter] addObserver: self
                                             selector: @selector (preferencesDidChange:)
                                                 name: PreferencesDidChangeNotification
                                               object: nil];
    
    [[NSNotificationCenter defaultCenter] addObserver: self
                                             selector: @selector (cacheCollectionDidChange:)
                                                 name: CacheCollectionDidChangeNotification
                                               object: nil];    
    
    [[NSNotificationCenter defaultCenter] addObserver: self
                                             selector: @selector (ticketListDidChange:)
                                                 name: TicketListDidChangeNotification
                                               object: nil];    

    [[NSNotificationCenter defaultCenter] addObserver: self
                                             selector: @selector (cacheSelectionDidChange:)
                                                 name: CacheSelectionDidChangeNotification
                                               object: nil];    
    
    [[NSNotificationCenter defaultCenter] addObserver: self
                                             selector: @selector (ticketSelectionDidChange:)
                                                 name: TicketSelectionDidChangeNotification
                                               object: nil];    
    
    [[NSNotificationCenter defaultCenter] addObserver: self
                                             selector: @selector (windowWillClose:)
                                                 name: NSWindowWillCloseNotification 
                                               object: nil];    
    
    [[[NSWorkspace sharedWorkspace] notificationCenter] addObserver: self
                                                           selector: @selector (wakeFromSleep:)
                                                               name: NSWorkspaceDidWakeNotification 
                                                             object: nil];    
    
    // disable items before ticket list opens to set up default state
    [self ticketListDidChange: NULL];  
    
    // Open ticket list, if needed (will send notification if opened)
    if (([[KerberosPreferences sharedPreferences] launchAction] == LaunchActionAlwaysOpenTicketWindow) ||
        (([[KerberosPreferences sharedPreferences] launchAction] == LaunchActionRememberOpenTicketWindow) &&
         ([[KerberosPreferences sharedPreferences] ticketWindowLastOpen]))) {
        [self showTicketList: self];
    }

    // Set up default cache state
    [self cacheCollectionDidChange: NULL];
}

// ---------------------------------------------------------------------------

- (NSMenu *) applicationDockMenu: (NSApplication *) sender
{
    return dockMenu;
}

// ---------------------------------------------------------------------------

- (void) menuNeedsUpdate: (NSMenu *) menu
{
    if (menu == ticketsMenu) {  
        dprintf ("Kerberos Controller tickets menu updating...");
        BOOL ticketListWindowHasSelectedCache = [self ticketListWindowHasSelectedCache];
        
        [renewTicketsMenuItem    setEnabled: ticketListWindowHasSelectedCache];
        [validateTicketsMenuItem setEnabled: [self ticketListWindowSelectedCacheNeedsValidation]];
        [destroyTicketsMenuItem  setEnabled: ticketListWindowHasSelectedCache];
        [changePasswordMenuItem  setEnabled: ticketListWindowHasSelectedCache];
        [showTicketInfoMenuItem  setEnabled: [self ticketListWindowHasSelectedCredential]];
        
    } else if (menu == activeUserMenu) {
        dprintf ("Kerberos Controller active user menu updating...");
        [Utilities synchronizeCacheMenu: menu 
                                  popup: NO
                 staticPrefixItemsCount: 0
                             headerItem: NO
                      checkDefaultCache: YES
                      defaultCacheIndex: NULL
                               selector: @selector (changeActiveUser:)
                                 sender: self];

    } else if (menu == dockMenu) {
        dprintf ("Kerberos Controller dock menu updating...");
        BOOL haveDefaultCache = [self haveDefaultCache];
            
        [dockRenewTicketsMenuItem    setEnabled: haveDefaultCache];
        [dockValidateTicketsMenuItem setEnabled: [self defaultCacheNeedsValidation]];
        [dockDestroyTicketsMenuItem  setEnabled: haveDefaultCache];
        [dockChangePasswordMenuItem  setEnabled: haveDefaultCache];            
        
        [Utilities synchronizeCacheMenu: menu 
                                  popup: NO
                 staticPrefixItemsCount: [dockMenu indexOfItem: dockSeparatorItem] + 1
                             headerItem: YES
                      checkDefaultCache: YES
                      defaultCacheIndex: NULL
                               selector: @selector (changeActiveUser:)
                                 sender: self];
    }
    dprintf ("Kerberos Controller done updating menu...");
}

#pragma mark --- Notifications --

// ---------------------------------------------------------------------------

- (void) applicationDidBecomeActive: (NSNotification *) notification
{
    static BOOL firstTime = YES; 
    
    // We would like to present the ticket list window whenever the application
    // comes to the front, except on app launch (because there's a preference for
    // that behavior).  So we let awakeFromNib do the work of looking at the 
    // preferenceand displaying the window on app launch.  However, applications 
    // get an activate event when launched, so we ignore the first call.
    
    if (!firstTime && (!ticketListController || ![[ticketListController window] isVisible])) {
        [self showTicketList: self];
    }
    
    firstTime = NO;
}

// ---------------------------------------------------------------------------

- (void) applicationWillTerminate: (NSNotification *) notification
{
    // Put the dock icon back
    NSImage *applicationIconImage = [NSImage imageNamed: @"NSApplicationIcon"];
    if (applicationIconImage) {
        [NSApp setApplicationIconImage: applicationIconImage];
    }

    // Remember the ticket window state on quit.
    [[KerberosPreferences sharedPreferences] setTicketWindowLastOpen: [self haveTicketListWindow]];
    
    [minuteTimer invalidate];
}

// ---------------------------------------------------------------------------

- (void) preferencesDidChange: (NSNotification *) notification
{
    dprintf ("Kerberos Controller got PreferencesDidChangeNotification");
    [self synchronizeDockIcon];
    [self menuNeedsUpdate: dockMenu];
    dprintf ("Kerberos Controller done with PreferencesDidChangeNotification");
}

// ---------------------------------------------------------------------------

- (void) cacheCollectionDidChange: (NSNotification *) notification
{
    dprintf ("Kerberos Controller got CacheCollectionDidChangeNotification");
    // Note: only put things here that do not depend on the ticket list.
    // If it depends on the ticket list, use ticketListDidChange: instead.
    [self synchronizeDockIcon];
    [self menuNeedsUpdate: dockMenu];
    [self menuNeedsUpdate: activeUserMenu];
    dprintf ("Kerberos Controller done with CacheCollectionDidChangeNotification");
}

// ---------------------------------------------------------------------------

- (void) ticketListDidChange: (NSNotification *) notification
{
    dprintf ("Kerberos Controller got TicketListDidChangeNotification");
    [self menuNeedsUpdate: ticketsMenu];
    dprintf ("Kerberos Controller done with TicketListDidChangeNotification");
}


// ---------------------------------------------------------------------------

- (void) cacheSelectionDidChange: (NSNotification *) notification
{
    if (ticketListController == [notification object]) {
        dprintf ("Kerberos Controller noticed KerberosCache List selection change...");
        [self menuNeedsUpdate: ticketsMenu];
    }
}

// ---------------------------------------------------------------------------

- (void) ticketSelectionDidChange: (NSNotification *) notification
{
    if (ticketListController == [notification object]) {
        dprintf ("Kerberos Controller noticed Ticket List selection change...");
        [self menuNeedsUpdate: ticketsMenu];
    }
}

// ---------------------------------------------------------------------------

- (void) windowWillClose: (NSNotification *) notification
{
    if ((ticketListController != NULL) && ([ticketListController window] == [notification object])) {
        // Okay, this is totally gross.  Since there isn't an NSWindowDidCloseNotification,
        // we have to do this by hand because the handy code in menuNeedsUpdate will call
        // isVisible on the window and it's still visible when this gets called.
        
        [renewTicketsMenuItem    setEnabled: NO];
        [validateTicketsMenuItem setEnabled: NO];
        [destroyTicketsMenuItem  setEnabled: NO];
        [changePasswordMenuItem  setEnabled: NO];
        [showTicketInfoMenuItem  setEnabled: NO];

        dprintf ("Kerberos Controller noticed Ticket List Window closing...");
    }
}

// ---------------------------------------------------------------------------

- (void) wakeFromSleep: (NSNotification *) notification
{
    dprintf("KerberosController waking from sleep...");
    // Since the computer was just asleep, credential renewal timers need 
    // to be adjusted to reflect the current time.
    // Note that we don't want to post the notification directly from here
    // because that would slow down the machine's wake from sleep process.
    // Just queue it up.
    NSNotification *credentialNotification = [NSNotification notificationWithName: KerberosCredentialTimersNeedResetNotification 
                                                                           object: self];
    if (credentialNotification) {
        [[NSNotificationQueue defaultQueue] enqueueNotification: credentialNotification
                                                   postingStyle: NSPostWhenIdle
                                                   coalesceMask: NSNotificationCoalescingOnName 
                                                       forModes: NULL];
    }
    dprintf ("KerberosController done waking from sleep.");
}

#pragma mark -- Callbacks --

// ---------------------------------------------------------------------------

- (void) minuteTimer: (NSTimer *) timer
{
    [self synchronizeDockIcon];
}

#pragma mark -- Ticket List State --

// ---------------------------------------------------------------------------

- (BOOL) haveTicketListWindow
{
    return ((ticketListController != NULL) && ([[ticketListController window] isVisible]));
}

// ---------------------------------------------------------------------------

- (BOOL) ticketListWindowHasSelectedCache
{
    return ([self haveTicketListWindow] && [ticketListController hasSelectedCache]);
}

// ---------------------------------------------------------------------------

- (BOOL) ticketListWindowSelectedCacheNeedsValidation
{
    return ([self haveTicketListWindow] && [ticketListController selectedCacheNeedsValidation]);
}

// ---------------------------------------------------------------------------

- (BOOL) ticketListWindowHasSelectedCredential
{
    return ([self haveTicketListWindow] && [ticketListController hasSelectedCredential]);
}

// ---------------------------------------------------------------------------

- (BOOL) haveDefaultCache
{
    return ([cacheCollection defaultCache] != NULL);
}

// ---------------------------------------------------------------------------

- (BOOL) defaultCacheNeedsValidation
{
    KerberosCache *defaultCache = [cacheCollection defaultCache];
    return ((defaultCache != NULL) && [defaultCache needsValidation]);
}

#pragma mark -- Dock Icon --

// ---------------------------------------------------------------------------

- (void) synchronizeDockIcon
{
    static BOOL sDockIconIsDynamic = NO;  // Use so we don't refresh icon when not dynamic
    KerberosCache *defaultCache = [cacheCollection defaultCache];
    
    if ([[KerberosPreferences sharedPreferences] showTimeInDockIcon]) {        
        NSImage *iconImage = [self dockIconImageForCache: defaultCache];
        if (iconImage) {
            [NSApp setApplicationIconImage: iconImage];
        }
        
        sDockIconIsDynamic = YES;
    } else {
        if (sDockIconIsDynamic) {
            NSImage *applicationIconImage = [NSImage imageNamed: @"NSApplicationIcon"];
            if (applicationIconImage) {
                [NSApp setApplicationIconImage: applicationIconImage];
                sDockIconIsDynamic = NO;  // remember so we don't keep doing this over and over
            }
        }
    }
}

// ---------------------------------------------------------------------------

- (NSImage *) ticketsKerberosIconImage
{
    if (!ticketsKerberosIconImage) {
        NSString *iconPathString = [[NSBundle mainBundle] pathForResource: @"DockHasTickets"
                                                                   ofType: @"tiff"];
        if (iconPathString) {
            ticketsKerberosIconImage = [[NSImage alloc] initWithContentsOfFile: iconPathString];
        }
    }
    return ticketsKerberosIconImage;
}

// ---------------------------------------------------------------------------

- (NSImage *) warningKerberosIconImage
{
    if (!warningKerberosIconImage) {
        NSString *iconPathString = [[NSBundle mainBundle] pathForResource: @"DockTicketsWarning"
                                                                   ofType: @"tiff"];
        if (iconPathString) {
            warningKerberosIconImage = [[NSImage alloc] initWithContentsOfFile: iconPathString];
        }
    }
    return warningKerberosIconImage;
}

// ---------------------------------------------------------------------------

- (NSImage *) noTicketsKerberosIconImage
{
    if (!noTicketsKerberosIconImage) {
        NSString *iconPathString = [[NSBundle mainBundle] pathForResource: @"DockNoTickets"
                                                                   ofType: @"tiff"];
        if (iconPathString) {
            noTicketsKerberosIconImage = [[NSImage alloc] initWithContentsOfFile: iconPathString];
        }
    }
    return noTicketsKerberosIconImage;
}

// ---------------------------------------------------------------------------

- (NSImage *) kerberosAppIconImage
{
    if (!kerberosAppIconImage) {
        NSString *iconPathString = [[NSBundle mainBundle] pathForResource: @"KerberosApp"
                                                                   ofType: @"icns"];
        if (iconPathString) {
            kerberosAppIconImage = [[NSImage alloc] initWithContentsOfFile: iconPathString];
        }
    }
    return kerberosAppIconImage;
}


// ---------------------------------------------------------------------------

- (NSImage *) dockIconImageForCache: (KerberosCache *) cache
{
    NSImage *newDockIconImage = NULL;
    NSImage  *baseImage = [self kerberosAppIconImage];
    NSString *string = NULL;
    
    if (cache) {
        if ([cache state] == CredentialValid) {
            string = [cache shortTimeRemainingString];
            
            if ([cache timeRemaining] > kFiveMinutes) {
                baseImage = [self ticketsKerberosIconImage];
            } else {
                baseImage = [self warningKerberosIconImage];
            }
        }
    }
    
    if (baseImage) {
        NSSize iconSize = [[self kerberosAppIconImage] size];
        newDockIconImage = [[[NSImage alloc] initWithSize: iconSize] autorelease];
        if (newDockIconImage) {
            [newDockIconImage lockFocus];
            [baseImage setScalesWhenResized: YES];
            [baseImage setSize: [newDockIconImage size]];
            [baseImage compositeToPoint: NSMakePoint (0.0, 0.0) operation: NSCompositeSourceOver];
            if (string) {
                static NSDictionary *dockAttributes = NULL;
                
                if (!dockAttributes) {
                    NSMutableParagraphStyle *style = [[[NSParagraphStyle defaultParagraphStyle] mutableCopy] autorelease];
                    [style setAlignment: NSLeftTextAlignment];
                    
                    dockAttributes = [[NSDictionary dictionaryWithObjectsAndKeys: 
                        [NSFont boldSystemFontOfSize: 22], NSFontAttributeName, 
                        style, NSParagraphStyleAttributeName, NULL] retain];
                }
                
                NSSize textSize = [string sizeWithAttributes: dockAttributes];
                
                // Add the time remaining string
                [string drawAtPoint: NSMakePoint (((iconSize.width - textSize.width) / 2) + 19, 
                                                  19.0 - (textSize.height / 2))
                     withAttributes: dockAttributes];
                
                [newDockIconImage unlockFocus];
            }
        }        
    }
    
    return newDockIconImage;
}

@end
