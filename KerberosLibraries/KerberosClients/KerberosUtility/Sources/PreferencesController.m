/*
 * PreferencesController.m
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

#import "PreferencesController.h"
#import "KerberosLifetimeSlider.h"
#import "KerberosLifetimeFormatter.h"
#import "Utilities.h"

@implementation PreferencesController

// ---------------------------------------------------------------------------

- (id) init
{
    if ((self = [super initWithWindowNibName: @"Preferences"])) {
        preferences = [KerberosPreferences sharedPreferences];
        if (!preferences) {
            [self release];
            return NULL;
        }
        [preferences retain];
    }
    return self;
}

// ---------------------------------------------------------------------------

- (void) dealloc
{
    dprintf ("PreferencesController deallocating ...");
    [preferences release];
    [super dealloc];
}


- (void) windowDidLoad
{
    dprintf ("PreferencesController %lx entering windowDidLoad:...", (long) self);

    [super windowDidLoad];
    
    [self setWindowFrameAutosaveName: @"KAPreferencesWindowPosition"];
}

// ---------------------------------------------------------------------------

- (IBAction) showWindow: (id) sender
{
    // Check to see if the window was closed before. ([self window] will load the window)
    if (![[self window] isVisible]) {
        dprintf ("PreferencesController %lx displaying window...", (long) self);
        
        [self preferencesToWindow];
        [[self window] setFrameUsingName: [self windowFrameAutosaveName]];
    }
    
    [super showWindow: sender];
}

// ---------------------------------------------------------------------------

- (IBAction) apply: (id) sender
{
    [self windowToPreferences];
}

// ---------------------------------------------------------------------------

- (IBAction) ok: (id) sender
{
    if ([self windowToPreferences]) {
        // Only close on no error
        [self close];
    }
}

// ---------------------------------------------------------------------------

- (IBAction) cancel: (id) sender
{
    [self close];
}


// ---------------------------------------------------------------------------

- (IBAction) defaultPrincipalRadioButtonWasHit: (id) sender
{
    NSButtonCell *selectedCell = [defaultPrincipalRadioButtonMatrix selectedCell];
    BOOL enabled = (selectedCell == defaultToThisPrincipalRadioButtonCell);
    NSColor *enabledColor = enabled ? [NSColor blackColor] : [NSColor grayColor];
        
    [nameHeaderTextField setTextColor: enabledColor];
    [realmHeaderTextField setTextColor: enabledColor];
    
    [nameTextField setEnabled: enabled];
    [realmComboBox setEnabled: enabled];
}

// ---------------------------------------------------------------------------

- (IBAction) defaultTicketOptionsRadioButtonWasHit: (id) sender
{
    NSButtonCell *selectedCell = [defaultOptionsRadioButtonMatrix selectedCell];
    BOOL enabled = (selectedCell == defaultToTheseOptionsRadioButtonCell);
    NSColor *enabledColor = enabled ? [NSColor blackColor] : [NSColor grayColor];
    
    [lifetimeHeaderTextField setTextColor: enabledColor];
    if ([lifetimeSlider maxValue] > [lifetimeSlider minValue]) {
        [lifetimeTextField setTextColor: enabledColor];
        [lifetimeSlider    setEnabled: enabled];    
    }
    
    [optionsHeaderTextField setTextColor: enabledColor];
    [forwardableCheckbox setEnabled: enabled];
    [addresslessCheckbox setEnabled: enabled];    
    [renewableCheckbox   setEnabled: enabled];    
    
    if ([renewableSlider maxValue] > [renewableSlider minValue]) {
        [renewableTextField  setTextColor: enabledColor];
        [renewableSlider     setEnabled: enabled];
    }
}

// ---------------------------------------------------------------------------

- (BOOL) tabView: (NSTabView *) tabView shouldSelectTabViewItem: (NSTabViewItem *) tabViewItem
{
    BOOL shouldSelect = YES;
    
    if (tabView == preferencesTabView) {
        if ([tabView selectedTabViewItem] == timeRangesTabViewItem) {
            // we are switching away from the time ranges tab
            time_t lifetimeMaximum = [self lifetimeMaximum];
            time_t lifetimeMinimum = [self lifetimeMinimum];
            time_t renewableMaximum = [self renewableMaximum];
            time_t renewableMinimum = [self renewableMinimum];
            
            if ([self validateLifetimeMaximum: lifetimeMaximum
                              lifetimeMinimum: lifetimeMinimum
                             renewableMaximum: renewableMaximum
                             renewableMinimum: renewableMinimum]) {
                // Ranges are acceptable.  Update the sliders and proceed
                time_t lifetime = [[lifetimeTextField formatter] lifetimeForControl: lifetimeSlider];
                time_t renewableLifetime = [[renewableTextField formatter] lifetimeForControl: renewableSlider];
                
                SetupLifetimeSlider (lifetimeSlider, lifetimeTextField,
                                     lifetimeMaximum, lifetimeMinimum, lifetime);
                
                SetupLifetimeSlider (renewableSlider, renewableTextField,
                                     renewableMaximum, renewableMinimum, renewableLifetime);

                [self defaultPrincipalRadioButtonWasHit: self];  // set enabledness of controls
                [self defaultTicketOptionsRadioButtonWasHit: self];  // set enabledness of controls
            } else {
                shouldSelect = NO;
            }
        }
    }
    
    return shouldSelect;
}

// ---------------------------------------------------------------------------

- (void) errorSheetDidEnd: (NSAlert *) alert returnCode: (int) returnCode contextInfo: (void *) contextInfo 
{
    // Do nothing.
}

// ---------------------------------------------------------------------------

- (BOOL) preferencesToWindow
{
    // Behavior
    
    [autoRenewCheckbox                    setState:  [preferences autoRenewTickets]            ? NSOnState : NSOffState];
    [showTimeRemainingInDockCheckbox      setState:  [preferences showTimeInDockIcon]          ? NSOnState : NSOffState];
    [rememberTicketWindowPositionCheckbox setState: ![preferences ticketWindowDefaultPosition] ? NSOnState : NSOffState];
    
    switch ([preferences launchAction]) {
        case LaunchActionAlwaysOpenTicketWindow:
            [launchActionMatrix selectCell: alwaysOpenListWindowRadioButtonCell];
            break;
            
        case LaunchActionNeverOpenTicketWindow:
            [launchActionMatrix selectCell: neverOpenListWindowRadioButtonCell];
            break;
            
        case LaunchActionRememberOpenTicketWindow:
            [launchActionMatrix selectCell: rememberTicketListWindowOpenessRadioButtonCell];
            break;
    }
    
    // Default KerberosPrincipal
    
    if ([preferences rememberPrincipalFromLastLogin]) {
        [defaultPrincipalRadioButtonMatrix selectCell: rememberPrincipalRadioButtonCell];
    } else {
        [defaultPrincipalRadioButtonMatrix selectCell: defaultToThisPrincipalRadioButtonCell];
    }
    
    [nameTextField setObjectValue: [preferences defaultName]];
    
    NSArray *realmsArray = [preferences realms];
    unsigned int i;
    [realmComboBox removeAllItems];
    for (i = 0; i < [realmsArray count]; i++) {
        NSString *realm = [realmsArray objectAtIndex: i];
        [realmComboBox addItemWithObjectValue: realm];
    }
    [realmComboBox selectItemWithObjectValue: [preferences defaultRealm]];
    [realmComboBox setObjectValue: [realmComboBox numberOfItems] > 0 ? [realmComboBox objectValueOfSelectedItem] : @""];
    [realmComboBox setNumberOfVisibleItems: [realmsArray count]];
    [realmComboBox setCompletes: YES];
    
    
    // Default Ticket Options
    
    if ([preferences rememberOptionsFromLastLogin]) {
        [defaultOptionsRadioButtonMatrix selectCell: rememberOptionsRadioButtonCell];
    } else {
        [defaultOptionsRadioButtonMatrix selectCell: defaultToTheseOptionsRadioButtonCell];
    }
    
    [forwardableCheckbox setState: [preferences defaultForwardable] ? NSOnState : NSOffState];
    [addresslessCheckbox setState: [preferences defaultAddressless] ? NSOnState : NSOffState];
    [renewableCheckbox   setState: [preferences defaultRenewable]   ? NSOnState : NSOffState];
        
    // Time Ranges
    time_t lifetimeMaximum = [preferences lifetimeMaximum];
    time_t lifetimeMinimum = [preferences lifetimeMinimum];
    time_t renewableMaximum = [preferences renewableLifetimeMaximum];
    time_t renewableMinimum = [preferences renewableLifetimeMinimum];
    
    SetupLifetimeSlider (lifetimeSlider, lifetimeTextField,
                         lifetimeMinimum, lifetimeMaximum, [preferences defaultLifetime]);
    
    SetupLifetimeSlider (renewableSlider, renewableTextField,
                         renewableMinimum, renewableMaximum , [preferences defaultRenewableLifetime]);
    
    [lifetimeMaximumDaysTextField    setObjectValue: [NSNumber numberWithInt: DAYS (lifetimeMaximum)]];
    [lifetimeMaximumHoursTextField   setObjectValue: [NSNumber numberWithInt: HOURS (lifetimeMaximum)]];
    [lifetimeMaximumMinutesTextField setObjectValue: [NSNumber numberWithInt: ROUNDEDMINUTES (lifetimeMaximum)]];
    
    [lifetimeMinimumDaysTextField    setObjectValue: [NSNumber numberWithInt: DAYS (lifetimeMinimum)]];
    [lifetimeMinimumHoursTextField   setObjectValue: [NSNumber numberWithInt: HOURS (lifetimeMinimum)]];
    [lifetimeMinimumMinutesTextField setObjectValue: [NSNumber numberWithInt: ROUNDEDMINUTES (lifetimeMinimum)]];
    
    [renewableMaximumDaysTextField    setObjectValue: [NSNumber numberWithInt: DAYS (renewableMaximum)]];
    [renewableMaximumHoursTextField   setObjectValue: [NSNumber numberWithInt: HOURS (renewableMaximum)]];
    [renewableMaximumMinutesTextField setObjectValue: [NSNumber numberWithInt: ROUNDEDMINUTES (renewableMaximum)]];
    
    [renewableMinimumDaysTextField    setObjectValue: [NSNumber numberWithInt: DAYS (renewableMinimum)]];
    [renewableMinimumHoursTextField   setObjectValue: [NSNumber numberWithInt: HOURS (renewableMinimum)]];
    [renewableMinimumMinutesTextField setObjectValue: [NSNumber numberWithInt: ROUNDEDMINUTES (renewableMinimum)]]; 
    
    [preferencesTabView selectFirstTabViewItem: self];
    
    [self defaultPrincipalRadioButtonWasHit: self];  // set enabledness of controls
    [self defaultTicketOptionsRadioButtonWasHit: self];  // set enabledness of controls

    return YES;  // For now, always succeed because we don't get errors from KerberosPreferences yet
}

// ---------------------------------------------------------------------------

- (BOOL) windowToPreferences
{
    NSButtonCell *selectedCell;
    
    // Time Ranges
    time_t lifetimeMaximum = [self lifetimeMaximum];
    time_t lifetimeMinimum = [self lifetimeMinimum];
    time_t renewableMaximum = [self renewableMaximum];
    time_t renewableMinimum = [self renewableMinimum];
    
    if (![self validateLifetimeMaximum: lifetimeMaximum
                       lifetimeMinimum: lifetimeMinimum
                      renewableMaximum: renewableMaximum
                      renewableMinimum: renewableMinimum]) {
        // note that the above function reports the error for us.
        return NO;
    }
    
    [preferences setLifetimeMaximum: lifetimeMaximum];
    [preferences setLifetimeMinimum: lifetimeMinimum];
    [preferences setRenewableLifetimeMaximum: renewableMaximum];
    [preferences setRenewableLifetimeMinimum: renewableMinimum];
    
    // Default Ticket Options
    
    selectedCell = [defaultOptionsRadioButtonMatrix selectedCell];
    if (selectedCell == rememberOptionsRadioButtonCell) {
        [preferences setRememberOptionsFromLastLogin: YES];
    } else if (selectedCell == defaultToTheseOptionsRadioButtonCell) {
        [preferences setRememberOptionsFromLastLogin: NO];
        
        [preferences setDefaultForwardable: ([forwardableCheckbox state] == NSOnState)];
        [preferences setDefaultAddressless: ([addresslessCheckbox state] == NSOnState)];
        [preferences setDefaultRenewable:   ([renewableCheckbox   state] == NSOnState)];
        
        KerberosLifetimeFormatter *lifetimeFormatter =  [lifetimeTextField formatter];
        KerberosLifetimeFormatter *renewableFormatter =  [renewableTextField formatter];
        
        [preferences setDefaultLifetime: [lifetimeFormatter lifetimeForControl: lifetimeSlider]];
        [preferences setDefaultRenewableLifetime: [renewableFormatter lifetimeForControl: renewableSlider]];
    }
    
    
    // Default KerberosPrincipal
    
    selectedCell = [defaultPrincipalRadioButtonMatrix selectedCell];
    if (selectedCell == rememberPrincipalRadioButtonCell) { 
        [preferences setRememberPrincipalFromLastLogin: YES];
    } else if (selectedCell == defaultToThisPrincipalRadioButtonCell) {
        [preferences setRememberPrincipalFromLastLogin: NO];
        
        [preferences setDefaultName: [nameTextField stringValue]];
        [preferences setDefaultRealm: [realmComboBox stringValue]];
    }
    
    // Behaviors
    
    [preferences setAutoRenewTickets:            ([autoRenewCheckbox                    state] == NSOnState)];
    [preferences setShowTimeInDockIcon:          ([showTimeRemainingInDockCheckbox      state] == NSOnState)];
    [preferences setTicketWindowDefaultPosition: ([rememberTicketWindowPositionCheckbox state] != NSOnState)];
    
    selectedCell = [launchActionMatrix selectedCell];
    if (selectedCell == alwaysOpenListWindowRadioButtonCell) {
        [preferences setLaunchAction: LaunchActionAlwaysOpenTicketWindow];
    } else if (selectedCell == neverOpenListWindowRadioButtonCell) {
        [preferences setLaunchAction: LaunchActionNeverOpenTicketWindow];
    } else if (selectedCell == rememberTicketListWindowOpenessRadioButtonCell) {
        [preferences setLaunchAction: LaunchActionRememberOpenTicketWindow];        
    }
    
    // Post notification so the rest of the application notices that the preferences changed.
    [[NSNotificationCenter defaultCenter] postNotificationName: PreferencesDidChangeNotification 
                                                        object: self];
    
    return YES;
}

// ---------------------------------------------------------------------------

- (BOOL) validateLifetimeMaximum: (time_t) lifetimeMaximum
                 lifetimeMinimum: (time_t) lifetimeMinimum
                renewableMaximum: (time_t) renewableMaximum
                renewableMinimum: (time_t) renewableMinimum
{
    BOOL valid = YES;
    
    if (lifetimeMaximum < lifetimeMinimum) {
        [self displayError: @"KAppStringBadLifetimeRange"];
        valid = NO;
    } else if (renewableMaximum < renewableMinimum) {
        [self displayError: @"KAppStringBadRenewableRange"];
        valid = NO;
    }
    
    return valid;
}

// ---------------------------------------------------------------------------

- (time_t) lifetimeMaximum
{
    return ([lifetimeMaximumMinutesTextField intValue] * 60 + 
            [lifetimeMaximumHoursTextField   intValue] * 3600 + 
            [lifetimeMaximumDaysTextField    intValue] * 86400);
}

// ---------------------------------------------------------------------------

- (time_t) lifetimeMinimum
{
    return ([lifetimeMinimumMinutesTextField intValue] * 60 + 
            [lifetimeMinimumHoursTextField   intValue] * 3600 + 
            [lifetimeMinimumDaysTextField    intValue] * 86400);
}

// ---------------------------------------------------------------------------

- (time_t) renewableMaximum
{
    return ([renewableMaximumMinutesTextField intValue] * 60 + 
            [renewableMaximumHoursTextField   intValue] * 3600 + 
            [renewableMaximumDaysTextField    intValue] * 86400);
}

// ---------------------------------------------------------------------------

- (time_t) renewableMinimum
{
    return ([renewableMinimumMinutesTextField intValue] * 60 + 
            [renewableMinimumHoursTextField   intValue] * 3600 + 
            [renewableMinimumDaysTextField    intValue] * 86400);
}

// ---------------------------------------------------------------------------

- (void) displayError: (NSString *) errorKey
{
    NSAlert *alert = [[[NSAlert alloc] init] autorelease];
    [alert addButtonWithTitle: NSLocalizedString (@"KAppStringOK", NULL)];
    [alert setMessageText: NSLocalizedString (@"KAppStringInvalidTimeRanges", NULL)];
    [alert setInformativeText: NSLocalizedString (errorKey, NULL)];
    [alert setAlertStyle: NSWarningAlertStyle];
    
    [alert beginSheetModalForWindow: [self window] 
                      modalDelegate: self 
                     didEndSelector: @selector(errorSheetDidEnd:returnCode:contextInfo:) 
                        contextInfo: NULL];
}


@end
