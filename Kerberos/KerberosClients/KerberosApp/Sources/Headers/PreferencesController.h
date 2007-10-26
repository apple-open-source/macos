/*
 * PreferencesController.h
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


#import <Cocoa/Cocoa.h>
#import "KerberosPreferences.h"

@interface PreferencesController : NSWindowController
{
    IBOutlet NSTabView *preferencesTabView;
    
    IBOutlet NSButton *autoRenewCheckbox;
    IBOutlet NSButton *showTimeRemainingInDockCheckbox;
    IBOutlet NSButton *rememberTicketWindowPositionCheckbox;
    IBOutlet NSMatrix *launchActionMatrix;
    IBOutlet NSButtonCell *alwaysOpenListWindowRadioButtonCell;
    IBOutlet NSButtonCell *neverOpenListWindowRadioButtonCell;
    IBOutlet NSButtonCell *rememberTicketListWindowOpenessRadioButtonCell;

    IBOutlet NSMatrix *defaultPrincipalRadioButtonMatrix;
    IBOutlet NSButtonCell *rememberPrincipalRadioButtonCell;
    IBOutlet NSButtonCell *defaultToThisPrincipalRadioButtonCell;
    IBOutlet NSBox *principalBox;
    IBOutlet NSTextField *nameHeaderTextField;
    IBOutlet NSTextField *realmHeaderTextField;
    IBOutlet NSTextField *nameTextField;
    IBOutlet NSComboBox *realmComboBox;

    IBOutlet NSMatrix *defaultOptionsRadioButtonMatrix;
    IBOutlet NSButtonCell *rememberOptionsRadioButtonCell;
    IBOutlet NSButtonCell *defaultToTheseOptionsRadioButtonCell;
    IBOutlet NSTextField *lifetimeHeaderTextField;
    IBOutlet NSTextField *lifetimeTextField;
    IBOutlet NSSlider *lifetimeSlider;
    IBOutlet NSTextField *optionsHeaderTextField;
    IBOutlet NSButton *forwardableCheckbox;
    IBOutlet NSButton *addresslessCheckbox;
    IBOutlet NSButton *renewableCheckbox;
    IBOutlet NSTextField *renewableTextField;
    IBOutlet NSSlider *renewableSlider;

    IBOutlet NSTabViewItem *timeRangesTabViewItem;
    IBOutlet NSTextField *lifetimeMaximumDaysTextField;
    IBOutlet NSTextField *lifetimeMaximumHoursTextField;
    IBOutlet NSTextField *lifetimeMaximumMinutesTextField;
    
    IBOutlet NSTextField *lifetimeMinimumDaysTextField;
    IBOutlet NSTextField *lifetimeMinimumHoursTextField;
    IBOutlet NSTextField *lifetimeMinimumMinutesTextField;
    
    IBOutlet NSTextField *renewableMaximumDaysTextField;
    IBOutlet NSTextField *renewableMaximumHoursTextField;
    IBOutlet NSTextField *renewableMaximumMinutesTextField;
    
    IBOutlet NSTextField *renewableMinimumDaysTextField;
    IBOutlet NSTextField *renewableMinimumHoursTextField;
    IBOutlet NSTextField *renewableMinimumMinutesTextField;
    
    KerberosPreferences *preferences;
}

- (id) init;
- (void) dealloc;

- (IBAction) showWindow: (id) sender;

- (IBAction) apply: (id) sender;
- (IBAction) cancel: (id) sender;
- (IBAction) ok: (id) sender;
- (IBAction) defaultPrincipalRadioButtonWasHit: (id) sender;
- (IBAction) defaultTicketOptionsRadioButtonWasHit: (id) sender;

- (BOOL) tabView: (NSTabView *) tabView shouldSelectTabViewItem: (NSTabViewItem *) tabViewItem;
- (void) errorSheetDidEnd: (NSAlert *) alert returnCode: (int) returnCode contextInfo: (void *) contextInfo;

- (BOOL) preferencesToWindow;
- (BOOL) windowToPreferences;

- (BOOL) validateLifetimeMaximum: (time_t) lifetimeMaximum
                 lifetimeMinimum: (time_t) lifetimeMinimum
                renewableMaximum: (time_t) renewableMaximum
                renewableMinimum: (time_t) renewableMinimum;

- (time_t) lifetimeMaximum;
- (time_t) lifetimeMinimum;
- (time_t) renewableMaximum;
- (time_t) renewableMinimum;

- (void) displayError: (NSString *) errorKey;

@end
