/*
 * KerberosLoginServer.m
 *
 * $Header: /cvs/kfm/KerberosFramework/KerberosLogin/Sources/KerberosLoginServer/KerberosLoginServer.m,v 1.5 2003/04/30 18:48:39 lxs Exp $
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

#import "KLSFloatingWindow.h"

int main (int argc, const char *argv[])
{
    dprintf ("Starting up...\n");
    
    // Make all future windows float:
    // We do this so we can use Apple's NSGetAlertPanel() rather than rolling our own dialogs.
    // And so that popups like the realms menu don't appear under the dialog!
    // Note that we can't use categories because we need to call the superclass.
    [KLSFloatingWindow poseAsClass: [NSWindow class]];

    // Run the application to initialize Cocoa and the event loop
    return NSApplicationMain (argc, argv);
}

