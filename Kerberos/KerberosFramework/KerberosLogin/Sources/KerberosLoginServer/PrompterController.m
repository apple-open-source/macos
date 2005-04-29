/*
 * PrompterController.m
 *
 * $Header: /cvs/kfm/KerberosFramework/KerberosLogin/Sources/KerberosLoginServer/PrompterController.m,v 1.8 2005/01/23 17:53:22 lxs Exp $
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
#import "PrompterController.h"
#import "AuthenticationController.h"
#import "ChangePasswordController.h"
#import "ErrorAlert.h"

#define ResponseSpacing 16
#define PromptSpacing   12

#pragma mark -

// ---------------------------------------------------------------------------

krb5_error_code GraphicalKerberosPrompter (krb5_context  context,
                                           void         *data,
                                           const char   *name,
                                           const char   *banner,
                                           int           num_prompts,
                                           krb5_prompt   prompts[])
{
    krb5_error_code err = 0;
    int i;
    NSMutableArray *promptsArray = NULL;
    PrompterController *controller = NULL;
    
    if (!err) {
        promptsArray = [[NSMutableArray alloc] init];
        if (promptsArray == NULL) { err = ENOMEM; }
    }
    
    if (!err) {
        for (i = 0; i < num_prompts; i++) {
            Prompt *prompt = [[Prompt alloc] initWithPrompt: &prompts[i]];
            if (prompt == NULL) { err = ENOMEM; break; }
            
            [promptsArray addObject: prompt];
            [prompt release];
        }
    }
    
    if (!err) {
        NSString *message     = (name   != NULL) ? [NSString stringWithUTF8String: name]   : NULL;
        NSString *description = (banner != NULL) ? [NSString stringWithUTF8String: banner] : NULL;;

        controller = [[PrompterController alloc] initWithTitle: message
                                                        banner: description
                                                       prompts: promptsArray];
        if (controller == NULL) { err = klFatalDialogErr; }
    }
    
    if (!err) {
        NSWindow *parentWindow = [[NSApp delegate] activeWindow];
        if (parentWindow != NULL) {
            err = [controller runSheetModalForWindow: parentWindow];
        } else {
            err = [controller runWindow];
        }
    }
    
    if (!err) {
        for (i = 0; i < num_prompts; i++) {
            Prompt *prompt = [promptsArray objectAtIndex: i];
            [prompt saveResponseInPrompt: &prompts[i]];
        }        
    }
    
    [promptsArray release];
    [controller release];
    
    return err;
}

#pragma mark -

@implementation Prompt

// ---------------------------------------------------------------------------

- (id) initWithPrompt: (krb5_prompt *) prompt
{
    if ((self = [super init])) {
        promptString = NULL;
        responseString = NULL;
        
        if (prompt == NULL) {
            [self release];
            return NULL;
        }
        
        promptString = [[NSString alloc] initWithCString: prompt->prompt];
        if (promptString == NULL) {
            [self release];
            return NULL;
        }
        
        secure = prompt->hidden;
        
        responseString = [[NSMutableString alloc] init];
        if (responseString == NULL) {
            [self release];
            return NULL;
        }
    }
    return self;
}

// ---------------------------------------------------------------------------

- (void) dealloc
{
    if (promptString   != NULL) { [promptString release]; }
    if (responseString != NULL) { [responseString release]; }
    
    [super dealloc];
}

// ---------------------------------------------------------------------------


- (NSString *) prompt
{
    return promptString;
}

// ---------------------------------------------------------------------------

- (BOOL) secure
{
    return secure;
}

// ---------------------------------------------------------------------------

- (void) setResponse: (NSString *) response
{
    [responseString setString: response];
}

// ---------------------------------------------------------------------------

- (void) saveResponseInPrompt: (krb5_prompt *) prompt
{
    if (prompt != NULL) {
        const char *response = [responseString UTF8String];
        
        unsigned int responseLength = strlen (response) + 1;
        unsigned int replyLength = (responseLength >= prompt->reply->length) ? prompt->reply->length : responseLength;

        memmove (prompt->reply->data, response, replyLength * sizeof (char));
        prompt->reply->data[replyLength - 1] = '\0';
        prompt->reply->length = strlen (prompt->reply->data);
    }
}

@end

#pragma mark -

@implementation PrompterController

// ---------------------------------------------------------------------------

- (id) initWithTitle: (NSString *) title
              banner: (NSString *) banner
             prompts: (NSArray *) prompts
{
    if ((self = [super initWithWindowNibName: @"PrompterController"])) {
        dprintf ("PrompterController initializing");
        
        result = klNoErr;
        titleString = NULL;
        bannerString = NULL;
        promptsArray = NULL;
        responseTextFieldArray = NULL;
        applicationNameString = NULL;
        applicationIconPathString = NULL;
        isSheet = NO;
        
        responseTextFieldArray = [[NSMutableArray alloc] init];
        if (responseTextFieldArray == NULL) {
            [self release];
            return NULL;
        }

        if (title   != NULL) { titleString  = [title retain]; }
        if (banner  != NULL) { bannerString = [banner retain]; }
        
        if (prompts == NULL) { 
            [self release];
            return NULL;
        }
        promptsArray = [prompts retain];
    }
    
    return self;
}

// ---------------------------------------------------------------------------

- (void) dealloc
{
    dprintf ("PrompterController deallocating");
    if (titleString               != NULL) { [titleString release]; }
    if (bannerString              != NULL) { [bannerString release]; }
    if (promptsArray              != NULL) { [promptsArray release]; }
    if (responseTextFieldArray    != NULL) { [responseTextFieldArray release]; }
    if (applicationNameString     != NULL) { [applicationNameString release]; }
    if (applicationIconPathString != NULL) { [applicationIconPathString release]; }
    [super dealloc];
}

#pragma mark -- Loading --

// ---------------------------------------------------------------------------

- (void) windowDidLoad
{
    dprintf ("Entering PrompterController windowDidLoad");

    // Set up some values used later to dynamically size the window
    NSRect singleResponseFrame = [responsesBox frame];
    float singlePromptHeight = [responsesBox frame].size.height;
    NSSize baseWindowSize = [[[self window] contentView] frame].size;
    baseWindowSize.height -= singlePromptHeight;
    int promptCount = [promptsArray count];
    int i;
        
    // Set up the dialog icon:
    NSImage *applicationIconImage = [NSImage imageNamed: @"NSApplicationIcon"];
    [kerberosIconImageView setImage: applicationIconImage];
    if (applicationIconPathString != NULL) {
        NSImage *badgeImage = [[NSWorkspace sharedWorkspace] iconForFile: applicationIconPathString];
        [kerberosIconImageView setBadgeImage: badgeImage];
    }
    
    // Title and Banner
    [[self window] setTitle: (titleString == NULL) ? @"" : titleString];
    [bannerTextField setStringValue: (bannerString == NULL) ? @"" : bannerString];

    if (promptCount > 0) {
        // Resize the prompts matrix
        NSSize spacingSize = { 0, ResponseSpacing };
        
        [promptsMatrix setIntercellSpacing: spacingSize];
        [promptsMatrix renewRows: promptCount columns: 1];
        [promptsMatrix sizeToCells];
    } else {
        // if there are no prompts, hide everything related to them
        [promptsMatrix setHidden: YES];
        [promptsMatrix setEnabled: FALSE];
        [responsesBox setHidden: YES];
        [cancelButton setHidden: YES];
        [cancelButton setEnabled: FALSE];
    }    
    
    // Resize the window to hold the new cells.
    // The matrix cells will automatically be moved down.
    NSSize windowSize = baseWindowSize;
    if (promptCount > 0) {
        windowSize.height += ((singlePromptHeight * [promptsArray count]) + 
                              (PromptSpacing * ([promptsArray count] - 1)));
    }
    [[self window] setContentSize: windowSize];
        
    // Prompt and Response Text Fields
    //
    // Note that we can't have an NSMatrix with NSSecureTextFields in it because
    // of a bug in NSMatrix (stringValue on the secure text field returns bullets).
    for (i = 0; i < promptCount; i++) {
        NSTextField *responseTextField = NULL;
        Prompt *prompt = [promptsArray objectAtIndex: i];
        
        // Set the prompt
        [[promptsMatrix cellAtRow: i column: 0] setObjectValue: [prompt prompt]];
        
        // Create the response text field:
        NSRect frameRect = singleResponseFrame;
        frameRect.origin.y += ((promptCount - 1) - i) * (PromptSpacing + frameRect.size.height);
        
        if ([prompt secure]) {
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
        
        [responseTextFieldArray addObject: responseTextField];
        [[[self window] contentView] addSubview: responseTextField];
        
        if (i == 0) {
            [[self window] setInitialFirstResponder: responseTextField];
        } else {
            // chain up the text field so we can tab between them
            [[responseTextFieldArray objectAtIndex: i - 1] setNextKeyView: responseTextField];
        }
        
        [responseTextField release];
    }
    
    // connect the last text field to the first one
    if (promptCount > 1) {
        [[responseTextFieldArray objectAtIndex: promptCount - 1] setNextKeyView: 
            [responseTextFieldArray objectAtIndex: 0]];
    }
}    

#pragma mark -- Actions --

// ---------------------------------------------------------------------------

- (IBAction) ok: (id) sender
{
    unsigned int i;
    
    for (i = 0; i < [promptsArray count]; i++) {
        NSTextField *responseTextField = [responseTextFieldArray objectAtIndex: i];
        Prompt *prompt = [promptsArray objectAtIndex: i];
        
        [prompt setResponse: [responseTextField stringValue]];
    }

    [self stopWithCode: klNoErr];
}

// ---------------------------------------------------------------------------

- (IBAction) cancel: (id) sender
{
    [self stopWithCode: klUserCanceledErr];
}

#pragma mark -- Miscellaneous --

// ---------------------------------------------------------------------------

- (int) runWindow
{
    isSheet = NO;
    
    [[NSApp delegate] addActiveWindow: [self window]];    
    [[self window] center];
    [self showWindow: self];
    [NSApp run];
    [self close];
    [[NSApp delegate] removeActiveWindow: [self window]];
    
    return result;
}


// ---------------------------------------------------------------------------

- (int) runSheetModalForWindow: (NSWindow *) parentWindow
{
    isSheet = YES;
    
    [NSApp beginSheet: [self window]
       modalForWindow: parentWindow 
        modalDelegate: self 
       didEndSelector: NULL
          contextInfo: NULL];
    
    int err = [NSApp runModalForWindow: [self window]];
    
    [self close];
    
    return err;
}


// ---------------------------------------------------------------------------

- (void) stopWithCode: (int) returnCode
{
    if (isSheet) {
        [NSApp endSheet: [self window] returnCode: returnCode];
        [NSApp stopModalWithCode: returnCode];
    } else {
        result = returnCode;
        [NSApp stop: self];
    }
}

// ---------------------------------------------------------------------------

- (BOOL) worksWhenModal
{
    return YES;
}

@end

