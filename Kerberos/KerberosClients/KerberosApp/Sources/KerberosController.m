/*
 * KerberosController.m
 *
 * $Header: /cvs/kfm/KerberosClients/KerberosApp/Sources/KerberosController.m,v 1.22 2005/02/01 15:33:20 lxs Exp $
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
#import "ErrorAlert.h"
#import "Utilities.h"
#import <IOKit/pwr_mgt/IOPMLib.h>
#import <IOKit/IOMessage.h>
#import <Kerberos/KerberosLoginPrivate.h>


static io_connect_t gRootPort = MACH_PORT_NULL;


// ---------------------------------------------------------------------------

static void HandlePowerManagerEvent (void *inContext,
                                     io_service_t inIOService,
                                     natural_t inMessageType,
                                     void *inMessageArgument)
{
    switch (inMessageType) {
        case kIOMessageSystemWillSleep:
            IOAllowPowerChange (gRootPort, (long) inMessageArgument);
            dprintf("Going to sleep...");
            break;
        case kIOMessageCanSystemSleep:
            IOAllowPowerChange (gRootPort, (long) inMessageArgument);
            dprintf("Allowing system to go to sleep...");
            break;
        case kIOMessageSystemHasPoweredOn:
            // Post notification to things with timers which fire on a particular date
            // so that they can be reset to fire now if the fire time already passed
            dprintf("Waking from sleep...");
            // Note that we cannot post the notification directly from this callback
            // so insert it into the notification queue instead
            NSNotification *notification = [NSNotification notificationWithName: WakeFromSleepNotification 
                                                                         object: (KerberosController *) inContext];
            if (notification != NULL) {
                [[NSNotificationQueue defaultQueue] enqueueNotification: notification
                                                           postingStyle: NSPostWhenIdle
                                                           coalesceMask: NSNotificationCoalescingOnName 
                                                               forModes: NULL];
            }
            dprintf ("Done waking from sleep.");
            break;
    }
    
}

@implementation KerberosController

#pragma mark -
    
// ---------------------------------------------------------------------------

- (id) init
{
    if ((self = [super init])) {
        cacheCollection = [[CacheCollection sharedCacheCollection] retain];
        if (cacheCollection == NULL) {
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
    
    if (cacheCollection            != NULL) { [cacheCollection release]; }
    if (minuteTimer                != NULL) { [minuteTimer release]; }
    if (preferencesController      != NULL) { [preferencesController release]; }
    if (ticketListController       != NULL) { [ticketListController release]; }
    if (ticketsKerberosIconImage   != NULL) { [ticketsKerberosIconImage release]; }
    if (warningKerberosIconImage   != NULL) { [warningKerberosIconImage release]; }
    if (noTicketsKerberosIconImage != NULL) { [noTicketsKerberosIconImage release]; }
    if (kerberosAppIconImage       != NULL) { [kerberosAppIconImage release]; }
    
    [super dealloc];
}

#pragma mark --- Actions --

// ---------------------------------------------------------------------------

- (IBAction) getTickets: (id) sender
{
    KLStatus err = [Principal getTickets];
    if (err == klNoErr) {
        [cacheCollection update];
    } else if (err != klUserCanceledErr) {
        [ErrorAlert alertForError: err
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
    Cache *cache = [cacheCollection defaultCache];
    if (cache != NULL) {
        KLStatus err = [[cache principal] changePassword];
        if (err == klNoErr) {
            [cacheCollection update];
        } else if (err != klUserCanceledErr) {
            [ErrorAlert alertForError: err
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
    Cache *cache = [cacheCollection defaultCache];
    if (cache != NULL) {
        KLStatus err = [[cache principal] destroyTickets];
        if (err == klNoErr) {
            [cacheCollection update];
        } else if (err != klUserCanceledErr) {
            [ErrorAlert alertForError: err
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
    Cache *cache = [cacheCollection defaultCache];
    if (cache != NULL) {
        KLStatus err = [[cache principal] renewTickets];
        if (err == klNoErr) {
            [cacheCollection update];
        } else if (err != klUserCanceledErr) {
            [ErrorAlert alertForError: err
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
    Cache *cache = [cacheCollection defaultCache];
    if (cache != NULL) {
        KLStatus err = [[cache principal] validateTickets];
        if (err == klNoErr) {
            [cacheCollection update];
        } else if (err != klUserCanceledErr) {
            [ErrorAlert alertForError: err
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
            Cache *cache = [cacheCollection cacheAtIndex: ([menu indexOfItem: item] - offset)];
            if (![cache isDefault]) {
                KLStatus err = [[cache principal] setDefault];
                if (err == klNoErr) {
                    [cacheCollection update];
                } else if (err != klUserCanceledErr) {
                    [ErrorAlert alertForError: err
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
    if (preferencesController == NULL) {
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
    if (ticketListController == NULL) {
        ticketListController = [[TicketListController alloc] init];
    }
    if (ticketListController != NULL) {
        [ticketListController showWindow: self];
        [self menuNeedsUpdate: ticketsMenu];
    }
}

// ---------------------------------------------------------------------------

- (IBAction) editRealms: (id) sender
{
    if (realmsEditorController == NULL) {
        realmsEditorController = [[RealmsEditorController alloc] init];
    }
    [realmsEditorController showWindow: self];
}


#pragma mark --- Delegate Methods --

// ---------------------------------------------------------------------------

- (void) awakeFromNib
{
    IONotificationPortRef notify;
    io_object_t iterator;

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
                                             selector: @selector (cacheCollectionDidChange:)
                                                 name: CacheCollectionDidChangeNotification
                                               object: nil];    
    
    [[NSNotificationCenter defaultCenter] addObserver: self
                                             selector: @selector (ticketListDidChange:)
                                                 name: TicketListDidChangeNotification
                                               object: nil];    

    [[NSNotificationCenter defaultCenter] addObserver: self
                                         selector: @selector (listSelectionDidChange:)
                                             name: CacheSelectionDidChangeNotification
                                           object: nil];    

    [[NSNotificationCenter defaultCenter] addObserver: self
                                             selector: @selector (windowWillClose:)
                                                 name: NSWindowWillCloseNotification 
                                               object: nil];    
    
    // disable items before ticket list opens to set up default state
    [self ticketListDidChange: NULL];  
    
    // Open ticket list, if needed (will send notification if opened)
    if (([[Preferences sharedPreferences] launchAction] == LaunchActionAlwaysOpenTicketWindow) ||
        (([[Preferences sharedPreferences] launchAction] == LaunchActionRememberOpenTicketWindow) &&
         ([[Preferences sharedPreferences] ticketWindowLastOpen]))) {
        [self showTicketList: self];
    }

    // Register for wake/sleep power events so we can reset our credential timers
    gRootPort = IORegisterForSystemPower (self, &notify, HandlePowerManagerEvent, &iterator);
    if (gRootPort == MACH_PORT_NULL) {
        NSLog (@"IORegisterForSystemPower failed.");
    } else {
        CFRunLoopAddSource (CFRunLoopGetCurrent (),
                            IONotificationPortGetRunLoopSource (notify),
                            kCFRunLoopDefaultMode);
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
                               fontSize: 0 // default size
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
                               fontSize: 0 // default size
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
    
    if (!firstTime && ((ticketListController == NULL) || ![[ticketListController window] isVisible])) {
        [self showTicketList: self];
    }
    
    firstTime = NO;
}

// ---------------------------------------------------------------------------

- (void) applicationWillTerminate: (NSNotification *) notification
{
    // Put the dock icon back
    NSImage *applicationIconImage = [NSImage imageNamed: @"NSApplicationIcon"];
    if (applicationIconImage != NULL) {
        [NSApp setApplicationIconImage: applicationIconImage];
    }

    // Remember the ticket window state on quit.
    [[Preferences sharedPreferences] setTicketWindowLastOpen: [self haveTicketListWindow]];
    
    [minuteTimer invalidate];
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

- (void) listSelectionDidChange: (NSNotification *) notification
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
    return ([self haveTicketListWindow] && [ticketListController haveSelectedCache]);
}

// ---------------------------------------------------------------------------

- (BOOL) ticketListWindowSelectedCacheNeedsValidation
{
    return ([self haveTicketListWindow] && [ticketListController selectedCacheNeedsValidation]);
}

// ---------------------------------------------------------------------------

- (BOOL) ticketListWindowHasSelectedCredential
{
    return ([self haveTicketListWindow] && [ticketListController haveSelectedCredential]);
}

// ---------------------------------------------------------------------------

- (BOOL) haveDefaultCache
{
    return ([cacheCollection defaultCache] != NULL);
}

// ---------------------------------------------------------------------------

- (BOOL) defaultCacheNeedsValidation
{
    Cache *defaultCache = [cacheCollection defaultCache];
    return ((defaultCache != NULL) && [defaultCache needsValidation]);
}

#pragma mark -- Dock Icon --

// ---------------------------------------------------------------------------

- (void) synchronizeDockIcon
{
    static BOOL sDockIconIsDynamic = NO;  // Use so we don't refresh icon when not dynamic
    Cache *defaultCache = [cacheCollection defaultCache];
    
    if ([[Preferences sharedPreferences] showTimeInDockIcon]) {        
        NSImage *iconImage = [self dockIconImageForCache: defaultCache];
        if (iconImage != NULL) {
            [NSApp setApplicationIconImage: iconImage];
        }
        
        sDockIconIsDynamic = YES;
    } else {
        if (sDockIconIsDynamic) {
            NSImage *applicationIconImage = [NSImage imageNamed: @"NSApplicationIcon"];
            if (applicationIconImage != NULL) {
                [NSApp setApplicationIconImage: applicationIconImage];
                sDockIconIsDynamic = NO;  // remember so we don't keep doing this over and over
            }
        }
    }
}

// ---------------------------------------------------------------------------

- (NSImage *) ticketsKerberosIconImage
{
    if (ticketsKerberosIconImage == NULL) {
        NSString *iconPathString = [[NSBundle mainBundle] pathForResource: @"DockHasTickets"
                                                                   ofType: @"tiff"];
        if (iconPathString != NULL) {
            ticketsKerberosIconImage = [[NSImage alloc] initWithContentsOfFile: iconPathString];
        }
    }
    return ticketsKerberosIconImage;
}

// ---------------------------------------------------------------------------

- (NSImage *) warningKerberosIconImage
{
    if (warningKerberosIconImage == NULL) {
        NSString *iconPathString = [[NSBundle mainBundle] pathForResource: @"DockTicketsWarning"
                                                                   ofType: @"tiff"];
        if (iconPathString != NULL) {
            warningKerberosIconImage = [[NSImage alloc] initWithContentsOfFile: iconPathString];
        }
    }
    return warningKerberosIconImage;
}

// ---------------------------------------------------------------------------

- (NSImage *) noTicketsKerberosIconImage
{
    if (noTicketsKerberosIconImage == NULL) {
        NSString *iconPathString = [[NSBundle mainBundle] pathForResource: @"DockNoTickets"
                                                                   ofType: @"tiff"];
        if (iconPathString != NULL) {
            noTicketsKerberosIconImage = [[NSImage alloc] initWithContentsOfFile: iconPathString];
        }
    }
    return noTicketsKerberosIconImage;
}

// ---------------------------------------------------------------------------

- (NSImage *) kerberosAppIconImage
{
    if (kerberosAppIconImage == NULL) {
        NSString *iconPathString = [[NSBundle mainBundle] pathForResource: @"KerberosApp"
                                                                   ofType: @"icns"];
        if (iconPathString != NULL) {
            kerberosAppIconImage = [[NSImage alloc] initWithContentsOfFile: iconPathString];
        }
    }
    return kerberosAppIconImage;
}


// ---------------------------------------------------------------------------

- (NSImage *) dockIconImageForCache: (Cache *) cache
{
    NSImage *newDockIconImage = NULL;
    NSImage  *baseImage = [self kerberosAppIconImage];
    NSString *string = NULL; // NSLocalizedStringFromTable (@"LifetimeStringExpired", @"LifetimeFormatter", NULL);
    
    if (cache != NULL) {
        time_t now = time (NULL);
        
        if ([cache stateAtTime: now] == CredentialValid) {
            string = [cache stringValueForDockIcon];
            
            if ([cache timeRemainingAtTime: now] > kFiveMinutes) {
                baseImage = [self ticketsKerberosIconImage];
            } else {
                baseImage = [self warningKerberosIconImage];
            }
        }
    }
    
    if (baseImage != NULL) {
        NSSize iconSize = [[self kerberosAppIconImage] size];
        newDockIconImage = [[[NSImage alloc] initWithSize: iconSize] autorelease];
        if (newDockIconImage != NULL) {
            [newDockIconImage lockFocus];
            [baseImage setScalesWhenResized: YES];
            [baseImage setSize: [newDockIconImage size]];
            [baseImage compositeToPoint: NSMakePoint (0.0, 0.0) operation: NSCompositeSourceOver];
            if (string != NULL) {
                NSDictionary *attributes = [Utilities attributesForDockIcon];
                NSSize textSize = [string sizeWithAttributes: attributes];
                
                // Add the time remaining string
                [string drawAtPoint: NSMakePoint (((iconSize.width - textSize.width) / 2) + 19, 
                                                  19.0 - (textSize.height / 2))
                     withAttributes: attributes];
                
                [newDockIconImage unlockFocus];
            }
        }        
    }
    
    return newDockIconImage;
}

@end
