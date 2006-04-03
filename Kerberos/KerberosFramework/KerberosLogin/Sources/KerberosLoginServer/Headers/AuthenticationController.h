/*
 * AuthenticationController.h
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

#import "Preferences.h"
#import "Principal.h"
#import "BadgedImageView.h"
#import "PopupButton.h"
#include <Kerberos/Kerberos.h>

typedef struct AuthenticationControllerState
{
    BOOL doesMinimize;
    BOOL isMinimized;
    
    NSString *callerNameString;
    NSImage *callerIconImage;
    Principal *callerProvidedPrincipal;
    
    NSString *serviceNameString;
    time_t startTime;
    time_t lifetime;
    BOOL forwardable;
    BOOL proxiable;
    BOOL addressless;
    BOOL renewable;
    time_t renewableLifetime;
    
} AuthenticationControllerState;

@interface AuthenticationController : NSWindowController
{
    IBOutlet NSBox *bannerBox;
    IBOutlet BadgedImageView *kerberosIconImageView;
    IBOutlet NSTextField *headerTextField;
    
    IBOutlet NSSecureTextField *passwordSecureTextField;
    IBOutlet NSComboBox *realmComboBox;
    IBOutlet NSTextField *callerProvidedRealmTextField;
    IBOutlet NSTextField *nameTextField;
    IBOutlet NSTextField *callerProvidedNameTextField;

    IBOutlet NSButton *okButton;
    IBOutlet NSButton *cancelButton;
    
    IBOutlet PopupButton *gearPopupButton;
    IBOutlet NSMenu      *gearMenu;
    IBOutlet NSMenuItem  *showOptionsMenuItem;
    IBOutlet NSMenuItem  *changePasswordMenuItem;
    
    IBOutlet NSWindow *optionsSheet;
    IBOutlet NSButton *addresslessCheckbox;
    IBOutlet NSButton *forwardableCheckbox;
    IBOutlet NSButton *renewableCheckbox;
    IBOutlet NSTextField *renewableTextField;
    IBOutlet NSSlider *renewableSlider;
    IBOutlet NSTextField *lifetimeTextField;
    IBOutlet NSSlider *lifetimeSlider;
    IBOutlet NSButton *optionsOKButton;
    IBOutlet NSButton *optionsCancelButton;
    
    float minimizedFrameHeight;
    float maximizedFrameHeight;
    
    AuthenticationControllerState state;
        
    Preferences *preferences;
    
    Principal *acquiredPrincipal;
    NSMutableString *acquiredCacheName;
    
    KLStatus result;
}

- (void) windowDidLoad;

- (void) controlTextDidChange: (NSNotification *) notification;
- (void) comboBoxSelectionDidChange: (NSNotification *) notification;
- (void) comboBoxWillDismiss: (NSNotification *) notification;
- (void) windowDidBecomeKey: (NSNotification *) notification;
- (void) windowDidResignKey: (NSNotification *) notification;

- (IBAction) changePassword: (id) sender;

- (IBAction) showOptions: (id) sender;
- (IBAction) renewableCheckboxWasHit: (id) sender;
- (IBAction) optionsOK: (id) sender;
- (IBAction) optionsCancel: (id) sender;

- (IBAction) ok: (id) sender;
- (IBAction) cancel: (id) sender;

- (void) setDoesMinimize: (BOOL) doesMinimize;

- (void) setCallerNameString: (NSString *) callerNameString;
- (void) setCallerIcon: (NSImage *) callerIcon;
- (void) setCallerProvidedPrincipal: (Principal *) callerProvidedPrincipal;

- (void) setServiceName: (NSString *) serviceName;
- (void) setStartTime: (time_t) startTime;
- (void) setLifetime: (time_t) lifetime;
- (void) setForwardable: (BOOL) forwardable;
- (void) setProxiable: (BOOL) proxiable;
- (void) setAddressless: (BOOL) addressless;
- (void) setRenewable: (BOOL) renewable;
- (void) setRenewableLifetime: (time_t) renewableLifetime;

- (Principal *) principal;
- (void) updateOKButtonState;
- (BOOL) validateMenuItem: (id <NSMenuItem>) menuItem;
- (void) setWindowContentHeight: (float) newHeight;
- (void) minimizeWindow;
- (void) maximizeWindow;
- (void) loginOptionsToPreferences;
- (int) getTickets;

- (Principal *) acquiredPrincipal;
- (NSString *) acquiredCacheName;

- (int) runWindow;
- (void) stopWithCode: (int) returnCode;

@end
