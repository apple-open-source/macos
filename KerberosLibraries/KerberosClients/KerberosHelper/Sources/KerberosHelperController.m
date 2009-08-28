/*
 * KerberosHelperController.m
 *
 * $Header$
 *
 * Copyright 2006 Massachusetts Institute of Technology.
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

/* 
 * KerberosHelper is an NSStatusItem version of Kerberos.app.
 * - auto-renews tickets in the background
 * - provides the same actions as Kerberos.app's dock menu
 * - doesn't take up any space in the dock
 */

#import "KerberosHelperController.h"
#import "KerberosErrorAlert.h"
#import "Utilities.h"
#import <Kerberos/Kerberos.h>
#import <IOKit/pwr_mgt/IOPMLib.h>
#import <IOKit/IOMessage.h>

#define kCacheCollectionUpdateTimerInterval 2
#define KerberosHelperModePreferenceKey CFSTR("KMShowMode")


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
                                                                         object: (KerberosHelperController *) inContext];
            if (notification) {
                [[NSNotificationQueue defaultQueue] enqueueNotification: notification
                                                           postingStyle: NSPostWhenIdle
                                                           coalesceMask: NSNotificationCoalescingOnName 
                                                               forModes: NULL];
            }
				dprintf ("Done waking from sleep.");
            break;
    }
    
}

@implementation KerberosHelperController

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
		
		context = NULL;
		lastChangeTime = 0;
        
        updateTimer = nil;
		minuteTimer = nil;
	}
    return self;
}

// ---------------------------------------------------------------------------

- (void) dealloc
{
    [[NSNotificationCenter defaultCenter] removeObserver: self];
    
    if (cacheCollection           ) { [cacheCollection release]; }
    if (updateTimer               ) { [updateTimer release]; }
	if (minuteTimer               ) { [minuteTimer release]; }
    
	if (context) { cc_context_release (context); }
	
    [super dealloc];
}

#pragma mark --- Actions --

// ---------------------------------------------------------------------------

- (IBAction) menuBarStatusItemModeDidChange: (id) sender
{
	[self setModePreference:[statusModesMenu indexOfItem:sender]];
	
	NSArray *modeItems = [statusModesMenu itemArray];
	unsigned i;
	for (i = 0; i < [modeItems count]; i++) {
		[[modeItems objectAtIndex: i] setState: NSOffState];
	}
	[sender setState: NSOnState];
	[self updateStatusItem];
}

// ---------------------------------------------------------------------------

