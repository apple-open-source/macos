/*
 * main.m
 *
 * $Header: /cvs/kfm/KerberosFramework/KerberosLogin/Sources/KerberosLoginServer/main.m,v 1.7 2005/04/19 22:38:37 lxs Exp $
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

#import "FloatingWindow.h"
#import "PrompterController.h"
#import "KerberosAgentIPCServer.h"
#import "AuthenticationController.h"

int main (int argc, const char *argv[])
{
    kern_return_t err = KERN_SUCCESS;
    NSAutoreleasePool *pool = [[NSAutoreleasePool alloc] init];
    
    // Make all future windows float:
    // We do this so we can use Apple's NSGetAlertPanel() rather than rolling our own dialogs.
    // And so that popups like the realms menu don't appear under the dialog!
    // Note that we can't use categories because we need to call the superclass.
    [FloatingWindow poseAsClass: [NSWindow class]];

    // Initialize the Login Library with our graphical prompter
    __KLSetApplicationPrompter (GraphicalKerberosPrompter);

    openlog (argv[0], LOG_CONS | LOG_PID, LOG_AUTH);
    
    syslog (LOG_INFO, "Starting up.");   
    
    // Initialize the application
    [NSApplication sharedApplication];
    [NSBundle loadNibNamed: @"KerberosAgent" owner: NSApp];
    
    // Testing:
    if (0) {
        AuthenticationController *controller = NULL;
        Principal *principal = NULL;
        
        if (!err) {
            controller = [[AuthenticationController alloc] init];
            if (controller == NULL) { err = ENOMEM; }
        }
        
        if (!err) {
            principal = [[Principal alloc] initWithString: @"me@EXAMPLE.COM" klVersion: kerberosVersion_V5];
        }
        
        if (!err) {
            //[controller setDoesMinimize: YES];
            //[controller setCallerProvidedPrincipal: principal];
            err = [controller runWindow];
            if (err) { NSLog (@"Got error %ld (%s)\n", err, error_message (err)); }
            err = 0;
        }
        
        if (principal  != NULL) { [principal release]; }
        if (controller != NULL) { [controller release]; }

    } else if (0) {        
        err = __KLPrompter (NULL, NULL, NULL, NULL, 0, NULL);
        
        if (!err) {
            err = __KLPrompter (NULL, NULL, NULL, "test message", 0, NULL);
        }
        
        if (!err) {
            char *prompt[4] = {"first prompt:", "second prompt:", "third prompt:", "fourth prompt:"};
            krb5_data replies[4];
            char reply[4][64];
            krb5_prompt prompts[4];
            int num_prompts = 4;
            
            prompts[0].hidden = 0;
            prompts[0].prompt = prompt[0];
            prompts[0].reply = &replies[0];
            replies[0].data = reply[0];
            replies[0].length = sizeof(reply[0]);
            
            prompts[1].hidden = 1;
            prompts[1].prompt = prompt[1];
            prompts[1].reply = &replies[1];
            replies[1].data = reply[1];
            replies[1].length = sizeof(reply[1]);
            
            prompts[2].hidden = 1;
            prompts[2].prompt = prompt[2];
            prompts[2].reply = &replies[2];
            replies[2].data = reply[2];
            replies[2].length = sizeof(reply[2]);
            
            prompts[3].hidden = 0;
            prompts[3].prompt = prompt[3];
            prompts[3].reply = &replies[3];
            replies[3].data = reply[3];
            replies[3].length = sizeof(reply[3]);

            err =  __KLPrompter (NULL, NULL, "Test Name", "Test Banner", num_prompts, prompts);        

            if (!err) {
                int i;
                for (i = 0; i < num_prompts; i++) {
                    NSLog (@"Prompt '%s' got response '%s'\n", prompt[i], reply[i]);
                }
            }        
        }
        
    } else {
        // Enter the server event loop
        err = mach_server_run_server (KerberosAgentIPC_server);
    }
    
    syslog (LOG_NOTICE, "Exiting: %s (%d)", mach_error_string (err), err);
    
    closelog ();    
    [pool release];
    
    return err;
}

