/*
 * KLSController.h
 *
 * $Header: /cvs/kfm/KerberosFramework/KerberosLogin/Sources/KerberosLoginServer/Headers/KLSController.h,v 1.13 2003/09/11 20:51:30 lxs Exp $
 *
 * Copyright 2003 Massachusetts Institute of Technology.
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

#import "KLSIconImageView.h"

#define kPrompterResponseSpacing 8
#define kPrompterPromptSpacing   12

typedef struct LoginWindowState {
    NSControl *usernameControl;
    NSControl *realmControl;
    krb5_flags flags;
    KLLifetime lifetime;
    KLBoolean renewable;
    KLLifetime renewableLifetime;
    KLLifetime startTime;
    KLBoolean forwardable;
    KLBoolean proxiable;
    KLBoolean addressless;
    const char *serviceName;
    
    NSMutableString *acquiredPrincipalString;
    NSMutableString *acquiredCacheNameString;
} LoginWindowState;

@interface KLSController : NSObject
{
    // Login Window
    IBOutlet NSWindow *loginWindow;
    IBOutlet NSTextField *loginHeaderTextField;
    IBOutlet NSButton *loginOkButton;
    IBOutlet NSButton *loginCancelButton;
    IBOutlet KLSIconImageView *loginKerberosIconImageView;
    IBOutlet NSTextField *loginVersionTextField;
    IBOutlet NSSecureTextField *loginPasswordSecureTextField;
    IBOutlet NSComboBox *loginRealmComboBox;
    IBOutlet NSTextField *loginCallerProvidedRealmTextField;
    IBOutlet NSTextField *loginUsernameTextField;
    IBOutlet NSTextField *loginCallerProvidedUsernameTextField;
    IBOutlet NSButton *loginOptionsButton;
    IBOutlet NSBox *loginOptionsBox;
    IBOutlet NSButton *loginAddresslessCheckbox;
    IBOutlet NSButton *loginForwardableCheckbox;
    IBOutlet NSButton *loginRenewableCheckbox;
    IBOutlet NSTextField *loginRenewableLifetimeText;
    IBOutlet NSSlider *loginRenewableLifetimeSlider;
    IBOutlet NSTextField *loginLifetimeText;
    IBOutlet NSSlider *loginLifetimeSlider;

    float loginWindowOptionsOnFrameHeight;
    float loginWindowOptionsOffFrameHeight;
    
    // Change Password Window
    IBOutlet NSWindow *changePasswordWindow;
    IBOutlet NSButton *changePasswordOkButton;
    IBOutlet NSButton *changePasswordCancelButton;
    IBOutlet KLSIconImageView *changePasswordKerberosIconImageView;
    IBOutlet NSTextField *changePasswordVersionTextField;
    IBOutlet NSTextField *changePasswordPrincipalTextField;
    IBOutlet NSSecureTextField *changePasswordOldPasswordSecureTextField;
    IBOutlet NSSecureTextField *changePasswordNewPasswordSecureTextField;
    IBOutlet NSSecureTextField *changePasswordVerifyPasswordSecureTextField;
    
    // Prompter Window
    IBOutlet NSWindow *prompterWindow;
    IBOutlet NSButton *prompterOkButton;
    IBOutlet NSButton *prompterCancelButton;
    IBOutlet KLSIconImageView *prompterKerberosIconImageView;
    IBOutlet NSTextField *prompterVersionTextField;
    IBOutlet NSTextField *prompterBannerTextField;
    IBOutlet NSMatrix *prompterPromptsMatrix;
    IBOutlet NSBox *prompterResponsesBox;

    NSSize prompterWindowSize;
    NSRect singleResponseFrame;

    // State Structures
    LoginWindowState loginState;
}

- (id) init;
- (void) dealloc;
- (void) awakeFromNib;

#pragma mark -

// Notifications

- (void) applicationDidFinishLaunching: (NSNotification *) aNotification;
- (void) controlTextDidChange: (NSNotification *) aNotification;
- (void) comboBoxSelectionDidChange: (NSNotification *) aNotification;
- (void) comboBoxWillDismiss: (NSNotification *) aNotification;


#pragma mark -

// Login Window

- (KLStatus) getTicketsForPrincipal: (const char *) principalUTF8String
                              flags: (krb5_flags) flags
                           lifetime: (KLLifetime) lifetime
                  renewableLifetime: (KLLifetime) renewableLifetime
                          startTime: (KLTime) startTime
                        forwardable: (KLBoolean) forwardable
                          proxiable: (KLBoolean) proxiable
                        addressless: (KLBoolean) addressless
                        serviceName: (const char *) serviceName
                    applicationName: (const char *) applicationNameString
                applicationIconPath: (const char *) applicationIconPathString;

- (void) loginUpdateOkButton;
- (void) loginUpdateWithCallerProvidedPrincipal: (KLBoolean) callerProvidedPrincipal;
- (void) loginSetupSlider: (NSSlider *) slider
                textField: (NSTextField *) textField
                  minimum: (int) minimum
                  maximum: (int) maximum
                    value: (int) value;

- (IBAction) loginAddresslessCheckboxWasHit: (id) sender;
- (IBAction) loginForwardableCheckboxWasHit: (id) sender;
- (IBAction) loginRenewableCheckboxWasHit: (id) sender;

- (IBAction) loginLifetimeSliderChanged: (id) sender;
- (IBAction) loginRenewableLifetimeSliderChanged: (id) sender;

- (IBAction) loginOptionsButtonWasHit: (id) sender;
- (IBAction) loginCancelButtonWasHit: (id) sender;
- (IBAction) loginOkButtonWasHit: (id) sender;

- (void) loginSaveOptionsIfNeeded;
- (const char *) loginAcquiredPrincipal;
- (const char *) loginAcquiredCacheName;

#pragma mark -

// Change Password Window

- (KLStatus) changePasswordForPrincipal: (const char *) principalUTF8String;

- (void) changePasswordUpdateOkButton;

- (IBAction) changePasswordCancelButtonWasHit: (id) sender;
- (IBAction) changePasswordOkButtonWasHit: (id) sender;

#pragma mark -

- (krb5_error_code) promptWithTitle: (const char *) title
                          banner: (const char *) banner
                          prompts: (krb5_prompt *) prompts
                          promptCount: (int) promptCount;

- (IBAction) prompterCancelButtonWasHit: (id) sender;
- (IBAction) prompterOkButtonWasHit: (id) sender;

#pragma mark -

- (void) displayKLError: (KLStatus) error;
                
- (void) displayKLError: (KLStatus) error
                windowIdentifier: (KLDialogIdentifier) identifier;

- (void) displayServerError: (const char *) errorUTF8String 
                description: (const char *) descriptionUTF8String;

- (void) displayError: (NSString *) errorCString 
                description: (NSString *) descriptionCString;

- (BOOL) askYesNoQuestion: (NSString *) question;

#pragma mark -

- (int) displayAndRunWindow: (NSWindow *) window;

- (void) sheetDidEnd: (NSWindow *) sheet returnCode: (int) returnCode contextInfo: (void *) cInfo;

- (NSWindow *) frontWindow;

@end