- (IBAction) openKerberosApplication: (id) sender
{
	[[NSWorkspace sharedWorkspace] launchAppWithBundleIdentifier: @"edu.mit.Kerberos.KerberosApp" options: NSWorkspaceLaunchDefault additionalEventParamDescriptor: nil launchIdentifier: nil];
}

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
        
		if (menu == statusMenu) {
            offset += [statusMenu indexOfItem: statusSeparatorItem] + 2; // include header item
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

#pragma mark --- Delegate Methods --

// ---------------------------------------------------------------------------

- (void) awakeFromNib
{
	IONotificationPortRef notify;
    io_object_t iterator;
	
    dprintf ("Entering awakeFromNib...");
	
    // Make sure we are always prompted with the dialog even if we have a controlling terminal
    __KLSetPromptMechanism (klPromptMechanism_GUI);
    
	[statusMenu setAutoenablesItems: YES];	
	[self addMenuBarStatusItem];
	
	// Load preferences
	int index = [self modePreference];
	
	if ((index < 0) || (index >= [statusModesMenu numberOfItems])) {
		NSLog(@"Invalid menu mode preference (%d), defaulting to \"Icon Only\"", index);
		index = 0;
	}
	[self menuBarStatusItemModeDidChange:[statusModesMenu itemAtIndex:index]];
	
    // Set up timers
	/*
	updateTimer = [[NSTimer scheduledTimerWithTimeInterval: 1  // keeps the default user reflected in the status item
												 target: self 
											   selector: @selector(updateStatusItemIfNeeded) 
											   userInfo: NULL
												repeats: YES] retain];
	*/
	minuteTimer = [[NSTimer scheduledTimerWithTimeInterval: 30 // controls the minute counter in the status item
													target: self 
												  selector: @selector(updateStatusItem) 
												  userInfo: NULL 
												   repeats: YES] retain];
	
	[[NSNotificationCenter defaultCenter] addObserver: self
                                             selector: @selector (cacheCollectionDidChange:)
                                                 name: CacheCollectionDidChangeNotification
                                               object: nil];  
	
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

#pragma mark --- Notifications --

// ---------------------------------------------------------------------------

- (void) applicationWillTerminate: (NSNotification *) notification
{
    [updateTimer invalidate];
	[minuteTimer invalidate];
}

// ---------------------------------------------------------------------------

- (void) cacheCollectionDidChange: (NSNotification *) notification
{
	[self updateStatusItem];
}

#pragma mark -- Callbacks --

// ---------------------------------------------------------------------------

- (void) updateStatusItemIfNeeded
{
	cc_int32 err = ccNoError;
	cc_time_t new_changed_time = 0;
	cc_ccache_t default_ccache = NULL;
	
	if (!context) {
		err = cc_initialize(&context, ccapi_version_4, NULL, NULL);
	}
	
	if (!err) {
		err = cc_context_get_change_time(context, &new_changed_time);
	}
	
	if (!err) {
		if (new_changed_time > lastChangeTime) {
			[cacheCollection update];
			[minuteTimer fire];
			
			// re-checking change time because updating cacheCollection causes the last change time to increment
			err = cc_context_get_change_time(context, &new_changed_time);
			lastChangeTime = new_changed_time;
		}		
	}
}

// ---------------------------------------------------------------------------

- (void) updateStatusItem
{
	KerberosCache *defaultCache = [cacheCollection defaultCache];
	// Message is sent with a delay for a cosmetic reason. Changing the title before the menu highlight disappears causes an ugly double flash of the NSStatusItem. Use the commented line below instead if delayed calls end up causing problems.
	[menuBarStatusItem performSelector:@selector(setTitle:) withObject:[self menuBarStatusItemTitleForCache: defaultCache] afterDelay:0];
	//[menuBarStatusItem setTitle:[self menuBarStatusItemTitleForCache: defaultCache]];
}

#pragma mark -- Ticket List State --

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

#pragma mark -- Menu Bar StatusItem --

// ---------------------------------------------------------------------------

- (void) addMenuBarStatusItem
{
	menuBarStatusItem = [[NSStatusBar systemStatusBar] statusItemWithLength:NSVariableStatusItemLength];
	[menuBarStatusItem retain];
	
	[menuBarStatusItem setTitle:@""];
	[menuBarStatusItem setHighlightMode:YES];
	NSImage *anImage = [[NSImage imageNamed:@"Kerberos"] copy];
	[anImage setScalesWhenResized:YES];
	[anImage setSize:NSMakeSize(22, 22)];
	[menuBarStatusItem setImage:[anImage autorelease]];
	
	[menuBarStatusItem setMenu: statusMenu];
}

// ---------------------------------------------------------------------------

- (NSString *) menuBarStatusItemTitleForCache: (KerberosCache *) cache
{
    NSString *string = NULL;
    NSString *modeString = [self menuBarStatusItemModeString];
		
    if (cache) {
        if ([cache state] == CredentialValid) {
			if ([modeString isEqualToString:@"Full Principal"]) {
				string = [NSString stringWithFormat:@"%@", [cache principalString]];
			} else if ([modeString isEqualToString:@"Name"]) {
				string = [NSString stringWithFormat:@"%@", [[[cache principal] componentsArray] componentsJoinedByString:@"/"]];
			} else if ([modeString isEqualToString:@"Realm"]) {
				string = [NSString stringWithFormat:@"%@", [[cache principal] realmString]];
			} else if ([modeString isEqualToString:@"Time"]) {
				string = [NSString stringWithFormat:@"(%@)", [cache shortTimeRemainingString]];
			}
        }
    }
    
    return string;
}

// ---------------------------------------------------------------------------

- (NSString *) menuBarStatusItemModeString
{
	NSString *string = nil;
	
	NSArray *items = [statusModesMenu itemArray];
	NSMenuItem *anItem = nil;
	unsigned i = 0;
	do {
		anItem = [items objectAtIndex:i++];
	} while (([anItem state] != NSOnState) && (i < [items count]));
	
	if ([anItem state] == NSOnState) {
		string = [anItem title];
	}
	
	// it's an error if no mode is set, so we default to Off
	
	return string;
}

// ---------------------------------------------------------------------------

- (BOOL)validateMenuItem:(NSMenuItem *)item
{
	BOOL haveDefaultCache = [self haveDefaultCache];
	
	if		([item action] == @selector(menuBarStatusItemModeDidChange:))	{ return YES; }
	else if ([item action] == @selector(getTickets:))						{ return YES; }
	else if ([item action] == @selector(destroyTicketsForActiveUser:))		{ return haveDefaultCache; }
	else if ([item action] == @selector(renewTicketsForActiveUser:))		{ return haveDefaultCache; }
	else if ([item action] == @selector(validateTicketsForActiveUser:))		{ return [self defaultCacheNeedsValidation]; }
	else if ([item action] == @selector(changePasswordForActiveUser:))		{ return haveDefaultCache; }
	// Since there's no apparent protocol for validation of the entire menu, use something innocuous and unique, the Open Kerb.app item, to act as a hint that the list of available tickets needs updating
	else if (item == statusOpenKerberosAppMenuItem) {
		
		// Remove and then add back the "Open Kerberos Application..." menu item and its preceding separator to avoid having to alter Utilities' -synchronizeCacheMenu:::::::: and to keep as much of the UI in the nib as possible (as opposed to creating the "Open Kerberos Application..." item in code everytime the menu is updated
		
		[statusOpenKerberosAppMenuItem retain];
		[statusMenu removeItemAtIndex: [statusMenu numberOfItems] - 1];
		[statusMenu removeItemAtIndex: [statusMenu numberOfItems] - 1];
		
		[cacheCollection update];
		[Utilities synchronizeCacheMenu: statusMenu 
							controlType: kUtilitiesStatusItemControlType
				 staticPrefixItemsCount: [statusMenu indexOfItem: statusSeparatorItem] + 1
							 headerItem: YES
					  checkDefaultCache: YES
					  defaultCacheIndex: NULL
							   selector: @selector (changeActiveUser:)
								 sender: self];
								
		[statusMenu addItem: [NSMenuItem separatorItem]];
		[statusMenu addItem: statusOpenKerberosAppMenuItem];
		[statusOpenKerberosAppMenuItem release];
		
		return YES;
	}
	
	return NO;
}

#pragma mark -- Preferences --

// ---------------------------------------------------------------------------

- (int) modePreference
{
	CFPropertyListRef aValue = CFPreferencesCopyAppValue(KerberosHelperModePreferenceKey, kCFPreferencesCurrentApplication);
	int value = 0;
	if ((aValue != NULL) && (CFGetTypeID (aValue) == CFNumberGetTypeID ())) {
		CFNumberGetValue(aValue, kCFNumberSInt32Type, &value);
		CFRelease(aValue);
	}
	return value;
}

// ---------------------------------------------------------------------------

- (void) setModePreference: (int) value
{
	CFNumberRef aValue = CFNumberCreate (kCFAllocatorDefault, kCFNumberSInt32Type, &value);
	CFPreferencesSetAppValue(KerberosHelperModePreferenceKey, aValue, kCFPreferencesCurrentApplication);
	CFPreferencesAppSynchronize (kCFPreferencesCurrentApplication);
	CFRelease(aValue);
}

@end
