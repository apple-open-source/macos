/*
 * KLSController.m
 *
 * $Header: /cvs/kfm/KerberosFramework/KerberosLogin/Sources/KerberosLoginServer/KLSController.m,v 1.25 2003/09/12 15:53:09 lxs Exp $
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

#import "KLMachIPC.h"
#import "KerberosLoginIPCServer.h"

#import "KLSLifetimeFormatter.h"
#import "KLSController.h"

// Callback function for the Kerberos Framework

krb5_error_code __KLSPrompter (krb5_context  context,
                               void         *data,
                               const char   *name,
                               const char   *banner,
                               int           num_prompts,
                               krb5_prompt   prompts[])
{
    krb5_error_code result = 0;

    KLSController *controller = [NSApp delegate];
    if (controller == NULL) {
        result = klFatalDialogErr;
    } else {
        result = [controller promptWithTitle: name
                                      banner: banner
                                     prompts: prompts
                                 promptCount: num_prompts];
    }
    
    return result;
}

// ---------------------------------------------------------------------------
#pragma mark -

@implementation KLSController

- (id) init
{
    if (self = [super init]) {
        loginState.acquiredPrincipalString = [[NSMutableString alloc] init];
        loginState.acquiredCacheNameString = [[NSMutableString alloc] init];
    }
    return self;
}

- (void) dealloc
{
    [loginState.acquiredPrincipalString  release];
    [loginState.acquiredCacheNameString  release];
    
    [super dealloc];
}

- (void) awakeFromNib
{
    // Fill in the version fields in the dialogs
    NSString *versionString = [[NSBundle mainBundle] objectForInfoDictionaryKey:@"KLSDisplayVersion"];
    if (versionString == NULL) {
        versionString = [[NSBundle mainBundle] objectForInfoDictionaryKey:@"CFBundleGetInfoString"];
    }
    if (versionString != NULL) {
        [loginVersionTextField setStringValue: versionString];
        [changePasswordVersionTextField setStringValue: versionString];
        [prompterVersionTextField setStringValue: versionString];
    } else {
        [loginVersionTextField setStringValue: @""];
        [changePasswordVersionTextField setStringValue: @""];
        [prompterVersionTextField setStringValue: @""];
    }
    
    // remember the dialog frame and height
    singleResponseFrame = [prompterResponsesBox frame];
    prompterWindowSize = [[prompterWindow contentView] frame].size;
    prompterWindowSize.height -= singleResponseFrame.size.height;
    
    // remember the size of the login window frame size with and without the options visible
    loginWindowOptionsOnFrameHeight = loginWindowOptionsOffFrameHeight = [loginWindow frame].size.height;
    loginWindowOptionsOffFrameHeight -= [loginOptionsBox frame].size.height;

    // save these so we can add and remove them
    [loginUsernameTextField retain];
    [loginRealmComboBox retain];
    [loginCallerProvidedUsernameTextField retain];
    [loginCallerProvidedRealmTextField retain];

    // by default, caller did not provide a principal
    [loginCallerProvidedUsernameTextField removeFromSuperview];
    [loginCallerProvidedRealmTextField removeFromSuperview];
}

#pragma mark -

// Notifications

- (void) applicationDidFinishLaunching: (NSNotification *) aNotification
{
    // Initialize the Login Library with our graphical prompter
    __KLSetApplicationPrompter (__KLSPrompter);
    
    // Launch the server after starting the application so that all the 
    // autorelease pools associated with the current run loop have been
    // created and initialized.  Otherwise we leak memory.

    // Initialize the mach server immediately so we start queueing events
    kern_return_t err = mach_server_init (LoginMachIPCServiceName, KerberosLoginIPC_server);
    if (err != KERN_SUCCESS) {
        dprintf ("%s server failed to launch: %s (%d)", LoginMachIPCServiceName, 
                    mach_error_string (err), err);
        exit (1);
    }
    
    err = mach_server_run_server ();
    if (err != KERN_SUCCESS) {
        dprintf ("KerberosLoginServer failed to start up...");
        exit (1);
    }

    exit (0);
}

- (void) controlTextDidChange: (NSNotification *) aNotification
{
    // Not sure which dialog it is, so just update both:
	[self loginUpdateOkButton];
	[self changePasswordUpdateOkButton];
}

- (void) comboBoxSelectionDidChange: (NSNotification *) aNotification
{
    [self loginUpdateOkButton];
}

- (void) comboBoxWillDismiss: (NSNotification *) aNotification
{
    [self loginUpdateOkButton];
}


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
                applicationIconPath: (const char *) applicationIconPathString
{
    KLStatus err = klNoErr;
    KLSize typeSize;
    KLLifetime maxLifetime, minLifetime;
    KLBoolean renewable = (flags & KRB5_GET_INIT_CREDS_OPT_RENEW_LIFE);
    KLBoolean showOptions = false;

    if (applicationNameString != NULL) {
        [loginHeaderTextField setStringValue: [NSString stringWithFormat: NSLocalizedString (@"KLApplicationRequest", NULL),
            [NSString stringWithUTF8String: applicationNameString]]];
    } else {
        [loginHeaderTextField setStringValue: NSLocalizedString (@"KLUnknownRequest", NULL)];
    }
    
    // Set up badge icon:
    if (applicationIconPathString != NULL) {
        NSImage *badgeIconImage = [[NSWorkspace sharedWorkspace] iconForFile:
            [NSString stringWithCString: applicationIconPathString]];
        [loginKerberosIconImageView setBadgeIconImage: badgeIconImage];
    } else {
        [loginKerberosIconImageView setBadgeIconImage: NULL];
    }
    
    ////////////////////////////////////
    // Setup Name, Realm and Password //
    ////////////////////////////////////
    
    if (principalUTF8String != NULL) {
        [self loginUpdateWithCallerProvidedPrincipal: YES];
        loginState.usernameControl = loginCallerProvidedUsernameTextField;
        loginState.realmControl = loginCallerProvidedRealmTextField;
        
#pragma warning Need support for "\@" in realm names
        NSString *principalString = [NSString stringWithUTF8String: principalUTF8String];
        NSRange separator = [principalString rangeOfString: @"@" options: (NSLiteralSearch | NSBackwardsSearch)];

        [loginState.usernameControl setStringValue: [principalString substringToIndex: separator.location]];
        [loginState.realmControl setStringValue: [principalString substringFromIndex: separator.location + separator.length]];
    } else {
        KLIndex defaultRealmIndex;
        KLIndex realmCount = KLCountKerberosRealms ();

        [self loginUpdateWithCallerProvidedPrincipal: NO];
        loginState.usernameControl = loginUsernameTextField;
        loginState.realmControl = loginRealmComboBox;

        if (KLGetDefaultLoginOption (loginOption_LoginName, NULL, &typeSize) == klNoErr) {
            char *name = (char *) malloc (sizeof (char) * (typeSize + 1));

            if (name != NULL) {
                if (KLGetDefaultLoginOption (loginOption_LoginName, name, &typeSize) == klNoErr) {
                    name [typeSize] = '\0'; // NUL-terminate so we can look it up as a UTF8String
                    [loginState.usernameControl setStringValue: [NSString stringWithUTF8String: name]];
                }

                free (name);
            }
        }

        // Fill in the rest of the realms into the combo box
        if (realmCount > 0) {
            int i;

            for (i = 0; i < realmCount; i++) {
                char *realm;
                if (KLGetKerberosRealm (i, &realm) == klNoErr) {
                    [loginRealmComboBox addItemWithObjectValue: [NSString stringWithUTF8String: realm]];
                    KLDisposeString (realm);
                }
            }
        }
        
        if (KLGetKerberosDefaultRealm (&defaultRealmIndex) == klNoErr) {
            [loginRealmComboBox selectItemAtIndex: defaultRealmIndex];
        } else if ([loginRealmComboBox numberOfItems] > 0) {
            [loginRealmComboBox selectItemAtIndex: 0];
        }
        [loginRealmComboBox setObjectValue: [loginRealmComboBox numberOfItems] > 0 ? [loginRealmComboBox objectValueOfSelectedItem] : @""];
        [loginRealmComboBox setNumberOfVisibleItems: [loginRealmComboBox numberOfItems]];
        [loginRealmComboBox setCompletes: YES];
    }

    // Clear the password text field
    [loginPasswordSecureTextField setObjectValue: @""];

    // Set the text insertion pointer location
    if ([[loginState.usernameControl stringValue] length] > 0) {
        [loginWindow setInitialFirstResponder: loginPasswordSecureTextField];
    } else {
        [loginWindow setInitialFirstResponder: loginState.usernameControl];
    }

    //////////////////////////
    // Setup Ticket Options //
    //////////////////////////

    // Fill in options with KLL defaults if not already overridden by client
    if (!(flags & KRB5_GET_INIT_CREDS_OPT_FORWARDABLE)) {
        typeSize = sizeof (forwardable);
        err = KLGetDefaultLoginOption (loginOption_DefaultForwardableTicket, &forwardable, &typeSize);
        if (err != klNoErr) {
            dprintf ("KLGetDefaultLoginOption (loginOption_DefaultForwardableTicket) returned %s (%ld)\n", error_message (err), err);
        } else {
            flags |= KRB5_GET_INIT_CREDS_OPT_FORWARDABLE;
        }
    }

    if (!(flags & KRB5_GET_INIT_CREDS_OPT_PROXIABLE)) {
        typeSize = sizeof (proxiable);
        err = KLGetDefaultLoginOption (loginOption_DefaultProxiableTicket, &proxiable, &typeSize);
        if (err != klNoErr) {
            dprintf ("KLGetDefaultLoginOption (loginOption_DefaultProxiableTicket) returned %s (%ld)\n", error_message (err), err);
        } else {
            flags |= KRB5_GET_INIT_CREDS_OPT_PROXIABLE;
        }
    }

    if (!(flags & KRB5_GET_INIT_CREDS_OPT_ADDRESS_LIST)) {
        typeSize = sizeof (addressless);
        err = KLGetDefaultLoginOption (loginOption_DefaultAddresslessTicket, &addressless, &typeSize);
        if (err != klNoErr) {
            dprintf ("KLGetDefaultLoginOption (loginOption_DefaultAddresslessTicket) returned %s (%ld)\n", error_message (err), err);
        } else {
            flags |= KRB5_GET_INIT_CREDS_OPT_ADDRESS_LIST;
        }
    }

    if (!(flags & KRB5_GET_INIT_CREDS_OPT_RENEW_LIFE)) {
        typeSize = sizeof (renewable);
        err = KLGetDefaultLoginOption (loginOption_DefaultRenewableTicket, &renewable, &typeSize);
        if (err != klNoErr) {
            dprintf ("KLGetDefaultLoginOption (loginOption_DefaultRenewableTicket) returned %s (%ld)\n", error_message (err), err);
        } else {
            if (renewable) { flags |= KRB5_GET_INIT_CREDS_OPT_RENEW_LIFE; }
        }
        typeSize = sizeof (renewableLifetime);
        err = KLGetDefaultLoginOption (loginOption_DefaultRenewableLifetime, &renewableLifetime, &typeSize);
        if (err != klNoErr) {
            dprintf ("KLGetDefaultLoginOption (loginOption_DefaultRenewableLifetime) returned %s (%ld)\n", error_message (err), err);
        }
    }

    if (!(flags & KRB5_GET_INIT_CREDS_OPT_TKT_LIFE)) {
        typeSize = sizeof (lifetime);
        err = KLGetDefaultLoginOption (loginOption_DefaultTicketLifetime, &lifetime, &typeSize);
        if (err != klNoErr) {
            dprintf ("KLGetDefaultLoginOption (loginOption_DefaultTicketLifetime) returned %s (%ld)\n", error_message (err), err);
        } else {
            flags |= KRB5_GET_INIT_CREDS_OPT_TKT_LIFE;
        }
    }

    // Load the options in the state variables
    loginState.flags = flags;
    loginState.lifetime = lifetime;
    loginState.renewable = renewable;
    loginState.renewableLifetime = renewableLifetime;
    loginState.startTime = startTime;
    loginState.forwardable = forwardable;
    loginState.proxiable = proxiable;
    loginState.addressless = addressless;
    loginState.serviceName = serviceName;

    /////////////////////////////////////
    // Copy Ticket Options to Controls //
    /////////////////////////////////////

    [loginForwardableCheckbox setIntValue: loginState.forwardable];
    [loginAddresslessCheckbox setIntValue: loginState.addressless];
    [loginRenewableCheckbox setIntValue: loginState.renewable];

    // Set up the slider ranges
    typeSize = sizeof (KLLifetime);
    err = KLGetDefaultLoginOption (loginOption_MinimalTicketLifetime, &minLifetime, &typeSize);
    if (err != klNoErr) {
        dprintf ("KLGetDefaultLoginOption (loginOption_MinimalTicketLifetime) returned %s (%ld)\n", error_message (err), err);
        minLifetime = loginState.lifetime;
    }

    err = KLGetDefaultLoginOption (loginOption_MaximalTicketLifetime, &maxLifetime, &typeSize);
    if (err != klNoErr) {
        dprintf ("KLGetDefaultLoginOption (loginOption_MaximalTicketLifetime) returned %s (%ld)\n", error_message (err), err);
        maxLifetime = loginState.lifetime;
    }

    [self loginSetupSlider: loginLifetimeSlider
                 textField: loginLifetimeText
                   minimum: minLifetime
                   maximum: maxLifetime
                     value: loginState.lifetime];

    err = KLGetDefaultLoginOption (loginOption_MinimalRenewableLifetime, &minLifetime, &typeSize);
    if (err != klNoErr) {
        dprintf ("KLGetDefaultLoginOption (loginOption_MinimalRenewableLifetime) returned %s (%ld)\n", error_message (err), err);
        minLifetime = loginState.renewableLifetime;
    }

    err = KLGetDefaultLoginOption (loginOption_MaximalRenewableLifetime, &maxLifetime, &typeSize);
    if (err != klNoErr) {
        dprintf ("KLGetDefaultLoginOption (loginOption_MaximalRenewableLifetime) returned %s (%ld)\n", error_message (err), err);
        maxLifetime = loginState.renewableLifetime;
    }

    [self loginSetupSlider: loginRenewableLifetimeSlider
                 textField: loginRenewableLifetimeText
                   minimum: minLifetime
                   maximum: maxLifetime
                     value: loginState.renewableLifetime];

    [self loginRenewableCheckboxWasHit: self]; // update the enabled/disabledness off the slider

    // Update the state of the options:
    typeSize = sizeof (KLBoolean);
    err = KLGetDefaultLoginOption (loginOption_ShowOptions, &showOptions, &typeSize);
    if (err != klNoErr) { showOptions = false; }
    [loginOptionsButton setState: showOptions ? NSOnState : NSOffState];
    [self loginOptionsButtonWasHit: self];

    // Run the window until the user hits ok or cancel
    return [self displayAndRunWindow: loginWindow];
}

- (void) loginUpdateOkButton
{
    if (([[loginState.usernameControl   stringValue] length] > 0) &&
        ([[loginState.realmControl      stringValue] length] > 0) &&
        ([[loginPasswordSecureTextField stringValue] length] > 0)) {
        [loginOkButton setEnabled: TRUE];
    } else {
        [loginOkButton setEnabled: FALSE];
    }
}

- (void) loginUpdateWithCallerProvidedPrincipal: (KLBoolean) callerProvidedPrincipal
{
    static KLBoolean callerProvidedPrincipalState = false;

    if (callerProvidedPrincipalState != callerProvidedPrincipal) {
        if (callerProvidedPrincipal) {
            [[loginUsernameTextField superview] addSubview: loginCallerProvidedUsernameTextField];
            [[loginRealmComboBox superview] addSubview: loginCallerProvidedRealmTextField];

            [loginUsernameTextField removeFromSuperview];
            [loginRealmComboBox removeFromSuperview];
        } else {
            [[loginCallerProvidedUsernameTextField superview] addSubview: loginUsernameTextField];
            [[loginCallerProvidedRealmTextField superview] addSubview: loginRealmComboBox];

            [loginCallerProvidedUsernameTextField removeFromSuperview];
            [loginCallerProvidedRealmTextField removeFromSuperview];
        }
        callerProvidedPrincipalState = callerProvidedPrincipal;
    }
}

- (void) loginSetupSlider: (NSSlider *) slider
                textField: (NSTextField *) textField
                  minimum: (int) minimum
                  maximum: (int) maximum
                    value: (int) value
{
    int min = minimum;
    int max = maximum;
    int increment = 0;

    if (max < min) {
        // swap values
        int temp = max;
        max = min;
        min = temp;
    }

    int	range = max - min;

    if (range < 5*60)              { increment = 1;       // 1 second if under 5 minutes
    } else if (range < 30*60)      { increment = 5;       // 5 seconds if under 30 minutes
    } else if (range < 60*60)      { increment = 15;      // 15 seconds if under 1 hour
    } else if (range < 2*60*60)    { increment = 30;      // 30 seconds if under 2 hours
    } else if (range < 5*60*60)    { increment = 60;      // 1 minute if under 5 hours
    } else if (range < 50*60*60)   { increment = 5*60;    // 5 minutes if under 50 hours
    } else if (range < 200*60*60)  { increment = 15*60;   // 15 minutes if under 200 hours
    } else if (range < 500*60*60)  { increment = 30*60;   // 30 minutes if under 500 hours
    } else                         { increment = 60*60; } // 1 hour otherwise

    int roundedMinimum = (min / increment) * increment;
    if (roundedMinimum > min) { roundedMinimum -= increment; }
    if (roundedMinimum <= 0)  { roundedMinimum += increment; }  // ensure it is positive

    int roundedMaximum = (max / increment) * increment;
    if (roundedMaximum < max) { roundedMaximum += increment; }

    int roundedValue = (value / increment) * increment;
    if (roundedValue < roundedMinimum) { roundedValue = roundedMinimum; }
    if (roundedValue > roundedMaximum) { roundedValue = roundedMaximum; }

    if (roundedMinimum == roundedMaximum) {
        [textField setTextColor: [NSColor grayColor]];
        [slider setEnabled: FALSE];
    } else {
        [textField setTextColor: [NSColor blackColor]];
        [slider setEnabled: TRUE];
    }

    // Attach the formatter to the slider
    NSDateFormatter *lifetimeFormatter = [[KLSLifetimeFormatter alloc] initWithMinimum: roundedMinimum
                                                                               maximum: roundedMaximum
                                                                             increment: increment];

    [textField setFormatter: lifetimeFormatter];
    [lifetimeFormatter release];  // the textField will retain it

    [slider setMinValue: 0];
    [slider setMaxValue: (roundedMaximum - roundedMinimum) / increment];
    [slider setIntValue: (roundedValue - roundedMinimum) / increment];
    [textField takeObjectValueFrom: slider];
}

- (IBAction) loginAddresslessCheckboxWasHit: (id) sender
{
    loginState.addressless = [loginAddresslessCheckbox intValue];
}

- (IBAction) loginForwardableCheckboxWasHit: (id) sender
{
    loginState.forwardable = [loginForwardableCheckbox intValue];
}

- (IBAction) loginRenewableCheckboxWasHit: (id) sender
{
    loginState.renewable = [loginRenewableCheckbox intValue];    
}

- (IBAction) loginLifetimeSliderChanged: (id) sender
{
    [loginLifetimeText takeObjectValueFrom: loginLifetimeSlider];
    loginState.lifetime = [[loginLifetimeText formatter] lifetimeForInt: [loginLifetimeSlider intValue]];
}

- (IBAction) loginRenewableLifetimeSliderChanged: (id) sender
{
    [loginRenewableLifetimeText takeObjectValueFrom: loginRenewableLifetimeSlider];
    loginState.renewableLifetime = [[loginRenewableLifetimeText formatter] lifetimeForInt: [loginRenewableLifetimeSlider intValue]];
}

- (IBAction) loginOptionsButtonWasHit: (id) sender
{
    NSRect loginWindowFrameRect = [loginWindow frame];
    
    if ([loginOptionsButton state] == NSOnState) {
        if (loginWindowFrameRect.size.height != loginWindowOptionsOnFrameHeight) {
            loginWindowFrameRect.origin.y -= (loginWindowOptionsOnFrameHeight - loginWindowOptionsOffFrameHeight);
            loginWindowFrameRect.size.height = loginWindowOptionsOnFrameHeight;
            [loginOptionsButton setTitle: NSLocalizedString (@"KLHideOptions", NULL)];
        }

    } else if ([loginOptionsButton state] == NSOffState) {
        if (loginWindowFrameRect.size.height != loginWindowOptionsOffFrameHeight) {
            loginWindowFrameRect.origin.y += (loginWindowOptionsOnFrameHeight - loginWindowOptionsOffFrameHeight);
            loginWindowFrameRect.size.height = loginWindowOptionsOffFrameHeight;
            [loginOptionsButton setTitle: NSLocalizedString (@"KLShowOptions", NULL)];
        }

    } else {
        NSLog (@"loginOptionsButtonWasHit: Unknown state!");
    }
    [loginWindow setFrame: loginWindowFrameRect display: YES animate: YES];
}

- (IBAction) loginCancelButtonWasHit: (id) sender
{
    [NSApp endSheet: loginWindow returnCode: klUserCanceledErr];
}

- (IBAction) loginOkButtonWasHit: (id) sender
{
    KLStatus err = klNoErr;
    NSString *principalString;
    KLLoginOptions loginOptions = NULL;
    KLPrincipal principal = NULL;
    char *cacheName = NULL;
    
    // Get Tickets:
    principalString = [NSString stringWithFormat: @"%@@%@", [loginState.usernameControl stringValue], [loginState.realmControl stringValue]];
    
    if (err == klNoErr) {
        dprintf ("Creating principal for '%s'\n", [principalString UTF8String]);
        err = KLCreatePrincipalFromString ([principalString UTF8String], kerberosVersion_V5, &principal);
    }
    
    if (err == klNoErr) {
        err = KLCreateLoginOptions (&loginOptions);
    }
    
    if (err == klNoErr) {
        err = KLLoginOptionsSetTicketLifetime (loginOptions, loginState.lifetime);
    }
    
    if (err == klNoErr) {
        if (loginState.renewable) {
            err = KLLoginOptionsSetRenewableLifetime (loginOptions, loginState.renewableLifetime);
        } else {
            err = KLLoginOptionsSetRenewableLifetime (loginOptions, 0L);
        }
    }

    if (err == klNoErr) {
        err = KLLoginOptionsSetTicketStartTime (loginOptions, loginState.startTime);
    }
    
    if (err == klNoErr) {
        err = KLLoginOptionsSetServiceName (loginOptions, loginState.serviceName);
    }
    
    if (err == klNoErr) {
        err = KLLoginOptionsSetForwardable (loginOptions, loginState.forwardable);
    }
    
    if (err == klNoErr) {
        err = KLLoginOptionsSetProxiable (loginOptions, loginState.proxiable);
    }
    
    if (err == klNoErr) {
        err = KLLoginOptionsSetAddressless (loginOptions, loginState.addressless);
    }
    
    if (err == klNoErr) {
        err = KLAcquireNewInitialTicketsWithPassword (principal, 
                                                      loginOptions, 
                                                      [[loginPasswordSecureTextField stringValue] UTF8String], 
                                                      &cacheName);
        if (err == KRB5KDC_ERR_KEY_EXP) {
            // Ask the user if s/he wants to change the password
            if ([self askYesNoQuestion: NSLocalizedString (@"KLStringPasswordExpired", NULL)] == YES) {
                if ([self changePasswordForPrincipal: [principalString UTF8String]] == klNoErr) {
                    // Okay, we changed the password, now try again with the new password:
                    err = KLAcquireNewInitialTicketsWithPassword (principal, 
                                                                  loginOptions, 
                                                                  [[changePasswordNewPasswordSecureTextField stringValue] UTF8String], 
                                                                  &cacheName);
                }
            }
        }
    }

    if (err == klNoErr) {
        // Save the current settings of the dialog if needed
        [self loginSaveOptionsIfNeeded];

        // Save the principal and cache name
        [loginState.acquiredPrincipalString setString: principalString];
        [loginState.acquiredCacheNameString setString: [NSString stringWithUTF8String: cacheName]];

        [NSApp endSheet: loginWindow returnCode: klNoErr];
    } else if (err != klUserCanceledErr) {
        [self displayKLError: err];
    }

    if (loginOptions != NULL) {
        KLDisposeLoginOptions (loginOptions);
    }
    if (principal != NULL) {
        KLDisposePrincipal (principal);
    }
    if (cacheName != NULL) {
        KLDisposeString (cacheName);
    }
}

- (void) loginSaveOptionsIfNeeded
{
    KLStatus err = klNoErr;
    KLBoolean rememberShowOptions;
    KLBoolean rememberPrincipal;
    KLBoolean rememberExtras;
    KLSize typeSize = sizeof (KLBoolean);

    // Should we remember the principal in the user's preferences?
    err = KLGetDefaultLoginOption (loginOption_RememberPrincipal, &rememberPrincipal, &typeSize);
    if (err == klNoErr && rememberPrincipal) {
        NSString *loginName = [loginState.usernameControl stringValue];
        NSString *loginRealm = [loginState.realmControl stringValue];
        KLIndex loginRealmIndex;

        err = KLSetDefaultLoginOption (loginOption_LoginName, [loginName UTF8String], [loginName length]);
        if (err != klNoErr) dprintf ("KLSetDefaultLoginOption (loginOption_LoginName) returned %s (%ld)\n", error_message (err), err);

        err = KLSetDefaultLoginOption (loginOption_LoginInstance, "", 0);
        if (err != klNoErr) dprintf ("KLSetDefaultLoginOption (loginOption_LoginInstance) returned %s (%ld)\n", error_message (err), err);

        err = KLFindKerberosRealmByName ([loginRealm UTF8String], &loginRealmIndex);
        if (err != klNoErr) {
            err = KLInsertKerberosRealm (realmList_End, [loginRealm UTF8String]); // Add the realm
        }
        if (err == klNoErr) {
            // Either we found the realm or we succeeded in adding it
            err = KLSetKerberosDefaultRealmByName ([loginRealm UTF8String]);
            if (err != klNoErr) dprintf ("KLSetKerberosDefaultRealmByName returned %s (%ld)\n", error_message (err), err);
        }
    }

    // Should we remember the state of the options button?
    err = KLGetDefaultLoginOption (loginOption_RememberShowOptions, &rememberShowOptions, &typeSize);
    if (err == klNoErr && rememberShowOptions) {
        KLBoolean showOptions = ([loginOptionsButton state] == NSOnState);

        err = KLSetDefaultLoginOption (loginOption_ShowOptions, &showOptions, sizeof (showOptions));
        if (err != klNoErr) dprintf ("KLSetDefaultLoginOption (loginOption_ShowOptions) returned %s (%ld)\n", error_message (err), err);
    }

    // Should we remember the ticket options in the user's preferences?
    err = KLGetDefaultLoginOption (loginOption_RememberExtras, &rememberExtras, &typeSize);
    if ((err == klNoErr) && rememberExtras) {
        err = KLSetDefaultLoginOption (loginOption_DefaultForwardableTicket, &loginState.forwardable, sizeof (loginState.forwardable));
        if (err != klNoErr) dprintf ("KLSetDefaultLoginOption (loginOption_DefaultForwardableTicket) returned %s (%ld)\n", error_message (err), err);

        err = KLSetDefaultLoginOption (loginOption_DefaultAddresslessTicket, &loginState.addressless, sizeof (loginState.addressless));
        if (err != klNoErr) dprintf ("KLSetDefaultLoginOption (loginOption_DefaultAddresslessTicket) returned %s (%ld)\n", error_message (err), err);

        err = KLSetDefaultLoginOption (loginOption_DefaultTicketLifetime, &loginState.lifetime, sizeof (loginState.lifetime));
        if (err != klNoErr) dprintf ("KLSetDefaultLoginOption (loginOption_DefaultTicketLifetime) returned %s (%ld)\n", error_message (err), err);

        err = KLSetDefaultLoginOption (loginOption_DefaultRenewableTicket, &loginState.renewable, sizeof (loginState.renewable));
        if (err != klNoErr) dprintf ("KLSetDefaultLoginOption (loginOption_DefaultRenewableTicket) returned %s (%ld)\n", error_message (err), err);

        if (loginState.renewable) {
            err = KLSetDefaultLoginOption (loginOption_DefaultRenewableLifetime, &loginState.renewableLifetime, sizeof (loginState.renewableLifetime));
            if (err != klNoErr) dprintf ("KLSetDefaultLoginOption (loginOption_DefaultRenewableLifetime) returned %s (%ld)\n", error_message (err), err);
        }
    }
}

- (const char *) loginAcquiredPrincipal
{
    return [loginState.acquiredPrincipalString UTF8String];
}

- (const char *) loginAcquiredCacheName
{
    return [loginState.acquiredCacheNameString UTF8String];
}

#pragma mark -

// Change Password Window

- (KLStatus) changePasswordForPrincipal: (const char *) principalUTF8String
{
    [changePasswordPrincipalTextField setObjectValue: [NSString stringWithUTF8String: principalUTF8String]];
    [changePasswordOldPasswordSecureTextField setObjectValue: @""];
    [changePasswordNewPasswordSecureTextField setObjectValue: @""];
    [changePasswordVerifyPasswordSecureTextField setObjectValue: @""];

    // Run until the user hits ok or cancel
    return [self displayAndRunWindow: changePasswordWindow];
}


- (void) changePasswordUpdateOkButton
{
    if ([[changePasswordOldPasswordSecureTextField stringValue] length] > 0
        && [[changePasswordNewPasswordSecureTextField stringValue] length] > 0
        && [[changePasswordVerifyPasswordSecureTextField stringValue] length] > 0) {
        [changePasswordOkButton setEnabled:TRUE];
    } else {
        [changePasswordOkButton setEnabled:FALSE];
    }    
}

- (IBAction) changePasswordCancelButtonWasHit: (id) sender
{
    [NSApp endSheet: changePasswordWindow returnCode: klUserCanceledErr];
}

- (IBAction) changePasswordOkButtonWasHit: (id) sender
{
    KLStatus err = klNoErr;
    KLPrincipal principal = NULL;
    char *rejectionErrorUTF8String = NULL;
    char *rejectionDescriptionUTF8String = NULL;
    Boolean rejected = FALSE;
    
    if ([[changePasswordNewPasswordSecureTextField stringValue] 
         isEqual: [changePasswordVerifyPasswordSecureTextField stringValue]] == false) {
        err = klPasswordMismatchErr;
    }
    
    if (err == klNoErr) {
        err = KLCreatePrincipalFromString ([[changePasswordPrincipalTextField stringValue] UTF8String], kerberosVersion_V5, &principal);
    }
    
    if (err == klNoErr) {
        err = KLChangePasswordWithPasswords (principal, 
                                             [[changePasswordOldPasswordSecureTextField stringValue] UTF8String], 
                                             [[changePasswordNewPasswordSecureTextField stringValue] UTF8String],
                                             &rejected, &rejectionErrorUTF8String, &rejectionDescriptionUTF8String);
    }
    
    if (err == klNoErr) {
        if (rejected) {
           [self displayServerError: rejectionErrorUTF8String description: rejectionDescriptionUTF8String];
        } else {
            [NSApp endSheet: changePasswordWindow returnCode: klNoErr];
        }
    } else if (err != klUserCanceledErr) {
        [self displayKLError: err];
    }
    
    if (principal != NULL) {
        KLDisposePrincipal (principal);
    }
    if (rejectionErrorUTF8String != NULL) {
        KLDisposeString (rejectionErrorUTF8String);
    }
    if (rejectionDescriptionUTF8String != NULL) {
        KLDisposeString (rejectionDescriptionUTF8String);
    }
}

#pragma mark -

- (krb5_error_code) promptWithTitle: (const char *) title
                          banner: (const char *) banner
                          prompts: (krb5_prompt *) prompts
                          promptCount: (int) promptCount
{
    KLStatus err = klNoErr;
    int i;
    NSRect frameRect;
    NSSize windowSize;
    NSMutableArray *responsesArray = [NSMutableArray arrayWithCapacity: promptCount];
    NSString *titleString = (title != NULL) ? [NSString stringWithUTF8String: title] : @"";
    NSString *bannerString = (banner != NULL) ? [NSString stringWithUTF8String: banner] :
                                                NSLocalizedString(@"KLStringPrompterChangeNotice", NULL);

    [prompterWindow setTitle: titleString];
    [prompterBannerTextField setStringValue: bannerString];

    [prompterPromptsMatrix renewRows: promptCount columns: 1];
    [prompterPromptsMatrix sizeToCells];
    
    // Calculate resize the window to hold the new cells.
    // The matrix cells will automatically be moved down.
    windowSize = prompterWindowSize;
    frameRect = [prompterWindow frame];
    windowSize.height += (singleResponseFrame.size.height * promptCount) + (kPrompterResponseSpacing * (promptCount - 1));
    [prompterWindow setContentSize: windowSize];

    for (i = 0; i < promptCount; i++) {
        NSTextField *responseTextField = NULL;

        [[prompterPromptsMatrix cellAtRow: i column: 0] setObjectValue: [NSString stringWithUTF8String: prompts[i].prompt]];

        frameRect = singleResponseFrame;
        frameRect.origin.y += ((promptCount - 1) - i) * (kPrompterResponseSpacing + frameRect.size.height);

        if (prompts[i].hidden) {
            NSSecureTextField *responseSecureTextField = [[NSSecureTextField alloc] initWithFrame: frameRect];

            [[responseSecureTextField cell] setEchosBullets: YES];
            responseTextField = responseSecureTextField;
        } else {
            responseTextField = [[NSTextField alloc] initWithFrame: frameRect];
        }

        [responseTextField setEnabled: YES];
        [responseTextField setEditable: YES];
        [responseTextField setBezeled: YES];
        [responseTextField setBezelStyle: NSTextFieldSquareBezel];
        [responseTextField setAlignment: NSLeftTextAlignment];
        [responseTextField setDrawsBackground: YES];
        [responseTextField setBackgroundColor: [NSColor whiteColor]];
        [responseTextField setFont: [NSFont systemFontOfSize: 13]];
        [responseTextField setStringValue: @""];
        [responseTextField autorelease];
        
        [responsesArray addObject: responseTextField];
        [[prompterWindow contentView] addSubview: responseTextField];
        if (i == 0) {
            [prompterWindow setInitialFirstResponder: responseTextField];
        } else {
            // chain up the text field so we can tab between them
            [[responsesArray objectAtIndex: i - 1] setNextKeyView: responseTextField];
        }
    }

    // connect the last text field to the first one
    if (promptCount > 1) {
        [[responsesArray objectAtIndex: promptCount - 1] setNextKeyView: [responsesArray objectAtIndex: 0]];
    }

    // Run until the user hits ok or cancel
    err = [self displayAndRunWindow: prompterWindow];
    if (err == klNoErr) {
        for (i = 0; i < promptCount; i++) {
            NSTextField *responseTextField = [responsesArray objectAtIndex: i];

            NSString *response = response = [responseTextField stringValue];
            int length = [response length] + 1;
            
            // Make sure it won't overflow:
            if (length > prompts[i].reply->length) 
            	length = prompts[i].reply->length;
                
            memmove (prompts[i].reply->data, [response UTF8String], length * sizeof (char));
            prompts[i].reply->data[length - 1] = '\0';
            prompts[i].reply->length = length;

            [responseTextField removeFromSuperview]; // Remove from prompter window
        }
    }
    
    return err;
}

- (IBAction) prompterCancelButtonWasHit: (id) sender
{
	[NSApp endSheet: prompterWindow returnCode: klUserCanceledErr];
}

- (IBAction) prompterOkButtonWasHit: (id) sender
{
 	[NSApp endSheet: prompterWindow returnCode: klNoErr];
}

#pragma mark -

- (void) displayKLError: (KLStatus) error
{
    NSWindow *parentWindow = [self frontWindow];
    KLStatus err = klNoErr;
    char *descriptionUTF8String;
    
    err = KLGetErrorString (error, &descriptionUTF8String);
    if (err == klNoErr) {
        KLDialogIdentifier identifier = loginLibrary_UnknownDialog;
        
        if (parentWindow == loginWindow) {
            identifier = loginLibrary_LoginDialog;
        } else if (parentWindow == changePasswordWindow) {
            identifier = loginLibrary_ChangePasswordDialog;
        } else if (parentWindow == prompterWindow) {
            identifier = loginLibrary_PrompterDialog;
        }
        
        [self displayKLError: error windowIdentifier: identifier];
    }
}

- (void) displayKLError: (KLStatus) error
                windowIdentifier: (KLDialogIdentifier) identifier
{
    KLStatus err = klNoErr;
    char *descriptionUTF8String;

    // Use a special error when the caps lock key is down and the password was incorrect
    // if the caps lock key is down accidentally, it will still be down now
    switch (error) {
        case INTK_BADPW:
        case KRB5KRB_AP_ERR_BAD_INTEGRITY:
        case KRBET_INTK_BADPW:
        case KRB5KDC_ERR_PREAUTH_FAILED:
        case klBadPasswordErr: {
            unsigned int modifiers = [[NSApp currentEvent] modifierFlags];
            if (modifiers & NSAlphaShiftKeyMask) {
                error = klCapsLockErr;
            }
        }
    }

    err = KLGetErrorString (error, &descriptionUTF8String);
    if (err == klNoErr) {
        NSString *key = @"KLStringUnknownError";
        
        // Get the header string:
        if (identifier == loginLibrary_LoginDialog) {
            key = @"KLStringLoginFailed";
        } else if (identifier == loginLibrary_ChangePasswordDialog) {
            key = @"KLStringChangePasswordFailed";
        } else if (identifier == loginLibrary_PrompterDialog) {
            key = @"KLStringPrompterFailed";
        }
        
        [self displayError: NSLocalizedString (key, NULL)
                description: [NSString stringWithUTF8String: descriptionUTF8String]];
    }
}


- (void) displayServerError: (const char *) errorUTF8String 
                description: (const char *) descriptionUTF8String
{
    // Remove all newlines from the server responses (they look goofy in the dialog):
    NSMutableString *errorString = [NSMutableString stringWithUTF8String: errorUTF8String];
    [errorString replaceOccurrencesOfString: @"\n" withString: @"" 
                        options: NSLiteralSearch 
                        range: NSMakeRange (0, [errorString length])];

    NSMutableString *descriptionString = [NSMutableString stringWithUTF8String: descriptionUTF8String];
    [descriptionString replaceOccurrencesOfString: @"\n" withString: @"" 
                              options: NSLiteralSearch 
                              range: NSMakeRange (0, [descriptionString length])];

    [self displayError: errorString description: descriptionString];
}

- (void) displayError: (NSString *) errorString 
                description: (NSString *) descriptionString
{
    NSPanel *errorPanel = NSGetAlertPanel (errorString, descriptionString, 
                                                NSLocalizedString (@"KLStringOK", NULL), NULL, NULL);
    [self displayAndRunWindow: errorPanel];
    NSReleaseAlertPanel (errorPanel);
}

- (BOOL) askYesNoQuestion: (NSString *) question
{
    NSWindow *parentWindow = [self frontWindow];
    NSString *key = @"KLStringUnknownError";
    int response;
    
    if (parentWindow == loginWindow) {
        key = @"KLStringLoginFailed";
    } else if (parentWindow == changePasswordWindow) {
        key = @"KLStringChangePasswordFailed";
    } else if (parentWindow == prompterWindow) {
        key = @"KLStringPrompterFailed";
    }
    
    
    NSPanel *questionPanel = NSGetAlertPanel (NSLocalizedString (key, NULL), 
                                              question, 
                                              NSLocalizedString (@"KLStringYes", NULL), 
                                              NSLocalizedString (@"KLStringNo", NULL), 
                                              NULL);
    response = [self displayAndRunWindow: questionPanel];
    NSReleaseAlertPanel (questionPanel);
    return (response == NSAlertDefaultReturn) ? YES : NO;
}

#pragma mark -

- (int) displayAndRunWindow: (NSWindow *) window
{
    NSWindow *parent = [NSApp modalWindow];
    int response = 0;
    
    // Can't display two sheets on the same window so make it a modal dialog
    if (parent != NULL && [parent attachedSheet] != NULL) {
        dprintf ("Can't display a sheet on a sheet");
        parent = NULL;
    }
    
    // Bring KLS to the front
    [NSApp activateIgnoringOtherApps: YES];
    
    // Prepare the window:
    if (parent != NULL) {
         [NSApp beginSheet: window 
                modalForWindow: parent 
                modalDelegate: self
                didEndSelector: @selector(sheetDidEnd:returnCode:contextInfo:)
                contextInfo: window];
    } else {
        [window center];
    }
    
    // Run the main event loop:
    response = [NSApp runModalForWindow: window];
        
    // Remove the window:
    [window orderOut: self];
    
    return response;
}


- (void) sheetDidEnd: (NSWindow *) sheet returnCode: (int) returnCode contextInfo: (void *) contextInfo 
{
    if (sheet == (NSWindow *) contextInfo) {
        [NSApp stopModalWithCode: returnCode];
    }
}

- (NSWindow *) frontWindow
{
    return [NSApp modalWindow];
}

@end

